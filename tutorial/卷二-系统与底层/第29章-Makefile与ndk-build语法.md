---
tags: [教程, 卷二, 构建, Makefile]
day: 17
aliases: [ch29]
---

# 第 29 章 · Makefile 与 ndk-build 语法

> [!abstract] 本章目标
> 会写 Makefile，并能逐行读懂本项目的 `Android.mk` 和 `Application.mk`。
> 这是第 33 章（真正编译）和第 44 章（工程结构）的基础。

## 先看一个最小 Makefile

```makefile
app: main.o utils.o
	gcc main.o utils.o -o app

main.o: main.c
	gcc -c main.c -o main.o

utils.o: utils.c
	gcc -c utils.c -o utils.o
```

三条规则，每条格式：

```
目标: 依赖1 依赖2 ...
	命令1
	命令2
```

**命令行必须以 Tab 开头**——不是空格，是 Tab。

> [!danger] Makefile 第一大坑：Tab
> 用空格缩进会报 `*** missing separator. Stop.`
> VS Code 里可以设 `"editor.insertSpaces": false` 或在右下角把缩进改成 Tab。
> 更彻底的办法：`.editorconfig` 里给 `Makefile` 设 `indent_style = tab`。

## make 怎么工作

1. 找到第一个目标（`app`）作为"默认目标"
2. 检查它的依赖（`main.o` `utils.o`）是否存在、是否比目标新
3. 如果依赖不存在或更旧，就先去生成依赖（递归）
4. 所有依赖就绪后，执行目标的命令

**增量编译的依据是文件修改时间（mtime）。**
改了 `main.c` → `main.o` 过期 → 重新编译 `main.o` → 重新链接 `app`。
`utils.o` 没变 → 不重新编译。

## 变量

```makefile
CC      = gcc
CFLAGS  = -Wall -O2
OBJS    = main.o utils.o
TARGET  = app

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
```

| 赋值 | 含义 |
|---|---|
| `=` | 递归展开（用到时才求值） |
| `:=` | 立即展开（推荐） |
| `?=` | 没定义才赋值 |
| `+=` | 追加 |

> [!note] 上面的 `$<` 和 `$@`
> 变量节那个例子里出现了 `$<` 和 `$@`——它们是 **make 的自动变量**，含义"随当前规则变化"。
> **下一节「自动变量」会专门讲。** 这里先照抄即可。

## 自动变量

| 变量 | 含义 |
|---|---|
| `$@` | 当前目标名 |
| `$<` | 第一个依赖 |
| `$^` | 所有依赖（去重） |
| `$+` | 所有依赖（不去重） |
| `$*` | 模式匹配的主干部分 |

```makefile
%.o: %.c
	gcc -c $< -o $@          # $< = xxx.c   $@ = xxx.o
```

## 模式规则与通配

```makefile
# 所有 .c 编译成 .o
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# 收集所有源文件
SRCS = $(wildcard src/*.c)
OBJS = $(SRCS:.c=.o)        # 后缀替换：src/a.c → src/a.o
```

`$(wildcard ...)` 和 `$(patsubst ...)` 是常用函数：

```makefile
SRCS = $(wildcard src/**/*.c)
OBJS = $(patsubst %.c,%.o,$(SRCS))
```

## 伪目标

```makefile
.PHONY: clean all

all: app

clean:
	rm -f *.o app
```

`.PHONY` 声明的目标**不代表文件**。
不加的话，如果目录里恰好有个叫 `clean` 的文件，`make clean` 会认为"已是最新"而不执行。

## 头文件依赖（重要）

上面的 Makefile 有个 bug：改了 `.h`，`.o` 不会重新编译！

```makefile
main.o: main.c          # 没提头文件
```

解决：让编译器自动生成依赖：

```makefile
CFLAGS += -MMD -MP

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

-include $(OBJS:.o=.d)
```

`-MMD` 会生成 `main.d`（记录 main.o 依赖哪些 .h），
`-MP` 为每个头文件生成空目标（防止删了头文件报错），
`-include` 把这些依赖文件包含进来（`-` 表示文件不存在也不报错）。

## 完整模板

```makefile
CC      := gcc
CFLAGS  := -Wall -Wextra -O2 -MMD -MP
LDFLAGS :=
TARGET  := app
SRCDIR  := src
OBJDIR  := build

SRCS := $(wildcard $(SRCDIR)/*.c)
OBJS := $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

clean:
	rm -rf $(OBJDIR) $(TARGET)

-include $(DEPS)
```

`| $(OBJDIR)` 是"order-only 依赖"：目录存在即可，不因为目录 mtime 变化而重建。

## ndk-build 是什么

