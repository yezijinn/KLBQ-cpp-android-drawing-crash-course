---
tags: [教程, 深挖, 卷五, Vulkan, ImGui]
aliases: [深挖E]
---

# 深挖 E · Vulkan 与 ImGui 实战问答

> [!abstract] 这篇解决什么
> 第 57-63 章把流程走了一遍。但实际写的时候会撞到一堆"文档没说"的问题：
> 验证层报错看不懂、交换链重建崩溃、ImGui 窗口跑到屏幕外、文字模糊……
> 这篇用问答形式把 20 个高频问题一次讲完。**建议学完卷五后读。**

## 一、Vulkan 初始化

### Q1：能给我一份最小可工作的初始化代码吗？

可以。这是把"能创建实例 + 选设备 + 建队列"压缩到最小的版本：

```cpp
#include <vulkan/vulkan.h>
#include <android/log.h>
#include <vector>
#include <cstdio>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "Vk", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "Vk", __VA_ARGS__)

struct VkCtx {
    VkInstance       instance       = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice         device         = VK_NULL_HANDLE;
    VkQueue          queue          = VK_NULL_HANDLE;
    uint32_t         queueFamily    = 0;
    VkCommandPool    cmdPool        = VK_NULL_HANDLE;
    VkDescriptorPool descPool       = VK_NULL_HANDLE;
};

bool VkInit(VkCtx &ctx) {
    // ---------- 1. Instance ----------
    VkApplicationInfo app{};
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = "overlay";
    app.apiVersion = VK_API_VERSION_1_0;

    const char *exts[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_ANDROID_SURFACE_EXTENSION_NAME,
    };

    VkInstanceCreateInfo ici{};
    ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ici.pApplicationInfo = &app;
    ici.enabledExtensionCount = 2;
    ici.ppEnabledExtensionNames = exts;

    VkResult r = vkCreateInstance(&ici, nullptr, &ctx.instance);
    if (r != VK_SUCCESS) { LOGE("vkCreateInstance = %d", r); return false; }
    LOGI("实例创建成功");

    // ---------- 2. PhysicalDevice ----------
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(ctx.instance, &count, nullptr);
    if (count == 0) { LOGE("没有物理设备"); return false; }

    std::vector<VkPhysicalDevice> gpus(count);
    vkEnumeratePhysicalDevices(ctx.instance, &count, gpus.data());

    ctx.physicalDevice = gpus[0];
    for (auto g : gpus) {
        VkPhysicalDeviceProperties p;
        vkGetPhysicalDeviceProperties(g, &p);
        LOGI("  设备: %s  API %u.%u.%u", p.deviceName,
             VK_VERSION_MAJOR(p.apiVersion),
             VK_VERSION_MINOR(p.apiVersion),
             VK_VERSION_PATCH(p.apiVersion));
        if (p.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            ctx.physicalDevice = g;
            break;
        }
    }

    // ---------- 3. QueueFamily ----------
    uint32_t qcount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(ctx.physicalDevice, &qcount, nullptr);
    std::vector<VkQueueFamilyProperties> fams(qcount);
    vkGetPhysicalDeviceQueueFamilyProperties(ctx.physicalDevice, &qcount, fams.data());

    bool found = false;
    for (uint32_t i = 0; i < qcount; i++) {
        if (fams[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            ctx.queueFamily = i;
            found = true;
            LOGI("队列族 %u 支持图形（共 %u 个队列）", i, fams[i].queueCount);
            break;
        }
    }
    if (!found) { LOGE("没有图形队列"); return false; }

    // ---------- 4. Device ----------
    float prio = 1.0f;
    VkDeviceQueueCreateInfo qci{};
    qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qci.queueFamilyIndex = ctx.queueFamily;
    qci.queueCount = 1;
    qci.pQueuePriorities = &prio;

    const char *devExts[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    VkDeviceCreateInfo dci{};
    dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &qci;
    dci.enabledExtensionCount = 1;
    dci.ppEnabledExtensionNames = devExts;

    r = vkCreateDevice(ctx.physicalDevice, &dci, nullptr, &ctx.device);
    if (r != VK_SUCCESS) { LOGE("vkCreateDevice = %d", r); return false; }

    vkGetDeviceQueue(ctx.device, ctx.queueFamily, 0, &ctx.queue);
    LOGI("逻辑设备与队列就绪");

    // ---------- 5. CommandPool ----------
    VkCommandPoolCreateInfo cpci{};
    cpci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cpci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cpci.queueFamilyIndex = ctx.queueFamily;
    r = vkCreateCommandPool(ctx.device, &cpci, nullptr, &ctx.cmdPool);
    if (r != VK_SUCCESS) { LOGE("命令池创建失败 = %d", r); return false; }

    // ---------- 6. DescriptorPool（ImGui 需要）----------
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo dpci{};
    dpci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    dpci.poolSizeCount = 1;
    dpci.pPoolSizes = &poolSize;
    dpci.maxSets = 1;
    dpci.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    r = vkCreateDescriptorPool(ctx.device, &dpci, nullptr, &ctx.descPool);
    if (r != VK_SUCCESS) { LOGE("描述符池创建失败 = %d", r); return false; }

    LOGI("Vulkan 初始化完成");
    return true;
}

void VkShutdown(VkCtx &ctx) {
    if (ctx.descPool) vkDestroyDescriptorPool(ctx.device, ctx.descPool, nullptr);
    if (ctx.cmdPool)  vkDestroyCommandPool(ctx.device, ctx.cmdPool, nullptr);
    if (ctx.device)   vkDestroyDevice(ctx.device, nullptr);
    if (ctx.instance) vkDestroyInstance(ctx.instance, nullptr);
    ctx = {};
}
```

