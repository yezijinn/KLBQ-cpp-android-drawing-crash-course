---
tags: [教程, 卷三, Android, STL]
day: 25
aliases: [ch40]
---

# 第 40 章 · c++_static 与 STL 选择

> [!abstract] 本章目标
> 理解静态/动态 STL 的取舍，知道本项目为什么选 `c++_static`，
> 以及它带来的体积代价。

## 先看差别

```bash
# c++_static
-rwxr-xr-x 1 shell shell  8.5M  app          # 单文件，推上去就能跑

# c++_shared
-rwxr-xr-x 1 shell shell  2.1M  app          # 小
-rwxr-xr-x 1 shell shell  1.2M  libc++_shared.so   # 还要带这个
```

## 什么是 STL

STL = C++ 标准库的实现，包含：

| 部分 | 例子 |
|---|---|
| 容器 | `vector` `map` `unordered_map` `string` |
| 算法 | `sort` `find` `clamp` |
| 智能指针 | `unique_ptr` `shared_ptr` |
| 工具 | `optional` `string_view` `span`（C++17/20） |
| 运行时 | 异常处理、RTTI、new/delete |

> [!note] RTTI 是什么
> **RTTI = Run-Time Type Information**（运行时类型信息）：让程序在**运行时**还能知道"这个对象到底是什么类型"。
> 它是 `dynamic_cast`（安全向下转型）和 `typeid` 的基础。关掉 RTTI（`-fno-rtti`）能省体积，但就不能用 `dynamic_cast` 了。

Android NDK 提供的是 **libc++**（LLVM 的实现）。

命名空间是 `std::__ndk1`（内部），
这样即使系统里有另一份 STL 也不会冲突。**`__ndk1` 是 NDK 给 libc++ 加的后缀**，防止和系统自带的 `std` 冲突——你写代码时仍写 `std::vector`，不用管它。

## 两种链接方式

| | `c++_static` | `c++_shared` |
|---|---|---|
| 库代码 | 编进你的产物 | 单独的 `.so` |
| 产物大小 | 大（+1~2MB） | 小 |
| 部署 | 单文件 | **必须带 `libc++_shared.so`** |
| 多模块共享 | 各有一份拷贝 | 共享一份 |
| 异常跨库传播 | 可能有问题 | 正常 |

## 静态链接的隐藏问题：异常跨库边界

如果：
- 主程序用 `c++_static`
- 加载的 `.so` 也用 `c++_static`

那么**两份 STL 各有自己的异常状态**。
在 `.so` 里抛的异常，主程序可能 catch 不到。

```
主程序 (STL副本A)          .so (STL副本B)
    │                          │
    │  call →                  │ throw MyError
    │  ← 异常？A 不认识 B 的类型 │
    ✗ std::terminate
```

本项目只有**一个可执行文件，没有自己的 .so**，所以没这个问题。

> [!warning] 什么时候该用 c++_shared
> - 你的产物包含多个 `.so`
> - 不同 `.so` 之间需要传递 C++ 对象或异常
> - 体积敏感且能接受额外部署一个文件
>
> 本项目不满足以上任何一条，所以 `c++_static` 是对的。

## 体积代价有多大

实测（典型数字，随 NDK 版本变化）：

| 配置 | 空 main() 的产物大小 |
|---|---|
| C 语言，不用 STL | ~10 KB |
| `c++_static` + 用 `vector/string` | +800 KB ~ 1.5 MB |
| `c++_static` + 异常 + RTTI | +1.5 ~ 2 MB |
| 再静态链接 Embree | +3 ~ 8 MB |

本项目的产物包含 ImGui + Embree + 字体数组，
最终在 **8~20 MB** 量级。

> [!tip] 用了什么才会编进来什么
> 静态链接也是"按需抽取"的（第 28 章）。
> 不用 `std::regex`、`std::locale` 这些大块头，就不会被链接进来。
> `std::regex` 一个就能加 500KB。

## 减少 STL 体积的技巧

**1. 避免 `std::regex`**（改用简单字符串查找）

