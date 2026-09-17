---
tags: [教程, 卷三, Android, 构建]
day: 22
aliases: [ch34]
---

# 第 34 章 · 本项目的构建脚本逐行精读

> [!abstract] 本章目标
> 把 KLBQ 的两个 `.mk` 文件每一行都讲透，并学会四种常见改造场景。
> 第 29 章讲语法，本章讲实战。

> [!note] 承上
> 上一章用 `ndk-build` 编出了第一个程序。
> 本章把背后的两个构建脚本——**`Android.mk` 和 `Application.mk`**——逐行讲透。

## 完整的 Android.mk

```makefile
LOCAL_PATH := $(call my-dir)
```

`my-dir` 返回**当前 .mk 文件所在目录**。因为 `Android.mk` 在 `jni/`，
所以 `LOCAL_PATH` = `<工程>/jni`。

> [!danger] `$(call my-dir)` 必须在 include 其它 .mk 之前调用
> 因为它返回"最后被 include 的文件所在目录"。
> 如果在 include 之后调用，会返回错误路径。
> 本项目所有 `include $(CLEAR_VARS)` 都在后面，符合规范。

### 预编译库块

```makefile
include $(CLEAR_VARS)
LOCAL_MODULE := embree_prebuilt
LOCAL_SRC_FILES := include/Embree/libembree4.a
include $(PREBUILT_STATIC_LIBRARY)
```

> [!note] `$(CLEAR_VARS)` 到底是什么
> 它不是普通变量，而是 **ndk-build 预置的一个"脚本片段路径"**——内部其实是 `build/core/clear_vars.mk`。
> `include $(CLEAR_VARS)` 相当于"把那个文件包含进来执行"，而那个文件的内容就是"把一堆 `LOCAL_*` 变量清空"。
> 这也解释了为什么写 `$(CLEAR_VARS)`（取值）而不是直接写名字——**make 需要先取出这个路径，再 include 它**。
>
> 为什么要清空：ndk-build 用同一组 `LOCAL_*` 变量描述"当前模块"。定义新模块前不清空，就会把上一个模块的设置带进来。

`CLEAR_VARS` 会清空除 `LOCAL_PATH` 外的所有 `LOCAL_*` 变量。
所以每定义一个模块，都要先 `CLEAR_VARS`。

预编译库的三要素：

| 变量 | 值 | 说明 |
|---|---|---|
| `LOCAL_MODULE` | `embree_prebuilt` | 引用时用的名字 |
| `LOCAL_SRC_FILES` | 相对于 `LOCAL_PATH` 的路径 | |
| `include` | `PREBUILT_STATIC_LIBRARY` | 类型：预编译静态库 |

对应的动态库版本是 `PREBUILT_SHARED_LIBRARY`。

本项目声明了 6 个预编译库，注意 **`simd_prebuilt` 排在最后**——
第 28 章讲过，被依赖的库要放右边。

### 主模块

```makefile
include $(CLEAR_VARS)
LOCAL_MODULE := Android_imgui_Vulkan.rc
```

产物名就叫 `Android_imgui_Vulkan.rc`（带 `.rc` 后缀是我项目的命名习惯，无特殊含义）。

```makefile
LOCAL_CFLAGS := -std=c17
LOCAL_CFLAGS += -fvisibility=hidden
LOCAL_CPPFLAGS := -std=c++20
LOCAL_CPPFLAGS += -fvisibility=hidden
LOCAL_CPPFLAGS += -fexceptions
LOCAL_CPPFLAGS += -frtti
```

注意 `Application.mk` 里也写了 `-std=c++17`。**模块级设置优先**，所以实际是 c++20。
这种"两处都写、值不同"的情况应当避免——建议统一。

```makefile
LOCAL_CPPFLAGS += -DVK_USE_PLATFORM_ANDROID_KHR
LOCAL_CPPFLAGS += -DIMGUI_IMPL_VULKAN_NO_PROTOTYPES
LOCAL_CPPFLAGS += -DIMGUI_DISABLE_DEBUG_TOOLS   # 禁用imgui调试工具
```

| 宏 | 为什么需要 |
|---|---|
| `VK_USE_PLATFORM_ANDROID_KHR` | 让 `vulkan.h` 声明 `vkCreateAndroidSurfaceKHR` 等 Android 专有函数 |
| `IMGUI_IMPL_VULKAN_NO_PROTOTYPES` | ImGui 不直接调 Vulkan 函数，改为保存函数指针，由我们注入（配合动态加载） |
| `IMGUI_DISABLE_DEBUG_TOOLS` | 去掉 ImGui 的调试/指标窗口，减小体积 |

