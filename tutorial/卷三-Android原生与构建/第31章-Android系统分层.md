---
tags: [教程, 卷三, Android, 系统]
day: 21
aliases: [ch31]
---

# 第 31 章 · Android 系统分层

> [!abstract] 本章目标
> 建立 Android 的完整分层图景，知道每层提供什么、你的程序站在哪一层。
> 后面所有章节都在这张图上定位。

> [!note] 承上
> 卷一卷二讲的都是"电脑上的程序"。
> 从本章起进入卷三：**让程序跑在手机上**。先建立 Android 的完整分层图景。

## 先看清全景

```
┌─────────────────────────────────────────────────────┐
│  应用层        App（Java/Kotlin）                     │  ← 普通 App 在这
│                SystemUI / Launcher / 设置             │
├─────────────────────────────────────────────────────┤
│  框架层        ActivityManager / PackageManager       │
│                WindowManager / SurfaceFlinger 客户端  │
│                (Java API + Binder IPC)               │
├─────────────────────────────────────────────────────┤
│  原生库层      libc / liblog / libandroid             │
│                libgui / libutils / SurfaceFlinger     │  ← ★ 本项目在这
│                libvulkan / libEGL / libGLESv2        │
│                ART 虚拟机 / libc++                    │
├─────────────────────────────────────────────────────┤
│  硬件抽象 HAL  Camera / Audio / Sensors / Gralloc     │
├─────────────────────────────────────────────────────┤
│  Linux 内核    进程调度 / 内存管理 / 驱动 / Binder     │
└─────────────────────────────────────────────────────┘
```

## 本章术语速查（先混个脸熟）

本章会出现不少英文缩写，**第一次见不用记住，遇到时回来查这张表即可**：

| 缩写 | 全称 | 一句话解释 |
|---|---|---|
| **NDK** | Native Development Kit | 谷歌给的一套工具，让你用 C/C++ 写 Android 能跑的程序。第 33 章装它 |
| **HAL** | Hardware Abstraction Layer（硬件抽象层） | 夹在"内核驱动"和"上层框架"之间的一层接口，把各家不同的硬件统一成同一套调用 |
| **Surface** | （无缩写，就是"表面/画布"） | 一块可以被绘制、再交给系统合成显示的内存缓冲区 |
| **BufferQueue** | （缓冲区队列） | 生产者和消费者之间传图像缓冲的队列。App 往里面放画面，SurfaceFlinger 从里面取 |
| **ANativeWindow** | （Android Native Window） | 原生层操作"窗口/画布"的 C 结构体，NDK 提供 |
| **SurfaceFlinger** | （合成器） | Android 里负责把**所有图层**叠成最终画面、送到屏幕的那个系统进程 |
| **z-order** | z 轴顺序 | 图层的前后叠放次序。z 越大越靠前、盖住后面的 |
| **HWC** | Hardware Composer（硬件合成器） | 专门做图层合成的硬件模块，比用 GPU 合成更省电 |
| **Ashmem** | Anonymous Shared Memory（匿名共享内存） | 一种跨进程共享大块内存的机制（第 23 章讲过共享内存） |
| **Parcel** | （包裹） | Binder 通信里"打包数据"的容器，类似一个可跨进程传的结构体 |
| **IBinder** | （Binder 接口对象） | 一个"可以跨进程传递的对象引用"，拿着它就能调用别的进程的服务 |
| **UID** | User Identifier（用户标识） | Linux 里标识"你是谁"的编号。Android 给每个 App 分一个独立 UID |

> [!tip] 记不住没关系
> 这些缩写**后面每一章都会反复出现**，见得多了自然记住。
> 现在只要知道"有这么个东西、大概干嘛的"，读到相关章节时再回来看。

## 每一层提供什么

| 层 | 语言 | 你能调用什么 | 举例 |
|---|---|---|---|
| 应用 | Java/Kotlin | Android SDK API | `Activity`、`View` |
| 框架 | Java + C++ | （内部） | `AMS`、`WMS` |
| **原生库** | **C/C++** | **NDK API + 私有 .so** | `ANativeWindow`、`libgui` |
| HAL | C/C++ | 硬件接口 | `camera HAL` |
| 内核 | C | 系统调用 | `open`、`process_vm_readv` |

