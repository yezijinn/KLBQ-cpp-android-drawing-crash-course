---
tags: [教程, 卷五, Android, uinput]
day: 44
aliases: [ch70]
---

# 第 70 章 · uinput 虚拟输入

> [!abstract] 本章目标
> 学会用 `/dev/uinput` 创建一个虚拟触摸设备，并注入触摸事件。
> 这是 `TouchHelperA.cpp` 后半部分的内容。

> [!note] 承上
> 上一章看清了触摸链路。
> 本章学**主动注入触摸**——用 `/dev/uinput` 创建虚拟输入设备（自瞄点屏幕靠它）。

> [!important] 前置条件
> - 需要 **root 设备** + 内核开了 `CONFIG_INPUT_UINPUT`（多数定制内核默认开）。
> - 先确认：`adb shell su -c 'ls -l /dev/uinput'`，有该节点才能做。

## 先看 uinput 是什么

`uinput`（user input）是内核提供的机制：
**让用户态程序创建一个"假的"输入设备**，然后往里写事件，
内核会把这些事件当成真实硬件产生的，分发给所有监听者。

```
你的程序
    │  写 input_event
    ↓
/dev/uinput
    ↓
内核 input 子系统
    ↓  当成真实设备
/dev/input/eventN  ← 新创建的虚拟设备
    ↓
InputReader → InputDispatcher → App
```

## 创建虚拟触摸设备

```c
#include <linux/input.h>
#include <linux/uinput.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

int CreateVirtualTouch(int screenW, int screenH) {
    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (fd < 0) { perror("open /dev/uinput"); return -1; }

    // 1. 声明支持哪些事件类型
    ioctl(fd, UI_SET_EVBIT, EV_ABS);     // 绝对坐标
    ioctl(fd, UI_SET_EVBIT, EV_KEY);     // 按键
    ioctl(fd, UI_SET_EVBIT, EV_SYN);     // 同步

    // 2. 声明支持哪些键
    ioctl(fd, UI_SET_KEYBIT, BTN_TOUCH);
    ioctl(fd, UI_SET_KEYBIT, BTN_TOOL_FINGER);

    // 3. 声明支持哪些绝对轴（用 SET_ABSBIT，不用新版 UI_ABS_SETUP）
    ioctl(fd, UI_SET_ABSBIT, ABS_X);
    ioctl(fd, UI_SET_ABSBIT, ABS_Y);
    ioctl(fd, UI_SET_ABSBIT, ABS_MT_POSITION_X);
    ioctl(fd, UI_SET_ABSBIT, ABS_MT_POSITION_Y);
    ioctl(fd, UI_SET_ABSBIT, ABS_MT_TRACKING_ID);

    // 4. 设备属性 + 随机名字/ID（项目用随机值，避免被识别为固定虚拟设备）
    ioctl(fd, UI_SET_PROPBIT, INPUT_PROP_DIRECT);   // 直触屏（不是触摸板）

    struct uinput_user_dev ui_dev;                  // ★ 项目用的是老的 uinput_user_dev
    memset(&ui_dev, 0, sizeof(ui_dev));
    strncpy(ui_dev.name, "randomname", UINPUT_MAX_NAME_SIZE);
    ui_dev.id.bustype = 0;
    ui_dev.id.vendor  = rand() % 10 + 5;
    ui_dev.id.product = rand() % 10 + 5;
    ui_dev.id.version = rand() % 10 + 5;

    // 5. 设置各轴的取值范围（老式写法：填 ui_dev.absmin / absmax 数组）
    ui_dev.absmin[ABS_MT_POSITION_X] = 0;
    ui_dev.absmax[ABS_MT_POSITION_X] = screenW;     // ★ 与屏幕一致
    ui_dev.absmin[ABS_MT_POSITION_Y] = 0;
    ui_dev.absmax[ABS_MT_POSITION_Y] = screenH;
    ui_dev.absmin[ABS_X] = 0;
    ui_dev.absmax[ABS_X] = screenW;
    ui_dev.absmin[ABS_Y] = 0;
    ui_dev.absmax[ABS_Y] = screenH;
    ui_dev.absmin[ABS_MT_TRACKING_ID] = 0;
    ui_dev.absmax[ABS_MT_TRACKING_ID] = 65535;

    // 6. 一次性 write 整个 uinput_user_dev 结构体
    write(fd, &ui_dev, sizeof(ui_dev));

    // 7. 创建！
    if (ioctl(fd, UI_DEV_CREATE) < 0) {
        perror("UI_DEV_CREATE");
        close(fd);
        return -1;
    }

    printf("虚拟触摸设备创建成功\n");
    return fd;
}
```

