---
tags: [教程, 卷五, ImGui, 渲染]
day: 39
aliases: [ch62]
---

# 第 62 章 · ImGui 后端如何生成三角形

> [!abstract] 本章目标
> 理解从 `ImDrawData` 到 GPU 三角形的每一步，
> 并能写一个极简的 CPU 光栅化后端来验证。

## 先看后端要做的六件事

`ImGui_ImplVulkan_RenderDrawData(draw_data, cmd_buffer)` 内部：

```
① 若顶点/索引缓冲不够大，重新分配 GPU 缓冲
② 把 draw_data 的顶点数据上传到 GPU
③ 设置渲染通道、管线、描述符
④ 设置正交投影矩阵（push constant 或 uniform）
⑤ 遍历每个 CmdList 的每个 Cmd，设置裁剪矩形和纹理，发起绘制
⑥ 结束渲染通道
```

## 步骤 ①：缓冲区管理

```cpp
// ImGui 后端里的典型写法
if (draw_data->TotalVtxCount > vertexBufferSize) {
    // 销毁旧的
    vkDestroyBuffer(device, vertexBuffer, allocator);
    vkFreeMemory(device, vertexBufferMemory, allocator);

    // 按 1.5 倍扩容（避免频繁重分配）
    vertexBufferSize = draw_data->TotalVtxCount + 5000;
    CreateBuffer(vertexBufferSize, ..., &vertexBuffer, &vertexBufferMemory);
}
```

**为什么要扩容而不是每次重建**：避免每帧分配 GPU 内存（很慢）。

## 步骤 ②：上传数据

```cpp
// 映射 GPU 内存，直接 memcpy
void *mapped;
vkMapMemory(device, vertexBufferMemory, 0, vertexSize, 0, &mapped);
memcpy(mapped, draw_data->... , vertexSize);
vkUnmapMemory(device, vertexBufferMemory);
```

也可以用 staging buffer + `vkCmdCopyBuffer`（更快，但代码多）。

对于每帧变化的小数据（几万个顶点），直接 map+memcpy 就够了。

## 步骤 ③：渲染通道与管线

ImGui 的 Vulkan 后端预先创建好：

> [!note] RenderPass（渲染通道）是什么
> 一次"渲染"不是直接往屏幕上画，而是先声明"我要用什么附件（颜色/深度）、开始时清不清空、结束时保存不保存"。
> 这份声明就是 **RenderPass**。Vulkan 要求你**先定义 RenderPass，再在它里面发起绘制**。
> 下面代码里的 `VK_ATTACHMENT_LOAD_OP_CLEAR`（每帧清空）、`finalLayout = PRESENT_SRC_KHR`（结束时供显示）就是在声明这些。

```cpp
// 渲染通道：一个颜色附件，加载时清空
VkAttachmentDescription attachment = {};
attachment.format = imageFormat;
attachment.samples = VK_SAMPLE_COUNT_1_BIT;
attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;      // ★ 每帧清空
attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

VkRenderPassCreateInfo rpInfo = {};
rpInfo.attachmentCount = 1;
rpInfo.pAttachments = &attachment;
vkCreateRenderPass(device, &rpInfo, nullptr, &renderPass);
```

> [!note] `loadOp = CLEAR` 就是"透明背景"的关键之一
> 每帧先用透明色（alpha=0）清空，然后画 UI。
> 没画到的地方就是透明的。
> 配合第 59 章的 `compositeAlpha = POST_MULTIPLIED`，
> 最终合成时透明部分不会遮挡下面的游戏画面。

## 步骤 ④：正交投影

ImGui 的坐标是屏幕像素（0,0 在左上），
需要变换到 NDC（-1..1，y 向上）：

```cpp
float L = draw_data->DisplayPos.x;
float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
float T = draw_data->DisplayPos.y;
float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;

float mvp[4][4] = {
    { 2.0f/(R-L),   0.0f,          0.0f, 0.0f },
    { 0.0f,         2.0f/(T-B),    0.0f, 0.0f },
    { 0.0f,         0.0f,          0.5f, 0.0f },
    { (R+L)/(L-R),  (T+B)/(B-T),   0.5f, 1.0f },
};

// 通过 push constant 传给顶点着色器
vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT,
                   sizeof(float)*0, sizeof(mvp), mvp);
```

逐项验证：

| 元素 | 作用 |
|---|---|
| `2/(R-L)` | x: `[L,R]` → `[-1,1]` |
| `2/(T-B)` | y: `[T,B]` → `[-1,1]`（注意 T < B，所以是负分母，实现翻转） |
| `(R+L)/(L-R)` | x 平移 |
| `(T+B)/(B-T)` | y 平移 |

## 步骤 ⑤：遍历命令绘制

