---
tags: [教程, 卷五, Vulkan, dlopen]
day: 39
aliases: [ch60]
---

# 第 60 章 · 动态加载 libvulkan

> [!abstract] 本章目标
> 理解"不链接也能用 Vulkan"的做法，以及它为什么对本项目是必需的。
> 本项目的 `vulkan_wrapper.cpp` 就是干这个的。

> [!note] 承上
> 前两章直接链接 Vulkan。
> 本章换一种方式——**用 `dlopen` 动态加载**，缺库时能优雅降级。本项目就走这条路。

## 先看问题

如果直接在 `Android.mk` 里写：

```makefile
LOCAL_LDLIBS += -lvulkan
```

产物会记录 `NEEDED: libvulkan.so`。在**没有 Vulkan 的设备上**：

```
CANNOT LINK EXECUTABLE: library "libvulkan.so" not found
（程序根本起不来）
```

改用 `dlopen`：

```cpp
void *lib = dlopen("libvulkan.so", RTLD_NOW);
if (!lib) {
    // 优雅降级：用别的方式，或提示用户
}
```

**程序照样能启动，只是没有 Vulkan 功能。**

## 三种链接方式对比

| 方式 | 编译时 | 运行时 | 缺失时 |
|---|---|---|---|
| 静态链接 `.a` | 需要 | 不需要 | 编译失败 |
| 动态链接 `-lvulkan` | 需要 `.so` | 自动加载 | **启动失败** |
| `dlopen` | 只需要头文件 | 手动加载 | **可降级** |

本项目选第三种。

## dlopen 四件套

```cpp
#include <dlfcn.h>

// 1. 加载
void *handle = dlopen("libvulkan.so", RTLD_NOW);
if (!handle) {
    printf("加载失败: %s\n", dlerror());
    return;
}

// 2. 取符号
void *sym = dlsym(handle, "vkCreateInstance");
if (!sym) { printf("找不到符号: %s\n", dlerror()); }

// 3. 转换成函数指针并调用
auto pfn = (PFN_vkCreateInstance)sym;
VkResult r = pfn(&info, nullptr, &instance);

// 4. 卸载（程序退出时）
dlclose(handle);
```

| 函数 | 作用 |
|---|---|
| `dlopen(path, flag)` | 加载 .so，返回句柄 |
| `dlsym(handle, name)` | 按名字取地址 |
| `dlerror()` | 取最后一次错误信息 |
| `dlclose(handle)` | 卸载 |

`flag`：

| 值 | 含义 |
|---|---|
| `RTLD_NOW` | 立即解析所有符号（慢一点，但能早发现缺失）|
| `RTLD_LAZY` | 用到时才解析（快，但可能在运行时崩）|

本项目用 `RTLD_NOW`。

## Vulkan 的函数指针类型

Vulkan 头文件为每个函数定义了函数指针类型：

```cpp
typedef VkResult (VKAPI_PTR *PFN_vkCreateInstance)(
    const VkInstanceCreateInfo*  pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkInstance*                  pInstance);
```

前提：定义 `IMGUI_IMPL_VULKAN_NO_PROTOTYPES`（本项目用的）
或 `VK_NO_PROTOTYPES`，这样头文件**只声明类型，不声明函数**。

> [!note] 两个宏的关系
> 本项目只加 `IMGUI_IMPL_VULKAN_NO_PROTOTYPES` 就够了——
> `imgui_impl_vulkan.h` 内部会自动帮你定义 `VK_NO_PROTOTYPES`：
> ```cpp
> #if defined(IMGUI_IMPL_VULKAN_NO_PROTOTYPES) && !defined(VK_NO_PROTOTYPES)
> #define VK_NO_PROTOTYPES
> #endif
> ```
> 所以你不用两个都写。

本项目的 `Android.mk`：

```makefile
LOCAL_CPPFLAGS += -DIMGUI_IMPL_VULKAN_NO_PROTOTYPES
```

## vulkan_wrapper：自动化的符号加载

手写几十个 `dlsym` 太蠢。本项目 `vulkan_wrapper.cpp` 的核心结构：

```cpp
// 声明函数指针变量
#define VK_FUNC_DEF(name) PFN_##name name;

// 加载函数
#define VK_LOAD_FUNC(name) \
    name = (PFN_##name)dlsym(handle, #name); \
    if (!name) { /* 记录缺失 */ }

// 全局函数（不需要设备）
VK_FUNC_DEF(vkCreateInstance)
VK_FUNC_DEF(vkEnumeratePhysicalDevices)
VK_FUNC_DEF(vkGetPhysicalDeviceProperties)
// ... 几十个

// 初始化
bool InitVulkan() {
    void *handle = dlopen("libvulkan.so", RTLD_NOW);
    if (!handle) return false;

    VK_LOAD_FUNC(vkCreateInstance)
    VK_LOAD_FUNC(vkEnumeratePhysicalDevices)
    // ...
    return true;
}
```

