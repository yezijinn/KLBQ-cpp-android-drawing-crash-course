---
tags: [教程, 卷三, Android, 日志]
day: 24
aliases: [ch38]
---

# 第 38 章 · logcat 与 android/log.h

> [!abstract] 本章目标
> 建立一套好用的原生日志系统，能分级、能格式化、能一键关闭。
> 本项目的 `LS_LOGI_TAG` / `LS_LOGE_TAG` 就是本章的产物。

> [!note] 承上
> 上一章讲了权限。
> 本章解决**调试的眼睛**——原生日志系统，让程序在手机上跑时你能看见它说什么。

## 先看本项目的用法

```cpp
LS_LOGI_TAG("Driver", "模块索引=%d 名称=%s 区段数量=%d", i, mod.name, mod.seg_count);
LS_LOGE_TAG("Dump", "无法创建 /sdcard/dump: %s", std::strerror(errno));
```

在手机上看：

```bash
adb logcat -s Driver:D Dump:E *:S
```

## 基础 API

```c
#include <android/log.h>

int __android_log_print(int prio, const char *tag, const char *fmt, ...);
```

| 参数 | 说明 |
|---|---|
| `prio` | 优先级（见下表） |
| `tag` | 标签（logcat 过滤用） |
| `fmt` | printf 格式串 |
| 返回值 | 写入的字节数 |

优先级常量：

| 常量 | 字符 | 用途 |
|---|---|---|
| `ANDROID_LOG_VERBOSE` | V | 最详细，默认被编译掉 |
| `ANDROID_LOG_DEBUG` | D | 调试 |
| `ANDROID_LOG_INFO` | I | 信息 |
| `ANDROID_LOG_WARN` | W | 警告 |
| `ANDROID_LOG_ERROR` | E | 错误 |
| `ANDROID_LOG_FATAL` | F | 致命（会 abort） |

链接：`Android.mk` 里加 `-llog`。

## 封装一套日志宏

```c
// log.h
#pragma once
#include <android/log.h>

#ifndef LOG_TAG
#define LOG_TAG "NativeApp"
#endif

// 发布时把 LOG_LEVEL 设为 ANDROID_LOG_WARN 即可关掉 I/D/V
#ifndef LOG_LEVEL
#ifdef NDEBUG
#define LOG_LEVEL ANDROID_LOG_INFO
#else
#define LOG_LEVEL ANDROID_LOG_VERBOSE
#endif
#endif

#define LOGV(...) do { if (LOG_LEVEL <= ANDROID_LOG_VERBOSE) \
    __android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__); } while(0)
#define LOGD(...) do { if (LOG_LEVEL <= ANDROID_LOG_DEBUG) \
    __android_log_print(ANDROID_LOG_DEBUG,   LOG_TAG, __VA_ARGS__); } while(0)
#define LOGI(...) do { if (LOG_LEVEL <= ANDROID_LOG_INFO) \
    __android_log_print(ANDROID_LOG_INFO,    LOG_TAG, __VA_ARGS__); } while(0)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
```

三个关键技巧：

**1. `##__VA_ARGS__` 处理空参数**

GCC 扩展：当可变参数为空时，自动删掉前面的逗号。

```c
#define LOGI(fmt, ...) __android_log_print(ANDROID_LOG_INFO, TAG, fmt, ##__VA_ARGS__)
LOGI("启动了");        // 展开成 (…, "启动了")，没有多余逗号 ✓
```

**2. `do { } while(0)` 包住**

让宏在 `if/else` 里也安全：

```c
if (err) LOGE("失败");        // 不加 do-while 会只执行第一条语句
else     ...;
```

**3. 编译期裁剪**

`if (LOG_LEVEL <= ANDROID_LOG_VERBOSE)` 是常量条件，
编译器在 `-O1` 以上会**直接删掉整个分支**——运行时零开销。

## 带 tag 参数的变体（本项目做法）

当代码分模块时，希望每条日志带不同 tag：

```c
#ifndef LS_LOGI_TAG
#define LS_LOGI_TAG(tag, fmt, ...) \
    __android_log_print(ANDROID_LOG_INFO, tag, fmt, ##__VA_ARGS__)
#endif

#ifndef LS_LOGE_TAG
#define LS_LOGE_TAG(tag, fmt, ...) \
    __android_log_print(ANDROID_LOG_ERROR, tag, fmt, ##__VA_ARGS__)
#endif
```

用 `#ifndef` 包住，允许外部先定义来覆盖（比如换成写文件）。

## 打印 string_view（重要）

`std::string_view` 是一段**带长度的字符串**，它**没有"结尾符"**。

