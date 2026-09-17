---
tags: [教程, 卷二, 二进制, 链接]
day: 13
aliases: [ch20]
---

# 第 20 章 · 动态链接：GOT 与 PLT

> [!abstract] 本章目标
> 理解"调用一个 .so 里的函数"在运行时是怎么找到地址的。
> 这是第 60 章"动态加载 libvulkan"和第 65 章"dlsym libgui"的必要背景。

> [!note] 承上
> 上一章看清了 ELF 的静态结构。
> 本章讲**动态结构**：程序运行时，`printf` 这些库函数的地址是怎么找到的。

## 先看一个矛盾

你的程序调用了 `printf`，但 `printf` 的代码在 `libc.so` 里。
而 `libc.so` 每次加载的基址都不同（ASLR）。

那编译 `call printf` 这条指令时，地址填什么？**填不了。**

解法是**延迟绑定**：先跳转到一个"中介"，中介第一次被调用时去查真实地址，
查到后存起来，以后直接用。

## 两个表：PLT 和 GOT

```
你的代码
   │  call printf@plt
   ↓
┌──────────────────────┐
│ PLT[n]  printf 的桩    │  ← 一小段跳板代码
│  jmp *GOT[n]          │
└──────────┬───────────┘
           ↓
┌──────────────────────┐
│ GOT[n]  存真实地址     │  ← 数据，运行时可写
└──────────────────────┘
```

| 表 | 是什么 | 在哪 | 可写吗 |
|---|---|---|---|
| **PLT** | 代码（跳板指令） | `.plt` 节，代码段 | 只读 |
| **GOT** | 数据（地址数组） | `.got.plt` 节，数据段 | **可写** |

## 第一次调用时发生了什么

```
1. call printf@plt
2. PLT[printf]: jmp *GOT[printf]
3. 此时 GOT[printf] 里存的是"PLT 里的下一条指令地址"
   → 于是又跳回 PLT，继续执行
4. push 一个序号（表示"我要找 printf"）
5. jmp PLT[0] → 调用 _dl_runtime_resolve
6. 动态链接器查 libc 的符号表，找到 printf 的真实地址
7. 把这个地址写进 GOT[printf]        ← 关键：改的是数据段，代码段不用动
8. 跳到 printf 执行
```

**第二次调用**：`jmp *GOT[printf]` 直接跳到真实地址。没有额外开销。

这就是"延迟绑定（lazy binding）"：不用到的函数永远不会被解析。

## 为什么需要 GOT 可写

如果代码段里直接写了 `printf` 的地址，那么每次 ASLR 换基址，
内核就得修改代码段——但代码段是只读的，而且改代码会破坏共享（多个进程共用同一份 libc 代码页）。

用 GOT 之后：
- 代码段保持不变 → 可以**在进程间共享**，省内存
- 每个进程有自己的 GOT 副本 → 各自填各自的地址

## 全局变量也要 GOT

不只是函数，跨模块的变量访问也要经过 GOT：

```c
extern int errno;      // 在 libc 里
errno = 0;             // 编译后：通过 GOT 找到 errno 的地址再写
```

> [!note] `objdump` / `readelf` 是工具，第 30 章会系统讲
> 本章后面反复用到 `objdump`、`readelf`——它们是"查看二进制文件"的命令行工具（第 30 章专门讲）。
> 现在你只要知道：
> - `objdump -d` → **反汇编**（把机器码翻译回汇编指令）
> - `readelf -d` → 看**动态段**（依赖哪些库）
> - `readelf --dyn-syms` → 看**动态符号表**（导出了哪些函数）
> 命令的具体用法和输出解读，第 30 章会讲。**本章先看"输出大概长什么样"。**

## 实际的反汇编

用 `objdump` 看一个调用：

```bash
objdump -d -j .plt ./app | head -30
```

你会看到类似：

```
0000000000001030 <printf@plt>:
    1030:  bnd jmp *0x2f8d(%rip)      # 跳转到 GOT 里的地址
    1037:  push $0x0                   # 符号序号
    103c:  jmp  1020 <.plt>            # 跳到公共解析入口
```