**宏 + 函数指针 = 动态加载的通用解法。**

## 两类 Vulkan 函数

| 类型 | 获取方式 | 例子 |
|---|---|---|
| **全局函数** | `dlsym` 直接拿 | `vkCreateInstance`、`vkEnumeratePhysicalDevices` |
| **设备函数** | `vkGetDeviceProcAddr` / `vkGetInstanceProcAddr` | `vkCreateSwapchainKHR`、`vkQueuePresentKHR` |

原因：设备相关函数依赖具体设备，不是 `libvulkan.so` 直接导出的。

```cpp
auto vkCreateSwapchainKHR = (PFN_vkCreateSwapchainKHR)
    vkGetDeviceProcAddr(device, "vkCreateSwapchainKHR");
```

> [!note] `vkGetInstanceProcAddr` / `vkGetDeviceProcAddr`
> 这两个是 Vulkan 的"元函数"：
> - `vkGetInstanceProcAddr(instance, "xxx")` — 取实例级函数
> - `vkGetDeviceProcAddr(device, "xxx")` — 取设备级函数
>
> 它们本身是全局函数，用 `dlsym` 拿到。

## ImGui 后端的对接

ImGui 的 Vulkan 后端需要一批函数指针。不自己填的话，
它会直接调用 `vkCreateXXX`（需要链接）。

开启 `IMGUI_IMPL_VULKAN_NO_PROTOTYPES` 后，
ImGui 提供了注入函数：

```cpp
ImGui_ImplVulkan_LoadFunctions([](const char *function_name, void *vulkan_instance) {
    return dlsym(vulkan_lib_handle, function_name);
});
```

一次性把整个 libvulkan 的符号喂给 ImGui。

**这就是那个宏存在的意义**——让 ImGui 支持动态加载。

## 版本兼容

Vulkan 1.0 / 1.1 / 1.2 的函数不同。动态加载时可以按需检查：

```cpp
// 核心函数（必须有）
if (!vkCreateInstance || !vkCreateDevice || !vkQueuePresentKHR) {
    printf("缺少核心 Vulkan 函数\n");
    return false;
}

// 可选函数（有就用，没有就降级）
if (vkGetPhysicalDeviceSurfaceCapabilities2KHR) {
    // 用新版 API
} else {
    // 用旧版
}
```

同样，扩展函数也可能不存在：

```cpp
auto vkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)
    vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
if (vkCreateDebugUtilsMessengerEXT) {
    // 开启调试输出
}
```

## 本项目的调用链

```
main.cpp
  └─ graphics = GraphicsManager::getGraphicsInterface()
       └─ return std::make_unique<VulkanGraphics>()
            └─ AndroidImgui::Init_Render()
                 └─ Create()   → VulkanGraphics::Create()
                      └─ vulkan_wrapper 的初始化（dlopen + dlsym）
                      └─ vkCreateInstance(...)
```

`VulkanGraphics.cpp` 里调用 `vkCreateInstance` 时，
实际上调用的是 `vulkan_wrapper` 里那个函数指针变量——
**同名，所以代码看不出差别**。这是 wrapper 设计的巧妙之处。

## 动手：自己实现一个 mini wrapper

```cpp
// mini_vulkan.h
#pragma once
#include <vulkan/vulkan.h>
#include <dlfcn.h>

// 声明函数指针
extern PFN_vkCreateInstance                 pfnCreateInstance;
extern PFN_vkEnumeratePhysicalDevices        pfnEnumeratePhysicalDevices;
extern PFN_vkGetPhysicalDeviceProperties     pfnGetPhysicalDeviceProperties;
extern PFN_vkCreateDevice                    pfnCreateDevice;
extern PFN_vkGetDeviceQueue                  pfnGetDeviceQueue;

bool MiniVulkanInit();
void MiniVulkanShutdown();
```

```cpp
// mini_vulkan.cpp
#include "mini_vulkan.h"
#include <cstdio>

PFN_vkCreateInstance                 pfnCreateInstance = nullptr;
PFN_vkEnumeratePhysicalDevices        pfnEnumeratePhysicalDevices = nullptr;
PFN_vkGetPhysicalDeviceProperties     pfnGetPhysicalDeviceProperties = nullptr;
PFN_vkCreateDevice                    pfnCreateDevice = nullptr;
PFN_vkGetDeviceQueue                  pfnGetDeviceQueue = nullptr;

static void *g_handle = nullptr;

#define LOAD(fn) do { \
    pfn##fn = (PFN_vk##fn)dlsym(g_handle, "vk" #fn); \
    if (!pfn##fn) printf("  缺失: vk%s\n", #fn); \
} while(0)

bool MiniVulkanInit() {
    g_handle = dlopen("libvulkan.so", RTLD_NOW);
    if (!g_handle) {
        printf("dlopen 失败: %s\n", dlerror());
        return false;
    }
    printf("libvulkan.so 加载成功\n");

    LOAD(CreateInstance);
    LOAD(EnumeratePhysicalDevices);
    LOAD(GetPhysicalDeviceProperties);
    LOAD(CreateDevice);
    LOAD(GetDeviceQueue);

    // 检查核心函数是否齐全
    if (!pfnCreateInstance || !pfnEnumeratePhysicalDevices) return false;
    return true;
}

void MiniVulkanShutdown() {
    if (g_handle) { dlclose(g_handle); g_handle = nullptr; }
}
```

