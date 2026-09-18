---
tags: [教程, 卷三, Android, 架构]
day: 21
aliases: [ch32]
---

# 第 32 章 · 为什么是可执行文件而非 APK

> [!abstract] 本章目标
> 理解本项目的形态选择：独立 native 可执行文件 vs APK 里的 .so。
> 这是架构决策，决定了后面所有技术路线。

> [!note] 承上
> 上一章看清了 Android 的分层。
> 本章回答一个架构决策：**我们的程序做成 APK，还是独立可执行文件？**

## 先看两种形态的差别

```
【形态 A】APK 里的 .so
  App(Java) → JNI → libnative.so
  进程 UID = u0_a123（App 沙箱）
  启动：点图标 / am start
  权限：受 Android 权限模型约束
  
【形态 B】独立可执行文件                    ← 本项目
  /data/local/tmp/klbq
  进程 UID = 执行者（su 下为 0/root）
  启动：adb shell ./klbq  或  shell 脚本
  权限：Linux 文件权限 + SELinux
```

> [!note] 上面出现的 `JNI` 是什么
> **JNI = Java Native Interface**（Java 原生接口）：Android 里 **Java 代码调用 C/C++ 代码的桥梁**。
> 普通 App 想跑原生代码，必须通过 JNI（Java 层声明一个 `native` 方法，C++ 层实现对应函数）。
> **本项目的可执行文件不走 JNI**——它直接以原生程序运行，不需要 Java 层。这正是形态 B 的便利之一。

## 五个维度的对比

| 维度 | APK + .so | 独立可执行文件 |
|---|---|---|
| 运行身份 | 普通 App UID | **可 root** |
| 访问其它进程 | 基本不可能 | 可以 |
| 加载私有系统库 | Android 7+ 被禁 | **可以** |
| 悬浮窗 | 需 `SYSTEM_ALERT_WINDOW` 授权 | 直接向 SurfaceFlinger 申请 |
| 被检测 | 有包名/组件信息 | 只是个进程 |
| 分发 | 安装 APK | push 文件 |
| 开发调试 | 需要 JNI 桥 | 直接 gdb/logcat |

## 关键差异一：命名空间限制

Android 7.0（API 24）引入了 **linker namespace**，
限制 App 只能加载"公开 NDK 库"白名单里的 `.so`。

```bash
adb shell cat /system/etc/public.libraries.txt
# libandroid.so
# libc.so
# libdl.so
# libEGL.so
# libGLESv2.so
# liblog.so
# libvulkan.so
# libz.so
# ...
```

App 里 `dlopen("libgui.so")` 会失败：

```
dlopen failed: library "libgui.so" needed or dlopened by ... is not accessible for the namespace "classloader-namespace"
```

**原生可执行文件不在 App 命名空间里，不受此限制。**

> [!note] 本项目必须加载 libgui
> 因为要绕过 Java 层的 `WindowManager`，直接向 SurfaceFlinger 申请图层（第 65 章）。
> 这是"做成可执行文件"最重要的理由。

## 关键差异二：权限与身份

APK 里的代码跑在 App UID 下：

```
u0_a123  进程
  ├─ 不能读 /proc/other_pid/mem
  ├─ 不能 process_vm_readv 别的 UID 的进程
  └─ 不能访问 /data/data/other_pkg/
```

独立可执行文件以执行者身份运行：

```bash
adb shell /data/local/tmp/klbq          # uid=2000 (shell)
adb shell su -c /data/local/tmp/klbq    # uid=0 (root)
```

root 下几乎无限制。

## 关键差异三：悬浮窗的申请方式

| 方式 | 路径 | 限制 |
|---|---|---|
| App 方式 | `WindowManager.addView()` | 需 `SYSTEM_ALERT_WINDOW`，Android 8+ 还需用户手动授权 |
| 原生方式 | `SurfaceComposerClient::createSurface()` | 需 root，但**无任何弹窗** |

本项目的 `ANativeWindowCreator` 走原生方式（第 65 章）。

## 关键差异四：生命周期

