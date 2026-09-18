---
tags: [教程, 卷一, C++, 面向对象]
day: 7
aliases: [ch11]
---

# 第 11 章 · 从 C 到 C++：类与 RAII

> [!abstract] 本章目标
> 理解"类 = 结构体 + 函数"，以及 RAII 这个 C++ 最重要的思想。
> 本项目 90% 的代码是 C++，这一章是分水岭。

> [!note] 承上
> 前 10 章你学的都是 C 语言（结构体、指针、函数）。
> 本章跨过分水岭进入 C++——**类 = 结构体 + 函数**，以及 RAII 这个 C++ 最重要的思想。

## 先看一个真实场景：文件句柄忘了关

> [!question] 你遇到过这个吗？
> 打开文件后遇到 `return`，忘了 `fclose`——文件句柄泄漏，跑久了系统资源耗尽。
> **根因**：手动管理资源，任何一条提前返回路径都可能漏掉释放。
> 本章的 RAII 用"对象生命周期"自动管资源——**绝不忘释放**。

## 从 C 到 C++：先搞清关系

到这里为止，你学的都是 **C 语言**。从现在开始，你接触的代码会变成 **C++**。

**先打消一个误会**：C++ **不是**一门和 C 完全无关的新语言，而是"**在 C 基础上扩展**"。

| 关系 | 说明 |
|---|---|
| **C 是 C++ 的子集** | 几乎所有合法的 C 代码，都是合法的 C++ 代码（个别地方例外） |
| **C++ 加了很多东西** | 类、引用、模板、命名空间、异常、STL（标准模板库，现成的容器/算法）…… |
| **本项目用 C++20** | `Android.mk` 里写的是 `-std=c++20`（第 34 章会看到） |

**所以你不会"忘掉 C 重新学"**——前面学的变量、数组、指针、结构体、函数，在 C++ 里**一模一样地继续用**。这一章开始讲的是 C++ **多出来的**那部分。

### C++ 源文件的扩展名

| 扩展名 | 语言 | 用什么编译器 |
|---|---|---|
| `.c` | C | `gcc` |
| `.cpp` / `.cc` / `.cxx` | C++ | `g++` |
| `.h` | 头文件（C 或 C++ 都可能） | 看被谁包含 |

**本项目几乎所有源文件都是 `.cpp`**（`.h` 也是 C++ 风格），只有少量遗留 C 代码。

### 用 `g++` 编译 C++

```bash
g++ -std=c++20 -Wall -Wextra hello.cpp -o hello.exe
./hello.exe
```

**`-std=c++20` 指定 C++ 标准版本**（本项目用 C++20，卷一的练习用 `c++17` 或 `c++20` 都行）。

```cpp
// hello.cpp
#include <iostream>   // C++ 的输入输出头文件

int main() {
    std::cout << "Hello, C++\n";
    return 0;
}
```

> [!note] `std::cout` 是 C++ 的输出方式
> C 里用 `printf`，C++ 里可以用 `std::cout`（读作"C-out"）：
> ```cpp
> std::cout << "内容" << 变量 << "\n";   // << 表示"往输出流里塞东西"
> ```
> 但**本项目大量使用 `printf`**——因为 C 风格的 `printf` 格式化更灵活（`%.2f` 之类），而且 NDK 环境下更好控制。
> **两种都能用，看项目习惯。** 本教程后面也主要用 `printf`，偶尔用 `std::cout`。

## C++ 第一个新东西：`std` 命名空间

C++ 标准库的东西都放在一个叫 `std` 的**命名空间**里。所以标准库的东西都要写 `std::` 前缀：

```cpp
std::cout         // 标准输出
std::string       // 标准字符串
std::vector       // 标准动态数组
std::printf       // （也有，但一般直接用 C 的 printf）
```

**什么是命名空间**？简单说，就是"给一堆名字套一层前缀，避免重名"。

