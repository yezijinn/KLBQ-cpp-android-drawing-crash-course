---
tags: [教程, 卷二, 二进制, ELF]
day: 12
aliases: [ch19]
---

# 第 19 章 · ELF 文件格式

> [!abstract] 本章目标
> 能自己读 ELF 头和程序头表，拿到 `e_entry`、段权限、以及动态符号表的位置。
> 这是第 77 章"从模块基址找函数地址"的直接前置。

> [!note] 承上
> 前两章讲"进程"和"内存"。
> 本章讲**可执行文件的格式**——ELF。它是第 20 章（动态链接）、第 30 章（二进制工具）、第 77 章（符号解析）的共同基础。

> [!note] `xxd` 是什么
> `xxd` 是一个"**十六进制查看器**"——把文件的字节按十六进制+ASCII 两栏打印出来。
> ```bash
> xxd -l 64 /bin/ls     # -l 64 表示"只看前 64 字节"
> ```
> 左边是偏移，中间是十六进制字节，右边是 ASCII 字符（不可打印的显示为 `.`）。
> **看二进制文件（ELF、图片、任意数据）都用它。** Windows 上没有 `xxd`，可用 `certutil -dump` 或 WSL。

## 先看一个文件的开头 64 字节

```bash
xxd -l 64 /bin/ls
```

```
00000000: 7f45 4c46 0201 0100 0000 0000 0000 0000  .ELF............
00000010: 0300 b700 0100 0000 4011 4000 0000 0000  ........@.@.....
00000020: 4000 0000 0000 0000 ...
```

第一行的 `7f 45 4c 46` 就是魔数 `\x7fELF`。**所有 Linux/Android 的可执行文件、
动态库、`.o` 文件，都是这个开头。**

逐字节解读：

| 偏移 | 字节 | 含义 |
|---|---|---|
| 0x00 | `7f 45 4c 46` | 魔数 `\x7f E L F` |
| 0x04 | `02` | EI_CLASS：1=32位，2=64位 → **64 位** |
| 0x05 | `01` | EI_DATA：1=小端，2=大端 → **小端** |
| 0x06 | `01` | EI_VERSION：版本，恒为 1 |
| 0x07 | `00` | EI_OSABI：0=SysV，3=Linux |
| 0x10 | `03 00` | e_type：1=REL(.o)，2=EXEC，**3=DYN(.so)** |
| 0x12 | `b7 00` | e_machine：**0xB7 = AArch64**（0x3C=x86_64, 0x03=x86） |
| 0x18 | `4011 4000...` | e_entry：程序入口地址 |

> [!tip] 常量速记
> ```
> 0xB7 = AArch64    0x3C = x86_64    0x03 = i386    0x28 = ARM
> e_type: 1=REL  2=EXEC  3=DYN  4=CORE
> ```

## ELF 的两张表

ELF 文件有两个视角：

```
┌─────────────────────────────────────┐
│  ELF Header                          │  ← 文件开头，描述两张表在哪
├─────────────────────────────────────┤
│  Program Header Table (段表)          │  ← 给"加载器"看：怎么把文件映射进内存
│   PT_LOAD / PT_DYNAMIC / ...         │
├─────────────────────────────────────┤
│  段内容 (.text .data ...)             │
├─────────────────────────────────────┤
│  Section Header Table (节表)          │  ← 给"链接器/调试器"看：符号在哪
│   .symtab .strtab .dynsym ...        │
└─────────────────────────────────────┘
```

| 表 | 给谁看 | 关键结构 | 运行时需要吗 |
|---|---|---|---|
| 程序头表（Program Header） | 内核加载器 | `PT_LOAD` 段 | **必须** |
| 节头表（Section Header） | 链接器、调试器 | `.symtab` `.text` | 可删（strip 就是删它） |

**运行时只需要程序头表。** 这就是为什么 strip 之后程序照样能跑。

## ELF Header 结构（64 位）

```c
#include <elf.h>     // 系统自带，直接用

typedef struct {
    unsigned char e_ident[16];   // 魔数 + 各种标识
    uint16_t e_type;             // 文件类型
    uint16_t e_machine;          // 架构
    uint32_t e_version;
    uint64_t e_entry;            // 入口虚拟地址
    uint64_t e_phoff;            // 程序头表在文件中的偏移
    uint64_t e_shoff;            // 节头表在文件中的偏移
    uint32_t e_flags;
    uint16_t e_ehsize;           // ELF 头大小（64 字节）
    uint16_t e_phentsize;        // 每个程序头多大（56 字节）
    uint16_t e_phnum;            // 程序头数量
    uint16_t e_shentsize;
    uint16_t e_shnum;            // 节头数量
    uint16_t e_shstrndx;         // 节名字符串表索引
} Elf64_Ehdr;
```

