---
tags: [教程, 附录, 数学, LaTeX, 公式]
aliases: [附录S, formulas]
---

# 附录 S · 数学公式速查（LaTeX 版）

> [!abstract] 这篇解决什么
> 卷四讲了所有公式，但正文里是纯文本写法（`t = tan(FOV/2)`）。
> 这篇用 **LaTeX** 重新排版，每个公式配：
> ① 标准数学记号　② 对应的 C++ 实现　③ 一句话说明。
>
> **Obsidian 原生渲染 LaTeX**（MathJax），不需要插件。

## 阅读约定

| 记号 | 含义 |
|---|---|
| $\vec{v}$ | 向量 |
| $\mathbf{M}$ | 矩阵 |
| $\hat{v}$ | 单位向量 |
| $\theta$ | 角度 |
| $\cdot$ | 点积 |
| $\times$ | 叉积 |
| $\|\vec{v}\|$ | 模长 |

**注意**：本项目的 `matrix[16]` 用**列主序存储**（`matrix[row + col*4]`），
矩阵公式按**行主序数学**书写——两者是转置关系（第 46 章）。

## 一、向量（第 45 章）

### 1.1 模长

$$
\|\vec{v}\| = \sqrt{v_x^2 + v_y^2 + v_z^2}
$$

```cpp
float Length() const { return std::sqrt(x*x + y*y + z*z); }
```

### 1.2 归一化

$$
\hat{v} = \frac{\vec{v}}{\|\vec{v}\|}, \quad \|\vec{v}\| > \varepsilon
$$

```cpp
Vec3 Normalized() const {
    const float len = Length();
    return len < 1e-6f ? Vec3{} : (*this) / len;   // ★ 必须防除零
}
```

### 1.3 点积

$$
\vec{a} \cdot \vec{b} = a_x b_x + a_y b_y + a_z b_z = \|\vec{a}\|\,\|\vec{b}\|\cos\theta
$$

```cpp
constexpr float Dot(const Vec3& v) const { return x*v.x + y*v.y + z*v.z; }
```

**单位化后**：$\hat{a} \cdot \hat{b} = \cos\theta$

| 点积值 | 含义 |
|---|---|
| $> 0$ | 同向（夹角 < 90°） |
| $= 0$ | 垂直 |
| $< 0$ | 反向 |

### 1.4 叉积

$$
\vec{a} \times \vec{b} =
\begin{bmatrix}
a_y b_z - a_z b_y \\
a_z b_x - a_x b_z \\
a_x b_y - a_y b_x
\end{bmatrix}
$$

```cpp
constexpr Vec3 Cross(const Vec3& v) const {
    return {y*v.z - z*v.y, z*v.x - x*v.z, x*v.y - y*v.x};
}
```

**性质**：结果垂直于两个输入，$\|\vec{a} \times \vec{b}\| = \|\vec{a}\|\|\vec{b}\|\sin\theta$

### 1.5 两点距离

$$
d(\vec{a}, \vec{b}) = \|\vec{b} - \vec{a}\|
$$

**优化**：比较远近时用平方，省一次开方：
$$
d^2 = (b_x - a_x)^2 + (b_y - a_y)^2 + (b_z - a_z)^2
$$

```cpp
static float DistanceSq(const Vec3& a, const Vec3& b) { return (b - a).LengthSq(); }
```

## 二、矩阵与变换（第 46 章）

### 2.1 矩阵乘向量

$$
(\mathbf{M}\vec{v})_i = \sum_{k=0}^{3} M_{ik}\,v_k
$$

### 2.2 矩阵乘矩阵

$$
(\mathbf{AB})_{ij} = \sum_{k=0}^{3} A_{ik}B_{kj}
$$

```cpp
inline Mat4 Mat4Multiply(const Mat4& a, const Mat4& b) {
    Mat4 r;
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            for (int k = 0; k < 4; k++)
                r.M[i][j] += a.M[i][k] * b.M[k][j];
    return r;
}
```

### 2.3 齐次坐标

$$
\text{点} =
\begin{bmatrix} x \\ y \\ z \\ 1 \end{bmatrix}, \quad
\text{方向} =
\begin{bmatrix} x \\ y \\ z \\ 0 \end{bmatrix}
$$