```cpp
namespace GameTools {
    void WorldToScreen();
}

namespace MyUtils {
    void WorldToScreen();   // 和上面同名，但不冲突
}

GameTools::WorldToScreen();   // 用的时候加上前缀
MyUtils::WorldToScreen();
```

**这就是为什么你看到 `std::vector`、`std::string` 这种写法**——它们是 `std` 这个命名空间里的名字。

> [!warning] 不要在头文件里 `using namespace std;`
> 你可以写 `using namespace std;` 让 `std::` 前缀省掉：
> ```cpp
> using namespace std;
> cout << "hi\n";        // 省掉 std::
> ```
> **但千万别写在头文件里**——它会让所有包含这个头文件的文件都"被污染"，可能引发名字冲突。
> 本项目的 `PhysX.h` 里有一句 `using namespace std;`，是历史遗留，属于应该改掉的写法。

## 从"结构体"到"类"：把函数也装进去

你在第 07 章学过结构体：**把几个变量打包成一个**。

C++ 的类（`class`）**在结构体基础上更进一步**：**不仅能把变量打包，还能把操作这些变量的函数也打包进去。**

看一个 C 的写法（你已经会的）：

```c
// C 写法：结构体 + 一组操作它的函数
struct Player {
    float x, y, z;
    int   hp;
};

void damage(struct Player *p, int dmg) {
    p->hp -= dmg;
}

int alive(const struct Player *p) {
    return p->hp > 0;
}

// 用：
struct Player pl = {0, 0, 0, 100};
damage(&pl, 30);
if (alive(&pl)) { /* ... */ }
```

**这里的问题**：`damage`、`alive` 这些函数明明是为 `Player` 服务的，却散落在外面，和 `Player` 没有任何语法上的绑定。

C++ 的做法是**把它们写进 `Player` 里面**：

```cpp
// C++ 写法：类
struct Player {
    float x = 0, y = 0, z = 0;
    int   hp = 100;

    void damage(int dmg) { hp -= dmg; }        // 成员函数
    bool alive() const   { return hp > 0; }    // 成员函数
};

// 用：
Player pl;
pl.damage(30);
if (pl.alive()) { /* ... */ }
```

**看两个关键变化**：

1. **函数写进了 `struct` 里面**——它们叫**成员函数**（member function）
2. **调用时用 `对象.函数名(...)`**，不用再传 `p`——函数里直接就能访问 `hp`（因为函数"属于"这个对象）

**这就是"面向对象"的核心思想**：**数据（字段）和操作数据的代码（成员函数）捆在一起。**

### 成员函数里访问字段，不需要前缀

```cpp
struct Player {
    int hp = 100;

    void damage(int dmg) {
        hp -= dmg;         // 直接写 hp，不用 p->hp
    }
};
```

**为什么能直接写 `hp`**？因为成员函数"属于"某个对象，函数体里所有裸字段名默认指的是**"当前对象的那个字段"**。

**这等价于 C 的**：`p->hp -= dmg;`，只不过 `p` 被隐式地提供了。

### `this`：指向当前对象的指针

成员函数里有个隐含的指针 `this`，指向"调用这个函数的对象"：

```cpp
struct Player {
    int hp = 100;

    void damage(int dmg) {
        this->hp -= dmg;    // 和直接写 hp 完全等价
    }
};
```

**大部分时候你不需要写 `this->`**。有两种情况必须写：

1. **参数和字段重名**时：
   ```cpp
   void set_hp(int hp) {
       this->hp = hp;     // this->hp 是字段，hp 是参数
   }
   ```
2. **返回自身**时（链式调用）：
   ```cpp
   Player& setX(float v) { x = v; return *this; }
   ```

**本项目的 `driver.h` 里的自旋锁就用了 `this`。** 现在只要知道有这个东西就行，不常用。


## 先看同一个东西的两种写法

**C 写法：**

```c
struct Player {
    float x, y, z;
    int   hp;
};

void player_damage(struct Player *p, int dmg) { p->hp -= dmg; }
int  player_alive(const struct Player *p)     { return p->hp > 0; }

struct Player pl = {0, 0, 0, 100};
player_damage(&pl, 30);
```

