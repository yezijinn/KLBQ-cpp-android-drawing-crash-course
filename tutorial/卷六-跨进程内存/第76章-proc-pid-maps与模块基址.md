---
tags: [教程, 卷六, 内存, maps]
day: 49
aliases: [ch76]
---

# 第 76 章 · /proc/pid/maps 与模块基址

> [!abstract] 本章目标
> 完成总纲承诺的第 4 个成果：**`memview`**——
> 一个能列出目标进程模块、扫描内存区域的小工具。

> [!note] 承上
> 有了读写器，但"读哪个地址"还没解决。
> 本章用 `/proc/pid/maps` **找到模块基址**，产出 `memview` 工具（总纲承诺的第 4 个成果）。

## 先看 memview 能输出什么

```
$ ./memview 1234
=== 进程 1234 的模块 ===
起始地址            结束地址              权限   模块
0x0000005f8a200000  0x0000005f8a300000   r--p   /data/local/tmp/target
0x0000005f8a300000  0x0000005f8a900000   r-xp   /data/local/tmp/target
0x0000007a1c000000  0x0000007a1d000000   r-xp   /system/lib64/libc.so
...

=== 可扫描区域 ===
共 45 个可读写区域，总计 128.4 MB

输入模块名查找: libc.so
  → 基址 0x0000007a1c000000
```

## maps 文件格式回顾

```
556b8a3000-556b8a9000 r-xp 00001000 fc:00 1234 /usr/bin/cat
└────┬────┘ └─┬─┘ └──┬──┘ └──┬─┘ └─┬┘ └──┬──┘
     │        │      │       │      │     └─ 路径
     │        │      │       │      └─ inode
     │        │      │       └─ 设备号
     │        │      └─ 文件内偏移
     │        └─ 权限
     └─ 地址区间（半开）
```

权限四字符：`r`/`-`、`w`/`-`、`x`/`-`、`p`(私有)/`s`(共享)。

## 解析实现

```cpp
struct MapRegion {
    uint64_t start;
    uint64_t end;
    uint8_t  perms;        // 位: 1=R 2=W 4=X
    bool     isPrivate;
    uint64_t offset;
    uint32_t devMajor, devMinor;
    uint64_t inode;
    std::string path;
};

std::vector<MapRegion> ParseMaps(const std::string &text) {
    std::vector<MapRegion> out;
    std::istringstream iss(text);
    std::string line;

    while (std::getline(iss, line)) {
        MapRegion r{};
        char permStr[8] = {};
        char pathBuf[512] = {};
        unsigned long long s, e, off, ino;
        unsigned dmaj, dmin;

        // sscanf 格式：地址-地址 权限 偏移 设备:设备 inode [路径]
        int n = sscanf(line.c_str(),
                       "%llx-%llx %4s %llx %x:%x %llu %511[^\n]",
                       &s, &e, permStr, &off, &dmaj, &dmin, &ino, pathBuf);
        if (n < 7) continue;

        r.start = s; r.end = e;
        r.perms = 0;
        if (permStr[0] == 'r') r.perms |= 1;
        if (permStr[1] == 'w') r.perms |= 2;
        if (permStr[2] == 'x') r.perms |= 4;
        r.isPrivate = (permStr[3] == 'p');
        r.offset = off;
        r.devMajor = dmaj; r.devMinor = dmin;
        r.inode = ino;
        if (n >= 8) r.path = pathBuf;

        out.push_back(r);
    }
    return out;
}
```

`%511[^\n]` 捕获可能含空格的路径。

## 找模块基址

```cpp
uint64_t FindModuleBase(const std::vector<MapRegion> &maps,
                        const std::string &name) {
    for (const auto &r : maps) {
        if (r.path.empty()) continue;
        if (r.path.length() < name.length()) continue;

        // 后缀完全匹配，且前面是 '/'
        size_t pos = r.path.length() - name.length();
        if (pos > 0 && r.path[pos - 1] != '/') continue;
        if (r.path.compare(pos, name.length(), name) != 0) continue;

        return r.start;      // 第一个映射通常就是基址
    }
    return 0;
}
```

