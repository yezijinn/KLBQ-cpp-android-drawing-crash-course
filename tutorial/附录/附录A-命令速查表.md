---
tags: [教程, 附录, 速查]
aliases: [附录A, cheatsheet]
---

# 附录 A · 命令速查表

> [!abstract] 怎么用
> 按场景分类，需要时直接搜。所有命令都在 Windows（Git Bash）或 adb shell 下验证过。

> [!warning] 先看你在哪个终端里
> 本表默认**在 Git Bash 里敲**（第 02 章装的，右键 "Open Git Bash here"）。
> 如果你在 **PowerShell / cmd** 里敲 `gcc`、`make`、`nm`、`adb` 等，报"不是内部或外部命令"，
> **不是命令写错，而是终端不对**——切到 Git Bash 再试。
> 三个终端的命令互不通用；完整对照表见 [[附录R-绝对零基础前置知识]] 第 3.2 节。
> 另外：**PowerShell 里查命令位置要用 `Get-Command gcc`（或 `where.exe gcc`），不能只敲 `where gcc`**。

## adb

```bash
# 连接与设备
adb devices                      # 列出设备
adb devices -l                   # 详细信息
adb -s <serial> shell            # 多设备指定
adb kill-server && adb start-server   # 重启（解玄学问题）
adb root                         # 重启 adbd 为 root（userdebug/eng）
adb shell su -c "命令"            # 通用 root 方式

# 文件
adb push <本地> <远程>
adb pull <远程> [本地]
adb shell chmod 755 /data/local/tmp/app
adb shell ls -ld /data/local/tmp

# 进程
adb shell ps -A | grep <name>
adb shell ps -A -o PID,USER,NAME
adb shell kill -9 <pid>
adb shell top -n 1

# 日志
adb logcat                       # 实时
adb logcat -d                    # dump 后退出
adb logcat -c                    # 清空
adb logcat -s <TAG>              # 按 tag
adb logcat TAG:D *:S             # 级别过滤
adb logcat -v time               # 带时间
adb logcat -b crash              # 崩溃缓冲

# 系统
adb shell getprop ro.build.version.sdk       # API 等级
adb shell getprop ro.build.version.release   # Android 版本
adb shell getprop ro.product.cpu.abi         # 主 ABI
adb shell getenforce                         # SELinux 模式
adb shell id / adb shell su -c id            # 当前身份
adb shell dumpsys SurfaceFlinger             # 图层列表 ★
adb shell dumpsys display                    # 显示设备
adb shell dumpsys meminfo <pid>              # 内存占用

# 其它
adb shell screencap -p /sdcard/s.png         # 截图
adb shell screenrecord --time-limit 5 /sdcard/v.mp4
adb shell settings put system pointer_location 1   # 显示触摸点 ★
adb shell input tap 500 1000                 # 模拟点击
adb shell input keyevent KEYCODE_BACK
```

## ndk-build

```bash
ndk-build                        # 增量构建
ndk-build -j8                    # 并行
ndk-build clean                  # 清理
ndk-build -B                     # 强制全量
ndk-build V=1                    # 打印完整命令 ★
ndk-build NDK_DEBUG=1            # 调试版
ndk-build APP_ABI=arm64-v8a      # 覆盖变量
```

## 编译器（直接调用 clang）

```bash
# 路径
$NDK/toolchains/llvm/prebuilt/windows-x86_64/bin/
    aarch64-linux-android21-clang.cmd       # 编译
    aarch64-linux-android-addr2line.exe
    aarch64-linux-android-nm.exe
    aarch64-linux-android-objdump.exe
    aarch64-linux-android-readelf.exe
    aarch64-linux-android-strip.exe

# 编译
aarch64-linux-android21-clang hello.c -o hello -llog
#                        ↑ 21 = 最低 API 等级
```

## 二进制分析

```bash
# nm：查符号
nm -C <file>                     # -C 解修饰
nm -g <file>                     # 只全局
nm -D <file>                     # 动态符号表
nm -u <file>                     # 未定义符号（依赖什么）
nm --size-sort -S <file>         # 按大小排序（找占地方的）
nm -A libfoo.a | grep ' T xxx'   # 符号在哪个 .o

# readelf：查结构
readelf -h <file>                # ELF 头（架构/类型/入口）
readelf -l <file>                # 程序头（段）
readelf -S <file>                # 节头
readelf -d <file>                # 动态段（依赖库）★
readelf --dyn-syms <file>        # 动态符号
readelf -r <file>                # 重定位表

# objdump
objdump -d <file>                # 反汇编
objdump -d --start-address=0x1000 --stop-address=0x1100 <file>
objdump -h <file>                # 节头

# 其它
size -A <file>                   # 各节大小
file <file>                      # 文件类型
strip <file>                     # 删符号
c++filt <mangled>                # 解修饰单个符号 ★
ar t libfoo.a                    # 列出静态库成员
```