**C++ 写法：**

```cpp
struct Player {
    float x = 0, y = 0, z = 0;
    int   hp = 100;

    void damage(int dmg) { hp -= dmg; }
    bool alive() const   { return hp > 0; }
};

Player pl;
pl.damage(30);
```

差别不在语法糖，在于：**数据和操作它的函数写在了一起**，不用每个函数都传 `p`。

## struct 与 class 的唯一区别

```cpp
struct A { int x; };        // 默认 public
class  B { int x; };        // 默认 private
```

就这样。没有别的区别。本项目偏爱 `struct`（`Config`、`mBase`、`BoneTransform`），
因为这些是"数据容器"，字段本来就该公开。

## 构造函数与析构函数

```cpp
class Buffer {
public:
    Buffer(size_t n) : size_(n), data_(new uint8_t[n]) {   // 构造
        // 初始化列表：比在函数体里赋值更高效
    }
    ~Buffer() {                                            // 析构
        delete[] data_;
    }
private:
    size_t  size_;
    uint8_t *data_;
};
```

调用时机：

```cpp
{
    Buffer b(1024);      // 构造被调用
    /* 用 b */
}                        // 离开作用域，析构自动被调用 → data_ 被释放
```

> [!note] 上面代码里的两个新语法
> **① `new` / `delete`**：在堆上分配/释放内存。
> ```cpp
> uint8_t *p = new uint8_t[100];   // 分配 100 字节
> delete[] p;                       // 释放（数组用 delete[]）
> ```
> 它和 `malloc`/`free`（第 04 章讲过）类似，但更安全（会调用构造函数）。**第 14 章会详解，这里先混个眼熟。**
>
> **② 初始化列表 `: size_(n), data_(new uint8_t[n])`**：写在构造函数参数列表**后面**、函数体**前面**，用冒号开头。
> 它是"**在对象创建时直接初始化成员变量**"的语法，比在函数体里赋值更高效：
> ```cpp
> Buffer(size_t n) : size_(n), data_(new uint8_t[n]) { }
> //      ↑冒号    ↑size_ 初始化为 n   ↑data_ 初始化为新数组
> ```
> **本项目大量使用初始化列表**（`driver.h`、`PhysX.h` 里到处是）。看到 `构造函数(...) : 成员(值), 成员(值) { }` 就是这个语法。

**这是 C++ 的核心：析构函数在离开作用域时自动执行，你不可能忘记释放。**

## RAII：用对象的生命周期管理资源

RAII = Resource Acquisition Is Initialization（资源获取即初始化）。
说人话：**把资源塞进对象，靠析构函数自动回收。**

对比：

```c
// C：忘了 free 就泄漏
void read_c(void) {
    uint8_t *buf = malloc(4096);
    if (some_error) return;          // ← 这里漏了 free！
    process(buf);
    free(buf);
}
```

```cpp
// C++：无论怎么退出都会释放
void read_cpp(void) {
    std::vector<uint8_t> buf(4096);
    if (some_error) return;          // ← 析构自动清理
    process(buf.data());
}
```

即使中间抛异常，C++ 也会保证析构被调用。这就是 RAII 的价值。

本项目的 RAII 用例：

```cpp
// driver.h
void HandleVirtualMemoryRWEvent(...) {
    std::scoped_lock<SpinLock> lock(m_mutex);   // 加锁
    /* ... */
}                                                // 离开作用域自动解锁
```

`std::scoped_lock` 就是 RAII 锁——**不可能忘记解锁，即使中间 return 或抛异常。**

## 本项目里的一个类：`AndroidImgui`

```cpp
class AndroidImgui {
protected:
    ANativeWindow *m_Window;
    float m_Width, m_Height;
public:
    char RenderName[16];

    bool  Init_Render(ANativeWindow *window, float width, float height);
    void  NewFrame(bool resize = false);
    void  EndFrame();
    void  Shutdown();

private:
    virtual bool  Create() = 0;          // 纯虚函数
    virtual void  Setup() = 0;
    virtual void  Render(ImDrawData*) = 0;
};
```

