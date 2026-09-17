---
tags: [教程, 卷一, C++, STL]
day: 8
aliases: [ch13]
---

# 第 13 章 · 模板与 STL 容器

> [!abstract] 本章目标
> 会读会写简单模板，会用 `vector`/`map`/`optional`/`span`/`string_view`。
> 本项目 `dr->Read<T>(addr)` 这种写法就靠模板实现。

> [!note] 承上
> 上一章学了现代 C++ 的书写习惯。本章学**模板**和 **STL 容器**——
> `dr->Read<T>(addr)` 这种"什么类型都能读"的写法，就靠模板实现。

## 先搞清：STL 是什么，为什么需要它

到目前为止，你学的"容器"只有一种：**数组**（第 06 章）。

```c
int scores[5];      // 5 个 int，长度写死
```

数组有两个硬伤：

| 硬伤 | 后果 |
|---|---|
| **长度固定** | 声明时写 5，就不能装 6 个；写 1000 又浪费 |
| **不能"自动增长"** | 想加一个元素，得手动搬数据 |

真实程序里，"能装多少个"往往**运行时才知道**：

- 从文件读到多少个 Actor？不定
- 网络收到多少字节？不定
- 用户输入多长的字符串？不定

**所以 C++ 标准库提供了一批"容器"**——能动态增长、能自动管理内存的"智能数组"。

### 什么是 STL

**STL = Standard Template Library（标准模板库）**，是 C++ 标准库的一部分，提供：

| 类别 | 例子 | 作用 |
|---|---|---|
| **容器（container）** | `vector`、`map`、`set` | 装数据的盒子 |
| **算法（algorithm）** | `sort`、`find`、`all_of` | 对容器做操作 |
| **迭代器（iterator）** | `begin()`、`end()` | 遍历容器的"指针" |
| **工具** | `pair`、`optional`、`string_view` | 各种小工具 |

**你现在只要会 `vector` 和几个常用的**，剩下的用到了再说。

### 为什么要有"模板"

STL 容器之所以"什么类型都能装"，靠的是**模板（template）**。

举个具体问题——**如果没有模板，你要给每种类型写一遍 `vector`**：

```cpp
class IntVector    { int    *data; ... };    // 装 int 的
class FloatVector  { float  *data; ... };    // 装 float 的
class StringVector { std::string *data; ... }; // 装 string 的
// ... 无穷无尽
```

**用了模板，一个就够**：

```cpp
template <typename T>       // T 是"待定的类型"
class Vector { T *data; ... };

Vector<int>         a;      // 编译器自动生成"装 int 的 Vector"
Vector<float>       b;      // 编译器自动生成"装 float 的 Vector"
Vector<std::string> c;      // 编译器自动生成"装 string 的 Vector"
```

**模板的作用就是"把类型也变成参数"**——写一次，用的时候指定装什么。

> [!important] 尖括号 `<>` 是模板的标志
> `Vector<int>`、`std::vector<float>`、`dr->Read<uint64_t>(addr)` —— 尖括号里的就是**模板参数**。
> 你以后会经常看到这种写法，读作"`std::vector` of `int`"、"`vector` 装 `int`"。

### 从 `vector` 开始：能自动增长的数组

**`std::vector` 是"动态数组"**——和数组一样是"一排同类型元素"，但：

- 长度可以随时变（加元素、删元素）
- 内部自动管理内存（不用手动 `malloc`/`free`）
- 支持 `[]` 下标访问（和数组一样）

**用法**：

```cpp
#include <vector>       // 用 vector 必须包含这个头文件

std::vector<int> v;     // 创建一个空 vector，里面装 int
```

**加元素**：

```cpp
v.push_back(10);        // 在末尾加一个 10
v.push_back(20);        // 现在 [10, 20]
v.push_back(30);        // 现在 [10, 20, 30]
```

**看大小**：

```cpp
printf("%zu\n", v.size());     // 3
printf("%d\n", v.empty());     // 输出 0
// 说明：empty() 问"你空吗？"——v 里有 3 个元素，所以它回答"不空"（false），
// 打印成整数就是 0。对照：若 v 是空的，empty() 返回 true，打印出 1。
```

**访问元素**（和数组一样）：

