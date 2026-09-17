---
tags: [教程, 卷七, 引擎, 相机]
day: 56
aliases: [ch90]
---

# 第 90 章 · 相机与 POV 数据

> [!abstract] 本章目标
> 理解 `CameraCache` 和 `ControlRotation` 的区别，
> 以及为什么本项目自瞄要区分"写哪个"。

> [!note] 承上
> 上一章保证了"读的数据不崩"。
> 本章读**相机数据**——`CameraCache` 和 `ControlRotation` 的区别，自瞄写哪个。

## 先看 UE4 里的两套相机数据

| 数据 | 位置 | 含义 |
|---|---|---|
| **ControlRotation** | `PlayerController + 0x378` | 玩家**输入**的朝向（你想看哪） |
| **CameraCache / POV** | `PlayerCameraManager + 0x2300` 起 | 相机**实际**的位置和朝向 |

**区别**：
- ControlRotation 是"意图"
- POV 是"结果"

正常情况下两者一致。但：
- 过场动画时，POV 被脚本控制，ControlRotation 不变
- 开镜时 FOV 变化，可能存在插值
- 载具/特殊状态下可能不同步

## POV 结构

```
CameraManager + 0x2300  →  Location (Vec3)      相机位置
               + 0x230C  →  Rotation (Rotator)  相机朝向
               + 0x2318  →  FOV (float)         视野角度
```

本项目读取：

```cpp
if (玩家相机 != 0) {
    dr->Read((uintptr_t)玩家相机 + 0x2300, &相机位置, sizeof(相机位置));
    dr->Read((uintptr_t)玩家相机 + 0x230C, &相机旋转, sizeof(相机旋转));
    相机FOV = dr->Read<float>((uintptr_t)玩家相机 + 0x2318);
}
```

**注意三个偏移是连续的**（0x2300、0x230C、0x2318），
间距分别是 12（Vec3）和 12（Rotator）。

> [!tip] 可以一次读完
> ```cpp
> struct CameraCache {
>     Vec3    location;     // 0x00
>     Rotator rotation;     // 0x0C
>     float   fov;          // 0x18
> };
> CameraCache c = dr->Read<CameraCache>(cam + 0x2300);
> ```
> 一次 I/O 而不是三次。

## 获取 CameraManager 的链

```
UWorld
  ↓ +0x188
GameInstance (或 OwningGameInstance)
  ↓ +0x38
LocalPlayers 数组
  ↓ +0x00
LocalPlayer
  ↓ +0x30
PlayerController
  ↓ +0x3A8
PlayerCameraManager
  ↓ +0x2300
CameraCache
```

本项目代码：

```cpp
PlayerController = dr->Read<uint64_t>(
    dr->Read<uint64_t>(
        dr->Read<uint64_t>(
            dr->Read<uint64_t>(Uworld + 0x188) + 0x38) + 0x0) + 0x30);

MySelf  = dr->Read<uint64_t>(PlayerController + 0x390);   // 自己的 Pawn
玩家相机 = dr->Read<uint64_t>(PlayerController + 0x3A8);   // CameraManager
```

**五级指针链，每级都可能失败。**

## mBase：本项目的相机数据汇总

```cpp
struct mBase {
    uintptr_t libUE4{};
    uintptr_t UWorld{};
    uintptr_t PlayerController{};
    uintptr_t AcknowledgedPawn{};
    uintptr_t CameraManager{};
    uintptr_t CameraCache{};
    uintptr_t PovPtr{};
    Vec3 Location;
    Rotator Rotation{};
    float Fov{};
};

extern mBase AppBase;
```

`pXthread()` 每帧刷新：

```cpp
void pXthread() {
    if (!初始化 || dr->GetGlobalPid() <= 0 || AppBase.libUE4 <= 0x10000) {
        return;
    }
    AppBase.UWorld           = Uworld;
    AppBase.PlayerController = PlayerController;
    AppBase.AcknowledgedPawn = MySelf;
    AppBase.CameraManager    = 玩家相机;
    AppBase.CameraCache      = 玩家相机 + 0x2300;
    AppBase.PovPtr           = AppBase.CameraCache;
    AppBase.Location         = Vec3(相机位置.X, 相机位置.Y, 相机位置.Z);
    AppBase.Rotation         = Rotator{相机旋转.X, 相机旋转.Y, 相机旋转.Z};
    AppBase.Fov              = 相机FOV;

    GameTools::WorldMatrix = GameTools::rotatorToMatrix(AppBase.Rotation);
}
```

**集中在一处刷新，其它地方只读 `AppBase`**——
避免每帧重复读同一份数据（第 81 章的原则）。

## ControlRotation：自瞄写入点

```cpp
#define PC_CONTROLROTATION 0x378
#define POV_LOCATION       0x2300
#define POV_ROTATION       0x230C
#define POV_FOV            0x2318
```

本项目的自瞄有两种写法：

```cpp
inline bool 写入角度(const Rotator &角) {
    // 1. 总是写 ControlRotation
    dr->Write<float>(AppBase.PlayerController + PC_CONTROLROTATION + 0x0, 角.Pitch);
    dr->Write<float>(AppBase.PlayerController + PC_CONTROLROTATION + 0x4, 角.Yaw);
    dr->Write<float>(AppBase.PlayerController + PC_CONTROLROTATION + 0x8, 角.Roll);

    // 2. 可选：同时写相机 POV
    if (Cfg.写相机POV) {
        dr->Write<float>(AppBase.CameraManager + POV_ROTATION + 0x0, 角.Pitch);
        dr->Write<float>(AppBase.CameraManager + POV_ROTATION + 0x4, 角.Yaw);
        dr->Write<float>(AppBase.CameraManager + POV_ROTATION + 0x8, 角.Roll);
    }
    return true;
}
```

