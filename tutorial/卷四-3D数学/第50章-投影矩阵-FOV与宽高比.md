---
tags: [教程, 卷四, 数学, 投影]
day: 31
aliases: [ch50]
---

# 第 50 章 · 投影矩阵、FOV 与宽高比

> [!abstract] 本章目标
> 理解 `t = tan(FOV/2)` 的几何意义，
> 以及为什么本项目让"垂直焦距"由屏幕半高决定。

> [!note] 承上
> 上一章把世界转到了"相机空间"。
> 本章做**投影**——把相机空间的三维点压成屏幕上的二维点（靠 `t = tan(FOV/2)`）。

## 先看一个三角形

相机在原点朝前看，FOV 是视野张角。

```
        上边界
          /|
         / |
        /  |
       /   |  屏幕高度的一半 (H/2)
      /    |
     /     |
    /θ/2   |
   ┌───────┤  ← 成像平面，距离 = d
   │       |
   相机    深度 d
```

`θ = FOV`（**这里按垂直方向推导**，先讲清楚 t 从哪来）。由三角函数：

```
tan(θ/2) = (H/2) / d
```

所以屏幕高度 H = 2 · d · tan(θ/2)。

**定义 `t = tan(FOV/2)`**：它把"深度"换算成"**归一化后的半屏尺寸**"（NDC 里是 1）。
到这里 t 是从垂直 FOV 推出来的。

> [!note] 横向为什么也用同一个 t
> 本项目横向、纵向**共用同一个 t**（见下方"投影的核心公式"和源码）。
> 这意味着：它假设横向的 FOV 与纵向相同（即 NDC 空间里两个方向的张角一致）。
> 真实的屏幕宽高比差异，**不靠 t 体现，而靠最后分别乘 `px`（半宽）/ `py`（半高）来补偿**。
> 这是本项目的一个约定（与游戏真实相机未必完全一致），所以 UI 里才需要两个"焦距系数"来微调。

## 投影的核心公式

给定相机空间的一点。回顾第 49 章的相机空间约定：**x = 前方（深度）、y = 右、z = 上**。
设深度为 `w`（就是相机空间的 x），右向分量为 `camY`、上向分量为 `camZ`：

| | 相对屏幕中心的**偏移量** |
|---|---|
| 横向偏移 | `(camY / w) / t × (屏幕半宽)` |
| 纵向偏移 | `-(camZ / w) / t × (屏幕半高)` |

> [!note] 这里算的是"偏移"，还要加上屏幕中心
> 上面两行给出的是**相对屏幕中心的偏移量**，不是最终坐标。
> 目标在相机正前方时（camY=camZ=0），偏移为 0，最终坐标就是屏幕中心 (px, py)。
> 所以完整坐标是 `屏幕中心 + 偏移`：
> ```
> screenX = 半宽 + (camY / w) / t × 半宽 = ((camY / w) / t + 1) × 半宽
> screenY = 半高 - (camZ / w) / t × 半高 = (1 - (camZ / w) / t) × 半高
> ```
> 这正是下面代码里 `(ndc_x + 1.0f) * px` 和 `(1.0f - ndc_y) * py` 的来历。

注意这里除以了 `t`：
- `t` 越大（FOV 越大）→ 同一位置在屏幕上越靠中间 → 视野越广 ✓
- `t` 越小（FOV 越小）→ 越靠边缘 → 视野越窄（望远）✓

**"除以 w"就是透视除法**：远的东西变小。

## 为什么叫"透视除法"

```
物体距离 10 米，高 2 米  →  屏幕上占 1/5 高度
物体距离 20 米，高 2 米  →  屏幕上占 1/10 高度（缩小一半）
```

`screenY ∝ 1 / w`。这就是透视。

本项目的代码：

```cpp
float w = matrix[3]*obj.X + matrix[7]*obj.Y + matrix[11]*obj.Z + matrix[15];
if (w < 0.001f) return false;         // 在相机后方或太近

float clip_x = matrix[0]*obj.X + matrix[4]*obj.Y + matrix[8]*obj.Z + matrix[12];
float clip_y = matrix[1]*obj.X + matrix[5]*obj.Y + matrix[9]*obj.Z + matrix[13];

float ndc_x = clip_x / w;
float ndc_y = clip_y / w;

screen.X = (ndc_x * 水平焦距系数 + 1.0f) * px;
screen.Y = (1.0f - ndc_y * 垂直焦距系数) * py;
```

逐行：

| 行 | 含义 |
|---|---|
| `w` | 深度（前方距离） |
| `clip_x` / `clip_y` | 已经除过 `t` 的相机空间坐标 |
| `ndc = clip / w` | 归一化设备坐标，范围约 [-1, 1] |
| `(ndc + 1) * px` | 从 [-1,1] 映射到 [0, 屏幕宽] |
| `1 - ndc_y` | **翻转 Y 轴**（屏幕 y 向下） |

