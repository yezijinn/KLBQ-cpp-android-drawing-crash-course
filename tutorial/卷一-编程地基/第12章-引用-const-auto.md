---
tags: [教程, 卷一, C++]
day: 6
aliases: [ch12]
---

# 第 12 章 · 引用、const、auto

> [!abstract] 本章目标
> 掌握现代 C++ 的三个书写习惯：用引用代替指针传参、到处加 `const`、用 `auto` 减少重复。
> 本项目代码里这三个符号占比极高，不熟会读得很痛苦。

## 先看一段本项目真实代码

```cpp
inline bool 提交候选(uint64_t actor,
                     const Vec3  &世界位置,
                     bool        被遮挡) {
    const Vec3 方向 = Vec3(世界位置.x - AppBase.Location.x, ...);
    const float 世界距离 = 长度(方向);
    if (世界距离 <= 1.0f || 世界距离 > Cfg.最大距离) return;
    /* ... */
}
```

一行里有 3 个 `const`、2 个引用。它们不是装饰，每一个都在传达信息。

## 引用：给变量起别名

```cpp
int hp = 100;
int &ref = hp;      // ref 是 hp 的别名

ref = 50;
printf("%d\n", hp);   // 50 —— 改 ref 就是改 hp
```

**引用必须初始化，且不能改指向。** 它永远不会是"空"的。

| 对比 | 指针 | 引用 |
|---|---|---|
| 可以为空 | 是 | **否** |
| 可以改指向 | 是 | 否 |
| 语法 | `p->x` / `*p` | `r.x` |
| 需要判空 | 要 | 不需要 |

### 引用传参优于指针传参

```cpp
// 不好：调用方可能传 nullptr，函数里要判空
void process(Vec3 *v) { if (!v) return; v->x = 0; }

// 好：调用方不可能传空
void process(Vec3 &v) { v.x = 0; }
```

但**输入参数**应该用 `const 引用`（见下）。

## const 引用：既能避免拷贝，又保证不改

```cpp
// 值传递：整个结构体被拷贝一份（4x4 float 矩阵 = 64 字节）
void draw(Matrix m);

// const 引用：只传地址，且不改原对象
void draw(const Matrix &m);
```

性能差别在大结构体上很明显。本项目：

```cpp
inline Matrix TransformToMatrix(const BoneTransform& transform);
inline Matrix MatrixMulti(const Matrix& m1, const Matrix& m2);
inline Vector3A MarixToVector(const Matrix& matrix);
```

全是 `const &`。

> [!warning] const 引用与临时对象
> ```cpp
> void f(const std::string &s);
> f("hello");        // OK：临时对象可以绑定 const 引用
> 
> void g(std::string &s);
> g("hello");        // 编译错误：不能把临时对象绑到非 const 引用
> ```

## const 的四种用法

```cpp
// 1. 常量
const int MAX = 100;

// 2. 指针指向的内容不可改
const uint8_t *p;        // *p 不能改
uint8_t *const q;        // q 不能改指向
const uint8_t *const r;  // 都不能改

// 3. 成员函数不变对象
bool alive() const { return hp > 0; }
//   ^^^^^ 承诺不修改任何成员变量

// 4. 引用参数（见上）
```

### const 成员函数

```cpp
class Player {
    int hp = 100;
public:
    int get_hp() const { return hp; }        // OK：只读
    void set_hp(int v) { hp = v; }           // OK：非 const
};

const Player p;
p.get_hp();      // OK
p.set_hp(50);    // 编译错误：const 对象不能调用非 const 函数
```

这是**编译期保证**，能拦住一大类误改 bug。

> [!tip] 判断 const 位置的口诀
> `const` 在 `*` 左边 → 内容不可改；在 `*` 右边 → 指针不可改。
> ```cpp
> const char *p;     // 内容不可改（指向常量的指针）
> char *const p;     // 指针不可改（常量指针）
> ```

## 本项目的 const 用法实例

```cpp
// driver.h
const virtual_memory &GetMemoryInfoRef();      // 返回 const 引用：给你看，不给你改
const break_point &GetHwbpInfoRef();
const env_params &GetEnvParamsRef() const;     // 后面那个 const：这个函数不改对象
```

返回 `const &` 的好处：
1. 不拷贝大结构体
2. 调用方改不了内部状态
3. 不涉所有权（不用管谁来 delete）

## auto：让编译器推导类型

```cpp
// 冗长
std::vector<std::pair<uintptr_t, uintptr_t>>::iterator it = regions.begin();

// 简洁
auto it = regions.begin();
```

真正好用的场合：

```cpp
// 1. 类型名超长
auto regions = dr->GetScanRegions();        // vector<pair<uintptr_t,uintptr_t>>
auto mesh = DynamicLoadScene->GetMeshDatas();

// 2. lambda
auto finite = [](const Vector2A &p) {
    return std::isfinite(p.X) && std::isfinite(p.Y);
};

// 3. 结构化绑定（C++17）
auto [ptr, ec] = std::from_chars(begin, end, pid);
```