**为什么要区分**：

| 只写 ControlRotation | 同时写 POV |
|---|---|
| 画面不立即变（等引擎下一帧同步） | 画面立即跟着动 |
| 更"静默" | 更"强锁" |
| 引擎可能覆盖你的值 | 引擎也可能覆盖 |

UI 上的说明：

```cpp
ImGui::Checkbox("写相机POV(会强锁!)", &ConfigManager::Settings.自瞄写相机POV);
if (ImGui::IsItemHovered())
    ImGui::SetTooltip("勾选=同时改相机POV, 屏幕视角会跟着动(强锁)\n"
                      "不勾=只改ControlRotation, 真静默");
```

## FOV 的作用

```
FOV 越大 → 视野越广 → 物体在屏幕上越小
FOV 越小 → 视野越窄 → 物体越大（开镜效果）
```

本项目用 FOV 算投影矩阵（第 50 章）：

```cpp
float t = tanf(camViewInfo.FOV * 0.5f * PI / 180.0f);
```

**开镜时 FOV 会变**——所以要每帧读，不能缓存。

常见 FOV 值：

| 状态 | FOV |
|---|---|
| 常规 | 80~100 |
| 开镜（2倍） | 40~50 |
| 开镜（8倍） | 10~15 |

## 相机校验

读到的相机数据可能是垃圾。本项目在 `camMakeMatrix` 里做了检查：

```cpp
if (camViewInfo.FOV < 1.0f || camViewInfo.FOV > 179.0f) return;
if (camX == 0.0f && camY == 0.0f && camZ == 0.0f) return;
if (camX != camX || camY != camY || camZ != camZ) return;   // NaN
```

三条：

| 检查 | 防什么 |
|---|---|
| FOV 在 1~179 | 除零、异常值 |
| 位置不全为 0 | 读到空数据（未初始化/切场景） |
| `x != x` | NaN |

**注意**：因为 `return` 而不更新矩阵，
所以上一帧的矩阵会继续用——**比崩溃好，比画错好**。

## 实战：验证相机数据

在 UI 上显示出来（本项目"信息"页就是这么做的）：

```cpp
ImGui::Text("相机位置: X %.2f  Y %.2f  Z %.2f",
            相机位置.X, 相机位置.Y, 相机位置.Z);
ImGui::Text("相机旋转: X %.2f  Y %.2f  Z %.2f",
            相机旋转.X, 相机旋转.Y, 相机旋转.Z);
ImGui::Text("FOV: %.2f", 相机FOV);
```

**验证方法**：
1. 在游戏里转身，看 Yaw 是否跟着变（0~360 循环）
2. 抬头低头，看 Pitch 是否变（-90~90）
3. 开镜，看 FOV 是否变小
4. 走动，看位置是否变

**四项都对，说明偏移没错。**

## 给 demo 引擎加相机

```cpp
struct PlayerCameraManager {
    Vec3    cacheLocation;      // 0x2300 相对 CameraManager
    Rotator cacheRotation;
    float   cacheFOV;
    // 前面留 0x2300 字节
};

struct PlayerController {
    Rotator controlRotation;    // 0x378
    uint64_t pawn;              // 0x390
    uint64_t cameraManager;     // 0x3A8
};
```

让 demo 每帧更新相机（模拟玩家操作），
然后用外部程序读——**对比 demo 打印的值**。

## 动手验证清单

> [!important] 前置条件
> 下面的验证需**读取真实游戏进程的相机数据**——要有 root 设备 + 已定位 `CameraCache` 偏移（第 87/88 章）。
> **无设备**：读懂 `MinimalViewInfo` 结构 + 用附录 I 的 demo 引擎模拟即可。

- [ ] **读相机位置**：从 `CameraCache` 读 `POV.Location` → 打印 3 个 float
- [ ] **读相机旋转**：读 `POV.Rotation` → 打印 pitch/yaw/roll
- [ ] **读 FOV**：读 `POV.FOV` → 打印（注意**每帧读**，开镜会变）
- [ ] **读控制器旋转**：读 `ControlRotation` → 与 POV.Rotation 对比
- [ ] **区分两者**：说出 CameraCache 是"实际视角"、ControlRotation 是"玩家输入"
- [ ] **UI 验证**：把相机数据画到屏幕上，与游戏实际视角对照 ★

> [!warning] FOV 不能缓存
> 开镜/切换武器时 FOV 会变。缓存会导致 W2S 偏移，ESP 框"飘"。
> 本项目每帧重读 FOV（第 90、91 章）。

## 本章小结

> [!abstract] 本章要点已收束
> 回头把本页的"动手"与结论串成一句话；有勾不上的验收项，回去补做。

## 验收清单

- [ ] 知道 ControlRotation 与 CameraCache/POV 的区别（说出"逻辑朝向 vs 实际渲染朝向"）
- [ ] 知道 POV 三个字段是连续的（可一次读完）（说出三个字段共 28 字节：Vec3 12 + Rotator 12 + float 4，一次读 28 字节而非三次）
- [ ] 能画出 PlayerController 的五級获取链（画出五级指针链）
- [ ] 理解"只写 ControlRotation"与"同时写 POV"的差别（说出后者画面会跟着动）
- [ ] 知道 FOV 必须每帧读（开镜会变）（说出为什么不能缓存）
- [ ] 知道 `camMakeMatrix` 的三条防御各自防什么（逐条说出）
- [ ] 会在 UI 上显示相机数据并验证其正确性 ★（UI 显示位置/朝向/FOV，与游戏一致）

→ 下一章：[[第91章-投影矩阵的两种来源]]　—— 理解"读引擎矩阵"和"自己算矩阵"的取舍，以及本项目为什么两个都保留。
