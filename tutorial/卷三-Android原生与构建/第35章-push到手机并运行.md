---
tags: [教程, 卷三, Android, 部署]
day: 23
aliases: [ch35]
---

# 第 35 章 · push 到手机并运行

> [!abstract] 本章目标
> 建立一套稳定的"编译 → 推送 → 运行 → 看日志"工作流，并写成脚本。
> **本章产出 `hello_arm64` 正式跑通**，是总纲 6 个成果之一。

> [!note] 承上
> 上一章理解了构建脚本。
> 本章把"编译→推送→运行→看日志"串成一条稳定工作流，产出**跑通的 `hello_arm64`**。

## 先看完整的一轮流程

> [!important] 前提：在工程根目录执行
> 下面的 `ndk-build` 和 `libs/arm64-v8a/` 都相对于**工程根目录**——
> 就是含 `jni/` 子目录的那个文件夹（第 33 章建的 `hello_ndk/`）。
> 执行前先确认：
> ```bash
> pwd            # 应显示 .../hello_ndk
> ls             # 应看到 jni/（构建后还会出现 libs/ 和 obj/）
> ```
> 不在这个目录会报 `Android NDK: Could not find application project directory`。

```bash
# 1. 编译
ndk-build -j8

# 2. 推送
adb push libs/arm64-v8a/hello_arm64 /data/local/tmp/

# 3. 给权限
adb shell chmod 755 /data/local/tmp/hello_arm64

# 4. 运行
adb shell /data/local/tmp/hello_arm64

# 5. 看日志（另一个终端）
adb logcat -s HelloNDK
```

每改一次代码就要敲五条命令——太蠢了。本章末尾会把它变成一条。

## 准备工作：设备与调试

1. 手机开启"开发者选项"（设置 → 关于手机 → 连点版本号 7 次）
2. 开启"USB 调试"
3. 用数据线连接，手机上授权弹窗点"允许"

验证：

```bash
adb devices
# List of devices attached
# ABCDEF123456    device
```

状态说明：

| 状态 | 含义 | 处理 |
|---|---|---|
| `device` | 正常 | — |
| `unauthorized` | 手机上没授权 | 拔插，点允许 |
| `offline` | adb 版本/连接问题 | `adb kill-server && adb start-server` |
| 空 | 没连上 | 检查线、驱动、USB 模式 |

## 推送：adb push

```bash
adb push <本地> <远程>
adb push hello_arm64 /data/local/tmp/
```

可以推送整个目录：

```bash
adb push libs/arm64-v8a/ /data/local/tmp/
```

反向拉取：

```bash
adb pull /data/local/tmp/log.txt ./
```

## 权限：chmod

```bash
adb shell chmod 755 /data/local/tmp/hello_arm64
```

`755` = 主人 rwx、其它人 rx。可执行文件必须至少对自己有 `x`。

> [!warning] 每次 push 后都要重新 chmod
> `adb push` 创建的文件权限通常是 `644`（无执行权限）。
> 所以"push + chmod"总是成对出现。

## 为什么是 /data/local/tmp

| 路径 | adb push 可写 | 可执行 | 说明 |
|---|---|---|---|
| `/data/local/tmp/` | ✓ | ✓ | **首选** |
| `/sdcard/` | ✓ | ✗ | 挂载了 `noexec` |
| `/data/data/<pkg>/` | 需 root | ✓ | App 私有目录 |
| `/system/bin/` | 需 remount | ✓ | 改系统分区，危险 |

```bash
# 验证 noexec
adb shell mount | grep sdcard
# ... /sdcard ... nosuid,nodev,noexec ...
```

## 运行

```bash
# 以 shell 身份运行
adb shell /data/local/tmp/hello_arm64

# 以 root 身份运行（有 root 才行）
adb shell su -c /data/local/tmp/hello_arm64

# 带参数
adb shell /data/local/tmp/hello_arm64 arg1 arg2

# 后台运行（不阻塞终端）
adb shell "nohup /data/local/tmp/hello_arm64 > /dev/null 2>&1 &"
```

> [!note] `adb shell` 后面可以接整条命令
> `adb shell "cd /data/local/tmp && chmod 755 x && ./x"`
> 注意引号：不加引号时 `&&` 会被本地 shell 解释。

## 看日志

Android 的原生日志走 logcat：

```bash
adb logcat                        # 全部（刷屏）
adb logcat -s HelloNDK            # 只看指定 tag
adb logcat | grep -i error        # 过滤
adb logcat -c                     # 清空缓冲区
adb logcat -v time                # 带时间戳
adb logcat --pid=<pid>            # 只看某进程
```

日志级别：

| 级别 | 函数 | 用途 |
|---|---|---|
| V | `__android_log_print(ANDROID_LOG_VERBOSE, ...)` | 最详细 |
| D | `..._LOG_DEBUG` | 调试 |
| I | `..._LOG_INFO` | 信息 |
| W | `..._LOG_WARN` | 警告 |
| E | `..._LOG_ERROR` | 错误 |
| F | `..._LOG_FATAL` | 致命（会终止进程） |