> [!warning] 这里的 `w` 和齐次坐标的 `w` 不是一回事
> 第 46 章讲齐次坐标时，向量第 4 个分量叫 `w`（点是 1、方向是 0）。
> 本章代码里的 `w` 指的是**"点在相机前方多远"的深度**。
> 两者**恰好都叫 w**，但含义完全不同——一个是齐次分量，一个是深度。
> 读代码时看上下文：凡是出现在 `w = matrix[3]*x + ...` 里的，都是深度。

> [!note] Y 轴翻转
> 数学坐标系 y 向上，屏幕坐标系 y 向下。
> 所以是 `1 - ndc_y` 而不是 `1 + ndc_y`。

## 宽高比（Aspect Ratio）去哪了？

标准投影矩阵里会有 aspect = 宽/高。本项目**没有显式用**。

原因：看 x 行和 y 行的构造：

```cpp
matrix[0]  = -sy / t;      // x 行
matrix[4]  =  cy / t;

matrix[1]  = (-sp*cy) / t; // y 行
matrix[9]  = (cp)     / t;
```

两行**都除以同一个 `t`**。这说明本项目的 `t` 对**水平、垂直两个轴是同一个值**——
也就是说，它假设两个方向在 NDC 空间里有相同的视野张角（这与开头按垂直方向推出来的 `t` 是一致的）。
真正的屏幕宽高比差异，靠后面分别乘 `px` / `py`（半宽 / 半高）来补偿。

源码注释写得很清楚：

```
//   screenX = px + row0(x) / camera * px     (水平焦距 px/t)
//   screenY = py - row1(x) / camera * py     (垂直焦距 py/t)
// 上行不乘 ratio, 垂直焦距由 WorldToScreen 的 *py 决定 (与 POV 矩阵一致)
```

**横向和纵向用不同的半屏尺寸（px 和 py）来缩放**，
等价于给横、纵两个方向**各自设了一个像素焦距**（水平 = px/t，垂直 = py/t）。
当游戏真实的宽高比与本项目假设的不一致时，这两个焦距的比例就不对，
于是需要 UI 里那两个独立的微调系数来校准：

```cpp
ImGui::SliderFloat("垂直焦距系数", &ConfigManager::Settings.垂直焦距系数, 0.2f, 2.5f, "%.3f");
ImGui::SliderFloat("水平焦距系数", &ConfigManager::Settings.水平焦距系数, 0.2f, 2.5f, "%.3f");
```

> [!tip] 为什么需要微调系数
> 不同游戏、不同设备的 FOV 定义可能不同：
> - 有的 FOV 是水平 FOV，有的是垂直 FOV
> - 有的游戏在超宽屏上做了 Hor+ 或 Vert- 适配
> - 有的会在开镜时改变 FOV
>
> 硬编码算不准时，让用户手动微调是最实用的方案。
> 这两个滑块存在的意义就是"校准"。

## 另一种投影：正交投影

透视投影有"近大远小"，正交投影没有：

| | 透视 | 正交 |
|---|---|---|
| 平行线 | 汇聚于消失点 | 保持平行 |
| 除法 | 除以 w | 不除 |
| 用途 | 游戏、真实感 | CAD、2D UI、等距视角 |

本项目是透视投影（因为要"近大远小"的 ESP）。

## 近远裁剪面

标准投影矩阵还有 `near` / `far` 两个参数：

```
near：比这更近的不画（防止除零、防止穿模）
far ：比这更远的不画（精度考虑）
```

本项目用了一个简化判据：

```cpp
if (w < 0.001f) return false;      // 相当于 near = 0.001
```

没有 far 裁剪——远处的物体照样算，只是投影后落在屏幕外，
被后续的可见性检查过滤掉。

## 从游戏引擎读矩阵（另一种方案）

除了自己算，还可以**直接读引擎算好的矩阵**：

```cpp
if (ConfigManager::Settings.矩阵W2S && MatrixPtr != 0) {
    const float* m = &MatrixData.M[0][0];
    const float w = m[3]*obj.X + m[7]*obj.Y + m[11]*obj.Z + m[15];
    if (w < 0.001f) return false;
    const float cx = m[0]*obj.X + m[4]*obj.Y + m[8]*obj.Z  + m[12];
    const float cy = m[1]*obj.X + m[5]*obj.Y + m[9]*obj.Z  + m[13];
    screen.X = (cx / w + 1.0f) * px;
    screen.Y = (1.0f - cy / w) * py;
    return ...;
}
```

两者的差别：

| | 自算 | 读引擎矩阵 |
|---|---|---|
| 准确度 | 依赖 FOV/偏移正确 | **引擎用的就是它，最准** |
| 兼容性 | 通用 | 依赖偏移是否失效 |
| 可调 | 有焦距微调 | 无（除非手动改） |
| 特殊情况 | 开镜/过场时可能错 | 自动跟随 |

所以本项目**两种都提供，由 UI 开关切换**（`矩阵W2S`）。
这是很务实的设计：默认用引擎矩阵，失效时降级到自算。

