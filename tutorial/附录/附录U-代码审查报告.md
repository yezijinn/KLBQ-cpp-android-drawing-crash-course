---
tags: [教程, 附录, 代码审查, 缺陷, 改进]
aliases: [附录U, codereview]
---

# 附录 U · 代码审查报告（已知缺陷与改进方案）

> [!abstract] 这篇解决什么
> 前面 100 章讲"项目怎么做的"。这篇讲**"项目里有 23 个问题，以及怎么改"**。
>
> 三个价值：
> 1. **学代码审查**——这是真实项目的真实缺陷，每个都标了检测方法
> 2. **避免重犯**——你在自己项目里会写出一模一样的问题
> 3. **可作为练习**——先自己找一遍，再对照本文
>
> **所有位置已逐一核对过行号。**

## 缺陷总览

| 级别 | 数量 | 后果 |
|---|---|---|
| **【严重】** | 4 | 崩溃、功能完全失效、死锁 |
| **【中等】** | 7 | 功能异常、行为不一致、潜在崩溃 |
| **【轻微】** | 8 | 代码质量、可维护性 |
| **【优化机会】** | 4 | 性能提升空间 |

**如果你在重写这个项目，必须避开前 11 个。**

## 一、严重缺陷（4 个）

### S1. 驱动握手无超时，无驱动时永久阻塞

**位置**：`src/Android_draw/driver.h:1003`

```cpp
LS_LOGI_TAG("Driver", "当前进程 PID=%d，等待驱动握手", getpid());

while (!req->user)         // ★ 没有超时，永远出不来
{
    asm volatile("yield");
}

req->user = false;
```

**症状**：用户选了"内核驱动"模式但设备上没装驱动 → 点"初始化绘制"后**整个程序卡死**，
UI 无响应，只能强杀进程。

**根因**：`req->user` 由内核置位。没有内核，这个位永远是 `false`。

**修法**（加超时 + 状态返回）：

```cpp
bool InitCommunication() {
    prctl(PR_SET_NAME, "LS", 0, 0, 0);

    req = (request_obj *)mmap((void *)0x2025827000, sizeof(request_obj),
                              PROT_READ | PROT_WRITE,
                              MAP_SHARED | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    if (req == MAP_FAILED) {
        LS_LOGE_TAG("Driver", "分配共享内存失败: errno=%d (%s)", errno, strerror(errno));
        req = nullptr;
        return false;
    }
    __builtin_memset(req, 0, sizeof(request_obj));

    // ★ 用超时替代无限等待（3 秒）
    using clock = std::chrono::steady_clock;
    const auto deadline = clock::now() + std::chrono::seconds(3);
    while (!req->user) {
        if (clock::now() >= deadline) {
            munmap(req, sizeof(request_obj));
            req = nullptr;
            LS_LOGE_TAG("Driver", "等待驱动握手超时（3 秒），请确认驱动已加载");
            return false;
        }
        asm volatile("yield");
    }
    req->user = false;
    LS_LOGI_TAG("Driver", "驱动已经连接");
    return true;
}
```

**构造函数的连锁修改**：

```cpp
Driver(int Vslot, bool initGyro, bool initGnss) {
    if (!InitCommunication()) return;      // ★ 失败就提前返回
    InitTouch(Vslot);
    InitGyro(initGyro);
    InitGnss(initGnss);
}

// 并暴露状态给 MemDriver
bool IsReady() const { return req != nullptr; }
```

然后 `MemDriver` 里：

```cpp
bool OpenKernel() {
    if (kernel_ == nullptr) kernel_ = new Driver(10, true, true);
    if (kernel_ == nullptr || !kernel_->IsReady()) {
        error_ = "内核驱动未就绪";
        return false;                       // ★ 不静默失败
    }
    return true;
}
```

**影响范围**：所有使用内核模式的用户。

**检测方法**：

```cpp
// 单元测试：不加载驱动的情况下构造 Driver，应在 3 秒内返回 false
auto t0 = std::chrono::steady_clock::now();
MemDriver d;
bool ok = d.Open(MemDriver::MODE_KERNEL);
auto elapsed = std::chrono::steady_clock::now() - t0;
assert(!ok && elapsed < std::chrono::seconds(5));
```

---

### S2. Embree 场景无锁并发访问

**位置**：`include/Embree/PhysX.h` 的 `VisibleScene::UpdateMesh` + 三个更新线程

```cpp
// 后台线程（PhysX.h 的 VisibleCheck）
thread(&VisibleCheck::UpdateSceneByRange).detach();        // 2s
thread(&VisibleCheck::UpdateDynamicHeightField).detach();  // 3s
thread(&VisibleCheck::UpdateDynamicRigid).detach();        // 300ms

// 后台线程里：修改 mesh_datas
void UpdateMesh(const vector<TriangleMeshData>& willAddMeshs,
                const set<T>& RemoveKey) {
    // ...
    mesh_datas.push_back(mesh_copy);        // ★ 写
    mesh_datas.erase(remove_if(...), mesh_datas.end());   // ★ 写
}

// 主线程（DrawESP → drawMesh）
for (const auto& Mesh : HitMesh) {          // ★ 读
    drawMesh(Mesh.get(), Draw);
}
```

