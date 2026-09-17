---
tags: [教程, 卷六, ELF, 符号]
day: 50
aliases: [ch77]
---

# 第 77 章 · ELF 解析找符号

> [!abstract] 本章目标
> 从模块基址出发，解析内存里的 ELF 结构，按名字找到函数/变量的地址。
> 这是"不用硬编码偏移"的另一条路。

> [!note] 承上
> 上一章靠 maps 找到了"模块从哪开始"。
> 本章往下走一层：**从模块里解析 ELF 符号表**，按名字找函数/变量地址。

## 先看为什么有用

硬编码偏移：

```cpp
uint64_t Uworld = Read<uint64_t>(libUE4 + 0xB3EC650);
```

游戏一更新，偏移就失效。

符号解析：

```cpp
uint64_t addr = FindSymbol("UWorld");      // 名字不会变
```

**但前提是：目标库必须有符号表（没被 strip）。**

| | 硬编码偏移 | 符号解析 |
|---|---|---|
| 抗更新 | ✗ 差 | ✓ 好 |
| 适用条件 | 总能用 | **需要符号表** |
| 实现难度 | 简单 | 中等 |
| 速度 | 快（一次读） | 慢（要遍历符号表） |

实战中通常是**混合**：启动时解析一次符号，缓存地址，之后当偏移用。

## 内存里的 ELF 布局

模块被加载后，ELF 结构在内存里：

```
基址 base
  ├── ELF Header（在 base + 0）
  ├── Program Headers（在 base + e_phoff）
  ├── .text（代码）
  ├── .rodata
  ├── .dynsym（动态符号表）★
  ├── .dynstr（符号名字符串表）★
  └── ...
```

**关键**：`e_phoff` 等字段是**相对文件开头的偏移**，
但因为 ELF 头在加载后的 `base + 0`，
所以 `base + e_phoff` 就是程序头表在内存里的位置。

（严格说要用程序头表把"虚拟地址"和"文件偏移"对应起来，
但对可执行段和常规布局，`base + vaddr` 通常成立。）

## 解析步骤

```
1. 读 ELF Header → 拿 e_phoff, e_phnum, e_phentsize
2. 遍历 Program Headers → 找 PT_DYNAMIC (p_type == 2)
3. 读 PT_DYNAMIC 段 → 是一堆 Elf64_Dyn 键值对
4. 从 Dyn 里取：
     DT_SYMTAB (6)  → 符号表地址
     DT_STRTAB (5)  → 字符串表地址
     DT_SYMENT (11) → 每个符号表项大小
     DT_HASH (4) 或 DT_GNU_HASH (0x6ffffef5) → 哈希表（可选，加速用）
5. 遍历符号表，比较名字 → 找到 st_value（相对基址的偏移）
6. 真实地址 = base + st_value
```

## 代码实现

```cpp
#include <elf.h>

struct ModuleInfo {
    uint64_t base = 0;
    uint64_t symtab = 0;      // 符号表地址
    uint64_t strtab = 0;      // 字符串表地址
    size_t   syment = 0;      // 每项大小
    size_t   symcount = 0;    // 符号个数（从 hash 表推或估算）
};

bool ParseDynamic(MemReader &mem, uint64_t base, ModuleInfo &out) {
    // 1. ELF 头
    Elf64_Ehdr ehdr;
    if (mem.Read(base, &ehdr, sizeof(ehdr)) <= 0) return false;

    // 检查魔数
    if (memcmp(ehdr.e_ident, "\x7f" "ELF", 4) != 0) return false;
    if (ehdr.e_ident[EI_CLASS] != ELFCLASS64) return false;

    out.base = base;

    // 2. 遍历程序头找 PT_DYNAMIC
    uint64_t phoff = base + ehdr.e_phoff;
    uint64_t dynAddr = 0, dynSize = 0;

    for (int i = 0; i < ehdr.e_phnum; i++) {
        Elf64_Phdr phdr;
        if (mem.Read(phoff + i * ehdr.e_phentsize, &phdr, sizeof(phdr)) <= 0)
            break;
        if (phdr.p_type == PT_DYNAMIC) {
            dynAddr = base + phdr.p_vaddr;
            dynSize = phdr.p_memsz;
            break;
        }
    }
    if (dynAddr == 0) return false;

    // 3. 读 dynamic 段
    std::vector<Elf64_Dyn> dyns(dynSize / sizeof(Elf64_Dyn));
    if (mem.Read(dynAddr, dyns.data(), dynSize) <= 0) return false;

    uint64_t hashAddr = 0;
    for (const auto &d : dyns) {
        switch (d.d_tag) {
        case DT_SYMTAB: out.symtab = base + d.d_un.d_ptr; break;
        case DT_STRTAB: out.strtab = base + d.d_un.d_ptr; break;
        case DT_SYMENT: out.syment = d.d_un.d_val;        break;
        case DT_HASH:   hashAddr   = base + d.d_un.d_ptr; break;
        case DT_NULL:   goto done;
        default: break;
        }
    }
done:
    if (!out.symtab || !out.strtab) return false;
    if (out.syment == 0) out.syment = sizeof(Elf64_Sym);

    // 4. 从 hash 表拿符号个数（DT_HASH 的第 1 个 uint32 是 nchain = 符号数）
    if (hashAddr) {
        uint32_t nbucket = 0, nchain = 0;
        mem.Read(hashAddr + 0, &nbucket, 4);
        mem.Read(hashAddr + 4, &nchain, 4);
        out.symcount = nchain;
    }
    return true;
}
```

