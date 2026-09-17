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
> `ParseMaps()`（第 60 行附近）、`FindModuleBase()`（第 100 行附近）、
> `GetScannableRegions()`（第 161 行附近）。
> **要编译，得把这三段函数复制到 `struct MapRegion` 下方**，
> 或者直接看附录 I / 附录 E 里整合好的完整文件。

```cpp
// memview.cpp
#include <cstdio>
#include <cstdlib>
#include <cstring>
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

加三个功能：

1. **按权限过滤**：`./memview <pid> --perm rw` 只显示可读写
2. **统计各模块大小**：按路径聚合，显示每个模块占多少
3. **导出为文件**：把 maps 存成文本，便于对比两次快照的差异

第 3 项特别有用——**对比游戏启动前后的 maps**，
能看出它加载了哪些库（这是分析的第一步）。

## 验收清单

- [ ] 会用 `sscanf` 解析 maps 每一行（解析出起止地址和权限）
- [ ] 知道权限字符 `r/w/x/p` 的含义（说出 p=私有、s=共享）
- [ ] 会实现"后缀匹配 + 前一个是 /"的模块查找 ★（正确找到 libUE4.so 且不误匹配 .backup）
- [ ] 理解 segment（段）与 section（节）的区别（说出"加载器看段、链接器看节"）
- [ ] 会筛出可扫描区域并合并相邻（输出合并后的区间列表）
- [ ] **跑通了 memview，能看到模块基址** ★（运行 memview，输出某模块基址）
- [ ] 知道 maps 会变，不能永久缓存（说出"库可能卸载或重载"）

→ 下一章：[[第77章-ELF解析找符号]]　—— 从模块基址出发，解析内存里的 ELF 结构，按名字找到函数/变量的地址。