> [!warning] 为什么要求"后缀完全匹配 + 前一个是 /"
> 用 `strstr` 或 `find` 会误判：
> - 找 `libUE4.so` 时命中 `libUE4.so.bak`
> - 找 `libc.so` 时命中 `libc.so.1`
>
> 本项目 `Driver::GetModuleAddress` 就是这么做的（第 22 章提过）。

## 段（segment）的概念

一个 `.so` 会被映射成多段：

```
7a1c000000-7a1c010000  r--p   libc.so     ← 段 0（只读头/重定位）
7a1c010000-7a1c100000  r-xp   libc.so     ← 段 1（代码）★
7a1c110000-7a1c120000  r--p   libc.so     ← 段 2（rodata）
7a1c120000-7a1c130000  rw-p   libc.so     ← 段 3（data/bss）
```

本项目 `GetModuleAddress(moduleName, segmentIndex, outAddr, isStart)`
的 `segmentIndex` 就是指这个：

| segmentIndex | 通常对应 |
|---|---|
| 0 | 可执行段（代码） |
| 1 | 只读数据 |
| 2 | 可读写数据 |
| -1 | BSS |

按权限分组后编号：

```cpp
int ClassifySegment(uint8_t perms) {
    if (perms == 5) return 0;      // r-x
    if (perms == 4) return 1;      // r--
    if (perms == 6) return 2;      // rw-
    return -1;
}
```

## 可扫描区域

找所有**可读写**的区域（用于内存扫描）：

```cpp
std::vector<std::pair<uint64_t,uint64_t>> GetScannableRegions(
        const std::vector<MapRegion> &maps) {
    std::vector<std::pair<uint64_t,uint64_t>> out;
    for (const auto &r : maps) {
        // 只要可读写、已映射文件/匿名的
        if (!(r.perms & 1)) continue;      // 不可读，跳过
        if (!(r.perms & 2)) continue;      // 不可写，跳过
        if (r.perms & 4) continue;         // 可执行（代码段，一般不变）
        if (r.end <= r.start) continue;
        out.emplace_back(r.start, r.end);
    }
    return out;
}
```

本项目 `MemDriver::GetScanRegions()` 在系统调用模式下就是这么做的：

```cpp
std::vector<std::pair<uintptr_t, uintptr_t>> GetScanRegions() override {
    std::vector<std::pair<uintptr_t, uintptr_t>> regions;
    if (kernel_ != nullptr) return kernel_->GetScanRegions();
    if (syscall_ != nullptr) {
        char filename[64];
        snprintf(filename, sizeof(filename), "/proc/%d/maps", GetGlobalPid());
        FILE *fp = fopen(filename, "r");
        if (fp == nullptr) return regions;
        char line[1024];
        while (fgets(line, sizeof(line), fp)) {
            unsigned long long start = 0, end = 0;
            if (sscanf(line, "%llx-%llx", &start, &end) == 2 && end > start) {
                regions.emplace_back(start, end);
            }
        }
        fclose(fp);
    }
    return regions;
}
```

## memview 主程序

> [!warning] 下面的代码用到三个函数，它们定义在本章前面
> `ParseMaps()`（"解析实现"一节）、`FindModuleBase()`（"找模块基址"一节）、
> `GetScannableRegions()`（"可扫描区域"一节）。
> **要编译，得把这三段函数复制到 `struct MapRegion` 下方**，
> 或者直接看附录 I / 附录 E 里整合好的完整文件。

