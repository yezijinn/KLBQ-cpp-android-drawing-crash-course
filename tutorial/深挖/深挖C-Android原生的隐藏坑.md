---
tags: [教程, 深挖, 卷三, Android, NDK, 系统]
aliases: [深挖C]
---

# 深挖 C · Android 原生的隐藏坑

> [!abstract] 这篇解决什么
> 第 31-44 章讲了"怎么做"。这篇讲"为什么有时候不work"：
> 版本差异的完整对照、dlopen 限制的精确规则、SELinux 的完整理解、
> 原生程序的完整生命周期。**建议学完卷三后读。**

## 一、Android 版本差异完整对照

写原生代码时，"这个 API 能不能用"取决于设备版本。这张表是关键。

| Android | API | 关键变化（影响原生开发） |
|---|---|---|
| 5.0 | 21 | 强制 PIE；引入 `linker` 的命名空间雏形 |
| 6.0 | 23 | 运行时权限；`libc++` 成为唯一官方 STL |
| 7.0 | 24 | **linker namespace 正式生效**（App 不能 dlopen 私有库）★ |
| 8.0 | 26 | `libgui` 符号变化；SELinux 更严 |
| 9 | 28 | 强制 `vendor` 分区隔离；`libutils` 改动 |
| 10 | 29 | `String8` 内部改成 `std::string` ★ |
| 11 | 30 | `createSurface` 加 `LayerMetadata` 参数 ★ |
| 12 | 31 | `trusted overlay`；`eSkipScreenshot` flag |
| 13 | 33 | `setTrustedOverlay`；镜像显示 API 变化 |
| 14 | 34 | 更严的 `exec` 限制；部分 shell 命令移除 |
| 15 | 35 | `apps_targetsdk` 相关的兼容性收紧 |

**对本项目影响最大的三个（带 ★）**：

1. **API 24**：App 无法 `dlopen` 系统私有库 → 这是选择"独立可执行文件"的根本原因
2. **API 29**：`String8` 布局变化 → 结构体大小从 8 字节变 32 字节
3. **API 30**：`createSurface` 签名变化 → 参数个数不同

### 版本检测代码

```cpp
#include <sys/system_properties.h>
#include <cstdlib>

int GetApiLevel() {
    char buf[PROP_VALUE_MAX] = {0};
    if (__system_property_get("ro.build.version.sdk", buf) <= 0) return 0;
    return atoi(buf);
}

bool IsAndroid12OrNewer() { return GetApiLevel() >= 31; }
```

**注意**：`getprop` 也可能失败（极少见，但厂商 ROM 可能改），所以要判断返回值。

## 二、dlopen 限制的精确规则

### 规则本体

Android 7.0 引入了 linker namespace。规则可以概括为：

```
App 进程（有 classloader-namespace）
  ├─ 可以 dlopen：public.libraries.txt 里列出的库
  ├─ 不可以 dlopen：其它系统库（libgui、libutils、libbinder...）
  └─ 可以 dlopen：自己 APK 里的库

原生可执行文件（/data/local/tmp/xxx）
  └─ 可以 dlopen：几乎任何库（受 SELinux 约束）
```

### 公开库清单

```bash
adb shell cat /system/etc/public.libraries.txt
```

典型内容：

```
libandroid.so
libc.so
libdl.so
libEGL.so
libGLESv1_CM.so
libGLESv2.so
libGLESv3.so
libjnigraphics.so
liblog.so
libm.so
libOpenSLES.so
libRS.so
libstdc++.so
libvulkan.so
libz.so
libaaudio.so
libamidi.so
libcamera2ndk.so
libmediandk.so
libnativewindow.so
libneuralnetworks.so
```

**注意**：`libgui.so`、`libutils.so`、`libbinder.so` **不在列表里**。

### 实测方法

```cpp
#include <dlfcn.h>
#include <cstdio>

int main(void) {
    const char *libs[] = {"libgui.so", "libutils.so", "libbinder.so",
                          "libvulkan.so", "liblog.so", nullptr};
    for (int i = 0; libs[i]; i++) {
        void *h = dlopen(libs[i], RTLD_NOW);
        printf("%-16s : %s\n", libs[i], h ? "OK" : dlerror());
        if (h) dlclose(h);
    }
    return 0;
}
```

- 在**原生可执行文件**里运行：大部分 OK
- 放进 **APK 的 .so** 里运行：`libgui`/`libutils`/`libbinder` 会失败

**这个实验能让你彻底理解"为什么做可执行文件"。**

### 绕不过去时怎么办

| 需求 | 替代方案 |
|---|---|
| 需要窗口/图层 | 用 Java 的 `WindowManager`（受限但合法） |
| 需要 Binder | 用 `libbinder_ndk.so`（在公开列表里） |
| 需要 Surface | 用 `ANativeWindow_fromSurface`（从 Java 传过来） |

