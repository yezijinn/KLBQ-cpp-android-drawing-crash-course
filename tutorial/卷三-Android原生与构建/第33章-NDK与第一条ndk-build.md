---
tags: [教程, 卷三, Android, NDK]
day: 22
aliases: [ch33]
---

# 第 33 章 · NDK 与第一条 ndk-build

> [!abstract] 本章目标
> 装好 NDK，用两条不同路线（ndk-build / clang 直编）编译出第一个 arm64 可执行文件。
> 本章的产物 `hello_arm64` 是总纲里承诺的第 1 个成果。

## 第一步：安装 NDK

> [!note] NDK 是什么
> **NDK = Native Development Kit**（原生开发工具包），谷歌提供的一套工具，
> 让你能用 C/C++ 写出在 Android 上运行的程序。装好它之后，你才有"交叉编译器"和"系统头文件"。


| 方式 | 分发形式 | 放哪里 | 说明 |
|---|---|---|---|
| **命令行工具包**（推荐） | 便携包（`.zip`） | `C:\dev\android-ndk-r27c` | 只下载 NDK，约 1~2GB，无需 Android Studio |
| Android Studio SDK Manager | `.exe` 安装包 | **用默认安装路径** | 有 IDE 的话最方便；装 Android Studio 时一路"下一步" |

> [!tip] 路径规矩沿用第 02 章
> **便携包 → `C:\dev\<软件名>`；`.exe` 安装包 → 默认路径不改。**
> 忘记为什么的话回看 [[第02章-装好第一套工具链]] 开头的"先定规矩：东西放哪里"。

命令行方式：

1. 打开 `https://developer.android.com/ndk/downloads`
2. 下载 Windows 版（如 `android-ndk-r27c-windows.zip`）
3. 解压到 `C:\dev\android-ndk-r27c`

> [!danger] 路径不要有中文和空格
> `C:\dev\android-ndk-r27c` 可以。
> `C:\开发工具\NDK` 会在后续构建时随机失败。
> 注意 NDK 解压后**目录名必须保持 `android-ndk-r27c` 这个形式**（含版本号），
> 因为 `ndk-build` 内部会按这个命名规则推导 `toolchains` 的位置。

NDK 目录结构（重要的几个）：

```
android-ndk-r27c/
├── ndk-build.cmd                              ← 构建入口
├── toolchains/llvm/prebuilt/windows-x86_64/
│   ├── bin/
│   │   ├── aarch64-linux-android21-clang.cmd   ← 编译器
│   │   ├── aarch64-linux-android-addr2line.exe
│   │   └── aarch64-linux-android-nm.exe
│   └── sysroot/                                ← 系统头文件和库
└── build/
```

## 第二步：设置环境变量

**三个终端的写法完全不同，按你在用的那个抄**：

```bash
# —— Git Bash（本教程默认）——
export NDK=/c/dev/android-ndk-r27c
export PATH=$NDK:$PATH
```

```bat
rem —— cmd（只对当前窗口有效）——
set NDK=C:\dev\android-ndk-r27c
set PATH=%NDK%;%PATH%
```

```powershell
# —— PowerShell（只对当前会话有效）——
$env:NDK = "C:\dev\android-ndk-r27c"
$env:PATH = "$env:NDK;$env:PATH"
```

> [!warning] 三种写法的关键差异，别记混
> | | 赋值 | 引用变量 | 路径分隔 |
> |---|---|---|---|
> | Git Bash | `export A=值` | `$A` 或 `${A}` | 冒号 `:`（且盘符写 `/c`）|
> | cmd | `set A=值` | `%A%` | 分号 `;` |
> | PowerShell | `$env:A = "值"` | `$env:A` | 分号 `;` |
>
> **最容易错的**：在 PowerShell 里用 `export`（不存在），或在 Git Bash 里用 `%NDK%`（不会展开）。
> 三个终端的完整对照表见 [[附录R-绝对零基础前置知识]] 第 3.2 节。

上面只是**临时**设置（关掉窗口就没了）。要长期生效，请按第 02 章的方法把它加进系统环境变量。