```cpp
ImVec2 clip_off = draw_data->DisplayPos;
int vtx_offset = 0, idx_offset = 0;

for (int n = 0; n < draw_data->CmdListsCount; n++) {
    const ImDrawList *cmd_list = draw_data->CmdLists[n];

    for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++) {
        const ImDrawCmd *pcmd = &cmd_list->CmdBuffer[cmd_i];

        // a. 设置裁剪矩形
        VkRect2D scissor;
        scissor.offset.x = (int32_t)(pcmd->ClipRect.x - clip_off.x);
        scissor.offset.y = (int32_t)(pcmd->ClipRect.y - clip_off.y);
        scissor.extent.width  = (uint32_t)(pcmd->ClipRect.z - pcmd->ClipRect.x);
        scissor.extent.height = (uint32_t)(pcmd->ClipRect.w - pcmd->ClipRect.y);
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        // b. 绑定纹理（字体图集）
        VkDescriptorSet desc = (VkDescriptorSet)pcmd->TextureId;
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipelineLayout, 0, 1, &desc, 0, nullptr);

        // c. 绘制！
        vkCmdDrawIndexed(cmd, pcmd->ElemCount, 1,
                         pcmd->IdxOffset + idx_offset,
                         pcmd->VtxOffset + vtx_offset, 0);
    }
    idx_offset += cmd_list->IdxBuffer.Size;
    vtx_offset += cmd_list->VtxBuffer.Size;
}
```

三个要点：
- **裁剪矩形**用 scissor（硬件裁剪，不消耗性能）
- **纹理**通过描述符集绑定
- **`vtx_offset` 累积**：因为所有 CmdList 的顶点在同一个大缓冲里

## 顶点着色器

```glsl
#version 450
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aColor;

layout(push_constant) uniform uPushConstant {
    vec4 uScale;
    vec4 uTranslate;
} pc;

layout(location = 0) out vec2 vUV;
layout(location = 1) out vec4 vColor;

void main() {
    vUV = aUV;
    vColor = aColor;
    gl_Position = vec4(aPos * pc.uScale.xy + pc.uTranslate.xy, 0.0, 1.0);
}
```

注意：这里把 4×4 矩阵简化成了两个 `vec4`（scale + translate），
因为 ImGui 只需要正交投影，不需要完整的 MVP。

## 片元着色器

```glsl
#version 450
layout(binding = 0) uniform sampler2D sTexture;
layout(location = 0) in vec2 vUV;
layout(location = 1) in vec4 vColor;
layout(location = 0) out vec4 outColor;

void main() {
    outColor = vColor * texture(sTexture, vUV);
}
```

顶点颜色 × 纹理采样。字体图集是**只有 alpha 通道**的，
所以文字颜色来自 `vColor`。

## 动手：写一个 CPU 后端

不用 GPU，直接把 `ImDrawData` 画到一个 RGBA 数组里，
存成 PPM 图片。这样能在电脑上验证 ImGui 逻辑是否正确。

