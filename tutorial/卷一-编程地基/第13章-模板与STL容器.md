---
tags: [教程, 卷一, C++, STL]
day: 7
aliases: [ch13]
---

# 第 13 章 · 模板与 STL 容器

> [!abstract] 本章目标
> 会读会写简单模板，会用 `vector`/`map`/`optional`/`span`/`string_view`。
> 本项目 `dr->Read<T>(addr)` 这种写法就靠模板实现。

## 先看一个让你少写 20 个函数的东西

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

### unordered_map：哈希表

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

### string_view：零拷贝字符串参数

```cpp
// 不好：会构造一个 std::string（可能分配内存）
void foo(const std::string &name);
foo("libUE4.so");

// 好：string_view 只是 {指针, 长度}，不分配
void foo(std::string_view name);
```

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
    printf("string = %s\n", dr.ReadString(0x200).value_or("<null>").c_str());

    auto arr = dr.ReadArray<uint64_t>(0x300, 5);
    for (const auto &a : arr) printf("  0x%llX\n", (unsigned long long)a);

    printf("越界读 = %d\n", dr.Read<int32_t>(0x9999));   // 应输出 0
    return 0;
}
```

编译：`g++ -std=c++17 -Wall fake.cpp -o fake.exe`

这个 `FakeDriver` 就是第 75 章真实跨进程读写类的雏形——先把逻辑跑通，再换成真驱动。

## 验收清单

- [ ] 理解 `template <typename T>` 是什么，能写一个 `max_value`
- [ ] 能解释 `dr->Read<float>(addr)` 背后发生了什么
- [ ] 会用 `vector`（含 `reserve`）、`unordered_map`、`optional`、`string_view`
- [ ] 知道 `string_view` 不拥有数据，不能返回局部变量的 view
- [ ] 跑通了 FakeDriver，包括越界返回 0 的情况

→ 下一章：[[第14章-智能指针与生命周期]]　—— 不再手写 `new`/`delete`，能用 `unique_ptr`/`shared_ptr` 表达清楚"谁拥有这块内存"。
