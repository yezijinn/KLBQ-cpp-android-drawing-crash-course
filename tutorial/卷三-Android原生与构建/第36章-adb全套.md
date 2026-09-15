---
tags: [教程, 卷三, Android, adb]
day: 23
aliases: [ch36]
---

# 第 36 章 · adb 全套

> [!abstract] 本章目标
> 掌握 adb 的完整命令集，能把设备当成一台"远程 Linux 机器"来操作。
> 后面所有章节的调试都建立在这些命令上。

## 第 0 步：装 platform-tools（便携包 → `C:\dev`）

adb **不是一个单独的程序**，它属于 Google 的 **platform-tools** 包，官方只提供 zip 压缩包：

1. 打开 `https://developer.android.com/tools/releases/platform-tools`
2. 下载 `platform-tools-latest-windows.zip`
3. 解压到 `C:\dev\platform-tools`
   （**路径规矩同第 02 章：便携包放 `C:\dev`，全英文、无空格**）
4. 确认里面有 `adb.exe`、`fastboot.exe`
5. 把 `C:\dev\platform-tools` 加进 PATH（加环境变量的步骤见第 02 章第二步）
6. 重开终端，验证（**按终端选一条，命令不通用**）：

```bash
# —— Git Bash ——
adb version
# 应输出：Android Debug Bridge version 1.0.41
which adb
# 应输出：/c/dev/platform-tools/adb
```

```bat
rem —— cmd ——
adb version
where adb
rem 应输出：C:\dev\platform-tools\adb.exe
```

```powershell
# —— PowerShell ——（注意：这里不能用 where）
adb version
Get-Command adb
(Get-Command adb).Source
# 应输出：C:\dev\platform-tools\adb.exe
```

> [!warning] PowerShell 里 `where adb` 也是"什么都不输出"
> 和 [[第02章-装好第一套工具链]] 里 `where gcc` 是同一个坑：`where` 属于 cmd。
> PowerShell 里用 `Get-Command adb` 或 `where.exe adb`。
> 三个终端的完整对照表见 [[附录R-绝对零基础前置知识]] 第 3.2 节。

