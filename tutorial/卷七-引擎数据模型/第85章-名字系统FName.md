---
tags: [教程, 卷七, 引擎, 字符串]
day: 54
aliases: [ch85]
---

# 第 85 章 · 名字系统 FName

> [!abstract] 本章目标
> 理解引擎为什么用"字符串表 + ID"而不是直接存字符串，
> 并实现一个完整的名字解析器。

> [!note] 承上
> 上一章能遍历出对象了，但"这是谁"还不知道。
> 本章解析**名字系统 FName**——从 ID 还原出字符串（第 06 章 UTF 转换的应用）。

## 先看为什么

场景里 100 个敌人，每个都叫 `BP_Character_Enemy`。

朴素做法：每个对象存一份字符串。

```
Actor[0]: "BP_Character_Enemy"   18 字节
Actor[1]: "BP_Character_Enemy"   18 字节
...
Actor[99]: "BP_Character_Enemy"  18 字节
------------------------------------------
总计 1800 字节 + 100 次字符串比较
```

字符串表做法：

```
名字池:
  [1] "BP_Character_Player"
  [2] "BP_Character_Enemy"
  [3] "BP_Character_JiaRen"

Actor[0].nameId = 2    4 字节
Actor[1].nameId = 2    4 字节
...
------------------------------------------
总计 400 字节 + 比较时只需比整数
```

**省内存 + 比较快（整数比较 vs 逐字符比较）。**

这就是 UE4 的 `FName`、Unity 的 `StringTable`。

## FName 的结构

一个 `FName` 就是一个整数（ComparisonIndex）：

```
bit 31                    16 15              0
┌──────────────────────────┬──────────────────┐
│      Block (块索引)        │   Offset (块内)   │
└──────────────────────────┴──────────────────┘
    高 16 位                    低 16 位
```

解析：

```cpp
uint32_t blockIdx = nameId >> 16;       // 块索引
uint32_t blockOff = nameId & 0xFFFF;    // 块内偏移（单位见下方说明，不是"条目数"）
```

> [!note] `blockOff` 的单位不是"条目个数"
> 新手容易误以为 `blockOff` 是"跳过几个 FNameEntry"。
> 实际它是"跳过几个 **2 字节单位**"（下面「blockOff * 2」一节会解释为什么）。
> 所以从 `blockOff` 算真实地址时要 **× 2**，不能直接当条目数用。

## 名字池结构

UE4 的 `FNamePool`：

```
FNamePool
  ├─ Blocks[0]  → 指向第 0 块
  ├─ Blocks[1]  → 指向第 1 块
  ├─ Blocks[2]  → ...
  └─ ...
  
每块大小 65536 字节（0x10000），存若干个 FNameEntry
```

每个 `FNameEntry`：

```
偏移 0x00: uint16_t header
偏移 0x02: 字符数据（UTF-8 或 UTF-32）
```

`header` 的位布局：

```
bit 15                6 5           0
┌─────────────────────┬─────────────┐
│   Length (长度)       │  标志位      │
└─────────────────────┴─────────────┘
     bit 6~15              bit 0: 是否宽字符
```

解析：

```cpp
uint32_t len    = header >> 6;
bool     isWide = header & 1;
```

## 本项目的实现（完整）

```cpp
std::string GetNameById(uint32_t nameId) {
    uint32_t blockIdx = nameId >> 16;
    uint32_t blockOff = nameId & 0xFFFF;

    // 1. 读块指针：GName + 0x40 + blockIdx * 8
    uint64_t blockAddr = 0;
    if (!dr->Read(GName + 0x40 + blockIdx * 8, &blockAddr, 8) || !blockAddr) {
        return {};
    }

    // 2. 条目地址 = 块地址 + 块内偏移 * 2
    uint64_t entryAddr = blockAddr + blockOff * 2;

    // 3. 读头部
    uint16_t header = 0;
    if (!dr->Read(entryAddr, &header, 2)) {
        return {};
    }

    uint32_t len    = header >> 6;
    bool     isWide = header & 1;

    // 4. 合法性检查
    if (len == 0 || len > 1024) {
        return {};
    }

    // 5. 读字符数据
    if (isWide) {
        std::u32string u32(len, 0);
        dr->Read(entryAddr + 2, u32.data(), len * 4);

        std::string s;
        s.reserve(len * 3);
        for (char32_t c : u32) {
            AppendUtf8(s, c);      // UTF-32 → UTF-8
        }
        return s;
    } else {
        std::string s(len, 0);
        if (!dr->Read(entryAddr + 2, s.data(), len)) {
            return {};
        }
        return s;
    }
}
```

