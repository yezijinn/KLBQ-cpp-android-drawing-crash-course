---
tags: [教程, 附录, 施工指南, 路线图]
aliases: [附录P, buildguide]
---

# 附录 P · 从零重建路线图

> [!abstract] 这篇解决什么
> 前面 100 章是"知识"，这篇是**"施工顺序"**。
> 八个阶段，每个阶段产出一个**可运行的里程碑**，每个阶段给出：
> 目录结构、文件清单、代码骨架、验证方法、常见坑。
>
> **照着做，你就能从空目录走到完整项目。**

## 总览：八阶段与八里程碑

| 阶段 | 对应卷 | 里程碑（可运行的产物） | 工时 |
|---|---|---|---|
| 1 | 卷一 | 多文件 C++ 程序 + Makefile | 10 天 |
| 2 | 卷二 | 进程体检工具（readelf/maps） | 10 天 |
| 3 | 卷三 | `hello_arm64` 在真机运行 | 8 天 |
| 4 | 卷四 | `mathlib` + `w2s_demo` 测试全绿 | 8 天 |
| 5 | 卷五 | `overlay` 透明悬浮窗 + 中文菜单 | 11 天 |
| 6 | 卷六 | `memview` + 读写 demo 进程 | 5 天 |
| 7 | 卷七 | 遍历/名字/骨骼全部对上真值 | 4 天 |
| 8 | 卷八 | `coverlay` 毕业项目 | 4 天 |

**每个阶段结束都要满足"验收清单"才进入下一阶段。**
跳阶段 = 后面加倍痛苦（因为依赖没搭好）。

## 阶段 1 · 语言与工具（对应卷一）

### 目标

能编译一个**多文件、带 Makefile、能调试**的 C++ 程序。

### 目录结构

```
stage1/
├── Makefile
├── include/
│   └── vec.h
└── src/
    ├── main.cpp
    └── vec.cpp
```

### 文件骨架

```makefile
# Makefile
CXX     := g++
CXXFLAGS:= -std=c++17 -Wall -Wextra -O2 -MMD -MP
TARGET  := stage1
SRCS    := $(wildcard src/*.cpp)
OBJS    := $(SRCS:.cpp=.o)
DEPS    := $(OBJS:.o=.d)

all: $(TARGET)
$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $@
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@
clean:
	rm -f $(OBJS) $(DEPS) $(TARGET)
-include $(DEPS)
.PHONY: all clean
```

```cpp
// include/vec.h
#pragma once
#include <cmath>
struct Vec3 {
    float x = 0, y = 0, z = 0;
    float Length() const { return std::sqrt(x*x + y*y + z*z); }
    Vec3 Normalized() const;
};
```

```cpp
// src/vec.cpp
#include "vec.h"
Vec3 Vec3::Normalized() const {
    const float len = Length();
    return len < 1e-6f ? Vec3{} : Vec3{x/len, y/len, z/len};
}
```

```cpp
// src/main.cpp
#include "vec.h"
#include <cstdio>
int main() {
    const Vec3 v{3, 4, 0};
    const Vec3 n = v.Normalized();
    printf("len=%.3f n=(%.3f,%.3f,%.3f)\n", v.Length(), n.x, n.y, n.z);
    return n.Length() > 0.999f && n.Length() < 1.001f ? 0 : 1;
}
```

### 验证清单

- [ ] `make` 编译通过，`./stage1` 输出 `len=5.000 n=(0.600,0.800,0.000)`
- [ ] 改 `vec.h` 后 `make` **只重编必要文件**（验证 `-MMD` 生效）
- [ ] `make clean` 后能重建
- [ ] 故意制造 `undefined reference`，能看懂错误
- [ ] 用 gdb `break main` + `p v` 能看到变量
- [ ] `-fsanitize=address` 跑一遍无报错

### 常见坑

| 坑 | 症状 | 解法 |
|---|---|---|
| Makefile 用空格缩进 | `missing separator` | 用 Tab |
| 忘了 `#pragma once` | 结构体重复定义 | 加保护 |
| 在 `.h` 里定义全局变量 | `multiple definition` | 改 `extern` + `.cpp` 定义 |
| 返回局部变量引用 | 随机值或崩溃 | 返回值，或改由调用方传缓冲 |