**为什么 $w$ 不同**：$w=0$ 时平移分量（第 4 列）被乘成 0 → 方向不受平移影响。

### 2.4 平移矩阵

$$
\mathbf{T}(t_x, t_y, t_z) =
\begin{bmatrix}
1 & 0 & 0 & t_x \\
0 & 1 & 0 & t_y \\
0 & 0 & 1 & t_z \\
0 & 0 & 0 & 1
\end{bmatrix}
$$

### 2.5 缩放矩阵

$$
\mathbf{S}(s_x, s_y, s_z) =
\begin{bmatrix}
s_x & 0 & 0 & 0 \\
0 & s_y & 0 & 0 \\
0 & 0 & s_z & 0 \\
0 & 0 & 0 & 1
\end{bmatrix}
$$

### 2.6 不可交换性

$$
\mathbf{TS} \neq \mathbf{ST}
$$

**验证**（本项目代码里的实验）：

| 顺序 | 对点 $(1,2,3)$ 的结果 |
|---|---|
| $\mathbf{T}(10,0,0)\,\mathbf{S}(2)$ | $(12, 4, 6)$ |
| $\mathbf{S}(2)\,\mathbf{T}(10,0,0)$ | $(22, 4, 6)$ |

### 2.7 转置与逆

$$
(\mathbf{M}^{\mathsf{T}})_{ij} = M_{ji}, \qquad
\mathbf{M}\mathbf{M}^{-1} = \mathbf{I}
$$

**正交矩阵（旋转）**：$\mathbf{M}^{-1} = \mathbf{M}^{\mathsf{T}}$

## 三、旋转（第 47 章）

### 3.1 绕 Z 轴旋转（Yaw）

$$
\mathbf{R}_z(\theta) =
\begin{bmatrix}
\cos\theta & -\sin\theta & 0 \\
\sin\theta & \cos\theta & 0 \\
0 & 0 & 1
\end{bmatrix}
$$

### 3.2 绕 Y 轴旋转（Pitch）

$$
\mathbf{R}_y(\theta) =
\begin{bmatrix}
\cos\theta & 0 & \sin\theta \\
0 & 1 & 0 \\
-\sin\theta & 0 & \cos\theta
\end{bmatrix}
$$

### 3.3 绕 X 轴旋转（Roll）

$$
\mathbf{R}_x(\theta) =
\begin{bmatrix}
1 & 0 & 0 \\
0 & \cos\theta & -\sin\theta \\
0 & \sin\theta & \cos\theta
\end{bmatrix}
$$

### 3.4 欧拉角组合（UE4 顺序：Roll → Pitch → Yaw）

$$
\mathbf{M} = \mathbf{R}_z(\text{Yaw})\,\mathbf{R}_y(\text{Pitch})\,\mathbf{R}_x(\text{Roll})
$$

**展开后的关键元素**（对应 `rotatorToMatrix`）：

$$
\begin{aligned}
M_{00} &= C_P C_Y, & M_{01} &= C_P S_Y, & M_{02} &= S_P \\
M_{10} &= S_R S_P C_Y - C_R S_Y, & M_{11} &= S_R S_P S_Y + C_R C_Y, & M_{12} &= -S_R C_P \\
M_{20} &= -(C_R S_P C_Y + S_R S_Y), & M_{21} &= C_Y S_R - C_R S_P S_Y, & M_{22} &= C_R C_P
\end{aligned}
$$

其中 $S_P = \sin(\text{Pitch})$、$C_P = \cos(\text{Pitch})$，其余同理。

**第三行 = 前方向**：

$$
\vec{F} = (C_P C_Y,\; C_P S_Y,\; S_P)
$$

```cpp
Vec3 RotatorToForward(const Rotator& r) {
    const float sp = sinf(r.Pitch * DEG2RAD), cp = cosf(r.Pitch * DEG2RAD);
    const float sy = sinf(r.Yaw   * DEG2RAD), cy = cosf(r.Yaw   * DEG2RAD);
    return {cp*cy, cp*sy, sp};        // ← 就是 M 的第 0 行
}
```