**症状**：

| 表现 | 原因 |
|---|---|
| 偶发崩溃（`mesh_datas` 迭代器失效） | `vector` 扩容时主线程正在遍历 |
| 画面闪过错误网格 | 读到半更新的数据 |
| 难以复现（只在特定时序出现） | 典型的数据竞争 |

**根因**：`std::vector` 的 `push_back` 可能重新分配内存，
此时正在遍历的迭代器全部失效 → 访问已释放内存。

**修法一：读写锁（推荐，读多写少）**

```cpp
#include <shared_mutex>

class VisibleScene {
public:
    // 读路径：多个读者可并发
    std::vector<std::shared_ptr<TriangleMeshData>> Snapshot() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return mesh_datas;                  // ★ 返回副本（shared_ptr 拷贝很轻）
    }

    // 写路径：独占
    void UpdateMesh(...) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        // ... 原来的逻辑
    }

private:
    mutable std::shared_mutex mutex_;
    std::vector<std::shared_ptr<TriangleMeshData>> mesh_datas;
};
```

**为什么返回副本可行**：`shared_ptr` 拷贝只增加引用计数（原子操作，纳秒级），
不拷贝实际的顶点数据。这样读路径拿到的是**稳定的快照**。

**修法二：双缓冲（无锁读）**

```cpp
class VisibleScene {
    std::vector<std::shared_ptr<TriangleMeshData>> buffers_[2];
    std::atomic<int> active_{0};

public:
    const std::vector<...> &ReadBuffer() const { return buffers_[active_.load()]; }

    void UpdateMesh(...) {
        const int cur = active_.load();
        const int next = 1 - cur;
        buffers_[next] = buffers_[cur];      // 拷贝（轻）
        // ... 在 next 上做增删
        active_.store(next);                 // ★ 原子切换
    }
};
```

**优点**：读路径完全无锁，不会阻塞渲染。
**代价**：内存翻倍（但只有 `shared_ptr` 数组，可接受）。

**修法三：只读快照 + 标志位（本项目最省事）**

```cpp
std::atomic<bool> sceneUpdating_{false};

// 后台线程
sceneUpdating_.store(true);
// ... 修改 mesh_datas
sceneUpdating_.store(false);

// 主线程
if (!sceneUpdating_.load()) {
    for (const auto &m : HitMesh) drawMesh(m.get(), Draw);
}
```

**注意**：这个方案的**正确性依赖"修改期间不读"**，
而 `atomic<bool>` 只保证标志本身原子，不保证 `mesh_datas` 的内存可见性顺序。
严格说应该用 `memory_order_acquire/release`：

```cpp
sceneUpdating_.store(true, std::memory_order_release);   // 写线程
// ...
sceneUpdating_.store(false, std::memory_order_release);

if (!sceneUpdating_.load(std::memory_order_acquire)) {   // 读线程
    /* 此时能看到所有写入 */
}
```

**影响范围**：所有开 PhysX 的场景。

**检测方法**：

```bash
# 用 ThreadSanitizer 编译（NDK 支持）
LOCAL_CPPFLAGS += -fsanitize=thread
```

或在 PC 上用等价的单线程模型 + TSan 复现。

---

### S3. `WorldToScreen` 两个分支行为不一致（缺 `isfinite` 检查）

**位置**：`include/Android_draw/WorldToScreen.h:75-99`

```cpp
inline bool WorldToScreen(const Vector3A& obj, Vector2A& screen) {
    // ===== 分支 A：引擎矩阵 =====
    if (ConfigManager::Settings.矩阵W2S && MatrixPtr != 0) {
        // ...
        screen.X = (cx / w + 1.0f) * px;
        screen.Y = (1.0f - cy / w) * py;
        return std::isfinite(screen.X) && std::isfinite(screen.Y);   // ★ 有检查
    }

    // ===== 分支 B：自算矩阵 =====
    float w = matrix[3]*obj.X + ... ;
    if (w < 0.001f) return false;

    screen.X = (clip_x / w * 水平焦距系数 + 1.0f) * px;
    screen.Y = (1.0f - clip_y / w * 垂直焦距系数) * py;
    return true;                                                     // ★ 没有检查
}
```

**症状**：切到"自算矩阵"模式时，如果 `matrix[]` 数组本身含 NaN
（相机数据异常时会发生），会返回 `true` 和一个 NaN 坐标。

**为什么目前没出事**：调用方有 `IsFiniteScreenPoint` 兜底（第 89 章第三道防线）。
**但这属于"靠调用方兜底"，函数自身契约不一致。**