> [!danger] 两个最容易踩的坑
> **坑 1：PATH 里加了 `C:\dev\platform-tools\bin`**
> 这个包**没有 `bin` 子目录**，`adb.exe` 直接躺在包根目录下。加了 `\bin` 等于没加。
>
> | 工具 | 可执行文件在哪一层 | PATH 里该填 |
> |---|---|---|
> | w64devkit | `C:\dev\w64devkit\`**`bin`**`\gcc.exe` | `C:\dev\w64devkit\bin` |
> | platform-tools | `C:\dev\platform-tools\adb.exe`（**没有 bin**）| `C:\dev\platform-tools` |
> | NDK | `C:\dev\android-ndk-r27c\`（`ndk-build.cmd` 在根）| `C:\dev\android-ndk-r27c` |
>
> **判断依据只有一个：`.exe` 实际在哪一层，PATH 里就填到那一层。**
>
> **坑 2：双击 `adb.exe` 运行**
> adb 是命令行工具，双击会瞬间闪退（窗口一闪而过），看起来像"没反应"。
> 正确做法是**先打开终端，再敲 `adb`**。
> 这两个坑的本质是同一件事：**它不是你双击的程序，它是一条命令。**

## 先看 adb 的本质

```
┌─────────────┐         ┌──────────────────┐
│  电脑         │         │  手机/模拟器       │
│  adb client  │◄───────►│  adbd (守护进程)   │
│  adb server  │  USB/   │  以 shell 或 root │
│              │  TCP    │  身份执行命令      │
└─────────────┘         └──────────────────┘
```

三个组件：

| 组件 | 跑在哪 | 作用 |
|---|---|---|
| `adb client` | 电脑 | 你敲的那个命令 |
| `adb server` | 电脑（后台） | 管理连接，端口 5037 |
| `adbd` | 设备 | 接收并执行 |

> [!important] 一条边界要先记住：`adb` 在电脑敲，`adb shell` 后面的部分在**手机**上跑
> | 你敲的 | 在哪执行 |
> |---|---|
> | `adb devices` | **电脑** |
> | `adb push a b` | **电脑 → 手机** |
> | `adb shell ls` | **手机**（`ls` 在手机里跑）|
> | 进了 `adb shell` 之后敲的每一条 | **手机** |
>
> 所以手机里的路径是 `/data/local/tmp/`（正斜杠、无盘符）——**手机是 Linux，不是 Windows**。
> 展开说明见 [[附录R-绝对零基础前置知识]] 第 3.2 节。

## 设备连接

```bash
adb devices                 # 列出设备
adb devices -l              # 详细信息（型号、transport_id）
adb -s <serial> shell       # 多设备时指定
adb kill-server             # 重启 adb（解决大部分玄学问题）
adb start-server
adb reconnect               # 重连
adb usb                     # 切回 USB 模式
```

多设备时：

```bash
export ANDROID_SERIAL=ABCDEF123456     # 设一次，之后不用 -s
```

## shell：远程终端

```bash
adb shell                              # 进入交互 shell
adb shell <命令>                        # 执行单条
adb shell "ls -l /data/local/tmp"      # 带空格要加引号
adb shell su -c "命令"                  # 以 root 执行
```

shell 里的可用命令是有限的玩具箱（toolbox/toybox），
但常用 Linux 命令都有：`ls cd cat grep ps top kill chmod chown mount getprop setprop`。

> [!note] 没有 bash 的完整特性
> Android 的 shell 是 `mksh`/`ash`，不支持 bash 的数组、双方括号条件测试等。
> 写复杂脚本时要么简化，要么 push 一个 busybox 上去。

## 文件操作

```bash
adb push <本地> <远程>
adb pull <远程> [本地]
adb shell ls -l /data/local/tmp
adb shell rm /data/local/tmp/xxx
adb shell mkdir -p /data/local/tmp/test
adb shell cat /proc/version
```

带权限：

```bash
adb shell chmod 755 <file>
adb shell chown root:root <file>
```

## 进程管理

```bash
adb shell ps -A                       # 所有进程
adb shell ps -A | grep klbq
adb shell ps -A -o PID,NAME,USER      # 自定义列
adb shell pidof com.example.app        # 按名字查 pid（不是所有设备都有）
adb shell kill -9 <pid>
adb shell top -n 1                     # 看 CPU 占用
```

`ps` 输出：

```
USER           PID  PPID     VSZ    RSS WCHAN            ADDR S NAME
root          1234     1  5678900 123456 0                   0 S surfaceflinger
u0_a123       5678   800  2345678  98765 0                   0 S com.example
```

| 列 | 含义 |
|---|---|
| `VSZ` | 虚拟内存大小（KB） |
| `RSS` | 实际物理内存（KB） |
| `WCHAN` | 正在等待的内核函数 |
| `S` | 状态（R/S/D/T/Z，第 18 章） |

## 日志

```bash
adb logcat                     # 实时
adb logcat -d                  # dump 后退出
adb logcat -c                  # 清空
adb logcat -s TAG              # 按 tag 过滤
adb logcat TAG:W *:S           # 只显示 TAG 的 W 及以上
adb logcat -v time             # 带时间
adb logcat --pid=<pid>         # 只某进程
adb logcat -b crash            # 崩溃缓冲区
adb logcat -g                  # 缓冲区大小
```

过滤组合：

```bash
adb logcat | grep -iE 'error|fatal|signal'
adb logcat -s MyTag Driver:*   # 多个 tag
```

## 系统属性

```bash
adb shell getprop                          # 全部
adb shell getprop ro.build.version.sdk     # API 等级
adb shell getprop ro.build.version.release # Android 版本
adb shell getprop ro.product.cpu.abi       # 主 ABI
adb shell getprop ro.product.cpu.abilist   # 支持的 ABI 列表
adb shell getprop ro.board.platform        # 芯片平台
adb shell getprop sys.boot_completed       # 是否启动完成
adb shell setprop debug.mytag 1            # 设置（需权限）
```

本项目用 `getprop` 判断 Android 版本，从而选择不同的 SurfaceFlinger 符号（第 66 章）。

上面 `getprop` 是 shell 命令；在 C 代码里读属性用 `__system_property_get`：

```cpp
#include <sys/system_properties.h>   // 属性 API 头文件

char sdk[PROP_VALUE_MAX];            // PROP_VALUE_MAX 是属性值最大长度（92）
__system_property_get("ro.build.version.sdk", sdk);   // 把属性值读进 sdk
int apiLevel = atoi(sdk);            // "29" → 29
```

> [!note] `__system_property_get` / `PROP_VALUE_MAX` 是什么
> 它们是 Android 的**系统属性读取 API**（在 `<sys/system_properties.h>`）——就是 `getprop` 命令的底层实现。
> `PROP_VALUE_MAX` 是属性值缓冲区的最小安全长度（必须按它开数组）。
> **第 66 章会用它做版本判断**，这里先认识。

## 应用管理

```bash
adb shell pm list packages              # 所有包名
adb shell pm list packages -3           # 第三方
adb shell pm path com.example           # APK 路径
adb shell pm dump com.example | head    # 详细信息
adb shell am start -n pkg/.Activity
adb shell am force-stop com.example
adb shell am kill com.example
```

`dumpsys` 是信息金矿：

```bash
adb shell dumpsys display       # 显示信息（第 68 章用它解析多屏）
adb shell dumpsys SurfaceFlinger   # 图层列表
adb shell dumpsys meminfo <pid>    # 内存占用
adb shell dumpsys cpuinfo
adb shell dumpsys battery
```

## 截屏与录屏

```bash
adb shell screencap -p /sdcard/screen.png
adb pull /sdcard/screen.png