### 3.5 四元数定义（轴角形式）

绕单位轴 $\hat{a} = (a_x, a_y, a_z)$ 旋转 $\theta$：

$$
\mathbf{q} = \left(\sin\frac{\theta}{2}\right)\hat{a} + \cos\frac{\theta}{2}
\quad\Longleftrightarrow\quad
\begin{cases}
q_x = a_x \sin(\theta/2) \\
q_y = a_y \sin(\theta/2) \\
q_z = a_z \sin(\theta/2) \\
q_w = \cos(\theta/2)
\end{cases}
$$

**注意是半角 $\theta/2$**——因为 $q$ 在旋转公式里出现两次。

### 3.6 四元数乘法

$$
\mathbf{q}_1 \mathbf{q}_2 =
\begin{bmatrix}
w_1 w_2 - \vec{v}_1 \cdot \vec{v}_2 \\
w_1 \vec{v}_2 + w_2 \vec{v}_1 + \vec{v}_1 \times \vec{v}_2
\end{bmatrix}
$$

（$\vec{v} = (x, y, z)$ 是向量部分）

**注意不可交换**：$\mathbf{q}_1\mathbf{q}_2 \neq \mathbf{q}_2\mathbf{q}_1$

### 3.7 四元数旋转向量

$$
\vec{v}' = \mathbf{q}\,\vec{v}\,\mathbf{q}^{-1}
$$

**快速形式**（本项目用的，省 14 次乘法）：

$$
\begin{aligned}
\vec{t} &= 2\,(\vec{q}_v \times \vec{v}) \\
\vec{v}' &= \vec{v} + q_w \vec{t} + (\vec{q}_v \times \vec{t})
\end{aligned}
$$

```cpp
Vec3 RotateByQuat(const Vec3 &v, const Quat &q) {
    const Vec3 t = Cross(q.xyz, v) * 2.0f;
    return v + t * q.w + Cross(q.xyz, t);
}
```

### 3.8 四元数转矩阵

设 $x_2 = 2q_x$、$y_2 = 2q_y$、$z_2 = 2q_z$：

$$
{\mathbf{M}} =
\begin{bmatrix}
1 - (y_2 q_y + z_2 q_z) & x_2 q_y + w z_2 & x_2 q_z - w y_2 \\[2pt]
x_2 q_y - w z_2 & 1 - (x_2 q_x + z_2 q_z) & y_2 q_z + w x_2 \\[2pt]
x_2 q_z + w y_2 & y_2 q_z - w x_2 & 1 - (x_2 q_x + y_2 q_y)
\end{bmatrix}
$$

```cpp
// 本项目的 TransformToMatrix 就是这个式子（再乘上 Scale3D）
const float x2 = q.x + q.x, y2 = q.y + q.y, z2 = q.z + q.z;
matrix.M[0][0] = (1 - (q.y*y2 + q.z*z2)) * scale.x;
// ...
```

### 3.9 角度差归一化

$$
\Delta\theta = \big((\theta_2 - \theta_1 + 180°) \bmod 360°\big) - 180°
$$

```cpp
inline float AngleDelta(float from, float to) {
    float d = to - from;
    while (d >  180.0f) d -= 360.0f;
    while (d <= -180.0f) d += 360.0f;
    return d;
}
```

**目的**：让 $350° \to 10°$ 的差是 $+20°$ 而不是 $-340°$。

### 3.10 万向节死锁的数学证明

当 $\text{Pitch} = 90°$ 时 $S_P = 1$、$C_P = 0$，代入 3.4：

$$
\begin{aligned}
M_{00} &= 0, & M_{01} &= 0, & M_{02} &= 1 \\
M_{10} &= S_R C_Y - C_R S_Y = \sin(\text{Roll} - \text{Yaw}) \\
M_{11} &= S_R S_Y + C_R C_Y = \cos(\text{Roll} - \text{Yaw}) \\
M_{20} &= -(C_R C_Y + S_R S_Y) = -\cos(\text{Roll} - \text{Yaw}) \\
M_{21} &= C_Y S_R - C_R S_Y = \sin(\text{Roll} - \text{Yaw})
\end{aligned}
$$