`0x2f8d(%rip)` 是**相对当前指令的偏移**——这就是"位置无关代码（PIC）"：
代码里没有一个绝对地址，全靠相对偏移，所以代码放到哪都能执行。

> [!note] PIE 与 PIC
> - **PIC**（Position Independent Code）：`.so` 必须是 PIC，否则无法在任意基址加载
> - **PIE**（Position Independent Executable）：可执行文件也做成位置无关，配合 ASLR
>
> Android 5.0 之后强制 PIE。所以你写的每个原生可执行文件都要能适应随机基址。

## 动态段：PT_DYNAMIC

链接器怎么知道符号表在哪、字符串表在哪？答案在 `PT_DYNAMIC` 段里，
它是一个 `Elf64_Dyn` 数组（键值对）：

```c
typedef struct {
    int64_t  d_tag;      // 类型
    uint64_t d_val;      // 值（或地址）
} Elf64_Dyn;
```

| `d_tag` | 值 | 含义 |
|---|---|---|
| `DT_NULL` | 0 | 结束 |
| `DT_NEEDED` | 1 | 依赖的库名（字符串表索引） |
| `DT_HASH` | 4 | 符号哈希表地址 |
| `DT_STRTAB` | 5 | **字符串表地址** |
| `DT_SYMTAB` | 6 | **符号表地址** |
| `DT_STRSZ` | 10 | 字符串表大小 |
| `DT_SYMENT` | 11 | 每个符号表项大小 |
| `DT_JMPREL` | 23 | PLT 重定位表地址 |
| `DT_RELA` | 7 | 重定位表地址 |

用法（第 77 章会完整实现）：

```
1. 找到 PT_DYNAMIC 段 → 遍历 Elf64_Dyn
2. 取 DT_SYMTAB → 符号表地址；DT_STRTAB → 字符串表地址
3. 遍历符号表，比较名字 → 找到符号的 st_value（相对基址的偏移）
4. 真实地址 = 模块基址 + st_value
```

## 动手：列出一个 .so 的动态符号

不用写代码，先用现成工具看：

```bash
# 动态符号表（运行时用）
readelf --dyn-syms /system/lib64/libc.so | head -20

# 依赖哪些库
readelf -d /system/lib64/libc.so | grep NEEDED

# 程序头（找 PT_DYNAMIC）
readelf -l /system/lib64/libc.so | grep -A1 DYNAMIC
```

输出示例：

```
Symbol table '.dynsym' contains 2000 entries:
   Num: Value          Size Type    Bind   Vis      Ndx Name
     1: 00000000000a1234 45 FUNC   GLOBAL DEFAULT  12 printf@@LIBC
```

`Value` 列就是相对基址的偏移。真实地址 = 基址 + 这个值。

**Windows 上没这些命令？** 用 WSL，或者跳过——第 30 章会教你用 Python 脚本代替。

## 与项目的关系

本项目有两处直接用到这套知识：

> [!note] `dlopen` / `dlsym` 是"手动动态链接"（第 60 章详讲）
> - `dlopen("libxxx.so", 标志)`：**在运行时打开一个动态库**，返回它的句柄
> - `dlsym(句柄, "函数名")`：**从库里找到某个函数的地址**
> 它们让"加载库、找函数"从"自动"变成"你手动控制"。本项目用它加载系统私有库。
> **第 60 章会完整讲，这里先看个形状。**

**1. `vulkan_wrapper.cpp`：动态加载 libvulkan**

```cpp
void *lib = dlopen("libvulkan.so", RTLD_NOW);
auto *p = dlsym(lib, "vkCreateInstance");
```

`dlopen`/`dlsym` 就是让动态链接器帮你做上面那套查找流程（第 60 章）。

**2. `ANativeWindowCreator`：dlsym 私有 API**

```cpp
void *handle = dlopen("libgui.so", RTLD_NOW);
auto fn = dlsym(handle, "_ZN7android21SurfaceComposerClient11createSurfaceERKNS_7String8EjjijPNS_2spINS_7IBinderEEEii");
```