## 查找符号

```cpp
uint64_t FindSymbol(MemReader &mem, const ModuleInfo &mod,
                    const char *name) {
    if (!mod.symtab || !mod.strtab || mod.symcount == 0) return 0;

    for (size_t i = 0; i < mod.symcount; i++) {
        Elf64_Sym sym;
        if (mem.Read(mod.symtab + i * mod.syment, &sym, sizeof(sym)) <= 0)
            break;

        // 跳过未定义符号
        if (sym.st_shndx == SHN_UNDEF) continue;

        // 读名字
        std::string symName = mem.ReadString(mod.strtab + sym.st_name, 128);
        if (symName.empty()) continue;

        if (symName == name) {
            return mod.base + sym.st_value;      // ★ 真实地址
        }
    }
    return 0;
}
```

> [!warning] 逐个读符号很慢
> 一个库有 2000 个符号 = 2000 次内存读取 = 几毫秒到几十毫秒。
> 而且每个符号要读两次（符号项 + 名字字符串）。
>
> **优化**：
> 1. 批量读整个符号表（一次 readv）
> 2. 批量读整个字符串表
> 3. 然后在本地内存里比较（不跨进程）
>
> ```cpp
> std::vector<Elf64_Sym> syms(mod.symcount);
> mem.Read(mod.symtab, syms.data(), mod.symcount * mod.syment);
> std::vector<char> strs(strSize);
> mem.Read(mod.strtab, strs.data(), strSize);
> // 本地遍历
> ```
> 从 2000 次系统调用降到 2 次。这就是第 81 章的原则。

## GNU Hash vs SysV Hash

现代库多用 `DT_GNU_HASH`（更快，但**不能**直接得到符号总数）：

```cpp
// GNU hash 布局
struct {
    uint32_t nbuckets;
    uint32_t symoffset;     // 前 symoffset 个符号不在 hash 里
    uint32_t bloom_size;
    uint32_t bloom_shift;
    uint64_t bloom[bloom_size];
    uint32_t buckets[nbuckets];
    uint32_t chain[];       // 从 symoffset 开始
};
```

符号总数要从 `chain` 数组推算：

```cpp
uint32_t maxIdx = 0;
for (uint32_t i = 0; i < nbuckets; i++)
    if (buckets[i] > maxIdx) maxIdx = buckets[i];
// 沿 chain 走到 st_value & 1 的位置
```

本项目没有走这条路（用的是硬编码偏移），
但知道它的存在——**真要做通用符号解析必须处理 GNU hash**。

## 另一种思路：读 .dynsym 的边界

从节头表（Section Header）拿 `.dynsym` 的大小：

```
.dynsym 的 sh_size / sizeof(Elf64_Sym) = 符号个数
```

但：**strip 后节头表没了**（第 19 章）。
所以运行时只能靠 hash 表推算。

## 实战：为什么本项目不用符号解析

原因很实际：

| 原因 | 说明 |
|---|---|
| 游戏库被 strip | 没有 `.symtab`，只剩 `.dynsym` |
| `.dynsym` 只有导出符号 | `UWorld` 这种全局变量通常**不在**里面 |
| 每帧都要用 | 解析太慢，不如硬编码 |
| 偏移确实会变 | 但项目维护者能快速重新定位 |

