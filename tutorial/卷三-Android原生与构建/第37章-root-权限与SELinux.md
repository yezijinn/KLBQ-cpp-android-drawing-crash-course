---
tags: [教程, 卷三, Android, root, SELinux]
day: 24
aliases: [ch37]
---

# 第 37 章 · root、权限与 SELinux

> [!abstract] 本章目标
> 理解 Android 的三层权限关卡（UID / capability / SELinux），
> 知道为什么"明明是 root 还是被拒"。

> [!note] 承上
> 上一章掌握 adb 后，你会发现有些操作**即使 root 也被拒**。
> 本章揭开 Android 的**三层权限关卡**：DAC / Capabilities / SELinux。

## 先看一个令人困惑的现象

```bash
$ adb shell su -c id
uid=0(root) gid=0(root)

$ adb shell su -c "cat /proc/1234/mem"
cat: /proc/1234/mem: Permission denied      ← ？？？
```

已经是 root 了，为什么还被拒？答案是 **SELinux**。

## 三道关卡

> [!note] 两个缩写：DAC 与 MAC
> - **DAC = Discretionary Access Control**（自主访问控制）：就是第 04 章讲的 `rwx` 权限位——**由文件主人自己决定**谁能访问。`chmod` 改的就是它。
> - **MAC = Mandatory Access Control**（强制访问控制）：由**系统统一策略**强制规定，文件主人也不能随便改。SELinux 就是 MAC。
> 一句话：DAC 看"你是谁 + 文件权限"，MAC 看"策略允不允许"。

```
你的操作
   │
   ├─ 关卡 1：DAC（自主访问控制）
   │    "这个文件属于谁？你的 uid 匹配吗？"
   │    依据：文件权限位 rwxrwxrwx、uid/gid
   │
   ├─ 关卡 2：Capabilities
   │    "你有没有这个特权？"
   │    依据：进程的 capability 集合
   │
   └─ 关卡 3：MAC（强制访问控制）= SELinux ★
        "策略允许你这个域对那个类型做这个操作吗？"
        依据：SELinux 策略规则
```

**三关全过，操作才成功。** root 只帮你过了第 1、2 关。

## 关卡 1：DAC（Linux 传统权限）

```bash
ls -l /data/local/tmp/app
# -rwxr-xr-x 1 shell shell 123456  app
#  ↑  ↑  ↑
#  │  │  └─ 其它用户
#  │  └──── 同组
#  └─────── 主人
```

规则：
- 你是 root（uid 0）→ **直接跳过权限检查**
- 你是文件主人 → 应用"主人"那三位
- 你是同组 → 应用"组"那三位
- 否则 → "其它"那三位

Android 上每个 App 有独立 uid，所以默认情况下：
App A 读不到 App B 的文件，也读不到 App B 的 `/proc/<pid>/`。

## 关卡 2：Capabilities

Linux 把 root 的特权拆成了几十个独立开关：

| capability | 允许什么 |
|---|---|
| `CAP_SYS_PTRACE` | ptrace 任意进程 |
| `CAP_SYS_ADMIN` | 挂载、各种管理操作 |
| `CAP_SYS_MODULE` | 加载内核模块 |
| `CAP_DAC_OVERRIDE` | 绕过文件权限检查 |
| `CAP_NET_ADMIN` | 网络配置 |

```bash
adb shell cat /proc/self/status | grep Cap
# CapInh: 0000000000000000
# CapPrm: 0000000000000000
# CapEff: 0000000000000000     ← 全 0 = 没有特权

adb shell su -c "cat /proc/self/status | grep CapEff"
# CapEff: 0000003fffffffff     ← 全 1 = 全部特权
```

本项目内核驱动方案需要 `CAP_SYS_MODULE`（加载 .ko），
而 `process_vm_readv` 方案只需要普通 root 的 `CAP_SYS_PTRACE` 级别即可
（实际需要 ptrace 权限或同 uid + 权限）。

## 关卡 3：SELinux（最麻烦的一关）

SELinux 给每个进程和每个资源都打一个**标签**，然后用策略表决定"谁能碰谁"。