几个关键点：

| 语法 | 含义 |
|---|---|
| `protected:` | 子类能访问，外部不能 |
| `public:` | 所有人能访问 |
| `private:` | 只有自己能访问 |
| `virtual ... = 0` | **纯虚函数**——子类必须实现 |
| 有纯虚函数的类 | **抽象类**，不能实例化 |

所以 `AndroidImgui` 是一个"接口"：规定了所有渲染后端必须提供哪些能力，
具体实现交给 `VulkanGraphics`（第 58 章）。

### 坏味道重构推演：从"每个后端各写一遍"到模板方法

> [!tip] `AndroidImgui` 为什么把 `Create`/`Render` 设成私有纯虚？
> 这是**模板方法模式**：基类定好「骨架流程」，子类只填「变化的那几步」。
> 先看没有它会写成什么样。

❌ **稚嫩写法：每个后端自己写完整初始化+渲染流程**

```cpp
// ❌ 坏味道：Vulkan 后端自己写一整套流程
class VulkanRenderer {
public:
    void Run(ANativeWindow* w, float wd, float ht) {
        ANativeWindow_acquire(w);            // ← 流程步骤 1
        createVulkanInstance();              // ← Vulkan 专属
        setupVulkanDevice();                 // ← Vulkan 专属
        ImGui::CreateContext();              // ← 流程步骤 3
        ImGui::GetIO().DisplaySize = {wd, ht};
        while (running) { /* 渲染循环 */ }
        ImGui::DestroyContext();
        ANativeWindow_release(w);
    }
};

// 若再加 OpenGL 后端，上面「acquire / CreateContext / DisplaySize / 循环 / 清理」
// 又要原样抄一遍——只有中间几步不同，其余全是重复。
```

| 坏味道 | 后果 |
|---|---|
| 骨架流程重复 | 每个后端抄一遍 acquire/CreateContext/清理 |
| 步骤顺序易错 | 漏了 acquire 或 release 就泄漏 |
| 无统一入口 | 调用方要记住每个后端的启动函数名 |

✅ **优雅写法：模板方法（本项目真实架构）**

```cpp
// ✅ AndroidImgui.h：基类定骨架（非虚的公开方法）+ 子类填步骤（私有纯虚）
class AndroidImgui {
public:
    bool Init_Render(ANativeWindow *window, float width, float height);  // 骨架
    void NewFrame(bool resize = false);
    void EndFrame();
    void Shutdown();
private:
    virtual bool Create() = 0;              // 子类填：后端初始化
    virtual void Setup() = 0;               // 子类填：后端配置
    virtual void Render(ImDrawData*) = 0;   // 子类填：后端绘制
};
```

基类的 `Init_Render` 把「不变的部分」固化（真实源码 `AndroidImgui.cpp`）：

```cpp
// ✅ 骨架流程固化在基类：acquire → Create → CreateContext → Setup
bool AndroidImgui::Init_Render(ANativeWindow *window, float width, float height) {
    m_Window = window;
    m_Width = width;  m_Height = height;
    ANativeWindow_acquire(window);          // ① 不变
    Create();                               // ② 变化 → 子类实现
    ImGui::CreateContext();                 // ③ 不变
    ImGui::GetIO().DisplaySize = {width, height};
    Setup();                                // ④ 变化 → 子类实现
    return true;
}
```

子类只实现那几个纯虚步骤：

```cpp
// ✅ VulkanGraphics 只填「变化的那几步」
class VulkanGraphics : public AndroidImgui {
    bool Create() override { /* Vulkan 初始化 */ return true; }
    void Setup()  override { /* Vulkan 配置 */ }
    void Render(ImDrawData *d) override { /* Vulkan 绘制 */ }
};
```

### 核心指标对比