```cpp
v[0]              // 10
v[1]              // 20
v[2]              // 30
```

**遍历**（两种方式）：

```cpp
// 方式 1：传统下标
for (size_t i = 0; i < v.size(); i++) {
    printf("%d\n", v[i]);
}

// 方式 2：范围 for（C++11 起）
for (int x : v) {
    printf("%d\n", x);
}
```

**`for (int x : v)`** 读作"对 `v` 里的每个元素 `x`，做……"。**这是 C++ 特有的遍历写法**，比下标更简洁。

**清空 / 删除**：

```cpp
v.clear();        // 清空所有元素
v.pop_back();     // 删掉最后一个
```

### `vector` vs 数组：什么时候用哪个

| 场景 | 用什么 |
|---|---|
| **数量运行时才知道** | **`vector`** |
| **数量编译期固定且很小** | 数组也行 |
| **需要频繁随机访问** | 都行，`vector` 和数组都是 O(1) |
| **需要频繁在中间插入/删除** | 都不是好选择（用 `list`，但现在先不管） |

**现代 C++ 的惯例**：**默认用 `vector`**，只有特殊情况（极小的固定数组、性能敏感的栈上数组）才用裸数组。

**本项目里 `vector` 到处都是**：网格数据、扫描区域、三角形列表、缓存……全部用 `vector`。

### 动手：第一个 `vector` 程序

```cpp
#include <cstdio>
#include <vector>

int main() {
    std::vector<int> v;

    // 加 5 个元素
    for (int i = 1; i <= 5; i++) {
        v.push_back(i * 10);
    }

    printf("size = %zu\n", v.size());   // 5

    // 遍历打印
    for (int x : v) {
        printf("%d ", x);
    }
    printf("\n");   // 10 20 30 40 50

    // 用下标改
    v[0] = 999;
    printf("v[0] = %d\n", v[0]);   // 999

    // 求和
    int sum = 0;
    for (int x : v) sum += x;
    printf("sum = %d\n", sum);

    return 0;
}
```

编译：`g++ -std=c++20 -Wall vec_demo.cpp -o vec_demo.exe`

**关键点**：

- `#include <vector>` 才能用
- 类型写 `std::vector<int>`——尖括号里指定元素类型
- `push_back` 加元素，`size()` 看个数，`v[i]` 访问，范围 for 遍历
- 不用管内存——`vector` 析构时自动释放（第 14 章会讲，这就是 RAII）

## 先看一个让你少写 20 个函数的东西

有了前面 `vector` 的铺垫，现在看第 04 章那个 `dr->Read<T>` 的动机就清楚了。

不用模板，读取不同类型要写一堆：

```cpp
uint8_t  ReadU8 (uint64_t addr);
uint16_t ReadU16(uint64_t addr);
uint32_t ReadU32(uint64_t addr);
uint64_t ReadU64(uint64_t addr);
float    ReadF32(uint64_t addr);
double   ReadF64(uint64_t addr);
Vec3     ReadVec3(uint64_t addr);      // 还有自定义类型...
Matrix   ReadMatrix(uint64_t addr);
// 无穷无尽
```

用模板，一个函数搞定：

```cpp
template <typename T>
T Read(uint64_t address) {
    T value = {};
    if (Read(address, &value, sizeof(T)) <= 0) value = T{};
    return value;
}
```

> [!note] 这段代码里的两个新语法
> 上面那个 `Read` 函数用到了两个卷一还没讲的东西：
>
> **① `T value = {};` 和 `T{}`**：这是"**值初始化**"语法——用一对空花括号表示"**把这个类型初始化为默认值**"。
> - 对 `int`，`int value = {}` 就是 `0`
> - 对指针，就是 `nullptr`
> - 对结构体，就是"所有字段都默认初始化"
> 用法很简单：**想"清零/清空"一个任意类型的变量，就写 `= {}`**。
>
> **② 两个同名 `Read` 函数**：一个是 `int Read(uint64_t, void*, size_t)`（普通函数），另一个是 `T Read(uint64_t)`（模板）。
> **C++ 允许"同名但参数不同"的多个函数共存**，这叫做**函数重载（overload）**——编译器根据你传的参数自动选哪个。
> 调用 `dr.Read<int32_t>(addr)` 时匹配模板版，调用 `dr.Read(addr, buf, size)` 时匹配普通版。
>
> 这两个语法现在"知道有这回事"即可，后面的章节会自然地再遇到。