**修法**：

```cpp
    screen.X = (clip_x / w * 水平焦距系数 + 1.0f) * px;
    screen.Y = (1.0f - clip_y / w * 垂直焦距系数) * py;
    return std::isfinite(screen.X) && std::isfinite(screen.Y);   // ★ 与分支 A 一致
}
```

**影响范围**：所有调用者（虽然当前有兜底）。

**检测方法**：构造一个含 NaN 的 `matrix[]`，断言两个分支都返回 false。

---

### S4. `TouchAim` 用"屏幕半宽"当"全屏尺寸"

**位置**：`include/Hack/TouchAim.h:38`

```cpp
inline void ScreenSize(int &w, int &h) {
    w = static_cast<int>(GameTools::ScreenSize.x);   // ★ 这是"半宽"！
    h = static_cast<int>(GameTools::ScreenSize.y);   // ★ 这是"半高"！
}
```

而 `GameTools::ScreenSize` 的来源（`draw_Gui.cpp` 的 `drawBegin`）：

```cpp
GameTools::ScreenSize.x = abs_ScreenX / 2;          // ← 除以了 2
GameTools::ScreenSize.y = abs_ScreenY / 2;
```

**调用点**（TouchAim::Run）：

```cpp
int sw = 0, sh = 0;
ScreenSize(sw, sh);                                  // sw = 屏幕宽/2
// ...
startX = (Cfg.起手X >= 0) ? Cfg.起手X : static_cast<int>(sw * 0.85f);   // 用了半宽算起手点
// ...
k->TouchMove(slot, tx, ty, sw, sh);                  // ★ 传进去当"全屏尺寸"做归一化
```

而 `Driver::HandleTouchEvent` 里：

```cpp
double normX = static_cast<double>(x) / screenW;     // x / (屏幕宽/2) → 结果偏大一倍
```

**症状**：触摸自瞄的起手点位置错误、拖拽幅度与预期不符（触摸坐标被放大一倍）。

**根因**：**同一个变量承载了两种语义**——
`GameTools::ScreenSize` 是"屏幕中心坐标"（W2S 用），
而触摸接口需要"屏幕尺寸"（归一化用）。

**修法一：改名 + 提供正确接口（推荐）**

```cpp
// 在 GameTools 里明确区分
namespace GameTools {
    extern Vec2 ScreenHalf;      // 半宽半高（W2S 用）
    extern Vec2 ScreenSize;      // 全宽全高（触摸用）
}
```

```cpp
// TouchAim.h 改成用真实的全屏尺寸
inline void ScreenSize(int &w, int &h) {
    w = static_cast<int>(GameTools::ScreenSize.x);   // 现在是全宽
    h = static_cast<int>(GameTools::ScreenSize.y);
}
```

**修法二：就地乘 2（最小改动）**

```cpp
inline void ScreenSize(int &w, int &h) {
    w = static_cast<int>(GameTools::ScreenSize.x) * 2;   // ★ 还原成全宽
    h = static_cast<int>(GameTools::ScreenSize.y) * 2;
}
```

**为什么推荐修法一**：修法二在同一行里又引入了一次"隐式换算"，
下次改代码的人还是容易搞混。**用两个明确命名的变量才是根治。**

**影响范围**：触摸自瞄功能。

**检测方法**：

```cpp
// 断言：起手点应该在屏幕右侧 85% 处（而不是 42.5%）
TouchAim::Cfg.起手X = -1;
TouchAim::Cfg.起手Y = -1;
// 触发一次 Run()，检查 TouchMove 收到的坐标
int expectedX = (int)(真实屏幕宽 * 0.85f);
```

## 二、中等缺陷（7 个）

### M1. 三个后台线程 detach，无停止机制

**位置**：`src/Android_draw/draw_Gui.cpp:593-595`

```cpp
if (LineTrace::initPhysX() == true) {
    thread(&VisibleCheck::UpdateSceneByRange).detach();
    thread(&VisibleCheck::UpdateDynamicHeightField).detach();
    thread(&VisibleCheck::UpdateDynamicRigid).detach();
    physx已初始化 = true;
}
```

**症状**：主线程退出时，三个 `detach` 线程可能正在操作 Embree 场景，
而主线程正在销毁它 → **退出时崩溃或卡死**。

**修法**：

```cpp
// 加一个全局停止标志
std::atomic<bool> g_running{true};

void UpdateSceneByRange() {
    while (g_running) {
        // ... 原来的更新逻辑
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}

// 主循环退出前
g_running.store(false);
std::this_thread::sleep_for(std::chrono::milliseconds(300));   // 给线程退出时间
graphics->Shutdown();
```

**更完整的做法（可等待）**：