```cpp
// 贵（+500KB）
std::regex re("pattern");

// 便宜
if (str.find("pattern") != std::string::npos) { ... }
```

**2. 避免 `std::locale` / `std::iostream`**

```cpp
// 贵
#include <sstream>
std::stringstream ss; ss << value;

// 便宜
char buf[32]; snprintf(buf, sizeof(buf), "%d", value);
```

本项目用的是 `snprintf` 和 `std::to_string`，没有 `stringstream`。

**3. 用 `string_view` 代替 `string` 参数**

不分配内存，也不需要构造临时对象（第 13 章）。

**4. 关掉异常和 RTTI（可选）**

```makefile
LOCAL_CPPFLAGS += -fno-exceptions -fno-rtti
```

能省 10~20%，但代价：
- 不能用 `try/catch`
- 不能用 `dynamic_cast`
- 部分 STL 容器行为改变（会直接 abort 而不是抛异常）

**本项目保留了异常和 RTTI**（`-fexceptions -frtti`），
因为代码里用了 `std::optional`、`std::vector` 等依赖异常语义的组件，
且体积不是首要矛盾。

## 检查产物里有什么

```bash
# 看哪些符号占地方
aarch64-linux-android-nm --size-sort -S -C app | tail -30

# 看 STL 相关的节
aarch64-linux-android-readelf -S app | grep -i 'text\|rodata'

# 按函数统计大小（需要带符号的版本）
aarch64-linux-android-nm --print-size --size-sort app | grep -i 'std::' | tail
```

## 动态 STL 时怎么部署

如果选了 `c++_shared`：

```bash
# 找到库
cp $NDK/toolchains/llvm/prebuilt/windows-x86_64/sysroot/usr/lib/aarch64-linux-android/libc++_shared.so ./

adb push libc++_shared.so /data/local/tmp/
adb shell chmod 755 /data/local/tmp/libc++_shared.so

# 运行时指定搜索路径
adb shell "cd /data/local/tmp && LD_LIBRARY_PATH=/data/local/tmp ./app"
```

忘了推这个库，运行时报：

```
CANNOT LINK EXECUTABLE: cannot locate symbol "..." 
  referenced by "/data/local/tmp/app"...
  (library "libc++_shared.so" not found)
```

## 与 Embree 的兼容性

Embree 的 `.a` 是用什么 STL 编的？**可能根本没用 C++ STL**——
它是 C API（`rtcNewDevice` 等），内部可能用 C++ 但符号被隐藏了。

判断方法：

```bash
nm -C libembree4.a | grep -c 'std::'
```

如果数量很少或没有，说明基本不依赖 STL，很安全。
如果有大量 `std::__ndk1::` 符号，就要确认版本匹配。

## 动手：对比两种 STL 的产物

```bash
# 1. 静态版
ndk-build clean && ndk-build APP_STL=c++_static
cp libs/arm64-v8a/app app_static
ls -l app_static
aarch64-linux-android-readelf -d app_static | grep NEEDED

# 2. 动态版
ndk-build clean && ndk-build APP_STL=c++_shared
cp libs/arm64-v8a/app app_shared
ls -l app_shared
aarch64-linux-android-readelf -d app_shared | grep NEEDED
```

对比：
- 大小差多少？
- `NEEDED` 列表差在哪？（动态版会多 `libc++_shared.so`）

## 验收清单

- [ ] 知道 libc++ 是 NDK 的 STL 实现
- [ ] 能说出 `c++_static` / `c++_shared` 的取舍
- [ ] 知道本项目为什么选 `c++_static`（单文件、无自己的 .so）
- [ ] 知道静态 STL 的"异常跨库边界"问题及何时该用动态
- [ ] 知道减少 STL 体积的 3 个技巧（避免 regex/iostream、用 string_view）
- [ ] 完成两种 STL 的对比实验，看到 NEEDED 差异

→ 下一章：[[第41章-体积与符号裁剪]]　—— 掌握 `-ffunction-sections` / `--gc-sections` / `-flto` / `strip` 四件套，并知道调试时该关掉哪个。
