---
tags: [教程, 卷六, 架构, 抽象层]
day: 51
aliases: [ch80]
---

# 第 80 章 · 设计驱动抽象层 IDriver

> [!abstract] 本章目标
> 学会用抽象接口隔离"后端实现"，让业务代码不关心数据从哪来。
> 这是本项目架构上最漂亮的一处设计。

> [!note] 承上
> 卷六到现在有两条路：内核驱动 + 系统调用。
> 本章用**抽象层 `IDriver`** 把它们统一——业务代码不关心数据从哪来。本项目最漂亮的架构设计。

## 先看没有抽象层会怎样

业务代码直接调内核驱动：

```cpp
uint64_t Uworld = kernel_driver->Read<uint64_t>(base + 0xB3EC650);
```

问题来了：用户没装内核驱动怎么办？
要么程序不能跑，要么每个调用点都写 `if/else`：

```cpp
uint64_t Uworld;
if (useKernel) Uworld = kernel_driver->Read<uint64_t>(...);
else           Uworld = syscall_read<uint64_t>(...);
```

几百处调用点，每处都写一遍——**不可维护**。

## 解法：面向接口编程

```
业务代码  ──→  IDriver（接口）  ←──  Driver（内核后端）
                   ↑
                   └──────────────  MemDriver → SysHal（系统调用后端）
```

业务代码只认 `IDriver*`，运行时指向哪个后端由配置决定。

## 本项目的接口定义

```cpp
class IDriver {
public:
    virtual ~IDriver() = default;

    // 内存读写（两个后端都支持）
    virtual int  Read(uint64_t address, void *buffer, size_t size) = 0;
    virtual int  Write(uint64_t address, void *buffer, size_t size) = 0;

    // 进程与模块
    virtual int  GetPid(std::string_view packageName) = 0;
    virtual int  GetGlobalPid() = 0;
    virtual void SetGlobalPid(int pid) = 0;
    virtual bool GetModuleAddress(std::string_view moduleName, short segmentIndex,
                                  uint64_t *outAddress, bool isStart) = 0;

    // 内核驱动专属（系统调用后端给安全默认值）
    virtual bool DumpMemory(std::string_view target, std::string *dumpPath = nullptr) = 0;
    virtual std::vector<std::pair<uintptr_t, uintptr_t>> GetScanRegions() = 0;

    // ---- 以下是非虚的便捷封装，所有后端共享 ----
    template <typename T> T Read(uint64_t address) {
        T value = {};
        if (Read(address, &value, sizeof(T)) <= 0) value = T{};
        return value;
    }

    template <typename T> int Write(uint64_t address, const T &value) {
        return Write(address, const_cast<T*>(&value), sizeof(T));
    }

    std::string ReadString(uint64_t address, size_t max_length = 128) {
        if (!address) return "";
        std::vector<char> buffer(max_length + 1, 0);
        if (Read(address, buffer.data(), max_length) > 0) {
            buffer[max_length] = '\0';
            return std::string(buffer.data());
        }
        return "";
    }
};
```

> [!note] 模板方法定义在基类里，非虚
> `Read<T>` 依赖虚函数 `Read(addr, buf, size)` 实现，
> 所以**所有后端自动获得**模板和字符串功能，不用各写一遍。
>
> 这是"模板方法模式"的经典应用。

## 虚函数开销有多大？

一次虚调用 = 一次间接跳转（查虚表 + 跳）：

```
直接调用:  ~2 周期
虚调用:    ~5-10 周期（多一次内存访问）
```

听起来有差别，但对比实际的驱动 I/O（几千周期）：

```
虚调用开销 10 周期 vs 驱动 I/O 5000 周期 = 0.2%
```

**完全可以忽略。** 源码注释里也写了这个理由：

> 只有这几个接口会走虚函数转发，每个接口内部都是一次真正的驱动 I/O（几十微秒级），
> 虚函数开销可以忽略。

## 后端一：Driver（内核）

```cpp
class Driver : public IDriver {
public:
    int Read(uint64_t address, void *buffer, size_t size) override {
        return HandleVirtualMemoryRWEvent(request_op_vmem_read, address, buffer, size);
    }
    int Write(uint64_t address, void *buffer, size_t size) override {
        return HandleVirtualMemoryRWEvent(request_op_vmem_write, address, buffer, size);
    }
    // ...

    // 内核专属能力（非 IDriver 接口，直接调用）
    void TouchDown(int slot, int x, int y, int w, int h);
    void GyroReport(int x, int y, int z);
    int  SetProcessHwbpRef(std::span<const bp_point> points);
    int  StartSyscallMonitor(int pid);
    // ...
};
```