| 维度 | ❌ 各写一遍 | ✅ 模板方法 |
|---|---|---|
| **可维护性** | 改骨架要改 N 个后端 | 改骨架只改基类 1 处 |
| **可扩展性** | 加后端抄一大段 | 加后端只填 3 个纯虚函数 |
| **正确性** | 顺序易错、易泄漏 | 骨架唯一，不会漏步骤 |
| **调用一致性** | 每个后端入口名不同 | 统一 `Init_Render`/`EndFrame` |

### 演进动因剖析

> [!note] 为什么本项目用模板方法？
> 1. **流程骨架是固定的**：不管什么后端，都要「获取窗口 → 创建上下文 → 配置 → 渲染循环 → 清理」。
> 2. **差异点是局部的**：只有 `Create`/`Setup`/`Render` 等几步随后端变。
> 3. **防错**：把 `ANativeWindow_acquire`/`release` 这类易漏的步骤**锁进基类**，
>    子类想漏都漏不掉——这正是 RAII 思想在「流程」上的延伸。
>
> **一句话**：当「流程骨架相同、个别步骤不同」时，用模板方法——
> **基类控制流程，子类实现变化**。本项目 `AndroidImgui` 就是这样统一了渲染后端。

## 继承与多态

```cpp
class VulkanGraphics : public AndroidImgui {
    bool Create() override { /* Vulkan 初始化 */ }
    void Render(ImDrawData *d) override { /* Vulkan 绘制 */ }
};
```

`override` 关键字：告诉编译器"我要覆盖父类的虚函数"。
写错了（比如参数类型不同）编译器会报错——不加 `override` 则会静默地变成新函数。

**多态的用处**：调用方只认父类，不关心具体实现。

```cpp
std::unique_ptr<AndroidImgui> graphics = GraphicsManager::getGraphicsInterface();
graphics->NewFrame(true);      // 实际调用 VulkanGraphics::NewFrame
graphics->EndFrame();
```

如果哪天要加 OpenGL 后端，只要新写一个子类，调用方一行都不用改。

这正是本项目 `IDriver` 抽象层的设计思路（第 80 章）：

```
IDriver (抽象接口)
├── Driver   → 内核驱动实现
└── MemDriver → 包装 SysHal 的系统调用实现
```

业务代码只写 `dr->Read(...)`，不关心背后是谁。

## 构造/析构顺序

```cpp
class Child : public Parent {
    Member m;
};
```

构造顺序：`Parent` → `m` → `Child 自己的函数体`
析构顺序：完全相反

**成员按声明顺序初始化，不是按初始化列表的顺序。**

```cpp
class Bad {
    int a;
    int b;
    Bad() : b(1), a(b) {}    // 危险！a 先被初始化，此时 b 还是垃圾值
};
```

开 `-Wall` 编译器会警告 `-Wreorder`。

## 拷贝控制：Rule of Three/Five/Zero

如果类里有裸指针，编译器生成的默认拷贝是"浅拷贝"——两个对象指向同一块内存，
析构时double free。**这是 C++ 最经典的崩溃之一。**

```cpp
class Bad {
    uint8_t *data_;
public:
    Bad(size_t n) : data_(new uint8_t[n]) {}
    ~Bad() { delete[] data_; }
    // 没写拷贝构造 → 编译器生成一个浅拷贝版 → 灾难
};

Bad a(100);
Bad b = a;      // b.data_ 和 a.data_ 指向同一块内存
// 析构时释放两次 → 崩溃
```

三条路：

| 做法 | 写法 | 适用 |
|---|---|---|
| **Rule of Zero**（推荐） | 用 `vector`/`unique_ptr` 管理，不手写析构 | 99% 的情况 |
| 禁止拷贝 | `Bad(const Bad&) = delete;` | 资源不可共享 |
| 手写拷贝 | 深拷贝 | 少用 |

本项目用的是第三条路：

```cpp
MemDriver(const MemDriver &) = delete;              // 禁止拷贝构造
MemDriver &operator=(const MemDriver &) = delete;   // 禁止拷贝赋值
```