用法：

```cpp
auto hp     = dr->Read<int32_t>(addr);
auto fov    = dr->Read<float>(addr + 0x2318);
auto matrix = dr->Read<Matrix>(addr);
auto ptr    = dr->Read<uint64_t>(addr);
```

**这就是本项目 `IDriver` 里的真实代码。** 模板让你把"类型"也变成参数。

## 模板语法

```cpp
template <typename T>        // 或 template <class T>，完全等价
T max_value(T a, T b) {
    return a > b ? a : b;
}

max_value(3, 5);          // 编译器生成 int 版
max_value(1.5, 2.5);      // 编译器生成 double 版
```

编译器在**调用点**根据实参类型生成对应版本的代码。这叫"实例化"。

多个类型参数：

```cpp
template <typename K, typename V>
struct Pair { K key; V value; };
```

非类型模板参数：

```cpp
template <typename T, size_t N>
struct Array {
    T data[N];
    size_t size() const { return N; }
};

Array<uint8_t, 256> buf;     // N=256 是编译期常量
```

## 为什么模板必须放头文件

因为编译器要看到完整定义才能实例化。放在 `.cpp` 里，别的编译单元只看到声明，
链接时会报 `undefined reference`。

（第 09 章讲过，这里重复是因为这是最容易踩的坑之一。）

## 本项目里的模板

```cpp
// TArray：通用数组容器
template <typename T>
struct TArray {
    uintptr_t base;
    int32_t   count;
    int32_t   max;

    std::vector<T> ToVec() const;
    T operator[](size_t u) const { return dr->Read<T>(base + u * sizeof(T)); }
    bool IsValid() const;
};
```

> [!note] `operator[]` 是"运算符重载"，先忽略
> 上面那行 `T operator[](size_t u) const` 是 C++ 的**运算符重载**——它让 `TArray` 对象也能用 `[]` 下标访问（就像数组一样）。
> **卷一不要求你掌握运算符重载**，这里出现只是因为它是本项目 `PhysX.h` 里的真实代码。
> 你只要知道：**看到 `operator` 开头的函数，就是"重载了某个运算符"。** 用的时候照常用 `arr[i]` 就行。

用法：

```cpp
TArray<uint64_t> actors;      // 元素是 8 字节指针
TArray<PrunerPayload> payloads;  // 元素是自定义结构
```

`sizeof(T)` 让偏移计算自动适配元素大小——这就是模板的威力。

## STL 容器速查

| 容器 | 用途 | 本项目用法 |
|---|---|---|
| `std::vector<T>` | 动态数组 | 网格数据、扫描区域、三角形 |
| `std::map<K,V>` / `unordered_map` | 键值映射 | 缓存 |
| `std::set<T>` / `unordered_set` | 去重集合 | 已处理集合 |
| `std::string` | 字符串 | 类名、路径 |
| `std::string_view` | 字符串只读视图 | 参数传递（零拷贝） |
| `std::span<T>` | 数组只读/可写视图 | 传数组给函数 |
| `std::optional<T>` | 可能有值可能没有 | 替代"用 -1 表示失败" |
| `std::pair<A,B>` | 二元组 | 地址区间 |

> [!note] `std::pair` 是什么
> `std::pair<A, B>` 是**把两个值捆成一对**的简单容器——`first` 是第一个，`second` 是第二个。
> ```cpp
> std::pair<uintptr_t, uintptr_t> region = {0x1000, 0x2000};
> printf("起: %llx 止: %llx\n", region.first, region.second);
> ```
> 本项目的"内存区间"就用它表示（起地址 + 止地址）。

### vector：用得最多的容器

```cpp
#include <vector>

std::vector<uint64_t> actors;
actors.push_back(0x1000);
actors.reserve(1000);            // 预分配，避免反复扩容

for (size_t i = 0; i < actors.size(); i++) { ... }
for (const auto &a : actors) { ... }      // 推荐

actors.clear();
actors.empty();
```

