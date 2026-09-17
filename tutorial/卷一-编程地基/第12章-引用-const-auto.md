---
tags: [教程, 卷一, C++]
day: 7
aliases: [ch12]
---

# 第 12 章 · 引用、const、auto

> [!abstract] 本章目标
> 掌握现代 C++ 的三个书写习惯：用引用代替指针传参、到处加 `const`、用 `auto` 减少重复。
> 本项目代码里这三个符号占比极高，不熟会读得很痛苦。

> [!note] 承上
> 上一章你学会了类。本章学三个高频写法：引用、`const`、`auto`——
> 它们是本项目代码里最常见的三个符号。

## 先搞清：为什么 C++ 要发明"引用"

第 05 章你学了指针——**指针很强，但也很危险**：

```cpp
void process(Player *p) {
    if (p == nullptr) return;   // 每次都得判空，忘了判就崩
    p->hp = 100;
}

Player *p = nullptr;
process(p);         // 传个空指针，函数里没判就崩
```

**指针的三个麻烦**：

1. **可能是空的**——每次都要判 `if (p == nullptr)`
2. **需要写 `*p` 或 `p->`**——看起来不清爽
3. **可以乱改指向**——`p = &something_else;` 悄悄地改，很难追踪

C++ 发明了**引用（reference）**来解决这三个问题。

**引用 = 变量的"别名"**。给一个变量起个别名，用别名和用原名是**同一个东西**。

```cpp
int hp = 100;
int &ref = hp;      // ref 是 hp 的别名

ref = 200;          // 改 ref 就是改 hp
printf("%d\n", hp);  // 200
```

**`&` 在这里是"引用声明"的意思**（和取地址的 `&` 长得一样，但位置不同含义不同，本章会辨析）。

### 引用的三条铁律

引用和指针最根本的区别，就在这三条：

| 铁律 | 引用 | 指针 |
|---|---|---|
| **必须初始化** | 声明时必须绑定到一个变量 | 可以声明不初始化 |
| **不能为空** | 永远指向某个东西 | 可以是 `nullptr` |
| **不能改绑** | 一旦绑定，永远是这个变量 | 可以随时改指向 |

用错就会编译报错：

```cpp
int &r1;            // 错：引用必须初始化
int *p1;            // 对：指针可以不初始化

int a = 1, b = 2;
int &r2 = a;        // 对：绑定到 a
r2 = b;             // 注意！这不是"改绑到 b"，而是"把 b 的值赋给 a"
                    // a 变成 2，r2 还是 a 的别名

int &r3 = nullptr;  // 错：引用不能是空
```

> [!important] `r2 = b;` 最容易误解
> 你可能会想"让 r2 从 a 改成绑定到 b"。
> **不行！引用一旦绑定不能改。** `r2 = b;` 的语义是"把 b 的值赋给 r2 指向的东西"，也就是"把 b 的值赋给 a"。
> 执行后 `a == 2`，但 `r2` 依然绑定 `a`。

### 引用就是"不会出问题的指针"

从机器实现上看，引用**几乎就是指针**（底层也是一段地址）。但语言层面它**更安全**：

| 好处 | 说明 |
|---|---|
| **不用判空** | 引用永远有效 |
| **语法清爽** | `r.hp` 而不是 `p->hp` |
| **不会改绑** | 传进函数就不会被偷偷指向别处 |

**代价**：**声明时必须绑定**，而且**不能改绑**。所以适合"我就用一下"的场景，不适合"我可能需要重新指向别的"的场景。

### 引用传参：省掉判空和箭头

对比一下同一个功能的三种写法：

```cpp
// 写法 1：值传递（拷贝一份）
void damage_v(Player p) { p.hp -= 10; }    // 改的是副本，外面不变

// 写法 2：指针传递（能改外面，但要判空、用 ->）
void damage_p(Player *p) {
    if (p == nullptr) return;
    p->hp -= 10;
}

// 写法 3：引用传递（能改外面，不用判空、用点号）
void damage_r(Player &p) {
    p.hp -= 10;      // 直接改外面的对象
}

// 用：
Player pl;
damage_v(pl);        // 改的是副本，pl.hp 不变
damage_p(&pl);       // 要传地址
damage_r(pl);        // 直接传对象，函数内部当引用用
```

**引用传参的写法最干净**：调用时看起来像值传递（直接 `damage_r(pl)`），但函数内部可以改到外面。

### 引用 vs 指针：什么时候用哪个

| 场景 | 用哪个 |
|---|---|
| **函数参数**（能改到外面） | **优先引用**（更安全、语法清爽） |
| **函数参数**（可能为空，表示"可选"） | 指针（可以传 `nullptr`） |
| **函数返回**（返回一个已存在的对象） | 引用 |
| **需要动态分配的对象** | 指针（第 14 章会用 `unique_ptr`） |
| **需要改指向** | 指针 |

**本项目的惯例**：几乎全部参数用引用（`const &` 或普通 `&`）；返回已存在对象用引用；只有"可能为空"和"动态分配"才用指针。

### 动手：验证引用的行为

```cpp
#include <cstdio>

int main() {
    int a = 10;
    int &r = a;        // r 是 a 的别名

    printf("a=%d, r=%d\n", a, r);    // 10 10

    r = 20;                            // 改 r
    printf("a=%d, r=%d\n", a, r);    // 20 20（a 也变了）

    a = 30;                            // 改 a
    printf("a=%d, r=%d\n", a, r);    // 30 30（r 也变了）

    printf("&a=%p, &r=%p\n", (void*)&a, (void*)&r);
    // 打印出来是同一个地址！证明 r 就是 a

    return 0;
}
```