> [!note] 两行分别禁止了什么
> - 第一行禁止"用另一个对象来创建新对象"：`MemDriver b = a;`（拷贝构造）
> - 第二行禁止"把一个对象赋给另一个"：`b = a;`（拷贝赋值，`operator=` 是"赋值运算符"）
> **两个都禁掉，才是真的"不可拷贝"。** 这是 C++ 里表达"这个类不许复制"的标准写法。

因为驱动对象持有内核连接，拷贝它没有意义。**明确禁止比留着隐患好。**

## 命名空间

```cpp
namespace GameTools {
    Vec2 ScreenSize;
    void WorldToScreen(Vec2 *bscreen, Vec3 *obj);
}

// 使用
GameTools::WorldToScreen(&screen, pos);
```

避免命名冲突。本项目用了多个命名空间：`GameTools`、`SilentAim`、`PovAim`、
`TouchAim`（自瞄三套）、`Touch`（触摸）、`android::ANativeWindowCreator`、`ConfigManager`。

> [!tip] 不要 `using namespace std;` 在头文件里
> 它会污染所有包含这个头文件的文件。本项目 `PhysX.h` 里有 `using namespace std;`
> 是历史遗留，属于应该改掉的写法。

## 补充：文件读写基础（C 的 `FILE*`）

下面那个 RAII 练习会操作文件。**文件 I/O 你还没学过，先补一补**。

### 打开文件：`fopen`

```c
#include <stdio.h>

FILE *fp = fopen("data.bin", "rb");   // 打开文件
if (fp == NULL) {
    // 打开失败（文件不存在、没权限等）
    return;
}
```

| 参数 | 含义 |
|---|---|
| 第一个 | 文件路径 |
| 第二个 | 模式字符串 |

**常用模式**：

| 模式 | 含义 |
|---|---|
| `"r"` | 只读（文件必须存在） |
| `"w"` | 只写（文件不存在就创建，存在就清空） |
| `"a"` | 追加（在末尾写） |
| `"rb"` / `"wb"` | 二进制模式（本项目读二进制数据都用这个） |

**`fopen` 返回 `FILE *`**（一个"文件句柄"），失败返回 `NULL`——**必须判空**。

### 读写：`fread` / `fwrite`

```c
uint8_t buf[100];
size_t n = fread(buf, 1, 100, fp);    // 从 fp 读最多 100 字节到 buf，返回实际读到的字节数

uint8_t data[16] = {0};
fwrite(data, 1, 16, fp);              // 把 data 的 16 字节写到 fp
```

| `fread` 参数 | 含义 |
|---|---|
| 1 | 每个元素几个字节 |
| 2 | 读几个元素 |
| 3 | 目标缓冲区 |
| 4 | 文件句柄 |

**返回值是"实际读到的元素个数"**。读到文件末尾时返回小于请求的数量。

### 定位：`fseek` / `ftell`

```c
fseek(fp, 0, SEEK_END);      // 把读位置移到文件末尾
long size = ftell(fp);       // 问"当前位置是第几字节"→ 就是文件大小
fseek(fp, 0, SEEK_SET);      // 移回文件开头
```

| `fseek` 第三个参数 | 含义 |
|---|---|
| `SEEK_SET` | 从文件开头算 |
| `SEEK_CUR` | 从当前位置算 |
| `SEEK_END` | 从文件末尾算 |

**"打开 → 移到末尾 → 问大小 → 移回开头"是获取文件大小的标准套路**（下面练习会用到）。

### 关闭文件：`fclose`

```c
fclose(fp);      // 用完必须关（不关会泄漏文件句柄，且缓冲数据可能没落盘）
fp = NULL;       // 好习惯：关掉后置空
```

**`fopen` 必须配 `fclose`**——和上面讲的 `new`/`delete` 一样是"手动配对"（本章已讲过 `new`/`delete`）。
**这正是下面那个 RAII 练习要解决的问题**：把 `fclose` 放进析构函数，就不用记得手动关了。

