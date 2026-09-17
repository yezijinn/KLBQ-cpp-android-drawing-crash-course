---
tags: [教程, 卷五, Vulkan, 图形]
day: 37
aliases: [ch58]
---

# 第 58 章 · Vulkan：实例、设备、队列

> [!abstract] 本章目标
> 理解 Vulkan 的五个核心对象，能写出第一个能跑的 Vulkan 初始化。
> 这是 `VulkanGraphics.cpp` 的骨架。

> [!note] 承上
> 上一章画了管线全景图。
> 本章动手搭 Vulkan 的五个核心对象——这是 `VulkanGraphics.cpp` 的骨架。

## 先看 Vulkan 的对象层级

```
VkInstance                 全局实例（进程一个）
    │
    ├─ VkPhysicalDevice    物理设备（GPU，可能是多个）
    │       │
    │       └─ VkDevice   逻辑设备（我们操作它）
    │               │
    │               ├─ VkQueue          队列（提交命令）
    │               ├─ VkCommandPool    命令池
    │               ├─ VkDescriptorPool 描述符池
    │               └─ VkPipeline       管线
    │
    └─ VkSurfaceKHR         窗口表面（第 59 章）
            └─ VkSwapchainKHR  交换链
```

**类比**：
- `VkInstance` = 打开图形库
- `VkPhysicalDevice` = 机器里插的显卡
- `VkDevice` = 你对这张显卡的"连接"
- `VkQueue` = 提交任务的通道
- `VkCommandBuffer` = 具体的任务清单

## 第一步：创建 Instance

```cpp
VkApplicationInfo appInfo = {};
appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
appInfo.pApplicationName = "MyOverlay";
appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
appInfo.pEngineName = "NoEngine";
appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
appInfo.apiVersion = VK_API_VERSION_1_0;

VkInstanceCreateInfo createInfo = {};
createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
createInfo.pApplicationInfo = &appInfo;

// 需要的扩展
const char *extensions[] = {
    VK_KHR_SURFACE_EXTENSION_NAME,
    VK_KHR_ANDROID_SURFACE_EXTENSION_NAME,
};
createInfo.enabledExtensionCount = 2;
createInfo.ppEnabledExtensionNames = extensions;

// 校验层（调试时才开）
#ifdef DEBUG
const char *layers[] = { "VK_LAYER_KHRONOS_validation" };
createInfo.enabledLayerCount = 1;
createInfo.ppEnabledLayerNames = layers;
#endif

VkInstance instance;
VkResult err = vkCreateInstance(&createInfo, nullptr, &instance);
if (err != VK_SUCCESS) { /* 失败 */ }
```

### 三个必填要点

| 字段 | 说明 |
|---|---|
| `sType` | **每个 Vulkan 结构体都要填**，告诉驱动这是什么结构 |
| `apiVersion` | 需要的最低 Vulkan 版本 |
| 扩展列表 | Android 上必须启用 `VK_KHR_surface` 和 `VK_KHR_android_surface` |

> [!note] sType 是 Vulkan 的"类型标签"
> Vulkan 的 C API 用 `void*` 传结构体，靠 `sType` 知道你传的是什么。
> 忘了填会得到 `VK_ERROR_VALIDATION_FAILED` 或直接崩溃。
> **这是 Vulkan 最常见的低级错误。**

## 第二步：枚举物理设备

```cpp
uint32_t gpuCount = 0;
vkEnumeratePhysicalDevices(instance, &gpuCount, nullptr);
std::vector<VkPhysicalDevice> gpus(gpuCount);
vkEnumeratePhysicalDevices(instance, &gpuCount, gpus.data());
```

手机通常只有 1 个（集成 GPU）。

查看属性：

```cpp
VkPhysicalDeviceProperties props;
vkGetPhysicalDeviceProperties(gpus[0], &props);
printf("设备: %s\n", props.deviceName);
printf("API: %d.%d.%d\n",
       VK_VERSION_MAJOR(props.apiVersion),
       VK_VERSION_MINOR(props.apiVersion),
       VK_VERSION_PATCH(props.apiVersion));
```

选择策略（本项目 `SetupVulkan_SelectPhysicalDevice`）：