**所有元素只依赖 $(\text{Roll} - \text{Yaw})$ 这个差值**——
$\text{Roll}$ 和 $\text{Yaw}$ 的独立值消失了 → **两个自由度塌缩成一个**。

## 四、局部空间与变换链（第 48 章）

### 4.1 骨骼世界变换

$$
\mathbf{M}_{\text{world}} = \mathbf{M}_{\text{local}} \times \mathbf{M}_{\text{parent}}
$$

本项目的对应写法（列主序约定）：

```cpp
inline Vector3A GetBoneWorldPos(uint64_t boneArray, int index, const Matrix& c2w) {
    return MarixToVector(MatrixMulti(
        TransformToMatrix(getBone(boneArray + index * BONE_STRIDE)),  // M_local
        c2w                                                            // M_parent(C2W)
    ));
}
```

### 4.2 骨骼步长

$$
\text{addr}_i = \text{base} + i \times \text{STRIDE}, \quad \text{STRIDE} = 48
$$

**注意**：$\text{STRIDE} \ne \text{sizeof}(\text{BoneTransform}) = 40$（第 07、86 章）。

### 4.3 层级累乘

对 $n$ 级层级：

$$
\mathbf{M}_{\text{world}} = \mathbf{M}_1 \mathbf{M}_2 \cdots \mathbf{M}_n
$$

**顺序不能颠倒**（矩阵不可交换）。

## 五、相机与视图矩阵（第 49 章）

### 5.1 相机基向量（忽略 Roll）

$$
\begin{aligned}
\vec{F} &= (C_Y C_P,\; S_Y C_P,\; S_P) \\
\vec{R} &= (-S_Y,\; C_Y,\; 0) \\
\vec{U} &= (-S_P C_Y,\; -S_P S_Y,\; C_P)
\end{aligned}
$$

### 5.2 视图矩阵（行主序数学）

$$
\mathbf{V} =
\begin{bmatrix}
\vec{F} & -\vec{F} \cdot \vec{C} \\
\vec{R} & -\vec{R} \cdot \vec{C} \\
\vec{U} & -\vec{U} \cdot \vec{C} \\
\vec{0} & 1
\end{bmatrix}
$$

**平移列就是 $-\vec{F} \cdot \vec{C}$ 之类**（把相机移到原点）。

### 5.3 深度分量

$$
w = \vec{F} \cdot \vec{p} - \vec{F} \cdot \vec{C} = \vec{F} \cdot (\vec{p} - \vec{C})
$$

```cpp
// 本项目的 matrix[3], matrix[7], matrix[11], matrix[15] 就是这一行
matrix[3]  = cy * cp;
matrix[7]  = sy * cp;
matrix[11] = sp;
matrix[15] = -(camX * matrix[3] + camY * matrix[7] + camZ * matrix[11]);   // = -F·C
```

## 六、投影（第 50 章）

### 6.1 焦距因子

$$
t = \tan\left(\frac{\text{FOV}}{2}\right)
$$

**几何意义**：深度 $d$ 处，屏幕半高对应的实际长度是 $d \cdot t$。

### 6.2 归一化设备坐标（NDC）

$$
\text{ndc}_x = \frac{r}{w \cdot t}, \qquad \text{ndc}_y = \frac{u}{w \cdot t}
$$

其中 $w$ 是深度，$r$ 是横向偏移，$u$ 是纵向偏移。

**范围**：$[-1, 1]$（对应屏幕边缘）

### 6.3 屏幕坐标

$$
\begin{aligned}
s_x &= (\text{ndc}_x \cdot k_x + 1) \cdot p_x \\
s_y &= (1 - \text{ndc}_y \cdot k_y) \cdot p_y
\end{aligned}
$$

| 符号 | 含义 |
|---|---|
| $p_x, p_y$ | 屏幕**半宽半高**（像素） |
| $k_x, k_y$ | 焦距微调系数（可调） |
| $1 - \text{ndc}_y$ | **Y 轴翻转**（屏幕 y 向下） |

### 6.4 打开非 Y 翻转会怎样

