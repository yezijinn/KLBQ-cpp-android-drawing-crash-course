---
tags: [教程, 卷五, ImGui, UI]
day: 39
aliases: [ch61]
---

# 第 61 章 · ImGui 架构与 DrawList

> [!abstract] 本章目标
> 理解立即模式 UI 的原理，看懂 `ImDrawList` 里到底装了什么。
> 这是本项目所有绘制代码的组织方式。

> [!note] 承上
> 前几章讲 Vulkan（怎么把三角形送进 GPU）。
> 本章讲 **ImGui**——本项目用"立即模式 UI"生成那些三角形。

## 先看两种 UI 范式

### 保留模式（Retained Mode）

Qt、Win32、Android View 都是这样：

```cpp
// 创建控件对象，它一直存在
Button *btn = new Button("确定");
btn->setOnClick([]{ ... });
layout->addChild(btn);

// 状态变化时手动更新
btn->setText("取消");
```

**框架保留一棵控件树**，你操作对象。

### 立即模式（Immediate Mode）

ImGui 是这样：

```cpp
// 每帧重新"描述"一次 UI
if (ImGui::Button("确定")) {
    // 点击处理
}
```

**没有控件对象**。每帧从零描述，ImGui 内部记录下"画了什么、在哪",
点击判定也在这一帧完成。

| | 保留模式 | 立即模式 |
|---|---|---|
| 状态 | 存在对象里 | 存在你的变量里 |
| 更新 | 手动同步 | 每帧重建，无需同步 |
| 代码量 | 多 | **少** |
| 适合 | 复杂应用 UI | 调试面板、工具、游戏内 UI |

> [!tip] 为什么游戏辅助都用 ImGui
> 1. 没有回调、没有事件绑定，几十行就能出一个面板
> 2. 数据直接来自变量，不会"UI 显示的和实际值不一致"
> 3. 单文件依赖，容易集成到任何渲染后端

## ImGui 的三层结构

```
ImGuiContext     全局状态（窗口位置、焦点、IO、样式）
    │
    ├─ ImGuiIO   输入（鼠标/键盘/触摸）、DeltaTime、DisplaySize
    │
    ├─ ImGuiStyle  颜色、间距、圆角
    │
    └─ ImDrawList 绘制指令列表（每帧生成）
            ├─ VtxBuffer   顶点数组
            ├─ IdxBuffer   索引数组
            └─ CmdBuffer   命令数组（按纹理分组）
```

## 一帧的完整流程

```cpp
// 1. 准备 IO
ImGuiIO &io = ImGui::GetIO();
io.DeltaTime = dt;
io.DisplaySize = ImVec2(w, h);

// 2. 开始新帧
ImGui::NewFrame();

// 3. 描述 UI（生成 DrawList）
ImGui::Begin("我的窗口");
ImGui::Text("Hello");
if (ImGui::Button("点我")) { /* ... */ }
ImGui::End();

// 4. 渲染：生成 ImDrawData
ImGui::Render();

// 5. 把 ImDrawData 交给后端
ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdBuffer);
```

本项目的 `main.cpp`：

```cpp
while (flag) {
    drawBegin();
    Touch::UpdateImGuiInput();      // ← 填 io
    graphics->NewFrame(true);       // ← NewFrame

    Layout_tick_UI(&flag);          // ← 描述 UI
    pXthread();
    DrawPlayer(ImGui::GetForegroundDrawList());   // ← 往前台 DrawList 画
    DrawESP(ImGui::GetForegroundDrawList());

    graphics->EndFrame();           // ← Render + 提交
}
```

## ImDrawList：核心数据结构

```cpp
struct ImDrawList {
    ImVector<ImDrawVert> VtxBuffer;    // 顶点
    ImVector<ImDrawIdx>  IdxBuffer;    // 索引
    ImVector<ImDrawCmd>  CmdBuffer;    // 绘制命令
    ImDrawListFlags      Flags;
};
```

