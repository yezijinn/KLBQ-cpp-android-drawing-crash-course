---
tags: [教程, 深挖, 卷七, 逆向, 方法论]
aliases: [深挖G]
---

# 深挖 G · 逆向工程方法论进阶

> [!abstract] 这篇解决什么
> 第 87 章讲了六种找偏移的方法。这篇讲**怎么把它们工程化**：
> 特征码怎么设计才稳定、指令模式怎么自动匹配、指针链怎么自动发现、
> 结构体布局怎么从访问模式反推、多版本偏移怎么管理。
> **建议学完卷七后读。**

## 一、特征码（Signature）设计

### 什么是特征码

一段**在目标代码里唯一且稳定**的字节序列，用来定位某段代码。

```c
// 特征码示例（十六进制）
"48 8B 05 ?? ?? ?? ?? 48 85 C0 74 0A"
```

`??` 表示"通配"——这部分会随版本变化（如地址偏移、立即数）。

### 设计原则

| 原则 | 说明 | 反例 |
|---|---|---|
| **唯一性** | 在整个模块里只出现一次 | 只写 `48 8B` 会匹配几万处 |
| **稳定性** | 不随版本更新变化 | 包含具体偏移的字节会变 |
| **足够长** | 至少 8-16 字节 | 太短容易撞 |
| **避开相对地址** | 相对跳转/调用的偏移会变 | 通配掉 |
| **避开立即数** | 具体数值会变 | 通配掉 |
| **优先选序言** | 函数开头的指令模式更稳定 | — |

### 怎么选出一段好特征码

**步骤**：

```
1. 反汇编目标函数
   objdump -d libfoo.so | grep -A30 '<target_func>'

2. 找"指令模式独特"的位置
   例如：三个连续的 movz/movk 构造一个大常量（通常是魔数）

3. 把易变部分通配
   - 相对跳转的目标 → ??
   - 立即数 → ??
   - 地址的高低位 → ??

4. 验证唯一性
   在整个模块里搜索，确认只有一处命中

5. 跨版本验证（如果有多个版本）
   在新旧版本里都能命中的才是好特征码
```

### 实战：给 demo 引擎写一个特征码

假设 demo 引擎里有个函数：

```cpp
uint64_t GetWorldPtr_Internal() {
    return (uint64_t)g_world;      // 编译器会生成 adrp + ldr
}
```

反汇编：

```asm
GetWorldPtr_Internal:
    adrp    x0, 0x1000          ; 取 g_world 所在页
    ldr     x0, [x0, #0x120]    ; 加页内偏移并读出
    ret
```

特征码可以这样设计：

```
?? ?? ?? ??   ; adrp（相对偏移会变，通配）
?? ?? ?? ??   ; ldr 的立即数部分会变，通配
C0 03 5F D6   ; ret（固定）
```

但这样太短。改进：加上前后的指令：

```
FD 7B BF A9   ; stp x29, x30, [sp, #-16]!  （序言，稳定）
FD 03 00 91   ; mov x29, sp
?? ?? ?? ??   ; adrp
F4 ?? ?? F9   ; ldr（部分通配）
FD 7B C1 A8   ; ldp x29, x30, [sp], #16
C0 03 5F D6   ; ret
```

**长度 20+ 字节，唯一性高，且通配了所有易变部分。**

### 特征码扫描的实现

```cpp
struct SigByte {
    uint8_t value;
    bool    wildcard;
};

// 从字符串解析特征码："48 8B ?? ?? C0"
std::vector<SigByte> ParseSig(const char *s);

uint64_t ScanSignature(const uint8_t *data, size_t size,
                       const std::vector<SigByte> &sig) {
    if (sig.empty() || size < sig.size()) return 0;

    for (size_t i = 0; i + sig.size() <= size; i++) {
        bool match = true;
        for (size_t j = 0; j < sig.size(); j++) {
            if (!sig[j].wildcard && data[i + j] != sig[j].value) {
                match = false;
                break;
            }
        }
        if (match) return i;
    }
    return 0;
}
```

