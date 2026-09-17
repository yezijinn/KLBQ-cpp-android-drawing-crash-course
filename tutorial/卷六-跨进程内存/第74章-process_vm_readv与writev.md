---
tags: [教程, 卷六, 内存, 系统调用]
day: 48
aliases: [ch74]
---

# 第 74 章 · process_vm_readv 与 writev

> [!abstract] 本章目标
> 掌握跨进程读写的核心系统调用，能手写一个可用的读写器。
> 本项目 `SysHal` 就是本章代码的工程化版本。

> [!note] 承上
> 上一章知道了"内核提供哪些口子"。
> 本章动手用第一个口子——**`process_vm_readv` 系统调用**，能手写一个读写器。

## 先看函数签名

```c
#include <sys/uio.h>

ssize_t process_vm_readv(pid_t pid,
                         const struct iovec *local_iov,
                         unsigned long liovcnt,
                         const struct iovec *remote_iov,
                         unsigned long riovcnt,
                         unsigned long flags);

ssize_t process_vm_writev(pid_t pid,
                          const struct iovec *local_iov,
                          unsigned long liovcnt,
                          const struct iovec *remote_iov,
                          unsigned long riovcnt,
                          unsigned long flags);
```

| 参数 | 含义 |
|---|---|
| `pid` | 目标进程 |
| `local_iov` | **本地**缓冲区数组 |
| `liovcnt` | 本地数组元素个数 |
| `remote_iov` | **远程**地址数组 |
| `riovcnt` | 远程数组元素个数 |
| `flags` | 必须是 0 |
| 返回值 | 成功传输的字节数，-1 表示失败 |

## iovec 是什么

```c
struct iovec {
    void  *iov_base;   // 起始地址
    size_t iov_len;    // 长度
};
```

就是一个"地址 + 长度"对。**两边都用这个结构**，
只不过一边是你进程的虚拟地址，一边是对方的。

```
本地: [buf_A (100字节)] [buf_B (200字节)]
        ↕                ↕
远程: [addr_A (100字节)] [addr_B (200字节)]
```

一次调用可以把分散的几块数据搬完——这叫**散布/聚集 I/O**（scatter/gather）。

## 最简单的用法：读一块

```c
#include <sys/uio.h>
#include <stdio.h>
#include <string.h>

bool ReadMemory(pid_t pid, void *remoteAddr, void *localBuf, size_t size) {
    struct iovec local[1];
    local[0].iov_base = localBuf;
    local[0].iov_len  = size;

    struct iovec remote[1];
    remote[0].iov_base = remoteAddr;
    remote[0].iov_len  = size;

    ssize_t n = process_vm_readv(pid, local, 1, remote, 1, 0);
    return n == (ssize_t)size;
}
```

**注意**：返回值可能小于请求的大小（部分成功）。
严格的实现要循环：

```c
bool ReadMemoryFull(pid_t pid, void *remoteAddr, void *localBuf, size_t size) {
    struct iovec local[1] = {{localBuf, size}};
    struct iovec remote[1] = {{remoteAddr, size}};

    size_t done = 0;
    while (done < size) {
        ssize_t n = process_vm_readv(pid, local, 1, remote, 1, 0);
        if (n <= 0) return false;

        done += n;
        local[0].iov_base = (char*)localBuf + done;
        local[0].iov_len  = size - done;
        remote[0].iov_base = (char*)remoteAddr + done;
        remote[0].iov_len  = size - done;
    }
    return true;
}
```

## 一次读多块

这是 `process_vm_readv` 相对 ptrace 最大的优势：

```c
// 一次性读 3 块分散的内存
struct iovec local[3] = {
    {buf0, 100},
    {buf1, 200},
    {buf2, 50},
};
struct iovec remote[3] = {
    {(void*)addr0, 100},
    {(void*)addr1, 200},
    {(void*)addr2, 50},
};
ssize_t n = process_vm_readv(pid, local, 3, remote, 3, 0);
// n 应该是 350
```

**一次系统调用读 3 块**——如果每块都在同一页附近，开销几乎和读一块一样。

> [!tip] 这是优化的关键
> 项目里读一个 Actor 要读名字、位置、骨骼……十几个字段。
> 如果一个个读 = 十几次系统调用。
> 用 iovec 数组一次读 = 1 次系统调用。
> 第 81 章会详细讲。

## 手写 syscall（NDK 头文件缺失时）

部分老 NDK 的 `<sys/uio.h>` 没有声明这两个函数。这时直接调 syscall：

```c
#include <unistd.h>
#include <sys/syscall.h>

#ifdef __aarch64__
    #define NR_PROCESS_VM_READV  270
    #define NR_PROCESS_VM_WRITEV 271
#else
    #define NR_PROCESS_VM_READV  310
    #define NR_PROCESS_VM_WRITEV 311
#endif

ssize_t my_readv(pid_t pid,
                 const struct iovec *l, unsigned long lc,
                 const struct iovec *r, unsigned long rc) {
    return syscall(NR_PROCESS_VM_READV, pid, l, lc, r, rc, 0);
}
```

