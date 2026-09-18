---
tags: [教程, 卷五, Vulkan, 窗口]
day: 38
aliases: [ch59]
---

# 第 59 章 · 交换链与 Android 窗口

> [!abstract] 本章目标
> 理解交换链为什么存在，以及 `ANativeWindow` 怎么接进 Vulkan。
> 本章结束时画面能显示出来。

> [!note] 承上
> 上一章创建了 Vulkan 设备和队列。
> 本章把它们接到 **Android 窗口**上（交换链），让画面真的显示出来。

> [!important] 前置条件
> - **编译**：本机 + 第 33 章装好的 NDK 即可。
> - **运行看画面**：需要**一台支持 Vulkan 的 Android 设备**（Android 7.0+，绝大多数现代手机都有）。
> - **没有设备**：`ndk-build` 编译能通过就算完成；画面验证可跳过，第 66 章会汇总。

## 先看为什么需要交换链

> [!note] 先理解两个词：缓冲 与 撕裂
> **缓冲（buffer）** = 内存里一块"画布"，程序把像素画进去，再送去显示。
> **撕裂（tearing）** = 屏幕显示到一半时，画布内容被改了，导致屏幕上半是旧帧、下半是新帧，中间出现一条错位的裂痕。
>
> 为什么会撕裂？因为显示不是一瞬间完成的，而是**从上到下逐行扫描**（60Hz 屏幕约 16.7ms 扫完一屏）。
> 如果程序在扫描过程中往同一块画布写新内容，屏幕上下就会来自不同时刻。

**最朴素的做法：只用一块画布。** 程序一边画、屏幕一边显示同一块，必然撕裂：

```
缓冲区：[上半已画][下半还是上一帧]
         ↑ 此时显示，屏幕上下是两个不同时刻的画面
```

**交换链（swap chain）** 用**多块画布轮流用**来解决：一块正在显示时，程序画另一块，画完再交换。

```
双缓冲：
  图像A ← 正在显示
  图像B ← 正在绘制
  绘制完成 → 交换 → A 变绘制，B 变显示

三缓冲：多一个备用，更流畅
```

```
┌────────┐  ┌────────┐  ┌────────┐
│ 图像 0  │  │ 图像 1  │  │ 图像 2  │
└────────┘  └────────┘  └────────┘
     ↑           ↑           ↑
   显示中      绘制中       空闲
```

## ANativeWindow：Android 的原生窗口

> [!note] 先理解"生产者-消费者"模型
> 这是并发编程里的经典模型：**一方不断"生产"数据放进队列，另一方不断从队列"取走"消费**。
> 好处是双方解耦——生产者不用等消费者，各干各的，中间用队列缓冲。
>
> Android 的图形系统就是靠这个模型运转的（你只要理解"谁生产、谁消费、中间有队列"即可，
> 具体并发细节到卷六第 82 章才展开）。

`ANativeWindow` 是 NDK 提供的窗口抽象，背后就是一个 **BufferQueue（缓冲队列）**：

```
生产者（你的程序）          消费者（SurfaceFlinger）
      │                            │
      │  dequeueBuffer             │
      │  ←──── 拿到一块空缓冲 ──────│
      │  往里画                     │
      │  queueBuffer ──────────→   │
      │                          合成并显示
```

**你的程序是生产者**（往缓冲里画），**SurfaceFlinger 是消费者**（拿去做屏幕合成）。
中间的 BufferQueue 让"画"和"显示"能各按各的节奏跑，不必互相卡等。

NDK API：

```cpp
#include <android/native_window.h>

ANativeWindow *window = ...;                    // 从某处获得
int32_t w = ANativeWindow_getWidth(window);
int32_t h = ANativeWindow_getHeight(window);
int32_t format = ANativeWindow_getFormat(window);

ANativeWindow_setBuffersGeometry(window, w, h, AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM);
```

本项目窗口来自 `ANativeWindowCreator`（第 65 章），
它绕过 Java 层直接向 SurfaceFlinger 申请 Surface。