顺序不能乱：**SET_EVBIT → SET_KEYBIT → SET_ABSBIT → SET_PROPBIT → 填 uinput_user_dev → write → DEV_CREATE**。

> [!note] 本项目用的是**老式 `uinput_user_dev`**，不是新版 `uinput_abs_setup`
> 新版 Linux 提供了 `UI_ABS_SETUP` + `UI_DEV_SETUP`（更清晰），
> 但**本项目 `TouchHelperA.cpp` 用的是老的 `uinput_user_dev` 结构体**：
> 把所有轴的范围填进 `ui_dev.absmin[] / absmax[]`，然后 `write(fd, &ui_dev, sizeof(ui_dev))` 一次性写入。
> 两种写法都能用，**以项目为准就是老式这套**。
>
> 项目还多做了一件事：**照抄真实触摸屏的能力**——用 `EVIOCGID` 读真实设备的 vendor/product/version，
> 用 `EVIOCGBIT(EV_KEY, ...)` 读它支持的按键位图，逐个 `UI_SET_KEYBIT` 抄过去。
> 目的：让虚拟设备和真实设备能力一致，App 不会因为它"缺少某个能力"而忽略它。

## 注入事件

```c
void EmitEvent(int fd, __u16 type, __u16 code, __s32 value) {
    struct input_event ev = {};
    gettimeofday(&ev.time, NULL);
    ev.type  = type;
    ev.code  = code;
    ev.value = value;
    write(fd, &ev, sizeof(ev));
}

void TouchDown(int fd, int slot, int x, int y) {
    EmitEvent(fd, EV_ABS, ABS_MT_SLOT, slot);
    EmitEvent(fd, EV_ABS, ABS_MT_TRACKING_ID, slot + 1000);  // 唯一 ID
    EmitEvent(fd, EV_ABS, ABS_MT_POSITION_X, x);
    EmitEvent(fd, EV_ABS, ABS_MT_POSITION_Y, y);
    EmitEvent(fd, EV_KEY, BTN_TOUCH, 1);
    EmitEvent(fd, EV_SYN, SYN_REPORT, 0);      // ★ 必须有
}

void TouchMove(int fd, int slot, int x, int y) {
    EmitEvent(fd, EV_ABS, ABS_MT_SLOT, slot);
    EmitEvent(fd, EV_ABS, ABS_MT_POSITION_X, x);
    EmitEvent(fd, EV_ABS, ABS_MT_POSITION_Y, y);
    EmitEvent(fd, EV_SYN, SYN_REPORT, 0);
}

void TouchUp(int fd, int slot) {
    EmitEvent(fd, EV_ABS, ABS_MT_SLOT, slot);
    EmitEvent(fd, EV_ABS, ABS_MT_TRACKING_ID, -1);    // -1 = 抬起
    EmitEvent(fd, EV_KEY, BTN_TOUCH, 0);
    EmitEvent(fd, EV_SYN, SYN_REPORT, 0);
}
```

> [!danger] `SYN_REPORT` 不能忘
> 没有同步信号，内核会把事件攒着不生效。
> 症状："写了但没反应"。

## 销毁设备

```c
ioctl(fd, UI_DEV_DESTROY);
close(fd);
```

**程序退出前必须销毁**，否则虚拟设备残留，系统里多一个无效输入设备。

## 本项目的封装

`TouchHelperA.cpp` 里的对应代码：

```cpp
// 初始化
nowfd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
ioctl(nowfd, UI_SET_PROPBIT, INPUT_PROP_DIRECT);
ioctl(nowfd, UI_SET_EVBIT, EV_ABS);
ioctl(nowfd, UI_SET_ABSBIT, ABS_X);
ioctl(nowfd, UI_SET_ABSBIT, ABS_Y);
ioctl(nowfd, UI_SET_ABSBIT, ABS_MT_POSITION_X);
ioctl(nowfd, UI_SET_ABSBIT, ABS_MT_POSITION_Y);
ioctl(nowfd, UI_SET_ABSBIT, ABS_MT_TRACKING_ID);
ioctl(nowfd, UI_SET_EVBIT, EV_SYN);
ioctl(nowfd, UI_SET_EVBIT, EV_KEY);
ioctl(nowfd, UI_SET_KEYBIT, BTN_TOUCH);
ioctl(nowfd, UI_SET_KEYBIT, BTN_TOOL_FINGER);
```