```cpp
// 优先选独立显卡，其次集成
for (auto& gpu : gpus) {
    VkPhysicalDeviceProperties p;
    vkGetPhysicalDeviceProperties(gpu, &p);
    if (p.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        return gpu;
}
return gpus[0];
```

## 第三步：找队列族

队列（Queue）分几种能力：

| 队列类型 | 用途 |
|---|---|
| `VK_QUEUE_GRAPHICS_BIT` | 图形渲染（**必需**） |
| `VK_QUEUE_COMPUTE_BIT` | 计算 |
| `VK_QUEUE_TRANSFER_BIT` | 数据传输 |

一个物理设备有多个队列族（QueueFamily），要找支持图形的：

```cpp
uint32_t queueFamilyCount = 0;
vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueFamilyCount, nullptr);
std::vector<VkQueueFamilyProperties> families(queueFamilyCount);
vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueFamilyCount, families.data());

uint32_t queueFamily = UINT32_MAX;
for (uint32_t i = 0; i < queueFamilyCount; i++) {
    if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
        queueFamily = i;
        break;
    }
}
if (queueFamily == UINT32_MAX) { /* 没有图形队列 */ }
```

> [!note] 手机上通常还要检查表面支持
> 能画图的队列，还要能"呈现到窗口"：
> ```cpp
> VkBool32 supported = VK_FALSE;
> vkGetPhysicalDeviceSurfaceSupportKHR(gpu, queueFamily, surface, &supported);
> ```
> 严格来说应该找一个**同时支持图形和呈现**的队列族。
> 手机上通常是同一个。

## 第四步：创建逻辑设备

```cpp
float queuePriority = 1.0f;
VkDeviceQueueCreateInfo queueInfo = {};
queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
queueInfo.queueFamilyIndex = queueFamily;
queueInfo.queueCount = 1;
queueInfo.pQueuePriorities = &queuePriority;

// 需要的设备扩展（交换链必需）
const char *deviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

VkDeviceCreateInfo deviceInfo = {};
deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
deviceInfo.queueCreateInfoCount = 1;
deviceInfo.pQueueCreateInfos = &queueInfo;
deviceInfo.enabledExtensionCount = 1;
deviceInfo.ppEnabledExtensionNames = deviceExtensions;

VkDevice device;
VkResult err = vkCreateDevice(gpu, &deviceInfo, nullptr, &device);
```

**`VK_KHR_swapchain` 是必需的**——没有它无法把画面显示到窗口。

## 第五步：获取队列

```cpp
VkQueue queue;
vkGetDeviceQueue(device, queueFamily, 0, &queue);
```

队列不是"创建"的，是从设备里"取"的。

## 辅助对象：命令池与描述符池

> [!note] "命令缓冲"和"命令池"是什么
> **命令缓冲（CommandBuffer）** = 一本"任务清单"。Vulkan 里你不能直接喊"画这个"，
> 而是先把一条条命令（绑定管线、设置视口、发起绘制…）**录制**进命令缓冲，
> 再一次性**提交**给队列执行。第 59 章的"每帧流程"、第 62 章的渲染，都是往命令缓冲里录制。
> **命令池（CommandPool）** = 分配命令缓冲的"工厂"。
> 命令缓冲不能凭空 new，必须从某个命令池里分配——所以创建池是初始化的必经步骤。
> （池按队列族绑定：这个池分配出的命令缓冲，只能提交给对应的队列。）

> [!note] "描述符"和"描述符池"是什么
> **描述符（Descriptor）** = 告诉 GPU"这个着色器要用哪块纹理/缓冲区"的**凭证**。
> 你不能直接把纹理塞给着色器，而是先创建一个描述符指向它，再在绘制时"绑定描述符"。
> 描述符统一放在 **描述符池（DescriptorPool）** 里分配——所以创建池是初始化的必经步骤。
> 本项目 ImGui 渲染时，每个纹理对应一个描述符（第62章会看到 `VkDescriptorSet`）。