```bash
adb shell ls -Z /data/local/tmp/app
# u:object_r:shell_data_file:s0  app

adb shell ps -Z | grep surfaceflinger
# u:r:surfaceflinger:s0  system  1234 ...
```

标签格式：`user:role:type:level`，关键是 **type**（也叫 domain）。

| 进程域 | 能访问什么 |
|---|---|
| `u:r:shell:s0` | 有限（adb shell 默认） |
| `u:r:su:s0` | 较多（root shell） |
| `u:r:init:s0` | 很多 |
| `u:r:untrusted_app:s0` | 极少（普通 App） |

查看当前模式：

```bash
adb shell getenforce
# Enforcing   ← 强制执行
# Permissive  ← 只记日志不阻止（调试用）
```

切换（需 root）：

```bash
adb shell getenforce              # ★ 先记录当前状态！可能是 Enforcing 或 Permissive
adb shell su -c setenforce 0      # 临时关闭（重启失效）
adb shell su -c setenforce 1      # 恢复为强制模式
```

> [!warning] 关 SELinux 前先记录当前状态
> 执行 `setenforce 0` 前，先跑一次 `getenforce` 并**记下输出**。
> 因为有些定制 ROM 出厂就是 `Permissive`——如果你不记录，
> 最后执行 `setenforce 1` 会**意外打开** SELinux，反而制造新问题。
>
> **恢复时**：原本 `Enforcing` → `setenforce 1`；原本 `Permissive` → 保持 `setenforce 0`。
> 不确定就跑 `adb reboot`——`setenforce` 不写盘，重启即回默认。

> [!danger] 不要建议用户永久关闭 SELinux
> 那是把整个系统的安全机制拆掉。
> 本教程的所有练习都在"临时 Permissive 或正确配置策略"下完成。
> 排错时可以 `setenforce 0` 确认"是不是 SELinux 的问题"，
> 确认后应该恢复 Enforcing，而不是永久关掉。

## 怎么知道是不是 SELinux 拦的

拒绝会在内核日志里留下 `avc: denied`：

```bash
adb shell su -c dmesg | grep avc
# 或
adb logcat -b all | grep avc
```

典型输出：

```
avc: denied { read } for pid=1234 comm="klbq" 
     scontext=u:r:shell:s0 
     tcontext=u:object_r:app_data_file:s0 
     tclass=file permissive=0
```

逐段解读：

| 字段 | 含义 |
|---|---|
| `{ read }` | 想做的操作 |
| `comm="klbq"` | 哪个进程 |
| `scontext` | **主体**（谁）：`shell` 域 |
| `tcontext` | **客体**（对谁）：`app_data_file` 类型 |
| `tclass=file` | 客体类别 |
| `permissive=0` | 0 = 真的拦了；1 = 只警告 |

**看到 `avc: denied` 就 100% 确定是 SELinux。**

## 常见绕过方式（按风险递增）

| 方式 | 做法 | 适用场景 |
|---|---|---|
| 换个域运行 | 用 `su` 进入 `u:r:su:s0` 域 | 首选 |
| 换位置 | 放到 SELinux 宽松的目录 | `/data/local/tmp` 通常比 `/data/data` 宽松 |
| 临时 permissive | `setenforce 0` | **仅排错** |
| 加策略 | 写 `.te` 规则并加载到内核 | 正规方案（需内核支持） |
| 永久关闭 | 改 cmdline | **不要做** |

## root 方案简述

| 方案 | 原理 | 特点 |
|---|---|---|
| Magisk | 修改 boot 镜像 + 挂载 overlay | 主流，支持模块、可隐藏 |
| KernelSU | 内核态 root | 更底层，需特定内核 |
| APatch | 内核补丁 | 类似 KernelSU |

它们的共同点：提供一个 `su` 二进制，把调用者提升到 uid 0 并切换 SELinux 域。

> [!note] 本项目需要什么
> 1. **进程内存读写**：root 即可（`process_vm_readv` 需要 ptrace 权限）
> 2. **内核驱动方案**：需要加载 `.ko` → 需要 Magisk 模块或 KernelSU 支持
> 3. **悬浮窗（原生方式）**：需要 root 或系统签名
> 4. **触摸注入（uinput）**：需要写 `/dev/uinput` 的权限
>
> 本教程的练习都在**自有设备 + 已授权 root** 下进行。

