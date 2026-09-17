---
tags: [教程, 卷二, 二进制, 工具]
day: 20
aliases: [ch30]
---

# 第 30 章 · nm / objdump / readelf 实战

> [!abstract] 本章目标
> 掌握二进制分析三件套，能独立完成"崩溃地址 → 源码行"的定位。
> 卷二收官章，第 77、87 章会反复用到。

> [!note] 承上
> 卷二前 13 章讲了"程序在系统里怎么跑"的全套知识。
> 本章是**收官章**：把知识落成工具——用 `nm`/`objdump`/`readelf` 分析真实二进制。

## 三件套分工

| 工具 | 看什么 | 典型用途 |
|---|---|---|
| `nm` | **符号表**（有哪些函数/变量） | 查符号是否存在、地址多少 |
| `readelf` | **ELF 结构**（头、段、节、重定位） | 查架构、查依赖、查动态信息 |
| `objdump` | **反汇编**（机器码 → 汇编） | 看函数实现、定位崩溃指令 |

Android NDK 里对应带前缀的版本：

```bash
aarch64-linux-android-nm
aarch64-linux-android-readelf
aarch64-linux-android-objdump
aarch64-linux-android-addr2line
```

路径通常在 `$NDK/toolchains/llvm/prebuilt/windows-x86_64/bin/`。

## nm：查符号

```bash
nm -C -n libfoo.so        # -C 解修饰(C++ 名字)  -n 按地址排序
nm -g libfoo.so           # 只看全局符号
nm -D libfoo.so           # 只看动态符号表
nm -u libfoo.so           # 只看未定义符号（依赖什么）
nm --size-sort libfoo.so  # 按大小排序（找占地方的大函数）
```

输出格式：

```
0000000000012345 T rtcNewDevice
0000000000012400 t internal_helper
0000000000020000 D g_config
                 U malloc
```

三列：地址、类型、名字。

| 类型字母（大写=全局） | 含义 |
|---|---|
| `T` / `t` | 代码段（函数） |
| `D` / `d` | 已初始化数据 |
| `B` / `b` | BSS |
| `R` / `r` | 只读数据 |
| `U` | 未定义 |
| `W` / `w` | 弱符号 |

### 实用命令

```bash
# 这个函数在库里吗？
nm -D libembree4.a | grep rtcNewDevice

# 有多大？（找体积杀手）
nm --size-sort -S libfoo.so | tail -20

# 依赖哪些外部符号？
nm -u libfoo.so | sort -u
```

> [!note] C++ 名字要解修饰
> 直接 `nm` 会看到 `_ZN7android14SurfaceControlC1Ev` 这种。
> 加 `-C` 变成 `android::SurfaceControl::SurfaceControl()`。
> 或者手动：`c++filt _ZN7android14SurfaceControlC1Ev`

## readelf：查结构

```bash
readelf -h file        # ELF 头（架构、类型、入口）
readelf -l file        # 程序头（段、权限）
readelf -S file        # 节头（所有节）
readelf -d file        # 动态段（依赖库、符号表地址）★
readelf -s file        # 符号表
readelf -r file        # 重定位表
readelf -x .rodata file  # 转储某个节的十六进制
```

### 最常用的：查依赖

```bash
readelf -d libfoo.so | grep NEEDED
# 0x00000001 (NEEDED)  Shared library: [liblog.so]
# 0x00000001 (NEEDED)  Shared library: [libc++.so]
```

放到 Android 上如果报 `library "libxxx.so" not found`，
用这条命令确认它到底依赖什么。

### 查架构

```bash
readelf -h libfoo.so | grep -E 'Class|Machine|Type'
#   Class:                             ELF64
#   Machine:                           AArch64
#   Type:                              DYN (Shared object file)
```

### 查动态符号

```bash
readelf --dyn-syms libfoo.so
```

第 77 章就是从这里开始解析的。

## objdump：反汇编

```bash
objdump -d file              # 反汇编所有可执行节
objdump -d --start-address=0x1234 --stop-address=0x1300 file   # 指定范围
objdump -t file              # 符号表（同 nm）
objdump -h file              # 节头
objdump -R file              # 动态重定位项
objdump -x file              # 全部头信息
```

### 定位崩溃位置

假设崩溃日志：

```
#00 pc 000000000000f234  /data/local/tmp/klbq
```

步骤：

```bash
# 1. 找到这条地址属于哪个函数
aarch64-linux-android-addr2line -e klbq -f -C 0xf234
# 输出：
# DrawPlayer(ImDrawList*)
# /path/jni/src/Android_draw/draw_Gui.cpp:273

# 2. 看看附近的汇编
aarch64-linux-android-objdump -d klbq | grep -A20 -B5 'f234:'
```

`addr2line` 三参数：
- `-e` 指定文件
- `-f` 输出函数名
- `-C` 解修饰 C++ 名字