**性能优化**：

```cpp
// 用第一个非通配字节做快速预筛
uint8_t firstByte = 0; size_t firstIdx = 0;
for (size_t j = 0; j < sig.size(); j++) {
    if (!sig[j].wildcard) { firstByte = sig[j].value; firstIdx = j; break; }
}

for (size_t i = 0; i + sig.size() <= size + firstIdx; i++) {
    if (data[i + firstIdx] != firstByte) continue;     // ★ 快速跳过
    /* 完整比对 */
}
```

这一优化能提速 10-50 倍。

### 从特征码到地址

```cpp
// 特征码命中位置 → 解析指令 → 得到目标地址
uint64_t ResolveWorldPtr(const uint8_t *code, size_t hitOffset,
                         uint64_t moduleBase) {
    // 假设特征码里包含 adrp + ldr 两条指令
    // adrp 编码：1xx10000 ... 
    // ldr  编码：11111001 01xxxxxx ...

    uint32_t adrpInst, ldrInst;
    memcpy(&adrpInst, code + hitOffset + kAdrpOffset, 4);
    memcpy(&ldrInst,  code + hitOffset + kLdrOffset,  4);

    // 解析 adrp：取页偏移（符号扩展 21 位）
    int64_t immlo = (adrpInst >> 29) & 0x3;
    int64_t immhi = (adrpInst >> 5)  & 0x7FFFF;
    int64_t imm   = ((immhi << 2) | immlo);
    if (imm & (1 << 20)) imm -= (1 << 21);         // 符号扩展

    uint64_t adrpTarget = (moduleBase + kAdrpOffset) & ~0xFFFULL;
    adrpTarget += imm << 12;

    // 解析 ldr：取页内偏移（12 位无符号）
    uint64_t ldrImm = (ldrInst >> 10) & 0xFFF;      // 简化，实际要看变体

    return adrpTarget + ldrImm * 8;                 // 如果是 64 位加载
}
```

**这就是自动化定位的核心。** 具体位域要对照 ARM 手册，
但思路是"特征码定位指令 → 解析指令编码 → 算出目标地址"。

> [!note] 更简单的替代方案
> 如果特征码能直接覆盖到"读取指令"，也可以：
> 1. 特征码定位到函数入口
> 2. 反汇编该函数的前 N 条指令
> 3. 在指令序列里找 `adrp` + `add/ldr` 对
> 4. 解析出地址
>
> 这样不用手工算位域，但需要一个小型反汇编器
> （或者只识别这两种指令的编码）。

## 二、指针链自动发现

### 问题

已知一个动态地址（比如某个对象的地址），
要找一条 `[模块基址 + 偏移] → +偏移 → ... → 目标地址` 的链。

### BFS 算法

```
1. 目标地址 = T
2. 扫描整个内存，找出所有"值 = T"的位置 → 得到一批指针 P1
   （即"谁指向 T"）
3. 对每个 Pi，再找"谁指向 Pi" → P2
4. 重复到找到"模块基址 + 常量偏移"为止
```

```cpp
struct ChainNode {
    uint64_t addr;         // 这个指针在哪
    uint64_t value;        // 它指向谁
    uint64_t base;         // 如果它落在某个模块内，记录模块基址
    uint64_t offset;       // 相对模块基址的偏移
    int      depth;
};

std::vector<ChainNode> FindPointersTo(
        const std::vector<std::pair<uint64_t,uint64_t>> &regions,
        uint64_t target, int maxResults = 10000) {

    std::vector<ChainNode> result;
    constexpr size_t CHUNK = 1 << 20;
    std::vector<uint8_t> buf(CHUNK);

    for (auto [start, end] : regions) {
        for (uint64_t addr = start; addr < end && (int)result.size() < maxResults;
             addr += CHUNK) {
            size_t len = std::min<size_t>(CHUNK, end - addr);
            if (ReadRaw(addr, buf.data(), len) <= 0) continue;

            // 按指针大小步进扫描
            for (size_t off = 0; off + 8 <= len; off += 8) {
                uint64_t v;
                memcpy(&v, buf.data() + off, 8);
                if (v == target) {
                    result.push_back({addr + off, v, 0, 0, 0});
                }
            }
        }
    }
    return result;
}
```