本项目的程序是一个**原生可执行文件**，直接站在"原生库层"：
- 能用 NDK 公开的 API（`liblog`、`libandroid`）
- 能用 `dlopen` 加载系统私有库（`libgui`、`libutils`）
- 直接发起系统调用（`process_vm_readv`）
- 有 root 时能碰内核模块

**它不属于任何 App，没有 Activity，没有 APK 壳。**

## 关键系统服务

| 服务 | 进程 | 作用 | 与本项目关系 |
|---|---|---|---|
| `zygote` | `zygote64` | App 孵化器 | 所有 App 都是它的子进程 |
| `system_server` | `system_server` | 框架核心 | 管理 Activity、窗口、包 |
| `surfaceflinger` | `surfaceflinger` | 屏幕合成 | **本项目向它申请图层** |
| `servicemanager` | `servicemanager` | Binder 服务注册中心 | 查服务要找它 |
| `adbd` | `adbd` | adb 守护 | 你 push 文件靠它 |

```bash
adb shell ps -A | grep -E 'zygote|system_server|surfaceflinger'
```

## SurfaceFlinger：屏幕的主人

Android 上**所有**可见内容都由 `surfaceflinger` 合成：

```
状态栏图层     ┐
导航栏图层     │
App 窗口图层   ├──→ SurfaceFlinger 合成 ──→ 显示到屏幕
悬浮窗图层     │
本项目图层     ┘
```

每个图层是一个 `Surface`（底层是 `ANativeWindow` + BufferQueue）。
SurfaceFlinger 按 z-order 排序，用 GPU 或硬件合成器（HWC）混合。

> [!note] 这就是"悬浮窗"的本质
> 不是你的程序在画屏幕，而是你申请了一个图层，把内容画进去，
> 由 SurfaceFlinger 把它和其它内容叠在一起。
> 第 64~66 章会带你直接调用 SurfaceFlinger 的原生接口。

## Binder：Android 的 IPC 脊柱

Android 上几乎所有跨进程通信都用 Binder：

```
客户端进程                    内核 Binder 驱动              服务进程
   │                              │                          │
   ├── transact(code, data) ──→   │  ──→ 分发 ──→            │
   │                              │            onTransact()  │
   │  ←── reply ───────────────   │  ←────────────────────── │
```

| 概念 | 含义 |
|---|---|
| `IBinder` | 一个可跨进程传递的对象引用 |
| `Parcel` | 序列化的数据容器 |
| `IServiceManager` | 服务注册表 |
| `transact(code, data, reply, flags)` | 发起调用 |

原生层的 Binder 接口在 `libbinder.so`。
本项目**没有直接用 Binder**，但它调用的 `SurfaceComposerClient`
内部就是通过 Binder 和 `surfaceflinger` 通信的。

> [!tip] 为什么本项目绕开 Binder 直接用 libgui
> `libgui.so` 里的 `SurfaceComposerClient` 是 C++ 类，
> 它内部封装了 Binder 通信。直接 `dlsym` 拿它的构造函数，
> 比自己手写 Binder 事务（要拼 Parcel、记 transaction code）简单得多。
> 代价：依赖私有 API，不同 Android 版本符号可能变（第 66 章解决）。

## 五种 IPC 在 Android 上的位置

| 机制 | 用在哪 |
|---|---|
| Binder | App ↔ 系统服务（绝对主力） |
| Ashmem（匿名共享内存） | 大块数据共享（如 Bitmap 传输） |
| socket | init ↔ 服务、部分守护进程 |
| 信号 | 进程终止、调试 |
| 共享内存 + 驱动 | 本项目内核驱动方案 |

## 应用沙箱：每个 App 一个 UID

```bash
adb shell ps -A -o USER,PID,NAME | grep -E 'u0_a'
# u0_a123    4567  com.example.app
# u0_a124    4590  com.other.app
```

Android 给每个安装的 App 分配一个**独立的 Linux UID**。
这是"沙箱"的基础：App A 的文件 App B 读不到（除非显式授权）。