> [!tip] 大量 push_back 前先 reserve
> `vector` 扩容时要重新分配内存并拷贝全部元素。
> 本项目 `GetScanRegions()` 里就有：
> ```cpp
> regions.reserve(static_cast<size_t>(regionCount) + moduleCount * 3);
> ```

### 结构体 vector 的坑

```cpp
std::vector<TriangleMeshData> meshes;
meshes.push_back(m);      // 拷贝一份
meshes.emplace_back(...); // 原地构造，少一次拷贝（推荐）
```

### 迭代器：遍历容器的"通用指针"

在讲 `unordered_map` 之前，先补一个所有 STL 容器共有的概念：**迭代器（iterator）**。

**迭代器 = 指向容器中某个元素的"指针"**。它可以 `++`（移到下一个），可以 `*`（取当前元素），可以比较。

```cpp
#include <vector>

std::vector<int> v = {10, 20, 30};

auto it = v.begin();     // it 指向第一个元素
printf("%d\n", *it);    // 10

++it;                    // 移到下一个
printf("%d\n", *it);    // 20

it = v.end();            // end() 指向"最后一个元素之后"
```

| 表达式 | 含义 |
|---|---|
| `v.begin()` | 指向第一个元素 |
| `v.end()` | 指向**最后一个元素之后**（不是最后一个！） |
| `*it` | 取迭代器当前指向的元素 |
| `++it` / `it++` | 移到下一个 |
| `it != v.end()` | "还没走到末尾" |

**为什么 `end()` 指向"最后一个之后"**：这样用 `it != v.end()` 判断就能覆盖所有元素——当 `it` 越过最后一个时，正好等于 `end()`，循环退出。

**标准的遍历写法**：

```cpp
for (auto it = v.begin(); it != v.end(); ++it) {
    printf("%d\n", *it);
}
```

**范围 for 就是它的语法糖**（第 12 章讲过）：

```cpp
for (int x : v) { printf("%d\n", x); }   // 内部就是上面那个循环
```

**什么时候必须用迭代器**：当容器**不支持下标访问**时（比如 `unordered_map`、`set`），或者需要"在遍历中删除元素"时。**普通的 `vector` 遍历优先用范围 for。**

### unordered_map：哈希表（键值对）

**哈希表**存的是"**键 → 值**"的映射——给一个键，立刻找到对应的值。

```cpp
#include <unordered_map>
std::unordered_map<uint64_t, RTCGeometry> cache;   // 键是 uint64_t，值是 RTCGeometry

cache[100] = geomA;         // 插入：键 100 → geomA
cache[200] = geomB;         // 键 200 → geomB

RTCGeometry g = cache[100]; // 查找：拿到 geomA
```

**`[...]` 访问时，键不存在会"自动创建"**——这点要注意：

```cpp
std::unordered_map<int, int> m;
int x = m[5];       // 键 5 不存在 → 自动创建一个，值是 0，x = 0
```

**如果想"只在存在时才取"**，用 `find`（这就需要迭代器）：

```cpp
auto it = cache.find(shape);    // 返回迭代器
if (it != cache.end()) {
    // 找到了：it->first 是键，it->second 是值
    use(it->second);
} else {
    // 没找到
    cache[shape] = newGeom;     // 现在插入
}
```

**`it->first` 和 `it->second`**：`unordered_map` 的每个元素是一个 `pair<键, 值>`，`.first` 是键，`.second` 是值。

**本项目 `VisibleScene` 用 `unordered_map` 缓存 shape → geometry 的映射**——每次几何更新时，通过 shape 找对应的 Embree 几何 ID。


```cpp
#include <unordered_map>
std::unordered_map<uint64_t, RTCGeometry> cache;

// 查找
auto it = cache.find(shape);
if (it != cache.end()) {
    // 找到了，it->second 是值
} else {
    cache[shape] = newGeom;      // 插入
}
```

本项目 `VisibleScene` 用 `unordered_map` 缓存 shape → geometry 的映射。

### string：C++ 的字符串类型

第 06 章讲过 C 的字符串——**以 `\0` 结尾的 `char` 数组**。它不好用：

- 长度得自己记（或每次 `strlen` 算）
- 复制要用 `strcpy`（还得自己保证目标够大）
- 拼接要用 `strcat`（同样有溢出风险）

