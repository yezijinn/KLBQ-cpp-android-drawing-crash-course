---
tags: [教程, 卷五, Vulkan, 窗口]
day: 38
aliases: [ch59]
---

# 第 59 章 · 交换链与 Android 窗口

> [!abstract] 本章目标
> 理解交换链为什么存在，以及 `ANativeWindow` 怎么接进 Vulkan。
> 本章结束时画面能显示出来。

## 先看为什么需要交换链

如果只有一个缓冲区，边画边显示会**撕裂**：

```
缓冲区：[上半已画][下半还是上一帧]
         ↑ 此时显示，屏幕上下是两个不同时刻的画面
```

交换链用多个缓冲区解决：

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

`ANativeWindow` 是 NDK 提供的窗口抽象，背后是 BufferQueue：

```
生产者（你的程序）          消费者（SurfaceFlinger）
      │                            │
      │  dequeueBuffer             │
      │  ←──── 拿到一块空缓冲 ──────│
      │  往里画                     │
      │  queueBuffer ──────────→   │
      │                          合成并显示
```

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

**`supportedCompositeAlpha` 对覆盖层至关重要**：

```cpp
VkCompositeAlphaFlagBitsKHR alphaMode = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
// 想要透明，需要检查：
if (caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR)
    alphaMode = VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR;   // ★
else if (caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR)
    alphaMode = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
```

> [!warning] 不设对就是你那个"黑色背景"问题的根源
> 如果用了 `OPAQUE`，你的悬浮窗会是不透明的黑块。
> 本项目需要"除了画的线以外全透明"，必须用
> `VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR`（或 INHERIT）。

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
| `compositeAlpha` | `POST_MULTIPLIED` | **透明** |
| `preTransform` | `caps.currentTransform` | 跟随屏幕旋转 |
| `oldSwapchain` | 旧交换链 | 重建时复用资源 |

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
if (caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR)
    printf("  POST_MULTIPLIED  ← 覆盖层需要这个\n");
if (caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR)
    printf("  INHERIT  ← 或这个\n");

uint32_t fmtCount;
vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &fmtCount, nullptr);
std::vector<VkSurfaceFormatKHR> fmts(fmtCount);
vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &fmtCount, fmts.data());
printf("支持的格式: %u 种\n", fmtCount);
for (auto& f : fmts)
    printf("  format=%d colorSpace=%d\n", f.format, f.colorSpace);
```

**重点确认你的设备支持 `POST_MULTIPLIED` 或 `INHERIT`**——
不支持的话透明覆盖层做不了（只能用 OPAQUE，会有黑底）。

## 验收清单

- [ ] 理解交换链解决"撕裂"问题（双/三缓冲）
- [ ] 知道 `ANativeWindow` 是 BufferQueue 的生产者接口
- [ ] 会创建 `VkSurfaceKHR`（需要 `VK_USE_PLATFORM_ANDROID_KHR`）
- [ ] 知道 `compositeAlpha` 必须设为 `POST_MULTIPLIED` 才能透明 ★
- [ ] 知道几种呈现模式的区别（FIFO 一定支持）
- [ ] 知道每帧三步：acquire → render → present
- [ ] 知道尺寸变化要重建交换链，且必须先 waitIdle
- [ ] 跑通了能力查询，确认设备支持透明合成

→ 下一章：[[第60章-动态加载libvulkan]]　—— 理解"不链接也能用 Vulkan"的做法，以及它为什么对本项目是必需的。