**注意销毁顺序是创建顺序的逆序。**

### Q2：验证层报的错看不懂怎么办？

开启验证层：

```cpp
#ifdef DEBUG
const char *layers[] = { "VK_LAYER_KHRONOS_validation" };
ici.enabledLayerCount = 1;
ici.ppEnabledLayerNames = layers;
#endif
```

常见报错的解读：

| 报错关键词 | 真正的问题 |
|---|---|
| `sType is invalid` | 某个结构体没填 `sType`，或填错了 |
| `VUID-VkXXX-...-parameter` | 某个参数不符合规范，看具体是哪个 |
| `was not destroyed` | 清理不完整（对象泄漏） |
| `imageLayout` | 图像布局转换缺失（渲染前后没做 transition） |
| `pNext` chain | 结构体链没接好 |
| `VK_ERROR_DEVICE_LOST` | GPU 挂了（通常是命令缓冲错误） |
| `VK_ERROR_OUT_OF_DEVICE_MEMORY` | 显存不够 |

**实用技巧**：`sType` 错误占了新手问题的一半。
每个 `VkXxxCreateInfo` 都必须填对应的 `VK_STRUCTURE_TYPE_XXX_CREATE_INFO`。

### Q3：为什么我的设备只有 1 个队列族？

手机上通常只有一个"通用队列族"，同时支持图形、计算、传输。
桌面独显通常有独立的计算队列族和传输队列族。

**代码要能处理两种**（本项目的做法：找第一个支持图形的就行）。

## 二、交换链

### Q4：交换链重建的完整流程？