```cpp
// memview.cpp
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>       // uint64_t / uint8_t / uint32_t
#include <sys/types.h>   // pid_t
#include <string>
#include <vector>
#include <fstream>
#include <sstream>

struct MapRegion {
    uint64_t start;
    uint64_t end;
    uint8_t  perms;        // 位: 1=R 2=W 4=X
    bool     isPrivate;
    uint64_t offset;
    uint32_t devMajor, devMinor;
    uint64_t inode;
    std::string path;
};

std::vector<MapRegion> g_maps;

bool LoadMaps(pid_t pid) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/maps", pid);
    std::ifstream f(path);
    if (!f) { printf("打不开 %s（进程不存在或没权限）\n", path); return false; }
    std::stringstream ss;
    ss << f.rdbuf();
    g_maps = ParseMaps(ss.str());
    return !g_maps.empty();
}

void PrintModules() {
    printf("=== 模块列表 ===\n");
    printf("%-18s %-18s %-6s %s\n", "起始", "结束", "权限", "路径");
    std::string last;
    for (const auto &r : g_maps) {
        if (r.path.empty() || r.path == last) continue;
        last = r.path;
        char p[8] = "---";
        if (r.perms & 1) p[0] = 'r';
        if (r.perms & 2) p[1] = 'w';
        if (r.perms & 4) p[2] = 'x';
        printf("0x%016llX 0x%016llX %-6s %s\n",
               (unsigned long long)r.start, (unsigned long long)r.end,
               p, r.path.c_str());
    }
}

void PrintScannable() {
    auto regions = GetScannableRegions(g_maps);
    size_t total = 0;
    for (auto &r : regions) total += r.second - r.first;
    printf("\n=== 可扫描区域: %zu 个，共 %.1f MB ===\n",
           regions.size(), total / 1024.0 / 1024.0);
}

int main(int argc, char **argv) {
    if (argc < 2) { printf("用法: %s <pid> [模块名]\n", argv[0]); return 1; }

    pid_t pid = atoi(argv[1]);
    if (!LoadMaps(pid)) return 1;

    PrintModules();
    PrintScannable();

    if (argc >= 3) {
        uint64_t base = FindModuleBase(g_maps, argv[2]);
        if (base) printf("\n模块 %s 基址 = 0x%016llX\n", argv[2],
                         (unsigned long long)base);
        else       printf("\n未找到模块 %s\n", argv[2]);
    }
    return 0;
}
```

编译运行：

```bash
g++ -std=c++17 memview.cpp -o memview    # 注意用 g++（代码用了 vector/string/ifstream）
./memview <pid>
./memview <pid> libc.so
```

## 实用技巧：合并相邻区域

扫描时应该合并相邻的同类区域，减少扫描次数：

```cpp
void MergeRegions(std::vector<std::pair<uint64_t,uint64_t>> &regions) {
    if (regions.empty()) return;
    std::sort(regions.begin(), regions.end());

    size_t w = 0;
    for (size_t i = 1; i < regions.size(); i++) {
        if (regions[i].first <= regions[w].second) {
            regions[w].second = std::max(regions[w].second, regions[i].second);
        } else {
            regions[++w] = regions[i];
        }
    }
    regions.resize(w + 1);
}
```

本项目 `Driver::GetScanRegions()` 里就有这段（第 22 章提过）：

```cpp
std::sort(regions.begin(), regions.end(), [](const auto &l, const auto &r) {
    return l.first < r.first || (l.first == r.first && l.second < r.second);
});
size_t mergedCount = 0;
for (const auto &region : regions) {
    if (mergedCount == 0 || region.first > regions[mergedCount-1].second) {
        regions[mergedCount++] = region;
        continue;
    }
    regions[mergedCount-1].second = std::max(
        regions[mergedCount-1].second, region.second);
}
regions.resize(mergedCount);
```

## 注意：maps 会变

进程运行中：
- 会 `mmap` 新区域（加载库、分配大内存）
- 会 `munmap` 释放
- 堆会增长

**所以要定期重新读取 maps**，不能缓存一辈子。

本项目每次"初始化"时重新获取模块列表：
`GetMemoryInfoRef()` 内部会向驱动请求一次最新的内存信息。

## 动手：扩展 memview

> [!important] 前置条件
> 需要 **Linux / WSL / Android**（依赖 `/proc/<pid>/maps`）。
> 读**自己的 demo 进程**无需 root；读别人的进程才要。

加三个功能：

1. **按权限过滤**：`./memview <pid> --perm rw` 只显示可读写
2. **统计各模块大小**：按路径聚合，显示每个模块占多少
3. **导出为文件**：把 maps 存成文本，便于对比两次快照的差异

