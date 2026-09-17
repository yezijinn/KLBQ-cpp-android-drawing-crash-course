---
tags: [教程, 附录, 全覆盖, 文件索引]
aliases: [附录L, filemap]
---

# 附录 L · 项目文件全覆盖手册

> [!abstract] 这篇解决什么
> **保证 106 个文件无一遗漏。**
> 逐个说明：它是什么、由哪一章讲、关键点在哪。
> 对正文未覆盖的 13 个文件，本附录**补充完整教学**。

## 一、总览：106 个文件的分布

| 类别 | 文件数 | 典型大小 | 教学位置 |
|---|---|---|---|
| 构建配置 | 2 | 0.4 / 3 KB | 第 29、34 章 |
| 主程序 | 1 | 1.5 KB | 第 03、61、100 章 |
| 业务逻辑 | 8 | 0.8–35 KB | 第 84–90 章 |
| 驱动与内存 | 5 | 0.5–45 KB | 第 74–80 章 |
| 渲染（Vulkan） | 6 | 0.2–30 KB | 第 58–62 章 |
| ImGui 核心 | 12 | 20 KB–1 MB | 第 42、61–63 章 |
| Android 适配 | 8 | 0.4–757 KB | 第 65–71 章 + **本附录** |
| 数学工具 | 4 | 0.4–2 KB | 第 45–51 章 |
| 物理与可见性 | 3 + 26 | 2–70 KB / 2–15 KB | 第 93–99 章 + **本附录** |
| 预编译库 | 6 | 0.9 KB–14 MB | 第 43 章 |
| 产物与中间产物 | 5 | — | 第 33、35 章 + **本附录** |

**共 106 个文件（含 `obj/` 与 `libs/` 的产物）。**

## 二、完整教学索引

### 2.1 构建配置（2 个）

| 文件 | 作用 | 章节 |
|---|---|---|
| `Android.mk` | 定义模块、源文件、include 路径、链接库 | 29、34 |
| `Application.mk` | ABI / API / STL / 优化选项 | 29、34、41 |

### 2.2 主程序与业务逻辑（9 个）

| 文件 | 作用 | 章节 |
|---|---|---|
| `src/main.cpp` | 主循环：初始化 → 帧循环 → 清理 | 03、61、100 |
| `src/Android_draw/driver.h` | **驱动统一接口 `IDriver` + 内核驱动实现** | 79、80 |
| `src/Android_draw/draw_Gui.cpp` | 数据获取、ESP 绘制、菜单 UI | 84–90 |
| `src/Android_draw/Draw_ESP.cpp` | PhysX 网格线框绘制 | 94、98 |
| `src/Android_draw/variable.h` | 全局变量、骨骼工具函数、FName 解析 | 85、86 |
| `src/Android_draw/draw_Gui.cpp.bak` | **备份文件**（正文未讲，见 3.7） | 本附录 |
| `src/Hack/Hack.cpp` | 刷新 `AppBase`（相机汇总） | 90 |
| `include/Hack/Hack.h` | `mBase` 结构定义 | 90 |
| `include/Hack/SilentAim.h` | 静默自瞄（选目标 + 写角度） | 47、56、90 |
| `include/Hack/PovAim.h` | 视角自瞄（平滑转动 + 抑制输入） | 49、90 |
| `include/Hack/TouchAim.h` | 触摸自瞄（内核触摸拖拽） | 70 |
| `include/Android_draw/draw.h` | 全局数据声明、`Vector2A/3A` 类型 | 45 |
| `include/Android_draw/WorldToScreen.h` | **投影管线**（camMakeMatrix + W2S） | 49–51、91 |
| `include/Android_draw/Draw_ESP.h` | ESP 绘制声明 | 94 |

### 2.3 驱动与内存（5 个）

| 文件 | 作用 | 章节 |
|---|---|---|
| `include/My_Utils/MemDriver.h` | 双后端选择器（内核 / syscall） | 79、80 |
| `include/My_Utils/SysHal.h` | `process_vm_readv/writev` 后端 | 74、75 |
| `src/My_Utils/SysHal.cpp` | 保证 inline 实例唯一 | 75 |
| `include/My_Utils/ConfigManager.h` | `Config` 结构体定义 | 12 |
| `src/My_Utils/ConfigManager.cpp` | 配置存取（XOR + `/data/Config`） | 22、24 |

### 2.4 渲染（Vulkan）（6 个）

| 文件 | 作用 | 章节 |
|---|---|---|
| `include/ImGui/AndroidImgui.h` | 渲染后端抽象接口 | 58 |
| `src/ImGui/AndroidImgui.cpp` | 基类实现（Init/NewFrame/EndFrame/纹理） | 58 |
| `include/Vulkan/GraphicsManager.h` | 工厂声明 | 58 |
| `src/Vulkan/GraphicsManager.cpp` | 返回 `VulkanGraphics` 实例 | 58 |
| `include/Vulkan/VulkanGraphics.h` | Vulkan 后端成员与接口 | 58、59 |
| `src/Vulkan/VulkanGraphics.cpp` | Vulkan 初始化、交换链、渲染 | 58、59、62 |
| `include/Vulkan/vulkan_wrapper.h` | 函数指针声明 | 60 |
| `src/Vulkan/vulkan_wrapper.cpp` | `dlopen` + `dlsym` 实现 | 60 |

### 2.5 ImGui 核心（12 个）

| 文件 | 作用 | 章节 |
|---|---|---|
| `include/ImGui/imgui.h` | ImGui 公共 API（434 KB） | 42、61 |
| `include/ImGui/imgui_internal.h` | 内部 API（311 KB） | 42 |
| `include/ImGui/imconfig.h` | 编译期配置 | 42 |
| `include/ImGui/imgui_impl_vulkan.h` | Vulkan 后端声明 | 62 |
| `include/ImGui/imstb_rectpack.h` | 矩形打包（字体图集用） | 63 |
| `include/ImGui/imstb_textedit.h` | 文本编辑 | 61 |
| `include/ImGui/imstb_truetype.h` | TTF 字形解析 | 63 |
| `src/ImGui/imgui.cpp` | 核心（982 KB） | 42 |
| `src/ImGui/imgui_draw.cpp` | 绘制（383 KB） | 42、62 |
| `src/ImGui/imgui_tables.cpp` | 表格（270 KB） | 42 |
| `src/ImGui/imgui_widgets.cpp` | 控件（555 KB） | 42 |
| `src/ImGui/imgui_impl_vulkan.cpp` | Vulkan 后端的六步实现 | 62 |
| `src/ImGui/heiti_ttf.cpp` | 黑体字体数组（**约 2.9 MB**，即 3,033,180 字节） | 63 |