```cpp
std::vector<std::thread> g_workers;

void StartWorkers() {
    g_workers.emplace_back(UpdateSceneByRange);
    g_workers.emplace_back(UpdateDynamicHeightField);
    g_workers.emplace_back(UpdateDynamicRigid);
}

void StopWorkers() {
    g_running.store(false);
    for (auto &t : g_workers) if (t.joinable()) t.join();   // ★ 等待退出
    g_workers.clear();
}
```

---

### M2. `SysHal::pvm` 用严格相等判断成功，部分成功被判为失败

**位置**：`include/My_Utils/SysHal.h:82`

```cpp
bool pvm(void *address, void *buffer, size_t size, bool iswrite) {
    // ...
    ssize_t bytes = process_v(pid, local, 1, remote, 1, 0, iswrite);
    return bytes == size;        // ★ 部分成功（如 bytes = size/2）返回 false
}
```

**症状**：跨页读取时，如果后半页不可访问，
`process_vm_readv` 会返回前半部分的字节数，这里被当成**完全失败**——
已经读到的数据被丢弃。

**对比**：`Driver::HandleVirtualMemoryRWEvent` 的实现会返回部分字节数
（第 79 章）。**两个后端行为不一致**。

**修法**：

```cpp
ssize_t pvm(void *address, void *buffer, size_t size, bool iswrite) {
    struct iovec local[1];
    struct iovec remote[1];
    local[0].iov_base  = buffer;
    local[0].iov_len   = size;
    remote[0].iov_base = address;
    remote[0].iov_len  = size;
    if (pid <= 0) return -ESRCH;                  // ★ 返回错误码而非 bool
    return process_v(pid, local, 1, remote, 1, 0, iswrite);
}

// 调用方
int Read(uintptr_t addr, void *buffer, size_t size) {
    const ssize_t n = pvm((void*)addr, buffer, size, false);
    return (n > 0) ? static_cast<int>(n) : -1;    // ★ 保留部分成功
}
```

**并让两个后端统一语义**：都返回"实际传输的字节数"，
`> 0` 表示成功（可能少于请求），`< 0` 表示失败。

---

### M3. `if (fd > 0)` 应为 `if (fd >= 0)`

**位置**：`include/ImGui/Utils.h:80, 91, 104`（三处）

```cpp
inline size_t ReadFile(const char *path, void *buff, size_t size) {
    int fd = open(path, O_RDONLY);
    if (fd > 0) {                    // ★ open 成功可能返回 0
        auto len = read(fd, buff, size);
        close(fd);
        return len;
    }
    return 0;
}
```

**症状**：理论上，如果标准输入/输出/错误都被关闭，
`open` 返回的文件描述符可能是 0——此时文件打开成功了，却走进 `else` 分支。

**实际风险**：很低（因为 0/1/2 通常被占用）。
**但这是明确的规范错误**：`open` 失败返回 `-1`，成功返回 `>= 0`。

**修法**：

```cpp
if (fd >= 0) {          // ★ 正确判断
    // ...
}
```

**同类的 `close` 判断也要一致**：

```cpp
if (fd >= 0) close(fd);
```

---

### M4. `std::stoi` 在属性为空时抛异常

**位置**：`include/ImGui/Utils.h:65, 74`

```cpp
inline int getAndroidSDKLevel() {
    static int var = 0;
    if (var > 0) return var;

    var = std::stoi(getSystemProperty("ro.build.version.sdk"));   // ★ 空字符串会抛异常
    return var;
}
```

**症状**：如果属性不存在或为空（某些定制 ROM），
`std::stoi("")` 抛 `std::invalid_argument` →
未捕获则 `std::terminate` → **崩溃**。

**而更糟的是**：因为 `var` 是 `static`，
即使异常被捕获，`var` 仍是 0 → **每次调用都重新抛一次**。

**修法**：

```cpp
inline int getAndroidSDKLevel() {
    static int var = -1;                                 // ★ 用 -1 表示"未查询"
    if (var >= 0) return var;

    const std::string s = getSystemProperty("ro.build.version.sdk");
    var = s.empty() ? 0 : std::atoi(s.c_str());          // ★ atoi 不抛异常
    return var;
}
```

**或更严谨（用 `from_chars` 不用异常）**：

```cpp
inline int getAndroidSDKLevel() {
    static int var = -1;
    if (var >= 0) return var;

    const std::string s = getSystemProperty("ro.build.version.sdk");
    int value = 0;
    const auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
    var = (ec == std::errc{} && value > 0) ? value : 0;
    return var;
}
```

---

### M5. `GetMemoryInfoRef()` 返回引用，可能被下次请求覆盖

**位置**：`src/Android_draw/driver.h` 的 `Driver::GetMemoryInfoRef`

```cpp
const virtual_memory &GetMemoryInfoRef() {
    if (HandleVirtualMemoryInfo() != 0) {
        __builtin_memset(&req->vmem_info, 0, sizeof(req->vmem_info));
    }
    return req->vmem_info;         // ★ 返回共享内存里的引用
}
```

