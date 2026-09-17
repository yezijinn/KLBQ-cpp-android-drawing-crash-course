---
tags: [教程, 卷三, 工程, 规范]
day: 27
aliases: [ch44]
---

# 第 44 章 · 工程目录与 include 约定

> [!abstract] 本章目标
> 建立一套能长期维护的目录规范，并复刻本项目的组织结构。
> 卷三收官章，从这里开始你会持续往工程里加文件。

> [!note] 承上
> 卷三从"编译第一个程序"走到"引入第三方库"。
> 本章收官：**把这一整套组织成能长期维护的目录规范**，后面卷四~卷八都在这个骨架上加代码。

## 先看本项目的实际结构

```
KLBQ/
├── jni/
│   ├── Android.mk                    构建脚本
│   ├── Application.mk
│   ├── include/                      所有头文件
│   │   ├── Android_draw/   draw.h WorldToScreen.h Draw_ESP.h
│   │   ├── Hack/           Hack.h SilentAim.h PovAim.h TouchAim.h
│   │   ├── My_Utils/       MemDriver.h SysHal.h ConfigManager.h
│   │   │                   GameTools.h VectorTools.h
│   │   ├── Embree/         PhysX.h rtcore*.h foundation/ *.a
│   │   ├── ImGui/          imgui*.h ANativeWindowCreator.h TouchHelperA.h
│   │   └── Vulkan/         vulkan_wrapper.h VulkanGraphics.h GraphicsManager.h
│   └── src/                          所有源文件
│       ├── main.cpp
│       ├── Android_draw/   draw_Gui.cpp Draw_ESP.cpp driver.h variable.h
│       ├── Hack/           Hack.cpp
│       ├── My_Utils/       GameTools.cpp ConfigManager.cpp SysHal.cpp
│       ├── ImGui/          imgui*.cpp AndroidImgui.cpp TouchHelperA.cpp
│       │                   my_imgui_impl_android.cpp heiti_ttf.cpp
│       └── Vulkan/         GraphicsManager.cpp VulkanGraphics.cpp vulkan_wrapper.cpp
├── libs/arm64-v8a/                   产物
├── obj/local/arm64-v8a/              中间产物
└── .vscode/                          编辑器配置
```

## 三条核心约定

### 1. include/ 与 src/ 镜像对应

```
include/My_Utils/GameTools.h    ↔    src/My_Utils/GameTools.cpp
include/Hack/SilentAim.h        ↔    （头文件实现，无 .cpp）
```

好处：看到一个 `.h` 就知道去哪找它的实现。

### 2. 按功能分层，不按类型分层

```
✓ 正确：                        ✗ 错误：
include/                        include/
├── Vulkan/                     ├── headers/
│   ├── VulkanGraphics.h        │   ├── VulkanGraphics.h
│   └── GraphicsManager.h       │   ├── draw.h
├── ImGui/                      │   └── GameTools.h
└── Hack/                       src/
                                ├── cpp/
                                └── h/
```

按功能分：改 Vulkan 相关时只碰一个目录。

### 3. 公共接口放 include/，内部实现细节放 src/

本项目有个例外：`src/Android_draw/driver.h` 是"全局驱动接口"，
却放在 `src/` 下（因为它在 `LOCAL_C_INCLUDES` 里被显式加入了）：

```makefile
LOCAL_C_INCLUDES += $(LOCAL_PATH)/src/Android_draw
```

> [!warning] 这是个历史包袱
> `driver.h` 是项目最重要的公共接口之一，理应放 `include/`。
> 现在它能用是因为额外加了 include 路径。
> **你自己建工程时不要学这个**——统一放 `include/`。

## 命名约定

| 类型 | 约定 | 例子 |
|---|---|---|
| 头文件 | 大驼峰或下划线 | `GameTools.h`、`draw.h` |
| 源文件 | 与头同名 | `GameTools.cpp` |
| 类 | 大驼峰 | `VulkanGraphics`、`MemDriver` |
| 命名空间 | 大驼峰 | `GameTools`、`SilentAim` |
| 全局变量 | 见下 | |
| 宏 | 全大写下划线 | `PAGE_SIZE`、`BP_SET_MASK` |
| 常量 | `constexpr` 小写或 `k` 前缀 | `kMaxCount` |

> [!note] `k` 前缀的来历
> `kMaxCount` 里的 `k` 没有语法含义，是一种**命名风格**（源自 Google C++ 风格指南）：
> 用 `k` 开头表示"这是个编译期常量"（k = constant 的首字母），一眼和普通变量区分开。
> 用不用随你，**关键是全项目统一**：要么都 `kXxx`，要么都 `XXX_MAX`，别混。