## 阶段 2 · 系统理解（对应卷二）

### 目标

做一个**进程体检工具**：能列进程、解析 maps、读 ELF 头。

### 目录结构

```
stage2/
├── Makefile
├── include/
│   ├── maps.h
│   └── elf.h        （或直接用系统 <elf.h>）
└── src/
    ├── maps.cpp
    ├── elfparse.cpp
    └── main.cpp
```

### 代码骨架

```cpp
// include/maps.h
#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct MapRegion {
    uint64_t start = 0, end = 0;
    uint8_t  perms = 0;        // 1=R 2=W 4=X
    bool     isPrivate = true;
    uint64_t offset = 0;
    std::string path;
};

// 解析 /proc/<pid>/maps 内容
std::vector<MapRegion> ParseMaps(const std::string &text);

// 按模块名找基址（后缀完全匹配，前面必须是 /）
uint64_t FindModuleBase(const std::vector<MapRegion> &maps, const std::string &name);

// 筛选可读写区域并合并相邻
std::vector<std::pair<uint64_t, uint64_t>>
GetScannableRegions(const std::vector<MapRegion> &maps);
```

```cpp
// src/maps.cpp 的关键部分
std::vector<MapRegion> ParseMaps(const std::string &text) {
    std::vector<MapRegion> out;
    size_t pos = 0;
    while (pos < text.size()) {
        const size_t eol = text.find('\n', pos);
        const std::string line = text.substr(pos, (eol == std::string::npos ? text.size() : eol) - pos);
        pos = (eol == std::string::npos) ? text.size() : eol + 1;
        if (line.empty()) continue;

        MapRegion r{};
        char perm[8] = {}; char dev[16] = {}; char path[512] = {};
        unsigned long long s = 0, e = 0, off = 0, ino = 0;
        const int n = sscanf(line.c_str(), "%llx-%llx %4s %llx %15s %llu %511[^\n]",
                             &s, &e, perm, &off, dev, &ino, path);
        if (n < 6) continue;
        r.start = s; r.end = e; r.offset = off;
        if (perm[0]=='r') r.perms |= 1;
        if (perm[1]=='w') r.perms |= 2;
        if (perm[2]=='x') r.perms |= 4;
        r.isPrivate = (perm[3] == 'p');
        if (n >= 7) r.path = path;
        out.push_back(r);
    }
    return out;
}
```

```cpp
// src/main.cpp 的功能
// ./stage2 <pid> [模块名]
// 输出：进程信息 / 模块列表 / 可扫描区域统计 / 指定模块基址
```

### 验证清单

- [ ] `./stage2 <自己的pid>` 能列出所有模块
- [ ] `./stage2 <pid> libc.so` 能输出正确基址
- [ ] 与 `cat /proc/pid/maps` 对照，模块列表一致
- [ ] 用 `nm -D` 对比程序输出的符号数（可选）
- [ ] 能解释为什么模块名匹配要"后缀 + 前一个字符是 `/`"

### 常见坑

| 坑 | 症状 | 解法 |
|---|---|---|
| 用 `strstr` 匹配模块名 | 命中 `libUE4.so.bak` 之类 | 后缀完全匹配 |
| `sscanf` 格式漏了路径 | 路径字段为空 | 用 `%511[^\n]` |
| 没处理 `end <= start` | 出现负长度区域 | 过滤 |
| 把 `/sdcard` 当可执行目录 | 运行失败 | 用 `/data/local/tmp` |

## 阶段 3 · 上手机（对应卷三）

### 目标

`hello_arm64` 在真机上运行，并有**一键脚本**。

### 目录结构

```
stage3/
├── jni/
│   ├── Android.mk
│   ├── Application.mk
│   └── src/
│       └── main.cpp
├── scripts/
│   └── run.sh
└── (libs/ obj/ 由 ndk-build 生成)
```