第 3 项特别有用——**对比游戏启动前后的 maps**，
能看出它加载了哪些库（这是分析的第一步）。

## 运行轨迹透视

> [!tip] memview 是命令行工具，但它背后的驱动调用应记录结构化日志
> 命令行程序的**用户可见输出**用 printf 没问题；
> 但一旦这段逻辑被集成进 driver.h（真实项目就是如此），
> 就该换成本项目的 LS_LOGI_TAG / LS_LOGE_TAG，让它出现在 logcat 里。

### 真实项目的日志形态（driver.h GetModuleAddress）

```cpp
// 成功：逐模块打印区段（driver.h 真实代码）
LS_LOGI_TAG("Driver", "模块索引=%d 名称=%s 区段数量=%d", i, mod.name, mod.seg_count);
LS_LOGI_TAG("Driver", "区段[%d] index=%d start=0x%016llX end=0x%016llX size=0x%llX (%llu bytes) prot=%d",
            j, seg.index,
            (unsigned long long)seg.start, (unsigned long long)seg.end,
            (unsigned long long)(seg.end - seg.start),
            (unsigned long long)(seg.end - seg.start), seg.prot);

// 失败：未找到模块/区段（driver.h 真实代码）
LS_LOGE_TAG("Driver", "模块 '%.*s' 中未找到区段索引 %d",
            (int)moduleName.size(), moduleName.data(), segmentIndex);
LS_LOGE_TAG("Driver", "未找到模块 '%.*s'", (int)moduleName.size(), moduleName.data());
```

### 成功路径的真实 logcat 输出

```text
I/Driver  ( 8123): 模块索引=0 名称=libUE4.so 区段数量=4
I/Driver  ( 8123): 区段[0] index=0 start=0x0000007A1C000000 end=0x0000007A1C010000 size=0x10000 (65536 bytes) prot=1
I/Driver  ( 8123): 区段[1] index=1 start=0x0000007A1C010000 end=0x0000007A1C100000 size=0xF0000 (983040 bytes) prot=5
I/Driver  ( 8123): 区段[2] index=2 start=0x0000007A1C110000 end=0x0000007A1C120000 size=0x10000 (65536 bytes) prot=1
```

### 失败路径的真实 logcat 输出

```text
E/Driver  ( 8123): 模块 libUE4.so 中未找到区段索引 9
E/Driver  ( 8123): 未找到模块 libNotExist.so
E/Driver  ( 8123): 获取内存信息失败
```

> [!note] 关键观测点
> - **tag 用模块名**（Driver/Dump）区分「谁在说话」；
> - **地址统一 0x%016llX 补零到 16 位**，方便肉眼对齐；
> - **失败必带具体名字**（%.*s 打印 string_view），而非笼统的「失败」——
>   这是 driver.h 的规范：**每条 ERROR 都要能独立定位问题**。

## 动手验证清单

- [ ] **看 maps 格式**：`adb shell cat /proc/self/maps | head -5` → 每行含 起始-结束 权限 偏移 路径
- [ ] **解析一行**：用 `sscanf` 从 maps 行提取 `start/end/perms`（见第 22 章习题 22.1）
- [ ] **找模块基址**：对 `/proc/<pid>/maps` 找含 `libUE4.so` 的行 → 第一行起始地址就是基址
- [ ] **筛可扫描区域**：过滤出 `r--`/`rw-` 的段（跳过 `---`/`r-xp` 纯代码段）
- [ ] **合并相邻**：把地址连续的段合并（减少扫描区域数）
- [ ] **跑通 memview**：`./memview <pid>` → 列出模块 + 基址

> [!note] maps 会变
> 库可能卸载/重载，maps 内容随进程状态变化——**不能永久缓存**，每次用前重读。

## 常见坑排查表