## 验证投影正确性的方法

**方法 1：屏幕中心点**

相机正前方 100 米的点，应该投影到屏幕正中心：

```cpp
Vec3 front = camPos + GetForward(camRot) * 100.0f;
Vec2 s = WorldToScreen(front);
// s 应该 ≈ (px, py)，即屏幕中心
```

**方法 2：已知物体**

放一个已知位置的物体，看方框是不是套在它身上。
这是最直观的验证——**调焦距系数直到方框贴合**。

**方法 3：对称性**

相机左右两侧等距离的点，投影后应该关于屏幕中心对称。

## 动手：验证投影公式

```cpp
#include <cstdio>
#include <cmath>
#include <cassert>

constexpr float PI = 3.14159265358979323846f;

// 简化的 W2S：相机在原点朝 +X，只做透视除法
struct Result { float x, y; bool ok; };

Result project(float camX, float camY, float camZ,
               float px, float py, float fovDeg,
               float kx=1.0f, float ky=1.0f) {
    // 相机空间：x 前、y 右、z 上（相机已在原点）
    float w = camX;                  // 深度
    if (w < 0.001f) return {0,0,false};

    float t = tanf(fovDeg * 0.5f * PI / 180.0f);
    float ndc_y_right = (camY / t) / w;      // 右方向
    float ndc_y_up    = (camZ / t) / w;      // 上方向

    return {
        (ndc_y_right * kx + 1.0f) * px,
        (1.0f - ndc_y_up * ky) * py,
        true
    };
}

int main(void) {
    const float px = 1080.0f;   // 半宽
    const float py = 1200.0f;   // 半高

    // 1. 正前方 → 屏幕中心
    auto c = project(100, 0, 0, px, py, 90);
    printf("正前方: (%.1f, %.1f)  期望 (%.1f, %.1f)\n", c.x, c.y, px, py);
    assert(fabsf(c.x - px) < 0.5f && fabsf(c.y - py) < 0.5f);

    // 2. 在相机后方 → 失败
    auto b = project(-100, 0, 0, px, py, 90);
    printf("后方 ok=%d\n", b.ok);
    assert(!b.ok);

    // 3. 右方：应偏右（x > px），且在竖直中心
    auto r = project(100, 100, 0, px, py, 90);
    printf("右前方: (%.1f, %.1f)\n", r.x, r.y);
    assert(r.x > px && fabsf(r.y - py) < 0.5f);

    // 4. 上方：应偏上（y < py），且水平居中
    auto u = project(100, 0, 100, px, py, 90);
    printf("上前方: (%.1f, %.1f)\n", u.x, u.y);
    assert(u.y < py && fabsf(u.x - px) < 0.5f);

    // 5. 距离加倍 → 偏移量减半（透视）
    auto near_ = project(100, 100, 0, px, py, 90);
    auto far_  = project(200, 100, 0, px, py, 90);
    float offNear = near_.x - px, offFar = far_.x - px;
    printf("近偏移=%.1f  远偏移=%.1f  比值=%.2f\n",
           offNear, offFar, offNear / offFar);
    assert(fabsf(offNear / offFar - 2.0f) < 0.01f);   // 应恰好 2 倍

    // 6. FOV 加倍 → 偏移量变小（视野变广）
    auto fov90 = project(100, 100, 0, px, py, 90);
    auto fov120 = project(100, 100, 0, px, py, 120);
    printf("FOV90偏移=%.1f  FOV120偏移=%.1f\n",
           fov90.x - px, fov120.x - px);
    assert((fov120.x - px) < (fov90.x - px));

    printf("全部通过\n");
    return 0;
}
```

六项验证每一行都对应一个物理直觉：
正前方在中心、后方不可见、右偏右、上偏上、距离翻倍偏移减半、FOV 变大偏移变小。

## 验收清单

- [ ] 知道 `t = tan(FOV/2)` 的几何含义（深度 → 半屏高的换算）（说出"深度 d 处半高对应 d*t"）
- [ ] 理解"除以 w"是透视除法（近大远小）（说出为什么远的看起来小）
- [ ] 知道为什么 `screenY` 是 `1 - ndc`（Y 轴翻转）（说出屏幕 Y 向下、NDC Y 向上）
- [ ] 理解本项目用 `px`/`py` 分别做横纵缩放，等价于独立焦距（说出两个系数的作用）
- [ ] 知道"自算矩阵"和"读引擎矩阵"两种方案的优劣（各说一条优缺点）
- [ ] 知道焦距微调系数存在的意义（说出"补偿不同宽高比的视野偏差"）
- [ ] 跑通投影验证，6 项断言全过（运行验证，6 项全绿）

→ 下一章：[[第51章-世界坐标转屏幕坐标]]　—— 完成总纲承诺的第 5 个成果：**`w2s_demo`**——一个能在电脑上离线验证 W2S 完整链路的程序。