> [!danger] strip 后 addr2line 失效
> 本项目 `APP_LDFLAGS` 有 `-s`。
> 调试方法：
> 1. 临时去掉 `-s` 重新编译一份
> 2. 或链接时保留符号副本：`objcopy --only-keep-debug klbq klbq.debug`
> 3. 用 `addr2line -e klbq.debug 0xf234`
>
> （`objcopy` 是 binutils 里的"复制/转换目标文件"工具，`--only-keep-debug` 表示"只保留调试信息到新文件"。）

### 看懂崩溃地址的含义

```
fault addr 0x00000000000000a0
```

这个地址很小 → 几乎肯定是"空指针 + 偏移"。
`0xa0 = 160`。打开你的结构体定义，找哪个成员在偏移 160。

```
struct PlayerController {
    /* ... */
    char pad[0x388];
    uint64_t Pawn;      // 假设在 0x390
};
```

`0xa0` 可能是另一个结构的成员偏移。对照 `offsetof` 反查（第 07 章）。

## 实战：一次完整的定位

**现象**：程序一启动就 SIGSEGV，`fault addr 0x30`。

**第一步**：看 logcat

```bash
adb logcat | grep -i 'signal\|backtrace\|DEBUG'
```

得到：

```
signal 11 (SIGSEGV), code 1, fault addr 0x30
backtrace:
  #00 pc 000000000000a1c8  /data/local/tmp/klbq
  #01 pc 000000000000b234  /data/local/tmp/klbq
```

**第二步**：addr2line

```bash
aarch64-linux-android-addr2line -e klbq -f -C 0xa1c8
# UpdateGameData()
# jni/src/Android_draw/draw_Gui.cpp:93
```

**第三步**：看第 93 行

```cpp
93:    Uleve = static_cast<long int>(dr->Read<uint64_t>(Uworld + 0x30));
```

`Uworld + 0x30`，崩溃地址 `0x30` → **`Uworld == 0`**。

**第四步**：往上找为什么

```cpp
90:    Uworld = static_cast<long int>(dr->Read<uint64_t>(libUE4 + 0xB3EC650));
91:    if (Uworld == 0) return;      // 这里有判空？
```

如果第 91 行的判空存在却还是崩了，说明 `Uworld` 非 0 但无效
（比如读到的是过期数据）。那就继续查 `libUE4` 是否正确。

**结论**：这类崩溃 90% 是"某一跳没读到，但没判空"。

## 对比两个二进制

```bash
# 符号差异（版本升级后哪些函数没了）
nm old.so | awk '{print $3}' | sort > old.txt
nm new.so | awk '{print $3}' | sort > new.txt
diff old.txt new.txt

# 大小变化
size old.so new.so
```

## 实用脚本：批量查符号

```bash
#!/bin/bash
# find_sym.sh <lib> <pattern>
LIB=$1
PAT=$2
nm -C -D "$LIB" 2>/dev/null | grep -i "$PAT" || \
    echo "动态符号表里没找到，试全符号表："
nm -C "$LIB" 2>/dev/null | grep -i "$PAT"
```

## Windows 上的替代方案

没有 GNU binutils 时：

| 需求 | 替代 |
|---|---|
| 看 ELF 结构 | Python 脚本（自己解析，第 19 章写过） |
| 反汇编 | `llvm-objdump`（NDK 里有） |
| addr2line | NDK 的 `aarch64-linux-android-addr2line`（有 Windows .exe） |
| 十六进制查看 | VS Code 的 Hex Editor 插件 |

NDK 工具链路径：

```
%NDK%\toolchains\llvm\prebuilt\windows-x86_64\bin\
    aarch64-linux-android21-clang.cmd
    aarch64-linux-android-addr2line.exe
    aarch64-linux-android-nm.exe
    aarch64-linux-android-objdump.exe
    aarch64-linux-android-readelf.exe
```

## 动手：分析一个真实的 .so

任务清单（用 NDK 的 `libc++_shared.so` 或系统里的 `/system/lib64/libc.so`）：

```bash
# 1. 基本信息
readelf -h libfoo.so | grep -E 'Class|Machine|Type|Entry'

# 2. 有几个 LOAD 段？权限分别是什么？
readelf -l libfoo.so | grep -E 'LOAD|Flags'

# 3. 依赖哪些库？
readelf -d libfoo.so | grep NEEDED

# 4. 动态符号表有多少项？
readelf --dyn-syms libfoo.so | head -5

# 5. 最大的 5 个函数是什么？
nm --size-sort -S -D libfoo.so | tail -5

# 6. 反汇编入口附近
objdump -d --start-address=<entry> --stop-address=<entry+64> libfoo.so
```

把每条命令的输出记录下来——这就是你的"二进制分析笔记"。

> [!success] 卷二成品：binpeek
> 把卷二的核心技能组装成一个工具——**输入一个 ELF 文件 + 一个地址，输出"文件:行号 + 所属函数 + 所属段"**。
>
> **它综合了**：
> - 第 19 章 ELF 头解析（`e_entry`、LOAD 段）
> - 第 22 章 maps 解析（地址落在哪个段）
> - 第 27 章符号表结构（`nm` 找函数边界）
> - 第 30 章 `addr2line`（地址→源码行）
>
> ```bash
> ./binpeek libfoo.so 0x1234
> # 输出：
> #   地址 0x1234 属于 .text 段
> #   所在函数：my_function
> #   源码位置：foo.cpp:42
> ```
>
> **验收**：对一个带符号的 .so 跑一次，结果与 `addr2line -f -e libfoo.so 0x1234` 一致。
>
> **这是卷六 `memview` 的前身**——都是"解析二进制 + 定位地址"的工具。