### 2.6 数学工具（4 个）

| 文件 | 作用 | 章节 |
|---|---|---|
| `include/My_Utils/VectorTools.h` | `Vec2/Vec3/Rotator/Matrix/Quat` | 45、46 |
| `include/My_Utils/GameTools.h` | 数学工具声明 | 47 |
| `src/My_Utils/GameTools.cpp` | `rotatorToMatrix` / `GetForward` / W2S 封装 | 47、51 |
| `include/ImGui/VectorStruct.h` | **`My_Vector2/3`（第三套向量）** | 本附录 3.3 |

### 2.7 物理与可见性（3 + 26 个）

| 文件 | 作用 | 章节 |
|---|---|---|
| `include/Embree/PhysX.h` | **PhysX 结构 + Embree 场景 + 几何重建**（70 KB） | 93–99 |
| `include/Embree/include/rtcore.h` | Embree 总入口头 | 97 |
| `include/Embree/include/rtcore_common.h` | 基础类型、`RTCDevice`、`rtcNewDevice` | 97 |
| `include/Embree/include/rtcore_config.h` | 编译期配置 | 97 |
| `include/Embree/include/rtcore_device.h` | 设备属性 | 97 |
| `include/Embree/include/rtcore_scene.h` | 场景管理、`rtcCommitScene` | 97 |
| `include/Embree/include/rtcore_geometry.h` | 几何体（三角形/四边形/实例） | 97 |
| `include/Embree/include/rtcore_ray.h` | `RTCRay` / `RTCRayHit` | 95、97 |
| `include/Embree/include/rtcore_buffer.h` | 共享缓冲区 | 97 |
| `include/Embree/include/rtcore_builder.h` | BVH 构建参数（Embree 4 新增） | 96、97 |
| `include/Embree/include/rtcore_quaternion.h` | 四元数（用于方向） | 47、97 |
| `include/Embree/foundation/` 下 26 个头文件 | PhysX 风格数学基础 | 本附录 3.8 |
| `include/Embree/libembree4.a` | 核心：BVH 构建、遍历、几何管理 | 43 |
| `include/Embree/liblexers.a` | 解析器 | 43 |
| `include/Embree/libmath.a` | 数学工具 | 43 |
| `include/Embree/libsimd.a` | SIMD 抽象层 | 43 |
| `include/Embree/libsys.a` | 系统抽象（线程、文件） | 43 |
| `include/Embree/libtasking.a` | 任务调度 | 43 |

### 2.8 Android 适配（8 个）

| 文件 | 作用 | 章节 |
|---|---|---|
| `include/ImGui/ANativeWindowCreator.h` | **窗口与图层创建**（74 KB） | 64–68 |
| `include/ImGui/TouchHelperA.h` | 触摸接口声明 | 69 |
| `src/ImGui/TouchHelperA.cpp` | `/dev/input` + uinput（20 KB） | 69–71 |
| `include/ImGui/AndroidImgui.h` | 见 2.4 | 58 |
| `include/ImGui/Utils.h` | **工具函数集** | 本附录 3.2 |
| `include/ImGui/spinlock.h` | **ImGui 自带自旋锁** | 本附录 3.1 |
| `include/ImGui/my_imgui_impl_android.h` | **Android 输入适配声明** | 本附录 3.4 |
| `src/ImGui/my_imgui_impl_android.cpp` | **键码映射 + 事件处理**（17 KB） | 本附录 3.4 |
| `include/ImGui/embedded_assets.h` | 字体数组声明 | 63 |
| `include/ImGui/gui_icon.h` | **图标码点常量**（137 KB） | 本附录 3.5 |
| `include/ImGui/fontawesome-*.h`（3 个） | **图标字体二进制**（共 1.4 MB） | 本附录 3.5 |
| `include/ImGui/stb_image.h` | **图片解码库**（24 KB） | 本附录 3.6 |
| `src/ImGui/stb_image.cpp` | **图片解码实现**（261 KB） | 本附录 3.6 |

## 三、补充教学：13 个此前未讲的文件

### 3.1 `spinlock.h` —— ImGui 自带的自旋锁

**注意**：本项目有**两种**自旋锁实现，放在不同地方：

| 位置 | 实现方式 | 使用场景 |
|---|---|---|
| `src/Android_draw/driver.h` | GCC 内置 `__atomic_*` | 驱动通信的请求互斥 |
| `include/ImGui/spinlock.h` | `std::atomic<bool>` | ImGui 内部（第三方代码） |

`spinlock.h` 全文只有 33 行，是一个教科书式的实现：

```cpp
struct spinlock {
    std::atomic<bool> lock_ = {false};

    void lock() noexcept {
        for (;;) {
            // 乐观假设：第一次就抢到
            if (!lock_.exchange(true, std::memory_order_acquire)) {
                return;
            }
            // 抢不到：先自旋读，避免持续产生 cache miss
            while (lock_.load(std::memory_order_relaxed)) {
                sched_yield();       // 让出 CPU
            }
        }
    }

    bool try_lock() noexcept {
        return !lock_.load(std::memory_order_relaxed) &&
               !lock_.exchange(true, std::memory_order_acquire);
    }

    void unlock() noexcept {
        lock_.store(false, std::memory_order_release);
    }
};
```

**三个值得学的点**：