> [!note] 先理解 `%s` 是怎么读字符串的
> `printf` 的 `%s` 遇到一个指针时，**不知道字符串有多长**——
> 它只能**从第一个字符开始，一个一个往后读，直到碰见 `\0`（空字符，值为 0）才停**。
> 所以普通 C 字符串必须在末尾放一个 `\0`（第 06 章讲过）。
>
> `std::string_view` **不保证末尾有 `\0`**——它只有"起始指针 + 长度"。
> 用 `%s` 打印它，`printf` 会一直读到**内存里偶然出现的 0** 才停，
> 可能多打几百个乱码字符，甚至读越界崩溃。

```cpp
// 错误：%s 不知道长度，会读越界，直到碰巧遇到 0
LOGI("模块: %s", moduleName.data());

// 正确：%.*s 用"精度"参数告诉它长度，只读 size 个字符
LOGI("模块: %.*s", (int)moduleName.size(), moduleName.data());
```

本项目 `driver.h` 里全是这个写法：

```cpp
LS_LOGE_TAG("Driver", "未找到模块 '%.*s'", (int)moduleName.size(), moduleName.data());
```

> [!danger] 这是本项目最容易被忽视的 bug 源
> `%s` 配 `string_view` 在调试时可能"看起来正常"（因为后面恰好有 0），
> 但在某些输入下会打印出几百个乱码字符，甚至崩溃。

## 打印地址

```cpp
uint64_t addr = 0x7a1b3ec650;
LOGI("Uworld: 0x%llx", (unsigned long long)addr);      // 小写
LOGI("Uworld: 0x%llX", (unsigned long long)addr);      // 大写
```

`%p` 需要 `void*`：

```cpp
LOGI("地址: %p", (void*)ptr);
```

## 打印 errno

```cpp
#include <cerrno>
#include <cstring>

LOGE("打开失败: errno=%d (%s)", errno, strerror(errno));
```

本项目 `DumpMemory` 里的用法：

```cpp
if (mkdir("/sdcard/dump", 0777) != 0 && errno != EEXIST) {
    LS_LOGE_TAG("Dump", "无法创建 /sdcard/dump: %s", std::strerror(errno));
    return false;
}
```

注意 `errno != EEXIST` —— 目录已存在不算错误。

## 高频日志的性能

每次 `__android_log_print` 都会：
1. 格式化字符串（vsnprintf）
2. 通过 socket 写给 logd 守护进程

> [!note] `v` 前缀是什么意思
> `printf` 家族的**名字里带 `v` 的，参数是 `va_list`**（"可变参数列表"），而不是"一个接一个的参数"。
> 对比：
> ```c
> printf("...", a, b);          // 普通版：直接列参数
> vprintf("...", ap);           // v 版：接收一个 va_list
>
> snprintf(buf, n, "...", a);   // 普通版
> vsnprintf(buf, n, "...", ap); // v 版
> ```
> **为什么需要 v 版**：当你要"把收到的可变参数原样转发给 printf"时，没法直接转发（参数个数不定），
> 只能先 `va_start` 拿到 `va_list`，再用 `v` 版输出。第 38 章前面 `FileLog::write` 就是这么做的。

开销约 **几十微秒**。**绝对不要放在每帧循环里**：

```cpp
// 灾难：每帧 200 次 × 50µs = 10ms
for (int i = 0; i < count; i++) {
    LOGI("actor %d: %llx", i, actors[i]);
}
```

正确做法：

```cpp
// 方案 1：限频
static int frame = 0;
if ((frame++ % 60) == 0) {
    LOGI("count=%d first=0x%llx", count, actors[0]);
}

// 方案 2：开关控制
if (Config.Debug) LOGI(...);

// 方案 3：累积后一次输出
if (!msg.empty()) LOGI("%s", msg.c_str());
```

## logd 的缓冲区

```bash
adb logcat -g
# main: ring buffer is 256Kb
# system: 256Kb
# crash: 256Kb
```

环形缓冲，写满就覆盖旧的。所以**崩溃前不久的日志可能被冲掉**：
- 重要的日志用 `-b crash` 或直接写文件
- 长时间运行的程序，关键事件写文件而不是只靠 logcat

## 写文件日志（调试长周期问题）

```cpp
class FileLog {
public:
    FileLog(const char *path) { fp_ = fopen(path, "a"); }
    ~FileLog() { if (fp_) fclose(fp_); }
    void write(const char *fmt, ...) {
        if (!fp_) return;
        va_list ap;
        va_start(ap, fmt);
        vfprintf(fp_, fmt, ap);
        va_end(ap);
        fflush(fp_);            // 立即落盘，崩溃时不丢
    }
private:
    FILE *fp_ = nullptr;
};
```