测试：

```cpp
int main(void) {
    if (!MiniVulkanInit()) { printf("无 Vulkan，降级处理\n"); return 0; }

    VkApplicationInfo app{};
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.apiVersion = VK_API_VERSION_1_0;
    VkInstanceCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo = &app;

    VkInstance inst;
    VkResult r = pfnCreateInstance(&ci, nullptr, &inst);
    printf("vkCreateInstance = %d\n", r);
    if (r != VK_SUCCESS) return 1;

    uint32_t n = 0;
    pfnEnumeratePhysicalDevices(inst, &n, nullptr);
    printf("物理设备数: %u\n", n);

    MiniVulkanShutdown();
    return 0;
}
```

在 Android 上跑：即使设备不支持 Vulkan，程序也会打印"降级处理"然后正常退出，
**而不是启动失败**。这就是动态加载的价值。

## 常见坑

| 问题 | 原因 | 解法 |
|---|---|---|
| `dlsym` 返回 null | 符号名拼错 / 该版本没有 | 打印 `dlerror()` |
| 编译报"未定义引用 vkCreateInstance" | 没定义 `IMGUI_IMPL_VULKAN_NO_PROTOTYPES` | 在 `Android.mk` 加 `-DIMGUI_IMPL_VULKAN_NO_PROTOTYPES`（它会自动带上 `VK_NO_PROTOTYPES`） |
| 调用崩 | 函数指针为 null | 调用前判空 |
| Android 7+ 找不到 libvulkan.so | 私有库限制（App 场景） | 原生可执行文件不受限（第 32 章） |
| 只加载了核心函数，扩展函数崩 | 扩展要用 `vkGetXxxProcAddr` | 分两类加载 |

## 动手验证清单

- [ ] **dlopen 成功**：`dlopen("libvulkan.so", RTLD_NOW)` → 非 NULL
- [ ] **加载核心函数**：用宏加载 `vkCreateInstance` 等 → 都非 NULL
- [ ] **验证降级**：在无 Vulkan 的机器上 dlopen 失败 → 程序仍能启动（打印"无 Vulkan"）
- [ ] **对比链接方式**：直接 `-lvulkan` 的程序在无 vulkan 设备上无法启动

> [!tip] 为什么必须 dlopen
> 不是所有 Android 设备都有 Vulkan。直接链接会让程序在无 Vulkan 设备上**根本起不来**；
> dlopen 能优雅降级。这就是 `vulkan_wrapper.cpp` 存在的意义。

## 常见坑排查表

| 症状 | 最可能的原因 | 解法 |
|---|---|---|
| `dlopen` 返回 NULL | 设备没有 Vulkan，或库名错 | `dlerror()` 看原因；库名是 `libvulkan.so` |
| `dlsym` 返回 NULL | 函数名拼错或该版本没导出 | 对照 `vulkan_wrapper.h` 里的函数名 |
| 程序启动就崩 | 直接链接了 `-lvulkan` 但设备没有 | 改用 dlopen（本章核心）|
| 函数指针调用崩溃 | 没检查 NULL 就调用 | 调用前判 `if (vkCreateInstance)` |

## 验收清单

- [ ] 知道动态链接 vs dlopen 的关键差别（缺失时能否启动）（说出"直接链接缺失则启动失败"）
- [ ] 会用 `dlopen` / `dlsym` / `dlerror` / `dlclose`（写一段打开某 .so 并取符号的代码）
- [ ] 知道 `IMGUI_IMPL_VULKAN_NO_PROTOTYPES` 的作用（说出"改用函数指针"）
- [ ] 知道 Vulkan 函数分"全局"和"设备"两类，获取方式不同（说出 vkGetInstanceProcAddr / vkGetDeviceProcAddr）
- [ ] 会用宏批量加载几十个符号（写一个 X-macro 批量 dlsym）
- [ ] 实现了 mini wrapper，在没有 Vulkan 的设备上能优雅降级（模拟 dlopen 失败，程序不崩）

→ 下一章：[[第61章-ImGui架构与DrawList]]　—— 理解立即模式 UI 的原理，看懂 `ImDrawList` 里到底装了什么。