本项目的 `driver.h` 里就有第 3 种：`const auto [ptr, ec] = std::from_chars(...)`。

> [!danger] auto 会丢掉引用和 const
> ```cpp
> const Vec3 &get();
> auto a = get();         // a 是 Vec3（拷贝了！不是引用）
> const auto &b = get();  // b 才是 const Vec3&
> auto &c = get();        // c 是 const Vec3&（auto 推导时保留 const）
> ```
> 想要引用就显式写 `auto &`。

## 范围 for 循环

```cpp
std::vector<uint64_t> actors = {0x1000, 0x2000, 0x3000};

// C 风格
for (size_t i = 0; i < actors.size(); i++) {
    printf("%llX\n", (unsigned long long)actors[i]);
}

// 范围 for（只读）
for (uint64_t a : actors) { ... }

// 范围 for（const 引用，避免拷贝）
for (const auto &a : actors) { ... }
```

本项目：

```cpp
for (const auto &region : regions) { ... }
for (const auto& Mesh : HitMesh) { drawMesh(Mesh.get(), Draw); }
```

## constexpr：编译期计算

```cpp
constexpr size_t PAGE_SIZE = 4096;
constexpr float PI = 3.14159265358979323846f;

constexpr float deg_to_rad(float d) {
    return d * PI / 180.0f;
}

float r = deg_to_rad(90.0f);   // 编译器直接算出结果，运行时零开销
```

本项目里：

```cpp
static constexpr int MODE_KERNEL  = MEM_DRIVER_KERNEL;
static constexpr uint64_t MAX_DUMP_SIZE = 1024ULL * 1024 * 500;
```

比 `#define` 好：有类型、有作用域、调试器能看见。

## 强制类型转换：四种 cast

C 风格的 `(int)x` 太粗暴，C++ 提供四种精确的：

```cpp
// 1. static_cast：普通类型转换（最常用）
float f = 3.9f;
int i = static_cast<int>(f);            // 3

// 2. const_cast：去掉 const（少用，通常是设计问题）
const int ci = 10;
int *p = const_cast<int*>(&ci);

// 3. reinterpret_cast：重新解释位模式（做底层内存操作时用）
uint64_t addr = 0x1000;
auto obj = reinterpret_cast<MyStruct*>(addr);

// 4. dynamic_cast：安全的向下转型（需要虚函数）
AndroidImgui *g = graphics.get();
auto *vk = dynamic_cast<VulkanGraphics*>(g);   // 失败返回 nullptr
```

本项目大量使用 `static_cast`：

```cpp
req->vinput_info.x = static_cast<int>(normX * req->vinput_info.POSITION_X);
*outAddress = static_cast<uint64_t>(base);
const auto nextDisplayInfo = ...;
```

> [!note] 为什么不用 `(int)x`？
> `static_cast` 可以被 grep 到、被 IDE 高亮、编译器会检查合理性。
> C 风格转换在重构时是隐患——它会悄悄把 `const` 也转掉。

## 动手：改写成现代 C++

把这段 C 风格代码改写：

```cpp
// 改写前
void HandleActors(long Arrayaddr, int Count) {
    int i;
    for (i = 0; i < Count; i++) {
        long obj = ReadLong(Arrayaddr + 8 * i);
        if (obj == 0) continue;
        float x = ReadFloat(obj + 0x1F0);
        float y = ReadFloat(obj + 0x1F4);
        float z = ReadFloat(obj + 0x1F8);
        printf("%f %f %f\n", x, y, z);
    }
}
```

要求：
- `long` → `uint64_t`（64 位地址安全）
- 循环改成范围 for 或 `auto i`
- 加 `const`
- `printf` 保留（或改成 `std::cout`）

参考答案：

```cpp
void HandleActors(const uint64_t arrayAddr, const int32_t count) {
    for (int32_t i = 0; i < count; ++i) {
        const uint64_t obj = ReadU64(arrayAddr + 8ULL * i);
        if (obj == 0) continue;
        const float x = ReadF32(obj + 0x1F0);
        const float y = ReadF32(obj + 0x1F4);
        const float z = ReadF32(obj + 0x1F8);
        printf("%f %f %f\n", x, y, z);
    }
}
```

注意 `8ULL * i`：`i` 是 `int`，`8 * i` 也是 `int`，`i` 很大时会溢出。
写成 `8ULL * i` 让整个表达式提升到 `unsigned long long`。

## 验收清单

- [ ] 能说出引用和指针的三个区别
- [ ] 知道什么时候用 `const &` 传参（大结构体、只读）
- [ ] 能看懂 `const virtual_memory &GetMemoryInfoRef() const` 里两个 const 的含义
- [ ] 知道 `auto a = x;` 和 `auto &a = x;` 的区别
- [ ] 会四种 cast，至少熟练 `static_cast`
- [ ] 完成改写练习，特别注意整数溢出

→ 下一章：[[第13章-模板与STL容器]]　—— 会读会写简单模板，会用 `vector`/`map`/`optional`/`span`/`string_view`。