### 完整例子

```c
#include <stdio.h>
#include <stdint.h>

int main(void) {
    FILE *fp = fopen("data.bin", "rb");
    if (fp == NULL) { printf("打不开文件\n"); return 1; }

    // 求文件大小
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    printf("文件大小: %ld 字节\n", size);

    // 读前 16 字节
    uint8_t buf[16];
    size_t n = fread(buf, 1, 16, fp);
    printf("读到 %zu 字节\n", n);

    fclose(fp);
    return 0;
}
```

> [!note] 这些函数属于 C 标准库（`<stdio.h>`），C++ 里是 `<cstdio>`
> 本项目读文件、读 `/proc` 都大量用到它们。**先认识这几个，后面的章节会反复见到。**

## 动手：写一个 RAII 文件类

```cpp
#include <cstdio>
#include <cstdint>
#include <vector>

class FileReader {
public:
    explicit FileReader(const char *path) {   // explicit：禁止"隐式转换"式构造，见下方说明
        fp_ = fopen(path, "rb");
        if (!fp_) { valid_ = false; return; }
        fseek(fp_, 0, SEEK_END);
        size_ = ftell(fp_);
        fseek(fp_, 0, SEEK_SET);
        valid_ = true;
    }
    ~FileReader() { if (fp_) fclose(fp_); }

    FileReader(const FileReader&) = delete;              // 禁止拷贝
    FileReader& operator=(const FileReader&) = delete;

    bool valid() const { return valid_; }
    size_t size() const { return size_; }

    bool read_all(std::vector<uint8_t> &out) {
        if (!valid_) return false;
        out.resize(size_);
        return fread(out.data(), 1, size_, fp_) == size_;
    }

private:
    FILE *fp_ = nullptr;
    size_t size_ = 0;
    bool valid_ = false;
};

int main(void) {
    FileReader f("test.bin");
    if (!f.valid()) { printf("打不开\n"); return 1; }
    std::vector<uint8_t> data;
    f.read_all(data);
    printf("读到了 %zu 字节\n", data.size());
    return 0;                       // f 析构，自动 fclose
}
```

用 `gcc -std=c++17 -Wall file.cpp -o file.exe` 编译。
注意：`main` 里没有 `fclose`，但文件一定被关闭了。

> [!note] `explicit` 是什么意思
> 加了 `explicit` 的构造函数**不允许"隐式转换"**——必须显式写出类型名才能构造。
> ```cpp
> FileReader a("data.bin");      // 对：显式构造
> FileReader b = "data.bin";     // 错：explicit 禁止这种隐式转换
> ```
> 好处：**防止"不小心把参数类型转成了这个类"**，减少误用。
> 只接受一个参数的构造函数，**本项目一律加 `explicit`**。

> [!note] 关于"异常"
> 本页多次提到"抛异常"（比如"即使中间抛异常，析构也会被调用"）。
> **异常是 C++ 的错误处理机制**：出错时用 `throw` 抛出一个错误，会被 `try/catch` 捕获。
> **卷一只需要知道"异常会导致函数提前退出"这一点**——这正是 RAII 有价值的原因（RAII 保证即使提前退出，资源也会被释放）。
> 异常的完整用法在卷二/卷五会用到时再讲。

## 课后习题

### 习题 11.1 用 RAII 管理动态数组（★★）

**任务要求**：写一个类 `IntBuffer`，构造时按大小 `new[]`，析构时 `delete[]`，
提供 `set`/`get`。验证离开作用域自动释放（用构造/析构打印观察）。

**参考实现**：