**结论**：如果不想做成可执行文件，就得接受 Java 层的限制。

## 三、SELinux 的完整理解

第 37 章讲了现象。这里讲原理和完整排查。

### 三要素：主体、客体、策略

```
主体（谁）：进程的 SELinux 域，如 u:r:shell:s0
客体（对谁）：文件的类型，如 u:object_r:app_data_file:s0
策略：allow 规则表
```

SELinux 的决策：

```
allow Domain Type:Class Permission;
```

例如：

```
allow shell app_data_file:file { read write };
```

如果找不到匹配的 `allow` 且没有 `dontaudit`，就是拒绝。

### 域（Domain）与转换

进程启动后有一个域，可能发生"域转换"：

```
init 启动 adbd → adbd 运行在 u:r:adbd:s0
adb shell      → 派生 shell 进程，域变为 u:r:shell:s0
su -c          → 域转换为 u:r:su:s0
```

**关键**：`su` 不只是改 uid，还**切换了 SELinux 域**。
如果 `su` 的实现没有正确切换域，就会出现"uid=0 但什么都干不了"。

### 查看当前域

```bash
adb shell id -Z
# context=u:r:shell:s0

adb shell su -c id -Z
# context=u:r:su:s0
```

### 文件上下文

```bash
adb shell ls -Z /data/local/tmp/
# u:object_r:shell_data_file:s0  app

adb shell ls -Z /data/data/com.example/
# u:object_r:app_data_file:s0  files
```

**常用路径的上下文**（决定 SELinux 宽松程度）：

| 路径 | 上下文 | 宽松度 |
|---|---|---|
| `/data/local/tmp` | `shell_data_file` | 较宽松 ★ |
| `/data/data/<pkg>` | `app_data_file` | 严格 |
| `/sdcard` | `media_rw_data_file` | 中等（但 noexec） |
| `/system/bin` | `system_file` | 只读 |

**本项目选 `/data/local/tmp` 不是随便选的**——它是最宽松的可执行位置。

### 完整的 avc 拒绝分析

```
avc: denied { read write } for pid=1234 comm="myapp"
     path="/data/data/com.game/files/data"
     dev="dm-5" ino=123456
     scontext=u:r:shell:s0
     tcontext=u:object_r:app_data_file:s0
     tclass=file permissive=0
```

逐字段：

| 字段 | 含义 |
|---|---|
| `{ read write }` | 被拒的权限集合 |
| `pid` `comm` | 哪个进程 |
| `path` | 目标路径（有 path 说明是文件操作） |
| `scontext` | 主体域 |
| `tcontext` | 客体类型 |
| `tclass` | 客体类别（file/dir/unix_stream_socket...） |
| `permissive=0` | 0 = 真拒绝；1 = 只告警（permissive 模式） |

**看到 `permissive=0` 说明确实被拦了。如果 `=1`，说明当前是 permissive 模式，
操作其实成功了——但这是临时状态，别依赖。**

### 排查流程

```
操作被拒
  ↓
dmesg | grep avc          ← 有没有 avc 记录？
  ├─ 有 → SELinux 问题
  │        ① 换个宽松的位置（如 /data/local/tmp）
  │        ② 换域（正确配置 su）
  │        ③ 写策略（需内核支持）
  │        ④ setenforce 0（仅诊断！）
  └─ 无 → DAC/capability 问题
           ls -l 看权限位；/proc/self/status 看 CapEff
```

## 四、原生程序的完整生命周期

### 启动阶段

```
1. 你执行 /data/local/tmp/app
2. shell/fork + execve
3. 内核加载 ELF：
   a. 读程序头表
   b. 映射 PT_LOAD 段到内存
   c. 找到 PT_INTERP → 启动动态链接器 (/system/bin/linker64)
4. 动态链接器：
   a. 加载所有 NEEDED 库
   b. 做重定位（R_AARCH64_RELATIVE 等）
   c. 调用 .init_array 里的构造函数
5. 跳到 ELF 头的 e_entry
6. C 运行时的启动代码（crt0）→ 调用 main
```

**调试"main 之前崩溃"**：
- 阶段 3 失败 → `CANNOT LINK EXECUTABLE`
- 阶段 4b 失败 → `cannot locate symbol`
- 阶段 4c 失败 → 全局对象构造函数有问题

### 运行阶段

```
main()
  ├─ 你的初始化
  ├─ 主循环
  └─ 清理
```

**注意**：原生可执行文件没有 Java 的 `Activity` 生命周期，
没有 `onPause`/`onResume`。被切后台时**不会被自动暂停**——除非系统杀进程。

### 退出与信号

| 信号 | 默认行为 | 来源 |
|---|---|---|
| `SIGTERM`(15) | 终止 | `kill` |
| `SIGKILL`(9) | 立即终止（不可捕获） | `kill -9`、OOM |
| `SIGSEGV`(11) | 段错误终止 | 非法内存访问 |
| `SIGABRT`(6) | 终止 | `abort()`、断言失败 |
| `SIGINT`(2) | 终止 | Ctrl+C |