验证：

```bash
# Git Bash
ndk-build --version
# GNU Make 4.3
```

```powershell
# PowerShell
ndk-build --version
# GNU Make 4.3
```

## 路线 A：直接用 clang 编译（最快看到结果）

> [!note] 这段代码用到的两个新东西
> - `#include <android/log.h>` + `__android_log_print(...)`：**Android 的原生日志函数**，把日志写到 logcat。
>   `ANDROID_LOG_INFO` 是日志级别。第 38 章会系统讲日志；**这里先照抄**。
> - `#define LOGI(...) ...`：宏定义（第 10 章讲过 `#define`），把 `__android_log_print` 包一层，少打点字。

```c
// hello.c
#include <stdio.h>
#include <android/log.h>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "HelloNDK", __VA_ARGS__)

int main(void) {
    printf("hello from arm64\n");
    LOGI("logcat 也能看到我");
    return 0;
}
```

编译：

```bash
$NDK/toolchains/llvm/prebuilt/windows-x86_64/bin/aarch64-linux-android21-clang.cmd \
    hello.c -o hello_arm64 -llog
```

参数拆解：

| 部分 | 含义 |
|---|---|
| `aarch64` | 目标架构：64 位 ARM |
| `linux` | 目标系统 |
| `android21` | **最低 API 等级 21**（Android 5.0），数字可改 |
| `clang.cmd` | Windows 上是 .cmd，Linux/Mac 上无后缀 |
| `-llog` | 链接 `liblog.so`（用 `__android_log_print` 必需） |

> [!note] 路线 A 的 `21` 和路线 B 的 `android-25` 是什么关系？
> 它们是**同一个概念的两种写法**，都表示"最低支持到哪个 Android 版本"：
> - **路线 A**（直接 clang）：版本写在**编译器名字**里——`aarch64-linux-android`**`21`**`-clang`
> - **路线 B**（ndk-build）：版本写在 `Application.mk` 的 **`APP_PLATFORM := android-25`**
>
> 数字可以不同（这里 21 vs 25 只是示例），表示"支持的最低 API 等级"：
> - 数字**越小**，能在越多老设备上跑，但用的新 API 越少
> - 数字**越大**，能用越多新 API，但老设备跑不了
>
> **只要"编译时的最低版本" ≤ "设备的实际版本"，程序就能跑。** 后面的章节统一用 `android-25`。

验证产物：

> [!note] `file` 命令
> `file <文件>` 会**读取文件开头，判断它是什么类型**（ELF？脚本？图片？）以及架构。
> 卷二的 `xxd`（看字节）、`readelf`（看 ELF 结构）是同类工具，`file` 是最快的"一眼看是什么"。


```bash
file hello_arm64
# ELF 64-bit LSB executable, ARM aarch64, ...
```

## 第三步：push 到手机运行

```bash
adb push hello_arm64 /data/local/tmp/
adb shell chmod 755 /data/local/tmp/hello_arm64
adb shell /data/local/tmp/hello_arm64
```

预期输出：

```
hello from arm64
```

另开一个终端看 logcat：

```bash
adb logcat -s HelloNDK
```

应该能看到 `logcat 也能看到我`。

## 常见失败与解法

| 报错 | 原因 | 解法 |
|---|---|---|
| `permission denied` | 没加执行权限 | `chmod 755` |
| `not executable: 64-bit ELF` | 推到 32 位设备了 | 换设备或改 ABI |
| `No such file or directory` | 动态库缺失 | `readelf -d` 查 NEEDED |
| `error: only position independent executables (PIE) are supported` | 没开 PIE | 加 `-fPIE -pie` |

> [!note] Android 5.0+ 强制 PIE
> **PIE = Position Independent Executable**（位置无关可执行文件）：代码里不使用固定绝对地址，
> 而是靠相对偏移，这样程序能加载到任意位置（配合 ASLR 随机化）。
> 现代 NDK 默认开启 PIE，不用你手动加。
> 现代 NDK 默认开启 PIE。如果手动指定了 `-no-pie` 或用很老的编译器，
> 会报上面的错误。解决：加 `-fPIE -pie`。


