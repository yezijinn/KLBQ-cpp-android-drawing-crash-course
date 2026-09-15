---
tags: [教程, 卷五, Android, SurfaceFlinger]
day: 40
aliases: [ch64]
---

# 第 64 章 · SurfaceFlinger 与图层

> [!abstract] 本章目标
> 理解 Android 屏幕合成的完整机制，以及"图层"到底是什么。
> 第 65 章直接用原生 API 创建图层。

## 先看一屏画面由什么组成

你看到的手机屏幕，通常是**多个图层叠在一起**：

```
z-order 高
    ┌──────────────────┐
    │  状态栏           │   SystemUI 的图层
    ├──────────────────┤
    │  悬浮窗/覆盖层     │   ← 我们要创建的
    ├──────────────────┤
    │  游戏画面         │   App 的图层
    ├──────────────────┤
    │  壁纸             │
    └──────────────────┘
z-order 低
```

SurfaceFlinger 负责把它们合成，输出到屏幕。

## SurfaceFlinger 是什么

一个**系统服务进程**（不是库）：

```bash
adb shell ps -A | grep surfaceflinger
# system  1234  1  ... S surfaceflinger
```

它做的事：

```
1. 接收各 App/系统组件提交的图形缓冲
2. 按 z-order 排序
3. 决定用 GPU 还是硬件合成器（HWC）合成
4. 输出到显示面板
```

**App 之间不直接通信，都把内容交给 SurfaceFlinger。**

## BufferQueue：生产者-消费者

每个图层背后是一个 BufferQueue：

```
┌───────────────┐                    ┌──────────────────┐
│  生产者         │                    │  消费者            │
│  (你的程序)     │                    │  (SurfaceFlinger) │
│                │                    │                  │
│ dequeueBuffer  │ ←── 空闲缓冲 ──────│                  │
│ 往里绘制        │                    │                  │
│ queueBuffer    │ ──── 已填充 ──────→│ 合成              │
│                │                    │  releaseBuffer    │
└───────────────┘                    └──────────────────┘
```

三个状态的缓冲：

| 状态 | 含义 |
|---|---|
| FREE | 空闲，可被 dequeue |
| DEQUEUED | 生产者正在画 |
| QUEUED | 画完，等消费者取 |
| ACQUIRED | 消费者正在用 |

通常 2~3 个缓冲轮转（对应交换链的双/三缓冲）。

## ANativeWindow：生产者的接口

NDK 提供的 `ANativeWindow` 就是 BufferQueue 的生产者端封装：

```cpp
#include <android/native_window.h>

ANativeWindow *window = ...;

// 取出一块缓冲来画
ANativeWindow_Buffer buffer;
ARect dirty = {0, 0, w, h};
ANativeWindow_lock(window, &buffer, &dirty);
// 直接写 buffer.bits（CPU 软件绘制）
ANativeWindow_unlockAndPost(window);
```

或者交给 Vulkan/OpenGL（GPU 绘制）：

```cpp
// Vulkan
vkCreateAndroidSurfaceKHR(instance, &info, nullptr, &surface);
```

**两种方式对应同一个 BufferQueue。**

## 关键 C++ 类（libgui.so）

Android 的原生图形层是 C++（不是 NDK 公开 API）：

| 类 | 作用 |
|---|---|
| `SurfaceComposerClient` | 与 SurfaceFlinger 通信的客户端 |
| `SurfaceControl` | 一个图层的句柄 |
| `Surface` | 图层里的可绘制表面（继承 `ANativeWindow`） |
| `Transaction` | 批量修改图层属性并原子提交 |

关系：

```
SurfaceComposerClient
    │  createSurface(name, w, h, format, flags)
    ↓
SurfaceControl  ──getSurface()──→  Surface (ANativeWindow)
    │
    │  Transaction{ setLayer, setPosition, ... }.apply()
    ↓
  SurfaceFlinger 更新图层属性
```

## 标准创建流程（C++）

```cpp
#include <gui/SurfaceComposerClient.h>
#include <gui/SurfaceControl.h>
#include <ui/DisplayInfo.h>

using namespace android;

// 1. 创建客户端（连接 SurfaceFlinger）
sp<SurfaceComposerClient> client = new SurfaceComposerClient();
if (client->initCheck() != NO_ERROR) { /* 失败 */ }

// 2. 创建图层
sp<SurfaceControl> sc = client->createSurface(
    String8("MyOverlay"),      // 名字
    1080, 2400,                // 宽高
    PIXEL_FORMAT_RGBA_8888,    // 像素格式
    ISurfaceComposerClient::eFXSurfaceBufferState,  // 类型
    nullptr);                  // parent（nullptr = 顶层）

// 3. 配置图层
SurfaceComposerClient::Transaction t;
t.setLayer(sc, INT_MAX - 1);              // 层级：最高
t.setPosition(sc, 0, 0);
t.setSize(sc, 1080, 2400);
t.show(sc);
t.apply();                                 // 原子提交

// 4. 拿到 ANativeWindow 来绘制
ANativeWindow *window = sc->getSurface().get();
```

> [!warning] 这不是 NDK 公开 API
> `libgui.so` 是系统私有库：
> - App（.so）在 Android 7+ 无法 `dlopen`（第 32 章）
> - **原生可执行文件可以**
> - 符号名随版本变化（第 66 章解决）

## 图层属性

`Transaction` 能设置很多东西：

| 方法 | 作用 |
|---|---|
| `setLayer(z)` | z-order，越大越靠上 |
| `setPosition(x, y)` | 位置 |
| `setSize(w, h)` | 尺寸 |
| `setAlpha(a)` | 整体透明度 |
| `setFlags(...)` | 各种标志 |
| `setMatrix(...)` | 变换矩阵 |
| `setCrop(...)` | 裁剪区域 |
| `setLayerStack(id)` | 显示到哪个屏幕 |
| `setMetadata(...)` | 元数据（Android 10+） |
| `show()` / `hide()` | 显示/隐藏 |

