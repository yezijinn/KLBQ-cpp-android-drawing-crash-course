---
tags: [教程, 深挖, 卷二, 编译, 链接, 二进制]
aliases: [深挖B]
---

# 深挖 B · 编译链接与二进制的深水区

> [!abstract] 这篇解决什么
> 第 03、19、27、28、30 章讲了主干。这篇补齐：
> 重定位类型的完整解读、符号可见性的规则、名字修饰的完整算法、
> 段与节的完整对照、以及 12 种链接错误的诊断路径。
> **建议学完卷二第 30 章后读。**

## 一、三个阶段、三类错误

新手最容易混淆的是"错误发生在哪个阶段"。分清这个，排查效率立刻翻倍。

| 阶段 | 谁在工作 | 典型错误 | 错误信息特征 |
|---|---|---|---|
| **预处理** | 预处理器 | 头文件找不到、宏错误 | `No such file`、`stray '\357'` |
| **编译** | 编译器（clang/gcc） | 语法、类型、未声明 | `error: expected ';'`、`'x' was not declared` |
| **汇编** | 汇编器 | 汇编语法错（手写 .S 时） | `Error: bad instruction` |
| **链接** | 链接器（ld/lld） | 符号找不到、重复定义 | `undefined reference`、`multiple definition` |
| **加载** | 动态链接器（`linker64`） | 缺库、符号版本不符 | `CANNOT LINK EXECUTABLE`、`library not found` |
| **运行** | CPU + 你的代码 | 段错误、逻辑错 | `SIGSEGV`、数据不对 |

**关键区分**：

```
undefined reference to 'foo'          → 链接期（编译单元里声明了，但没人实现）
error: 'foo' was not declared          → 编译期（连声明都没有）
CANNOT LINK EXECUTABLE: library "x" not found → 加载期（库不在设备上）
```

三者是完全不同的问题，解法也完全不同。

## 二、重定位的完整解读

### 为什么需要重定位

编译单个 `.cpp` 时，编译器不知道：
- 外部函数/变量最终在哪个地址
- 自己这段代码会被放在哪个地址

所以它留下"填空题"：

```asm
bl  0          ; 占位，等着填 printf 的地址
```

链接器负责填这些空。**"填空"这件事就叫重定位。**

### 常见重定位类型（AArch64）

```bash
aarch64-linux-android-readelf -r foo.o
```

| 类型 | 出现在 | 含义 |
|---|---|---|
| `R_AARCH64_CALL26` | `bl` 指令 | 函数调用目标，26 位相对偏移（±128MB） |
| `R_AARCH64_JUMP26` | `b` 指令 | 跳转目标 |
| `R_AARCH64_ADR_PREL_PG_HI21` | `adrp` | 取目标所在**页**的地址（高 21 位） |
| `R_AARCH64_ADD_ABS_LO12_NC` | `add` | 页内低 12 位偏移 |
| `R_AARCH64_ABS64` | 数据段里的指针 | 64 位绝对地址 |
| `R_AARCH64_RELATIVE` | `.got` 等 | "基址 + 常量"，PIE 特有 |
| `R_AARCH64_LDST64_ABS_LO12_NC` | `ldr/str` | 访问静态变量 |

### `adrp` + `add` 是怎么工作的

这是 AArch64 访问全局变量的标准两步：

```asm
adrp    x0, 0x1000           ; 取目标所在的 4KB 页基址（PC 相对，±4GB）
add     x0, x0, #0x650       ; 加上页内偏移（0~4095）
ldr     w1, [x0]             ; 读出值
```

为什么分两步？因为单条指令的立即数位宽不够（21 位页号 + 12 位页内 = 33 位，
覆盖 ±4GB 范围），而 64 位绝对地址需要更多位。

**这个模式对第 87 章"特征码定位"极其重要**——你要找的就是 `adrp + add`
这一对指令，从中解出目标地址。

### `R_AARCH64_RELATIVE`：PIE 的核心

```bash
aarch64-linux-android-readelf -r libfoo.so | head
```

```
Offset          Info           Type            Sym. Value
0000000000012000  0000000000000403 R_AARCH64_RELATIVE   1234
```

含义：**在偏移 0x12000 处，填入 "基址 + 0x1234"**。

这就是 PIE 的实现方式：代码里没有一个绝对地址，全是相对偏移，
加载时由动态链接器按实际基址计算。

## 三、符号可见性的完整规则

### 三级可见性

```cpp
__attribute__((visibility("default")))  // 导出，可被外部引用
__attribute__((visibility("hidden")))   // 不导出（默认在 -fvisibility=hidden 下）
__attribute__((visibility("protected")))// 导出但不可被覆盖
```

本项目 `Application.mk`：

```makefile
APP_CPPFLAGS += -fvisibility=hidden
```

**作用**：
1. 符号表大幅减小（体积优化）
2. 外部无法通过 `dlsym` 找到你的内部函数
3. 避免符号碰撞

### 为什么要 `hidden`

```cpp
// 不设置 visibility（默认全部导出）
void internal_helper() { ... }     // 这个也会出现在动态符号表里
```