| 点 | 说明 |
|---|---|
| **双层循环** | 外层用 `exchange` 尝试抢锁，内层用 `load` 纯读等待 |
| **为什么内层用 relaxed** | 只是"看着锁什么时候释放"，不需要严格顺序，且 `relaxed` 读不会导致缓存行争抢 |
| **`try_lock` 先 relaxed 读** | 避免在锁被占用时白白产生 cache miss |

**与 `driver.h` 版本的对比**：

```cpp
// driver.h 的写法（GCC 内置）
while (__atomic_exchange_n(&locked, 1, __ATOMIC_ACQUIRE)) {
    while (__atomic_load_n(&locked, __ATOMIC_RELAXED)) {
        asm volatile("yield");        // ARM64 指令
    }
}

// spinlock.h 的写法（标准库）
while (lock_.load(std::memory_order_relaxed)) {
    sched_yield();                    // 系统调用！
}
```

> [!warning] `sched_yield()` 与 `asm("yield")` 有本质区别
> - `asm volatile("yield")` 是**一条 CPU 指令**（纳秒级），提示 CPU 在自旋
> - `sched_yield()` 是**系统调用**（微秒级），会让出整个时间片
>
> 对于"等待时间极短"的场景（驱动通信），用 `asm("yield")` 才对。
> `sched_yield()` 会在每次迭代都陷入内核，开销大得多。
>
> **这也解释了为什么本项目在 `driver.h` 里自己写了一个而不是用 ImGui 的**。

标准库版本还有个细节：`std::atomic` 在 C++20 里有 `wait()`/`notify_one()`，
能用内核 futex 实现真正的阻塞等待，比自旋更省电：

```cpp
// C++20 的更优写法
void lock() noexcept {
    while (lock_.exchange(true, std::memory_order_acquire)) {
        lock_.wait(true, std::memory_order_relaxed);   // 睡眠等待
    }
}
void unlock() noexcept {
    lock_.store(false, std::memory_order_release);
    lock_.notify_one();
}
```

### 3.2 `Utils.h` —— 通用工具函数集

8.3 KB，全是 `inline` 小函数。**这一节的内容正文完全没有覆盖**，逐个说明：

**① 字符串判断（命名风格有点特别）**

```cpp
inline bool isStartWith(const std::string &str, const char *check) {
    return (str.rfind(check, 0) == 0);      // rfind(..., 0) 从位置 0 开始找
}

inline bool isEqual(const std::string &s1, const std::string &s2) {
    return (s1 == s2);
}

inline bool isContain(const std::string &str, const std::string &check) {
    size_t found = str.find(check);
    return (found != std::string::npos);
}
```

教学点：

| 函数 | 常规写法 | 这个写法 | 说明 |
|---|---|---|---|
| 前缀判断 | `str.compare(0, n, check) == 0` | `rfind(check, 0) == 0` | 都是 O(n)，后者更简洁 |
| 单一 `isEqual` | `==` | 保留是因为要兼容 `char*` | 历史原因 |

> [!note] 命名风格提醒
> `isStartWith` / `isEqual` / `isContain` 是"动词 + 形容词"的混搭，
> 标准命名应该是 `startsWith` / `equals` / `contains`。
> **你自己写时建议用后者**——这是英文习惯问题（第三人称单数用 starts）。

**② 裁剪空格**

```cpp
inline void trimStr(std::string &str) {
    str.erase(std::remove(str.begin(), str.end(), ' '), str.end());
}
```

这是 **erase-remove 惯用法**（erase-remove idiom），C++ 里删除元素的固定套路：

```
std::remove 把要保留的元素移到前面，返回"新的逻辑末尾"
str.erase(从该位置到真正的末尾)    删掉尾部垃圾
```

**注意**：这里的 `remove` 只是"搬移"，**不改变 size**；必须配 `erase` 才真正缩短。

**③ 读系统属性**

```cpp
inline std::string getSystemProperty(const char *name) {
    char tmp[PROP_VALUE_MAX]{0};          // 初始化列表，全 0
    __system_property_get(name, tmp);     // Android 专有 API
    return tmp;
}
```

**这解决了第 66 章"如何判断 Android 版本"的问题**：

```cpp
inline int getAndroidSDKLevel() {
    static int var = 0;                    // ★ static 缓存
    if (var > 0) return var;
    var = std::stoi(getSystemProperty("ro.build.version.sdk"));
    return var;
}
```

两个教学点：

| 点 | 说明 |
|---|---|
| `static int var` | 函数内静态变量——**只初始化一次，之后一直存在**（第 04 章讲过存储期） |
| 缓存理由 | `__system_property_get` 每次都读系统属性，有开销 |

> [!danger] `static` 缓存有个隐患
> 如果 SDK 读失败（属性不存在），`std::stoi("")` 会**抛异常**。
> 本项目依赖"Android 设备一定有这个属性"，所以没做错误处理。
> **更稳妥的写法**：
> ```cpp
> inline int getAndroidSDKLevel() {
>     static int var = -1;
>     if (var >= 0) return var;
>     const std::string s = getSystemProperty("ro.build.version.sdk");
>     var = s.empty() ? 0 : std::atoi(s.c_str());    // atoi 不抛异常
>     return var;
> }
> ```

**④ 判断系统语言**

```cpp
inline bool getLocalLanguageIsCN() {
    std::string strs[] = {
        getSystemProperty("persist.sys.locale"),
        getSystemProperty("ro.product.locale"),
        getSystemProperty("persist.sys.country"),
        getSystemProperty("persist.sys.language"),
    };
    return std::any_of(std::begin(strs), std::end(strs), [](const std::string &str) {
        return isContain(str, "zh") || isContain(str, "CN");
    });
}
```

教学点：

| 点 | 说明 |
|---|---|
| **四个属性都查** | 不同 ROM 用不同的属性记录语言，所以要"任一命中即算" |
| `std::any_of` | 标准算法：只要有一个满足就返回 true |
| `std::begin/std::end` | C++11 起的统一写法（也可用 `std::begin(arr)` 兼容 C 数组） |
| lambda 捕获为空 | `[]` 表示不用外部变量 |