### 顶点

```cpp
struct ImDrawVert {
    ImVec2 pos;      // 8 字节
    ImVec2 uv;       // 8 字节
    ImU32  col;      // 4 字节（RGBA 打包）
};                   // 共 20 字节
```

### 命令

```cpp
struct ImDrawCmd {
    ImVec4     ClipRect;      // 裁剪矩形
    ImTextureID TextureId;    // 纹理（字体图集）
    unsigned int VtxOffset;   // 本批次的顶点起点
    unsigned int IdxOffset;   // 索引起点
    unsigned int ElemCount;   // 索引数量
};
```

**为什么要分命令？** 因为不同批次用不同纹理或裁剪区。
切换纹理要重新绑定，所以按纹理分组能减少切换。

```
CmdBuffer:
  [0] TextureId=字体图集  ClipRect=窗口1  ElemCount=1200
  [1] TextureId=字体图集  ClipRect=窗口2  ElemCount=600
  [2] TextureId=我的图标  ClipRect=全屏   ElemCount=300
```

## 绘制 API

ImGui 提供两类绘制接口：

### 1. 窗口内绘制（自动裁剪、自动位置）

```cpp
ImGui::Begin("窗口");
ImGui::Text("文字");
ImGui::Button("按钮");
ImGui::End();
```

坐标相对于窗口，超出窗口会被裁剪。

### 2. DrawList 直绘（全屏坐标）

```cpp
ImDrawList *draw = ImGui::GetForegroundDrawList();   // 前景（在所有窗口之上）
// 或
ImDrawList *draw = ImGui::GetBackgroundDrawList();   // 背景

draw->AddLine(ImVec2(x1,y1), ImVec2(x2,y2), color, thickness);
draw->AddRect(min, max, color, rounding, flags, thickness);
draw->AddCircle(center, radius, color, segments, thickness);
draw->AddText(pos, color, "文字");
draw->AddTriangle(p1, p2, p3, color, thickness);
```

**本项目 ESP 全部用 DrawList 直绘**——因为它要画在世界坐标对应的屏幕位置，
不属于任何窗口。

### 本项目用到的方法

| 方法 | 用途 | 出现在 |
|---|---|---|
| `AddRect` | 方框 | 方框 ESP |
| `AddLine` | 射线、骨骼连线 | 射线、骨骼 |
| `AddCircle` | 目标点、头部圆 | 自瞄目标点 |
| `AddCircleFilled` | 骨骼索引点 | 调试 |
| `AddText` | 类名、人数、状态 | 多处 |
| `AddTriangle` | PhysX 线框 | DrawESP |

## 颜色：ImU32 与 ImColor

```cpp
// ImColor 转 ImU32
ImColor red(255, 0, 0, 255);          // RGBA
ImU32 packed = red;

// 直接构造
ImU32 c = IM_COL32(255, 0, 0, 255);   // 宏

// 常用颜色
ImColor BoxColor = ImColor(255, 0, 0, 255);       // 红
ImColor BlockBoxColor = ImColor(0, 255, 0, 255);  // 绿（被遮挡）
```

本项目的颜色约定：

| 颜色 | 含义 |
|---|---|
| 红色 | 可见（未被遮挡） |
| 绿色 | 被遮挡 |
| 黄色 | 文字标签 |
| 品红 | 自瞄目标点 |

## 前台 vs 背景 DrawList

```cpp
ImGui::GetBackgroundDrawList();   // 在所有 ImGui 窗口之后画
ImGui::GetForegroundDrawList();   // 在所有 ImGui 窗口之前画
```

本项目用 **Foreground**——ESP 画在菜单之上。

> [!note] 这意味着 ESP 会盖住菜单
> 如果你的方框画在菜单区域，会盖住按钮。
> 本项目菜单默认在角落，冲突不大。
> 如果想让菜单在上层，应该用 Background DrawList。