`ndk-build` 是 Android NDK 提供的构建工具，本质是一个 **GNU Make 封装**。
它读取 `jni/Android.mk` 和 `jni/Application.mk`，自动生成复杂规则。

```
你的工程/
├── jni/
│   ├── Android.mk        ← 模块定义（编译什么、链接什么）
│   └── Application.mk    ← 全局配置（ABI、平台、STL）
├── libs/                 ← 产物输出（ndk-build 生成）
│   └── arm64-v8a/
└── obj/                  ← 中间产物
```

## Application.mk 逐行（本项目）

```makefile
APP_ABI := arm64-v8a
```

目标 CPU 架构。可选：

| 值 | 架构 | 说明 |
|---|---|---|
| `arm64-v8a` | ARM 64 位 | **现代 Android 主流** |
| `armeabi-v7a` | ARM 32 位 | 老设备 |
| `x86` / `x86_64` | Intel | 模拟器 |
| `all` | 全部 | 产物大 |

本项目只要 `arm64-v8a`——因为目标设备是 64 位手机，且 Embree 的 `.a` 也只提供了这个架构。

```makefile
APP_PLATFORM := android-25
```

最低支持的 Android API 级别（25 = Android 7.1）。
影响：可用哪些系统 API、符号版本。

```makefile
APP_STL := c++_static
```

C++ 标准库选择：

| 值 | 含义 |
|---|---|
| `c++_static` | 静态链接 libc++（产物大，无依赖）**本项目用** |
| `c++_shared` | 动态链接（产物小，需要带 `libc++_shared.so`） |
| `system` | 用系统的 STL（功能残缺，已废弃） |

```makefile
APP_OPTIM := release
```

`release` → `-O2` + `NDEBUG`；`debug` → `-O0` + 调试信息。
**调试阶段改成 `debug`，发布改回 `release`。**

```makefile
APP_SHORT_COMMANDS := true
```

缩短命令行（Windows 上规避命令行长度限制）。

```makefile
APP_CPPFLAGS := \
    -std=c++17 \
    -fexceptions \
    -frtti \
    -fvisibility=hidden \
    -fdata-sections \
    -ffunction-sections \
    -funwind-tables \
    -fstack-protector-strong
```

| 选项 | 作用 |
|---|---|
| `-std=c++17` | C++ 标准（注意：模块里的 `LOCAL_CPPFLAGS` 又指定了 c++20，后者优先） |
| `-fexceptions` | 启用 C++ 异常 |
| `-frtti` | 启用运行时类型信息（`dynamic_cast` 需要） |
| `-fvisibility=hidden` | **默认隐藏符号**，减小体积、防被外部 dlopen 到 |
| `-fdata-sections` / `-ffunction-sections` | 每个函数/数据单独成节，配合 gc-sections 裁剪 |
| `-funwind-tables` | 生成栈回溯表（崩溃分析必需，第 16 章） |
| `-fstack-protector-strong` | 栈溢出检测（安全加固） |

```makefile
APP_LDFLAGS := -flto -Wl,--gc-sections -s
```

| 选项 | 作用 |
|---|---|
| `-flto` | 链接时优化（跨文件内联，能显著减小体积、提升性能） |
| `-Wl,--gc-sections` | 丢弃未被引用的节 |
| `-s` | **strip**：删除符号表 |

> [!warning] `-s` 会让崩溃无法定位
> strip 后 `addr2line` 找不到行号。
> 调试阶段注释掉 `-s`，或保留一份未 strip 的副本。
>
> （`addr2line` 是"把崩溃地址还原成源码行号"的工具，**第 30 章会讲**。）

## Android.mk 逐行（本项目）

```makefile
LOCAL_PATH := $(call my-dir)
```

必须第一行。把当前目录（jni/）路径存进 `LOCAL_PATH`。

```makefile
include $(CLEAR_VARS)
LOCAL_MODULE := embree_prebuilt
LOCAL_SRC_FILES := include/Embree/libembree4.a
include $(PREBUILT_STATIC_LIBRARY)
```

声明一个**预编译静态库模块**。四件套：
`CLEAR_VARS`（清空变量）→ 设 `LOCAL_MODULE`（名字）→ 设 `LOCAL_SRC_FILES`（路径）→ `PREBUILT_STATIC_LIBRARY`（声明类型）。

本项目声明了 6 个：embree、lexers、math、simd、sys、task。

```makefile
include $(CLEAR_VARS)
LOCAL_MODULE := Android_imgui_Vulkan.rc
```

主模块开始。`LOCAL_MODULE` 就是产物文件名。