**这个函数的用途**：根据系统语言选择加载哪个字体（中文/英文）。

**⑤ 读文件（两个重载）**

```cpp
// 版本 1：读进已有缓冲
inline size_t ReadFile(const char *path, void *buff, size_t size) {
    int fd = open(path, O_RDONLY);
    if (fd > 0) {                          // ★ 严格说应该判断 >= 0
        auto len = read(fd, buff, size);
        close(fd);
        return len;
    }
    return 0;
}

// 版本 2：读成 std::string
inline std::string ReadFile(const std::string &path) { /* ... */ }
```

教学点：

| 点 | 说明 |
|---|---|
| 重载 | 同名函数不同参数——第 08 章的"函数重载" |
| `if (fd > 0)` | **这里有个小瑕疵**：`open` 成功可能返回 0（虽然实践中几乎不会，因为 0/1/2 被标准流占了）。**应该用 `>= 0`** |
| 返回值 | 返回实际读到的字节数（可能小于请求，见第 22 章"短读"） |

**⑥ 随机数（文件后半部分）**

`.h` 里还包含随机数相关工具（用于第 67 章的图层名随机后缀、层级随机偏移）。核心是 `std::random` 的使用：

```cpp
#include <random>

// 现代 C++ 随机数（不用 rand()）
std::random_device rd;              // 真随机源（慢）
std::mt19937 gen(rd());             // 梅森旋转引擎
std::uniform_int_distribution<> dist(0, 999);
int v = dist(gen);                  // [0, 999]
```

> [!tip] 为什么不用 `rand()`
> | | `rand()` | `<random>` |
> |---|---|---|
> | 质量 | 差（低位周期短） | 好 |
> | 范围 | 固定 `RAND_MAX` | 任意 |
> | 可重现 | 全局状态，难控制 | 引擎可独立实例化 |
> | 线程安全 | 不一定 | 各用各的引擎 |
>
> **C++ 项目应该用 `<random>`**。

### 3.3 `VectorStruct.h` —— 第三套向量类型

11 KB，定义了 `My_Vector2` 和 `My_Vector3`。

**这就是"三套向量类型并存"的第三套**：

| 文件 | 类型 | 用途 | 谁在用 |
|---|---|---|---|
| `VectorTools.h` | `Vec2` / `Vec3` | 数学计算 | 自瞄、ESP 逻辑 |
| `draw.h` | `Vector2A` / `Vector3A` | 屏幕/世界坐标 | 投影、绘制 |
| `VectorStruct.h` | `My_Vector2` / `My_Vector3` | 触摸坐标 | `TouchHelperA` |

**为什么会有三套**：三个模块由不同时间/不同人写，各自定义了自己的类型。
`My_Vector2` 实现得**最完整**——带全套运算符重载：

```cpp
struct My_Vector2 {
    float x, y;

    inline My_Vector2 operator+(const My_Vector2 &other) const { ... }
    inline My_Vector2 operator+(const float other) const { ... }   // ★ 与标量相加
    inline My_Vector2 operator*(const float value) const { ... }
    inline My_Vector2 operator/(const float value) const {
        if (value != 0) {                       // ★ 除零保护
            return My_Vector2(x / value, y / value);
        }
        return My_Vector2();
    }
    inline My_Vector2 operator-() const { return My_Vector2(-x, -y); }  // ★ 一元负号
    inline My_Vector2 &operator+=(const My_Vector2 &other) { ... }      // ★ 复合赋值
};
```

值得注意的三处：

| 实现 | 说明 |
|---|---|
| **标量运算重载** | `v + 5.0f` 也能用（三个类型里只有它支持） |
| **除零保护** | `operator/` 里判断了 `value != 0`，返回零向量 |
| **复合赋值返回引用** | `operator+=` 返回 `My_Vector2&`，才能支持 `a += b += c` 链式 |

> [!warning] 三套类型并存的代价
> 类型之间要转换，容易出错：
> ```cpp
> // 从 Vec2 转到 My_Vector2
> My_Vector2 mv{v.x, v.y};
> ```
> **你自己写项目时：只定义一套**。
> 如果确实需要不同语义（屏幕坐标 vs 世界坐标），用**类型别名**而不是新结构体：
> ```cpp
> using ScreenPos = Vec2;
> using WorldPos  = Vec3;
> ```
> 这样既表达了语义，又不用写转换代码。

### 3.4 `my_imgui_impl_android.h/cpp` —— Android 输入适配层

**这两个文件是 ImGui 官方示例里的 Android 后端**，本项目只改了个前缀（把 `ImGui_ImplAndroid` 改成 `My_ImGui_ImplAndroid`）。

**6 个函数，各司其职**：

| 函数 | 作用 | 行数 |
|---|---|---|
| `KeyCodeToImGuiKey` | Android 键码 → ImGui 键枚举 | 16–230（约 215 行） |
| `HandleInputEvent` | 处理 `AInputEvent`（新版） | 233–330 |
| `HandleInputEvent_old` | 旧版事件处理 | 332–383 |
| `Init` | 初始化（记录窗口） | 385–394 |
| `Shutdown` | 清理 | 396–399 |
| `NewFrame` | 每帧更新（时间、尺寸） | 401–425 |

**① 键码映射（占了文件一半篇幅）**

```cpp
static ImGuiKey ImGui_ImplAndroid_KeyCodeToImGuiKey(int32_t key_code) {
    switch (key_code) {
        case AKEYCODE_TAB:          return ImGuiKey_Tab;
        case AKEYCODE_DPAD_LEFT:    return ImGuiKey_LeftArrow;
        case AKEYCODE_ENTER:        return ImGuiKey_Enter;
        case AKEYCODE_ESCAPE:       return ImGuiKey_Escape;
        case AKEYCODE_DEL:          return ImGuiKey_Backspace;
        case AKEYCODE_NUMPAD_0:     return ImGuiKey_Keypad0;
        // ... 约 100 个 case
        default:                    return ImGuiKey_None;
    }
}
```

教学点：