## Program Header（段的描述）

```c
typedef struct {
    uint32_t p_type;      // 段类型
    uint32_t p_flags;     // 权限：1=X 2=W 4=R
    uint64_t p_offset;    // 在文件中的偏移
    uint64_t p_vaddr;     // 应该映射到哪个虚拟地址（相对基址）
    uint64_t p_paddr;     // 物理地址（用户态不用）
    uint64_t p_filesz;    // 文件里占多少字节
    uint64_t p_memsz;     // 内存里占多少字节
    uint64_t p_align;     // 对齐
} Elf64_Phdr;
```

| `p_type` | 值 | 含义 |
|---|---|---|
| `PT_NULL` | 0 | 忽略 |
| `PT_LOAD` | 1 | **需要加载进内存的段**（最重要） |
| `PT_DYNAMIC` | 2 | 动态链接信息 |
| `PT_INTERP` | 3 | 解释器路径（如 `/system/bin/linker64`） |
| `PT_NOTE` | 4 | 附加信息 |

> [!note] `p_memsz > p_filesz` 的情况
> `.bss` 段在文件里不占空间（全是 0，不必存），但内存里要占位。
> 所以 `p_memsz` 比 `p_filesz` 大的那部分，加载器要**补零**。

> [!note] 这段代码用到的几个新语法，先集中说明
> 下面的解析程序用到几个卷一没讲过的东西，这里一次讲清：
>
> **① `main(int argc, char **argv)` —— 命令行参数**
> 你的程序可以接受"启动时传进来的参数"。`argc` 是参数个数，`argv` 是参数字符串数组：
> ```c
> int main(int argc, char **argv) {
>     // 运行 ./elfhead /bin/ls
>     // argc = 2
>     // argv[0] = "./elfhead"（程序自己的名字）
>     // argv[1] = "/bin/ls"（第一个真正的参数）
> }
> ```
> **`argv[0]` 永远是程序名**，真正的参数从 `argv[1]` 开始。
>
> **② `switch` 语句 —— 多分支选择**
> 当"要比较的是同一个值、有很多种情况"时，`switch` 比一串 `if/else if` 更清晰：
> ```c
> switch (e_type) {
>     case ET_REL:  printf("可重定位"); break;   // 等于 ET_REL 时走这里
>     case ET_EXEC: printf("可执行");   break;
>     default:      printf("其它");     break;   // 都不匹配时
> }
> ```
> 规则：每个 `case` 末尾要 `break`（否则会"贯穿"到下一个 case）；`default` 处理"其它情况"。
>
> **③ `fseek(fp, 偏移, 起点)` —— 移动文件读写位置**
> ```c
> fseek(fp, eh.e_phoff, SEEK_SET);   // 从文件开头（SEEK_SET）跳到偏移 e_phoff 处
> ```
> 起点可以是 `SEEK_SET`（开头）、`SEEK_CUR`（当前位置）、`SEEK_END`（末尾）。第 11 章"求文件大小"用过它。
>
> **④ `perror("xxx")` —— 打印错误信息**
> ```c
> if (!fp) { perror("fopen"); return 1; }   // 打印：fopen: No such file or directory
> ```
> 它会根据 `errno`（系统记录的最近一次错误）打印可读的错误原因。

## 动手：解析一个 ELF 头