**本项目用了大量中文标识符**（第 15 章讲过）：

```cpp
long int 玩家相机 = 0;
Vector3A 相机位置;
bool 初始化 = false;
```

是否采用取决于你。如果采用，**全项目统一**，别一半中文一半英文。

## 编码约定：UTF-8 无 BOM

> [!danger] BOM 会让 GCC 报错
> ```
> error: stray '\357' in program
> ```
> 这是 UTF-8 BOM 的 `EF BB BF`。

设置（VS Code `settings.json`）：

```json
{
  "files.encoding": "utf8",
  "files.eol": "\n",
  "files.trimTrailingWhitespace": true,
  "files.insertFinalNewline": true
}
```

已有文件转码：

```bash
# 去掉 BOM（Git Bash）
sed -i '1s/^\xEF\xBB\xBF//' file.cpp
```

## include 的写法

```cpp
// 自己的头文件：用引号，路径从 include 根开始
#include "draw.h"
#include "Hack.h"
#include "MemDriver.h"

// 系统/第三方：用尖括号
#include <cstdio>
#include <vector>
#include <android/log.h>
```

因为 `LOCAL_C_INCLUDES` 里加了每个子目录，所以不用写 `Android_draw/draw.h`。

**好处**：移动文件时不用改所有 include。
**代价**：不同目录下不能有同名文件（本项目 `draw.h` 只有一个）。

## 防止循环包含

```
draw.h  ←─────┐
  │            │
  ↓            │
variable.h ────┘   （如果 variable.h 也 include draw.h）
```

解法：
1. 所有头文件都加 `#pragma once`
2. 尽量用前向声明代替包含：

```cpp
// 不需要完整定义时
struct ImDrawList;          // 前向声明，不用 #include "imgui.h"

void DrawPlayer(ImDrawList *draw);
```

> [!note] 为什么只写一行 `struct ImDrawList;` 就够了
> 因为编译器处理到 `void DrawPlayer(ImDrawList *draw);` 时，**只需要知道"ImDrawList 是一个类型名"**，
> 不需要知道它内部长什么样——参数是个**指针**，指针大小固定（8 字节），跟指向的类型细节无关。
>
> 什么时候必须真 `#include`：**用到类型的内部**时，比如访问 `draw->CmdBuffer`（取成员）、
> `sizeof(ImDrawList)`（求大小）、或定义该类型的对象。这时光有"名字"不够，得有完整定义。

本项目 `draw.h` 和 `Draw_ESP.h` 就是这么做的。

## 一份推荐的目录模板

```
myproject/
├── .vscode/
│   ├── settings.json
│   ├── c_cpp_properties.json
│   └── launch.json
├── jni/
│   ├── Android.mk
│   ├── Application.mk
│   ├── include/
│   │   ├── Core/         核心：配置、日志、工具
│   │   ├── Platform/     平台相关：窗口、输入
│   │   ├── Render/       渲染：Vulkan/ImGui
│   │   ├── Memory/       跨进程读写
│   │   └── ThirdParty/   第三方头文件
│   └── src/
│       ├── main.cpp
│       ├── Core/
│       ├── Platform/
│       ├── Render/
│       └── Memory/
├── scripts/
│   ├── build.sh
│   ├── push.sh
│   └── run.sh
├── docs/                  设计文档
├── libs/                  产物（gitignore）
└── obj/                   中间产物（gitignore）
```

## .gitignore

```gitignore
libs/
obj/
*.o
*.d
*.a
!jni/include/**/*.a        # 第三方 .a 要保留
*.exe
build/
.vscode/ipch/
```

## 脚本目录

把第 35 章的一键脚本固化下来：

```bash
#!/bin/bash
# scripts/build.sh
set -e
cd "$(dirname "$0")/.."
ndk-build -j8 "$@"
```

```bash
#!/bin/bash
# scripts/run.sh
# 注意：这里刻意不用 set -e——理由见第 35 章「run.sh 的两个坑」：
# 出错的 adb 命令若因 set -e 直接退出，你反而看不到任何错误提示。
TARGET=${1:-app}
REMOTE=/data/local/tmp/$TARGET

./scripts/build.sh || { echo "编译失败"; exit 1; }
adb push libs/arm64-v8a/$TARGET $REMOTE || { echo "推送失败（设备连接？）"; exit 1; }
adb shell chmod 755 $REMOTE
adb logcat -c
adb shell "$REMOTE" &          # 常驻程序要后台跑（见第 35 章坑 2）
sleep 1
adb logcat -d -s $TARGET | tail -30
```