**优化技巧**：

| 技巧 | 效果 |
|---|---|
| 按 8 字节对齐扫描 | 减少 7/8 的比较 |
| 只扫可读写区域 | 排除代码段 |
| 只扫前 47 位有效范围 | 排除明显不是指针的值 |
| 用 SIMD 比较 | 一次比 2-4 个 |
| 限制结果数 | 避免内存爆炸 |

### 递归构建链

```cpp
void BuildChains(const std::vector<std::pair<uint64_t,uint64_t>> &regions,
                 const std::vector<uint64_t> &moduleRanges,   // [(base,end)]
                 uint64_t target, int maxDepth,
                 std::vector<std::vector<uint64_t>> &outChains,
                 std::vector<uint64_t> currentChain = {}) {

    if ((int)currentChain.size() > maxDepth) return;

    // 检查目标是否落在某个模块的静态偏移上（= 找到了根）
    for (auto [base, end] : moduleRanges) {
        if (target >= base && target < end) {
            auto chain = currentChain;
            chain.push_back(target - base);        // 记下偏移
            std::reverse(chain.begin(), chain.end());
            outChains.push_back(chain);            // 找到一条完整链
            return;
        }
    }

    // 否则继续往上找
    auto pointers = FindPointersTo(regions, target, 200);
    for (auto &p : pointers) {
        // 只取"看起来在数据区"的指针
        if (!IsDataRegion(p.addr)) continue;

        auto nextChain = currentChain;
        nextChain.push_back(target);
        BuildChains(regions, moduleRanges, p.addr, maxDepth, outChains, nextChain);
        if (outChains.size() >= 50) return;        // 限制数量
    }
}
```

**结果过滤**（重要）：

```
1. 链长度不超过 5 层（太长的链不稳定）
2. 根偏移是合理值（通常是 0x100000 ~ 0x4000000 范围）
3. 重启目标进程后验证：**只有仍有效的链才是真的**
4. 优先选"偏移是 8 的倍数"的链（结构体对齐）
```

**第 3 步是关键**——重启后基址变了，只有真正的静态链还能工作。

### 重启验证的实现

```cpp
std::vector<Chain> ValidateChains(pid_t newPid, uint64_t newBase,
                                  const std::vector<Chain> &candidates,
                                  uint64_t expectedValue) {
    std::vector<Chain> good;
    SafeReader mem(newPid);

    for (auto &c : candidates) {
        uint64_t cur = newBase + c[0];
        for (size_t i = 1; i < c.size(); i++) {
            cur = mem.ReadChecked<uint64_t>(cur + c[i], 0);
            if (!SafeReader::IsValidPtr(cur)) { cur = 0; break; }
        }
        if (cur == expectedValue) good.push_back(c);
    }
    return good;
}
```

## 三、从访问模式反推结构体布局

### 思路

如果能看到某段代码访问 `[base + 0x18]`、`[base + 0x20]`、`[base + 0x2A0]`，
就能推断出结构体的字段偏移。

### 实现：收集访问偏移

```cpp
// 在一段反汇编里找所有 "基址 + 立即数" 的访问模式
std::set<int64_t> CollectOffsets(const std::vector<Instruction> &insts) {
    std::set<int64_t> offsets;

    for (const auto &ins : insts) {
        // ldr/str xN, [xM, #imm]
        if ((ins.op == Op::LDR || ins.op == Op::STR) && ins.hasImmOffset) {
            offsets.insert(ins.immOffset);
        }
        // add xN, xM, #imm  （准备访问）
        if (ins.op == Op::ADD && ins.hasImm) {
            offsets.insert(ins.imm);
        }
    }
    return offsets;
}
```

### 从偏移推断类型

