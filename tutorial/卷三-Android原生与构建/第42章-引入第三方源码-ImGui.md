---
tags: [教程, 卷三, Android, ImGui, 第三方库]
day: 26
aliases: [ch42]
---

# 第 42 章 · 引入第三方源码：ImGui

> [!abstract] 本章目标
> 学会把"源码形式的第三方库"塞进 ndk-build 工程，并跑通第一个 ImGui 程序。
> 本项目 `jni/src/ImGui/` 下有 **10 个 .cpp**：ImGui 核心 5 个（`imgui` / `imgui_draw` / `imgui_tables` / `imgui_widgets` / `imgui_impl_vulkan`）+ 本项目自己的 5 个（Android 适配、字体、触摸、stb 等），就是这么来的。

> [!note] 承上
> 前几章都是"自己写代码"。
> 本章开始引入第三方库——先是**源码形式**的 ImGui（它提供 UI，卷五会大量用到）。

## 先看 ImGui 需要哪些文件

> [!tip] 选型权衡：为什么用 Dear ImGui？（供应链卡片）
> **本项目锁定版本**：`IMGUI_VERSION "1.93.0 WIP"`（`IMGUI_VERSION_NUM 19294`，见 `imgui.h`）。
>
> | 维度 | 说明 |
> |---|---|
> | **为何选它** | 立即模式 UI，无控件对象、无回调绑定，几十行出面板（第 61 章详述） |
> | **核心优势** | 单文件依赖、易集成任意渲染后端（Vulkan/OpenGL）、数据直接绑变量 |
> | **在项目中的角色** | 覆盖层的**全部 UI 与 2D 绘制**（菜单、方框、文字、骨骼线） |
> | **供应链风险** | 迭代频繁、API 偶有变更（如 1.92 起纹理/字体后端大改）；`imconfig.h` 升级会被覆盖 |
>
> **版本兼容提示**：本项目用 **1.93.0 WIP**（`IMGUI_HAS_TEXTURES` 已启用）。
> 若你换成 1.89 及更早，`imgui_impl_vulkan.cpp` 的纹理接口不同，需同步换后端文件。
> **锁定建议**：整目录拷贝并记版本号（下节「版本管理」），不追最新。

Dear ImGui 的核心只有几个文件：

| 文件 | 必需 | 作用 |
|---|---|---|
| `imgui.h` / `imgui.cpp` | ✓ | 核心 |
| `imgui_internal.h` | ✓ | 内部（很多高级用法需要） |
| `imgui_draw.cpp` | ✓ | 绘制 |
| `imgui_tables.cpp` | ✓ | 表格 |
| `imgui_widgets.cpp` | ✓ | 控件 |
| `imconfig.h` | 可选 | 配置（如禁用某些功能） |
| `imstb_rectpack.h` | ✓ | 字体图集打包 |
| `imstb_textedit.h` | ✓ | 文本输入 |
| `imstb_truetype.h` | ✓ | TTF 字体解析 |
| `imgui_impl_vulkan.cpp/h` | 后端 | Vulkan 渲染后端 |
| `imgui_demo.cpp` | 可选 | 演示窗口（发布时删掉） |

本项目 `Android.mk` 里列的：

```makefile
LOCAL_SRC_FILES += src/ImGui/imgui.cpp
LOCAL_SRC_FILES += src/ImGui/imgui_draw.cpp
LOCAL_SRC_FILES += src/ImGui/imgui_tables.cpp
LOCAL_SRC_FILES += src/ImGui/imgui_widgets.cpp
LOCAL_SRC_FILES += src/ImGui/imgui_impl_vulkan.cpp
```

**没有** `imgui_demo.cpp`（第 41 章说的体积优化）。

## 目录规划

推荐做法：第三方源码和自己的源码分开：

```
jni/
├── include/
│   ├── ImGui/           ← 第三方头文件
│   │   ├── imgui.h
│   │   ├── imconfig.h
│   │   └── ...
│   └── My_Utils/        ← 自己的
├── src/
│   ├── ImGui/           ← 第三方源码
│   │   ├── imgui.cpp
│   │   └── ...
│   └── main.cpp         ← 自己的
```