| 症状 | 最可能的原因 | 解法 |
|---|---|---|
| 找不到目标模块 | 模块名后缀不匹配 | 用**后缀匹配**（`libUE4.so` 可能显示为全路径）|
| 基址每次都变 | maps 重新加载了库 | 每次用前重读 maps，别缓存 |
| 读 `/proc/<pid>/maps` 为空 | 权限不足 | 需要 root 或同 UID |
| 扫描时崩在某个区域 | 该区域不可读（`---`）| 只扫 `r--`/`rw-` 的段 |

## 综合实战：扩展 memview 成一个内存分析工具（脱离指导）

> [!important] 独立完成
> 基础 `memview` 只列模块。现在把它扩展成一个**有实际用处的分析工具**。

**业务需求**：`memview <pid> [选项]` 支持：

```bash
./memview 1234                 # 默认：列模块 + 基址
./memview 1234 --maps          # 打印完整 maps 分组（可读/可写/可执行）
./memview 1234 --find 0x7f00   # 找哪个模块落在该地址（定位崩溃地址用）
./memview 1234 --regions       # 列出可扫描区域（合并相邻）
```

**接口骨架**：

```cpp
#include <vector>
#include <string>
#include <cstdint>

struct Region {
    uint64_t start, end;
    char perms[5];      // "r-xp"
    std::string path;   // 模块路径
};

// 解析 /proc/<pid>/maps 的每一行（第 22 章学过 sscanf）
std::vector<Region> ParseMaps(int pid) {
    // TODO: 逐行 sscanf 出 start/end/perms/path
}

// --find：二分查找地址落在哪个 Region
const Region* FindRegion(const std::vector<Region>& regions, uint64_t addr) {
    // TODO: 用二分（regions 已按地址升序）
}

// --regions：只留 r--/rw- 的段，合并相邻
std::vector<Region> ScanRegions(const std::vector<Region>& regions) {
    // TODO: 过滤 + 合并
}

int main(int argc, char** argv) {
    // TODO: 解析 pid + 选项，分派到上面三个函数
}
```

**自主实现要求**：

| 要求 | 考察点 |
|---|---|
| `--find` 用**二分**（不是线性扫）| 算法效率 |
| `--regions` 合并相邻段 | 区间合并 |
| maps 每行解析健壮（path 可能为空）| 边界处理 |
| pid 不存在时明确报错 | 错误处理 |
| 输出格式化（对齐的表格）| 可用性 |

**验收**：`--maps` 输出与 `cat /proc/<pid>/maps` 一致；`--find <崩溃地址>` 能报出对应模块。

## 性能考量与复杂度说明

**【时间复杂度】**

| 操作 | 复杂度 | 说明 |
|---|---|---|
| 解析 maps | O(L) | L = maps 行数（通常几十~几百）|
| 线性查找地址 | O(L) | 逐行比对 |
| **二分查找**（`--find`）| O(log L) | maps 按地址升序，可二分 |
| 合并相邻区域 | O(L) | 一次遍历 |

**【空间开销】**
- 每行解析出一个 `Region`（约 30 字节），L 行共 `L × 30` 字节——可忽略。

**【并发与 I/O 瓶颈】**
- 读 `/proc/<pid>/maps` 是一次**文件 I/O**（内核生成，几毫秒）。
- **不能缓存**：库可能卸载/重载，maps 随进程状态变化。
- **建议**：`--find` 用二分而非线性（本章练习要求）。

> [!note] 工程权衡：为何每次重读 maps？
> 缓存看似省事，但**目标进程状态会变**（库加载/卸载、内存映射变化），
> 用过期 maps 会算错基址。**正确性优先于性能**——几毫秒的读取代价值得。

## 课后习题

### 习题 76.1 解析 maps 找基址（★★，应用变式）

**任务要求**：写一个函数，从 `/proc/pid/maps` 中找到指定模块的**基址**（即该模块**第一个映射段的起始地址**，也就是 ELF 头所在处）。

**参考骨架**：

```cpp
uint64_t FindModuleBase(int pid, const char* name) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/maps", pid);
    FILE* fp = fopen(path, "r");
    if (!fp) return 0;

    char line[512];
    uint64_t base = 0;
    while (fgets(line, sizeof(line), fp)) {
        uint64_t start, end;
        char perms[8], mod[256] = {0};
        if (sscanf(line, "%lx-%lx %7s %*s %*s %*s %255s",
                   &start, &end, perms, mod) < 4) continue;
        if (strstr(mod, name)) {
            if (base == 0 || start < base) base = start;   // 取最小起点
        }
    }
    fclose(fp);
    return base;
}
```

