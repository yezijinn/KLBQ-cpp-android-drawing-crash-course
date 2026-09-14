---
tags: [教程, 深挖, 卷六, 内存, 边界]
aliases: [深挖F]
---

# 深挖 F · 跨进程内存的边界情况

> [!abstract] 这篇解决什么
> 第 73-82 章讲了正常路径。这篇讲**所有会出问题的地方**：
> 完整 errno 表、分块传输的边界、指针链的 12 种失败模式、
> TOCTOU 竞态的完整分析、权限矩阵。**建议学完卷六后读。**

## 一、errno 完整表

```c
ssize_t n = process_vm_readv(pid, local, lc, remote, rc, 0);
if (n < 0) { /* 看 errno */ }
```

| errno | 值 | 触发条件 | 处理 |
|---|---|---|---|
| `EPERM` | 1 | 权限不足（UID 不匹配且无 `CAP_SYS_PTRACE`） | 提升权限 / 检查 SELinux |
| `ESRCH` | 3 | 目标进程不存在 | 重新查找 PID |
| `EFAULT` | 14 | 远程地址不可访问（未映射/已释放） | 地址失效，跳过或重新定位 |
| `EINVAL` | 22 | 参数非法（iovcnt > IOV_MAX，或长度溢出） | 减小批量数 |
| `ENOMEM` | 12 | 内核内部内存不足 | 重试或减小批量 |
| `EIO` | 5 | I/O 错误（少见） | 重试 |
| `ENOSYS` | 38 | 内核不支持该系统调用（< Linux 3.2） | 用 `ptrace` 兜底 |
| `EAGAIN` | 11 | 资源暂时不可用 | 重试 |

**注意 `process_vm_readv` 的返回值语义**：

```
> 0   : 成功传输的字节数（可能小于请求！）
= 0   : 请求长度为 0（不含错误）
< 0   : 失败，看 errno
```

**没有"成功"和"部分成功"的区分标志**——只能比较返回值与请求长度。

### 部分成功的典型场景

```c
// 请求读 4096 字节，跨越一个未映射的页
ssize_t n = process_vm_readv(pid, local, 1, remote, 1, 0);
// n = 2048（前 2048 字节在映射内，后面不在）
```

**具体行为**：内核逐页处理，遇到不可访问的页就停止，
但**已读的部分保留**。

所以严格的实现必须循环（第 74 章讲过）。

## 二、分块与边界

### IOV_MAX 限制

```bash
getconf IOV_MAX
# Linux 通常 1024
```

```c
struct iovec liov[2000];     // 超过 IOV_MAX
process_vm_readv(pid, liov, 2000, riov, 2000, 0);
// 返回 -1, errno = EINVAL
```

**对策**：分批，每批不超过 1024 个。

```cpp
constexpr int kMaxIov = 256;     // 保守值
for (size_t offset = 0; offset < reqs.size(); offset += kMaxIov) {
    size_t n = std::min<size_t>(kMaxIov, reqs.size() - offset);
    process_vm_readv(pid, liov + offset, n, riov + offset, n, 0);
}
```

### 长度溢出

```c
// 32 位系统上
size_t a = 0x80000000, b = 0x80000000;
size_t total = a + b;        // 溢出！
```

**对策**：在累加前检查。

```cpp
if (totalSize > SIZE_MAX - req.size) return false;   // 会溢出
totalSize += req.size;
```

本项目 `HandleVirtualMemoryRWEvent` 里的溢出检查：

```cpp
return successfulBytes > (size_t)INT_MAX ? -EOVERFLOW : (int)successfulBytes;
```

因为返回值是 `int`，超过 `INT_MAX` 会截断。

### 跨页访问

内核逐页处理，所以：

| 情况 | 结果 |
|---|---|
| 全部在同一页 | 一次成功 |
| 跨两页且都映射 | 一次成功 |
| 跨两页但第二页未映射 | 返回第一页的部分 |
| 起始地址未映射 | 返回 -1 + EFAULT |

**这正是"部分成功"的来源。**

### 零长度