```cpp
// 命令池：分配命令缓冲
VkCommandPoolCreateInfo poolInfo = {};
poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
poolInfo.queueFamilyIndex = queueFamily;
vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool);

// 描述符池：给 ImGui 分配纹理描述符
VkDescriptorPoolSize poolSizes[] = {
    { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
};
VkDescriptorPoolCreateInfo descInfo = {};
descInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
descInfo.poolSizeCount = 1;
descInfo.pPoolSizes = poolSizes;
descInfo.maxSets = 1;
vkCreateDescriptorPool(device, &descInfo, nullptr, &descriptorPool);
```

## 本项目的封装

`VulkanGraphics.h` 里的成员（**照抄项目源码**）：

```cpp
class VulkanGraphics : public AndroidImgui {
private:
    VkAllocationCallbacks *m_Allocator = nullptr;
    VkInstance          m_Instance       = VK_NULL_HANDLE;
    VkPhysicalDevice    m_PhysicalDevice = VK_NULL_HANDLE;
    VkDevice            m_Device         = VK_NULL_HANDLE;
    uint32_t            m_QueueFamily    = (uint32_t)-1;
    VkQueue             m_Queue          = VK_NULL_HANDLE;
    VkDebugReportCallbackEXT m_DebugReport = VK_NULL_HANDLE;   // 调试回调
    VkPipelineCache     m_PipelineCache  = VK_NULL_HANDLE;
    VkDescriptorPool    m_DescriptorPool = VK_NULL_HANDLE;

    std::unique_ptr<ImGui_ImplVulkanH_Window> wd{};
    int m_MinImageCount = 2;
    bool m_SwapChainRebuild = false;

    int m_LastWidth = 0;      // 用于检测尺寸变化、重建交换链
    int m_LastHeight = 0;
};
```

**全部初始化为 `VK_NULL_HANDLE`**——Vulkan 里空句柄就是 `nullptr`。

> [!tip] 为什么建议初始化为 VK_NULL_HANDLE
> 很多 Vulkan 对象是可选的（比如 `m_Allocator` 可以为 `nullptr`）。
> 初始化为空，清理时可以统一判断：
> ```cpp
> if (m_Device) { vkDestroyDevice(m_Device, m_Allocator); m_Device = VK_NULL_HANDLE; }
> ```

## 错误处理：每个调用都要检查

```cpp
VkResult err = vkCreateInstance(&info, nullptr, &m_Instance);
check_vk_result(err);      // ImGui 提供的辅助函数
```

ImGui 的 `check_vk_result`：

```cpp
static void check_vk_result(VkResult err) {
    if (err == VK_SUCCESS) return;
    fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
    if (err < 0) abort();
}
```

**所有 Vulkan 函数都返回 `VkResult`**，必须检查。
不检查的话，错误会累积到某个莫名其妙的地方才爆发。

常见错误码：

| 值 | 含义 |
|---|---|
| `VK_SUCCESS` (0) | 成功 |
| `VK_ERROR_OUT_OF_HOST_MEMORY` (-1) | 内存不足 |
| `VK_ERROR_INITIALIZATION_FAILED` (-3) | 初始化失败 |
| `VK_ERROR_INCOMPATIBLE_DRIVER` (-9) | 驱动不兼容 |
| `VK_ERROR_EXTENSION_NOT_PRESENT` (-7) | 扩展不存在 |
| `VK_ERROR_DEVICE_LOST` (-4) | 设备丢失（GPU 挂了） |

## 清理：反向销毁

```cpp
void VulkanGraphics::Cleanup() {
    vkDestroyDescriptorPool(m_Device, m_DescriptorPool, m_Allocator);
    vkDestroyDevice(m_Device, m_Allocator);
    vkDestroyInstance(m_Instance, m_Allocator);
    // 顺序：后创建的先销毁
}
```

**创建顺序的逆序**。这和普通资源管理的原则一致（第 14 章 RAII）。

## 动手：写一个 Vulkan 探针

不渲染，只初始化到"能拿到设备名"，验证环境：