## App 侧的运行时权限

原生可执行文件**不走** Android 的运行时权限体系（`requestPermissions`）。
那些权限是给 APK 用的。

但有些权限对应到 Linux 层：

| Android 权限 | Linux 层 |
|---|---|
| `INTERNET` | 加入 `inet` 组（`GID 3003`） |
| `READ_EXTERNAL_STORAGE` | 加入 `sdcard_r` 组 |
| `CAMERA` | 加入 `camera` 组 |
| `WRITE_EXTERNAL_STORAGE` | 文件组 |

原生程序只要 uid/gid 对，不需要"申请权限"。root 下更是什么都有。

```bash
adb shell su -c id
# uid=0(root) gid=0(root) groups=1007(log),1004(input),... 
# context=u:r:su:s0
```

注意 `groups` 里有 `3003(inet)` 等，那些就是"权限"的底层实现。

## 动手：诊断一次权限拒绝

任务：故意触发三种拒绝，分别识别它们。

```bash
# 1. DAC 拒绝（没执行权限）
adb push hello /data/local/tmp/noperm
adb shell /data/local/tmp/noperm
# → Permission denied（DAC）

# 2. 没 root
adb shell cat /proc/1/cmdline
# → 可能 Permission denied（DAC/uid）

# 3. SELinux
adb shell su -c "cat /proc/1/mem" 2>&1
adb shell su -c dmesg | grep avc | tail -3
# → 看有没有 avc: denied
```

每种都记录下来：错误文本、当时你的 uid、是不是 root、有没有 avc 日志。

## 排错决策树

```
操作被拒
  │
  ├─ 你是不是 root？ → adb shell id
  │    ├─ 不是 → 加 su
  │    └─ 是 ↓
  │
  ├─ 有 avc: denied 日志吗？ → dmesg | grep avc
  │    ├─ 有 → SELinux 问题
  │    │        ├─ 临时 setenforce 0 验证
  │    │        └─ 确认后恢复，改用合规路径
  │    └─ 没有 ↓
  │
  ├─ 文件权限位对吗？ → ls -l
  │    └─ chmod
  │
  └─ capability 够吗？ → /proc/self/status CapEff
```

## 动手验证清单

- [ ] **看当前模式**：`adb shell getenforce` → `Enforcing` 或 `Permissive`（**记下来**）
- [ ] **制造 DAC 拒绝**：`adb shell cat /data/data/some_app/file` → `Permission denied`（无 avc）
- [ ] **制造 SELinux 拒绝**：`adb shell su -c 'dmesg | grep avc'` → 看 `avc: denied` 日志
- [ ] **临时诊断**（有 root）：`adb shell su -c setenforce 0` → 确认是不是 SELinux 拦的
- [ ] **恢复**：按之前记录的状态 `setenforce 1`（或原本就是 Permissive 则不动）
- [ ] **确认恢复**：`adb shell getenforce` → 与最初记录一致

> [!danger] 关 SELinux 前必须记录原状态
> 有些定制 ROM 出厂就是 `Permissive`。不记录就 `setenforce 1` 会**意外打开** SELinux。
> 不确定就 `adb reboot`——`setenforce` 不写盘，重启即回默认。

## 验收清单

- [ ] 能说出 Android 的三层权限关卡（DAC → Capabilities → SELinux）
- [ ] 知道 root 只解决前两层，SELinux 是第三层（说出"root 也过不了 avc 策略"）
- [ ] 会用 `getenforce` 查 SELinux 模式（输出 Enforcing 或 Permissive）
- [ ] 会看 `avc: denied` 日志，能说出 scontext/tcontext 的含义（指出主体/客体）
- [ ] 知道 `setenforce 0` 只能用于临时诊断，不该永久关闭（说出原因）
- [ ] 能区分 DAC 拒绝和 SELinux 拒绝（看有没有 avc 日志）
- [ ] 完成三种拒绝的诊断练习（DAC/root/SELinux 各制造一次并识别）

→ 下一章：[[第38章-logcat与android-log-h]]　—— 建立一套好用的原生日志系统，能分级、能格式化、能一键关闭。