```c
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <elf.h>

int main(int argc, char **argv) {
    if (argc < 2) { printf("用法: %s <elf文件>\n", argv[0]); return 1; }

    FILE *fp = fopen(argv[1], "rb");
    if (!fp) { perror("fopen"); return 1; }

    Elf64_Ehdr eh;
    if (fread(&eh, 1, sizeof(eh), fp) != sizeof(eh)) {
        printf("读 ELF 头失败\n"); return 1;
    }

    // 1. 检查魔数
    if (eh.e_ident[0] != 0x7f || eh.e_ident[1] != 'E' ||
        eh.e_ident[2] != 'L' || eh.e_ident[3] != 'F') {
        printf("不是 ELF 文件\n"); return 1;
    }

    printf("类别   : %s\n", eh.e_ident[EI_CLASS] == 2 ? "64位" : "32位");
    printf("字节序 : %s\n", eh.e_ident[EI_DATA] == 1 ? "小端" : "大端");
    printf("类型   : ");
    switch (eh.e_type) {
        case ET_REL:  printf("可重定位 (.o)\n"); break;
        case ET_EXEC: printf("可执行文件\n"); break;
        case ET_DYN:  printf("动态库/PIE\n"); break;   // PIE = 位置无关可执行文件
        default:      printf("其它 (%d)\n", eh.e_type); break;
    }
    printf("架构   : 0x%X %s\n", eh.e_machine,
           eh.e_machine == 0xB7 ? "(AArch64)" :
           eh.e_machine == 0x3C ? "(x86_64)" : "");
    printf("入口   : 0x%llX\n", (unsigned long long)eh.e_entry);
    printf("程序头 : 偏移 0x%llX  数量 %d  每个 %d 字节\n",
           (unsigned long long)eh.e_phoff, eh.e_phnum, eh.e_phentsize);

    // 2. 遍历程序头表
    fseek(fp, eh.e_phoff, SEEK_SET);
    printf("\n%-10s %-6s %-12s %-12s %-10s\n",
           "TYPE", "FLAGS", "VADDR", "FILESZ", "MEMSZ");
    for (int i = 0; i < eh.e_phnum; i++) {
        Elf64_Phdr ph;
        if (fread(&ph, 1, sizeof(ph), fp) != sizeof(ph)) break;

        const char *t = ph.p_type == PT_LOAD ? "LOAD" :
                        ph.p_type == PT_DYNAMIC ? "DYNAMIC" :
                        ph.p_type == PT_INTERP ? "INTERP" :
                        ph.p_type == PT_NOTE ? "NOTE" : "OTHER";
        char flags[4] = "---";
        if (ph.p_flags & PF_R) flags[0] = 'r';
        if (ph.p_flags & PF_W) flags[1] = 'w';
        if (ph.p_flags & PF_X) flags[2] = 'x';

        printf("%-10s %-6s 0x%010llX 0x%010llX 0x%08llX\n",
               t, flags,
               (unsigned long long)ph.p_vaddr,
               (unsigned long long)ph.p_filesz,
               (unsigned long long)ph.p_memsz);
    }

    fclose(fp);
    return 0;
}
```

编译运行：

```bash
gcc -Wall elfhead.c -o elfhead
./elfhead /bin/ls
# 或者解析一个 .so
./elfhead /system/lib64/libc.so
```

> [!note] 上面出现的 `PIE` 是什么
> **PIE = Position Independent Executable**（位置无关可执行文件）：不写死绝对地址、靠相对偏移定位，
> 因此能加载到内存任意位置（配合 ASLR 地址随机化）。**Android 5.0 起强制要求可执行文件是 PIE。**
> 所以 `ET_DYN`（动态类型）现在既可能是 `.so`，也可能是 PIE 可执行文件——这是现代系统的常态。

你会看到 3~5 个 `LOAD` 段：R、R+X（代码）、R+W（数据）。
对照第 17 章的 `/proc/pid/maps`——**那些段的权限就是这么来的。**

## 段与节的对应

| 常见节（Section） | 会被合并进哪个段 |
|---|---|
| `.text` `.rodata` | 只读段 / 代码段 |
| `.data` `.bss` `.got` | 可读写段 |
| `.init_array` | 可读写段（构造函数表） |
| `.dynsym` `.dynstr` | 可读写段（动态链接需要） |

> [!note] `nm` 是符号表查看器（第 27 章详讲）
> 下面的 `nm` 命令用来**列出文件里的符号**（函数、变量）。
> `nm ls_copy` 输出 "no symbols" 就说明这个文件被 strip 过、没有符号信息了。
> 第 27 章会系统讲 `nm`，第 30 章讲 `nm`/`objdump`/`readelf` 三件套。

## strip 之后少了什么

```bash
cp /bin/ls ls_copy
strip ls_copy
ls -l /bin/ls ls_copy        # 后者明显变小
nm ls_copy                    # 提示 "no symbols"
./ls_copy                     # 照样能跑
```

strip 删掉的是**节头表里那些调试/符号节**（`.symtab`、`.strtab`、`.debug_*`），
以及整个节头表。程序头表还在，所以照样能加载。

> [!note] 对本项目的影响
> 目标程序如果是 strip 过的（几乎所有发布版本都是），
> 你就**不能**靠符号名找函数，只能靠硬编码偏移。
> 这也是为什么本项目 `Android.mk` 里有 `-s`（strip）：让自己的产物也无从分析。
> 第 77 章会讲 strip 之后怎么找东西。