`_ZN7android...` 这种乱码叫 **mangled name**（名字修饰）。
C++ 为了支持重载，把函数名、参数类型编码进符号名（第 65 章详述）。

## 常见坑

> [!danger] 32 位 / 64 位 ELF 结构不同
> `Elf32_Ehdr` 和 `Elf64_Ehdr` 字段大小不一样，别混用。
> 判断方法：`e_ident[EI_CLASS]` == 1 用 32 位结构，== 2 用 64 位。

> [!danger] 地址是虚拟地址，不是文件偏移
> `DT_SYMTAB` 里的值是**加载后的虚拟地址**（相对基址）。
> 如果你在解析**文件**（而不是内存），需要先用程序头表把 vaddr 转成 file offset。

> [!warning] Android 7.0 之后限制 dlopen 系统私有库
> `libgui.so`、`libutils.so` 属于"灰名单"库，普通 App 的 `dlopen` 会被拒绝。
> 但**原生可执行文件不受此限制**——这正是本项目选择做成可执行文件而非 APK 的原因之一（第 32 章）。

## 课后习题

### 习题 20.1 用 readelf 查依赖与动态符号（★）

**要求**：对一个 `.so` 文件，用 `readelf` 查出它依赖哪些库、导出了哪些符号。

**验证方式**（Linux/WSL）：
1. 对一个有依赖的 `.so`（如 `/system/lib64/libgui.so` 或自己编的）跑 `readelf -d | grep NEEDED`，应列出若干 `[libxxx.so]`
2. 跑 `readelf --dyn-syms | head`，应看到导出符号（`FUNC`/`OBJECT`）
3. 交叉验证：`ldd <文件>`（Linux）列的依赖应与 `NEEDED` 一致

**参考实现**：

```bash
# 查依赖哪些库（NEEDED）
readelf -d libfoo.so | grep NEEDED
# 输出示例：
#   0x00000001 (NEEDED)  Shared library: [liblog.so]
#   0x00000001 (NEEDED)  Shared library: [libc.so]

# 查导出的动态符号
readelf --dyn-syms libfoo.so | head
```

> [!note] 为什么看 NEEDED
> 程序在设备上报 `library "libxxx.so" not found` 时，用 `readelf -d | grep NEEDED`
> 就能确认它到底依赖什么（第 30 章实战）。

### 习题 20.2 理解延迟绑定（★★，概念题）

**要求**：说出第一次调用 `printf` 和第二次调用分别发生了什么。

> [!question]- 参考答案
> **第一次**：`call printf@plt` → PLT 跳到 GOT，GOT 此时指向"解析桩"，
> 于是调用 `_dl_runtime_resolve`，动态链接器查 `libc` 符号表找到 `printf` 真实地址，**写回 GOT**。
> **第二次**：PLT 跳 GOT，GOT 已经是真实地址，**直接跳过去**，无额外开销。
> 这就是"延迟绑定"——不用的函数永远不会被解析。

## 验收清单

- [ ] 能解释为什么 `call printf` 在编译时填不了地址（说出"libc 基址每次不同，ASLR"）
- [ ] 能说出 PLT（代码）和 GOT（数据）各自的作用（PLT 是跳板指令、GOT 存真实地址）
- [ ] 知道延迟绑定：第一次调用才解析，第二次直接用（用 `objdump -d -j .plt` 看跳板结构）
- [ ] 知道 `DT_SYMTAB` / `DT_STRTAB` 是找符号的入口（用 `readelf -d` 找出这两项）
- [ ] 会用 `readelf -d` 和 `readelf --dyn-syms` 查看依赖与动态符号（对某 .so 跑两条命令，读出 NEEDED 和符号）
- [ ] 知道 `dlsym` 背后就是这套机制（说出"dlsym 让动态链接器帮你查符号表")

→ 下一章：[[第21章-系统调用]]　—— 理解用户态如何向内核请求服务，能直接用 `syscall()` 写一个 hello world。