还有一段"复制物理设备能力"的逻辑：

```cpp
// 读取真实触摸屏支持的按键，照抄到虚拟设备
if (ioctl(fd, EVIOCGID, &id) == 0) {
    // ...
    res = ioctl(fd, EVIOCGBIT(EV_KEY, bits_size), bits);
    for (int j = 0; j < bits_size; j++)
        for (int k = 0; k < 8; k++)
            if (bits[j] & (1 << k))
                ioctl(nowfd, UI_SET_KEYBIT, j * 8 + k);   // 照抄
}
```

**为什么要照抄**：让虚拟设备和真实设备能力一致，
这样 App 不会因为它"缺少某个能力"而忽略它。

## 触摸槽位（slot）机制

多点触控用槽位区分手指：

```
slot 0: 手指 A
slot 1: 手指 B
...
```

每个槽位有独立的 `TRACKING_ID`：
- 按下：分配一个新的正数 ID
- 移动：ID 不变
- 抬起：发送 `-1`

本项目：

```cpp
struct touchObj {
    My_Vector2 pos{};
    int id = 0;
    bool isDown = false;
};

struct Device {
    int fd;
    float S2TX;
    float S2TY;
    input_absinfo absX, absY;
    touchObj Finger[10];      // 最多 10 个手指
};
```

## 权限

```bash
ls -l /dev/uinput
# crw-rw---- 1 root uhid 10, 223 uinput
```

需要 root 或者相应权限。有些 ROM 直接没有这个设备节点：

```bash
adb shell ls -l /dev/uinput
# ls: /dev/uinput: No such file or directory   ← 内核没开 CONFIG_INPUT_UINPUT
```

**没有 uinput 时用不了软件注入**，只能用内核驱动方案（第 78 章）。

## 限制与检测

| 限制 | 说明 |
|---|---|
| Android 有 `INJECT_EVENTS` 权限 | 系统签名或 root 才有 |
| 部分 ROM 禁用了 uinput | 内核没编译该模块 |
| 注入的事件可被区分 | 通过设备名/厂商 ID 能识别是虚拟设备 |
| 注入有延迟 | 经过内核 input 子系统，比真实触摸慢几毫秒 |

## 动手：虚拟点击器

```c
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <linux/input.h>
#include <linux/uinput.h>

// （插入上面的 CreateVirtualTouch / EmitEvent / TouchDown/Move/Up）

int main(int argc, char **argv) {
    int W = 1080, H = 2400;
    int fd = CreateVirtualTouch(W, H);
    if (fd < 0) return 1;

    sleep(1);   // 等设备被系统识别

    printf("3 秒后在屏幕中心点击...\n");
    sleep(3);

    int cx = W / 2, cy = H / 2;
    TouchDown(fd, 0, cx, cy);
    usleep(50000);              // 按住 50ms
    TouchUp(fd, 0);

    printf("点击完成\n");

    // 滑动演示
    printf("3 秒后滑动...\n");
    sleep(3);
    TouchDown(fd, 0, 200, 1200);
    for (int x = 200; x < 900; x += 50) {
        TouchMove(fd, 0, x, 1200);
        usleep(16000);
    }
    TouchUp(fd, 0);
    printf("滑动完成\n");

    ioctl(fd, UI_DEV_DESTROY);
    close(fd);
    return 0;
}
```

测试方法：打开一个能画画的 App 或者"开发者选项→指针位置"，
看有没有出现触摸轨迹。

```bash
# 开启指针位置显示（超好用）
adb shell settings put system pointer_location 1
```

## 动手验证清单

- [ ] **开 uinput**：`adb shell ls -l /dev/uinput` → 设备存在
- [ ] **创建虚拟设备**：写 uinput ioctl → 创建虚拟触摸屏
- [ ] **注入点击**：向 uinput 写 `EV_ABS` + `EV_SYN` → 屏幕上出现一次点击
- [ ] **验证在游戏里生效**：注入的点击能被游戏接收
- [ ] **清理**：关闭 fd → 虚拟设备消失