| 特征 | 推断 |
|---|---|
| 偏移差 4，且后续有 `ldr w` | 4 字节字段（int/uint32/float） |
| 偏移差 8，`ldr x` | 8 字节字段（指针/int64） |
| 偏移差 0xC（12） | 可能是 3 个 float（Vec3/Rotator） |
| 偏移差 0x10（16） | 4 个 float（Quat）或 2 个指针 |
| 偏移差较大（0x100+） | 内嵌子结构体或填充 |

### 实战：验证已知结构体

```cpp
// 已知 Actor 结构，用访问模式验证
struct CandidateActor {
    uint64_t vtable;      // 0x00
    uint32_t objectId;    // 0x08
    uint32_t nameId;      // 0x0C
    uint64_t components;  // 0x10
    // ...
};

// 检查这些偏移是否都出现在访问模式里
std::set<int64_t> expected = {0, 8, 12, 0x10, 0x18, 0x168, 0x370};
std::set<int64_t> actual = CollectOffsets(funcInsts);

std::vector<int64_t> missing;
for (auto off : expected)
    if (!actual.count(off)) missing.push_back(off);

if (!missing.empty()) {
    printf("这些偏移从未被访问，可能不存在或已变化：\n");
    for (auto m : missing) printf("  +0x%llX\n", (unsigned long long)m);
}
```

**这个方法的价值**：找偏移时能验证"我猜的结构体是否与代码的实际访问一致"。

## 四、多版本偏移管理

### 版本识别的三种方法

| 方法 | 实现 | 可靠性 |
|---|---|---|
| 文件哈希 | 读库文件算 MD5 | 最高（但文件可能读不到） |
| 版本字符串 | 读游戏版本号 | 高 |
| **指纹自检** | 用某组偏移做一致性检查 | 中（最实用） |

### 指纹自检设计

```cpp
struct OffsetSet {
    const char *name;
    Offsets     offsets;
    bool (*validate)(IDriver *dr, uint64_t base);   // 验证函数
};

bool ValidateV1(IDriver *dr, uint64_t base) {
    using O = OffsetsV1;
    uint64_t world = dr->Read<uint64_t>(base + O::UWORLD);
    if (world < 0x10000000ULL || world > 0x10000000000ULL) return false;

    uint64_t level = dr->Read<uint64_t>(world + O::UWORLD_LEVEL);
    if (level < 0x10000000ULL || level > 0x10000000000ULL) return false;

    int32_t count = dr->Read<int32_t>(level + O::ULEVEL_ACTORCOUNT);
    return count > 0 && count < 10000;
}

// 依次尝试
const Offsets *DetectOffsets(IDriver *dr, uint64_t base) {
    static const OffsetSet sets[] = {
        {"v1.2.0", OffsetsV1::data(), ValidateV1},
        {"v1.1.0", OffsetsV2::data(), ValidateV2},
        {"v1.0.0", OffsetsV3::data(), ValidateV3},
    };

    for (const auto &s : sets) {
        if (s.validate(dr, base)) {
            LOGI("偏移集匹配: %s", s.name);
            return &s.offsets;
        }
    }
    LOGE("没有匹配的偏移集，可能游戏已更新");
    return nullptr;
}
```

**核心思想**：**"用偏移验证偏移"**——如果一组偏移能让整条链走通且值合理，
那它大概是对的。

### 版本记录表模板

```markdown
# 偏移集版本记录

## v1.2.0 · 2026-11-02
- 触发：游戏更新
- 变化：`ACTOR_MESH` 0x370 → 0x378；新增 `ACTOR_TEAM` 0x2A0
- 定位方法：组件链表遍历 + 名字表交叉验证
- 验证：ESP 方框贴合、骨骼正确、遮挡判定通过
- 指纹：`UWorld + 0x30` 可解出 `Count ∈ [1, 10000]`

## v1.1.0 · 2026-09-14
- 初始版本
```

## 五、完整验证体系

一个成熟的偏移体系需要四层验证：