**`Transaction` 是原子的**——所有修改要么全生效，要么全不生效。
这避免了"先移动后缩放"中间态被看到。

## z-order：谁在上面

```
层级值越大，越靠上
```

常见的层级（Android 源码定义）：

| 层级 | 用途 |
|---|---|
| 0 | 壁纸 |
| 0~20000 | App 窗口 |
| 20000+ | 系统 UI（状态栏） |
| 21000 | 导航栏 |
| **INT_MAX** | 理论最高 |

> [!tip] 本项目为什么用 INT_MAX 附近
> ```cpp
> t.setLayer(sc, INT_MAX - rand() % 1000);
> ```
> 保证覆盖层在所有内容之上，
> 随机减一点是避免与其它用 INT_MAX 的图层冲突。

## 像素格式

```cpp
PIXEL_FORMAT_RGBA_8888     // 32 位，带 alpha ★ 覆盖层需要
PIXEL_FORMAT_RGBX_8888     // 32 位，alpha 忽略
PIXEL_FORMAT_RGB_565       // 16 位，省内存
```

**覆盖层必须用带 alpha 的格式**（RGBA_8888），
配合 Vulkan 的 `compositeAlpha = POST_MULTIPLIED`，
没画的地方就是透明的。

## 用 dumpsys 观察图层

```bash
adb shell dumpsys SurfaceFlinger
```

输出（简化）：

```
Display 0 HWC layers:
-----------------------------------------------
 Layer name
           Z |  Comp Type |   Disp Frame (LTRB) |  Source Crop (LTRB)
-----------------------------------------------
 com.example.app/com.example.MainActivity#0
       21015 |     Device |    0    0 1080 2400 |    0.0    0.0 1080.0 2400.0
- - - - - - - - - - - - - - - - - - - - - - - -
 StatusBar#0
       21100 |     Device |    0    0 1080   80 |    0.0    0.0 1080.0   80.0
- - - - - - - - - - - - - - - - - - - - - - - -
 MyOverlay#0
  2147483000 |     Device |    0    0 1080 2400 |    0.0    0.0 1080.0 2400.0
```

| 列 | 含义 |
|---|---|
| Layer name | 图层名（`#0` 是序号） |
| Z | z-order |
| Comp Type | `Device` = 硬件合成；`Client` = GPU 合成 |
| Disp Frame | 屏幕上的位置 |
| Source Crop | 源裁剪 |

**这是调试覆盖层最有用的命令**——
能确认你的图层是否创建成功、层级对不对、位置对不对。

## 硬件合成 vs GPU 合成

| | 硬件合成 (HWC) | GPU 合成 |
|---|---|---|
| 执行者 | 显示控制器 | GPU |
| 速度 | 快、省电 | 慢、耗电 |
| 限制 | 图层数有限（通常 4~8 个） | 无限制 |

超过硬件层数上限时，SurfaceFlinger 会把部分图层交给 GPU 先合成成一张，
再交给硬件。

**覆盖层会增加一个图层**——如果设备硬件层数已满，可能触发 GPU 合成，
导致游戏掉帧。这是覆盖层的隐性代价。

## 显示设备与 layerStack

多屏（投屏、外接显示器）时，每个屏幕有一个 `layerStack`：

```
Display 0 (内置屏幕)  layerStack = 0
Display 1 (外接)      layerStack = 1
Display 2 (虚拟屏)    layerStack = 2
```

图层通过 `setLayerStack(id)` 决定显示在哪个屏幕。
**默认是 0（内置屏幕）**。

第 68 章讲多屏镜像时会用到这个。

## 动手：观察你设备的图层

```bash
# 1. 完整图层列表
adb shell dumpsys SurfaceFlinger | head -60

# 2. 只看图层名和 z-order
adb shell dumpsys SurfaceFlinger | grep -E '^ [A-Za-z]|Z \|'

# 3. 显示设备信息
adb shell dumpsys display | grep -E 'DisplayDeviceInfo|layerStack|mDisplayId'

# 4. 硬件合成能力
adb shell dumpsys SurfaceFlinger | grep -i 'hwc\|max layers'
```

做三件事：
1. 记录当前有哪些图层、各自的 z 值
2. 打开一个 App，看图层怎么变
3. 思考：如果我要在所有内容之上加一层，z 应该设多少？

## 验收清单

- [ ] 知道屏幕画面是多个图层合成的（说出状态栏/导航栏/App 各是图层）
- [ ] 理解 BufferQueue 的生产者-消费者模型（说出谁生产、谁消费）
- [ ] 知道 `SurfaceComposerClient` / `SurfaceControl` / `Surface` 的关系（画出三者层级）
- [ ] 知道 `Transaction` 是原子批量修改（说出"要么全生效、要么全不生效"）
- [ ] 知道 z-order 越大越靠上，以及常见层级数值（说出状态栏/导航栏的大致值）
- [ ] 知道覆盖层需要 `PIXEL_FORMAT_RGBA_8888`（说出为什么需要 alpha）
- [ ] 会用 `dumpsys SurfaceFlinger` 看图层列表 ★（运行命令，找到图层名和 Z 值）
- [ ] 知道硬件合成有层数上限，覆盖层会占一个（说出超出会退化到 GPU 合成）

→ 下一章：[[第65章-dlsym-libgui创建窗口]]　—— 理解为什么不用 SDK 也能创建图层，以及 C++ 符号名（mangled name）是怎么回事。