## 文档约定

每个模块目录下放一个 `README.md`，写清楚：
- 这个模块负责什么
- 关键数据结构
- 对外接口
- 已知问题

3 个月后你会感谢自己。

## 动手：搭一个规范的工程骨架

```bash
mkdir -p myproject/{jni/{include/{Core,Platform,Render,Memory},src/{Core,Platform,Render,Memory}},scripts,docs,.vscode}

# 创建 Android.mk / Application.mk
# 创建 main.cpp（先只打印一句话）
# 创建 .gitignore
# 创建 scripts/build.sh 和 run.sh
# 编译并运行
```

要求：
1. `ndk-build` 能通过
2. `./scripts/run.sh` 能一键跑
3. 加一个新模块（比如 `Memory/`）时，知道要改哪几个地方

## 卷三收官

| 章 | 能力 |
|---|---|
| 31 | Android 五层结构、关键系统服务 |
| 32 | 可执行文件 vs APK 的架构决策 |
| 33 | NDK 安装、clang 直编、ndk-build 首条命令 |
| 34 | 本项目 .mk 脚本逐行精读 |
| 35 | push → chmod → 运行 → 抓日志的工作流 |
| 36 | adb 全套命令 |
| 37 | 三层权限、SELinux、avc 日志 |
| 38 | 分级日志系统、string_view 打印 |
| 39 | 交叉编译 8 大坑 |
| 40 | c++_static 与 STL 取舍 |
| 41 | 四件套体积裁剪 |
| 42 | 引入 ImGui 源码 |
| 43 | 引入 Embree 静态库 |
| 44 | 目录规范与工程约定 |

> [!success] 卷三综合练习
> 建一个完整工程，包含：
> 1. 规范的目录结构（include/src 镜像）
> 2. ImGui 源码 + 一个最小 Embree 库
> 3. 分级日志模块（第 38 章）
> 4. 一键构建/推送/运行脚本
> 5. 能在手机上跑起来并输出日志
>
> 这个工程就是后面 56 章的基础，别跳过。

## 动手验证清单

- [ ] **建镜像目录**：`include/` 与 `src/` 按功能分层（Vulkan/ImGui/Hack/My_Utils）
- [ ] **放一份 .h + .cpp**：如 `include/My_Utils/X.h` ↔ `src/My_Utils/X.cpp`
- [ ] **写 .gitignore**：排除 `libs/` `obj/`
- [ ] **验证 git 忽略**：`git status` → 看不到 `libs/obj`
- [ ] **编码统一**：确认源文件是 UTF-8 无 BOM（`file X.cpp`）
- [ ] **一键脚本**：`scripts/build.sh` + `scripts/run.sh`
- [ ] **完整构建**：从空目录按规范搭建并 `make` 成功

> [!tip] 这个骨架就是卷四~卷八的工作目录
> 第 44 章的综合练习产出，后面 56 章都在这个结构上继续加代码。

> [!warning] 搭错了想重来？
> 目录结构搭乱、`make` 报奇怪的错时，**不用推倒重来**：
> 1. `ndk-build clean`（清产物）→ 再 `ndk-build` 看是否恢复
> 2. 还不行就 `rm -rf obj/ libs/` 删掉中间产物，重新构建
> 3. 只在**目录结构本身错乱**时，才按上面清单重新搭一次
>
> 保留 `jni/` 里你的源码——重建只针对产物目录，别删源码。

## 验收清单

- [ ] 能画出本项目的目录结构，并说出每个目录的作用（画 jni/include/src/libs/obj 并标注）
- [ ] 知道 include/src 镜像、按功能分层的约定（说出"看到 .h 就知道 .cpp 在哪"）
- [ ] 知道源文件必须 UTF-8 无 BOM（用带 BOM 文件编译看 stray 报错）
- [ ] 会用前向声明减少 include 依赖（写 `struct X;` 代替 #include，编译通过）
- [ ] 建好了自己的工程骨架，能一键构建运行（`./scripts/run.sh` 跑通）
- [ ] 写了 .gitignore，libs/obj 不进版本库（`git status` 看不到 libs/obj）

→ 下一卷：[[卷四-本卷导航]] · [[第45章-向量]]　—— 从"箭头"到代码，掌握向量的四种运算，并理解它们在游戏里的实际含义。