**为什么这么写**：结构有 13 MB（附录 M 算过），返回值拷贝代价太高。

**风险**：

```cpp
const auto &info1 = dr->GetMemoryInfoRef();
// ... 中间又发生了一次请求（比如另一个线程调了 GetScanRegions）
const auto &info2 = dr->GetMemoryInfoRef();     // 这次请求覆盖了同一块内存
// info1 和 info2 现在是同一份数据！
```

**修法一：明确生命周期契约（文档化，最小改动）**

```cpp
// ★ 返回值仅在下次 Driver 请求前有效。调用方必须立即使用，不要保存引用。
const virtual_memory &GetMemoryInfoRef();
```

**修法二：改成回调（推荐，强制正确用法）**

```cpp
template <typename Fn>
void WithMemoryInfo(Fn &&fn) {
    std::scoped_lock<SpinLock> lock(m_mutex);   // ★ 持锁期间有效
    HandleVirtualMemoryInfoUnlocked();
    fn(static_cast<const virtual_memory&>(req->vmem_info));
}

// 用法：数据只在 lambda 内有效
dr->WithMemoryInfo([&](const virtual_memory &info) {
    for (int i = 0; i < info.module_count; i++) { /* ... */ }
});
```

**修法三：只拷贝需要的最小数据**

```cpp
// 不需要整个 13 MB，只要模块列表
std::vector<ModuleBrief> GetModuleList();      // 只拷贝名字+基址，几百 KB
```

---

### M6. `Device` 构造函数用 `memset(this, 0, ...)`

**位置**：`include/ImGui/TouchHelperA.h:22`

```cpp
struct Device {
    int   fd;
    float S2TX;
    float S2TY;
    input_absinfo absX, absY;
    touchObj Finger[10];

    Device() { memset((void *)this, 0, sizeof(*this)); }   // ★
};
```

**风险**：

| 将来改动 | 后果 |
|---|---|
| 加一个 `std::string` 成员 | `memset` 破坏它的内部状态 → 崩溃 |
| 加一个虚函数 | 覆盖 `vptr` → 崩溃 |
| 加非 POD 成员 | 未定义行为 |

**当前是安全的**（所有成员都是 POD），但**非常脆弱**。

**修法**：

```cpp
struct Device {
    int   fd        = -1;                    // ★ 用默认成员初始化
    float S2TX      = 0.0f;
    float S2TY      = 0.0f;
    input_absinfo absX{};
    input_absinfo absY{};
    touchObj Finger[10]{};

    Device() = default;                      // ★ 不需要自定义构造函数
};
```

**好处**：将来加任何类型成员都不会出问题。

---

### M7. `GetNameById` 宽字符分支有空代码块

**位置**：`src/Android_draw/variable.h:202`

```cpp
if (isWide) {
    std::u32string u32(len, 0);
    if (!dr->Read(entryAddr + 2, u32.data(), len * 4)) {
        // ★ 空代码块：读取失败没有处理
    }
    std::string s;
    s.reserve(len * 3);
    for (char32_t c : u32) {
        AppendUtf8(s, c);
    }
    return s;
}
```

**症状**：读取失败时，`u32` 全是 0 值 → `AppendUtf8` 会追加 `len` 个 `\0` →
返回一个"长度为 len 的全零字符串"，而不是空字符串。

**对比**：窄字符分支有正确的 `return {}`：

```cpp
} else {
    std::string s(len, 0);
    if (!dr->Read(entryAddr + 2, s.data(), len)) {
        return {};                           // ★ 正确
    }
    return s;
}
```

**修法**：

```cpp
if (isWide) {
    std::u32string u32(len, 0);
    if (!dr->Read(entryAddr + 2, u32.data(), len * 4)) {
        return {};                            // ★ 补上
    }
    std::string s;
    s.reserve(len * 3);
    for (char32_t c : u32) AppendUtf8(s, c);
    return s;
}
```

## 三、轻微问题（8 个）

### L1. `mBase::Location` 缺 `{}` 初始化

**位置**：`include/Hack/Hack.h:15`

```cpp
struct mBase {
    uintptr_t libUE4{};
    // ... 其它指针都有 {}
    Vec3      Location;          // ★ 缺 {}
    Rotator   Rotation{};
    float     Fov{};
};
```

**影响**：`Vec3` 的默认构造函数会初始化字段，所以实际安全，
但**风格不一致**，且如果 `Vec3` 将来改成 POD 就会出问题。

**修法**：`Vec3 Location{};`

---

### L2. `mBase::PovPtr` 与 `CameraCache` 重复

**位置**：`include/Hack/Hack.h:14-16`

```cpp
uintptr_t CameraCache{};
uintptr_t PovPtr{};        // ★ 与 CameraCache 完全相同
```

赋值处（`Hack.cpp`）：