```cpp
#include "imgui.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>

struct CPUBackend {
    int W = 0, H = 0;
    std::vector<uint32_t> pixels;

    CPUBackend(int w, int h) : W(w), H(h), pixels(w*h, 0) {}

    void Clear(uint32_t color = 0x00000000) {
        std::fill(pixels.begin(), pixels.end(), color);
    }

    // 画一个三角形（重心坐标 + 简单填充）
    void DrawTriangle(const ImDrawVert& v0, const ImDrawVert& v1,
                      const ImDrawVert& v2, uint32_t *tex, int texW) {
        float minX = fminf(v0.pos.x, fminf(v1.pos.x, v2.pos.x));
        float maxX = fmaxf(v0.pos.x, fmaxf(v1.pos.x, v2.pos.x));
        float minY = fminf(v0.pos.y, fminf(v1.pos.y, v2.pos.y));
        float maxY = fmaxf(v0.pos.y, fmaxf(v1.pos.y, v2.pos.y));

        int x0 = (int)floorf(minX), x1 = (int)ceilf(maxX);
        int y0 = (int)floorf(minY), y1 = (int)ceilf(maxY);

        float area = (v1.pos.x - v0.pos.x) * (v2.pos.y - v0.pos.y)
                   - (v2.pos.x - v0.pos.x) * (v1.pos.y - v0.pos.y);
        if (fabsf(area) < 1e-6f) return;

        for (int y = y0; y <= y1; y++) {
            for (int x = x0; x <= x1; x++) {
                if (x < 0 || x >= W || y < 0 || y >= H) continue;
                float px = x + 0.5f, py = y + 0.5f;

                float w0 = ((v1.pos.x - px) * (v2.pos.y - py)
                          - (v2.pos.x - px) * (v1.pos.y - py)) / area;
                float w1 = ((v2.pos.x - px) * (v0.pos.y - py)
                          - (v0.pos.x - px) * (v2.pos.y - py)) / area;
                float w2 = 1.0f - w0 - w1;
                if (w0 < 0 || w1 < 0 || w2 < 0) continue;   // 三角形外

                // 插值 UV 和颜色
                float u = w0*v0.uv.x + w1*v1.uv.x + w2*v2.uv.x;
                float v = w0*v0.uv.y + w1*v1.uv.y + w2*v2.uv.y;

                // 采样纹理（这里简化为取颜色，忽略纹理）
                uint32_t c = v0.col;   // 简化：不插值颜色

                // alpha 混合
                float a = ((c >> 24) & 0xFF) / 255.0f;
                uint32_t dst = pixels[y*W + x];
                uint8_t dr = (dst >> 0)  & 0xFF, dg = (dst >> 8)  & 0xFF, db = (dst >> 16) & 0xFF;
                uint8_t sr = (c   >> 0)  & 0xFF, sg = (c   >> 8)  & 0xFF, sb = (c   >> 16) & 0xFF;
                uint8_t r = (uint8_t)(sr*a + dr*(1-a));
                uint8_t g = (uint8_t)(sg*a + dg*(1-a));
                uint8_t b = (uint8_t)(sb*a + db*(1-a));
                pixels[y*W + x] = (r) | (g << 8) | (b << 16) | (0xFFu << 24);
            }
        }
    }

    void Render(ImDrawData *dd) {
        Clear();
        for (int n = 0; n < dd->CmdListsCount; n++) {
            const ImDrawList *l = dd->CmdLists[n];
            for (int ci = 0; ci < l->CmdBuffer.Size; ci++) {
                const ImDrawCmd &c = l->CmdBuffer[ci];
                for (unsigned int i = 0; i + 2 < c.ElemCount; i += 3) {
                    auto v0 = l->VtxBuffer[l->IdxBuffer[c.IdxOffset + i + 0]];
                    auto v1 = l->VtxBuffer[l->IdxBuffer[c.IdxOffset + i + 1]];
                    auto v2 = l->VtxBuffer[l->IdxBuffer[c.IdxOffset + i + 2]];
                    DrawTriangle(v0, v1, v2, nullptr, 0);
                }
            }
        }
    }

    void SavePPM(const char *path) {
        FILE *fp = fopen(path, "wb");
        fprintf(fp, "P6\n%d %d\n255\n", W, H);
        for (auto p : pixels) {
            fputc((p >> 0) & 0xFF, fp);
            fputc((p >> 8) & 0xFF, fp);
            fputc((p >> 16) & 0xFF, fp);
        }
        fclose(fp);
    }
};
```

配合 ImGui 使用：

```cpp
IMGUI_CHECKVERSION();
ImGui::CreateContext();
ImGuiIO &io = ImGui::GetIO();
io.DisplaySize = ImVec2(400, 300);
io.DeltaTime = 1.0f/60.0f;
io.Fonts->GetTexDataAsRGBA32(&texPixels, &texW, &texH);

CPUBackend backend(400, 300);

ImGui::NewFrame();
ImGui::Begin("测试");
ImGui::Text("Hello ImGui");
ImGui::Button("一个按钮");
ImGui::End();
// 用 DrawList 画点东西
auto *dl = ImGui::GetForegroundDrawList();
dl->AddLine(ImVec2(10,10), ImVec2(390,290), IM_COL32(255,0,0,255), 2.0f);
dl->AddRect(ImVec2(50,50), ImVec2(150,150), IM_COL32(0,255,0,255), 0, 0, 3.0f);
ImGui::Render();

backend.Render(ImGui::GetDrawData());
backend.SavePPM("output.ppm");
```

用图片查看器打开 `output.ppm`（或用 Python 转 PNG），
你应该能看到一条红线和绿色的方框。

**这个 CPU 后端做的事情和 Vulkan 后端完全一样**，
区别只是 GPU 并行而你用循环。

## 验收清单

- [ ] 能说出后端渲染的六个步骤
- [ ] 知道为什么要扩容缓冲而不是每次重建
- [ ] 知道 `loadOp = CLEAR` 配合透明清屏色 = 透明背景
- [ ] 能读懂正交投影矩阵的每个元素
- [ ] 知道 scissor 用于硬件裁剪
- [ ] 知道 `vtx_offset` 为什么要累积
- [ ] 写了 CPU 后端并输出了图片

→ 下一章：[[第63章-中文字体与图集]]　—— 搞懂字体图集原理，解决中文显示方块问题，并理解本项目为什么把字体内嵌成 `.cpp` 数组。