过滤语法：`adb logcat TAG:LEVEL *:S`

```bash
adb logcat HelloNDK:D *:S     # 只显示 HelloNDK 的 D 及以上
```

## 崩溃时看什么

```bash
adb logcat | grep -E 'SIGSEGV|SIGABRT|backtrace|DEBUG'
```

典型崩溃输出：

```
--------- beginning of crash
F/libc    (12345): Fatal signal 11 (SIGSEGV), code 1 (SEGV_MAPERR), fault addr 0x30 in tid 12345 (hello_arm64), pid 12345 (hello_arm64)
```

然后系统会生成 tombstone：

```bash
adb shell ls -lt /data/tombstones/ | head
adb shell cat /data/tombstones/tombstone_00 | head -40
```

里面有完整的寄存器状态和调用栈。用第 30 章的 `addr2line` 解析。

## 自动化脚本

**Windows（Git Bash）** `run.sh`：

```bash
#!/bin/bash
# 注意：不用 set -e，而是每个关键步骤手动判错（见下方"两个坑"）

TARGET=hello_arm64
REMOTE=/data/local/tmp/$TARGET

echo "==> 编译"
ndk-build -j8 || { echo "编译失败，终止"; exit 1; }

echo "==> 推送"
adb push libs/arm64-v8a/$TARGET $REMOTE || { echo "推送失败（设备连接？）"; exit 1; }
adb shell chmod 755 $REMOTE

echo "==> 清日志"
adb logcat -c

echo "==> 运行"
adb shell $REMOTE

echo "==> 日志"
adb logcat -d -s HelloNDK | tail -20
```

> [!danger] 这个脚本有 2 个坑，用在长程序上会踩
> **坑 1：`set -e` 让错误无输出**
> 原版用 `set -e`，`adb push` 失败时脚本直接退出，你**看不到任何提示**。
> 上面已改成显式判错（`|| { echo ...; exit 1; }`）。
>
> **坑 2：`adb shell $REMOTE` 对长期程序会卡住**
> 本章的 hello 跑完就退，没问题。但第 66 章的 overlay 是**常驻程序**，
> 这一行会永远阻塞。应该后台运行：
> ```bash
> adb shell "nohup $REMOTE > /dev/null 2>&1 &"
> sleep 3                  # 等它启动（复杂程序需 3~5 秒）
> adb logcat -d -s HelloNDK | tail -20
> ```

**Windows（批处理）** `run.bat`：

```bat
@echo off
set TARGET=hello_arm64
set REMOTE=/data/local/tmp/%TARGET%

echo ==^> 编译
call ndk-build -j8
if errorlevel 1 goto :fail

echo ==^> 推送
adb push libs\arm64-v8a\%TARGET% %REMOTE%
adb shell chmod 755 %REMOTE%

echo ==^> 运行
adb shell %REMOTE%

echo ==^> 日志
adb logcat -d -s HelloNDK
goto :eof

:fail
echo 编译失败
exit /b 1
```

> [!tip] 用 `adb logcat -d` 而不是 `adb logcat`
> `-d` 表示"dump 完就退出"，不会一直阻塞。适合脚本末尾抓日志。

## 无线调试（Android 11+）

不用插线：

```bash
# 1. 手机：开发者选项 → 无线调试 → 使用配对码配对
adb pair 192.168.1.100:37000
# 输入配对码

# 2. 连接
adb connect 192.168.1.100:5555

# 3. 正常用
adb shell ...
```

老版本（Android 10 及以下）需要先插线：

```bash
adb tcpip 5555
adb connect 192.168.1.100:5555
```

## 常见故障排查

| 现象 | 排查 |
|---|---|
| `adb: no devices` | `adb kill-server && adb start-server`；检查线/授权 |
| `Permission denied` | 没 chmod，或 SELinux 拦截 |
| `not found`（明明有这个文件） | 缺动态库，用 `readelf -d` 查 |
| 程序闪退无输出 | 看 logcat；可能是崩溃 |
| `read-only file system` | 推错位置了，用 `/data/local/tmp` |
| 中文输出乱码 | 终端编码，或 logcat 本身处理 |

## 动手：搭一套自己的工作脚本

要求：

1. 一个脚本完成 编译 → push → chmod → 运行 → 抓日志
2. 编译失败时脚本要停下来，不继续 push
3. 支持传参切换 debug/release

参考实现（Linux/Git Bash）：

```bash
#!/bin/bash
set -e
MODE=${1:-release}
TARGET=hello_arm64
REMOTE=/data/local/tmp/$TARGET

echo "==> 模式: $MODE"
ndk-build -j8 APP_OPTIM=$MODE

adb push libs/arm64-v8a/$TARGET $REMOTE
adb shell chmod 755 $REMOTE
adb logcat -c
adb shell $REMOTE &
sleep 1
adb logcat -d -s HelloNDK | tail -20
```