一个中等项目几千个符号，全导出会：
- 产物大
- 加载慢（动态链接器要处理所有符号）
- 可能被别人 `dlsym` 到（安全 + 稳定性风险）

**对可执行文件来说，导出符号毫无用处**——所以全设 `hidden` 是纯收益。

### 但有些情况必须导出

```cpp
// 插件模式：主程序要通过 dlsym 找插件的函数
extern "C" __attribute__((visibility("default")))
void plugin_init() { ... }
```

还有 `extern "C"` —— 防止 C++ 名字修饰，让符号名可预测。

## 四、名字修饰的完整规则（Itanium ABI）

ARM64 的 Linux/Android 用 Itanium C++ ABI 的名字修饰（mangling）。

### 基本结构

```
_ZN  <嵌套开始>
     <长度><标识符>     命名空间
     <长度><标识符>     类名
     <长度><标识符>     函数名
E    <参数列表结束分隔>
     <参数类型编码>
```

看一个完整的例子：

```cpp
namespace android {
class SurfaceComposerClient {
    sp<SurfaceControl> createSurface(const String8& name,
                                     uint32_t w, uint32_t h);
};
}
```

```
_ZN7android21SurfaceComposerClient13createSurfaceERKNS_7String8Ejj
│  │       │                    │              ││   │
│  │       │                    │              ││   └ j = uint32_t
│  │       │                    │              │└──── j = uint32_t（第二个）
│  │       │                    │              └───── RKNS_7String8E = const String8&
│  │       │                    └──────────────────── 13 = 函数名长度
│  │       └───────────────────────────────────────── 21 = 类名长度
│  └───────────────────────────────────────────────── 7 = "android"
└──────────────────────────────────────────────────── _ZN = 嵌套符号开始
```

### 类型编码表

| 编码 | 类型 |
|---|---|
| `v` | void |
| `b` | bool |
| `c` | char |
| `i` | int |
| `j` | unsigned int |
| `l` | long |
| `m` | unsigned long |
| `x` | long long |
| `f` | float |
| `d` | double |
| `P` | 指针（前缀） |
| `R` | 引用（前缀） |
| `K` | const（前缀） |
| `N...E` | 嵌套名 |
| `S_` | 替换（压缩） |

`S_` 是"替换表"：出现过的名字用 `S_`、`S0_`、`S1_` 引用，避免重复。

### 特殊函数名的编码

| 函数 | 编码 |
|---|---|
| 构造函数 | `C1`（完整对象）、`C2`（基类子对象） |
| 析构函数 | `D1`、`D2` |
| `operator new` | `nw` |
| `operator[]` | `ix` |

所以：

```
_ZN7android14SurfaceControlC1Ev       → SurfaceControl::SurfaceControl()
_ZN7android14SurfaceControlD1Ev       → SurfaceControl::~SurfaceControl()
```

**这就是第 65 章那些乱码符号的完整解释。**

### 自己解修饰

```bash
aarch64-linux-android-c++filt _ZN7android14SurfaceControlC1Ev
# android::SurfaceControl::SurfaceControl()
```

## 五、段与节的完整对照

### 程序头（段）给加载器

| `p_type` | 值 | 含义 |
|---|---|---|
| `PT_LOAD` | 1 | 需要映射进内存 |
| `PT_DYNAMIC` | 2 | 动态链接信息 |
| `PT_INTERP` | 3 | 解释器路径 |
| `PT_NOTE` | 4 | 附加信息（如 build-id） |
| `PT_GNU_EH_FRAME` | 0x6474e550 | 异常处理帧信息 |
| `PT_GNU_STACK` | 0x6474e551 | 栈是否可执行 |
| `PT_GNU_RELRO` | 0x6474e552 | 重定位后只读 |

### 节头（节）给链接器/调试器

| 节 | 内容 | 归属段 |
|---|---|---|
| `.text` | 代码 | 代码段（r-x） |
| `.rodata` | 只读数据（字符串常量、跳转表） | 只读段（r--） |
| `.data` | 已初始化全局变量 | 数据段（rw-） |
| `.bss` | 未初始化全局变量 | 数据段（rw-，只占内存不占文件） |
| `.init_array` | 构造函数指针数组 | 数据段 |
| `.fini_array` | 析构函数指针数组 | 数据段 |
| `.got` / `.got.plt` | 全局偏移表 | 数据段 |
| `.plt` | 过程链接表（桩代码） | 代码段 |
| `.dynsym` | 动态符号表 | 只读段 |
| `.dynstr` | 动态字符串表 | 只读段 |
| `.symtab` | 完整符号表（**strip 删除**） | 不加载 |
| `.strtab` | 完整字符串表（strip 删除） | 不加载 |
| `.debug_*` | 调试信息（strip 删除） | 不加载 |

**记住这条分界线**：`.dynsym`/`.dynstr` 在**加载段内**，strip 删不掉；
`.symtab`/`.strtab` 在**加载段外**，strip 删掉不影响运行。

这就是第 19 章"strip 后程序仍能跑"的原因，也是第 77 章"符号解析只能用 `.dynsym`"的原因。

### `.init_array` 与 C++ 全局对象