| | APK | 可执行文件 |
|---|---|---|
| 谁启动 | 用户/系统（Activity 生命周期） | 你手动执行 |
| 被杀 | 系统可随时回收 | 也会，但优先级可设 |
| 崩溃 | 弹出"应用已停止" | 静默退出（看 logcat） |
| 保活 | 需要各种手段 | 靠 shell 脚本/守护 |

## 关键差异五：检测面

| 特征 | APK | 可执行文件 |
|---|---|---|
| 包名 | 有，可被枚举 | 无 |
| 组件（Activity/Service） | 有 | 无 |
| `/proc/self/cmdline` | 包名 | **自定义字符串** |
| 安装痕迹 | `pm list packages` 可见 | 无 |

本项目正是利用了最后一条——它甚至把**窗口名伪装成系统层名**（第 67 章）。

> [!warning] 技术中立说明
> 上面这些"降低被检测概率"的手段，在合法的自研调试工具上同样有价值：
> 你不想让自己的调试悬浮窗出现在录屏里，也不想让性能剖析面板被用户的手势误触。
> 本教程只讲机制，**把技术手段用在未授权的第三方程序上不在本教程范围内**。

## 什么时候该选 APK？

独立可执行文件不是万能的：

| 需求 | 该选 |
|---|---|
| 需要 UI（按钮、列表、输入） | APK（或原生 + ImGui，本项目做法） |
| 需要上架分发 | APK |
| 需要系统回调（通知、广播） | APK |
| 需要后台长期运行 | 各有方案 |
| 需要 root 能力 | 可执行文件 |
| 需要访问私有系统库 | 可执行文件 |
| 需要读其它进程 | 可执行文件 |

本项目的选择是**"原生可执行文件 + 自带 ImGui 界面"**——
用可执行文件拿权限，用 ImGui 补 UI 短板。

## 混合方案：APK 启动 native 程序

还有一种常见做法：

```
APK（有界面、能上架）
  └─ 启动时 Runtime.exec("su -c /data/local/tmp/daemon")
       └─ daemon 以 root 运行，提供能力
```

这样既有正规 UI，又有 root 能力。代价是依赖 `su`。

## 本项目的文件清单

一个典型的部署：

```
/data/local/tmp/
├── klbq                    # 主程序（可执行）
└── （可选）依赖的 .so
```

主程序**除系统库外全部静态链接**（ImGui + Embree + c++_static 都打进了同一个文件），
只动态依赖 Android 系统自带的 `liblog.so`、`libandroid.so`、`libz.so`（这些每个设备都有，不用你带）。

验证：

```bash
adb shell readelf -d /data/local/tmp/klbq | grep NEEDED
```

## 动手：两种形态都试一次

**A. 编译并运行一个独立可执行文件**

```bash
# hello.c
cat > hello.c << 'EOF'
#include <stdio.h>
int main(void) { printf("hello from native\n"); return 0; }
EOF

# 用 NDK 编译（第 33 章会详细讲）
$NDK/toolchains/llvm/prebuilt/windows-x86_64/bin/aarch64-linux-android21-clang \
    hello.c -o hello

adb push hello /data/local/tmp/
adb shell chmod 755 /data/local/tmp/hello
adb shell /data/local/tmp/hello
```

**B. 试试它的身份**

```bash
adb shell "echo \$UID"                        # shell 的 uid
adb shell "su -c 'echo \$UID'"                # root 的 uid（有 root 才行）
```

**C. 看看能不能加载私有库**

> [!note] `dlopen` / `dlsym` 是"运行时加载动态库"（第 65 章详讲）
> - `dlopen("libxxx.so", 标志)`：**在运行时打开一个 .so**，返回它的句柄
> - `dlsym(句柄, "符号名")`：**从库里找到某个函数/变量的地址**
> - `dlerror()`：取最近一次 `dlopen`/`dlsym` 的错误信息
> 它们让"加载库、找符号"从"编译时自动"变成"运行时手动"。本章用它验证命名空间限制，**第 65 章会完整讲。**

```c
#include <dlfcn.h>
#include <stdio.h>
int main(void) {
    void *h = dlopen("libgui.so", RTLD_NOW);
    printf("libgui: %s\n", h ? "OK" : dlerror());
    return 0;
}
```