## 崩溃定位

```bash
# 1. 从 logcat 拿到崩溃地址
adb logcat | grep -E 'SIGSEGV|backtrace'
# #00 pc 000000000000f234  /data/local/tmp/app

# 2. 地址转源码行
aarch64-linux-android-addr2line -e app -f -C 0xf234
# 输出：函数名 + 文件:行号

# 3. 看附近的汇编
aarch64-linux-android-objdump -d app | grep -A20 -B5 'f234:'

# 4. 看 tombstone
adb shell ls -lt /data/tombstones/ | head
adb shell cat /data/tombstones/tombstone_00 | head -40
```

## 本机开发

```bash
# 编译四步
gcc -E hello.c -o hello.i        # 预处理
gcc -S hello.i -o hello.s        # 编译
gcc -c hello.s -o hello.o        # 汇编
gcc hello.o -o hello             # 链接

# 常用选项
gcc -Wall -Wextra hello.c -o hello      # 警告
gcc -g -O0 hello.c -o hello             # 调试
gcc -O2 -DNDEBUG hello.c -o hello       # 发布
gcc -fsanitize=address -g hello.c -o h  # 地址检查 ★
gcc -fsanitize=undefined -g ...         # 未定义行为检查
gcc -fsanitize=thread -g ...            # 线程竞争检查

# Makefile
make                             # 构建
make clean
make -j8
make V=1                         # 打印命令
```

## 进程与内存（设备端）

```bash
cat /proc/<pid>/maps             # 内存映射 ★
cat /proc/<pid>/status           # 状态（含 TracerPid）
cat /proc/<pid>/cmdline          # 启动命令（Android 上是包名）
ls /proc/<pid>/task/             # 所有线程
cat /proc/self/status | grep TracerPid   # 是否被调试

adb shell getevent -l            # 观察原始输入事件 ★
adb shell getevent -p            # 列出输入设备
ls -l /dev/input/                # 输入设备节点
ls -l /dev/uinput                # 虚拟输入（可能不存在）
```

## Linux 常用

```bash
# 文件权限
chmod 755 <file>                 # rwxr-xr-x
chmod 644 <file>                 # rw-r--r--
chown root:root <file>

# 权限位速算
# r=4 w=2 x=1
# 755 = rwxr-xr-x   644 = rw-r--r--   660 = rw-rw----

# 编码处理
dos2unix <file>                  # CRLF → LF
file <file>                      # 查看编码
iconv -f GBK -t UTF-8 <in> > <out>
```

## 常用 errno

| errno | 值 | 含义 |
|---|---|---|
| `EPERM` | 1 | 权限不足 |
| `ENOENT` | 2 | 文件不存在 |
| `ESRCH` | 3 | 进程不存在 |
| `EIO` | 5 | IO 错误 |
| `EFAULT` | 14 | 地址不可访问 |
| `EINVAL` | 22 | 参数无效 |
| `ENOMEM` | 12 | 内存不足 |
| `EACCES` | 13 | 权限被拒 |

## 系统调用号（process_vm）

| 架构 | readv | writev |
|---|---|---|
| aarch64 | 270 | 271 |
| arm | 376 | 377 |
| x86_64 | 310 | 311 |
| i386 | 347 | 348 |

## Vulkan 常用

```bash
# 检查设备是否支持
adb shell dumpsys SurfaceFlinger | grep -i 'hwc\|gpu'
# 列出库
ls /system/lib64/libvulkan.so
```

| 常量 | 值 / 含义 |
|---|---|
| `VK_KHR_SURFACE_EXTENSION_NAME` | 实例扩展（必需） |
| `VK_KHR_ANDROID_SURFACE_EXTENSION_NAME` | 实例扩展（必需） |
| `VK_KHR_SWAPCHAIN_EXTENSION_NAME` | 设备扩展（必需） |
| `VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR` | 合成方式之一（本项目**未用**，透明靠 RGBA 图层 + 清屏 alpha=0） |
| `VK_PRESENT_MODE_FIFO_KHR` | 垂直同步（一定支持） |
| `VK_PRESENT_MODE_MAILBOX_KHR` | 低延迟 |
| `RTC_INVALID_GEOMETRY_ID` | Embree 未命中标记 |

→ 返回 [[00-开始之前]]