```cpp
void RebuildSwapchain(VkCtx &ctx, VkSurfaceKHR surface,
                      int width, int height, SwapchainData &sc) {
    // ① 必须等设备空闲！不能在使用中销毁
    vkDeviceWaitIdle(ctx.device);

    // ② 查询新的能力
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ctx.physicalDevice, surface, &caps);

    // ③ 确定尺寸（如果 currentExtent 不是 0xFFFFFFFF，就用它）
    VkExtent2D extent;
    if (caps.currentExtent.width != UINT32_MAX) {
        extent = caps.currentExtent;
    } else {
        extent.width  = std::clamp((uint32_t)width,
                                    caps.minImageExtent.width,
                                    caps.maxImageExtent.width);
        extent.height = std::clamp((uint32_t)height,
                                    caps.minImageExtent.height,
                                    caps.maxImageExtent.height);
    }

    // ④ 创建新交换链，把旧的传给 oldSwapchain
    VkSwapchainKHR old = sc.swapchain;
    VkSwapchainCreateInfoKHR info{};
    info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    info.surface = surface;
    info.minImageCount = caps.minImageCount;      // 用最小值减一，省内存
    info.imageFormat = sc.format;
    info.imageColorSpace = sc.colorSpace;
    info.imageExtent = extent;
    info.imageArrayLayers = 1;
    info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.preTransform = caps.currentTransform;
    info.compositeAlpha = sc.alphaMode;           // ★ 透明合成
    info.presentMode = sc.presentMode;
    info.clipped = VK_TRUE;
    info.oldSwapchain = old;                      // ★ 复用资源

    VkSwapchainKHR newSc;
    VkResult r = vkCreateSwapchainKHR(ctx.device, &info, nullptr, &newSc);
    if (r != VK_SUCCESS) { LOGE("重建交换链失败 = %d", r); return; }

    // ⑤ 销毁旧的（此时可以安全销毁，因为已经 idle 且 newSc 不依赖它）
    if (old != VK_NULL_HANDLE) vkDestroySwapchainKHR(ctx.device, old, nullptr);
    sc.swapchain = newSc;

    // ⑥ 重建 image view 和 framebuffer
    // ...
}
```

**四个关键点**：

| 步骤 | 为什么 |
|---|---|
| `vkDeviceWaitIdle` | 命令缓冲可能还在用旧图像 |
| `oldSwapchain` | 让驱动复用底层资源，更快 |
| `compositeAlpha` 要重设 | 每次创建都要给，不会继承 |
| 重建 image view/framebuffer | 旧的和旧交换链绑定 |

### Q5：尺寸变化时有一帧闪烁怎么办？

因为重建期间有一帧没有内容。缓解方法：

1. **重建前不要 present**（跳过这一帧）
2. 用 `oldSwapchain` 让驱动平滑过渡
3. 把重建放在帧的开始而不是中间

本项目在 `drawBegin()` 里检测尺寸变化并重建：

```cpp
const bool displaySizeChanged = validDisplaySize &&
    (native_window_screen_x != displayInfo.width ||
     native_window_screen_y != displayInfo.height);
```

**只在真的变化时重建**（用 `!=` 比较，而非每帧重建）。

## 三、ImGui

### Q6：窗口跑到屏幕外了怎么拖回来？

```cpp
// 每帧检查并夹紧
ImVec2 pos = ImGui::GetWindowPos();
ImVec2 size = ImGui::GetWindowSize();
float maxX = ImGui::GetIO().DisplaySize.x - size.x;
float maxY = ImGui::GetIO().DisplaySize.y - size.y;

if (pos.x < 0 || pos.y < 0 || pos.x > maxX || pos.y > maxY) {
    ImGui::SetWindowPos(ImVec2(
        std::clamp(pos.x, 0.0f, std::max(0.0f, maxX)),
        std::clamp(pos.y, 0.0f, std::max(0.0f, maxY))));
}
```

本项目在窗口重建后恢复位置时就这么做：

```cpp
ImGui::SetWindowPos({
    std::clamp(LastCoordinate.Pos_x, 0.0f, maxX),
    std::clamp(LastCoordinate.Pos_y, 0.0f, maxY)
});
```

**注意 `std::max(0.0f, maxX)`**——屏幕比窗口还小时 `maxX` 会是负数。

### Q7：ImGui 控件 ID 冲突怎么办？

ImGui 用控件**位置 + 标签**生成 ID。同一个窗口里两个同名控件会冲突：

```cpp
ImGui::Begin("W");
ImGui::Button("确定");     // ID 冲突！
ImGui::Button("确定");
```

三种解法：