**本项目 `SysHal.h` 正是这么做的**（第 21 章讲过调用号因架构而异）。

## 权限与错误处理

```c
ssize_t n = process_vm_readv(pid, ...);
if (n < 0) {
    switch (errno) {
    case EPERM:  printf("权限不足（需要 ptrace 权限）\n"); break;
    case ESRCH:  printf("进程不存在（可能已经退出）\n"); break;
    case EFAULT: printf("地址不可访问（未映射或已释放）\n"); break;
    case EINVAL: printf("参数无效（长度溢出或 iovcnt 过大）\n"); break;
    case ENOMEM: printf("内存不足（iovcnt 超过 IOV_MAX）\n"); break;
    }
}
```

| errno | 常见原因 |
|---|---|
| `EPERM` | 权限不够（UID 不匹配且无 CAP_SYS_PTRACE） |
| `ESRCH` | 目标进程已退出 ← **游戏切后台/崩溃时会遇到** |
| `EFAULT` | 地址未映射 ← **偏移失效的典型表现** |
| `EINVAL` | 参数非法 |

> [!note] 对本项目来说 `ESRCH` 和 `EFAULT` 最常⻅
> - `ESRCH`：游戏没启动或刚退出 → 要重新找 PID
> - `EFAULT`：偏移失效或指针链断了一环 → 要靠判空过滤（第 89 章）

## 写内存

```c
int newValue = 9999;
struct iovec local[1]  = {{&newValue, sizeof(newValue)}};
struct iovec remote[1] = {{remoteAddr, sizeof(newValue)}};

ssize_t n = process_vm_writev(pid, local, 1, remote, 1, 0);
if (n == sizeof(newValue)) printf("写入成功\n");
```

**注意**：目标页必须是可写的（`rw-p`）。写代码段（`r-xp`）会失败（EFAULT）。

## 性能：一次调用多少开销？

粗略数据（现代 ARM 手机）：

| 操作 | 耗时 |
|---|---|
| 一次 `process_vm_readv`（读 8 字节） | ~2-5 µs |
| 一次 `process_vm_readv`（读 4KB） | ~3-6 µs |
| 本地 memcpy（4KB） | ~0.2 µs |

**关键结论：读 8 字节和读 4KB 耗时差不多。** 因为开销主要在
系统调用本身（特权级切换 + 页表查询），不在数据搬运。

所以：
```
读 100 个 8 字节字段：
  逐个读 = 100 × 3µs = 300 µs
  批量读（若地址连续） = 1 × 4µs = 4 µs
                                    ↑ 快 75 倍
```

**这是本项目最重要的性能杠杆**（第 81 章）。

## 边界情况

| 情况 | 行为 |
|---|---|
| 部分地址不可读 | 返回实际读到的字节数（小于请求） |
| 全部不可读 | 返回 -1，errno = EFAULT |
| 地址跨越页边界 | 正常（内核会处理） |
| 目标进程正在退出 | 可能 ESRCH |
| `iovcnt` 太大 | EINVAL（上限 `IOV_MAX`，通常 1024） |

## 完整的读写器

```c
// pmem.h
#pragma once
#include <sys/types.h>
#include <sys/uio.h>

bool  pmem_attach(pid_t pid);
bool  pmem_read (pid_t pid, void *remote, void *local, size_t size);
bool  pmem_write(pid_t pid, void *remote, const void *local, size_t size);
bool  pmem_read_batch(pid_t pid, void **remotes, void **locals,
                      size_t *sizes, int count, size_t *outRead);
```

```c
// pmem.c
#include "pmem.h"
#include <unistd.h>
#include <errno.h>
#include <string.h>

static bool xfer(pid_t pid, void *remote, void *local, size_t size, bool isWrite) {
    if (!remote || !local || size == 0) return false;

    struct iovec liov = {local, size};
    struct iovec riov = {remote, size};

    size_t done = 0;
    while (done < size) {
        ssize_t n = isWrite
            ? process_vm_writev(pid, &liov, 1, &riov, 1, 0)
            : process_vm_readv (pid, &liov, 1, &riov, 1, 0);
        if (n <= 0) return false;

        done += (size_t)n;
        liov.iov_base  = (char*)local  + done;
        liov.iov_len   = size - done;
        riov.iov_base  = (char*)remote + done;
        riov.iov_len   = size - done;
    }
    return true;
}

bool pmem_read (pid_t pid, void *remote, void *local, size_t size) {
    return xfer(pid, remote, local, size, false);
}
bool pmem_write(pid_t pid, void *remote, const void *local, size_t size) {
    return xfer(pid, remote, (void*)local, size, true);
}

// 批量：一次系统调用读多块
bool pmem_read_batch(pid_t pid, void **remotes, void **locals,
                     size_t *sizes, int count, size_t *outRead) {
    if (count <= 0 || count > 64) return false;

    struct iovec liov[64], riov[64];
    size_t total = 0;
    for (int i = 0; i < count; i++) {
        liov[i].iov_base = locals[i];
        liov[i].iov_len  = sizes[i];
        riov[i].iov_base = remotes[i];
        riov[i].iov_len  = sizes[i];
        total += sizes[i];
    }

    ssize_t n = process_vm_readv(pid, liov, count, riov, count, 0);
    if (n <= 0) return false;
    if (outRead) *outRead = (size_t)n;
    return (size_t)n == total;
}
```