## 逐点说明

### 1. `GName + 0x40`

`0x40` 是 `FNamePool` 里 `Blocks` 数组的偏移。
它前面是锁、当前块数等字段。

**这个偏移是版本相关的**——UE 4.23 和 4.27 可能不同。

### 2. `blockOff * 2`

这是代码里最容易被误解的一处：

```cpp
uint64_t entryAddr = blockAddr + blockOff * 2;
```

`blockOff` 是 `nameId` 的低 16 位，单位是**"2 字节"**而非"条目"。
也就是说：**块内偏移是字节数，且按 2 字节对齐**。

> [!note] 为什么是 ×2
> 因为 `FNameEntry` 最小是 2 字节（头部），
> UE4 用"除以 2 后的值"作为偏移存储，以扩大低 16 位的寻址范围。
> 所以从 `nameId` 还原地址时要乘回 2。
>
> 这也是为什么**必须实测验证**：如果你的目标版本不是这样，解析就会全错。

### 3. 长度上限检查

```cpp
if (len == 0 || len > 1024) return {};
```

`header` 是从内存读来的，可能是垃圾。
长度是垃圾 → 分配巨大缓冲 → 内存爆炸。

**任何从外部读来的"长度"都必须检查上界。**

### 4. 宽字符处理

`isWide = true` 表示 UTF-32（每个字符 4 字节）。
读 `len * 4` 字节，再转 UTF-8。

`AppendUtf8` 的实现（第 06 章讲过原理）：

```cpp
static void AppendUtf8(std::string& out, char32_t c) {
    if (c < 0x80) {
        out += (char)c;
    } else if (c < 0x800) {
        out += (char)(0xC0 | (c >> 6));
        out += (char)(0x80 | (c & 0x3F));
    } else if (c < 0x10000) {
        out += (char)(0xE0 | (c >> 12));
        out += (char)(0x80 | ((c >> 6) & 0x3F));
        out += (char)(0x80 | (c & 0x3F));
    } else {
        out += (char)(0xF0 | (c >> 18));
        out += (char)(0x80 | ((c >> 12) & 0x3F));
        out += (char)(0x80 | ((c >> 6) & 0x3F));
        out += (char)(0x80 | (c & 0x3F));
    }
}
```

## 我们自己实现一个名字表

给第 83 章的 demo 引擎加上：

```cpp
// name_table.h
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

// 模拟 FNamePool
struct FNameEntry {
    uint16_t header;          // bit6-15: 长度, bit0: 是否宽字符
    char     data[];          // 变长
};

class NameTable {
public:
    // 加入一个名字，返回 ID
    uint32_t Intern(const std::string &name);

    // 按 ID 取名字
    std::string GetName(uint32_t id) const;

    // 供外部程序读取的结构
    uint64_t BlocksAddr() const { return (uint64_t)blocks_.data(); }

private:
    static constexpr size_t BLOCK_SIZE = 65536;

    struct Block {
        std::vector<uint8_t> data;
        Block() : data(BLOCK_SIZE, 0) {}
    };

    std::vector<Block> blocks_;                      // 名字块
    std::unordered_map<std::string, uint32_t> index_;// 名字 → ID
    std::vector<std::pair<uint32_t,uint32_t>> entries_; // ID → (块, 偏移)
};
```

实现：

```cpp
uint32_t NameTable::Intern(const std::string &name) {
    auto it = index_.find(name);
    if (it != index_.end()) return it->second;

    // 新名字：找一个块放进去
    uint32_t blockIdx = 0, offset = 0;
    bool placed = false;

    for (size_t b = 0; b < blocks_.size() && !placed; b++) {
        // 简化：从块内偏移 0 开始找空位（实际要维护空闲链表）
        // 这里简化为：新名字放块末尾
        uint32_t used = 0;
        for (auto &e : entries_)
            if (e.first == b) used += e.second;
        // 简化处理...
    }

    // 简化版：每个块最多放 1000 个名字，超了开新块
    // （真实实现要处理内存布局，这里只演示原理）
    ...
}

std::string NameTable::GetName(uint32_t id) const {
    uint32_t blockIdx = id >> 16;
    uint32_t blockOff = id & 0xFFFF;

    if (blockIdx >= blocks_.size()) return {};

    const uint8_t *entry = blocks_[blockIdx].data.data() + blockOff * 2;
    uint16_t header;
    memcpy(&header, entry, 2);

    uint32_t len    = header >> 6;
    bool     isWide = header & 1;

    if (len == 0 || len > 1024) return {};

    if (isWide) {
        // UTF-32 → UTF-8
        const char32_t *u32 = (const char32_t*)(entry + 2);
        std::string out;
        for (uint32_t i = 0; i < len; i++) AppendUtf8(out, u32[i]);
        return out;
    } else {
        return std::string((const char*)(entry + 2), len);
    }
}
```