**验证断言**：对 `libc.so` 调用，返回的地址应落在 `0x7xxxxxxx` 量级（用户态库区）。

> [!note] 为什么取"第一个（最低）映射段起点"，而不是 r-xp 段
> 模块的**第一个映射段通常是 `r--p`**，文件偏移为 0——**ELF 头就在 `base + 0`**。
> 第 77 章解析符号时要读 `base + e_phoff`，**必须用这个 r--p 段的起点**。
> 若误取 `r-xp` 段起点（通常是 `base + 0x1000` 量级），
> `base + e_phoff` 就会读到错误位置，魔数检查（`\x7fELF`）直接失败。
> **一句话：基址 = 第一个映射段起点，不是可执行段起点。**

### 习题 76.2 分析"读 maps 为什么这么重要"（★★★，分析+评价）

**任务要求**：本章说"**读哪个地址**"是内存读取的起点问题。分析：

1. 硬编码一个绝对地址会有什么问题？
2. `maps` 解决了哪一类问题？（结合 ASLR）
3. 什么情况下 `maps` 也**不够**？

**参考答案要点**：
1. 每次进程启动地址都不同（ASLR），硬编码必然失效；
2. maps 给出"**模块基址 + 区间**"，把"绝对地址"变成"基址 + 相对偏移"，抗随机化；
3. maps 只给到"区间级"信息，**不知道区间里哪个函数在哪**——需要第 77 章的 ELF 符号解析补齐。

> [!tip] 评价层要点
> 理解两级定位：**maps 定位"模块从哪开始"，ELF 符号定位"模块里某函数在哪"**。
> 单独用任一级都不够。

## 自动化单元自测

> [!tip] 本节可独立编译运行
> 对应本章核心单元：ParseMaps(解析 maps 行)、FindModuleBase(找模块基址)。
> 用一段固定的 maps 文本做输入，验证解析字段和基址选取逻辑。

### 测试脚本（保存为 ch76_test.cpp）