本项目正是这个结构。好处：
- 升级第三方库时只换对应目录
- 自己的代码不会被污染

## 在 Android.mk 里加入

```makefile
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include/ImGui

LOCAL_SRC_FILES += src/ImGui/imgui.cpp
LOCAL_SRC_FILES += src/ImGui/imgui_draw.cpp
LOCAL_SRC_FILES += src/ImGui/imgui_tables.cpp
LOCAL_SRC_FILES += src/ImGui/imgui_widgets.cpp
LOCAL_SRC_FILES += src/ImGui/imgui_impl_vulkan.cpp
```

核心的 4 个 .cpp 要一起加（`imgui.cpp` + `imgui_draw.cpp` + `imgui_tables.cpp` + `imgui_widgets.cpp`），
少了任何一个都会 `undefined reference`。（若用 Vulkan 后端，还要加 `imgui_impl_vulkan.cpp`。）

## 需要的宏

```makefile
LOCAL_CPPFLAGS += -DIMGUI_IMPL_VULKAN_NO_PROTOTYPES
LOCAL_CPPFLAGS += -DIMGUI_DISABLE_DEBUG_TOOLS
```

| 宏 | 作用 |
|---|---|
| `IMGUI_IMPL_VULKAN_NO_PROTOTYPES` | 不直接调用 Vulkan 函数，改为保存函数指针（配合动态加载） |
| `IMGUI_DISABLE_DEBUG_TOOLS` | 去掉调试工具窗口 |
| `IMGUI_DISABLE_DEMO_WINDOWS` | 去掉演示窗口（省体积） |
| `IMGUI_USE_WCHAR32` | 支持 Unicode（emoji 等） |

`NO_PROTOTYPES` 的含义：

```cpp
// 不开这个宏
vkCreateInstance(&info, nullptr, &instance);      // 直接调用

// 开了这个宏
ImGui_ImplVulkan_LoadFunctions([](const char* name, void*) {
    return dlsym(vulkan_lib, name);               // 从动态库取函数地址
});
```

第 60 章会讲为什么要这样做（不是所有设备都有 libvulkan）。

## 编译期配置：imconfig.h

不改源码就能定制 ImGui：

```cpp
// imconfig.h
#pragma once

// 不用 std::string（省体积）
#define IMGUI_DEFINE_MATH_OPERATORS       // 启用 ImVec2 的运算符

// 自定义断言
#define IM_ASSERT(x) do { if (!(x)) __android_log_print(ANDROID_LOG_ERROR, "ImGui", "断言失败: %s:%d", __FILE__, __LINE__); } while(0)

// 不用 demo
#define IMGUI_DISABLE_DEMO_WINDOWS
```

## ImGui 的最小使用流程

```cpp
#include "imgui.h"

// 1. 创建上下文（进程内单例）
IMGUI_CHECKVERSION();
ImGui::CreateContext();

// 2. 配置 IO
ImGuiIO &io = ImGui::GetIO();
io.DisplaySize = ImVec2(width, height);
io.DeltaTime = 1.0f / 60.0f;

// 3. 每帧
while (running) {
    io.DeltaTime = dt;

    ImGui::NewFrame();          // 开始

    ImGui::Begin("窗口标题");
    ImGui::Text("你好");
    if (ImGui::Button("点我")) { /* ... */ }
    ImGui::End();

    ImGui::Render();            // 生成 ImDrawData
    // 4. 把 ImDrawData 交给渲染后端（第 62 章）
}

// 5. 清理
ImGui::DestroyContext();
```

本项目 `main.cpp` 就是这个结构，只是把渲染后端换成了 Vulkan。

## 本项目对 ImGui 的三处定制

**1. 中文字体（内嵌）**

```cpp
bool M_Android_LoadFont(float SizePixels) {
    ImGuiIO &io = ImGui::GetIO();
    ImFontConfig config;
    config.FontDataOwnedByAtlas = false;      // 数据是静态数组，别让 ImGui 释放
    config.SizePixels = SizePixels;

    zh_font = io.Fonts->AddFontFromMemoryTTF(
        const_cast<unsigned char *>(g_heiti_ttf_data),   // 内嵌字节数组
        static_cast<int>(EmbeddedAssets::HeitiSize()),
        SizePixels,
        &config,
        io.Fonts->GetGlyphRangesChineseFull());          // 全量中文

    io.FontDefault = zh_font;
    return zh_font != nullptr;
}
```