## 动手：读写你自己的 demo 进程

写一个"靶子"程序：

```c
// target.c
#include <stdio.h>
#include <unistd.h>

struct Player {
    int   hp;
    float x, y, z;
    char  name[16];
};

struct Player g_player = {100, 12.5f, 30.0f, 5.0f, "Alice"};

int main(void) {
    printf("pid=%d\n", getpid());
    printf("&g_player = %p\n", (void*)&g_player);
    fflush(stdout);
    while (1) {
        sleep(1);
        printf("\rhp=%d pos=(%.1f,%.1f,%.1f) name=%s   ",
               g_player.hp, g_player.x, g_player.y, g_player.z, g_player.name);
        fflush(stdout);
    }
    return 0;
}
```

写一个"读者"程序：

```c
// reader.c
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>    // ← pid_t 定义于此（缺了会报 unknown type name 'pid_t'）
#include <sys/uio.h>

struct Player {
    int   hp;
    float x, y, z;
    char  name[16];
};

int main(int argc, char **argv) {
    if (argc < 3) { printf("用法: %s <pid> <地址(16进制)>\n", argv[0]); return 1; }
    pid_t pid = atoi(argv[1]);
    void *addr = (void*)strtoull(argv[2], nullptr, 16);

    struct Player p;
    struct iovec local  = {&p, sizeof(p)};
    struct iovec remote = {addr, sizeof(p)};

    ssize_t n = process_vm_readv(pid, &local, 1, &remote, 1, 0);
    if (n != sizeof(p)) { perror("readv"); return 1; }

    printf("读到: hp=%d pos=(%.1f,%.1f,%.1f) name=%s\n",
           p.hp, p.x, p.y, p.z, p.name);

    // 改一下 hp
    int newHp = 9999;
    struct iovec l2 = {&newHp, sizeof(newHp)};
    struct iovec r2 = {addr, sizeof(newHp)};
    n = process_vm_writev(pid, &l2, 1, &r2, 1, 0);
    printf("写入 hp=%d, 返回 %zd\n", newHp, n);
    return 0;
}
```

**先编译**（两个程序分别编译）：

```bash
gcc target.c -o target
gcc reader.c -o reader
```

> [!tip] 编译报错时
> - `unknown type name 'pid_t'` → 缺 `<sys/types.h>`（上面 reader.c 已补上）
> - `undefined reference to 'process_vm_readv'` → 目标环境不支持该系统调用，
>   按本章前面"手写 syscall"一节改用 `syscall(270, ...)`

**再运行**（需要三个终端，或两个终端轮流）：

```bash
# 终端 1：跑 target（前台，能看到 hp 变化）
./target
# 输出：pid=12345
#       &g_player = 0x404060
#       （记下这两个值 —— 下面要用）

# 终端 2：跑 reader，把上面记的 pid 和地址填进去
./reader 12345 0x404060
# 应输出：
#   读到: hp=100 pos=(12.5,30.0,5.0) name=Alice
#   写入 hp=9999, 返回 4
```

回到终端 1，target 的下一行打印应该变成 `hp=9999`。

> [!warning] 每次重启 target，pid 和地址都会变
> 因为操作系统有 **ASLR**（地址空间随机化，第 17 章讲过）——
> 每次运行程序，加载地址都是随机的。
> **重新跑 target 后，必须重新记录 pid 和 `&g_player`，再用新值跑 reader。**
> 用旧地址会得到 `EFAULT`（地址不可访问）。
>
> 想固定地址调试？可以用 `setarch $(uname -m) -R ./target`（Linux）关闭 ASLR。

> [!tip] target 是无限循环，用 `Ctrl+C` 结束

**这就是跨进程读写的完整闭环。**

## 验收清单

- [ ] 理解 `iovec` 是"地址 + 长度"（写出结构体两个字段）
- [ ] 会写单块读，且处理了"部分成功"（说出为什么返回值可能小于请求）
- [ ] 会用 iovec 数组一次读多块 ★（用两块 iovec 一次读，验证都读到）
- [ ] 知道系统调用号因架构而异，会手写 syscall 兜底（写出 arm64 下 nr=270）
- [ ] 知道 `EPERM`/`ESRCH`/`EFAULT` 各自代表什么（权限/进程不存在/地址无效）
- [ ] 知道"读 8 字节和读 4KB 开销差不多"（说出为什么——系统调用固定开销占主）
- [ ] 跑通了 target/reader 实验，看到 hp 被改 ★（运行 target 和 reader，reader 改掉 target 的 hp）

→ 下一章：[[第75章-封装跨进程读写类]]　—— 把裸系统调用封装成一个好用的类：模板读、字符串读、错误处理、PID 管理。
