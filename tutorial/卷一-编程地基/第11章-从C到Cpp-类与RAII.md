---
tags: [教程, 卷一, C++, 面向对象]
day: 6
aliases: [ch11]
---

# 第 11 章 · 从 C 到 C++：类与 RAII

> [!abstract] 本章目标
> 理解"类 = 结构体 + 函数"，以及 RAII 这个 C++ 最重要的思想。
> 本项目 90% 的代码是 C++，这一章是分水岭。

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
MemDriver(const MemDriver &) = delete;
MemDriver &operator=(const MemDriver &) = delete;
```

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

## 动手：写一个 RAII 文件类

```cpp
#include <cstdio>
#include <cstdint>
#include <vector>

class FileReader {
public:
    explicit FileReader(const char *path) {
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

## 验收清单

- [ ] 能把 C 的"结构体+函数"改写成一个 C++ 类
- [ ] 能解释 RAII，并举出本项目的一个例子（`std::scoped_lock`）
- [ ] 知道构造/析构的调用时机
- [ ] 理解纯虚函数与抽象类的作用（`AndroidImgui`）
- [ ] 知道为什么本项目给 `MemDriver` 加了 `= delete`
- [ ] 完成了 RAII 文件类，并确认没有手动 fclose

→ 下一章：[[第12章-引用-const-auto]]　—— 掌握现代 C++ 的三个书写习惯：用引用代替指针传参、到处加 `const`、用 `auto` 减少重复。