关键点：`FontDataOwnedByAtlas = false`——因为数据在 `.rodata` 里，
让 ImGui 去 `free` 它会崩溃。

**2. 整体缩放**

```cpp
ImGui::StyleColorsLight();
M_Android_LoadFont(25.0f);
ImGui::GetStyle().ScaleAllSizes(3.0f);      // 手机上放大 3 倍
```

手机屏幕 DPI 高，不缩放的话控件小得没法点。

**3. 触摸输入**

ImGui 默认是鼠标键盘。本项目用 `TouchHelperA` 把触摸事件喂给 `io`：

```cpp
io.MousePos = ImVec2(touch_x, touch_y);
io.MouseDown[0] = touch_pressed;
```

第 71 章详细讲。

## 常见编译问题

| 报错 | 原因 | 解法 |
|---|---|---|
| `imgui.h: No such file` | 缺 include 路径 | 加 `LOCAL_C_INCLUDES` |
| `undefined reference to ImGui::Begin` | 没加 `imgui.cpp` | 补源文件 |
| `undefined reference to ImGui::TextEx` | 没加 `imgui_draw.cpp` | 补 |
| `undefined reference to vkCreateInstance` | 没链接 vulkan，且没用 NO_PROTOTYPES | 加宏 + `LoadFunctions` |
| 中文显示成方块 | 字体没加载 / GlyphRange 不对 | 检查 `AddFontFromMemoryTTF` |
| 崩溃在 `IM_ASSERT` | 通常是没调 `NewFrame` 就画 | 检查调用顺序 |

## 版本管理

ImGui 更新频繁，API 偶尔变。建议：

1. 把版本号写进目录或注释
2. 升级时先在一个分支试
3. 注意 `imconfig.h` 的改动会被新版本覆盖——**备份它**

```cpp
// 在 imconfig.h 顶部记下
// ImGui version: 1.90.x
// 本地修改: 加入 IM_ASSERT 定制、禁用 demo
```

## 动手：最小 ImGui 工程

不用 Vulkan，先用**空后端**跑通 ImGui 逻辑：

```cpp
#include <cstdio>
#include "imgui.h"

int main(void) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.DisplaySize = ImVec2(1080, 2400);
    io.DeltaTime = 1.0f / 60.0f;

    // 分配字体图集（纯 CPU，不依赖渲染后端）
    unsigned char *pixels;
    int w, h;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &w, &h);
    printf("字体图集: %dx%d\n", w, h);

    for (int frame = 0; frame < 3; frame++) {
        io.DeltaTime = 1.0f / 60.0f;
        ImGui::NewFrame();

        static bool show = true;
        static float value = 0.5f;
        ImGui::Begin("测试窗口", &show);
        ImGui::Text("第 %d 帧", frame);
        if (ImGui::Button("按钮")) printf("被点了\n");
        ImGui::SliderFloat("滑块", &value, 0.0f, 1.0f);
        ImGui::End();

        ImGui::Render();

        ImDrawData *dd = ImGui::GetDrawData();
        printf("帧 %d: %d 个绘制列表, 共 %d 条命令, %d 个顶点\n",
               frame, dd->CmdListsCount,
               dd->TotalIdxCount > 0 ? dd->CmdLists[0]->CmdBuffer.Size : 0,
               dd->TotalVtxCount);
    }

    ImGui::DestroyContext();
    return 0;
}
```

`Android.mk`：

```makefile
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := imgui_min
LOCAL_CPPFLAGS := -std=c++17
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include/ImGui
LOCAL_SRC_FILES := main.cpp \
    src/ImGui/imgui.cpp \
    src/ImGui/imgui_draw.cpp \
    src/ImGui/imgui_tables.cpp \
    src/ImGui/imgui_widgets.cpp
LOCAL_LDLIBS := -llog
include $(BUILD_EXECUTABLE)
```

推到手机跑，观察输出的顶点数——这就是第 62 章要渲染的数据。

## 动手验证清单