```makefile
LOCAL_ASFLAGS += -I$(LOCAL_PATH)/include
```

汇编文件的 include 路径（本项目没有 .S 文件，这行是冗余的，但无害）。

```makefile
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include/Android_draw
...
```

10 个头文件路径。加了之后源码里可以直接 `#include "draw.h"`。

> [!tip] 用 `$(LOCAL_PATH)` 而不是相对路径
> 相对路径是相对于**执行 ndk-build 的目录**，换个目录执行就崩。
> `$(LOCAL_PATH)` 永远正确。

```makefile
LOCAL_SRC_FILES := src/main.cpp
LOCAL_SRC_FILES += src/Android_draw/draw_Gui.cpp
...（共 20 个）
```

字体是 `src/ImGui/heiti_ttf.cpp` 里的一个**大字节数组**（`g_heiti_ttf_data[]`），
直接编译进程序——所以工程里**没有单独的 `.ttf` 字体文件**。

```makefile
LOCAL_LDLIBS := -llog -landroid -lz
```

系统动态库。**注意顺序在这里不重要**（系统库总是最后链接，且循环依赖会被处理）。
（这和 [[第28章-静态库与链接顺序]] 讲的不矛盾——**第28章讲的是静态库 `.a`，顺序很重要**；这里 `-lxxx` 是系统动态库，规则不同。）

```makefile
LOCAL_STATIC_LIBRARIES := \
    embree_prebuilt \
    lexers_prebuilt \
    math_prebuilt \
    sys_prebuilt \
    task_prebuilt \
    simd_prebuilt
```

顺序重要（第 28 章）。

```makefile
include $(BUILD_EXECUTABLE)
```

产物类型：可执行文件。

## 完整的 Application.mk

```makefile
APP_ABI := arm64-v8a
APP_PLATFORM := android-25
APP_STL := c++_static
APP_OPTIM := release
APP_SHORT_COMMANDS := true
```

| 变量 | 本项目值 | 为什么 |
|---|---|---|
| `APP_ABI` | `arm64-v8a` | 目标设备是 64 位；Embree 的 .a 也只有这个架构 |
| `APP_PLATFORM` | `android-25` | API 25 = Android 7.1，覆盖了绝大多数在用设备 |
| `APP_STL` | `c++_static` | 静态链接，产物自包含，不用带 `libc++_shared.so` |
| `APP_OPTIM` | `release` | 发布模式（`-O2` + `NDEBUG`） |
| `APP_SHORT_COMMANDS` | `true` | Windows 命令行长度限制保护 |

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

`-fvisibility=hidden` 的作用：
默认所有符号都导出 → 符号表巨大、可能被外部 dlopen 到。
加上之后只有 `__attribute__((visibility("default")))` 标记的才导出。
对可执行文件（不需要导出符号）来说是纯收益。

`-fdata-sections -ffunction-sections` 配合 `-Wl,--gc-sections`：
每个函数/变量单独成节 → 链接时未被用到的节被丢弃 → 体积减小。

```makefile
APP_LDFLAGS := -flto -Wl,--gc-sections -s
```

三个都是"减小体积 + 提速"，但 `-s`（strip）会让崩溃无法定位。

## 调试期建议的修改

```makefile
# Application.mk 调试配置
APP_OPTIM := debug                    # -O0 + 调试信息

# 把 -s 注释掉
APP_LDFLAGS := -flto -Wl,--gc-sections
#              ↑ 去掉 -s

# Android.mk 里加
LOCAL_CPPFLAGS += -DDEBUG
LOCAL_CPPFLAGS += -g
```

改完 `ndk-build clean && ndk-build`，产物会大很多，但：
- `addr2line` 能定位到行
- 变量不会被优化掉
- 断言生效（没定义 NDEBUG）

## 四种常见改造场景

### 1. 加一个新的源文件

```makefile
LOCAL_SRC_FILES += src/My_Utils/NewTool.cpp
```

然后 `ndk-build`（增量，只编译新增的）。

### 2. 加一个新的头文件目录

```makefile
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include/NewModule
```

### 3. 引入一个新的第三方静态库