## 创建 VkSurfaceKHR

```cpp
VkAndroidSurfaceCreateInfoKHR surfaceInfo = {};
surfaceInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
surfaceInfo.window = window;      // ANativeWindow*

VkSurfaceKHR surface;
VkResult err = vkCreateAndroidSurfaceKHR(instance, &surfaceInfo, nullptr, &surface);
```

这就是"把 Vulkan 和 Android 窗口连起来"。

> [!note] 需要 `VK_USE_PLATFORM_ANDROID_KHR` 宏
> 本项目 `Android.mk` 里有：
> ```makefile
> LOCAL_CPPFLAGS += -DVK_USE_PLATFORM_ANDROID_KHR
> ```
> 没有这个宏，`vulkan.h` 不会声明 `vkCreateAndroidSurfaceKHR`。

## 查询表面能力

创建交换链前，先问窗口"你支持什么"：

```cpp
VkSurfaceCapabilitiesKHR caps;
vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu, surface, &caps);

printf("最小图像数: %u\n", caps.minImageCount);
printf("最大图像数: %u\n", caps.maxImageCount);
printf("当前尺寸: %ux%u\n", caps.currentExtent.width, caps.currentExtent.height);
printf("最小尺寸: %ux%u\n", caps.minImageExtent.width, caps.minImageExtent.height);
printf("最大尺寸: %ux%u\n", caps.maxImageExtent.width, caps.maxImageExtent.height);
```

| 字段 | 说明 |
|---|---|
| `minImageCount` | 至少几张图（2 = 双缓冲，3 = 三缓冲） |
| `currentExtent` | 当前窗口尺寸；若为 `0xFFFFFFFF` 表示由我们决定 |
| `supportedTransforms` | 支持的旋转 |
| `supportedCompositeAlpha` | **支持的透明合成方式** ★ |

**`supportedCompositeAlpha`** 决定"交换链的 alpha 怎么和合成器配合"。

本项目**没有自己选这个值**，而是直接调用 ImGui 的 helper
`ImGui_ImplVulkanH_CreateOrResizeWindow`，由它按支持情况选（**照抄项目实际行为**）：

```cpp
// 真实源码 imgui_impl_vulkan.cpp（helper 内部）
if (cap.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)
    info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
else if (cap.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR)
    info.compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
else
    IM_ASSERT(false && "No supported composite alpha mode found!");
```

> [!important] 本项目覆盖层的透明，靠的是**另外两件事**，不是 compositeAlpha
> 很多人以为"透明必须设 POST_MULTIPLIED"，但**本项目实际用的是 OPAQUE 或 INHERIT**。
> 覆盖层的透明来自：
> 1. **图层像素格式是 `RGBA_8888`**（第 64 章 `ANativeWindowCreator` 里设的），带 alpha 通道；
> 2. **每帧用 alpha=0 清屏**（见第 62 章），没画到的地方 alpha 就是 0。
>
> 项目源码注释写得很直白（`VulkanGraphics.cpp`）：
> ```cpp
> //透明 默认已经是0了
> //memset(wd->ClearValue.color.float32, 0, sizeof(wd->ClearValue.color.float32));
> ```
> `ClearValue` 默认全 0，所以清屏就是全透明——**不用额外设 compositeAlpha**。
>
> **实践提醒**：如果你自己手写交换链、又想要透明，`POST_MULTIPLIED` 确实是一种做法；
> 但本项目走的是"图层 RGBA + 透明清屏"这条路，两者都能达成透明效果，别混为一谈。

## 选择表面格式

```cpp
uint32_t formatCount;
vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &formatCount, nullptr);
std::vector<VkSurfaceFormatKHR> formats(formatCount);
vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &formatCount, formats.data());

// 优先选 BGRA8 + sRGB
VkSurfaceFormatKHR chosen = formats[0];
for (const auto& f : formats) {
    if (f.format == VK_FORMAT_B8G8R8A8_UNORM &&
        f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
        chosen = f;
        break;
    }
}
```