```cpp
AppBase.CameraCache = 玩家相机 + 0x2300;
AppBase.PovPtr      = AppBase.CameraCache;      // 一样
```

**修法**：删掉 `PovPtr`，统一用 `CameraCache`。

---

### L3. `Config` 有三处无用字段

**位置**：`include/My_Utils/ConfigManager.h`

```cpp
int  RunFPS = 90;              // ★ 未使用（实际用 gui_frame_rate）
bool 胶囊体大小 = false;        // ★ 未使用
bool 子弹 = false;              // ★ 未使用
```

**影响**：占用配置空间、误导读者（以为有这些功能）。

**修法**：删除，或加注释标明"预留"。

---

### L4. `DrawPlayer` 里读了但没用到的数据

**位置**：`src/Android_draw/draw_Gui.cpp:225`

```cpp
const float 胶囊体数值 = dr->Read<float>(
    dr->Read<uint64_t>(objectAddress + 0x380) + 0x54C);
// ★ 这个值从未被使用
```

**影响**：**每个对象多两次内存读取**（一次读指针、一次读 float）。
在 50 个对象的场景里，这是 100 次无效 I/O。

**修法**：删除这两行。

---

### L5. `MarixToVector` 拼写错误

**位置**：`src/Android_draw/variable.h:150`

```cpp
inline Vector3A MarixToVector(const Matrix& matrix) {   // ★ Matrix 拼成了 Marix
    return Vector3A(matrix.M[3][0], matrix.M[3][1], matrix.M[3][2]);
}
```

**修法**：重命名为 `MatrixToVector`（IDE 重构功能可以安全改名）。

---

### L6. 三套向量类型并存

**位置**：`VectorTools.h`（`Vec2/Vec3`）+ `draw.h`（`Vector2A/3A`）+ `VectorStruct.h`（`My_Vector2/3/4`）

**影响**：类型之间需要手工转换，容易出错（附录 L 有详细分析）。

**修法**（渐进）：

```cpp
// 第一步：加转换函数集中处理，避免各处裸写
inline Vector3A ToV3A(const Vec3 &v) { return Vector3A(v.x, v.y, v.z); }
inline Vec3     FromV3A(const Vector3A &v) { return Vec3(v.X, v.Y, v.Z); }

// 第二步：新代码只用一套（推荐 Vec2/Vec3）
// 第三步：逐步替换旧代码（可选，成本高）
```

---

### L7. `SaveConfig` 的原地修改 API 隐患

**位置**：`src/My_Utils/ConfigManager.cpp:50`

```cpp
bool SaveConfig() {
    Config ConfigSave = Settings;                              // ★ 先拷贝
    CryptoProcess(reinterpret_cast<uint8_t*>(&ConfigSave), sizeof(Config));
    // ...
}
```

**当前写法是正确的**（先拷贝再加密）。
**但 `CryptoProcess` 的 API 设计是隐患**：

```cpp
void CryptoProcess(uint8_t* data, size_t length);      // 原地修改
```

调用方一旦写错（直接传 `&Settings`），内存里的配置就变成密文。

**修法**：改成返回值形式，从 API 层面消除误用可能：

```cpp
std::vector<uint8_t> Encrypt(const void *data, size_t length) {
    std::vector<uint8_t> out(length);
    memcpy(out.data(), data, length);
    for (size_t i = 0; i < length; ++i) {
        out[i] ^= CRYPTO_KEY[i % KEY_LENGTH];
    }
    return out;
}

bool SaveConfig() {
    const auto blob = Encrypt(&Settings, sizeof(Config));   // ★ 不可能误改 Settings
    // ... 写文件
}
```

---

### L8. `draw_Gui.cpp.bak` 混在源码目录

**位置**：`src/Android_draw/draw_Gui.cpp.bak`（34 KB）

**影响**：

| 问题 | 说明 |
|---|---|
| 搜索结果污染 | `grep -r "某函数" src/` 会命中两份 |
| 构建系统风险 | 如果通配符写得不严谨可能被编译 |
| 阅读干扰 | 看到两个相似文件容易困惑 |

**修法**：

```bash
# 确认没用后删除
rm jni/src/Android_draw/draw_Gui.cpp.bak

# 并在 .gitignore 里加
echo "*.bak" >> .gitignore
echo "*.tmp" >> .gitignore
```

## 四、优化机会（4 个）

### O1. 遮挡判定该用 `rtcOccluded1` 而非 `rtcIntersect1`

**位置**：`include/Embree/PhysX.h` 的 `VisibleScene::Raycast`

**现状**：内部用 `rtcIntersect1`（找最近命中，要遍历所有可能更近的节点）。

**优化**：遮挡判定只需要"有没有命中"：