**C++ 的 `std::string` 解决了这些问题**——它自动管理内存、能直接赋值、能拼接、自带长度：

```cpp
#include <string>

std::string s = "hello";       // 直接赋值
std::string t = s;             // 直接拷贝（自动分配内存）
std::string u = s + " world";  // 直接拼接（+ 运算符）

printf("%zu\n", s.size());    // 5（自带长度）
```

**常用操作**：

| 操作 | 写法 | 说明 |
|---|---|---|
| 构造 | `std::string s = "abc";` | 从字符串字面量 |
| 拷贝 | `std::string t = s;` | 深拷贝，独立 |
| 拼接 | `s + "x"` 或 `s += "x"` | 自动扩容 |
| 长度 | `s.size()` / `s.length()` | 元素个数 |
| 判空 | `s.empty()` | 比 `s.size() == 0` 更清楚 |
| 取字符 | `s[i]` | 第 i 个字符 |
| 转 C 字符串 | `s.c_str()` | 得到 `const char*`，可给 `printf` 用 |
| 查找 | `s.find("x")` | 返回位置，找不到返回 `std::string::npos` |

```cpp
std::string name = "Player";
name += "_01";                          // "Player_01"
if (name.find("Player") != std::string::npos) {
    printf("找到了\n");
}
printf("%s\n", name.c_str());           // 传给 printf 要 .c_str()
```

> [!important] 为什么 `printf` 要写 `.c_str()`
> `printf` 的 `%s` 要的是"以 `\0` 结尾的 `char*`"，而 `std::string` 是"对象"。
> `.c_str()` 就是"取出内部那个 `const char*`"。
> 如果直接用 `printf("%s", name)` 会出错——**必须 `.c_str()`**。
> （用 C++ 的 `std::cout << name` 则不需要，但本项目主要用 `printf`。）

> [!note] `std::string` vs `std::string_view`
> - `std::string`：**拥有**字符数据（自己管内存），可修改
> - `std::string_view`：**不拥有**数据（只是"看"一段已有的字符串），只读、零拷贝
> 传参数时用 `string_view`（省一次拷贝），存数据时用 `string`。**下一个节详讲。**

### string_view：零拷贝字符串参数

```cpp
// 不好：会构造一个 std::string（可能分配内存）
void foo(const std::string &name);
foo("libUE4.so");

// 好：string_view 只是 {指针, 长度}，不分配
void foo(std::string_view name);
```

`string_view` 常用方法（和 `string` 类似）：

| 方法 | 作用 |
|---|---|
| `.size()` / `.length()` | 长度（字符数） |
| `.empty()` | 是否为空 |
| `.data()` | 首字符指针（**不保证以 \0 结尾**） |
| `[i]` | 第 i 个字符 |

```cpp
std::string_view sv = "hello";
printf("len=%zu\n", sv.size());      // 5
if (sv.empty()) { /* 空串 */ }
printf("首字符=%c\n", sv[0]);          // h
```

> [!warning] `string_view` 没有 `c_str()`
> 它**不保证以 `\0` 结尾**，所以**不能直接传给 `printf("%s")`**。
> 打印它要用 `%.*s` 指定长度（第 16 章会讲）：
> ```cpp
> printf("%.*s", (int)sv.size(), sv.data());
> ```

本项目的接口全部用 `string_view`：

```cpp
int GetPid(std::string_view packageName) override;
bool GetModuleAddress(std::string_view moduleName, short segmentIndex,
                      uint64_t *outAddress, bool isStart) override;
bool DumpMemory(std::string_view target, std::string *dumpPath = nullptr) override;
```

> [!danger] string_view 不拥有数据
> ```cpp
> std::string_view bad() {
>     std::string s = "hello";
>     return s;                    // 灾难：s 已被销毁
> }
> ```
> `string_view` 只适合做**参数**和指向长期存在的数据。

### optional：优雅地表达"没有值"

```cpp
#include <optional>

// 老办法：用特殊值表示失败，容易忘判
uint64_t find_module(const char *name);   // 返回 0 表示没找到？还是 -1？

// 新办法
std::optional<uint64_t> find_module(std::string_view name);

if (auto addr = find_module("libUE4.so")) {
    use(*addr);        // 有值
} else {
    // 没找到
}
```