> [!tip] 教学版本可以简化
> 真实的 FNamePool 有内存对齐、空闲链表、多线程锁等复杂机制。
> 学习阶段只需要理解三点：
> 1. 名字存中央表，对象存 ID
> 2. ID 拆成"块号 + 块内偏移"
> 3. 条目有头部（长度 + 是否宽字符）+ 数据

## 从外部解析

```cpp
std::string ReadNameFromProcess(MemReader &mem, uint64_t gnameBase,
                                 uint32_t nameId) {
    uint32_t blockIdx = nameId >> 16;
    uint32_t blockOff = nameId & 0xFFFF;

    uint64_t blockAddr = mem.Read<uint64_t>(gnameBase + 0x40 + blockIdx * 8);
    if (!blockAddr) return {};

    uint64_t entryAddr = blockAddr + blockOff * 2;
    uint16_t header = mem.Read<uint16_t>(entryAddr);
    if (header == 0) return {};

    uint32_t len    = header >> 6;
    bool     isWide = header & 1;
    if (len == 0 || len > 1024) return {};

    if (isWide) {
        std::vector<char32_t> buf(len);
        mem.Read(entryAddr + 2, buf.data(), len * 4);
        std::string out;
        for (char32_t c : buf) AppendUtf8(out, c);
        return out;
    } else {
        return mem.ReadString(entryAddr + 2, len);
    }
}
```

## 用途：按名字过滤

本项目用名字做两件事：

```cpp
// 1. 只处理角色
if (name_.find("BP_Character") == std::string::npos) {
    continue;
}

// 2. 区分人机和真人
const bool 是人机 = (name_.find("BP_Character_JiaRen") != std::string::npos);
```

**用名字而不是类型判断**，因为名字是稳定的字符串，
而类型信息（UClass 指针）会随版本变。

## 常见问题

| 问题 | 原因 | 解法 |
|---|---|---|
| 读出来是乱码 | 偏移 `0x40` 不对 | 用已知名字反推 |
| 长度异常巨大 | header 读错 | 加上界检查（已做） |
| 全是空字符串 | GName 地址错 | 用特征码定位（第 87 章） |
| 中文显示不对 | 宽字符没转 UTF-8 | 检查 `isWide` 分支 |

## 动手：名字表扫描器

写一个程序，遍历 demo 引擎的名字表，把所有名字打出来：

> [!note] 这段 main 用到本章前面定义的 `MemReader`、`ReadNameFromProcess()` 等，
> **需把它们拼在一起才能编译**。

```cpp
int main(int argc, char **argv) {
    pid_t pid = atoi(argv[1]);
    uint64_t gname = strtoull(argv[2], nullptr, 16);

    MemReader mem(pid);

    printf("遍历名字表:\n");
    for (uint32_t id = 0; id < 200; id++) {
        std::string n = ReadNameFromProcess(mem, gname, id);
        if (!n.empty() && isprint(n[0]))
            printf("  [%u] %s\n", id, n.c_str());
    }
    return 0;
}
```

对比 demo 引擎自己打印的名字列表——**应该完全一致**。

## 验收清单

- [ ] 理解为什么用"字符串表 + ID"（说出"省内存、比较快"）
- [ ] 知道 FName 的位布局（高 16 位块号、低 16 位偏移）（画出一个 FName 整数的位图）
- [ ] 知道 `header >> 6` 取长度、`& 1` 判宽字符（写出这两行并解释）
- [ ] 理解 `blockOff * 2` 的含义（说出每个 FNameEntry 头是 2 字节）
- [ ] 知道长度必须检查上界 ★（说出不检查会读越界）
- [ ] 会写 `AppendUtf8`（UTF-32 → UTF-8）（转换一个中文 FName，输出正确）
- [ ] 跑通了名字表扫描器，输出与真值一致（扫描出的名字与 demo 记录一致）

→ 下一章：[[第86章-变换组件与骨骼层级]]　—— 把第 48 章的骨骼数学接到真实数据上，并给 demo 引擎加上骨骼，完成端到端验证。