```cpp
// ① 用 ## 隐藏 ID 后缀
ImGui::Button("确定##1");
ImGui::Button("确定##2");   // 显示都是"确定"，但 ID 不同

// ② 用 PushID
ImGui::PushID(0); ImGui::Button("确定"); ImGui::PopID();
ImGui::PushID(1); ImGui::Button("确定"); ImGui::PopID();

// ③ 加不可见的唯一前缀
ImGui::Button("##btn1");
```

**注意 `##` 后面不显示，前面的显示。**

### Q8：循环里创建控件要注意什么？

```cpp
for (int i = 0; i < count; i++) {
    char label[32];
    snprintf(label, sizeof(label), "物品 %d", i);
    ImGui::Button(label);      // 自动不同 ID（因为标签不同）
}
```

如果标签相同（比如都用 `"删除"`），必须用 `PushID`：

```cpp
for (int i = 0; i < count; i++) {
    ImGui::PushID(i);
    if (ImGui::Button("删除")) { /* ... */ }
    ImGui::PopID();
}
```

**忘记 `PopID` 会导致后续所有控件 ID 错位**——这是很难查的 bug。

### Q9：为什么文字模糊？

三个原因：

| 原因 | 解法 |
|---|---|
| 字号与显示尺寸不匹配 | 调 `SizePixels` |
| `OversampleH/V` 太小 | 设成 2 或 3 |
| 缩放方式不当 | 用 `Style.ScaleAllSizes` 而不是改字号 |

本项目：

```cpp
config.SizePixels = 25.0f;
config.OversampleH = 2;
config.OversampleV = 1;
```

`OversampleH=2` 表示水平方向采样 2 次（图集宽 2 倍），字形边缘更平滑。
代价是图集变宽。

### Q10：怎么让 ImGui 支持高 DPI？

```cpp
ImGuiStyle &style = ImGui::GetStyle();
float scale = displayDensity / 1.0f;   // 设备像素密度
style.ScaleAllSizes(scale);
io.FontGlobalScale = 1.0f;              // 或者用这个缩放字体
```

或者更简单：直接加载更大的字号 + `ScaleAllSizes`（本项目做法）。

### Q11：ImGui 一帧的顶点数多少算正常？

| 场景 | 顶点数 |
|---|---|
| 一个简单窗口 | 200~500 |
| 带表格和滑块的完整菜单 | 2000~5000 |
| 50 个方框 + 骨骼 | 5000~15000 |
| 上千个绘制项 | 50000+ |

**超过 10 万就该优化了**。GPU 能处理百万级，但 CPU 生成顶点会成为瓶颈。

### Q12：绘制性能怎么优化？

| 手段 | 效果 |
|---|---|
| 减少 Cmd 数量（同纹理合并） | 减少状态切换 |
| 用 `ImDrawList::AddRect` 而非四个 `AddLine` | 顶点数少 |
| 避免每帧重新计算不变的顶点 | 缓存 |
| 减少文字（每字符 6 顶点） | 显著 |
| 限制绘制在屏幕内（第 52 章剔除） | 最有效 |

**本项目的经验**：真正的瓶颈通常在 CPU 侧的内存读取（第 81 章），
而不是 ImGui 的顶点生成。所以先优化前者。

## 四、混合与透明

### Q13：透明效果为什么不对？

Vulkan 的 Alpha 混合公式：

```
finalColor = srcColor × srcFactor + dstColor × dstFactor
```

标准 Alpha 混合：

```cpp
blend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
blend.colorBlendOp        = VK_BLEND_OP_ADD;
blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;   // ★ Alpha 也要混合
blend.alphaBlendOp        = VK_BLEND_OP_ADD;
```

**最容易漏的是 alpha 通道的混合**。只设 RGB 不设 Alpha 会导致
透明叠加异常（多个半透明物体叠在一起时 alpha 不对）。

### Q14：预乘 Alpha 是什么？

```
非预乘：color = (r, g, b, a)         需要乘 a
预乘：  color = (r×a, g×a, b×a, a)   已经乘过
```

预乘的混合公式更简单：