- [ ] **准备目录**：按本章规划建 `include/ImGui/` 和 `src/ImGui/`
- [ ] **放入核心文件**：`imgui.cpp` + `imgui_draw.cpp` + `imgui_tables.cpp` + `imgui_widgets.cpp`
- [ ] **加头文件路径**：`LOCAL_C_INCLUDES += $(LOCAL_PATH)/include/ImGui`
- [ ] **加宏**：`-DIMGUI_IMPL_VULKAN_NO_PROTOTYPES -DIMGUI_DISABLE_DEBUG_TOOLS`
- [ ] **编译**：`ndk-build` → 无 `undefined reference`（5 个 .cpp 都链上）
- [ ] **验证裁剪**：`grep -c 'imgui_demo' Android.mk` → 应为 0（没引入 demo）

> [!warning] 4 个核心 .cpp 缺一不可
> 少了 `imgui_draw`/`imgui_tables`/`imgui_widgets` 任一 → `undefined reference`。

## 课后习题

### 习题 42.1 搭一个最小 ImGui 工程（★★，应用变式）

**任务要求**：把 ImGui 核心 4 个 `.cpp`（`imgui`/`imgui_draw`/`imgui_tables`/`imgui_widgets`）
加入你的 `Android.mk`，编译通过（先不接后端，只要能编译）。

**参考骨架**：

```makefile
LOCAL_SRC_FILES += src/ImGui/imgui.cpp
LOCAL_SRC_FILES += src/ImGui/imgui_draw.cpp
LOCAL_SRC_FILES += src/ImGui/imgui_tables.cpp
LOCAL_SRC_FILES += src/ImGui/imgui_widgets.cpp
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include/ImGui   # ★ 头文件在 include/ImGui，不是 src/ImGui
LOCAL_CPPFLAGS += -DIMGUI_DISABLE_DEBUG_TOOLS
```

**验证断言**：`ndk-build` 无 `undefined reference`；`nm app | grep -c ImGui` > 0。

### 习题 42.2 分析"后端职责"（★★★，分析）

**任务要求**：ImGui 核心（`imgui.cpp` 等）与后端（`imgui_impl_vulkan.cpp`）
**各负责什么**？为什么本项目要 `-DIMGUI_IMPL_VULKAN_NO_PROTOTYPES`？

**参考答案要点**：

| 部分 | 职责 |
|---|---|
| 核心 | 布局、控件逻辑、生成**顶点/索引缓冲**（DrawData）|
| 后端 | 把 DrawData **翻译成 Vulkan 调用**（管线、命令缓冲、上传纹理）|

`NO_PROTOTYPES` 的原因：本项目**动态加载 libvulkan**（第 60 章），
不链接 Vulkan 的 import 库，所以要禁止 ImGui 后端直接声明/调用 Vulkan 函数原型，
改为**由我们注入函数指针**。

> [!tip] 评价层要点
> 这是"**关注点分离**"的经典案例：
> 核心不关心"用什么渲染 API"，后端不关心"UI 长什么样"。
> 理解这个分工，才能看懂第 61~62 章的三角形是怎么生成的。

## 本章小结

> [!abstract] 本章要点已收束
> 回头把本页的"动手"与结论串成一句话；有勾不上的验收项，回去补做。

## 验收清单

- [ ] 知道 ImGui 必需的 5 个 .cpp 和它们的分工（imgui/draw/tables/widgets/impl_vulkan 各管什么）
- [ ] 会在 Android.mk 里加 include 路径和源文件（加一次并编译通过）
- [ ] 知道 `IMGUI_IMPL_VULKAN_NO_PROTOTYPES` 的作用（说出"改用函数指针，配合动态加载"）
- [ ] 能说出 ImGui 的 5 步使用流程（创建上下文→配置 IO→每帧 NewFrame/Render→交后端→清理）
- [ ] 知道内嵌字体时 `FontDataOwnedByAtlas = false` 是必需的（说出"否则 free 静态数组会崩"）
- [ ] **跑通了最小 ImGui 工程，看到顶点数输出** ★（运行程序，输出字体图集尺寸和顶点数）

→ 下一章：[[第43章-引入预编译静态库-Embree]]　—— 把 6 个 `.a` 文件正确链接进工程，并跑通第一个 Embree 光线投射。