```
① 启动自检      —— 进程启动时跑一次，失败就明确提示
② 运行时统计    —— PtrValidator 的拒绝原因计数
③ 周期性交叉验证 —— 用两条独立路径得到同一个值，比对
④ 用户可见状态  —— UI 上显示"偏移有效/失效"
```

### 启动自检的完整实现

```cpp
enum class OffsetStatus {
    Ok,
    NoProcess,
    BadBase,
    BadWorld,
    BadLevel,
    BadCount,
    BadCamera,
    BadName,
};

OffsetStatus FullSelfTest(IDriver *dr, uint64_t base) {
    // 1. 进程
    if (dr->GetGlobalPid() <= 0) return OffsetStatus::NoProcess;

    // 2. 基址
    if (base < 0x10000000ULL || base > 0x10000000000ULL)
        return OffsetStatus::BadBase;

    // 3. UWorld
    uint64_t world = dr->Read<uint64_t>(base + O::UWORLD);
    if (!IsValidPtr(world)) return OffsetStatus::BadWorld;

    // 4. ULevel
    uint64_t level = dr->Read<uint64_t>(world + O::UWORLD_LEVEL);
    if (!IsValidPtr(level)) return OffsetStatus::BadLevel;

    // 5. Actor 数量
    int32_t count = dr->Read<int32_t>(level + O::ULEVEL_ACTORCOUNT);
    if (count <= 0 || count > 10000) return OffsetStatus::BadCount;

    // 6. 相机
    uint64_t pc = /* ... */;
    float fov = dr->Read<float>(pc + O::PC_CAMERAMANAGER + O::CAM_CACHE_FOV);
    if (fov < 1.0f || fov > 179.0f) return OffsetStatus::BadCamera;

    // 7. 名字表（能解析出至少一个名字）
    if (GetNameById(base + O::GNAME_POOL, 1).empty())
        return OffsetStatus::BadName;

    return OffsetStatus::Ok;
}

const char *StatusText(OffsetStatus s) {
    switch (s) {
    case OffsetStatus::Ok:         return "偏移有效";
    case OffsetStatus::NoProcess:  return "目标进程未运行";
    case OffsetStatus::BadBase:    return "模块基址异常";
    case OffsetStatus::BadWorld:   return "UWorld 读取失败";
    case OffsetStatus::BadLevel:   return "ULevel 读取失败";
    case OffsetStatus::BadCount:   return "Actor 数量异常";
    case OffsetStatus::BadCamera:  return "相机数据异常";
    case OffsetStatus::BadName:    return "名字表解析失败";
    }
    return "未知";
}
```

**UI 上直接显示状态文本**——用户看到"UWorld 读取失败"就知道是游戏更新了。

## 六、把这篇用在项目里

| 方法 | 项目中的用途 |
|---|---|
| 特征码 | 自动定位 `GName` / `UWorld` 静态指针 |
| 指令解析 | 从 `adrp+add` 解出目标地址 |
| 指针链扫描 | 首次建立偏移时用 |
| 访问模式分析 | 验证结构体布局是否符合预期 |
| 多版本指纹 | 一套程序适配多个游戏版本 |
| 四层验证 | 保证失效时能明确报错而不是静默出错 |

> [!warning] 边界说明
> 这篇讲的方法是**通用的逆向工程技能**，适用于：
> 调试自己的程序、分析授权/开源软件、CTF、安全研究。
>
> 本教程的所有练习都在**自己写的 demo 引擎**上。
> 把它用于未授权的第三方程序，可能违反用户协议与法律——
> 技术是中立的，用法不是。

> [!tip] 一个可以立刻做的练习
> 给第 83 章的 demo 引擎加一组"魔数"：
> ```cpp
> static const uint32_t kMagic1 = 0xDEADBEEF;
> static const uint64_t kMagic2 = 0x123456789ABCDEF0ULL;
> ```
> 然后写一个扫描器，通过这两个魔数定位 `g_world`——
> 完整走一遍"特征码 → 定位 → 验证"的流程。

→ 返回 [[卷七-本卷导航]]
