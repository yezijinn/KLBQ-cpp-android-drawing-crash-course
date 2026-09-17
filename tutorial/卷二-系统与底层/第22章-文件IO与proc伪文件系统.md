---
tags: [教程, 卷二, 操作系统, 文件IO]
day: 13
aliases: [ch22]
---

# 第 22 章 · 文件 IO 与 /proc 伪文件系统

> [!abstract] 本章目标
> 掌握 POSIX 文件 IO 三件套（`open/read/write`），
> 并能从 `/proc` 里读出系统信息。
> 本项目的配置文件、maps 解析、模块基址查找全靠这些。

> [!note] 承上
> 上一章讲了系统调用的原理。
> 本章讲**两个最常用的系统调用家族**：文件 IO（`open/read/write`）和 `/proc` 伪文件系统。

> [!tip] 本章内容较多，建议分两个"半天"学
> 这一章是卷二最"重"的一章，因为它是**两块知识的合集**：
>
> **第一个半天：文件 IO（通用底层技能）**
> - 两种 IO 的区别 → 文件描述符 fd → open 的 flags → read/write → 二进制读写例子
> - 学完能回答："`open` 和 `fopen` 有什么区别？fd 是什么？"
>
> **第二个半天：/proc 与 maps 解析（本项目的直接技能）**
> - /proc 伪文件系统 → 解析 maps 找模块基址 → 遍历目录 → maps 解析器实战
> - 学完能回答："怎么从一个进程名找到它的模块基址？"
>
> 两块的分界就是下面 **## /proc 伪文件系统** 这个标题。学累了可以在那里停。

## 先看两种 IO 的区别

| 层次 | 函数 | 特点 |
|---|---|---|
| 系统调用 | `open` `read` `write` `close` | 无缓冲，每次都是真系统调用 |
| C 标准库 | `fopen` `fread` `fprintf` `fclose` | 带缓冲，减少系统调用次数 |

```c
// 系统调用层
int fd = open("/data/Config", O_RDONLY);
read(fd, buf, 100);
close(fd);

// 标准库层
FILE *fp = fopen("/data/Config", "rb");
fread(buf, 1, 100, fp);
fclose(fp);

// 按行读（标准库专属，很方便）
while (fgets(line, sizeof(line), fp)) { ... }
```

**本项目混用两者：**
- 配置读写用 `open/read/write`（`ConfigManager.cpp`）
- maps、status 解析用 `fopen/fgets`（`SysHal.cpp`、`MemDriver.h`）

选哪个？**按行解析用 `fgets`，二进制块读写用 `read/write`。**

## 文件描述符 fd

`open` 返回一个小整数，叫文件描述符。它是**进程 fd 表的下标**。

```
进程 fd 表
┌────┬───────────────────┐
│ 0  │ stdin  标准输入    │
│ 1  │ stdout 标准输出    │
│ 2  │ stderr 标准错误    │
│ 3  │ 你打开的文件       │  ← open() 返回这个
│ 4  │ ...               │
└────┴───────────────────┘
```

三个预留 fd：

| fd | 名称 | C 常量 |
|---|---|---|
| 0 | 标准输入 | `STDIN_FILENO` |
| 1 | 标准输出 | `STDOUT_FILENO` |
| 2 | 标准错误 | `STDERR_FILENO` |

所以 `write(1, buf, n)` 就是往屏幕写。

> [!danger] 忘了 close 会泄漏 fd
> 每个进程默认上限 1024 个 fd（`ulimit -n` 查看）。
> 在循环里反复 `open` 不 `close`，很快就会 `EMFILE: Too many open files`。
> 本项目 `Driver::GetPid()` 里每个分支都记得 `closedir` + `close(fd)`，就是这个原因。

## open 的 flags

```c
#include <fcntl.h>

open(path, O_RDONLY);                          // 只读
open(path, O_WRONLY | O_CREAT, 0644);          // 只写，不存在则创建
open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);// 清空后写
open(path, O_RDWR | O_APPEND);                 // 读写，追加
```

| flag | 含义 |
|---|---|
| `O_RDONLY` / `O_WRONLY` / `O_RDWR` | 访问模式（三选一） |
| `O_CREAT` | 不存在就创建（需第三个参数 mode） |
| `O_TRUNC` | 存在则清空 |
| `O_APPEND` | 每次写都追加到末尾 |
| `O_CLOEXEC` | exec 时自动关闭（安全习惯） |

权限位 `0644`：

```
0 6 4 4
  │ │ │
  │ │ └─ 其它用户：r--
  │ └─── 同组用户：r--
  └───── 文件主人：rw-
r=4  w=2  x=1
```