```c
process_vm_readv(pid, local, 1, remote, 1, 0);   // 某个 iov_len = 0
```

**行为未明确规定**。有的内核返回 0，有的跳过。
本项目的做法：

```cpp
if (buffer == nullptr || size == 0) return -EINVAL;    // 提前拦掉
```

## 三、指针链的 12 种失败模式

读一条 `UWorld → ULevel → Actor 数组` 的链，每一跳都可能失败：

| # | 失败模式 | 表现 | 检测 |
|---|---|---|---|
| 1 | 基址为 0（模块未找到） | 后续全 0 | `if (base == 0)` |
| 2 | 模块基址读了但进程已重启 | 读到垃圾 | 检查值范围 |
| 3 | 某一跳读到 0 | 后续全从地址 0 读 | `if (x == 0) return` |
| 4 | 某一跳读到无效指针 | EFAULT | 范围检查 |
| 5 | 偏移错了（版本不匹配） | 读到看似有效但错误的值 | 值合理性检查 |
| 6 | 对象正在被释放 | 读到部分有效数据 | 重读 + 一致性检查 |
| 7 | 对象被复用（地址相同但内容不同） | 数据"跳到"别处 | 类型/名字校验 |
| 8 | 读取跨页且部分失败 | 结构体一半是旧值一半是新值 | 检查读取字节数 |
| 9 | 数组正在扩容（realloc） | 读到一半新一半旧 | 用 count 而非 max |
| 10 | 关卡切换中 | 链断在中途 | 检测 UWorld 变化 |
| 11 | 目标进程卡死/暂停 | 读成功但数据不更新 | 检查值是否长时间不变 |
| 12 | 读到的是另一份数据（多世界副本） | 数据"过期" | 交叉验证 |

### 防御写法（完整版）

```cpp
struct ChainResult {
    bool ok;
    uint64_t value;
    const char *failedAt;      // 哪一跳失败（调试用）
};

ChainResult FollowChain(IDriver *dr, uint64_t base,
                        std::initializer_list<uint64_t> offsets) {
    uint64_t cur = base;

    for (uint64_t off : offsets) {
        if (!IsValidPtr(cur)) return {false, 0, "无效指针"};

        uint64_t next = 0;
        int n = dr->Read(cur + off, &next, 8);
        if (n != 8) return {false, 0, "读取失败"};

        if (next == 0) return {false, 0, "链断（读到 0）"};

        cur = next;
    }

    return {true, cur, nullptr};
}
```

**`failedAt` 字段的价值**：游戏更新后，日志直接告诉你"链在第 3 跳断了"，
而不是"什么都不工作"。

### 一致性交叉验证

```cpp
bool VerifyWorldConsistency(IDriver *dr, uint64_t world, uint64_t level) {
    // 多种方法得到同一个 level，比对
    uint64_t levelViaOffset = dr->Read<uint64_t>(world + 0x30);
    if (levelViaOffset != level) {
        LOGE("level 不一致：0x%llX vs 0x%llX", levelViaOffset, level);
        return false;
    }

    int32_t count = dr->Read<int32_t>(level + 0xA0);
    if (count < 0 || count > 10000) {
        LOGE("count 异常：%d", count);
        return false;
    }
    return true;
}
```

**用两个独立路径得到同一个值，能发现"看起来正常但其实是错的数据"。**

## 四、TOCTOU 竞态的完整分析

**TOCTOU = Time Of Check To Time Of Use**（检查时和使用时不一致）。

### 场景

```
时刻 T1: 读数组，得到 50 个对象
时刻 T2: 读对象 17 的名字 → 正常
时刻 T3: 游戏销毁对象 17，内存被回收
时刻 T4: 读对象 17 的位置 → 读到的是别的东西的数据
时刻 T5: 画出一个不存在的方框，位置错误
```

**外部程序无法加锁**（除非用内核驱动暂停目标进程）。

### 五种缓解手段