在独立可执行文件里能成功；如果放进 APK 的 .so 里就会失败。


## 动手验证清单

- [ ] **查身份**：`adb shell id` → 普通 shell 是 `uid=2000`
- [ ] **试 root**：`adb shell su -c id` → 有 root 则 `uid=0(root)`，没有会报错（正常）
- [ ] **查 App 沙箱**：`adb shell ps -A -o USER,NAME | grep u0_a` → 每个 App 一个独立 UID
- [ ] **确认 `/sdcard` 不可执行**：`adb shell mount | grep sdcard` → 含 `noexec`
- [ ] **编译 hello 并 push 运行**：见"动手：两种形态都试一次" → 看到 `hello from native`

> [!note] 没有 root 设备
> 第 2 步会失败，正常。`adb root`（userdebug）或直接跳过——卷一到卷四不需要 root。

## 课后习题

### 习题 32.1 给需求选形态（★★，应用变式）

**任务要求**：下面 5 个需求，各自该做成 **APK**、**独立可执行文件**、还是**混合方案**？
说明理由（对照本章"什么时候该选 APK"表）。

| # | 需求 | 该选 | 理由 |
|---|---|---|---|
| 1 | 一个上架应用商店的记账 App | ？ | ？ |
| 2 | 一个需要读其它游戏进程内存的调试工具 | ？ | ？ |
| 3 | 一个需要常驻后台、监听通知的工具 | ？ | ？ |
| 4 | 一个要给普通用户用、但底层要 root 能力的工具 | ？ | ？ |
| 5 | **KLBQ 本体** | ？ | ？ |

**参考答案要点**：
1. APK（要上架、无 root 需求）；
2. 独立可执行文件（要读别的进程、要私有库）；
3. APK（要系统回调）；
4. 混合方案（APK 壳 + Runtime.exec 启动 root 守护）；
5. 独立可执行文件 + 自带 ImGui（拿权限 + 补 UI 短板）。

### 习题 32.2 论证题：为什么 KLBQ 不能是 APK 里的 .so（★★★，分析+评价）

**任务要求**：写一段 150~250 字的论证，回答：
"**把 KLBQ 改成一个 APK 里的 .so，会导致哪两个**致命**问题？**"

要求：
1. 至少引用本章两个"关键差异"（命名空间限制 / 权限身份）；
2. 每个问题给出"**具体会报什么错/失败在哪一步**"；
3. 结论：为什么"独立可执行文件"是**唯一可行**的选择，而非"偏好"。

**参考骨架**：
- **问题一**：Android 7+ linker namespace 限制 → App 里 `dlopen("libgui.so")` 报 `is not accessible for the namespace "classloader-namespace"` → 拿不到 `SurfaceComposerClient` → 无法创建悬浮图层。
- **问题二**：App UID（u0_aXXX）→ 无法 `process_vm_readv` 读游戏进程 → 整个内存读取链路断掉。

> [!tip] 评价层的要点
> 不只是"列出差异"，而要判断**哪个差异是"非此不可"的硬约束**——
> 命名空间限制是**系统级硬墙**（无法用代码绕过），权限问题是**身份级硬墙**（除非有系统签名）。
> 这两个硬约束共同决定了形态选择。

## 本章小结

> [!abstract] 本章要点已收束
> 回头把本页的"动手"与结论串成一句话；有勾不上的验收项，回去补做。

## 验收清单

- [ ] 能说出 APK+so 与独立可执行文件的 5 个差异（对照本章对比表，逐条复述）
- [ ] 知道 Android 7+ 的 linker namespace 限制，以及它为什么逼本项目选可执行文件（说出 App 无法 dlopen libgui）
- [ ] 知道 `/data/local/tmp` 的作用（说出"adb push 可写且能执行"）
- [ ] 知道 root 下 UID=0、普通 shell UID=2000（用 `adb shell id` 和 `su -c id` 各验证一次）
- [ ] 编译并 push 运行了一个 hello（哪怕只完成 A 步）（看到 hello from native 输出）

→ 下一章：[[第33章-NDK与第一条ndk-build]]　—— 装好 NDK，用两条不同路线（ndk-build / clang 直编）编译出第一个 arm64 可执行文件。