$$
s_y' = (1 + \text{ndc}_y) \cdot p_y \quad \Rightarrow \quad \text{画面上下颠倒}
$$

### 6.5 正交投影矩阵（ImGui 用）

把 $x \in [L, R]$、$y \in [T, B]$ 映射到 $[-1, 1]$：

$$
\mathbf{P} =
\begin{bmatrix}
\frac{2}{R-L} & 0 & 0 & \frac{R+L}{L-R} \\[4pt]
0 & \frac{2}{T-B} & 0 & \frac{T+B}{B-T} \\[4pt]
0 & 0 & -1 & 0 \\[4pt]
0 & 0 & 0 & 1
\end{bmatrix}
$$

```cpp
// ImGui 后端就是这个矩阵
float mvp[4][4] = {
    { 2.0f/(R-L),   0.0f,         0.0f,  0.0f },
    { 0.0f,         2.0f/(T-B),   0.0f,  0.0f },
    { 0.0f,         0.0f,        -1.0f,  0.0f },
    { (R+L)/(L-R),  (T+B)/(B-T),  0.0f,  1.0f },
};
```

### 6.6 透视除法与近平面

$$
\lim_{w \to 0^+} \frac{r}{w \cdot t} = +\infty
$$

**所以要设近平面**：

```cpp
if (w < 0.001f) return false;      // 0.001 就是 $\varepsilon$
```

## 七、世界坐标转屏幕坐标（第 51 章）

### 7.1 完整公式

$$
\begin{aligned}
w &= m_3 x + m_7 y + m_{11} z + m_{15} \\
c_x &= m_0 x + m_4 y + m_8 z + m_{12} \\
c_y &= m_1 x + m_5 y + m_9 z + m_{13} \\
s_x &= \left(\frac{c_x}{w} k_x + 1\right) p_x \\
s_y &= \left(1 - \frac{c_y}{w} k_y\right) p_y
\end{aligned}
$$

（$m_i$ 是 `matrix[]` 的列主序下标：第 0 行 = $m_0, m_4, m_8, m_{12}$）

### 7.2 投影的线性性验证

**正前方投到屏幕中心**：

$$
\vec{p} = \vec{C} + d \cdot \vec{F} \quad \Rightarrow \quad (s_x, s_y) = (p_x, p_y)
$$

**距离翻倍，偏移减半**：

$$
\frac{s_x(2d) - p_x}{s_x(d) - p_x} = \frac{1}{2}
$$

（这是 `w2s_demo` 的第 5 项验证）

## 八、角度与三角函数（第 56 章）

### 8.1 方向转欧拉角

$$
\begin{aligned}
\text{水平} &= \sqrt{v_x^2 + v_y^2} \\
\text{Pitch} &= \operatorname{atan2}(v_z,\; \text{水平}) \cdot \frac{180°}{\pi} \\
\text{Yaw} &= \operatorname{atan2}(v_y,\; v_x) \cdot \frac{180°}{\pi}
\end{aligned}
$$

**为什么用 $\operatorname{atan2}$**：

$$
\operatorname{atan}\frac{\Delta y}{\Delta x} \quad \text{丢失象限信息}, \qquad
\operatorname{atan2}(\Delta y, \Delta x) \in (-\pi, \pi] \quad \text{覆盖四象限}
$$

例如：

| $(\Delta x, \Delta y)$ | $\operatorname{atan}(\Delta y/\Delta x)$ | $\operatorname{atan2}(\Delta y, \Delta x)$ |
|---|---|---|
| $(1, 1)$ | $45°$ ✓ | $45°$ |
| $(-1, -1)$ | $45°$ ✗ | $-135°$ ✓ |

### 8.2 角度制与弧度制

$$
\text{rad} = \text{deg} \times \frac{\pi}{180°}, \qquad
\text{deg} = \text{rad} \times \frac{180°}{\pi}
$$

**C 标准库的三角函数只接受弧度**。

### 8.3 `acos` 的精度悬崖

$$
\cos(0°) = 1.0, \quad \cos(1°) = 0.99985, \quad \cos(0.1°) = 0.9999985
$$

**float 只有 7 位有效数字** → 小角度处 $\arccos$ 结果极不稳定。
**且浮点误差可能让输入略大于 1**：