```makefile
LOCAL_CFLAGS := -std=c17
LOCAL_CFLAGS += -fvisibility=hidden
LOCAL_CPPFLAGS := -std=c++20
LOCAL_CPPFLAGS += -fvisibility=hidden
LOCAL_CPPFLAGS += -fexceptions
LOCAL_CPPFLAGS += -frtti
```

C 用 c17，C++ 用 c++20。注意 `LOCAL_CPPFLAGS` 的 c++20 **覆盖**了 `Application.mk` 里的 c++17
（因为这是针对具体模块的设置，优先级更高）。

```makefile
LOCAL_CPPFLAGS += -DVK_USE_PLATFORM_ANDROID_KHR
LOCAL_CPPFLAGS += -DIMGUI_IMPL_VULKAN_NO_PROTOTYPES
LOCAL_CPPFLAGS += -DIMGUI_DISABLE_DEBUG_TOOLS
```

三个宏（第 10 章讲过 `-D` 等价于 `#define`）。

```makefile
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include/Android_draw
...
```

头文件搜索路径。加了这些，代码里就能写 `#include "draw.h"` 而不是完整路径。

```makefile
LOCAL_SRC_FILES := src/main.cpp
LOCAL_SRC_FILES += src/Android_draw/draw_Gui.cpp
...
```

源文件列表（相对于 `LOCAL_PATH`）。

```makefile
LOCAL_LDLIBS := -llog -landroid -lz
```

链接**系统动态库**：
- `-llog` → `liblog.so`（`__android_log_print`）
- `-landroid` → `libandroid.so`（`ANativeWindow`）
- `-lz` → `libz.so`（压缩）

```makefile
LOCAL_STATIC_LIBRARIES := \
    embree_prebuilt \
    lexers_prebuilt \
    math_prebuilt \
    sys_prebuilt \
    task_prebuilt \
    simd_prebuilt
```

链接静态库（第 28 章的顺序问题）。

```makefile
include $(BUILD_EXECUTABLE)
```

**关键**：指定产物类型。

| 指令 | 产物 |
|---|---|
| `BUILD_EXECUTABLE` | **可执行文件** ← 本项目 |
| `BUILD_SHARED_LIBRARY` | `.so` 动态库 |
| `BUILD_STATIC_LIBRARY` | `.a` 静态库 |

## 常用 ndk-build 变量速查

| 变量 | 用途 |
|---|---|
| `LOCAL_PATH` | 模块根目录 |
| `LOCAL_MODULE` | 模块名（产物名） |
| `LOCAL_SRC_FILES` | 源文件 |
| `LOCAL_C_INCLUDES` | 头文件路径 |
| `LOCAL_CFLAGS` | C 编译选项 |
| `LOCAL_CPPFLAGS` | C++ 编译选项 |
| `LOCAL_LDLIBS` | 链接系统库 |
| `LOCAL_STATIC_LIBRARIES` | 链接静态库 |
| `LOCAL_SHARED_LIBRARIES` | 链接动态库 |
| `LOCAL_LDFLAGS` | 链接选项 |

## 动手：写一个多目录工程的 Makefile

```
proj/
├── Makefile
├── include/utils.h
├── src/main.c
└── src/utils.c
```

要求：
- 支持 `make`（编译）、`make clean`、`make run`
- 头文件改动能触发重编译（`-MMD -MP`）
- 中间产物放 `build/`

写完后故意改一下 `utils.h` 的内容（加个空格也行），
看 `make` 是否重新编译——验证依赖是否生效。

## 验收清单

- [ ] 知道 Makefile 规则格式，以及命令行必须用 Tab（用空格缩进看 missing separator 报错）
- [ ] 会用 `$@` `$<` `$^` 和模式规则 `%.o: %.c`（写一条模式规则，编译多个 .c）
- [ ] 知道 `-MMD -MP` + `-include` 解决头文件依赖（改一个 .h，确认 .o 被重新编译）
- [ ] 能逐行解释本项目 `Application.mk` 的每个变量（逐行说出 APP_ABI/APP_PLATFORM/APP_STL 的作用）
- [ ] 知道 `BUILD_EXECUTABLE` / `BUILD_SHARED_LIBRARY` 的区别（说出"前者出可执行文件，后者出 .so"）
- [ ] 知道 `LOCAL_LDLIBS`（系统库）和 `LOCAL_STATIC_LIBRARIES`（静态库）的区别（说出各自链接什么）
- [ ] 完成了多目录 Makefile，且改头文件能触发重编译（make 成功，改 .h 后 make 只重编相关文件）

→ 下一章：[[第30章-nm-objdump-readelf实战]]　—— 掌握二进制分析三件套，能独立完成"崩溃地址 → 源码行"的定位。