> [!note] 上面用到的可变参数机制（`va_list` 等）
> 想让函数像 `printf` 一样"参数个数不定"，要用这套：
> - `void write(const char *fmt, ...)`：`...` 表示"后面还可以接任意个参数"
> - `va_list ap; va_start(ap, fmt);`：开始遍历可变参数
> - `vfprintf(fp_, fmt, ap);`：用"可变参数版"的 printf 输出（`vfprintf` 对应 `fprintf`）
> - `va_end(ap);`：结束遍历
>
> **固定四步：`va_list` → `va_start` → 用 `v...` 函数 → `va_end`。** 本项目日志/格式化函数都是这个模式。

`fflush` 代价高但能保证崩溃时日志不丢——调试崩溃问题时值得。

## 结构化日志技巧

加时间戳和线程 id：

```cpp
#include <chrono>
#include <thread>

#define LOGT(...) LOGI("[%s] " __VA_ARGS__, thread_name())
```

或者用 logcat 自带的时间：

```bash
adb logcat -v time          # 01-14 01:23:45.678  1234  1234 I Tag: msg
adb logcat -v threadtime    # 带线程名（推荐）
```

## 动手：做一个分级日志模块

要求：
1. 支持 V/D/I/W/E 五级
2. 支持自定义 tag（每个模块一个）
3. 定义 `NDEBUG` 时自动关掉 V 和 D
4. 打印 `string_view` 不能崩
5. 提供一个"每 N 次才输出"的限频宏

参考实现：

```cpp
// log.h
#pragma once
#include <android/log.h>
#include <string_view>

#ifndef LOG_MIN_LEVEL
#ifdef NDEBUG
#define LOG_MIN_LEVEL ANDROID_LOG_INFO
#else
#define LOG_MIN_LEVEL ANDROID_LOG_VERBOSE
#endif
#endif

#define LOG_AT(prio, tag, fmt, ...) do {                       \
    if ((prio) >= LOG_MIN_LEVEL)                               \
        __android_log_print(prio, tag, fmt, ##__VA_ARGS__);    \
} while (0)

#define LOGI_T(tag, fmt, ...) LOG_AT(ANDROID_LOG_INFO,  tag, fmt, ##__VA_ARGS__)
#define LOGE_T(tag, fmt, ...) LOG_AT(ANDROID_LOG_ERROR, tag, fmt, ##__VA_ARGS__)

// 限频：每 n 次调用输出一次
#define LOG_EVERY_N(n, prio, tag, fmt, ...) do {               \
    static int counter = 0;                                    \
    if (++counter % (n) == 0)                                  \
        __android_log_print(prio, tag, fmt, ##__VA_ARGS__);     \
} while (0)

// 安全的 string_view 打印辅助
#define SV_FMT(sv) (int)(sv).size(), (sv).data()
// 用法：LOGI_T("Tag", "名称=%.*s", SV_FMT(name));
```

测试：

```cpp
LOGI_T("Test", "普通日志 %d", 42);
std::string_view sv("libUE4.so");
LOGI_T("Test", "模块=%.*s", SV_FMT(sv));
for (int i = 0; i < 100; i++)
    LOG_EVERY_N(10, ANDROID_LOG_INFO, "Test", "第 %d 次", i);
// 应只输出 10 条
```

## 动手验证清单

- [ ] **写一个日志宏**：定义 `LOGI`/`LOGE`，编译进程序
- [ ] **看分级输出**：`adb logcat -s 你的TAG` → 只看到自己 tag 的日志
- [ ] **验证级别过滤**：`adb logcat 你的TAG:D *:S` → 只显示 D 及以上
- [ ] **验证 NDEBUG 裁剪**：加 `-DNDEBUG` 编译 → 调试日志消失
- [ ] **安全打印 string_view**：用 `%.*s` + `(int)sv.size(), sv.data()` → 不乱码

## 验收清单

- [ ] 知道 `__android_log_print` 的参数和 5 个优先级（prio/tag/fmt/...，V/D/I/W/E）
- [ ] 理解 `##__VA_ARGS__` 和 `do{}while(0)` 的作用（前者去空参数逗号，后者保 if/else 安全）
- [ ] 会用编译期常量裁剪掉低级别日志（设 LOG_LEVEL=WARN，确认 I/D 被编掉）
- [ ] **知道 `string_view` 必须用 `%.*s`** ★（写 %s 观察乱码，改 %.*s 正常）
- [ ] 知道高频日志的危害，会限频（说出"每帧几十微秒，200 次占 10ms"）
- [ ] 完成了分级日志模块，5 项要求都满足（五级/自定义 tag/NDEBUG 裁剪/string_view 安全/限频宏）

→ 下一章：[[第39章-交叉编译的坑]]　—— 认识交叉编译里最常见的 8 个坑，知道每个坑的症状和解法。