| 点 | 说明 |
|---|---|
| **为什么需要映射** | Android 有自己的键码定义（`AKEYCODE_*`），ImGui 有自己的（`ImGuiKey_*`），必须翻译 |
| **`default: return ImGuiKey_None`** | 未识别的键返回"无"，而不是随便返回一个 |
| **纯函数** | 只有输入输出、无副作用——可以放心测试 |

**② 事件处理的两种版本**

```cpp
int32_t My_ImGui_ImplAndroid_HandleInputEvent(AInputEvent *input_event) {
    // 新版：用 io.AddKeyEvent() / io.AddMousePosEvent() 等 API
}

int32_t My_ImGui_ImplAndroid_HandleInputEvent_old(AInputEvent *input_event) {
    // 旧版：直接写 io.KeysDown[] / io.MousePos 等字段
}
```

**为什么有两个**：ImGui 在 1.87 版重构了输入 API——
从"直接写字段"改成"通过 `AddXxxEvent()` 投递事件"（这样能正确处理一帧内的多个事件）。

本项目两个都保留，方便适配不同版本的 ImGui。

**③ 本项目**没有用**它**

这点很重要：

```cpp
// main.cpp 里用的是自己写的触摸处理，不是这个适配层
Touch::Init({(float)abs_ScreenX, (float)abs_ScreenY}, true);
Touch::UpdateImGuiInput();
```

| 方案 | 实现 | 适用 |
|---|---|---|
| `my_imgui_impl_android` | 依赖 `AInputEvent`（来自 Java 层） | APK / 有 Java 层时 |
| **`TouchHelperA`** | 直接读 `/dev/input` | **独立可执行文件**（本项目） |

**因为本项目是原生可执行文件，没有 Java 层，拿不到 `AInputEvent`**，所以自己实现了触摸处理（第 69–71 章）。

> [!note] 这个文件为什么还留在项目里
> 两个可能原因：
> 1. 早期版本用过它，后来换成 `TouchHelperA` 但保留了文件
> 2. 作为参考实现留着，方便对照
>
> **这正是"遗留代码"的典型形态**。你维护自己的项目时会遇到同样情况——
> 记得在注释里标明"已废弃，改用 XXX"，否则半年后自己都搞不清。

### 3.5 `gui_icon.h` + `fontawesome-*.h` —— 图标字体

四个文件共 1.4 MB，作用是**让 UI 能显示图标**（而不是只能用文字）。

**① 三个文件的分工**

| 文件 | 内容 | 大小 |
|---|---|---|
| `fontawesome-solid.h` | 实心图标字体（二进制数组） | 758 KB |
| `fontawesome-regular.h` | 线框图标字体 | 117 KB |
| `fontawesome-brands.h` | 品牌图标（GitHub、Twitter 等） | 529 KB |
| `gui_icon.h` | 图标**码点常量**定义 | 137 KB |

**② 字体文件是怎么转成 C 数组的**

`fontawesome-solid.h` 的开头写明了来源和方法：

```cpp
// File: '/sdcard/视频文件夹[i]/webfonts/fa-solid-900.ttf' (419720 bytes)
// Exported using binary_to_compressed_c.cpp
const unsigned int font_awesome_solid_compressed_size = 244005;
const unsigned int font_awesome_solid_compressed_data[244008/4] = { ... };
```

| 信息 | 含义 |
|---|---|
| 原始 419,720 字节 | `fa-solid-900.ttf` 的原始大小 |
| 压缩后 244,005 字节 | 压缩率约 42% |
| `binary_to_compressed_c.cpp` | ImGui 官方提供的转换工具 |
| `_compressed_` | **注意：这是压缩过的**，使用时要先解压 |

**③ 用法的关键步骤**

```cpp
// 1. 解压（ImGui 提供了 ImFontAtlas 的解压支持）
//    在 imconfig.h 里定义：
#define IMGUI_ENABLE_FREETYPE       // 或用内置的压缩解压
#define IMGUI_USE_WCHAR32

// 2. 或直接加载未压缩的版本（本项目用的是 heiti 字体，不是 FontAwesome）
```

**④ 图标码点的使用**

`gui_icon.h` 定义了每个图标对应的 Unicode 码点：

```cpp
#define ICON_FA_0 "\x30"    // U+30
#define ICON_FA_2 "\x32"    // U+32
#define ICON_MIN_FA 0x21
#define ICON_MAX_FA 0xf8ff

// 实际使用时（FontAwesome 的图标都在 U+F000~U+F8FF 私有区）
#define ICON_FA_COG      "\xef\x80\x93"     // U+F013 齿轮
#define ICON_FA_SEARCH   "\xef\x80\x82"     // U+F002 放大镜
```

**为什么用 `\xef\x80\x93` 这种多字节转义**：那是 UTF-8 编码的 U+F013。

**⑤ 完整的图标使用流程**

```cpp
// 步骤 1：加载图标字体（和加载中文字体一样）
ImFontConfig iconCfg;
iconCfg.MergeMode = true;        // ★ 合并到已有字体，不新建
iconCfg.GlyphMinAdvanceX = 20.0f;

static const ImWchar iconRanges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
io.Fonts->AddFontFromMemoryCompressedTTF(
    font_awesome_solid_compressed_data,
    font_awesome_solid_compressed_size,
    20.0f, &iconCfg, iconRanges);

// 步骤 2：在 UI 里用
ImGui::Button(ICON_FA_COG "  设置");        // 显示效果：齿轮图标 + "设置"
ImGui::Text(ICON_FA_SEARCH " 搜索");
```

**`MergeMode = true` 是关键**：它让图标字形"合进"中文字体，
这样你可以混排"文字 + 图标"，而不用切换字体。

> [!tip] 图标的实际价值
> 手机上屏幕小，**图标比文字省空间**。
> 一个齿轮图标 ≈ 一个字块，但"设置"两个字要占两倍宽度。
>
> **代价**：字体体积增加（1.4 MB）、加载稍慢。
> 本项目最终没用（用了纯文字 UI），但保留了文件。

### 3.6 `stb_image.h` / `stb_image.cpp` —— 图片加载