> [!danger] 需要 root
> 开 `/dev/uinput` 一般需要 root 权限。本项目把触摸转发做成可选功能（第 70 章）。

## 课后习题

### 习题 70.1 让虚拟设备"点"一个坐标（★★，应用变式）

**任务要求**：基于本章的 `CreateVirtualTouch`，实现 `Tap(x, y)`：在 (x,y) 处模拟一次按下+抬起。

**参考骨架**：

```cpp
void Tap(int fd, int x, int y) {
    struct input_event ev{};
    // 按下
    ev.type = EV_ABS; ev.code = ABS_X; ev.value = x; write(fd, &ev, sizeof(ev));
    ev.type = EV_ABS; ev.code = ABS_Y; ev.value = y; write(fd, &ev, sizeof(ev));
    ev.type = EV_KEY; ev.code = BTN_TOUCH; ev.value = 1; write(fd, &ev, sizeof(ev));
    ev.type = EV_SYN; ev.code = SYN_REPORT; ev.value = 0; write(fd, &ev, sizeof(ev));
    // 抬起
    ev.type = EV_KEY; ev.code = BTN_TOUCH; ev.value = 0; write(fd, &ev, sizeof(ev));
    ev.type = EV_SYN; ev.code = SYN_REPORT; ev.value = 0; write(fd, &ev, sizeof(ev));
}
```

**验证断言**：调用后屏幕上对应位置出现一次"点击"（如点亮某个按钮）。

### 习题 70.2 分析"uinput 的权限与风险"（★★★，分析+评价）

**任务要求**：分析：

1. 为什么 `/dev/uinput` 只有 root 能写？
2. 一个能写 uinput 的程序，**理论上能做什么**？
3. 从系统安全角度，这个设计合理吗？

**参考答案要点**：
1. 因为它能**伪造任意输入**，等于控制整台设备的交互；
2. 能模拟任意点击/滑动/按键，可操作任何 App；
3. 合理——**输入伪造是高权限操作**，必须限制在 root。

> [!tip] 评价层要点
> 理解"**能力越大，权限越严**"的安全设计逻辑。
> 本项目的 uinput 用法（转发真实触摸）是正当用途，但同一机制可被滥用——所以系统把它锁在 root。

## 本章小结

> [!abstract] 本章要点已收束
> 回头把本页的"动手"与结论串成一句话；有勾不上的验收项，回去补做。

> [!danger] 【安全最佳实践】输入注入的双刃剑
> `/dev/uinput` 能**伪造触摸/按键**——可用于自动化测试、无障碍辅助，也可被滥用。
> **威胁机制**：注入输入可绕过用户意图（如自动点击确认、游戏外挂）。
> **规避原则**：
> - **只在自己设备/自己授权**的场景使用；
> - 自动化测试里注入输入是**正当用途**（第 70 章的原始目标）；
> - 生产代码加**开关 + 用户确认**，不做静默注入；
> - 遵循平台政策（如游戏反作弊协议）。

## 验收清单

- [ ] 知道 uinput 是"用户态创建虚拟输入设备"的机制（说出"内核给一个 /dev/uinput 接口"）
- [ ] 能说出创建虚拟触摸设备的七个步骤和顺序（UI_SET_EVBIT → ... → UI_DEV_CREATE）
- [ ] 知道 `SYN_REPORT` 必须发（说出"不发内核不处理这批事件"）
- [ ] 知道 `TRACKING_ID = -1` 表示抬起（说出抬起时的写法）
- [ ] 理解多点触控的 slot 机制（说出每个 slot 存一个触点）
- [ ] 知道销毁时要 `UI_DEV_DESTROY`（说出不销毁的后果）
- [ ] 知道某些 ROM 可能没有 `/dev/uinput`（说出检查方式）
- [ ] 跑通了虚拟点击器，看到点击/滑动生效 ★（运行程序，屏幕出现点击/滑动）

→ 下一章：[[第71章-读取并转发真实触摸]]　—— 实现"独占读取 + 处理后重新注入"的完整回路，让覆盖层既能接收触摸，又不妨碍游戏操作。
