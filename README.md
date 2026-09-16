# KLBQ · 安卓内核绘制 C/C++ 入门路线拆解 新手学习教程


[![语言](https://img.shields.io/badge/language-C%2FC%2B%2B-blue)](https://en.wikipedia.org/wiki/C%2B%2B)
[![协议](https://img.shields.io/badge/license-MIT-green)](./LICENSE)
[![Stars](https://img.shields.io/github/stars/yezijinn/KLBQ-cpp-android-drawing-crash-course?style=social)](https://github.com/yezijinn/KLBQ-cpp-android-drawing-crash-course/stargazers)


## 这是什么

一套从零讲「如何自己写出一个安卓原生内存读取 + 图形覆盖层」的 **100 章图文教程**，配套可运行的 C/C++ 源码与自写 demo 引擎，专门面向完全零基础的读者。

## 功能特性 ✅

- ✅ **零基础友好**：从最底层的「计算机三层结构 / 命令行 / 进制 / 内存」讲起，不需要任何前置知识
- ✅ **真实项目驱动**：不是玩具例子，而是完整拆解一个安卓原生项目（覆盖层 + 跨进程读取 + 3D 数学 + 图形绘制）
- ✅ **可运行源码**：所有概念都配可编译、可离线验证的程序（如 `mathlib`、`w2s_demo`、`overlay`、`memview`、`coverlay`，以及一个自写的 demo 引擎）
- ✅ **系统化知识体系**：100 章正文 + 8 篇深挖 + 26 篇附录 + 英文词汇表，覆盖 C/C++、编译链接、系统进程、Android 原生、3D 数学、图形渲染、跨进程内存、物理可见性
- ✅ **Obsidian 优化**：全部用 Markdown 双链 + 标签写成，用 Obsidian 打开能自动生成关系图谱和进度看板

## 它和同类项目的不同（项目亮点）

- **不按字典顺序，按「出现回报率」排序**：英语词汇表直接统计项目源码里谁出现最多，先学回报最高的那批
- **不跳步**：每一步都给可直接复制的命令，并标注适用系统；专有名词第一次出现都附通俗解释
- **教学与工程一致**：教程讲的偏移、结构体、算法，和 `jni/` 里真实源码一一对应，学完就能读懂源码
- **合规边界清晰**：练习对象限定为「你自己写的 demo 程序」，安全合法，且能对照真值验证

## 适用场景

- 想学 **C/C++** 但被「从哪开始」劝退的纯小白
- 想理解 **安卓原生开发 / NDK / Vulkan / ImGui** 的读者
- 想搞懂 **跨进程内存读取、3D 数学（矩阵 / 四元数 / 投影）、图形覆盖层** 背后原理的人
- 需要一份 **离线、系统、可自查进度** 的学习路线的人

## 快速开始（三步走）

> 通俗解释：**构建（build）** = 把人能读懂的 C/C++ 代码，编译成手机能运行的机器码；**部署（deploy）** = 把编译好的程序传到手机上；**运行（run）** = 在手机上启动它。

### 第 0 步：准备环境（一次性）

| 你要装的东西 | 是什么（通俗解释） | 放哪里（推荐） |
|---|---|---|
| **w64devkit**（Windows 编译工具包） | 一个便携的「编译器 + 命令行终端」合集，解压即用 | `C:\dev\w64devkit` |
| **Android NDK** | 谷歌提供的、专门给安卓编译 C/C++ 的工具链（里面有 `ndk-build`） | 用 Android Studio 的 SDK Manager 装，**默认路径，不要改** |
| **platform-tools**（含 `adb`） | 电脑和安卓手机「说话」的工具（`adb` = Android Debug Bridge，安卓调试桥） | `C:\dev\platform-tools` |
| **Git** | 版本管理 + 上传到 GitHub 的工具 | 用 `.exe` 安装包，**默认路径** |
| **VS Code**（可选但推荐） | 写代码的编辑器 | 用 `.exe` 安装包，**默认路径** |

> 详细安装图文步骤见教程 `tutorial/附录R-绝对零基础前置知识.md`（假设读者只会用鼠标）。

### 第 1 步：拿到代码

```bash
# 【Git Bash / macOS / Linux 终端】把仓库下载到本地
git clone https://github.com/yezijinn/KLBQ-cpp-android-drawing-crash-course.git
cd KLBQ
```

```powershell
# 【Windows PowerShell】等价于上面。更推荐用 Git Bash（教程里大部分命令以它为准）
git clone https://github.com/yezijinn/KLBQ-cpp-android-drawing-crash-course.git
cd KLBQ
```

### 第 2 步：编译（用 NDK）

```bash
# 【Git Bash】进入 jni 目录，用 ndk-build 编译
cd jni
# 下面的命令假设你在 jni/ 下准备了 Android.mk / Application.mk
ndk-build NDK_PROJECT_PATH=. APP_BUILD_SCRIPT=./Android.mk
```

> 如果编译报错 `ndk-build: command not found`，说明 NDK 没加进 PATH。把 NDK 里的 `prebuilt/.../bin` 路径加进系统环境变量 PATH 后，重新开一个终端再试。

### 第 3 步：部署并运行（需要一台安卓手机）

```bash
# 【Git Bash】用 adb 把编译出的程序推到手机，并运行
adb push libs/arm64-v8a/你的程序 /data/local/tmp/你的程序
adb shell chmod +x /data/local/tmp/你的程序
adb shell /data/local/tmp/你的程序
```

> ⚠️ **合规提醒**：上面这套「读取内存 + 画覆盖层」的能力，请**只在你自有或已授权的设备、仅作学习**时使用。不要对不拥有的应用使用，不要违反任何服务条款或法律。

## 使用示例（带注释）

下面用「把世界坐标算到屏幕上（World-To-Screen）」这个最核心的数学步骤举例（代码来自教程配套的 `mathlib`）：

```cpp
// w2s_demo 里的核心函数：把一个 3D 世界坐标，变成 2D 屏幕坐标
// cameraPos = 相机在哪；target   = 你想标点的世界坐标
Vec2 world_to_screen(Vec3 cameraPos, Vec3 target, Mat4 view, Mat4 proj, Vec2 screen)
{
    Vec4 clip = proj * view * Vec4(target, 1.0f);        // 先乘视图矩阵，再乘投影矩阵
    if (clip.w <= 0.0f) return {0, 0};                   // w<=0 说明在相机背后，不画
    Vec3 ndc = clip.xyz / clip.w;                        // 除以 w，得到标准化设备坐标(NDC)
    float x = (ndc.x * 0.5f + 0.5f) * screen.x;         // 映射到屏幕 X
    float y = (1.0f - (ndc.y * 0.5f + 0.5f)) * screen.y; // 映射到屏幕 Y（Y 轴翻转）
    return {x, y};
}
```

> 看不懂没关系——这正是教程 `卷四（3D 数学）` 和 `第 51 章 W2S` 会带你从零推导的内容。

## 目录结构（仓库长这样）

```
KLBQ/
├── README.md            # 你正在看的文件
├── LICENSE              # MIT 开源协议
├── .gitignore           # 忽略本地配置与编译产物
├── PROJECT_ANALYSIS.md  # 项目源码整体分析
├── jni/                 # C/C++ 实际源码（覆盖层 / 内存读取 / demo 引擎 / 各示例）
└── tutorial/            # 152 篇 Markdown 教程
    ├── 00-开始之前.md    # 总纲：100 章目录 + 60 天学习日程 + Obsidian 用法
    ├── 卷一-编程地基/     # 第 01–16 章
    ├── 卷二-系统与底层/   # 第 17–30 章
    ├── …（共八卷）
    ├── 深挖/             # 8 篇进阶内容
    ├── 英文词汇/          # 词频分级英语词汇表（W0–W7，带音标 + 中文翻译 + 源码例句）
    └── …（附录 A–V，含 R 绝对零基础前置、P 重建路线、Q 动手清单、T 进度看板）
```

## 新手最常踩的坑（FAQ）

**Q1：我完全不会编程，能学吗？**
能。教程开头 `附录R` 假设读者「只会用鼠标」，从文件、命令行、进制、内存讲起。先把 `附录R` 的 21 项自测清单全勾上，再进 `卷一`。

**Q2：`ndk-build` 提示找不到命令？**
NDK 没加进系统 PATH。把 NDK 里的 `prebuilt/.../bin` 加进 PATH，或改用完整路径调用；改完后**重新开一个终端**再编译。

**Q3：为什么教程里的「练习对象」是我自己写的 demo，而不是某个游戏？**
两个原因：① 合法安全，你对自己的程序有完全权利；② 你能「对答案」——demo 引擎里的值是已知的，方便验证你读得对不对。真实偏移 / 自瞄实现教程**故意不提供**。

**Q4：用 Obsidian 打开教程，双链和图谱怎么用？**
直接把整个 `tutorial/` 文件夹拖进 Obsidian 新建的仓库即可。`.md` 文件里的 `[[双链]]` 会自动跳转，关系图谱会自动画出章节关联。`附录T` 还教你怎么用 Dataview 做学习进度看板。

**Q5：编译出来的 `.so` / `obj` 要提交到 GitHub 吗？**
不要。它们都是编译产物，已写进 `.gitignore` 自动忽略；只提交源码，别人拉下来自己编译即可。

## 报错排查（常见错误 + 解决办法）

| 报错 / 现象 | 大概率原因 | 解决办法 |
|---|---|---|
| `ndk-build: command not found` | NDK 未加入 PATH | 把 NDK 的 `prebuilt/.../bin` 加进 PATH 后，用新开的终端重试 |
| `error: cannot find -lxxx` | 链接时找不到某个库 | 检查 `Android.mk` 的 `LOCAL_LDLIBS` / `LOCAL_STATIC_LIBRARIES` 是否写全 |
| `adb: device not found` | 手机没连上 / 没开 USB 调试 | 重新插线、手机确认「允许调试」；用 `adb devices` 看是否识别 |
| `Permission denied`（运行程序时） | 没给执行权限 | `adb shell chmod +x /data/local/tmp/你的程序` |
| 仓库变慢 / 体积巨大 | 不小心把 `libs/`、`obj/` 提交了 | 确认 `.gitignore` 已忽略它们；已误提交可用 `git rm -r --cached libs obj` 移除跟踪 |

## 配套资源

- **总纲入口**：`tutorial/00-开始之前.md`
- **零基础前置**：`tutorial/附录R-绝对零基础前置知识.md`
- **动手任务清单**：`tutorial/附录Q-动手任务总清单.md`（100 个可打勾任务 + 6 个里程碑）
- **从零重建路线图**：`tutorial/附录P-从零重建路线图.md`（8 阶段 × 8 里程碑的施工顺序）
- **英文词汇表**：`tutorial/英文词汇/`（词频分级，带音标 + 中文翻译 + 源码例句）

## 许可证

本项目以 **MIT 协议** 开源。完整文本见 [LICENSE](./LICENSE)。