```cpp
bool IsOccluded(const PxVec3 &from, const PxVec3 &to) const {
    const PxVec3 d = to - from;

    RTCRay ray{};
    ray.org_x = from.x; ray.org_y = from.y; ray.org_z = from.z;
    ray.dir_x = d.x;    ray.dir_y = d.y;    ray.dir_z = d.z;
    ray.tnear = 0.0f;
    ray.tfar  = 1.0f - 1e-4f;             // 未归一化方向 + tfar=1，省一次开方
    ray.mask  = 0xFFFFFFFF;
    ray.flags = 0;

    // Embree 4：rtcOccluded1(scene, ray, args)，不需要高级参数传 nullptr
    rtcOccluded1(scene_, &ray, nullptr);   // ★ 找到任意命中就停
    return ray.tfar < 0.0f;
}
```

**预期收益**：**2~3 倍**（深挖 H 有实测数据）。

**注意**：`rtcOccluded1` 会修改 `ray.tfar`，
所以**不能复用同一个 `RTCRay` 对象连续查询多个场景**，必须每次重新构造。

---

### O2. 骨骼逐根读取应该批量

**位置**：`src/Android_draw/variable.h:154` 的 `GetBoneWorldPos` + 调用处

**现状**：15 根骨骼 = 15 次独立 I/O + 1 次 C2W = **16 次**。

**优化**：一次读完整个骨骼数组：

```cpp
// 一次读完 56 根骨骼（56 × 48 = 2688 字节）
std::vector<uint8_t> raw(boneCount * BONE_STRIDE);
if (dr->Read(boneArray, raw.data(), raw.size()) <= 0) return;

// 之后全部本地解析
for (int i = 0; i < 15; i++) {
    const int idx = boneIdx[i];
    BoneTransform bt;
    memcpy(&bt, raw.data() + idx * BONE_STRIDE, sizeof(BoneTransform));
    const Vec3 world = MatrixGetPosition(MatrixMulti(TransformToMatrix(bt), c2w));
    // ...
}
```

**预期收益**：**16 次 I/O → 1 次**，约 16 倍。

---

### O3. `DrawPlayer` 的批量读写优化

**位置**：`src/Android_draw/draw_Gui.cpp:215-418` 的遍历循环

**现状**（每个对象）：

| 步骤 | I/O 次数 |
|---|---|
| 读对象指针 | 1 |
| 读名字 ID | 1 |
| 读名字内容（块指针 + 头部 + 数据） | 3 |
| 读胶囊体（指针 + 值） | 2（**且无用**，见 L4） |
| 读 RootComponent | 1 |
| 读世界坐标 | 1 |
| 骨骼（15 根 + C2W） | 16 |
| **合计** | **约 25 次** |

**50 个对象 = 1250 次 I/O ≈ 4 ms**（按 3 µs/次）。

**优化后**：

```cpp
// ① 一次读完整个指针数组
std::vector<uint64_t> ptrs(count);
dr->Read(arrayAddr, ptrs.data(), count * 8);            // 1 次

// ② 批量读所有对象的字段（iovec 数组）
std::vector<ReadRequest> reqs;
for (auto p : ptrs) {
    reqs.push_back({p + 0x18,  &nameIds[i], 4});
    reqs.push_back({p + 0x168, &roots[i],   8});
}
ReadBatch(dr, reqs);                                     // 1 次

// ③ 批量读所有世界坐标
reqs.clear();
for (auto r : roots) reqs.push_back({r + 0x1F0, &positions[i], 12});
ReadBatch(dr, reqs);                                     // 1 次

// ④ 骨骼也批量（每个角色一次）
// → 50 个角色 = 50 次
```

**预期收益**：**1250 次 → 约 55 次**，提速 20 倍以上。

---

### O4. `spinlock.h` 用 `sched_yield()` 而非 `yield` 指令

**位置**：`include/ImGui/spinlock.h:18`

```cpp
void lock() noexcept {
    for (;;) {
        if (!lock_.exchange(true, std::memory_order_acquire)) return;
        while (lock_.load(std::memory_order_relaxed)) {
            sched_yield();          // ★ 这是系统调用（微秒级）
        }
    }
}
```

**对比 `driver.h` 的实现**：

```cpp
while (__atomic_load_n(&locked, __ATOMIC_RELAXED)) {
    asm volatile("yield");          // ★ 这是 CPU 指令（纳秒级）
}
```

**差异**：`sched_yield()` 每次迭代都陷入内核（~1 µs），
而 `asm("yield")` 只是一条指令（~1 ns）。**差三个数量级。**

**注意**：`spinlock.h` 是 ImGui 官方代码（第三方），
本项目没有直接用它（驱动锁用的是自己写的）。
**这里列出来是为了说明"为什么本项目要自己写一个锁"。**

**如果要改（谨慎，因为它是第三方代码）**：

```cpp
while (lock_.load(std::memory_order_relaxed)) {
#if defined(__aarch64__) || defined(__arm__)
    asm volatile("yield" ::: "memory");
#else
    asm volatile("pause" ::: "memory");
#endif
}
```