**关键观察**：`&a` 和 `&r` 打印出来是**同一个地址**。这就是"引用是别名"的物理含义。

> [!tip] 为什么 `&r` 不报错（r 是引用，怎么还能取地址）
> 你可能会困惑：`&` 不是"取地址"吗？`r` 是引用，取它的地址应该是引用变量的地址吧？
> **不是。** 对引用取地址，返回的是**被引用变量的地址**。
> 也就是说：`&r` 等价于 `&a`。因为 `r` 和 `a` 是同一个东西，取谁地址都一样。
> **这就是"引用不是一个独立对象，只是别名"的体现。**


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

**引用一旦建立，就始终代表那一个变量**——不能改指别人，也不会是空。

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

> [!note] 这里的 class 和 public: 先认识一下
> 下面的 `class Player { ... };` 是定义一个 C++ 类（第 11 章讲过 struct，class 和它几乎一样，只是默认访问权限不同）。
> `public:` 表示下面这些成员外部可以访问。
> 本节的重点是 const 成员函数，类的其他细节不用纠结。

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

// 2. lambda（匿名函数，第 13 章会讲）
auto finite = [](const Vector2A &p) {
    return std::isfinite(p.X) && std::isfinite(p.Y);   // isfinite 判断是否为有效数值
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

## 课后习题

### 习题 12.1 用引用改写“交换两个数”（★）

**任务要求**：写函数 `void swap_int(int &a, int &b)` 交换两数（用引用，不用指针）。再写调用代码验证交换成功。

**参考实现**：

```cpp
#include <cstdio>
#include <cassert>

void swap_int(int &a, int &b) {
    int t = a;
    a = b;
    b = t;
}

int main() {
    int x = 3, y = 8;
    swap_int(x, y);
    printf("x=%d y=%d\n", x, y);   // x=8 y=3
    assert(x == 8 && y == 3);
    printf("习题 12.1 通过\n");
    return 0;
}
```

**验证断言**：

```cpp
int main() {
    int x = 1, y = 2;
    swap_int(x, y);
    assert(x == 2 && y == 1);
    x = -5; y = 5;
    swap_int(x, y);
    assert(x == 5 && y == -5);
    printf("习题 12.1 全部通过\n");
    return 0;
}
```

> [!tip] 为什么用引用不用指针
> 引用调用时写 `swap_int(x, y)`（清爽），不用 `swap_int(&x, &y)`，函数体里也直接用 `a`/`b` 而非 `*a`/`*b`。这正是本章“引用传参优于指针传参”的实践。

### 习题 12.2 `const &` 避免大对象拷贝（★★）

**任务要求**：定义 `struct Big { int data[1000]; }`（约 4KB）。写两个函数：`int sum_by_value(Big b)`（值传递）和 `int sum_by_cref(const Big &b)`（const 引用）。两者结果相同，并说明为什么 `const &` 更好。

**参考实现**：

```cpp
#include <cstdio>
#include <cassert>

struct Big { int data[1000]; };

int sum_by_value(Big b) {            // ★ 拷贝 4KB
    int s = 0;
    for (int i = 0; i < 1000; i++) s += b.data[i];
    return s;
}

int sum_by_cref(const Big &b) {      // ★ 只传地址，不拷贝，且承诺不改
    int s = 0;
    for (int i = 0; i < 1000; i++) s += b.data[i];
    return s;
}

int main() {
    Big big{};
    for (int i = 0; i < 1000; i++) big.data[i] = i;
    assert(sum_by_value(big) == sum_by_cref(big));
    printf("sum=%d\n", sum_by_cref(big));
    printf("习题 12.2 通过\n");
    return 0;
}
```

> [!note] 值传递 vs const 引用
> `sum_by_value` 每次调用**拷贝整个 4KB 结构体**；`sum_by_cref` 只传一个地址（8 字节）。大对象参数一律用 `const &`——这正是本项目 `draw(const Matrix &m)` 的写法（Matrix 是 64 字节）。

## 本章小结

> [!abstract] 本章要点已收束
> 回头把本页的"动手"与结论串成一句话；有勾不上的验收项，回去补做。

## 验收清单

- [ ] 能说出引用和指针的三个区别（"必须初始化/不能为空/不能改绑"）
- [ ] 知道什么时候用 `const &` 传参（大结构体、只读）（写一个接收 `const Matrix&` 的函数，说明避免拷贝 64 字节）
- [ ] 能看懂 `const virtual_memory &GetMemoryInfoRef() const` 里两个 const 的含义（前一个修饰返回值，后一个修饰成员函数）
- [ ] 知道 `auto a = x;` 和 `auto &a = x;` 的区别（写两行，改 `a` 后观察原变量是否变化）
- [ ] 会四种 cast，至少熟练 `static_cast`（用 `static_cast` 做一次 float→int）
- [ ] 完成改写练习，特别注意整数溢出（运行改写后的代码，确认无溢出、结果正确）

→ 下一章：[[第13章-模板与STL容器]]　—— 会读会写简单模板，会用 `vector`/`map`/`optional`/`span`/`string_view`。