**注意**：触摸、陀螺仪、断点这些**不在 `IDriver` 里**，
因为系统调用后端提供不了。它们通过 `Driver*` 具体类型直接访问。

## 后端二：MemDriver（包装 SysHal）

```cpp
class MemDriver : public IDriver {
public:
    bool Open(int mode) {
        if (kernel_ != nullptr || syscall_ != nullptr) return ok_;   // 已打开，不能切

        mode_ = mode;
        ok_ = (mode == MEM_DRIVER_SYSCALL) ? OpenSyscall() : OpenKernel();
        return ok_;
    }

    int Read(uint64_t address, void *buffer, size_t size) override {
        if (buffer == nullptr || size == 0) return -EINVAL;
        if (syscall_ != nullptr) {
            return syscall_->read((uintptr_t)address, buffer, size)
                 ? (int)size : -EIO;
        }
        if (kernel_ != nullptr) return kernel_->Read(address, buffer, size);
        return -EIO;
    }
    // ...

    // 调试用：拿到内部的具体后端
    Driver *kernel()  { return kernel_; }
    SysHal *syscall() { return syscall_; }

private:
    Driver *kernel_  = nullptr;
    SysHal *syscall_ = nullptr;
    int     mode_    = MEM_DRIVER_KERNEL;
    bool    ok_      = false;
    int     global_pid_ = 0;
    const char *error_ = "";
};
```

`MemDriver` 是个**转发器**：内部持有两个具体后端之一，把调用转过去。

## 为什么"打开后不能切换"

```cpp
bool Open(int mode) {
    if (kernel_ != nullptr || syscall_ != nullptr) return ok_;
    ...
}
bool CanSwitch() const { return kernel_ == nullptr && syscall_ == nullptr; }
```

源码里的解释：

> 后端一旦打开就不再切换：内核驱动握手成功后不能安全释放
> （用户态退出但内核侧仍在轮询，会空转），所以必须先选好模式再点初始化。

**这不是偷懒，是安全性考虑**：内核线程还在轮询共享内存，
用户态把内存释放了，内核就会访问已释放的地址 → 崩溃。

UI 上也体现了这个约束：

```cpp
ImGui::BeginDisabled(!g_driver.CanSwitch());
if (ImGui::Combo("驱动选择", &driverIdx, "lsdriver\0syscall\0")) { ... }
ImGui::EndDisabled();
```

启动后下拉框变灰，不可改。

## 全局入口

```cpp
// draw_Gui.cpp
static MemDriver g_driver;
IDriver *dr = &g_driver;

// driver.h
extern IDriver *dr;
```

全项目只有一个 `dr` 指针，业务代码到处用它：

```cpp
dr->Read<uint64_t>(addr);
dr->GetPid("...");
```

## 专属能力怎么访问

内核专属功能需要拿到具体类型：

```cpp
// 初始化时
TouchAim::g_kernel = g_driver.kernel();      // nullptr 表示无内核
PovAim::g_kernel   = g_driver.kernel();

// 使用时
if (TouchAim::g_kernel == nullptr) {
    // 无内核，功能不可用
} else {
    TouchAim::g_kernel->TouchDown(slot, x, y, w, h);
}
```

UI 上会提示：

```cpp
if (TouchAim::g_kernel == nullptr)
    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "无内核驱动!");
else
    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "内核已就绪");
```

**这就是"能力探测"模式**：不是所有后端都支持所有功能，使用前先问。

## 设计原则总结

| 原则 | 本项目的体现 |
|---|---|
| 依赖倒置 | 业务依赖抽象 `IDriver`，不依赖具体后端 |
| 单一职责 | `Driver` 只管内核通信，`SysHal` 只管系统调用 |
| 开闭原则 | 加新后端（如 ptrace）不用改业务代码 |
| 能力探测 | 专属功能通过 `kernel()` 取值 + 判空 |
| 失败降级 | 无内核时给出安全默认值，不崩溃 |

## 动手：加第三个后端

练习：实现一个 `PtraceDriver`（用 ptrace 读写），接入抽象层。