```cpp
#include <vulkan/vulkan.h>
#include <cstdio>
#include <vector>

int main(void) {
    VkApplicationInfo app{};
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo = &app;

    VkInstance inst;
    VkResult r = vkCreateInstance(&ci, nullptr, &inst);
    if (r != VK_SUCCESS) { printf("创建实例失败: %d\n", r); return 1; }
    printf("实例创建成功\n");

    uint32_t n = 0;
    vkEnumeratePhysicalDevices(inst, &n, nullptr);
    printf("物理设备数: %u\n", n);
    if (n == 0) return 1;

    std::vector<VkPhysicalDevice> gpus(n);
    vkEnumeratePhysicalDevices(inst, &n, gpus.data());

    for (auto g : gpus) {
        VkPhysicalDeviceProperties p;
        vkGetPhysicalDeviceProperties(g, &p);
        printf("  设备: %s  API %u.%u.%u\n", p.deviceName,
               VK_VERSION_MAJOR(p.apiVersion),
               VK_VERSION_MINOR(p.apiVersion),
               VK_VERSION_PATCH(p.apiVersion));

        uint32_t qn = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(g, &qn, nullptr);
        std::vector<VkQueueFamilyProperties> qf(qn);
        vkGetPhysicalDeviceQueueFamilyProperties(g, &qn, qf.data());
        for (uint32_t i = 0; i < qn; i++) {
            printf("    队列族%d: %s%s%s (共%u个)\n", i,
                   (qf[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) ? "图形 " : "",
                   (qf[i].queueFlags & VK_QUEUE_COMPUTE_BIT)  ? "计算 " : "",
                   (qf[i].queueFlags & VK_QUEUE_TRANSFER_BIT) ? "传输 " : "",
                   qf[i].queueCount);
        }
    }

    vkDestroyInstance(inst, nullptr);
    return 0;
}
```

在电脑上（装了 Vulkan SDK）编译运行；
在 Android 上用 NDK 编译后 push 运行，看输出什么 GPU。

## 动手验证清单

- [ ] **创建 Instance**：调用 `vkCreateInstance` → 返回 `VK_SUCCESS`
- [ ] **枚举物理设备**：`vkEnumeratePhysicalDevices` → 至少 1 个
- [ ] **查设备属性**：`vkGetPhysicalDeviceProperties` → 打印 GPU 名称
- [ ] **找队列族**：确认有支持图形操作的队列族
- [ ] **创建设备+队列**：`vkCreateDevice` + `vkGetDeviceQueue` → 拿到 `VkQueue`
- [ ] **清理**：按创建逆序销毁所有对象（无验证层报错）

> [!note] 每个结构体都要填 `sType`
> 忘填会得到 `VK_ERROR_VALIDATION_FAILED` 或直接崩。这是 Vulkan 最常见的低级错误。

## 常见坑排查表

| 症状 | 最可能的原因 | 解法 |
|---|---|---|
| `vkCreateInstance` 返回失败 | 扩展没启用或 `sType` 没填 | 填全所有结构体的 `sType` |
| 找不到物理设备 | 设备不支持 Vulkan | 确认设备支持（第 60 章检查）|
| `vkCreateDevice` 失败 | 请求了不支持的队列族 | 先枚举队列族，选支持的 |
| 程序退出时**验证层报错** | 对象没按逆序销毁 | 创建的逆序销毁 |

## 本章小结

> [!abstract] 本章要点已收束
> 回头把本页的"动手"与结论串成一句话；有勾不上的验收项，回去补做。

## 验收清单

- [ ] 知道五个核心对象的层级关系（Instance → PhysicalDevice → Device → Queue，画出层级）
- [ ] 知道每个 Vulkan 结构体都要填 `sType`（说出"sType 让驱动知道结构体类型"）
- [ ] 知道 Android 需要 `VK_KHR_surface` + `VK_KHR_android_surface` 扩展（说出各自作用）
- [ ] 知道设备需要 `VK_KHR_swapchain` 扩展（说出为什么必需）
- [ ] 会找支持图形的队列族（说出如何判断队列族支持图形）
- [ ] 知道必须检查每个 `VkResult`（说出不检查的后果）
- [ ] 跑通了 Vulkan 探针，看到设备名和队列族（运行程序，输出 GPU 名和队列族数量）

→ 下一章：[[第59章-交换链与Android窗口]]　—— 理解交换链为什么存在，以及 `ANativeWindow` 怎么接进 Vulkan。