$$
\cos\theta = 1.0000001 \Rightarrow \arccos(\cdot) = \text{NaN}
$$

**必须 clamp**：$\arccos(\operatorname{clamp}(x, -1, 1))$

## 九、浮点（第 26、55 章）

### 9.1 IEEE754 单精度值

$$
\text{value} = (-1)^S \times \left(1 + \frac{M}{2^{23}}\right) \times 2^{E - 127}
$$

| 字段 | 位数 | 说明 |
|---|---|---|
| $S$ | 1 | 符号位 |
| $E$ | 8 | 指数（偏移 127） |
| $M$ | 23 | 尾数（隐含前导 1） |

**验证**：$3.14_f$ 的位模式是 `0x4048F5C3`

$$
S = 0,\quad E = 128 \Rightarrow 2^{1},\quad M = 0x48F5C3
$$

$$
\left(1 + \frac{4781507}{8388608}\right) \times 2^1 = 1.57 \times 2 = 3.14 \quad \checkmark
$$

### 9.2 float 的精度间隔

float 有 24 位有效二进制位（约 7 位十进制）：

$$
\text{最小间隔}(\text{数量级 } 2^n) = 2^{\,n - 23}
$$

| 数量级 | 最小间隔 |
|---|---|
| $10^0$ | $\approx 1.2 \times 10^{-7}$ |
| $10^4$ | $\approx 9.8 \times 10^{-4}$ |
| $10^6$ | $\approx 0.0625$ |
| $10^7$ | $\approx 1.0$ |

**结论**：在 $10^7$ 量级下，加 $0.05$ **完全没有效果**。

### 9.3 抵消误差

$$
a = 1.2345678, \quad b = 1.2345677 \quad \Rightarrow \quad a - b = 10^{-7}
$$

但两个数各需 8 位有效数字才能表示 → **相减后有效位数急剧减少**。

### 9.4 相对误差判断

$$
|a - b| < \varepsilon \cdot \max(1,\; |a|,\; |b|)
$$

```cpp
bool NearlyEqual(float a, float b, float eps = 1e-5f) {
    const float diff  = std::fabs(a - b);
    const float scale = std::fmax(1.0f, std::fmax(std::fabs(a), std::fabs(b)));
    return diff < eps * scale;
}
```

## 十、光线求交（第 95 章）

### 10.1 射线方程

$$
\vec{P}(t) = \vec{O} + t\vec{D}, \quad t \ge 0
$$

### 10.2 三角形重心坐标

$$
\vec{P}(u,v) = (1 - u - v)\vec{V}_0 + u\vec{V}_1 + v\vec{V}_2
$$

**约束**：$u \ge 0$，$v \ge 0$，$u + v \le 1$

### 10.3 求交（联立两个方程）

由 $\vec{O} + t\vec{D} = \vec{V}_0 + u\vec{E}_1 + v\vec{E}_2$，其中 $\vec{E}_1 = \vec{V}_1 - \vec{V}_0$、$\vec{E}_2 = \vec{V}_2 - \vec{V}_0$：

$$
t\vec{D} - u\vec{E}_1 - v\vec{E}_2 = \vec{V}_0 - \vec{O}
$$

记 $\vec{T} = \vec{O} - \vec{V}_0$，写成矩阵形式：

$$
\begin{bmatrix} -\vec{D} & \vec{E}_1 & \vec{E}_2 \end{bmatrix}
\begin{bmatrix} t \\ u \\ v \end{bmatrix} = -\vec{T}
$$

**用克莱姆法则解**（Möller–Trumbore 的关键优化是复用中间叉积）：

$$
\begin{aligned}
\vec{P} &= \vec{D} \times \vec{E}_2 \\
\det &= \vec{E}_1 \cdot \vec{P} \\
u &= \frac{\vec{T} \cdot \vec{P}}{\det} \\
\vec{Q} &= \vec{T} \times \vec{E}_1 \\
v &= \frac{\vec{D} \cdot \vec{Q}}{\det} \\
t &= \frac{\vec{E}_2 \cdot \vec{Q}}{\det}
\end{aligned}
$$