## 卷二收官：你学到了什么

| 章 | 能力 |
|---|---|
| 17 | 虚拟地址空间、页表、maps 布局 |
| 18 | 进程、/proc、按包名找 PID |
| 19 | ELF 头、程序头表、段与节 |
| 20 | GOT/PLT、延迟绑定、动态符号 |
| 21 | 系统调用、调用号、strace |
| 22 | 文件 IO、/proc 解析、maps 扫描 |
| 23 | 六种 IPC、共享内存、轮询标志 |
| 24 | 字节序、对齐、位运算、位图 |
| 25 | ARM64 寄存器、调用约定、汇编 |
| 26 | IEEE754、精度、NaN/Inf |
| 27 | 符号解析、重定位、强弱符号 |
| 28 | 静态库、按需抽取、链接顺序 |
| 29 | Makefile、Android.mk/Application.mk |
| 30 | nm/readelf/objdump/addr2line |

> [!success] 卷二综合练习
> 1. 写一个程序：扫描 `/proc`，找出所有进程的模块基址，输出到文件
> 2. 解析一个 `.so` 的 ELF 头，打印架构、入口、所有 LOAD 段
> 3. 制造一次段错误，用 `addr2line` 定位到源码行
> 4. 做一个静态库 + 一个可执行文件，用 Makefile 管理，故意打乱链接顺序再修好

## 课后习题

### 习题 30.1 从崩溃地址到源码行（★★，需带符号产物）

**要求**：用 `addr2line` 把一个地址转成 `文件:行号`。

**参考实现**：

```bash
# 1. 编译一份带调试信息的产物
gcc -g -O0 crash.c -o crash

# 2. 用 objdump 找 main 的地址（地址是随机的，每次编译都可能不同）
objdump -d crash | grep -A8 '<main>:'
# 输出示例（看第一行冒号前的地址）：
#   0000000140001490 <main>:
#     140001498:  e8 f3 00 00 00   call ...
#   ↑ 记下这个地址（这里是 0x140001498）

# 3. 用第 2 步得到的地址还原源码行
addr2line -e crash -f -C 0x140001498
# 输出：
#   main
#   C:/code/crash.c:3
```

> [!warning] 地址每次编译都可能变，不要照抄
> 上面的 `0x140001498` 只是**示例**。你必须用自己 `objdump` 输出里的真实地址。
> **Windows/MinGW** 的地址通常是 `0x14000xxxx` 这样的长格式；
> **Linux/Android** 常见 `0x1149` 这样的短格式（PIE 的偏移）。
> 两者都直接用 `addr2line -e <文件> -f -C <你查到的地址>` 即可。
>
> **如果 addr2line 输出 `??  ??:0`**：说明该地址没落在有调试信息的代码里，
> 多半是地址抄错了，或编译时没加 `-g`（见下条）。

> [!danger] strip 后 addr2line 失效
> `-s`（strip）会删掉调试信息，`addr2line` 就查不到了。
> 调试期去掉 `-s`，或用 `objcopy --only-keep-debug` 分离出 `.debug` 文件单独保留。

### 习题 30.2 看懂 fault addr 的含义（★）

**要求**：崩溃日志 `fault addr 0x00000000000000a0` 说明什么？

> [!question]- 参考答案
> 这个地址很小（0xa0 = 160），几乎肯定是"**空指针 + 偏移**"——
> 即某处 `base + 0xa0` 里 base 是 0。查结构体定义，0xa0 处是哪个字段，
> 就能反推是哪一步没判空。这是本项目最常见的崩溃模式（第 30、89 章）。

## 验收清单

- [ ] 会用 `nm -C -D` 查符号，`nm -u` 查依赖（对某 .so 跑两条命令，读出导出符号和未定义符号）
- [ ] 会用 `readelf -h/-l/-d/--dyn-syms` 查 ELF 结构（四条命令各跑一次，读出架构/段/依赖/动态符号）
- [ ] 会用 `objdump -d` 反汇编指定地址范围（用 --start-address/--stop-address 反汇编某函数）
- [ ] **会用 addr2line 把崩溃地址转成 文件:行号** ★（对带符号产物跑 addr2line，得到 文件:行号）
- [ ] 能完成一次"SIGSEGV → 定位到源码行"的完整流程（从 fault addr 到源码行，走通全流程）
- [ ] 知道 strip 会让 addr2line 失效，以及怎么保留符号（strip 后 addr2line 失效，用 --only-keep-debug 保留）

→ 下一卷：[[卷三-本卷导航]] · [[第31章-Android系统分层]]　—— 建立 Android 的完整分层图景，知道每层提供什么、你的程序站在哪一层。