用法：`./run.sh debug` 或 `./run.sh release`。

## 动手验证清单

- [ ] **编译**：`ndk-build -j8` → `libs/arm64-v8a/` 下出现产物
- [ ] **推送**：`adb push libs/arm64-v8a/hello_arm64 /data/local/tmp/` → 显示传输速率
- [ ] **赋权**：`adb shell chmod 755 /data/local/tmp/hello_arm64` → 无报错
- [ ] **运行**：`adb shell /data/local/tmp/hello_arm64` → 输出 `hello from arm64`
- [ ] **看日志**：另开终端 `adb logcat -s HelloNDK` → 看到自己打的 log
- [ ] **一键脚本**：`./run.sh` → 一条命令跑完编译→推送→运行→抓日志
- [ ] **制造一次崩溃**：故意访问空指针，在 logcat 找 `Fatal signal`

> [!tip] 每次改代码后都走一遍这 5 步
> 编译→推送→赋权→运行→看日志，这就是本教程后面一直用的工作流。

> [!warning] 中途某步失败，如何重来？
> 这几步都是**可重复执行**的，失败后修正再跑即可，不用从头：
> - **编译失败** → 看报错改代码，重新 `ndk-build`
> - **push 失败** → 确认设备连着（`adb devices`），重新 push
> - **运行 Permission denied** → 忘了 `chmod 755`，补上再跑
> - **推错了想清干净** → `adb shell rm /data/local/tmp/你的程序`，重新走
>
> 唯一要注意：`adb push` 会**覆盖**同名文件，多推几次不会有副作用。

## 综合实战：一键部署工具链（脱离指导）

> [!important] 这是你第一个"不看教程、独立完成"的任务
> 前面所有动手都有完整代码可抄。**这个任务只给需求 + 接口骨架**，实现由你独立完成。
> 做不出来说明前面是"读懂了"而非"会用了"——回炉重做本章动手。

**业务需求**（真实项目的 `run.sh` 每天要用几十次）：

写一个 `deploy.sh`，一条命令完成"编译→推送→赋权→运行→抓日志"：

```bash
./deploy.sh                    # 用默认设置跑
./deploy.sh --no-build         # 跳过编译，直接推送现有产物
./deploy.sh --log 3            # 运行后抓 3 秒日志再退出
./deploy.sh --clean            # 清理设备上的旧产物
```

**接口骨架**（你来填实现）：

```bash
#!/bin/bash
set -e   # 任一步失败即退出

# 1. 解析参数（--no-build / --log N / --clean）
#   提示：用 case 或 getopts

# 2. 若未 --no-build：调 ndk-build

# 3. 确认设备连接（adb devices 里有 device 状态）

# 4. 推送到 /data/local/tmp/（若 --clean 先 rm 旧文件）

# 5. chmod 755

# 6. 运行（若 --log，后台运行 + 抓 N 秒 logcat + 杀掉）
```

**自主实现要求**（对照本项目的 `scripts/run.sh` 思路）：

| 要求 | 考察点 |
|---|---|
| `set -e` 让任一步失败即停 | 脚本健壮性 |
| 设备未连接时**明确报错**而非继续 | 环境检查 |
| 支持 `--no-build` 加速迭代 | 工程化（改一行不用重编）|
| 日志抓取用 `timeout` 而非死等 | 超时处理 |
| 路径用变量集中定义（如 `TARGET=...`）| 可维护性 |

**验收**：`./deploy.sh` 一条命令跑通全流程；`./deploy.sh --no-build` 跳过编译；设备没连时报错清晰。

## 本章小结

> [!abstract] 本章要点已收束
> 回头把本页的"动手"与结论串成一句话；有勾不上的验收项，回去补做。

## 验收清单

- [ ] 手机已连上，`adb devices` 显示 `device`（输出一行 xxx device）
- [ ] **成功 push 并运行了 `hello_arm64`，看到输出** ★（运行程序看到 hello from arm64）
- [ ] 会在 logcat 里过滤自己的 tag（`adb logcat -s HelloNDK` 只看到自己的日志）
- [ ] 知道崩溃时看 `Fatal signal` 和 `/data/tombstones/`（制造一次崩溃，在 logcat 找到 Fatal signal）
- [ ] 知道 `/sdcard` 不能执行、`/data/local/tmp` 可以（说出 noexec 原因）
- [ ] 写了自己的一键脚本，能一条命令跑完整流程（`./run.sh` 完成编译→推送→运行→抓日志）

→ 下一章：[[第36章-adb全套]]　—— 掌握 adb 的完整命令集，能把设备当成一台"远程 Linux 机器"来操作。