## 一个必须知道的点：file offset vs vaddr

```
文件里的偏移 p_offset  ←→  内存里的虚拟地址 p_vaddr
```

两者不相同，但有一个固定关系（对同一个段）：

```
vaddr = 基址 + p_vaddr
文件偏移 = p_offset + (vaddr - (基址 + p_vaddr))
```

读一个"文件里的地址"和读一个"内存里的地址"要用不同的偏移。
**本项目全部是读内存**，所以一律用 `基址 + p_vaddr`。

## 课后习题

### 习题 19.1 手写 ELF 魔数检测器（★★）

**任务要求**：写 `is_elf(const uint8_t* data)`，检查文件头 4 字节是否为 `7f 45 4c 46`（`\x7fELF`）。

**参考实现**：

```c
#include <stdio.h>
#include <stdint.h>
#include <assert.h>

int is_elf(const uint8_t* data) {
    return data[0] == 0x7f && data[1] == 'E' &&
           data[2] == 'L'  && data[3] == 'F';
}

int main(void) {
    uint8_t elf[4] = {0x7f, 'E', 'L', 'F'};
    uint8_t notelf[4] = {'P', 'K', 3, 4};   // ZIP 头
    assert(is_elf(elf));
    assert(!is_elf(notelf));
    printf("ELF 魔数 = 7f 45 4c 46\n");
    printf("习题 19.1 全部通过\n");
    return 0;
}
```

**验证断言**：`7f 45 4c 46` 判为 ELF，`50 4b 03 04`（ZIP）判为非 ELF。

> [!tip] 魔数是什么
> 每个文件格式开头都有"魔数"标识类型。ELF 的魔数是 `0x7f` 后跟 ASCII 的 `ELF`（`45 4c 46`）。
> `file` 命令就是靠读魔数判断文件类型的。

### 习题 19.2 解析 `e_type` 判断文件类型（★★）

**要求**：说出 `e_type` 为 1、2、3 分别代表什么；为什么现代可执行文件常是 3（DYN）。

> [!question]- 参考答案
> - `1 = ET_REL`（可重定位，`.o` 文件）；
> - `2 = ET_EXEC`（传统可执行，写死绝对地址）；
> - `3 = ET_DYN`（动态库 **或 PIE 可执行文件**）。
> 现代系统（Android 5.0+ 强制）要求可执行文件是 **PIE**（位置无关），
> 以便配合 ASLR 加载到随机基址——所以现在可执行文件也标 `ET_DYN`，
> 需要用其他字段区分它到底是 `.so` 还是 PIE 可执行文件。

## 常见坑排查表

| 症状 | 最可能的原因 | 解法 |
|---|---|---|
| 解析 ELF 头**字段错位** | 没按结构体顺序读 | `e_ident`(16) → `e_type`(2) → `e_machine`(2)… |
| `e_machine` 值不认识 | 架构编码不同 | `0x3E`=x86_64，`0xB7`=AArch64 |
| 把 `.so` 当成可执行文件 | 只看 `e_type` | `ET_DYN` 既可能是 .so 也可能是 PIE |
| 读符号表**为空** | 库被 strip 了 | strip 后只剩 `.dynsym` |

## 本章小结

> [!abstract] 本章要点已收束
> 回头把本页的"动手"与结论串成一句话；有勾不上的验收项，回去补做。

## 验收清单

- [ ] 能从 16 进制里认出 ELF 魔数 `7f 45 4c 46`（用 `xxd -l 16 /bin/ls` 看到 `7f 45 4c 46`）
- [ ] 知道 `e_type` 3 = 动态库、`e_machine` 0xB7 = AArch64（用 `readelf -h` 验证某文件的这两项）
- [ ] 能说出程序头表（给加载器）和节头表（给链接器）的区别（说出"运行时只需程序头表，strip 删的是节头表"）
- [ ] 编译运行了 ELF 头解析程序，看到至少 3 个 LOAD 段及其权限（运行程序，看到 R / R+X / R+W 段）
- [ ] 理解 `p_offset`（文件）与 `p_vaddr`（内存）的区别（指着一个 LOAD 段，说出两个偏移各用在哪）
- [ ] 知道 strip 删了什么、为什么程序还能跑（`strip` 后 `nm` 提示 no symbols，但程序仍能运行）

→ 下一章：[[第20章-动态链接GOT与PLT]]　—— 理解"调用一个 .so 里的函数"在运行时是怎么找到地址的。