`optional` 强制你处理"没有值"的情况，不会忘记判空。

### span：传数组给函数

```cpp
#include <span>

int SetProcessHwbpRef(std::span<const bp_point> points);

// 调用：vector、普通数组都能传
std::vector<bp_point> v;
SetProcessHwbpRef(v);

bp_point arr[4];
SetProcessHwbpRef(arr);
```

比 `bp_point* ptr, size_t n` 两个参数更不容易出错。

## lambda：匿名函数

```cpp
auto finite = [](const Vector2A &p) {
    return std::isfinite(p.X) && std::isfinite(p.Y);
};

if (finite(pt[0])) { ... }
```

语法：`[捕获](参数) { 函数体 }`

| 捕获 | 含义 |
|---|---|
| `[]` | 不捕获外部变量 |
| `[&]` | 按引用捕获所有（少用） |
| `[=]` | 按值捕获所有（少用） |
| `[this]` | 捕获当前对象 |
| `[x, &y]` | x 按值，y 按引用 |

常用在排序、遍历回调：

```cpp
std::sort(regions.begin(), regions.end(),
    [](const auto &l, const auto &r) {
        return l.first < r.first || (l.first == r.first && l.second < r.second);
    });
```

本项目 `driver.h` 里就有一段几乎一样的（`GetScanRegions` 里合并区间时的排序）。

## 动手：写一个通用读取器

```cpp
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>
#include <string>
#include <optional>

// 模拟"另一个进程的内存"
std::vector<uint8_t> g_memory;

class FakeDriver {
public:
    int Read(uint64_t addr, void *buf, size_t size) {
        if (addr + size > g_memory.size()) return -1;
        memcpy(buf, g_memory.data() + addr, size);
        return (int)size;
    }

    template <typename T>
    T Read(uint64_t addr) {
        T v = {};
        if (Read(addr, &v, sizeof(T)) <= 0) v = T{};
        return v;
    }

    template <typename T>
    std::vector<T> ReadArray(uint64_t addr, size_t count) {
        std::vector<T> out(count);
        for (size_t i = 0; i < count; i++)
            out[i] = Read<T>(addr + i * sizeof(T));
        return out;
    }

    std::optional<std::string> ReadString(uint64_t addr, size_t maxLen = 128) {
        if (addr == 0 || addr >= g_memory.size()) return std::nullopt;
        std::string s;
        for (size_t i = 0; i < maxLen && addr + i < g_memory.size(); i++) {
            char c = Read<char>(addr + i);
            if (c == 0) break;
            s += c;
        }
        return s;
    }
};

int main(void) {
    g_memory.resize(1024, 0);
    // 在偏移 0x100 放一个 int32 = 1234
    int32_t v = 1234;
    memcpy(g_memory.data() + 0x100, &v, 4);
    // 在 0x200 放字符串
    strcpy((char*)g_memory.data() + 0x200, "BP_Character");
    // 在 0x300 放 5 个 uint64
    for (int i = 0; i < 5; i++) {
        uint64_t a = 0x1000 * (i + 1);
        memcpy(g_memory.data() + 0x300 + i * 8, &a, 8);
    }

    FakeDriver dr;
    printf("int32  = %d\n", dr.Read<int32_t>(0x100));
    // value_or：optional 有值就返回它，没值就返回括号里的默认值
    // c_str()：把 std::string 转成 C 风格的 const char*，好给 printf 用
    printf("string = %s\n", dr.ReadString(0x200).value_or("<null>").c_str());

    auto arr = dr.ReadArray<uint64_t>(0x300, 5);
    for (const auto &a : arr) printf("  0x%llX\n", (unsigned long long)a);

    printf("越界读 = %d\n", dr.Read<int32_t>(0x9999));   // 应输出 0
    return 0;
}
```

编译：`g++ -std=c++17 -Wall fake.cpp -o fake.exe`

这个 `FakeDriver` 就是第 75 章真实跨进程读写类的雏形——先把逻辑跑通，再换成真驱动。

## 课后习题

### 习题 13.1 写一个泛型 `max_value`（★）