本项目存配置：

```cpp
fd = open("/data/Config", O_WRONLY | O_CREAT | O_TRUNC, 0660);
```

`0660` = 主人读写、同组读写、其它无权限。

> [!warning] 实际权限还受 umask 影响
> `umask` 是"**权限掩码**"——创建文件时，系统会自动把你的权限位**减掉** umask 里的位。
> 它是"默认不该给别人的权限"。默认 umask 通常是 `022`，所以 `0660 & ~022` 实际得到 `0640`。
> 想精确控制权限，创建后调用 `chmod(path, 0660)`。

## read / write 的返回值

```c
ssize_t n = read(fd, buf, sizeof(buf));
```

| 返回值 | 含义 |
|---|---|
| `> 0` | 实际读到的字节数（**可能小于你要求的**） |
| `0` | 已到文件末尾（EOF） |
| `-1` | 出错，看 `errno` |

> [!danger] 短读（short read）是真实存在的
> 网络、管道、某些设备文件上，`read` 可能只读到部分数据就返回。
> 严谨的写法要循环：
> ```c
> size_t total = 0;
> while (total < want) {
>     ssize_t n = read(fd, buf + total, want - total);
>     if (n <= 0) break;
>     total += n;
> }
> ```
> 本项目 `ConfigManager::LoadConfig` 一次读取整个 Config 结构体，
> 在普通文件上通常一次成功，但严格来说也应该循环。

## 二进制读写的完整例子

本项目的配置存取（简化）：

```c
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    int   RunFPS;
    bool  PhysX;
    float 骨骼偏移X;
} Config;

int save_config(const char *path, const Config *cfg) {
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0660);
    if (fd < 0) return -1;
    ssize_t n = write(fd, cfg, sizeof(Config));
    close(fd);
    return n == sizeof(Config) ? 0 : -1;
}

int load_config(const char *path, Config *cfg) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    uint8_t raw[sizeof(Config)];
    memset(raw, 0, sizeof(raw));
    ssize_t n = read(fd, raw, sizeof(raw));
    close(fd);
    if (n <= 0) return -1;
    memcpy(cfg, raw, sizeof(Config));
    return 0;
}
```

> [!note] 这种"整块结构体落盘"的做法
> 优点：简单、快。
> 缺点：结构体一改，旧配置文件就全乱了。
> 本项目的解法：文件短于结构体时，缺失部分用默认值补（本项目 `ConfigManager` 的做法）。
> 更健壮的"带版本号"方案见 [[附录M-核心数据结构全解]] 的「结构体设计三原则」（扁平 / 定长 / 版本）。

## /proc 伪文件系统

`/proc` 里的文件：
- 大小显示为 0
- 内容由内核**动态生成**
- 读它不会真的访问磁盘

```bash
ls -l /proc/self/status
# -r--r--r-- 1 root root 0 9月 14 01:00 /proc/self/status
cat /proc/self/status     # 明明有内容
```

常用的：

| 路径 | 内容 | 本项目用途 |
|---|---|---|
| `/proc/<pid>/maps` | 内存映射 | 模块基址、扫描区域 |
| `/proc/<pid>/cmdline` | 启动命令 | 按包名找 PID |
| `/proc/<pid>/status` | 进程状态 | 调试 |
| `/proc/<pid>/mem` | 内存本体 | （不用，需要 ptrace） |
| `/proc/self/maps` | 自己的映射 | 调试 |
| `/proc/cpuinfo` | CPU 信息 | 判断架构 |
| `/proc/version` | 内核版本 | 兼容性判断 |

## 解析 maps：找模块基址

本项目 `SysHal::get_module_base()` 的逻辑：

```c
uintptr_t get_module_base(pid_t pid, const char *module_name) {
    char filename[64], line[1024];
    snprintf(filename, sizeof(filename), "/proc/%d/maps", pid);
    FILE *fp = fopen(filename, "r");
    if (!fp) return 0;

    uintptr_t base = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, module_name)) {          // 找到含模块名的行
            char *dash = strchr(line, '-');
            if (dash) {
                *dash = 0;                        // 截断成起始地址字符串
                base = strtoul(line, NULL, 16);   // 16 进制转整数
                break;
            }
        }
    }
    fclose(fp);
    return base;
}
```