常见格式：`VK_FORMAT_B8G8R8A8_UNORM`（Android 主流）。

## 选择呈现模式

```cpp
// 查询支持的模式
uint32_t modeCount;
vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, &modeCount, nullptr);
std::vector<VkPresentModeKHR> modes(modeCount);
vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, &modeCount, modes.data());
```

| 模式 | 行为 | 适用 |
|---|---|---|
| `VK_PRESENT_MODE_FIFO_KHR` | 垂直同步，队列满则等待 | **保证支持**，最省电 |
| `VK_PRESENT_MODE_MAILBOX_KHR` | 覆盖最旧的，低延迟 | 游戏 |
| `VK_PRESENT_MODE_IMMEDIATE_KHR` | 立即显示，可能撕裂 | 性能测试 |
| `VK_PRESENT_MODE_FIFO_RELAXED_KHR` | FIFO 但允许晚一帧撕裂 | 特殊情况 |

> [!tip] 覆盖层建议用 MAILBOX 或 FIFO
> 本项目是持续刷新的覆盖层，用 `MAILBOX` 能降低延迟；
> 如果设备不支持会自动回退到 `FIFO`（**它一定支持**）。

## 创建交换链

```cpp
VkSwapchainCreateInfoKHR info = {};
info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
info.surface = surface;
info.minImageCount = imageCount;              // 通常 minImageCount + 1
info.imageFormat = chosen.format;
info.imageColorSpace = chosen.colorSpace;
info.imageExtent = extent;                    // 尺寸
info.imageArrayLayers = 1;
info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
info.preTransform = caps.currentTransform;
info.compositeAlpha = alphaMode;              // ★ 透明合成
info.presentMode = presentMode;
info.clipped = VK_TRUE;
info.oldSwapchain = oldSwapchain;             // 重建时传入旧的

vkCreateSwapchainKHR(device, &info, nullptr, &swapchain);
```

**关键参数**：

| 参数 | 值 | 说明 |
|---|---|---|
| `imageUsage` | `COLOR_ATTACHMENT_BIT` | 作为渲染目标 |
| `compositeAlpha` | `OPAQUE` 或 `INHERIT`（helper 按支持情况选） | 透明靠图层 RGBA + 清屏 alpha=0 |
| `preTransform` | `caps.currentTransform` | 跟随屏幕旋转 |
| `oldSwapchain` | 旧交换链 | 重建时复用资源 |

> [!note] 本项目不手写这段，而是调 helper
> 上面是**手写交换链**的写法（理解原理用）。
> 本项目实际调用 `ImGui_ImplVulkanH_CreateOrResizeWindow(...)`（`VulkanGraphics.cpp`），
> 交换链、RenderPass、Framebuffer 都由 helper 一并创建。

## 获取图像与视图

交换链创建后，取出其中的图像：

```cpp
uint32_t imageCount;
vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
std::vector<VkImage> images(imageCount);
vkGetSwapchainImagesKHR(device, swapchain, &imageCount, images.data());

// 为每个图像创建视图
for (auto img : images) {
    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = img;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = surfaceFormat.format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;
    vkCreateImageView(device, &viewInfo, nullptr, &view);
}
```

`VkImage` 是资源本身，`VkImageView` 是"怎么看它"（哪几层、哪个格式）。

## 每帧流程

```cpp
// 1. 获取一张可用的图像
uint32_t imageIndex;
VkResult r = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX,
                                   imageAcquiredSemaphore, VK_NULL_HANDLE, &imageIndex);

// 2. 渲染到这张图像
//    （录制命令缓冲 → 提交到队列）

// 3. 呈现
VkPresentInfoKHR presentInfo = {};
presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
presentInfo.waitSemaphoreCount = 1;
presentInfo.pWaitSemaphores = &renderCompleteSemaphore;
presentInfo.swapchainCount = 1;
presentInfo.pSwapchains = &swapchain;
presentInfo.pImageIndices = &imageIndex;
vkQueuePresentKHR(queue, &presentInfo);
```