**结论**：对游戏这种"内部数据"，符号解析用处有限，
主要还是靠特征码扫描（第 87 章）+ 手工定位。

但对于**系统库**（`libc.so`、`libvulkan.so`），
符号解析非常有用——因为它们保留了完整的 `.dynsym`。

## 动手：符号查找器

> [!warning] 这段 main 是**片段**，用到本章前面定义的 `MemReader`、`LoadMaps()`、
> `FindModuleBase()`、`ParseDynamic()`、`FindSymbol()`——**不能单独编译**。
> 要跑通，把前面几节的实现拼在一起（或参考附录 I / 附录 E 的整合版）。

```cpp
int main(int argc, char **argv) {
    if (argc < 4) {
        printf("用法: %s <pid> <模块名> <符号名>\n", argv[0]);
        return 1;
    }
    pid_t pid = atoi(argv[1]);

    MemReader mem(pid);

    // 1. 找模块基址
    auto maps = LoadMaps(pid);
    uint64_t base = FindModuleBase(maps, argv[2]);
    if (!base) { printf("未找到模块 %s\n", argv[2]); return 1; }
    printf("模块 %s 基址 = 0x%llX\n", argv[2], (unsigned long long)base);

    // 2. 解析 dynamic
    ModuleInfo mod;
    if (!ParseDynamic(mem, base, mod)) {
        printf("解析 dynamic 失败（可能已 strip 或无 PT_DYNAMIC）\n");
        return 1;
    }
    printf("符号表 = 0x%llX  字符串表 = 0x%llX  符号数 = %zu\n",
           (unsigned long long)mod.symtab,
           (unsigned long long)mod.strtab, mod.symcount);

    // 3. 查找
    uint64_t addr = FindSymbol(mem, mod, argv[3]);
    if (addr)
        printf("%s → 0x%llX (偏移 0x%llX)\n", argv[3],
               (unsigned long long)addr, (unsigned long long)(addr - base));
    else
        printf("未找到符号 %s\n", argv[3]);
    return 0;
}
```

测试（用系统库，它们有符号）：

```bash
# 找 libc 里的 printf
./symfind <pid> libc.so printf

# 找 libEGL 里的某个函数
./symfind <pid> libEGL.so eglGetDisplay
```

如果能找到，说明解析正确。再试试找一个游戏库的内部符号——大概率找不到，
**这正好验证了"strip 后符号解析失效"这一结论**。

## 动手验证清单

- [ ] **找 PT_DYNAMIC**：遍历程序头，找到 `p_type == PT_DYNAMIC` 的段
- [ ] **解析 Dyn 数组**：取 `DT_SYMTAB`（符号表）、`DT_STRTAB`（字符串表）地址
- [ ] **遍历符号表**：逐个读 `Elf64_Sym`，用 `st_name` 索引字符串表比对名字
- [ ] **算真实地址**：`真实地址 = 模块基址 + st_value`
- [ ] **找 libc 符号**：`./symfind <pid> libc.so printf` → 查到地址
- [ ] **验证 strip 影响**：对 strip 过的库 → 内部符号查不到（只剩 `.dynsym`）

> [!warning] strip 后只剩动态符号
> 发布版本通常 strip 过，`nm` 显示 `no symbols`。此时只能查 `.dynsym`（导出符号），
> 内部全局变量通常不在里面（第 19、77 章）。

## 验收清单

- [ ] 知道解析五步：ELF头 → PT_DYNAMIC → Dyn 键值对 → 符号表/字符串表 → 遍历（复述五步）
- [ ] 会读 `DT_SYMTAB` / `DT_STRTAB` / `DT_SYMENT`（从 PT_DYNAMIC 读出这三项）
- [ ] 知道真实地址 = base + st_value（说出公式）
- [ ] 知道单个读慢，要批量读符号表和字符串表（说出为什么）
- [ ] 知道 strip 后只剩 `.dynsym`，且内部全局变量通常不在里面（说出为什么导不出）
- [ ] 知道 GNU Hash 与 SysV Hash 的区别（说出 GNU Hash 查找更快）
- [ ] 跑通了符号查找器，用系统库验证成功（查到 libc 里某符号的地址）

→ 下一章：[[第78章-内核模块与用户态通信-概念]]　—— 理解内核模块是什么、它能做什么、以及用户态怎么跟它说话。