**运算量**：2 个叉积 + 4 个点积 + 1 次除法，**没有三角函数、没有开方**。

### 10.4 射线与 AABB（slab 法）

对每个轴求射线进入/离开该轴"平板"的参数：

$$
t_{1,i} = \frac{\text{min}_i - O_i}{D_i}, \quad t_{2,i} = \frac{\text{max}_i - O_i}{D_i}
$$

取交集：

$$
t_{\text{enter}} = \max(\min(t_{1,i}, t_{2,i})), \quad
t_{\text{exit}} = \min(\max(t_{1,i}, t_{2,i}))
$$

$$
\text{命中} \iff t_{\text{enter}} \le t_{\text{exit}}
$$

## 十一、遮挡判定（第 95、98 章）

### 11.1 遮挡的条件

$$
\text{被遮挡} \iff \exists\, t \in (0,\; d_{\text{target}}),\ \text{射线命中几何体}
$$

其中 $d_{\text{target}} = \|\vec{P}_{\text{target}} - \vec{O}\|$

**这就是为什么 $t_{\text{far}}$ 必须设为目标距离**——否则会把目标背后的墙也算成遮挡。

### 11.2 用未归一化方向省一次开方

令 $\vec{D} = \vec{P}_{\text{target}} - \vec{O}$（**不归一化**），取 $t_{\text{far}} = 1 - \varepsilon$：

$$
t = 1 \iff \text{到达目标位置}
$$

```cpp
ray.dir   = to - from;          // 未归一化
ray.tfar  = 1.0f - 1e-4f;       // ★ 留余量避免自交
```

**收益**：省一次 `sqrt`，且 $t \in [0,1]$ 的语义更清晰。

## 十二、公式 ↔ 代码 对照总表

| 公式 | C++ 位置 | 章节 |
|---|---|---|
| $\|\vec{v}\| = \sqrt{\sum v_i^2}$ | `Vec3::Length()` | 45 |
| $\vec{a} \cdot \vec{b}$ | `Vec3::Dot()` | 45 |
| $\vec{a} \times \vec{b}$ | `Vec3::Cross()` | 45 |
| $(\mathbf{AB})_{ij} = \sum_k A_{ik}B_{kj}$ | `Mat4Multiply` | 46 |
| $\mathbf{M} = \mathbf{R}_z\mathbf{R}_y\mathbf{R}_x$ | `rotatorToMatrix` | 47 |
| $q_w = \cos(\theta/2)$ | `QuatFromAxisAngle` | 47 |
| $\vec{v}' = \vec{v} + q_w\vec{t} + \vec{q}_v \times \vec{t}$ | `RotateByQuat` | 86 |
| 四元数 → 矩阵 | `TransformToMatrix` | 47 |
| $\mathbf{M}_{\text{world}} = \mathbf{M}_{\text{local}}\mathbf{M}_{\text{C2W}}$ | `GetBoneWorldPos` | 86 |
| $\vec{F} = (C_PC_Y, C_PS_Y, S_P)$ | `RotatorToForward` | 47 |
| $t = \tan(\text{FOV}/2)$ | `camMakeMatrix` | 50 |
| $s_x = (\text{ndc}_x + 1)p_x$ | `WorldToScreen` | 51 |
| $\operatorname{atan2}(v_z, \text{水平})$ | `向量转角度` | 47、56 |
| $\Delta\theta$ 归一化 | `AngleDelta` | 56 |
| value $= (-1)^S(1+M/2^{23})2^{E-127}$ | （第 26 章解析器） | 26 |
| Möller–Trumbore | `RayTriangleIntersect` | 95 |
| slab 法 | `RayAABB` | 95 |
| $t_{\text{far}} = d_{\text{target}}$ | `IsOccluded` | 98 |

> [!tip] 用 LaTeX 复习的方法
> 盖住右侧的"对应代码"，只看公式，问自己：
> **"这个公式在项目里怎么实现的？"**
>
> 想不起来的，回去看那一章。这是最高效的复习方式——
> 因为数学公式是"最浓缩的知识点"，一个符号背后可能是一段代码加三行解释。

→ 返回 [[00-开始之前]]