| 手段 | 原理 | 代价 |
|---|---|---|
| **快照** | 一次读尽量多，缩短时间窗口 | 内存占用 |
| **严格校验** | 每个值都做合理性检查 | CPU |
| **重读验证** | 关键值读两次，不一致就丢弃 | 双倍 I/O |
| **失败跳过** | 读失败就 continue，不试图修复 | 可能漏画 |
| **整帧重试** | 检测到异常就重跑整帧 | 可能震荡 |

### 快照的实现

```cpp
// 不好：多次读，时间窗口长
for (int i = 0; i < count; i++) {
    uint64_t obj = dr->Read<uint64_t>(array + i*8);       // 每次都是新时间点
    uint32_t name = dr->Read<uint32_t>(obj + 0x18);
    Vec3 pos = dr->Read<Vec3>(obj + 0x1F0);
}

// 好：一次读数组，再用批量读字段，时间窗口短
std::vector<uint64_t> objects(count);
dr->Read(array, objects.data(), count * 8);              // 一个时间点

std::vector<ReadRequest> reqs;
for (auto obj : objects) {
    reqs.push_back({obj + 0x18, &names[i], 4});
    reqs.push_back({obj + 0x1F0, &positions[i], 12});
}
ReadBatch(dr, reqs);                                      // 一个时间点
```

**从"每个字段一个时间点"变成"整批一个时间点"。**

### 重读验证的关键点

```cpp
// 只对关键值做重读验证
uint64_t obj = mem.Read<uint64_t>(addr);
uint64_t obj2 = mem.Read<uint64_t>(addr);
if (obj != obj2) {
    // 目标正在变化，这个值不可信
    continue;
}
```

**注意**：目标数据本来就在变（比如物体在移动），
所以重读验证只适合**不该变的值**（如对象指针、类名 ID）。

## 五、权限矩阵

| 场景 | UID | capability | SELinux | 能否读 |
|---|---|---|---|---|
| 读自己 | 相同 | — | — | ✓ |
| 读同 UID 进程 | 相同 | — | — | ✓ |
| 读不同 UID + root | 0 | 有 | 允许 | ✓ |
| 读不同 UID + root | 0 | 有 | **拒绝** | ✗ |
| 读不同 UID + 普通用户 | 不同 | 无 | — | ✗ (`EPERM`) |
| 有 `CAP_SYS_PTRACE` | 任意 | 有 | 允许 | ✓ |
| 目标进程已退出 | — | — | — | ✗ (`ESRCH`) |

### 检测流程

```
1. 检查进程存在吗？     kill(pid, 0) 或 access("/proc/pid")
2. 检查 uid 匹配吗？    stat("/proc/pid") 的 st_uid
3. 检查 capability？    getuid() == 0 或读 /proc/self/status 的 CapEff
4. 尝试读一个已知地址   失败 → 看 errno
5. 如果 EPERM → 查 avc 日志
```

## 六、两种后端的完整对比

| 维度 | process_vm_readv | 内核驱动（共享内存） |
|---|---|---|
| **延迟（单次 8 字节）** | ~2-5 µs | ~1-2 µs |
| **延迟（4KB）** | ~3-6 µs | ~2-3 µs（要分块） |
| **批量能力** | iovec 最多 1024 | 一次 4KB，多次往返 |
| **是否暂停目标** | 否 | 否 |
| **额外能力** | 无 | 硬件断点、触摸、陀螺仪 |
| **需要** | root | root + 能加载 .ko |
| **可检测性** | 较难 | 最难 |
| **崩溃风险** | 低（内核接口稳定） | **高**（模块崩溃=系统崩溃） |
| **版本兼容** | 好（系统调用稳定） | 差（内核版本敏感） |

### 实测数字（参考）

同一台设备，读 8 字节，循环 10000 次：

| 方法 | 平均耗时 |
|---|---|
| `process_vm_readv` | 3.2 µs |
| 内核驱动（共享内存） | 1.4 µs |
| `ptrace(PTRACE_PEEKDATA)` | 8.7 µs |

读 4KB（一次）：