### 文件骨架

```makefile
# jni/Android.mk
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE    := stage3
LOCAL_CPPFLAGS  := -std=c++17 -Wall -O2
LOCAL_SRC_FILES := src/main.cpp
LOCAL_LDLIBS    := -llog -landroid
include $(BUILD_EXECUTABLE)
```

```makefile
# jni/Application.mk
APP_ABI := arm64-v8a
APP_PLATFORM := android-25
APP_STL := c++_static
APP_OPTIM := debug
```

```cpp
// jni/src/main.cpp
#include <android/log.h>
#include <sys/system_properties.h>
#include <cstdio>
#include <cstdlib>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "stage3", __VA_ARGS__)

int main() {
    char sdk[PROP_VALUE_MAX] = {0};
    __system_property_get("ro.build.version.sdk", sdk);
    LOGI("API=%s", sdk);
    printf("stage3 跑起来了\n");
    return 0;
}
```

```bash
#!/bin/bash
# scripts/run.sh
set -e
TARGET=stage3
REMOTE=/data/local/tmp/$TARGET
ndk-build -j8
adb push libs/arm64-v8a/$TARGET $REMOTE
adb shell chmod 755 $REMOTE
adb logcat -c
adb shell $REMOTE
adb logcat -d -s stage3 | tail -20
```

### 验证清单

- [ ] `ndk-build` 通过，产物在 `libs/arm64-v8a/`
- [ ] `file libs/arm64-v8a/stage3` 显示 `ARM aarch64`
- [ ] push + chmod + 运行成功，看到输出
- [ ] `adb logcat -s stage3` 能看到日志
- [ ] 脚本一条命令跑通全流程
- [ ] **故意改成 `armeabi-v7a` 编译**，在 64 位设备上观察报错

### 常见坑

| 坑 | 症状 | 解法 |
|---|---|---|
| 路径含中文/空格 | 各种诡异失败 | 全英文短路径 |
| 忘了 `chmod` | `Permission denied` | 脚本里加上 |
| 推到 `/sdcard` | 不可执行 | 用 `/data/local/tmp` |
| `APP_PLATFORM` 太高 | 老设备找不到符号 | 降到 25 或更低 |

## 阶段 4 · 数学库（对应卷四）

### 目标

`mathlib` + `w2s_demo`，**测试全绿**。

### 目录结构

```
stage4/
├── Makefile
├── include/
│   └── mathlib.h          ← 全部 inline，单头文件
└── tests/
    └── mathlib_test.cpp   ← 断言测试
```

### 代码骨架

```cpp
// include/mathlib.h 的结构（详见第 53 章）
namespace ml {

constexpr float PI = 3.14159265358979323846f;
constexpr float DEG2RAD = PI / 180.0f;
constexpr float RAD2DEG = 180.0f / PI;
constexpr float EPS = 1e-6f;

struct Vec2 { float x = 0, y = 0; /* 运算符 */ };
struct Vec3 { float x = 0, y = 0, z = 0; /* 运算符 + Dot + Cross + Length + Normalized */ };
struct Rotator { float Pitch = 0, Yaw = 0, Roll = 0; };
struct Quat { float x = 0, y = 0, z = 0, w = 1; };
struct Mat4 { float M[4][4] = {}; /* Identity + Multiply + Translation + ... */ };

// 角度工具
inline float NormalizeAngle(float deg);
inline float AngleDelta(float from, float to);

// 旋转工具
inline Rotator DirectionToRotator(const Vec3 &dir);
inline Vec3    RotatorToForward(const Rotator &r);
inline Quat    QuatFromAxisAngle(const Vec3 &axis, float degrees);
inline Mat4    QuatToMatrix(const Quat &q, const Vec3 &pos, const Vec3 &scale);

// 相机（核心）
struct Camera {
    Vec3 Position; Rotator Rotation; float FOV = 90.0f;
    float HalfWidth = 0, HalfHeight = 0;
    float Kx = 1.0f, Ky = 1.0f;
    float M[16] = {};
    bool BuildMatrix();
    bool WorldToScreen(const Vec3 &world, Vec2 &out) const;
};

} // namespace ml
```