**优雅退出**：

```cpp
#include <csignal>
#include <atomic>

static std::atomic<bool> g_running{true};

void OnSignal(int sig) {
    g_running = false;      // 只做最简单的操作！
}

int main(void) {
    struct sigaction sa{};
    sa.sa_handler = OnSignal;
    sigaction(SIGINT,  &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);

    while (g_running) { /* 主循环 */ }

    // 清理：释放 EVIOCGRAB、销毁窗口、保存配置
    Cleanup();
    return 0;
}
```

> [!danger] 信号处理函数里不要做复杂操作
> 信号处理器可能在任意时刻打断你的代码。
> 里面只能做**异步信号安全**的操作（写 `volatile sig_atomic_t` 变量、`write()` 系统调用）。
> `printf`、`malloc`、`mutex` 都可能死锁。
>
> **正确做法**：只设一个 `atomic<bool>`，主循环检查它。

### 后台存活

```bash
# 简单后台
adb shell "nohup /data/local/tmp/app > /dev/null 2>&1 &"

# 用 setsid 脱离终端
adb shell "setsid /data/local/tmp/app < /dev/null > /dev/null 2>&1 &"
```

被 OOM killer 杀掉的风险始终存在（尤其是内存紧张的设备）。

查看被谁杀：

```bash
adb logcat | grep -i 'lowmemorykiller\|lmkd'
dmesg | grep -i 'killed process'
```

## 五、root 方案差异

| 方案 | 原理 | `su` 的域 | 能否加载 .ko |
|---|---|---|---|
| Magisk | 修改 boot 镜像 + overlay 挂载 | `u:r:magisk:s0` | 可以（模块机制） |
| KernelSU | 内核态 root | `u:r:su:s0` | 需要内核支持 |
| APatch | 内核补丁 | 类似 | 可以 |
| 官方 userdebug | 系统自带 | `u:r:su:s0` | 可以（需解锁） |

**对本项目的影响**：

| 项目 | 需要什么 |
|---|---|
| 读其它进程内存 | root（任意方案） |
| 创建悬浮图层 | root + SELinux 允许 |
| uinput 触摸注入 | root + `/dev/uinput` 存在 |
| 内核驱动方案 | 能加载 `.ko` + 内核版本匹配 |

**KernelSU/Magisk 的差异主要在 SELinux 域的命名上**，写代码时不要硬编码域的假设。

## 六、NDK 与系统库可用性

| 库 | NDK 可链接 | 说明 |
|---|---|---|
| `liblog.so` | ✓ | `-llog` |
| `libandroid.so` | ✓ | `-landroid`，ANativeWindow 等 |
| `libz.so` | ✓ | `-lz` |
| `libm.so` | ✓ | 数学（通常自动链接） |
| `libdl.so` | ✓ | 但 Android 7+ 后 `dlopen` 在 `libc` 里 |
| `libvulkan.so` | ✓ | 但建议 `dlopen`（兼容性） |
| `libEGL.so` / `libGLESv2.so` | ✓ | OpenGL ES |
| `libOpenSLES.so` | ✓ | 音频 |
| `libmediandk.so` | ✓ | 媒体编解码（API 21+） |
| `libcamera2ndk.so` | ✓ | 相机（API 24+） |
| `libnativewindow.so` | ✓ | `AHardwareBuffer`（API 26+） |
| `libbinder_ndk.so` | ✓ | AIDL 的 NDK 后端（API 29+） |

**不在 NDK 里、需要 `dlopen` 的**：`libgui.so`、`libutils.so`、`libbinder.so`（C++ 版）。

## 七、把这篇用在项目里

| 知识点 | 项目应用 |
|---|---|
| 版本检测 | 第 66 章按 API 选符号 |
| dlopen 限制 | 第 32 章选可执行文件的理由；第 65 章能 dlopen libgui |
| SELinux 域 | 第 37 章权限排查 |
| `/data/local/tmp` 上下文 | 部署位置的选择 |
| 生命周期 | 第 35 章的运行方式 |
| 信号处理 | 第 71 章的"退出前 UNGRAB" |
| 启动阶段 | 排查 `CANNOT LINK EXECUTABLE` |

> [!tip] 三个最有用的排查命令
> ```bash
> # 1. 这个库能不能 dlopen？（在设备上跑你的探测程序）
> adb shell su -c /data/local/tmp/dltest
> 
> # 2. 为什么被拒？（SELinux）
> adb shell su -c dmesg | grep avc | tail -5
> 
> # 3. main 之前为什么崩？（加载期）
> adb shell /data/local/tmp/app 2>&1 | head -5
> adb logcat | grep -E 'linker|CANNOT LINK'
> ```

→ 返回 [[卷三-本卷导航]]