adb shell screenrecord /sdcard/demo.mp4 --time-limit 10
adb pull /sdcard/demo.mp4
```

> [!note] 这正是"过录制"要防的东西
> 本项目第 67 章讲的 `skipScreenshot` 图层属性，
> 就是让 `screencap`/`screenrecord` 跳过你的图层。

## 端口与网络

```bash
adb forward tcp:8080 tcp:8080        # 电脑端口 → 设备端口
adb reverse tcp:3000 tcp:3000        # 设备端口 → 电脑端口（更常用）
adb shell netstat -tuln
adb shell ip addr
```

`adb reverse` 让设备能访问电脑上跑的服务器，调试网络请求时很有用。

## root 相关

```bash
adb root                    # 重启 adbd 为 root（仅 userdebug/eng 版本）
adb unroot                  # 恢复
adb shell su -c "命令"       # 通用方式（需要已 root 的设备）
adb shell id                # 看当前 uid
```

| 设备类型 | `adb root` 能用吗 |
|---|---|
| user（零售版） | 不能 |
| userdebug | 能 |
| eng | 能 |
| 已刷 Magisk | 用 `su` |

## 调试增强

```bash
adb shell setprop debug.debuggerd wait_for_debugger   # 等调试器
adb jdwp                                  # 列出可调试的 Java 进程
adb forward tcp:1234 jdwp:<pid>           # Java 调试
# 原生调试用 gdbserver / lldb-server
adb shell gdbserver :5039 --attach <pid>
adb forward tcp:5039 tcp:5039
```

## 高效技巧

**一次执行多条**：

```bash
adb shell "cd /data/local/tmp && chmod 755 app && ./app"
```

**别名**（加到 `~/.bashrc`）：

```bash
alias adbsh='adb shell'
alias adbpush='adb push'
alias adbroot='adb shell su -c'
alias adbcat='adb logcat -s'
alias adbcrash='adb logcat -b crash'
```

**watch 循环**：

```bash
while true; do adb shell ps -A | grep klbq; sleep 1; done
```

**传文件给 stdin**：

```bash
cat script.sh | adb shell       # 把脚本内容喂给设备 shell
```

## 常见错误

| 报错 | 原因 | 解法 |
|---|---|---|
| `device offline` | adbd 卡了 | `adb kill-server && adb start-server` |
| `more than one device` | 多设备 | `-s <serial>` 或 `ANDROID_SERIAL` |
| `Permission denied` | 路径不可写 / 没 root | 换 `/data/local/tmp` 或加 `su -c` |
| `adb: command not found` | 没加 PATH，或 PATH 里多写了 `\bin` | PATH 填 `C:\dev\platform-tools`（**没有 bin 层**）|
| `protocol fault` | Windows 换行符 | 脚本转 LF |

## 动手：写一个设备体检脚本

```bash
#!/bin/bash
echo "=== 基本信息 ==="
adb shell getprop ro.product.model
adb shell getprop ro.build.version.release
adb shell getprop ro.build.version.sdk
adb shell getprop ro.product.cpu.abi
adb shell uname -a

echo "=== root 状态 ==="
echo -n "shell uid: "; adb shell id
echo -n "su   uid: "; adb shell su -c id 2>/dev/null || echo "无 root"

echo "=== SELinux ==="
adb shell getenforce

echo "=== 关键进程 ==="
adb shell ps -A -o PID,USER,NAME | grep -E 'surfaceflinger|zygote|system_server'

echo "=== 内存 ==="
adb shell cat /proc/meminfo | head -3

echo "=== /data/local/tmp 权限 ==="
adb shell ls -ld /data/local/tmp
```

跑一遍，把输出存成 `device_info.txt`。这个信息后面每章都要用。

## 验收清单

- [ ] 会用 `adb devices` / `-s` 管理多设备（列出设备，用 -s 指定）
- [ ] 会用 `push/pull/shell/chmod`（推一个文件、拉回来、进 shell、改权限各一次）
- [ ] 会用 `ps` / `pidof` 查进程，`kill` 结束进程（查到某进程 PID 并 kill）
- [ ] 会用 `logcat -s` / `-c` / `-d` / `-b crash`（清空、dump、按 tag 过滤、看崩溃缓冲各一次）
- [ ] 会用 `getprop` 查版本、ABI（`getprop ro.build.version.sdk` 等各查一次）
- [ ] 会用 `dumpsys display` 和 `dumpsys meminfo`（各跑一次，读出关键信息）
- [ ] 写了设备体检脚本并保存了输出（运行脚本，输出存成 device_info.txt）

→ 下一章：[[第37章-root-权限与SELinux]]　—— 理解 Android 的三层权限关卡（UID / capability / SELinux），知道为什么"明明是 root 还是被拒"。