**测试文件**覆盖 6 组（第 54 章）：

```cpp
void TestVec3();       // 长度、归一化、点积、叉积、零向量安全
void TestAngle();      // 归一化、最短路、跨 ±180
void TestRotator();    // 前方向、方向↔角度往返
void TestQuat();       // 单位四元数、轴角、矩阵正交性
void TestMatrix();     // 单位、平移、不可交换、转置
void TestCamera();     // W2S 对称性、透视比例、后方不可见、无效相机
```

### 验证清单

- [ ] `g++ -std=c++17 -Wall tests/mathlib_test.cpp -o t && ./t` 全绿
- [ ] 正前方 100 米投影到屏幕正中心（误差 < 0.5 像素）
- [ ] 距离翻倍 → 偏移减半（比值 = 2.0 ± 0.01）
- [ ] 相机位置全 0 时 `BuildMatrix()` 返回 false
- [ ] 零向量归一化不产生 NaN
- [ ] **故意改错一处**（如 `1-ndc` 改成 `1+ndc`），测试能抓到

### 常见坑

| 坑 | 症状 | 解法 |
|---|---|---|
| 归一化没防除零 | 出现 NaN 并传染 | 判 `len < EPS` |
| 矩阵行/列主序搞混 | 旋转方向反 | 用单位矩阵验证 |
| `atan` 代替 `atan2` | 象限错 | 用 `atan2` |
| `acos` 没 clamp | 输入 1.0000001 → NaN | `std::clamp(x,-1,1)` |
| 角度差没归一化 | 自瞄绕远路 | `NormalizeAngle` |

## 阶段 5 · 覆盖层（对应卷五）

### 目标

`overlay`：透明悬浮窗 + 中文菜单 + 能点击。

### 目录结构

```
stage5/
├── jni/
│   ├── Android.mk
│   ├── Application.mk
│   ├── include/
│   │   ├── ImGui/          ← 引入 ImGui 源码 + ANativeWindowCreator.h
│   │   ├── Vulkan/
│   │   └── Android_draw/
│   └── src/
│       ├── main.cpp
│       ├── ImGui/          ← imgui.cpp 等 5 个 + 字体
│       ├── Vulkan/         ← VulkanGraphics + vulkan_wrapper
│       └── Android_draw/   ← draw_Gui.cpp（自定义 UI）
└── scripts/run.sh
```

### 关键代码骨架

```cpp
// jni/src/main.cpp
int main() {
    // ① 创建图层（替身：先用系统窗口验证渲染，再换 libgui）
    ANativeWindow *win = android::ANativeWindowCreator::Create("myOverlay", w, h, false);
    if (!win) return 1;

    // ② 渲染后端
    auto graphics = GraphicsManager::getGraphicsInterface();
    graphics->Init_Render(win, w, h);

    // ③ 触摸
    Touch::Init({(float)w, (float)h}, true);
    Touch::setOrientation(displayInfo.orientation);

    // ④ 字体与样式
    ImGui::StyleColorsLight();
    LoadChineseFont(25.0f);
    ImGui::GetStyle().ScaleAllSizes(3.0f);

    // ⑤ 主循环
    bool running = true;
    while (running) {
        LimitFrameRate(60);
        Touch::UpdateImGuiInput();
        graphics->NewFrame(true);
        DrawMyUI(&running);
        graphics->EndFrame();
    }

    graphics->Shutdown();
    android::ANativeWindowCreator::Destroy(win);
    return 0;
}
```

**分三步实现，每步都能验证**：

| 步骤 | 做什么 | 验证方法 |
|---|---|---|
| ① 先跑通 Vulkan | 不建图层，用普通窗口 | 能看到清屏颜色 |
| ② 再换透明图层 | `compositeAlpha = POST_MULTIPLIED` | 背景透明、无黑底 |
| ③ 最后加 UI 与触摸 | ImGui 菜单 + 触摸转发 | 菜单能点 |

**不要一次性写完再调试**——分步验证能快速定位问题层。