| 文件 | 大小 | 作用 |
|---|---|---|
| `stb_image.h` | 24 KB | 图片解码声明（单头文件库） |
| `stb_image.cpp` | 261 KB | 解码实现（**注意：实现被编译成一个 .cpp**） |

**① stb 库的特殊写法**

stb 系列是"单文件库"（single-header library）。用法很特别：

```cpp
// 在一个 .cpp 里定义这个宏，才会编译实现
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
```

**其他文件只 include 头文件**（不定义宏），拿到的是声明。

**为什么这样设计**：

| 优点 | 缺点 |
|---|---|
| 只有一个文件，方便集成 | 编译时间长（261 KB 的实现） |
| 没有构建系统负担 | 每个项目都要重复编译 |

**本项目的做法**：把实现单独放在 `src/ImGui/stb_image.cpp`，
这样只有它编译一次（其他文件只 include 头文件）。

**② 支持的格式**

STB Image 支持：PNG、JPG、BMP、TGA、GIF、PSD、HDR、PNM。

**不支持**：WebP、AVIF、SVG。

**③ 核心 API（只有 4 个函数）**

```cpp
// 1. 从文件加载
int w, h, channels;
unsigned char *data = stbi_load("icon.png", &w, &h, &channels, 4);
//                                                         ↑ 强制 4 通道（RGBA）

// 2. 从内存加载（本项目内嵌资源时用这个）
unsigned char *data = stbi_load_from_memory(buf, len, &w, &h, &channels, 4);

// 3. 释放（不是 free！）
stbi_image_free(data);

// 4. 错误信息
const char *reason = stbi_failure_reason();
```

**④ 与 ImGui 结合：创建纹理**

```cpp
// 完整流程
int w, h, ch;
unsigned char *pixels = stbi_load("logo.png", &w, &h, &ch, 4);
if (!pixels) { printf("加载失败: %s\n", stbi_failure_reason()); return; }

// 创建 ImGui 纹理（内部会调 Vulkan 的 vkCreateImage 等）
ImTextureID texId = ImGui::GetIO().Fonts->TexID;      // 简化的示例
// 实际要用后端提供的创建函数（本项目在 AndroidImgui::LoadTextureFromMemory 里）

// 用完后释放 CPU 端数据
stbi_image_free(pixels);
```

**⑤ 本项目对它的封装**

`AndroidImgui` 提供了三个加载接口：

```cpp
TextureInfo LoadTextureFromFile(const char *filepath);
TextureInfo LoadTextureFromMemory(void *data, int len);
TextureInfo_gif LoadTextureFromMemory_gif(void *data, int len);
```

内部流程：

```
stbi_load → 拿到 RGBA 像素
    ↓
后端 CreateTexture（Vulkan: 建 VkImage + 上传）
    ↓
返回 TextureInfo（GPU 句柄 + 宽高）
    ↓
ImGui::Image(texId, size) 绘制
```

**⑥ GIF 支持**

`LoadTextureFromMemory_gif` 说明项目还支持 GIF（多帧）——
需要把每帧都上传成独立纹理，再按延迟切换。

> [!tip] 本项目实际用了吗
> `Draw_ESP.cpp` 和 `draw_Gui.cpp` 里**没有**调用图片加载。
> 所以 `stb_image` 也是**预留能力**——项目有"显示图片"的底层支持，
> 但当前 UI 是纯文字 + 矢量绘制（方框/线/圆）。
>
> **预留能力是常见的工程做法**：需要时立刻能用，不用临时集成库。

### 3.7 `draw_Gui.cpp.bak` —— 备份文件

34,666 字节。`.bak` 是 backup（备份）的缩写。

**它和你手动复制文件的区别**：

| 方式 | 文件名 | 常见来源 |
|---|---|---|
| 手动复制 | `draw_Gui copy.cpp` | 编辑器 |
| **扩展名备份** | `draw_Gui.cpp.bak` | `sed -i.bak`、某些编辑器、构建脚本 |
| 版本控制 | — | Git（**推荐**） |

**教学点：`.bak` 文件的处理**

```bash
# 1. 看它和当前版本的差异
diff jni/src/Android_draw/draw_Gui.cpp.bak jni/src/Android_draw/draw_Gui.cpp

# 2. 差异很大？先看统计
diff ... | wc -l

# 3. 只在确认没用后删除
rm jni/src/Android_draw/draw_Gui.cpp.bak
```

> [!warning] `.bak` 混在源码目录的隐患
> 1. **构建系统可能误吃**：如果 `LOCAL_SRC_FILES` 用了通配符（`$(wildcard src/**/*.cpp)`），
>    `.bak` 不会被当 `.cpp`；但如果通配的是 `*`，就可能出问题
> 2. **IDE 索引混乱**：VS Code 会把 `.bak` 当纯文本，但在文件列表里干扰视线
> 3. **搜索污染**：`grep -r "某个函数" src/` 会搜到 `.bak`，得到重复结果
>
> **正确做法**：用 Git 管版本，`.bak` 只作临时应急（并且及时清理）。
> 加进 `.gitignore`：
> ```
> *.bak
> *.tmp
> *.orig
> ```

**本项目为什么有这个文件**：说明开发过程中某次大改动前做了备份。
**教程的建议**：学完卷三第 44 章后，把它删掉或移出源码目录——
它不参与构建，但会干扰阅读。

### 3.8 `include/Embree/foundation/*.h`（26 个）—— PhysX 风格数学基础

这是 PhysX SDK 的数学库（Embree 与 PhysX 同源，都用这套）。
**正文只在第 47 章提过一句**，这里逐个说明。

**① 基础类型与工具（8 个）**