> [!note] 这段用到的三个字符串函数
> 第 06 章讲过 `strlen`/`strcmp`/`strcpy` 等，这里补三个新的（都在 `<string.h>`）：
>
> | 函数 | 作用 | 例子 |
> |---|---|---|
> | `strstr(hay, needle)` | 在 `hay` 里找子串 `needle`，返回首次出现的**指针**（找不到返回 `NULL`） | `strstr(line, "libUE4")` |
> | `strchr(s, c)` | 在字符串里找**某个字符**，返回指向它的指针 | `strchr(line, '-')` |
> | `strtoul(s, end, base)` | 把字符串转成**无符号长整数**，`base` 指定进制（16 = 十六进制） | `strtoul("1F", NULL, 16)` → `31` |
>
> **为什么能"找到就截断"**：`strchr` 返回指向 `-` 的指针 `dash`，`*dash = 0` 把那个字符改成结束符 `\0`——
> 于是 `line` 就从 `"556b8a3000-556b8a9000 r-xp ..."` 变成了 `"556b8a3000"`，直接丢给 `strtoul` 就能解析。
> 这是 C 里"就地解析"的常用技巧。

> [!warning] `strstr` 是子串匹配，容易误判
> 找 `libUE4.so` 时，如果有一行是 `libUE4.so.backup` 也会命中。
> 更严谨的做法：匹配路径的**结尾**，且前面是 `/`。
> 本项目 `Driver::GetModuleAddress` 里就是这么做的：
> ```cpp
> size_t pos = fullPath.length() - moduleName.length();
> if (pos > 0 && fullPath[pos - 1] != '/') continue;   // 前面必须是 /
> if (fullPath.substr(pos) != moduleName) continue;    // 后缀必须完全相等
> ```

## 遍历目录：opendir / readdir

```c
#include <dirent.h>

DIR *dir = opendir("/proc");
struct dirent *entry;
while ((entry = readdir(dir)) != NULL) {
    printf("%s\n", entry->d_name);
}
closedir(dir);
```

`dirent` 关键字段：

```c
struct dirent {
    ino_t d_ino;             // inode
    unsigned char d_type;    // 类型：DT_DIR=目录 DT_REG=文件 DT_LNK=链接
    char d_name[256];        // 文件名
};
```

用 `d_type` 可以省掉一次 `stat` 调用：

```c
if (entry->d_type == DT_DIR) { /* 是目录 */ }
```

## 动手：写一个 maps 解析器

> [!tip] Windows 用户怎么办
> 本练习依赖 `/proc`，**Windows 上没有**。用 WSL，或在 Android 设备上跑。

任务：读出某个进程的所有 `r-xp`（可执行）区间，打印出来。

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    const char *pid = argc > 1 ? argv[1] : "self";
    char path[64];
    snprintf(path, sizeof(path), "/proc/%s/maps", pid);

    FILE *fp = fopen(path, "r");
    if (!fp) { perror(path); return 1; }

    char line[1024];
    printf("%-26s %-6s %s\n", "RANGE", "PERM", "PATH");
    while (fgets(line, sizeof(line), fp)) {
        unsigned long long start, end;
        char perms[8], offset[32], dev[16], inode[32], name[512] = "";

        // 格式: start-end perms offset dev inode [path]
        int n = sscanf(line, "%llx-%llx %7s %31s %15s %31s %511[^\n]",
                       &start, &end, perms, offset, dev, inode, name);
        if (n < 6) continue;

        if (strncmp(perms, "r-x", 3) == 0) {     // 只要可执行段
            printf("%08llx-%08llx %-6s %s\n", start, end, perms, name);
        }
    }
    fclose(fp);
    return 0;
}
```

`%511[^\n]` 表示"读到行尾（不含换行）"，用来捕获可能带空格的路径。

跑：`./mapscan self`，看看输出结果。这就是第 76 章 `memview` 的核心。

## 验收清单

- [ ] 知道 `open/read/write` 与 `fopen/fread` 的区别及各自适用场合（说出"系统调用无缓冲、标准库带缓冲；按行用 fgets"）
- [ ] 知道 fd 0/1/2 分别是什么，以及 fd 泄漏的后果（说出 stdin/stdout/stderr，泄漏会 EMFILE）
- [ ] 会设置 `open` 的 flags 和权限位（写 `open(path, O_WRONLY|O_CREAT, 0644)` 并验证文件创建）
- [ ] 知道 `read` 可能"短读"，以及怎么处理（写出循环读直到读满的代码）
- [ ] 能用 `fgets` + `sscanf` 解析 `/proc/pid/maps`（运行解析器，正确输出每行的区间和权限）
- [ ] 完成并运行了 maps 解析器（`./mapscan self` 输出所有 r-xp 区间）

→ 下一章：[[第23章-进程间通信概览]]　—— 知道 6 种主流 IPC 各自的代价和适用场合，并理解本项目为什么选"共享内存 + 轮询标志"而不是别的。