**任务要求**：用模板实现 `max_value(a, b)`，对 `int`/`float`/`double` 都能工作。

**参考实现**：

```cpp
#include <cstdio>
#include <cassert>

template <typename T>
T max_value(T a, T b) {
    return (a > b) ? a : b;
}

int main() {
    assert(max_value(3, 5) == 5);
    assert(max_value(1.5, 2.5) == 2.5);
    assert(max_value(-1, -9) == -1);
    printf("max(3,5)=%d  max(1.5,2.5)=%.1f\n", max_value(3,5), max_value(1.5,2.5));
    printf("习题 13.1 全部通过\n");
    return 0;
}
```

> [!tip] 模板 = 把类型也变成参数
> 写一次，编译器为每种用到的 `T` 生成一个版本。这正是 `dr->Read<T>(addr)` 能读任意类型的原因。

### 习题 13.2 用 `optional` 表达“可能没有”（★★）

**任务要求**：写函数 `std::optional<int> find_index(const std::vector<int>& v, int target)`，
找到返回下标，找不到返回 `std::nullopt`。

**参考实现**：

```cpp
#include <cstdio>
#include <vector>
#include <optional>
#include <cassert>

std::optional<int> find_index(const std::vector<int>& v, int target) {
    for (int i = 0; i < (int)v.size(); i++)
        if (v[i] == target) return i;
    return std::nullopt;      // 明确的“没有值”
}

int main() {
    std::vector<int> v = {10, 20, 30, 40};
    auto a = find_index(v, 30);
    auto b = find_index(v, 99);
    assert(a.has_value() && *a == 2);
    assert(!b.has_value());
    // 惯用写法：带初始化的 if
    if (auto idx = find_index(v, 20)) printf("找到 20 在下标 %d\n", *idx);
    printf("习题 13.2 全部通过\n");
    return 0;
}
```

> [!note] 为什么用 optional 而不是返回 -1
> 返回 `-1` 需要调用方记住"-1 表示没有"这个约定；`optional` 把"可能没有"写进了类型里，
> 调用方必须显式处理 `has_value()`。本项目 C++17 起大量用这套（第 15 章 `parse_pid`）。

### 习题 13.3 `string_view` 零拷贝传参（★★）

**任务要求**：写 `bool starts_with(std::string_view s, std::string_view prefix)`，
用 `string_view` 避免拷贝，并用 `substr` 分割验证。

**参考实现**：

```cpp
#include <cstdio>
#include <string_view>
#include <string>      // std::string
#include <cassert>

bool starts_with(std::string_view s, std::string_view prefix) {
    return s.size() >= prefix.size() &&
           s.substr(0, prefix.size()) == prefix;
}

int main() {
    std::string_view sv("libUE4.so");
    assert(starts_with(sv, "lib"));
    assert(!starts_with(sv, "xyz"));
    // 切出模块名部分（不拷贝）
    std::string_view name = sv.substr(0, 5);   // "libUE"
    printf("name=%s\n", std::string(name).c_str());
    assert(name == "libUE");
    printf("习题 13.3 全部通过\n");
    return 0;
}
```

> [!warning] `string_view` 不拥有数据
> 上面 `name` 只是指向 `sv` 的一段，`sv` 一旦销毁，`name` 就悬空。
> 所以**不能返回局部变量的 view**（本章"不拥有数据"警告）。

## 验收清单

- [ ] 理解 `template <typename T>` 是什么，能写一个 `max_value`（写 `max_value(3,5)` 和 `max_value(1.5,2.5)` 验证泛型生效）
- [ ] 能解释 `dr->Read<float>(addr)` 背后发生了什么（"编译器生成 float 版 Read，读 4 字节"）
- [ ] 会用 `vector`（含 `reserve`）、`unordered_map`、`optional`、`string_view`（各写一段最小示例运行）
- [ ] 知道 `string_view` 不拥有数据，不能返回局部变量的 view（写错误版本，观察悬空）
- [ ] 跑通了 FakeDriver，包括越界返回 0 的情况（运行程序，确认越界读返回 0）

→ 下一章：[[第14章-智能指针与生命周期]]　—— 不再手写 `new`/`delete`，能用 `unique_ptr`/`shared_ptr` 表达清楚"谁拥有这块内存"。