### 验证清单

- [ ] Vulkan 初始化成功（日志有 `Vulkan 初始化完成`）
- [ ] 图层创建成功：`adb shell dumpsys SurfaceFlinger | grep myOverlay`
- [ ] **背景透明**（没有黑底）
- [ ] 中文正常显示（不显示方块）
- [ ] 菜单能点（触摸生效）
- [ ] 稳定 60fps，有 FPS 显示
- [ ] 换一台不同 Android 版本的设备能跑

### 常见坑（按概率排序）

| 坑 | 症状 | 排查 |
|---|---|---|
| `compositeAlpha` 用 OPAQUE | **黑底** | 查代码那一行 |
| `sType` 漏填 | 创建失败/崩溃 | 验证层会指出 |
| 符号名版本不匹配 | `dlsym` 返回 null | 用符号侦察器 |
| 字体没设 `FontDataOwnedByAtlas=false` | 崩溃 | 检查字体加载 |
| 图层没 `show()` | 看不见 | 调用 `t.show(sc)` |
| z-order 太低 | 被 App 盖住 | `setLayer(INT_MAX - n)` |

## 阶段 6 · 跨进程读写（对应卷六）

### 目标

`memview` + 能读写 demo 进程。

### 目录结构

```
stage6/
├── Makefile
├── include/
│   └── memreader.h        ← 读写类
├── src/
│   ├── memreader.cpp
│   ├── memview.cpp        ← 工具
│   └── target.cpp         ← 靶子进程（自己写的 demo）
└── scripts/
```

### 代码骨架

```cpp
// include/memreader.h
class MemReader {
public:
    explicit MemReader(pid_t pid) : pid_(pid) {}

    int Read(uint64_t addr, void *buf, size_t size) const;   // 循环处理部分成功
    int Write(uint64_t addr, const void *buf, size_t size) const;

    template <typename T> T Read(uint64_t addr) const;        // 失败返回 T{}
    template <typename T> bool Write(uint64_t addr, const T &v) const;

    std::string ReadString(uint64_t addr, size_t maxLen = 128) const;
    bool IsAlive() const;

    // 批量读（性能关键）
    struct Request { uint64_t remote; void *local; size_t size; };
    bool ReadBatch(const std::vector<Request> &reqs) const;

    // 指针链
    uint64_t FollowChain(uint64_t base, std::initializer_list<uint64_t> offs) const;

private:
    pid_t pid_ = 0;
};
```

```cpp
// src/target.cpp —— 靶子进程（自己的）
struct Player {
    int   hp = 100;
    float x = 0, y = 0, z = 0;
    char  name[32] = "Player1";
};

Player g_players[8];          // 全局数组，便于扫描定位
Player *g_current = &g_players[0];

int main() {
    printf("pid=%d\ng_players=%p\ng_current=%p\n", getpid(), (void*)g_players, (void*)g_current);
    fflush(stdout);
    int tick = 0;
    while (true) {
        g_players[0].x = (float)(tick % 100);       // 让数据变化，便于验证
        sleep(1); tick++;
    }
}
```

### 验证清单

- [ ] `./memview <pid>` 能列出 target 的模块
- [ ] 用 `MemReader` 读出 `g_players[0]` 的 hp/x/name，**与 target 打印的一致**
- [ ] 写 `hp = 9999`，target 那边能看到变化
- [ ] 批量读 100 次 vs 逐个读 100 次，**实测快 10 倍以上**
- [ ] 传一个非法地址，返回失败而不是崩溃
- [ ] 杀掉 target，`IsAlive()` 返回 false

### 常见坑

| 坑 | 症状 | 解法 |
|---|---|---|
| 没处理部分成功 | 偶尔读到半截数据 | 循环推进 `iov_base` |
| 字符串没限长 | 越界读到几百 KB | `ReadString` 带 `maxLen` |
| 用 `int` 存地址 | 64 位截断 | 用 `uint64_t` |
| 忘了判空指针链 | 崩 | 每跳 `if (x == 0) return 0` |
| `EPERM` | 权限不足 | root 或检查 SELinux |