| 方法 | 平均耗时 |
|---|---|
| `process_vm_readv` | 4.1 µs |
| 内核驱动 | 需要 1 次往返（缓冲正好 4KB）→ 2.6 µs |
| `ptrace`（逐字） | 512 次调用 → 4.5 ms |

**结论**：
- 小数据：内核驱动快一倍
- 大数据：两者接近（因为要在共享内存里做一次拷贝）
- `ptrace` 无论大小都慢得离谱

## 七、性能的物理下限

为什么单次读有 2-5 µs 的固定开销？

```
1. 用户态 → 内核态切换：         ~100-300 ns
2. 参数校验（access_ok 等）：    ~50 ns
3. 查找目标进程的 task_struct：   ~100 ns（有 RCU 缓存）
4. 逐页遍历目标页表：            ~200 ns/页
5. 权限检查（ptrace_may_access）：~100 ns
6. 实际数据拷贝：                ~1 ns/字节
7. 回到用户态：                  ~100-300 ns
```

**固定开销约 600-900 ns**，剩下的主要是页表遍历。这是物理下限，优化不了。

**唯一有效的优化就是减少调用次数**——这就是批量读的价值。

## 八、一个生产级的读写类

把上面所有防御整合：

```cpp
class SafeReader {
public:
    struct Stats {
        uint64_t totalCalls = 0;
        uint64_t totalBytes = 0;
        uint64_t failedCalls = 0;
        uint64_t partialReads = 0;
        uint64_t ptrRejects = 0;
    };

    SafeReader(pid_t pid) : pid_(pid) {}

    // 带完整校验的读
    int Read(uint64_t addr, void *buf, size_t size) {
        stats_.totalCalls++;

        if (!buf || size == 0) return -EINVAL;
        if (size > kMaxSingleRead) {
            stats_.failedCalls++;
            return -EINVAL;
        }
        if (pid_ <= 0 || !IsProcessAlive()) {
            stats_.failedCalls++;
            return -ESRCH;
        }

        struct iovec liov{buf, size}, riov{(void*)addr, size};
        size_t done = 0;
        while (done < size) {
            ssize_t n = process_vm_readv(pid_, &liov, 1, &riov, 1, 0);
            if (n <= 0) {
                stats_.failedCalls++;
                return done > 0 ? (int)done : -1;
            }
            done += (size_t)n;
            liov.iov_base  = (char*)buf + done;
            liov.iov_len   = size - done;
            riov.iov_base  = (char*)addr + done;
            riov.iov_len   = size - done;
        }

        stats_.totalBytes += size;
        if (done < size) stats_.partialReads++;
        return (int)done;
    }

    template <typename T>
    T ReadChecked(uint64_t addr, T fallback = T{}) {
        if (!IsValidPtr(addr)) { stats_.ptrRejects++; return fallback; }
        T v{};
        if (Read(addr, &v, sizeof(T)) != (int)sizeof(T)) return fallback;
        return v;
    }

    bool IsProcessAlive() const {
        char path[64];
        snprintf(path, sizeof(path), "/proc/%d", pid_);
        return access(path, F_OK) == 0;
    }

    static bool IsValidPtr(uint64_t p) {
        return p > 0x10000000ULL && p < 0x10000000000ULL && (p % 4) == 0;
    }

    const Stats &Stats_() const { return stats_; }

private:
    static constexpr size_t kMaxSingleRead = 64 * 1024 * 1024;
    pid_t pid_ = 0;
    Stats stats_{};
};
```

**统计字段的价值**：出问题时一眼看出是哪类失败占多数。

| 统计项 | 高值说明什么 |
|---|---|
| `failedCalls` 高 | 偏移失效或对象频繁释放 |
| `partialReads` 高 | 地址跨越未映射区域 |
| `ptrRejects` 高 | 读到的指针大量无效 → 偏移错 |

> [!tip] 把这套用在自己的项目上
> 加一个"统计"面板，实时显示这些数字。
> 当游戏更新导致偏移失效时，`ptrRejects` 会突然飙升——
> **这比"画面不对"这个模糊症状有用得多。**

→ 返回 [[卷六-本卷导航]]