```cpp
class PtraceDriver : public IDriver {
public:
    ~PtraceDriver() { if (attached_) ptrace(PTRACE_DETACH, pid_, 0, 0); }

    int Read(uint64_t addr, void *buf, size_t size) override {
        // ptrace 一次只能读一个字，要循环
        uint8_t *p = (uint8_t*)buf;
        for (size_t i = 0; i + 8 <= size; i += 8) {
            long v = ptrace(PTRACE_PEEKDATA, pid_, (void*)(addr + i), 0);
            if (errno) return -EIO;
            memcpy(p + i, &v, 8);
        }
        // 处理尾部不足 8 字节
        return (int)size;
    }
    // ...其它接口
private:
    pid_t pid_ = 0;
    bool attached_ = false;
};
```

然后在 `MemDriver` 里加一个分支。你会发现：**业务代码一行都不用改**。

这就是抽象层的价值——亲手加一次体会最深。

## 课后习题

### 习题 80.1 用接口 + 多态实现可切换后端（★★）

**任务要求**：定义抽象接口 `IDriver`（`read` 纯虚），实现两个后端 `MemDriver` 和 `FakeDriver`，
业务函数只认 `IDriver*`，验证切换后端时业务代码**零改动**。

**参考实现**：

```cpp
#include <cstdio>
#include <cstring>
#include <memory>
#include <cassert>

class IDriver {
public:
    virtual ~IDriver() = default;
    virtual int Read(uint64_t addr, void* buf, size_t size) = 0;
    template <typename T> T Read(uint64_t addr) {
        T v{};
        if (Read(addr, &v, sizeof(T)) <= 0) v = T{};
        return v;
    }
};

// 后端 1：真实内存
class MemDriver : public IDriver {
    const uint8_t* base_;
public:
    explicit MemDriver(const uint8_t* b) : base_(b) {}
    int Read(uint64_t addr, void* buf, size_t size) override {
        memcpy(buf, base_ + addr, size);
        return (int)size;
    }
};

// 后端 2：假数据（用于测试）
class FakeDriver : public IDriver {
public:
    int Read(uint64_t, void* buf, size_t size) override {
        memset(buf, 0xAB, size);   // 总是返回 0xAB
        return (int)size;
    }
};

// 业务函数：只认 IDriver*
int business_read_hp(IDriver* dr) {
    return dr->Read<int>(0);   // 读偏移 0 处的 int
}

int main() {
    uint8_t mem[16] = {0};
    int hp = 1234;
    memcpy(mem, &hp, 4);

    MemDriver  real(mem);
    FakeDriver fake;
    IDriver* drivers[2] = { &real, &fake };

    assert(business_read_hp(drivers[0]) == 1234);      // 真实后端
    assert(business_read_hp(drivers[1]) == 0xABABABAB); // 假后端（同一业务代码）
    printf("真实后端 hp=%d  假后端 hp=0x%X\n",
           business_read_hp(drivers[0]), business_read_hp(drivers[1]));
    printf("习题 80.1 全部通过（业务代码零改动）\n");
    return 0;
}
```

**验证断言**：同一个 `business_read_hp`，传 `MemDriver` 得 `1234`，传 `FakeDriver` 得 `0xABABABAB`——
**业务代码一行没改**。

> [!tip] 这道题练什么
> ① **面向接口编程**——业务只依赖 `IDriver*`，不关心背后是谁；
> ② **多态分发**——虚函数表在运行时选对实现；
> ③ 这正是本项目 `dr` 能是 `Driver` 或 `MemDriver` 的原理（第 80 章核心）。

## 验收清单

- [ ] 理解"面向接口编程"解决的问题（说出"业务代码不依赖具体后端"）
- [ ] 知道 `IDriver` 哪些方法是虚的、哪些不是（为什么）（说出模板方法非虚的原因）
- [ ] 知道虚函数开销相对驱动 I/O 可以忽略（说出"I/O 几十微秒，虚函数几纳秒"）
- [ ] 理解为什么"打开后不能切换后端"（说出"内核连接无法安全释放"）
- [ ] 理解"能力探测"模式（`kernel()` 判空）（说出怎么判断后端能力）
- [ ] 知道本项目全局只有一个 `dr` 指针（说出为什么全局唯一）
- [ ] 实现了第三个后端，验证了业务代码不用改 ★（加 ptrace 后端，业务代码零改动）

→ 下一章：[[第81章-批量读与缓存]]　—— 把每帧几千次 I/O 压到几十次。