## 阶段 7 · 数据解析（对应卷七）

### 目标

遍历对象数组、解析名字、读骨骼，**全部对上真值**。

### 目录结构

```
stage7/
├── include/
│   ├── offsets.h          ← 偏移集中管理
│   ├── engine.h           ← 结构体定义
│   └── validator.h        ← 合法性过滤
├── src/
│   ├── engine.cpp         ← 解析逻辑
│   └── main.cpp
└── demo_engine/           ← 附录 I 的完整引擎（作为靶子）
```

### 代码骨架

```cpp
// include/offsets.h —— 所有偏移集中在这里
struct Offsets {
    // 静态
    static constexpr uint64_t GNAME_POOL = 0xB236B00;
    static constexpr uint64_t UWORLD     = 0xB3EC650;
    static constexpr uint64_t MATRIX_CHAIN = 0xB3C5D00;
    // UWorld / ULevel
    static constexpr uint64_t UWORLD_LEVEL   = 0x30;
    static constexpr uint64_t ULEVEL_ACTORS  = 0x98;
    static constexpr uint64_t ULEVEL_COUNT   = 0xA0;
    // Actor
    static constexpr uint64_t ACTOR_NAMEID   = 0x18;
    static constexpr uint64_t ACTOR_ROOT     = 0x168;
    static constexpr uint64_t ACTOR_MESH     = 0x370;
    // Mesh
    static constexpr uint64_t MESH_C2W       = 0x2A0;
    static constexpr uint64_t MESH_BONES     = 0x578;
    // 相机
    static constexpr uint64_t PC_CONTROLROT  = 0x378;
    static constexpr uint64_t PC_PAWN        = 0x390;
    static constexpr uint64_t PC_CAMERA      = 0x3A8;
    static constexpr uint64_t CAM_LOCATION   = 0x2300;
    static constexpr uint64_t CAM_ROTATION   = 0x230C;
    static constexpr uint64_t CAM_FOV        = 0x2318;
    // 骨骼
    static constexpr uint32_t BONE_STRIDE    = 48;
    // 元信息
    static constexpr const char *VERSION = "1.0.0";
};
```

```cpp
// include/validator.h
class PtrValidator {
public:
    struct Stats {
        uint64_t total = 0, rejected_range = 0, rejected_align = 0, rejected_unmapped = 0;
    };
    bool Check(uint64_t addr);
    const Stats &stat() const { return stat_; }
private:
    Stats stat_{};
};
```

```cpp
// src/engine.cpp 的核心流程
bool ParseWorld(MemReader &mem, uint64_t base, WorldSnapshot &out) {
    // ① 世界与关卡
    const uint64_t world = mem.Read<uint64_t>(base + Offsets::UWORLD);
    if (!IsValidPtr(world)) return false;
    const uint64_t level = mem.Read<uint64_t>(world + Offsets::UWORLD_LEVEL);
    if (!IsValidPtr(level)) return false;

    // ② 数组与计数（带上界检查）
    const uint64_t arr   = mem.Read<uint64_t>(level + Offsets::ULEVEL_ACTORS);
    const int32_t  count = mem.Read<int32_t>(level + Offsets::ULEVEL_COUNT);
    if (!IsValidPtr(arr) || count <= 0 || count > 10000) return false;

    // ③ 一次读完整个指针数组
    std::vector<uint64_t> ptrs(count);
    if (mem.Read(arr, ptrs.data(), count * 8) != (int)(count * 8)) return false;

    // ④ 批量读字段 → 本地解析
    out.actors.clear();
    out.actors.reserve(count);
    for (uint64_t p : ptrs) {
        if (!IsValidPtr(p)) continue;
        ActorSnapshot a;
        a.address = p;
        a.nameId  = mem.Read<uint32_t>(p + Offsets::ACTOR_NAMEID);
        a.name    = GetNameById(mem, base + Offsets::GNAME_POOL, a.nameId);
        a.root    = mem.Read<uint64_t>(p + Offsets::ACTOR_ROOT);
        if (IsValidPtr(a.root)) {
            mem.Read(a.root + 0x1F0, &a.position, sizeof(a.position));
        }
        out.actors.push_back(a);
    }
    return true;
}
```