> [!note] 这里为什么要"信号量"？
> 上一节说：你的程序（生产者）和 SurfaceFlinger（消费者）各按各的节奏跑。
> 但有两件事必须**严格按顺序**：
> 1. 必须**先拿到一张可画的图像**，才能往上画；
> 2. 必须**先画完，才能拿去显示**。
>
> **信号量（Semaphore）** 就是干这个的——它是一个**同步工具**：
> 一方"发出信号"，另一方"等信号"。
> - `imageAcquiredSemaphore`：等"图像已就绪"，到了才能开始画；
> - `renderCompleteSemaphore`：等"渲染已完成"，到了才能提交显示。
>
> 这样 GPU/CPU 各阶段不会乱序执行，也不会互相覆盖。
> （信号量的并发原理到卷六第 82 章详讲；这里记住"它是用来让两步按顺序发生的"即可。）

**信号量（Semaphore）**同步三步：
- `imageAcquiredSemaphore`：图像可以画了
- `renderCompleteSemaphore`：画完了可以显示了

## 处理尺寸变化

窗口尺寸变了（旋转、分屏）必须重建交换链：

```cpp
if (m_SwapChainRebuild || width != m_LastWidth || height != m_LastHeight) {
    // 等队列空闲
    vkDeviceWaitIdle(device);

    // 销毁旧的 framebuffer / imageview
    // 用 oldSwapchain 创建新的
    VkSwapchainCreateInfoKHR info = {...};
    info.oldSwapchain = oldSwapchain;
    vkCreateSwapchainKHR(device, &info, nullptr, &newSwapchain);
    vkDestroySwapchainKHR(device, oldSwapchain, nullptr);
}
```

**必须先 `vkDeviceWaitIdle`**——不能在使用中的时候销毁。

本项目 `drawBegin()` 检测到屏幕尺寸变化就重建整个窗口（第 35、66 章）。

## 动手：检查设备能力

把第 58 章的探针扩展，加上表面相关查询：

```cpp
// 在拿到 surface 之后
VkSurfaceCapabilitiesKHR caps;
vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu, surface, &caps);

printf("图像数: %u ~ %u\n", caps.minImageCount, caps.maxImageCount);
printf("当前尺寸: %ux%u\n", caps.currentExtent.width, caps.currentExtent.height);

printf("支持的合成模式:\n");
if (caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)
    printf("  OPAQUE\n");
if (caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR)
    printf("  PRE_MULTIPLIED\n");
if (caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)
    printf("  OPAQUE\n");
if (caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR)
    printf("  INHERIT  ← 本项目 helper 会用到的\n");
if (caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR)
    printf("  POST_MULTIPLIED\n");

uint32_t fmtCount;
vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &fmtCount, nullptr);
std::vector<VkSurfaceFormatKHR> fmts(fmtCount);
vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &fmtCount, fmts.data());
printf("支持的格式: %u 种\n", fmtCount);
for (auto& f : fmts)
    printf("  format=%d colorSpace=%d\n", f.format, f.colorSpace);
```

**重点是确认设备至少支持 `OPAQUE` 或 `INHERIT` 之一**（几乎所有设备都支持）。
本项目的透明**不依赖某个特定的 compositeAlpha**，而是靠"图层 RGBA + 清屏 alpha=0"，
所以这里主要是了解设备能力，不是"必须找到 POST_MULTIPLIED"。

## 动手验证清单

- [ ] **创建 Surface**：`vkCreateAndroidSurfaceKHR` → 返回成功
- [ ] **查表面能力**：`vkGetPhysicalDeviceSurfaceCapabilitiesKHR` → 打印 min/maxImageCount
- [ ] **查支持格式**：`vkGetPhysicalDeviceSurfaceFormatsKHR` → 选一个 `R8G8B8A8_UNORM`
- [ ] **创建交换链**：`vkCreateSwapchainKHR` → 拿到至少 2 张图像
- [ ] **拿图像**：`vkGetSwapchainImagesKHR` → 确认图像数量
- [ ] **画面显示**：清屏成某颜色 → 提交 → 屏幕上看到纯色（**这步要设备**）