> [!note] ABI 是什么
> **ABI = Application Binary Interface**（应用二进制接口）：规定"程序跑在什么 CPU 架构、用什么调用约定、二进制怎么摆"。
> 常见值：`arm64-v8a`（64 位 ARM）、`armeabi-v7a`（32 位 ARM）、`x86_64`（模拟器）。
> 它比"架构"含义更广——同样 64 位 ARM，不同的调用约定也算不同 ABI。第 39 章会专门讲。
## 路线 B：ndk-build（本项目的正式构建方式）

建目录结构：

```
hello_ndk/
└── jni/
    ├── Android.mk
    ├── Application.mk
    └── hello.c
```

**Application.mk**：

```makefile
APP_ABI := arm64-v8a
APP_PLATFORM := android-25
APP_STL := c++_static
APP_OPTIM := release
```

**Android.mk**：

```makefile
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := hello_ndk
LOCAL_SRC_FILES := hello.c
LOCAL_LDLIBS := -llog
include $(BUILD_EXECUTABLE)
```

**构建**：

```bash
cd hello_ndk
ndk-build
```

输出：

```
[arm64-v8a] Compile        : hello_ndk <= hello.c
[arm64-v8a] Executable     : hello_ndk
[arm64-v8a] Install        : hello_ndk => libs/arm64-v8a/hello_ndk
```

产物在 `libs/arm64-v8a/hello_ndk`。

## ndk-build 常用参数

```bash
ndk-build                      # 增量构建
ndk-build clean                # 清理
ndk-build -j8                  # 8 线程并行
ndk-build V=1                  # 显示完整命令（排错必备）
ndk-build NDK_DEBUG=1          # 生成调试版本
```

> [!tip] `V=1` 是排错神器
> 想知道编译器到底收到了哪些参数？`ndk-build V=1` 全部打印出来。
> 遇到"明明加了这个 flag 却没生效"，用 V=1 一眼看穿。

## 目录约定

```
工程/
├── jni/                 # 源码和构建脚本（ndk-build 找这个目录）
│   ├── Android.mk
│   ├── Application.mk
│   └── *.c/*.cpp
├── libs/                # 产物（ndk-build 生成）
│   └── arm64-v8a/
│       └── hello_ndk
└── obj/                 # 中间产物（.o 和 .d）
    └── local/arm64-v8a/
```

本项目的实际结构多了 `include/`（头文件）和 `src/`（源码），
通过 `LOCAL_C_INCLUDES` 和 `LOCAL_SRC_FILES` 指定（第 29、44 章）。

## 动手：编译两个版本对比

```bash
# release 版
ndk-build

# debug 版（改 Application.mk 的 APP_OPTIM := debug，或用命令行覆盖）
ndk-build APP_OPTIM=debug

# 比较大小
ls -l libs/arm64-v8a/hello_ndk obj/local/arm64-v8a/hello_ndk
```

debug 版会明显更大（带调试信息），且可以用 `addr2line` 精确定位（第 30 章）。

再试试：故意在 `hello.c` 里写个越界，用 debug 版复现崩溃，
用 `aarch64-linux-android-addr2line` 定位到行号。

## 验收清单

- [ ] NDK 装好，`ndk-build --version` 能输出
- [ ] 用 clang 直接编译出 `hello_arm64` 并在手机上运行成功 ★
- [ ] 知道 `aarch64-linux-android21-clang` 名字里 21 是什么意思
- [ ] 用 ndk-build 编译出 `hello_ndk`，产物在 `libs/arm64-v8a/`
- [ ] 会用 `adb logcat -s TAG` 过滤原生日志
- [ ] 知道 `ndk-build V=1` 和 `ndk-build clean`

→ 下一章：[[第34章-Android.mk与Application.mk逐行]]　—— 把 KLBQ 的两个 `.mk` 文件每一行都讲透，并学会四种常见改造场景。