| 文件 | 作用 | 关键内容 |
|---|---|---|
| `Px.h` | 总入口头 | 包含其他所有 |
| `PxSimpleTypes.h` | 基础类型定义 | `PxI8/U8/I16/U32/F32/F64` 等 |
| `PxPreprocessor.h` | 预处理宏 | 平台检测、编译期断言 |
| `PxIntrinsics.h` | 内联函数/内建指令抽象 | `PxAbs` `PxSqrt` `PxSin`（跨平台统一） |
| `PxMemory.h` | 内存操作 | `PxMemCopy` `PxMemSet` |
| `PxAllocatorCallback.h` | 分配器回调接口 | 让用户自定义内存分配 |
| `PxErrorCallback.h` | 错误回调接口 | 统一错误上报 |
| `PxErrors.h` | 错误码枚举 | `PxErrorCode::eABORT` 等，配合上两个使用 |
| `PxFoundation.h` | 基础框架 | 把上面两个回调串起来 |
| `PxFoundationVersion.h` | 版本号 | 编译期版本检查 |

**`PxIntrinsics.h` 是本项目间接依赖的关键**：它把 `sqrtf`/`sinf` 等
包装成跨平台一致的形式，Embree 内部大量使用。

**② 数学类型（12 个）**

| 文件 | 类型 | 本项目对应 |
|---|---|---|
| `PxVec2.h` | 二维向量 | `Vec2` |
| `PxVec3.h` | **三维向量** | **`Vec3`** |
| `PxVec4.h` | 四维向量 | （无，SIMD 用） |
| `PxQuat.h` | **四元数** | **`Quat`** |
| `PxMat33.h` | 3×3 矩阵 | （无，本项目用 4×4） |
| `PxMat44.h` | **4×4 矩阵** | **`Matrix`** |
| `PxTransform.h` | **位置 + 旋转** | `BoneTransform` 的概念来源 |
| `PxPlane.h` | 平面 | （无） |
| `PxBounds3.h` | **轴对齐包围盒** | 第 96 章的 `AABB` |
| `PxStrideIterator.h` | **跨步迭代器** | 第 07 章 `BONE_STRIDE` 的道理 |
| `PxUnionCast.h` | 联合体类型转换 | 第 26 章的位级转换 |
| `PxBitAndData.h` | 指针低位复用 | 用指针的低位存标志（省内存） |

**三个最值得看的**：

**`PxTransform.h`** —— 本项目 `BoneTransform` 的原型：

```cpp
struct PxTransform {
    PxQuat q;       // 旋转
    PxVec3 p;       // 位置
};
```

本项目在 `PhysX.h` 里用的就是这个结构（第 98 章的变换就是它的字段）。

**`PxBounds3.h`** —— 第 96 章 BVH 用的包围盒：

```cpp
struct PxBounds3 {
    PxVec3 minimum;
    PxVec3 maximum;

    PxVec3 getExtents() const { return maximum - minimum; }
    PxVec3 getCenter() const   { return (minimum + maximum) * 0.5f; }
    float  getVolume() const   { /* ... */ }
    void   include(const PxVec3 &p) { /* 扩展以包住这个点 */ }
};
```

第 96 章的 `AABB` 就是它的简化版。

**`PxStrideIterator.h`** —— 解释了"为什么需要步长迭代器"：

```cpp
// 内存里有 [顶点A(16字节)] [顶点B(16字节)] ...，但只想要位置（前12字节）
PxStrideIterator<const PxVec3> it(reinterpret_cast<const PxVec3*>(data), 16);
//                                                                         ↑ 步长
for (...) { DoSomething(*it); ++it; }    // ++it 会自动跳 16 字节
```

**这正是本项目 `BONE_STRIDE = 48` 的场景**——数组元素有填充，必须按固定步长跳。

**③ 平台相关与其他（7 个）**

| 文件 | 作用 |
|---|---|
| `foundation/unix/PxUnixIntrinsics.h` | Unix/Linux 平台的内联函数实现 |
| `PxMath.h` | 数学函数（`PxSin` `PxCos` `PxPow` 等） |
| `PxMathUtils.h` | 数学工具（角度转换、插值） |
| `PxFlags.h` | 类型安全的位标志（`PxFlags<T>`） |
| `PxAssert.h` | 断言宏 |
| `PxIO.h` | 输入输出（序列化辅助） |
| `PxProfiler.h` | 性能剖析接口 |

**`PxFlags.h` 值得单独说**：

```cpp
// C 风格：用 int 存标志位，容易搞混
int flags = FLAG_A | FLAG_B;

// PxFlags：类型安全
PX_FLAGS(PxShapeFlag)
struct PxShapeFlag {
    enum Enum {
        eSIMULATION_SHAPE = (1 << 0),
        eSCENE_QUERY_SHAPE = (1 << 1),
        // ...
    };
};
PxFlags<PxShapeFlag, PxU8> shapeFlags;      // ★ 类型检查
```

**好处**：不能把 `PxShapeFlag` 赋给 `PxActorFlag`（类型不同），
比裸 `int` 安全。这是 C++ 里"用类型系统防错"的经典手法。

### 3.9 `obj/` 中间产物（4 类文件）—— 解读方法

```
obj/local/arm64-v8a/objs/Android_imgui_Vulkan.rc/
├── src/.../*.o          每个源文件编译出的目标文件
├── src/.../*.o.d        依赖文件（记录了 .o 依赖哪些 .h）
└── linker.list          链接器响应文件（列出了所有 .o）
```

**① `.o` 文件**

```bash
# 看它的架构
aarch64-linux-android-readelf -h main.o | grep -E 'Machine|Type'
# Machine: AArch64        Type: REL (可重定位)

# 看它有哪些符号
aarch64-linux-android-nm main.o

# 看它需要什么符号（依赖）
aarch64-linux-android-nm -u main.o

# 反汇编
aarch64-linux-android-objdump -d main.o | head -30
```

**注意 `.o` 的 `e_type` 是 `REL`（可重定位）**，不是 `EXEC`/`DYN`——
因为它还没被链接（第 19 章讲过三种类型）。

**② `.o.d` 文件（依赖文件）**

这是 `-MMD` 生成的（第 29 章讲过）。内容示例：

```makefile
/home/user/klbq/jni/src/main.cpp: \
  /home/user/klbq/jni/include/draw.h \
  /home/user/klbq/jni/include/AndroidImgui.h \
  /home/user/klbq/jni/include/ConfigManager.h
```