### 验证清单

- [ ] 对象数组遍历出的**数量与真值一致**
- [ ] 每个对象的**名字与真值一致**（含人机标识）
- [ ] 骨骼坐标与真值一致（用附录 I 的引擎作为靶子）
- [ ] 偏移集中在一个文件，改一处即生效
- [ ] 加 `PtrValidator` 后统计正常（拒绝数为 0）
- [ ] **故意改错一个偏移**，观察统计变化 + 程序优雅降级（不崩）

### 常见坑

| 坑 | 症状 | 解法 |
|---|---|---|
| 忘了判空 | 崩在某个链上 | 每跳判空 |
| `Count` 用了 `Max` | 读到未初始化的垃圾 | 用 `Count` |
| 骨骼步长用了 `sizeof` | 位置全错 | 用 `BONE_STRIDE=48` |
| 索引越界没检查 | 崩 | 检查 `index < boneCount` |
| 名字长度没上界 | 内存爆炸 | `len > 1024` 拒绝 |
| 偏移散落各处 | 更新时改漏 | 集中到 `offsets.h` |

## 阶段 8 · 遮挡判定（对应卷八）

### 目标

`coverlay` 毕业项目：**能判断目标是否被墙挡住**。

### 目录结构

```
stage8/
├── include/
│   ├── physics.h          ← 几何重建 + Embree 封装
│   ├── visibility.h       ← 可见性系统
│   └── （其余沿用阶段 5-7）
├── src/
│   ├── physics.cpp
│   ├── visibility.cpp
│   └── main.cpp
└── libs/                  ← Embree 的 6 个 .a
```

### 代码骨架

```cpp
// include/physics.h
struct TriangleMesh {                  // 重建产物
    std::vector<Vec3>     vertices;
    std::vector<uint32_t> indices;
};

// 从碰撞体数据重建三角形
bool BuildBox(float hx, float hy, float hz, const Mat4 &transform, TriangleMesh &out);
bool BuildTriangleMesh(const std::vector<Vec3> &verts, const std::vector<uint32_t> &idx,
                       const Mat4 &transform, TriangleMesh &out);
bool BuildConvexMesh(const std::vector<Vec3> &verts,
                     const std::vector<std::vector<uint32_t>> &polys,
                     const Mat4 &transform, TriangleMesh &out);
bool BuildHeightField(const std::vector<float> &heights, int rows, int cols,
                      float rowScale, float colScale, float heightScale,
                      const Mat4 &transform, TriangleMesh &out);

// Embree 场景封装
class PhysicsScene {
public:
    bool Init(int threads = 1);
    void Shutdown();

    // 增量更新：新增 + 删除
    void Update(const std::vector<TriangleMesh> &add,
                const std::vector<uint64_t> &removeKeys);
    void Commit();

    // 只判断遮挡（最快路径）
    bool IsOccluded(const Vec3 &from, const Vec3 &to) const;

    // 完整求交（需要命中信息时）
    struct Hit { bool hit = false; float t = 0; int geomID = -1; };
    Hit Raycast(const Vec3 &from, const Vec3 &to) const;

private:
    RTCDevice device_ = nullptr;
    RTCScene  scene_  = nullptr;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
```

```cpp
// IsOccluded 的关键实现（第 97、98 章 + 深挖 H 的优化）
bool PhysicsScene::IsOccluded(const Vec3 &from, const Vec3 &to) const {
    const Vec3 d = to - from;
    if (d.LengthSq() < 1e-6f) return false;

    RTCRay ray{};
    ray.org_x = from.x; ray.org_y = from.y; ray.org_z = from.z;
    ray.dir_x = d.x;    ray.dir_y = d.y;    ray.dir_z = d.z;
    ray.tnear = 0.0f;
    ray.tfar  = 1.0f - 1e-4f;       // ★ 用未归一化方向 + tfar=1，省一次开方
    ray.mask  = 0xFFFFFFFF;
    ray.flags = 0;

    RTCIntersectContext ctx;
    rtcInitIntersectContext(&ctx);

    // ★ rtcOccluded1 比 rtcIntersect1 快 2~3 倍
    rtcOccluded1(scene_, &ctx, &ray);
    return ray.tfar < 0.0f;          // 命中时 tfar 被设为负数
}
```