```cpp
blend.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;      // 已经乘过了
blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
```

> [!note] 本项目实际没有设 `POST_MULTIPLIED`
> 这里讲"预乘 Alpha 是什么"是**通用知识**（理解混合公式用）。
> 但**本项目覆盖层的透明，不靠 `POST_MULTIPLIED`**：
> 它用 ImGui helper 建交换链（`compositeAlpha` 选 OPAQUE/INHERIT），
> 透明来自"图层 RGBA_8888 + 每帧 alpha=0 清屏"（第 59 章）。
> 别把"预乘概念"和"本项目实现"混为一谈。

### Q15：两个半透明物体叠加，颜色不对？

因为你没写深度，绘制顺序决定叠加结果。
预乘 Alpha 能减轻（因为公式满足结合律），但顺序仍影响观感。

**本项目**：ImGui 按提交顺序绘制，后来的盖住先前的。
所以 ESP 绘制顺序会影响重叠区域的外观——这是可以接受的（都是线框）。

## 五、工程实践

### Q16：为什么需要在 `vkDeviceWaitIdle` 之前销毁资源？

反了。**必须先 `vkDeviceWaitIdle`，再销毁**：

```cpp
vkDeviceWaitIdle(device);        // ① 等所有命令执行完
vkDestroySwapchainKHR(...);      // ② 才能安全销毁
```

否则驱动可能在后台还在用这块资源 → 崩溃或画面异常。

### Q17：多线程提交命令要注意什么？

| 规则 | 说明 |
|---|---|
| 一个 `VkQueue` 不能多线程同时提交 | 需要加锁 |
| 一个 `VkCommandPool` 不能多线程同时分配 | 每个线程一个池 |
| `VkCommandBuffer` 不能多线程同时录制 | 每个线程一个缓冲 |

**本项目单线程渲染，没有这个问题**。
但如果把物理更新和渲染分到不同线程提交命令，就要注意。

### Q18：怎么调试画面不对？

按这个顺序：

```
1. 先确认真的在渲染：用固定颜色清屏（不看 ImGui）
   clearValue.color = {{1, 0, 0, 1}};    // 红色
2. 有颜色 → 渲染通道正常，问题在几何/管线
3. 没颜色 → 渲染通道/交换链问题
4. 有颜色但 ImGui 不显示 → 检查 ImGui 的 DisplaySize 和 DrawData 是否为空
5. 有部分显示 → 检查裁剪矩形和顶点缓冲偏移
```

**这个"逐层剥离"的方法适用于所有图形问题。**

### Q19：为什么用 `VK_PRESENT_MODE_MAILBOX` 反而更耗电？

MAILBOX 不等待垂直同步，会尽快提交新帧——
渲染频率可能远高于屏幕刷新率，GPU 一直忙。

**省电的选择**：`VK_PRESENT_MODE_FIFO_KHR`（垂直同步）。
本项目把它作为默认，MAILBOX 作为可选（低延迟优先时用）。

### Q20：整个卷五最容易出错的三处？

| 排名 | 问题 | 症状 | 定位方法 |
|---|---|---|---|
| 1 | 图层像素格式不是 RGBA_8888 / 清屏色 alpha≠0 | 黑底 | 查图层格式 + ClearValue |
| 2 | `sType` 漏填 | 创建失败/崩溃 | 验证层会指出 |
| 3 | 符号名版本不匹配 | `dlsym` 返回 null | 符号侦察器 |

**这三个占了新手问题的大部分。**

> [!tip] 一个调试开关
> 在代码里加一个"最小渲染模式"：
> ```cpp
> if (Config.MinimalRender) {
>     // 只清屏成半透明色，不画任何东西
>     return;
> }
> ```
> 出问题时先切到这个模式——
> 如果能看到半透明色，说明渲染链路是通的，问题在绘制内容；
> 如果还是黑块，说明是图层格式/清屏色的问题（注意：不是 compositeAlpha）。**能省很多时间。**

→ 返回 [[卷五-本卷导航]]