**格式**：`目标: 依赖1 依赖2 ...`

**它的作用**：`make` 读它就知道"改了 draw.h 要重编 main.o"。

**自己看它的实用价值**：

```bash
# 这个文件到底 include 了什么？（比翻代码快）
cat main.o.d | tr ' ' '\n' | grep '\.h$' | sort -u
```

**③ `linker.list`**

链接器的响应文件（把几百个 `.o` 路径写进一个文件，避免命令行过长）。

```bash
# 看有多少个目标文件被链接
wc -l obj/local/arm64-v8a/objs/Android_imgui_Vulkan.rc/linker.list

# 看链接顺序（这就是第 28 章讲的"顺序"！）
cat obj/local/arm64-v8a/objs/Android_imgui_Vulkan.rc/linker.list | head -20
```

> [!tip] 排查链接问题的实用技巧
> 遇到 `undefined reference` 时，看 `linker.list` 能确认：
> 1. 某个 `.o` 到底有没有被链接进来
> 2. 链接顺序是什么（顺序错会导致符号找不到）
>
> 这比 `ndk-build V=1` 的输出更容易读（那个太长）。

### 3.10 `libs/` 产物目录

| 文件 | 说明 |
|---|---|
| `libs/arm64-v8a/Android_imgui_Vulkan.rc` | **最终产物**（可执行文件） |

验证方法（第 33 章讲过，这里汇总）：

```bash
# 架构
file libs/arm64-v8a/Android_imgui_Vulkan.rc
# ELF 64-bit LSB executable, ARM aarch64

# 依赖的动态库
readelf -d libs/arm64-v8a/Android_imgui_Vulkan.rc | grep NEEDED
# liblog.so  libandroid.so  libz.so  libc.so  libdl.so

# 大小
ls -lh libs/arm64-v8a/Android_imgui_Vulkan.rc

# 确认已经 strip
nm libs/arm64-v8a/Android_imgui_Vulkan.rc 2>&1 | head -3
# nm: ...: no symbols        ← 说明 strip 生效
```

## 四、专题：三套向量类型并存

**这是本项目最典型的"技术债"案例，值得单独讲。**

```
VectorTools.h   Vec2 / Vec3          ← 数学层
draw.h          Vector2A / Vector3A  ← 绘制层
VectorStruct.h  My_Vector2/3         ← 触摸层
```

**造成的实际问题**：

```cpp
// 场景：把触摸坐标传给投影函数
My_Vector2 touchPos = /* 来自 TouchHelperA */;
Vector3A worldPos = /* 来自内存解析 */;

// 必须手工转换三次
Vector2A screen;
WorldToScreen(Vector3A(worldPos.X, worldPos.Y, worldPos.Z), screen);
Vec2 finalPos{screen.X, screen.Y};
```

**如何避免（给你的项目建议）**：

| 做法 | 说明 |
|---|---|
| **只定义一套 `Vec2/Vec3`** | 数学运算都在它上面 |
| **语义用别名表达** | `using ScreenPos = Vec2;` |
| **需要不同精度时用模板** | `template<typename T> struct Vec2T` + `using Vec2f = Vec2T<float>` |
| **类型转换只在一个地方** | 定义 `toVec3(const Vector3A&)` 集中处理 |

**重构的代价评估**：本项目有 100+ 处使用点，
统一类型需要改所有文件——**风险大于收益**，所以保留现状。
**这正是"技术债"的含义：知道不理想，但改动成本更高。**

**你能从中学到的**：**项目初期把基础类型定好，后期能省大量麻烦。**

## 五、自检：项目文件覆盖清单

用这个清单确认你对项目的理解没有盲区：

### 必读（18 个核心文件）

- [ ] `main.cpp` —— 主循环
- [ ] `driver.h` —— 驱动接口
- [ ] `MemDriver.h` / `SysHal.h` —— 双后端
- [ ] `draw_Gui.cpp` —— 数据与绘制核心
- [ ] `variable.h` —— 全局变量与骨骼
- [ ] `WorldToScreen.h` —— 投影
- [ ] `VectorTools.h` —— 数学类型
- [ ] `GameTools.cpp` —— 数学工具
- [ ] `PhysX.h` —— 物理与可见性
- [ ] `Draw_ESP.cpp` —— 网格绘制
- [ ] `SilentAim.h` / `PovAim.h` / `TouchAim.h` —— 三种自瞄
- [ ] `ANativeWindowCreator.h` —— 窗口创建
- [ ] `TouchHelperA.cpp` —— 触摸
- [ ] `VulkanGraphics.cpp` —— 渲染
- [ ] `ConfigManager.cpp` —— 配置
- [ ] `Android.mk` / `Application.mk` —— 构建

### 知道作用即可（其余 88 个）

| 类别 | 数量 | 要求 |
|---|---|---|
| ImGui 核心 | 12 | 知道哪些是必需的（第 42 章列了 5 个） |
| ImGui 第三方 | 6 | 知道 `imstb_*` 是给字体/文本编辑用的 |
| 图标字体 | 4 | 知道是"预留能力"，本项目未用 |
| stb_image | 2 | 知道是"预留能力" |
| Embree foundation | 26 | 见过名字，知道是数学基础（本附录 3.8） |
| Embree rtcore | 9 | 知道是公共 API（第 97 章） |
| 预编译库 | 6 | 知道链接顺序（第 43 章） |
| 产物与中间产物 | 5 | 知道怎么验证/解读（本附录 3.9、3.10） |
| Android 适配遗留 | 2 | 知道是遗留代码（本附录 3.4） |
| 备份文件 | 1 | 知道该清理（本附录 3.7） |

> [!success] 覆盖度确认
> 本文档完成后，**106 个文件全部有对应教学**：
> - 93 个由正文 100 章覆盖
> - 13 个由本附录补充（3.1–3.10 共 10 节）
>
> 如果你发现还有文件没被提到，欢迎按同样的格式补充到这一节。

→ 返回 [[00-开始之前]]