**更好的方案（C++20）**：用 `atomic::wait/notify_one` 让线程真正睡眠：

```cpp
void lock() noexcept {
    while (lock_.exchange(true, std::memory_order_acquire)) {
        lock_.wait(true, std::memory_order_relaxed);    // 内核级睡眠，零 CPU 占用
    }
}
void unlock() noexcept {
    lock_.store(false, std::memory_order_release);
    lock_.notify_one();
}
```

## 五、汇总与修复优先级

| # | 级别 | 问题 | 修复成本 | 建议优先级 |
|---|---|---|---|---|
| S1 | **严重** | 握手无超时 | 低 | **P0** |
| S2 | **严重** | Embree 无锁并发 | 中 | **P0** |
| S3 | **严重** | W2S 分支不一致 | 极低 | **P0** |
| S4 | **严重** | TouchAim 尺寸语义错 | 低 | **P0** |
| M1 | **中等** | detach 无停止 | 低 | P1 |
| M2 | **中等** | pvm 部分成功被丢 | 低 | P1 |
| M3 | **中等** | `fd > 0` 应为 `>= 0` | 极低 | P1 |
| M4 | **中等** | `stoi` 可能抛异常 | 极低 | P1 |
| M5 | **中等** | 引用可能失效 | 中 | P2 |
| M6 | **中等** | `memset(this)` | 低 | P2 |
| M7 | **中等** | 空代码块漏 return | 极低 | P1 |
| L1-L8 | **轻微** | 代码质量 8 项 | 极低 | P3 |
| O1-O4 | **优化** | 性能优化 4 项 | 中 | P2（S2 修复后） |

**修复顺序建议**：先把 4 个严重 修掉（都是小改动但影响大），
再修 P1 的 5 个中等（也都是小改动），最后做优化。

## 六、从这份报告学到的：代码审查清单

**这 23 个问题可以归为 8 类，你在自己项目里也按这 8 类查：**

### 1. 等待/循环有没有退出条件？

```
✗ while (!flag) { }              ← 没有超时
✓ while (!flag && now < deadline) { }
```

**检查点**：所有 `while` 循环、所有等待外部状态的地方。

### 2. 共享数据有没有同步？

```
✗ 后台线程写 vector，主线程读
✓ shared_mutex / 双缓冲 / 原子标志 + acquire_release
```

**检查点**：`detach` 线程、全局容器、跨线程的标志位。

### 3. 同一个变量的语义是否唯一？

```
✗ ScreenSize 有时指"半宽"，有时指"全宽"
✓ ScreenHalf / ScreenSize 分开命名
```

**检查点**：所有全局变量的名字是否准确表达了它的含义。

### 4. 失败路径是否处理？

```
✗ if (!read()) { }               ← 空代码块
✗ 函数返回值语义不一致
✓ 明确的错误返回 + 统一的失败语义
```

**检查点**：搜 `if (...)` 后面跟空 `{}` 的地方；比对同类函数的行为。

### 5. 边界条件对吗？

```
✗ if (fd > 0)     ← open 成功可能返回 0
✓ if (fd >= 0)
```

**检查点**：所有 `> 0` / `>= 0` / `< 0` 的判断，逐个想"边界值应该怎么处理"。

### 6. API 有没有可能被误用？

```
✗ void f(uint8_t *data) { /* 原地修改 */ }   ← 调用方可能传错
✓ std::vector<uint8_t> f(const void *data)   ← 从签名上消除误用
```

**检查点**：所有"原地修改"的参数、所有返回引用/指针的函数。

### 7. 有没有无用代码？

```
✗ 读了但没用的变量（浪费 I/O）
✗ 未使用的配置字段
✗ 遗留的 .bak 文件
```

**检查点**：编译器的 `-Wunused` 警告（默认可能没开全）。

### 8. 有没有性能上的明显浪费？

```
✗ 循环里重复读同一个值
✗ 逐条读本可以批量读
✗ 该用"只判断"的接口却用了"完整求交"
```

**检查点**：分段计时（第 72 章），看哪一段耗时最长。

> [!tip] 一个实用习惯
> 写代码时**每隔几天回头 review 自己前几天的代码**。
> 这时你已经忘了当时的思路，会像看别人的代码一样发现问题。
>
> 本报告的 23 个问题，绝大多数用上面的 8 条清单都能发现。

> [!note] 关于这份报告的性质
> 这是一份**教学用的代码审查报告**，目的是：
> 1. 展示真实项目里的真实问题长什么样
> 2. 教你一套可复用的审查方法
> 3. 让你在自己的项目里提前规避
>
> 项目本身能长时间稳定运行，说明这些问题在**当前使用场景下影响有限**。
> 但如果你要重写或长期维护，建议按优先级处理。

→ 返回 [[00-开始之前]]