## 常见坑排查表

| 症状 | 最可能的原因 | 解法 |
|---|---|---|
| `vkCreateSwapchainKHR` 失败 | surface 能力没查就直接建 | 先 `vkGetPhysicalDeviceSurfaceCapabilitiesKHR` |
| 画面**纯黑** | 图像数或格式不对 | 用 surface 支持的格式（如 `R8G8B8A8_UNORM`）|
| 提交后**画面不更新** | 没 `vkQueuePresentKHR` 或没等 fence | 确认每帧 `Acquire`→画→`Present` 完整 |
| 黑屏一闪退出 | `vkAcquireNextImageKHR` 返回 `OUT_OF_DATE` | 重建交换链（窗口尺寸变了）|

## 课后习题

### 习题 59.1 推导"三缓冲如何避免撕裂"（★★，应用变式）

**任务要求**：画一个时间轴图，展示**双缓冲**和**三缓冲**在"程序绘制慢"时的差异，
并说明为什么三缓冲更不容易卡顿。

**参考骨架**：

```
双缓冲（绘制慢时）：
  帧0显示 | 帧1绘制中...（超时）| 等待 vsync | 帧1显示
  问题：绘制没跟上，出现"等待"

三缓冲（绘制慢时）：
  帧0显示 | 帧1绘制中（超时）| 帧1显示 | 帧2已在第三块画布绘制
  好处：多一块备用，绘制不用等显示完成
```

**验证断言**：能说出"双缓冲 = 1 显示 + 1 绘制；三缓冲 = 1 显示 + 1 就绪 + 1 绘制"。

### 习题 59.2 分析"交换链为什么和窗口绑定"（★★★，分析）

**任务要求**：分析：

1. 交换链里的"图像"最终要交给谁显示？
2. 为什么交换链必须用 `VkSurfaceKHR` 创建，而不能凭空造？
3. 如果屏幕旋转、尺寸变化，交换链会怎样？

**参考答案要点**：
1. 交给 **SurfaceFlinger**（通过 `ANativeWindow` 的 BufferQueue）合成显示；
2. 交换链的图像要和"窗口的 BufferQueue"格式/数量匹配，必须知道窗口参数 → 需要 Surface；
3. 会**失效**（`VK_ERROR_OUT_OF_DATE_KHR`），必须重建交换链（窗口尺寸/方向是它的创建参数）。

> [!tip] 评价层要点
> 交换链不是"独立的内存池"，而是"**程序与合成器之间的契约**"。
> 窗口变了，契约就失效——这就是为什么窗口大小改变时必须重建交换链。

## 本章小结

> [!abstract] 本章要点已收束
> 把上面的动手与结论串成一句话，再进入下一章。

## 验收清单

- [ ] 理解交换链解决"撕裂"问题（双/三缓冲）（说出"一个显示、一个绘制"）
- [ ] 知道 `ANativeWindow` 是 BufferQueue 的生产者接口（说出生产/消费关系）
- [ ] 会创建 `VkSurfaceKHR`（需要 `VK_USE_PLATFORM_ANDROID_KHR`）（成功创建 surface）
- [ ] 知道本项目的透明靠"图层 RGBA + 清屏 alpha=0"，不依赖 `POST_MULTIPLIED` ★（说出项目和"必须 POST_MULTIPLIED"说法的区别）
- [ ] 知道几种呈现模式的区别（FIFO 一定支持）（说出 FIFO/MAILBOX/IMMEDIATE 差异）
- [ ] 知道每帧三步：acquire → render → present（按顺序复述）
- [ ] 知道尺寸变化要重建交换链，且必须先 waitIdle（说出为什么）
- [ ] 跑通了能力查询，看到设备支持的 compositeAlpha 模式（输出各模式）

→ 下一章：[[第60章-动态加载libvulkan]]　—— 理解"不链接也能用 Vulkan"的做法，以及它为什么对本项目是必需的。