### 验证清单

- [ ] Embree 库链接成功（`nm -C | grep rtcNewDevice`）
- [ ] 能重建一个盒子的 12 个三角形
- [ ] **三个遮挡测试用例通过**：
  - 相机在墙左、目标在墙右 → 被遮挡 ✓
  - 目标在墙前 → 不被遮挡 ✓
  - 从侧面绕过 → 不被遮挡 ✓
- [ ] 增量更新生效（移动物体后能正确更新）
- [ ] **50 个目标 × 遮挡判定 < 1 ms**（深挖 H 的性能目标）
- [ ] 画面上"被遮挡的目标显示为绿色"

### 常见坑

| 坑 | 症状 | 解法 |
|---|---|---|
| `hit.geomID` 没重置 | 永远"命中" | 每次查询前重置（用 `rtcIntersect1` 时） |
| `tMax` 设无穷 | 目标背后的墙也算遮挡 | 设为到目标的距离 |
| 忘了 Actor 变换 | 物体堆在原点 | 应用 `Actor × Shape` 变换 |
| 索引没加 base 偏移 | 三角形连错 | `base + idx[i]` |
| 退化三角形 | 法线计算失败 | 过滤面积 ≈ 0 的 |
| 每帧全量重建 | 卡顿 | 增量更新 |

## 施工顺序的三个原则

### 原则 1：每阶段必须"能跑"

**不要**写完 8 个阶段再统一调试。
每个阶段的产物都要能独立运行——这样出错时只需排查这一阶段。

### 原则 2：先假后真

| 假的（先做） | 真的（后替换） |
|---|---|
| 用系统窗口调试渲染 | 换成 libgui 透明图层 |
| 用自己的 demo 进程做靶子 | 换成实际目标 |
| 用固定测试数据 | 换成真实读取 |
| 用 CPU 光栅化验证数学 | 换成 Vulkan |

**好处**：把"环境问题"和"逻辑问题"分开排查。
（第 51 章的 `w2s_demo`、第 62 章的 CPU 后端都是这个思路。）

### 原则 3：每阶段留一份可回退的版本

```
stage1/  ← 完成后打包存档
stage2/  ← 基于 stage1，出问题时能对比
...
```

**用 Git 的话**：

```bash
git tag stage1-done
git tag stage2-done
# 出问题时：git diff stage1-done stage2-done 找变化
```

## 完整验收：8 个阶段的产物汇总

| 阶段 | 产物 | 核心能力 |
|---|---|---|
| 1 | 多文件 C++ + Makefile | 语言与构建 |
| 2 | 进程体检工具 | 系统理解 |
| 3 | hello_arm64 | 交叉编译与部署 |
| 4 | mathlib + w2s_demo | 3D 数学 |
| 5 | overlay | 图形与交互 |
| 6 | memview + 读写工具 | 跨进程访问 |
| 7 | 解析器 + offsets.h | 数据模型 |
| 8 | **coverlay** | 全套整合 |

**8 个都完成，你就"从头写出了这个完整项目"。**

> [!success] 与教程的对照
> | 阶段 | 对应章节 | 对应成品 |
> |---|---|---|
> | 1 | 01-16 | — |
> | 2 | 17-30 | — |
> | 3 | 31-44 | `hello_arm64` |
> | 4 | 45-56 | `mathlib` `w2s_demo` |
> | 5 | 57-72 | `overlay` |
> | 6 | 73-82 | `memview` |
> | 7 | 83-92 | 解析器 + `offsets.h` |
> | 8 | 93-100 | `coverlay` |
>
> **六个成品 + 两个中间产物 = 完整项目。**

→ 返回 [[00-开始之前]]