| 概念 | 说明 |
|---|---|
| `userId` | 面向用户的编号（多用户） |
| `appId` | 应用编号（10000 起） |
| `uid` | `userId * 100000 + appId` |

要跨 UID 操作，要么：
1. 两个 App 共享 UID（`sharedUserId`，已废弃）
2. 有 root 权限
3. 通过系统服务中转

**本项目走第 2 条。**

## 原生可执行文件的运行方式

Android 上跑原生程序有两条路：

| 方式 | 路径 | 权限 | 说明 |
|---|---|---|---|
| 作为 App 的一部分 | `/data/app/.../lib/arm64/libxxx.so` | App 的 UID | 受沙箱限制 |
| **独立可执行文件** | `/data/local/tmp/xxx` | **执行者的 UID**（root 下就是 root） | **本项目** |

```bash
adb push klbq /data/local/tmp/
adb shell chmod 755 /data/local/tmp/klbq
adb shell su -c /data/local/tmp/klbq
```

> [!warning] /data/local/tmp 是唯一稳妥的位置
> | 路径 | 能执行吗 |
> |---|---|
> | `/sdcard/` | 不能（挂载了 `noexec`） |
> | `/data/local/tmp/` | **能**（且 adb push 可写） |
> | `/system/bin/` | 能，但需要重新挂载为可写 |
> | `/data/data/pkg/` | 能，但需要 root 或 run-as |

## 动手：探索你的设备

```bash
# 1. 系统信息
adb shell getprop ro.build.version.release      # Android 版本
adb shell getprop ro.build.version.sdk          # API 等级
adb shell getprop ro.product.cpu.abi            # CPU 架构
adb shell uname -a                              # 内核版本

# 2. 关键进程
adb shell ps -A | grep -E 'surfaceflinger|zygote|system_server'

# 3. 有哪些原生库
adb shell ls /system/lib64/ | head -40

# 4. SELinux 状态
adb shell getenforce        # Enforcing / Permissive

# 5. 是不是 root
adb shell id
adb shell su -c id
```

把这五条的输出记下来，后面章节都要用。


## 动手验证清单

按顺序执行，每步确认输出符合预期再往下：

- [ ] **确认设备连接**：`adb devices` → 看到 `device` 状态（不是 `unauthorized`）
- [ ] **查 Android 版本**：`adb shell getprop ro.build.version.release` → 如 `13`
- [ ] **查 API 等级**：`adb shell getprop ro.build.version.sdk` → 如 `33`
- [ ] **查 CPU 架构**：`adb shell getprop ro.product.cpu.abi` → 如 `arm64-v8a`
- [ ] **看关键进程**：`adb shell ps -A | grep -E 'surfaceflinger|zygote|system_server'` → 三个都在
- [ ] **看原生库**：`adb shell ls /system/lib64/ | head -20` → 能看到 `libgui.so` 等
- [ ] **查 SELinux**：`adb shell getenforce` → `Enforcing` 或 `Permissive`

> [!tip] 把 7 步输出记下来
> 后面第 33、36、37、66 章都会用到（尤其 API 等级和架构）。

## 验收清单

- [ ] 能画出 Android 五层结构，并说出本项目在哪一层（画五层图，标出"原生库层"）
- [ ] 知道 SurfaceFlinger 是合成所有图层的人（说出"所有可见内容都由它合成"）
- [ ] 知道 Binder 是主力 IPC，以及 `libgui` 内部就用了它（说出 `SurfaceComposerClient` 内部走 Binder）
- [ ] 理解"每个 App 一个 UID"的沙箱机制（用 `adb shell ps -A -o USER,NAME` 看不同 App 的不同 UID）
- [ ] 知道 `/data/local/tmp` 是唯一稳妥的原生程序存放位置（说出为什么 /sdcard 不行：noexec）
- [ ] 跑通了 5 条探索命令，记录下了设备的 Android 版本、API、架构（保存 getprop 输出）

→ 下一章：[[第32章-为什么是可执行文件而非APK]]　—— 理解本项目的形态选择：独立 native 可执行文件 vs APK 里的 .so。