```makefile
# 在文件开头（主模块之前）加
include $(CLEAR_VARS)
LOCAL_MODULE := newlib_prebuilt
LOCAL_SRC_FILES := include/NewLib/libnewlib.a
include $(PREBUILT_STATIC_LIBRARY)

# 然后在主模块的 LOCAL_STATIC_LIBRARIES 里加
LOCAL_STATIC_LIBRARIES += newlib_prebuilt
```

### 4. 加一个条件编译开关

```makefile
ifeq ($(TARGET_ARCH_ABI),arm64-v8a)
    LOCAL_CPPFLAGS += -DUSE_NEON
endif
```

`TARGET_ARCH_ABI` 是 ndk-build 自动设置的变量，当前正在编译的 ABI。

## 调试构建脚本的技巧

```bash
# 看所有变量的值
ndk-build V=1 2>&1 | head -50

# 打印某个变量（在 Android.mk 里加）
$(warning LOCAL_PATH = $(LOCAL_PATH))
$(warning SRC = $(LOCAL_SRC_FILES))
```

`$(warning ...)` 会在构建时打印到 stderr，是 Makefile 的 "printf"。

```bash
# 只编译某个 ABI
ndk-build APP_ABI=arm64-v8a

# 强制重新编译
ndk-build -B
```

## 常见错误对照

| 报错 | 原因 | 解法 |
|---|---|---|
| `missing separator` | 用了空格缩进 | 改成 Tab（第 29 章） |
| `undefined reference to rtcXXX` | Embree 库没链/顺序错 | 检查 `LOCAL_STATIC_LIBRARIES` |
| `fatal error: xxx.h: No such file` | 缺 include 路径 | 加 `LOCAL_C_INCLUDES` |
| `Android NDK: Could not find application project directory` | 不在工程根目录执行 | `cd` 到有 `jni/` 的目录 |
| `error: undefined reference to 'std::xxx'` | STL 不一致 | 统一 `APP_STL` |
| 编译很慢 | LTO 开销 | 调试时去掉 `-flto` |

## 动手：给示例工程加一个模块

在 `hello_ndk/jni/` 里：

1. 新建 `src/utils.c` 和 `include/utils.h`
2. 修改 `Android.mk`：
   - 加 `LOCAL_C_INCLUDES += $(LOCAL_PATH)/include`
   - 加 `LOCAL_SRC_FILES += src/utils.c`
3. `ndk-build`，确认编译通过
4. 用 `ndk-build V=1` 看一下实际传给 clang 的参数

再试一次：故意把 `LOCAL_SRC_FILES` 写成 `utils.c`（漏了 `src/`），
看报什么错——这个错误你以后会经常遇到。

## 动手验证清单

- [ ] **读懂 `LOCAL_PATH`**：找到 `$(call my-dir)` → 指出它返回 `jni/` 目录
- [ ] **数预编译库块**：`grep -c PREBUILT_STATIC_LIBRARY Android.mk` → 应为 6
- [ ] **看模块名**：`grep LOCAL_MODULE Android.mk` → 看到 `embree_prebuilt` 等
- [ ] **确认产物类型**：`grep BUILD_ Android.mk` → 末行是 `BUILD_EXECUTABLE`
- [ ] **验证 c++20 生效**：`grep -n 'c++' Android.mk Application.mk` → 模块级 `c++20` 覆盖应用级 `c++17`

## 验收清单

- [ ] 能解释本项目 `Android.mk` 每一段的作用（逐段说出 CLEAR_VARS/预编译库/主模块/链接 的作用）
- [ ] 知道 `$(call my-dir)` 必须在最前面（说出"它返回最后 include 的文件目录"）
- [ ] 知道预编译静态库的三要素和引用方式（说出 LOCAL_MODULE/LOCAL_SRC_FILES/PREBUILT_STATIC_LIBRARY）
- [ ] 能说出 `-fvisibility=hidden` 和 `-ffunction-sections` 的作用（前者隐藏符号，后者每函数独立节供裁剪）
- [ ] 知道调试时要改哪三处（APP_OPTIM / -s / -g）（改完重编，产物变大、addr2line 可用）
- [ ] 会在 .mk 里加源文件、include 路径、预编译库（各加一次并编译通过）
- [ ] 完成动手练习，并见过"源文件路径写错"的报错（漏写 src/ 看报错）

→ 下一章：[[第35章-push到手机并运行]]　—— 建立一套稳定的"编译 → 推送 → 运行 → 看日志"工作流，并写成脚本。