```cpp
struct Init { Init() { printf("构造\n"); } };
Init g_init;      // 全局对象
```

编译后，它的构造函数被放进 `.init_array`，
由动态链接器在 `main` 之前逐个调用。

**调试"main 之前就崩溃"的问题时，看这里。**

## 六、12 种链接/加载错误诊断路径

| # | 错误信息 | 原因 | 诊断命令 |
|---|---|---|---|
| 1 | `undefined reference to 'foo'` | 没实现/没链库 | `nm -u` 看谁需要它 |
| 2 | `undefined reference to 'std::__ndk1::...'` | STL 不一致 | 检查 `APP_STL` |
| 3 | `multiple definition of 'x'` | 头文件里定义了变量 | 改 `extern` 或 `inline` |
| 4 | `duplicate symbol` | 同名 C 函数在两个 `.o` | `nm -A \| grep` 定位 |
| 5 | `cannot find -lfoo` | 库不在搜索路径 | `-L` 指定路径 |
| 6 | `skipping incompatible libfoo.a` | ABI 不符 | `file` / `readelf -h` |
| 7 | `file not recognized` | 文件损坏或非对象文件 | `file` |
| 8 | `relocation truncated to fit` | 跳转超范围（±128MB） | 用 `-mcmodel` 或中间跳板 |
| 9 | `CANNOT LINK EXECUTABLE: library "x" not found` | 加载期缺库 | `readelf -d \| grep NEEDED` |
| 10 | `cannot locate symbol "y"` | 符号版本不符 | 检查设备上该库的符号 |
| 11 | `has text relocations` | 有需要运行时改代码段的重定位 | 加 `-fPIC` |
| 12 | `only position independent executables are supported` | 缺 PIE | 加 `-fPIE -pie` |

### 诊断练习

```bash
# 1. 谁需要 foo？
nm -u libfoo.a | grep foo

# 2. foo 在哪个库里定义？
for f in *.a; do nm "$f" 2>/dev/null | grep -q ' T foo' && echo "$f"; done

# 3. 这个 .a 是什么架构？
aarch64-linux-android-readelf -h libfoo.a | grep Machine | sort -u

# 4. 这个可执行文件需要哪些动态库？
readelf -d app | grep NEEDED

# 5. 设备上有这些库吗？
adb shell ls /system/lib64/libfoo.so
```

## 七、LTO 的原理与陷阱

### 原理

```
普通编译：
  a.cpp → a.o（机器码，优化只能看本文件）
  b.cpp → b.o（同上）
  链接：把机器码拼起来

LTO：
  a.cpp → a.o（LLVM IR，中间表示）
  b.cpp → b.o（同上）
  链接：合并所有 IR → 全局优化 → 生成最终机器码
```

### 能做什么

| 优化 | 说明 |
|---|---|
| 跨文件内联 | `b.cpp` 里的小函数被内联进 `a.cpp` |
| 全局死代码消除 | `a.cpp` 里没人调的函数，即使 `b.cpp` 引用了也能判断出来 |
| 常量传播 | `a.cpp` 定义的 `const` 在 `b.cpp` 里被直接替换 |
| 虚函数去虚化 | 如果能确定具体类型，直接调用而不是查虚表 |

### 陷阱

| 问题 | 症状 | 对策 |
|---|---|---|
| 编译慢 | 几分钟 | 调试期关掉 |
| 内存占用高 | 大项目吃几 GB | 分模块编译 |
| `addr2line` 不准 | 行号偏差 | 调试期关掉 |
| 内联后调试信息错乱 | 单步跳来跳去 | `-g -O0` |
| 某些符号被"优化没了" | `nm` 找不到 | 加 `__attribute__((used))` |

**实用建议**：

```makefile
# 发布
APP_LDFLAGS := -flto -Wl,--gc-sections -s
# 调试
APP_LDFLAGS := -Wl,--gc-sections
```

## 八、把这篇用在项目里

| 知识点 | 项目位置 |
|---|---|
| 重定位 `adrp+add` | 第 87 章找偏移时会读这种指令 |
| 符号可见性 | `Application.mk` 的 `-fvisibility=hidden` |
| 名字修饰 | 第 65 章 dlsym libgui |
| 段与节 | 第 19、30 章分析 ELF |
| `.dynsym` vs `.symtab` | 第 77 章符号解析 |
| STL 一致性 | 第 40 章 `APP_STL` |
| 链接顺序 | 第 28 章 + `LOCAL_STATIC_LIBRARIES` |
| LTO | 第 41 章体积裁剪 |

> [!tip] 一个实用的排查顺序
> 遇到构建错误，按这个顺序走：
> ```
> 1. 看错误发生在哪个阶段（编译/链接/加载/运行）
> 2. 编译期 → 看第一行错误，别被后面的连锁反应干扰
> 3. 链接期 → nm -u 看谁缺，nm -A 看谁有，检查顺序
> 4. 加载期 → readelf -d 看 NEEDED，确认设备上有
> 5. 运行期 → 看 logcat，用 addr2line 定位
> ```

→ 返回 [[卷二-本卷导航]]