```cpp
// ch76_test.cpp —— 第76章 maps 解析单元自测（自包含）
#include <cstdio>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>
#include <string>

struct MapRegion {
    uint64_t start = 0, end = 0;
    uint8_t  perms = 0;         // 位: 1=R 2=W 4=X
    bool     isPrivate = false;
    std::string path;
};

// ===== 被测单元1：解析一行 maps =====
// 格式: start-end perms offset dev:dev inode path
bool ParseMapLine(const char* line, MapRegion& r) {
    unsigned long long s, e, off, ino;
    unsigned dmaj, dmin;
    char perms[8] = {};
    char path[512] = {};
    int n = std::sscanf(line, "%llx-%llx %7s %llx %x:%x %llu %511s",
                        &s, &e, perms, &off, &dmaj, &dmin, &ino, path);
    if (n < 7) return false;
    r.start = s; r.end = e;
    r.perms = 0;
    if (perms[0]=='r') r.perms |= 1;
    if (perms[1]=='w') r.perms |= 2;
    if (perms[2]=='x') r.perms |= 4;
    r.isPrivate = (perms[3]=='p');
    if (n >= 8) r.path = path;
    return true;
}

// ===== 被测单元2：找模块基址（取第一个映射段起点 = ELF 头处）=====
uint64_t FindModuleBase(const std::vector<MapRegion>& maps, const std::string& name) {
    uint64_t base = 0;
    for (const auto& r : maps) {
        if (r.path.empty()) continue;
        if (r.path.length() < name.length()) continue;
        size_t pos = r.path.length() - name.length();
        if (pos > 0 && r.path[pos-1] != '/') continue;
        if (r.path.compare(pos, name.length(), name) != 0) continue;
        if (base == 0 || r.start < base) base = r.start;   // 取最小起点
    }
    return base;
}

int main() {
    // Arrange: 一段典型 libc.so 的多段映射
    std::vector<MapRegion> maps;
    MapRegion r;
    ParseMapLine("7a1c000000-7a1c010000 r--p 00000000 00:00 0 /system/lib64/libc.so", r); maps.push_back(r);
    ParseMapLine("7a1c010000-7a1c100000 r-xp 00010000 00:00 0 /system/lib64/libc.so", r); maps.push_back(r);
    ParseMapLine("7a1c110000-7a1c120000 r--p 00010000 00:00 0 /system/lib64/libc.so", r); maps.push_back(r);
    ParseMapLine("7a1c120000-7a1c130000 rw-p 00010000 00:00 0 /system/lib64/libc.so", r); maps.push_back(r);

    // ===== 正常路径 =====
    // 1. 第一行权限 = r--p（只读私有），perms=1
    assert(maps[0].perms == 1 && maps[0].isPrivate);
    std::printf("[PASS] 解析 r--p 权限\n");

    // 2. 第二行权限 = r-xp（可执行），perms=5
    assert(maps[1].perms == 5 && maps[1].isPrivate);
    std::printf("[PASS] 解析 r-xp 权限\n");

    // 3. 基址 = 第一个映射段起点（含 ELF 头），不是 r-xp 段
    uint64_t base = FindModuleBase(maps, "libc.so");
    assert(base == 0x7a1c000000ULL);
    assert(base == maps[0].start);         // 第一段起点
    std::printf("[PASS] 基址取第一个映射段起点\n");

    // 4. 后缀匹配：找 libc.so 不应命中 libc.so.1
    MapRegion r2;
    ParseMapLine("7a2c000000-7a2c010000 r--p 0 00:00 0 /system/lib64/libc.so.1", r2);
    std::vector<MapRegion> maps2 = { r2 };
    assert(FindModuleBase(maps2, "libc.so") == 0);   // 不匹配（前一个是 . 不是 /）
    std::printf("[PASS] 后缀精确匹配（不误命中 .1）\n");

    // ===== 反向异常路径 =====
    // 5. 空行/格式错误应返回 false
    assert(!ParseMapLine("garbage", r));
    std::printf("[PASS] 反向: 非法行被拒绝\n");

    // 6. 找不到模块返回 0
    assert(FindModuleBase(maps, "libnotexist.so") == 0);
    std::printf("[PASS] 反向: 模块不存在返回 0\n");

    std::printf("\n第76章 全部断言通过\n");
    return 0;
}
```

### 执行指引

```bash
g++ -std=c++17 -Wall ch76_test.cpp -o ch76_test && ./ch76_test
```

**预期成功输出**：

```text
[PASS] 解析 r--p 权限
[PASS] 解析 r-xp 权限
[PASS] 基址取第一个映射段起点
[PASS] 后缀精确匹配（不误命中 .1）
[PASS] 反向: 非法行被拒绝
[PASS] 反向: 模块不存在返回 0

第76章 全部断言通过
```

## 本章小结

> [!abstract] 本章要点已收束
> 把上面的动手与结论串成一句话，再进入下一章。

## 验收清单

- [ ] 会用 `sscanf` 解析 maps 每一行（解析出起止地址和权限）
- [ ] 知道权限字符 `r/w/x/p` 的含义（说出 p=私有、s=共享）
- [ ] 会实现"后缀匹配 + 前一个是 /"的模块查找 ★（正确找到 libUE4.so 且不误匹配 .backup）
- [ ] 理解 segment（段）与 section（节）的区别（说出"加载器看段、链接器看节"）
- [ ] 会筛出可扫描区域并合并相邻（输出合并后的区间列表）
- [ ] **跑通了 memview，能看到模块基址** ★（运行 memview，输出某模块基址）
- [ ] 知道 maps 会变，不能永久缓存（说出"库可能卸载或重载"）

→ 下一章：[[第77章-ELF解析找符号]]　—— 从模块基址出发，解析内存里的 ELF 结构，按名字找到函数/变量的地址。