## 裁剪矩形

```cpp
draw->PushClipRect(min, max, true);
/* 这些绘制会被裁剪到 [min,max] */
draw->PopClipRect();
```

用途：限制绘制区域（如只在游戏画面区域内画 ESP）。

## 性能：每帧多少顶点？

一个方框 = 4 条线 = 8 个顶点（ImGui 把线变成细长的四边形）
一个文字字符 = 6 个顶点（两个三角形）

本项目典型一帧：

```
50 个方框   × 8   = 400
50 个骨骼   × 14条 × 4 = 2800
文字         × 50字符 × 6 = 300
菜单窗口             = 2000
------------------------------
总计约 5500 个顶点
```

**对 GPU 来说微不足道**（现代 GPU 每帧能处理百万级）。
所以本项目真正的性能瓶颈在 CPU 侧的内存读取（第 81 章）。

## 动手：统计一帧的 DrawData

在第 42 章的最小工程基础上：

```cpp
void PrintDrawData(ImDrawData *dd) {
    printf("=== 第 %d 帧 ===\n", frame);
    printf("CmdLists 数量: %d\n", dd->CmdListsCount);
    printf("总顶点: %d  总索引: %d\n", dd->TotalVtxCount, dd->TotalIdxCount);

    for (int i = 0; i < dd->CmdListsCount; i++) {
        const ImDrawList *l = dd->CmdLists[i];
        printf("  List %d: 顶点=%d 索引=%d 命令=%d\n",
               i, l->VtxBuffer.Size, l->IdxBuffer.Size, l->CmdBuffer.Size);

        for (int j = 0; j < l->CmdBuffer.Size; j++) {
            const ImDrawCmd &c = l->CmdBuffer[j];
            printf("    Cmd %d: ElemCount=%u Clip=(%.0f,%.0f)-(%.0f,%.0f)\n",
                   j, c.ElemCount,
                   c.ClipRect.x, c.ClipRect.y, c.ClipRect.z, c.ClipRect.w);
        }
    }

    // 导出第一个命令的顶点看看
    if (dd->CmdListsCount > 0) {
        const ImDrawList *l = dd->CmdLists[0];
        printf("  前 4 个顶点:\n");
        for (int v = 0; v < 4 && v < l->VtxBuffer.Size; v++) {
            const ImDrawVert &vt = l->VtxBuffer[v];
            printf("    pos=(%.1f,%.1f) uv=(%.3f,%.3f) col=0x%08X\n",
                   vt.pos.x, vt.pos.y, vt.uv.x, vt.uv.y, vt.col);
        }
    }
}
```

观察：
1. 加一个按钮，顶点数增加多少？（约 100+）
2. 加一行文字呢？（每字符 6 个顶点）
3. 用 `GetForegroundDrawList()->AddLine()` 画一条线，它出现在哪个 CmdList？

## 验收清单

- [ ] 能说出立即模式与保留模式的区别（说出"立即模式无控件对象、每帧重描述"）
- [ ] 知道 ImGui 一帧的五个步骤（NewFrame→构造 UI→Render→取 DrawData→交后端）
- [ ] 知道 `ImDrawList` 的三个缓冲区（Vtx/Idx/Cmd）（各说一句存什么）
- [ ] 知道为什么要分 Cmd（按纹理/裁剪分组）（说出"减少状态切换"）
- [ ] 会区分窗口内绘制和 DrawList 直绘（写一段用 GetWindowDrawList 直绘的代码）
- [ ] 知道 Foreground / Background DrawList 的区别（说出前者在上、后者在下）
- [ ] 跑通了 DrawData 统计，看到顶点结构（输出顶点数、索引数、命令数）

→ 下一章：[[第62章-ImGui后端如何生成三角形]]　—— 理解从 `ImDrawData` 到 GPU 三角形的每一步，并能写一个极简的 CPU 光栅化后端来验证。