```cpp
#include <cstdio>
#include <cassert>

class IntBuffer {
public:
    explicit IntBuffer(size_t n) : n_(n), data_(new int[n]) {
        printf("构造：分配 %zu 个 int\n", n_);
    }
    ~IntBuffer() {
        delete[] data_;                 // ★ 析构自动释放，不可能忘
        printf("析构：已释放\n");
    }
    // 禁止拷贝（避免 double free）
    IntBuffer(const IntBuffer&) = delete;
    IntBuffer& operator=(const IntBuffer&) = delete;

    void set(size_t i, int v) { if (i < n_) data_[i] = v; }
    int  get(size_t i) const  { return (i < n_) ? data_[i] : 0; }
    size_t size() const { return n_; }

private:
    size_t n_;
    int*   data_;
};

int main() {
    printf("--- 进入作用域 ---\n");
    {
        IntBuffer buf(4);
        for (size_t i = 0; i < buf.size(); i++) buf.set(i, (int)i * 10);
        assert(buf.get(0) == 0 && buf.get(3) == 30);
    }
    printf("--- 已离开 ---\n");
    printf("习题 11.1 通过\n");
    return 0;
}
```

**预期输出**：构造打印 → 使用 → 离开 `{}` 时析构打印（不用手写 `delete[]`）。

> [!note] 为什么要 `= delete` 拷贝
> 若允许拷贝，两个 `IntBuffer` 会指向**同一块堆内存**，析构时 `delete[]` 两次 → 崩溃。
> 本项目 `MemDriver(const MemDriver&) = delete;` 就是同样的道理（驱动不可拷贝）。

### 习题 11.2 纯虚函数与抽象类（★★）

**任务要求**：定义抽象基类 `Shape`（纯虚 `area()`），派生 `Circle` 和 `Rect`，
用基类指针调用，验证多态。

**参考实现**：

```cpp
#include <cstdio>
#include <cassert>
#include <cmath>

class Shape {
public:
    virtual ~Shape() = default;
    virtual double area() const = 0;   // ★ 纯虚函数 → Shape 是抽象类
};

class Circle : public Shape {
    double r_;
public:
    explicit Circle(double r) : r_(r) {}
    double area() const override { return 3.14159265 * r_ * r_; }
};

class Rect : public Shape {
    double w_, h_;
public:
    Rect(double w, double h) : w_(w), h_(h) {}
    double area() const override { return w_ * h_; }
};

int main() {
    Circle c(1.0);
    Rect   r(2.0, 3.0);
    Shape* shapes[2] = { &c, &r };
    for (Shape* s : shapes) printf("面积 = %.2f\n", s->area());  // 多态分发
    assert(std::fabs(shapes[0]->area() - 3.14159265) < 0.01);
    assert(std::fabs(shapes[1]->area() - 6.0) < 0.01);
    printf("习题 11.2 通过\n");
    return 0;
}
```

> [!tip] 抽象类不能实例化
> `Shape s;` 会编译报错——有纯虚函数的类是抽象类。
> 本项目 `AndroidImgui` 就是这种"接口"，具体实现交给 `VulkanGraphics`（第 58 章）。

## 本章小结

> [!abstract] 一句话回顾
> **类=结构体+函数；RAII=用对象生命周期自动管理资源**

## 验收清单

- [ ] 能把 C 的"结构体+函数"改写成一个 C++ 类（把 C 版 `player_damage(&p, 30)` 改成 `p.damage(30)`）
- [ ] 能解释 RAII，并举出本项目的一个例子（`std::scoped_lock`）（说出"构造加锁、析构解锁，不可能忘"）
- [ ] 知道构造/析构的调用时机（在构造/析构里打印，观察进入/离开作用域的输出顺序）
- [ ] 理解纯虚函数与抽象类的作用（`AndroidImgui`）（说出"有纯虚函数的类不能实例化"）
- [ ] 知道为什么本项目给 `MemDriver` 加了 `= delete`（说出"驱动持有内核连接，拷贝没意义"）
- [ ] 完成了 RAII 文件类，并确认没有手动 fclose（运行程序，观察析构自动调用、文件被关）

→ 下一章：[[第12章-引用-const-auto]]　—— 掌握现代 C++ 的三个书写习惯：用引用代替指针传参、到处加 `const`、用 `auto` 减少重复。
