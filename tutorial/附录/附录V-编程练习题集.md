---
tags: [教程, 附录, 练习题, 编程]
aliases: [附录V, exercises]
---

# 附录 V · 编程练习题集（73 题）

> [!abstract] 与自测题的区别
> - [[附录F-学习检查点]] 的 81 题是**问答题**——测你"懂不懂"
> - **本篇 73 题是编程题**——测你"写不写得出来"
>
> **光读懂代码不算会，能写出来才算。**
>
> 每题标注难度：★ 基础 / ★★ 进阶 / ★★★ 挑战
> 参考答案用折叠块，**先自己做，再看答案**。

## 使用方法

```
1. 学完一卷 → 做该卷的题
2. 每题先看"要求"，自己写
3. 卡住 15 分钟 → 看"提示"
4. 再卡 15 分钟 → 看参考答案
5. 写完对照答案，重点看"我没想到的点"
```

**重要**：参考答案不是唯一解。
如果你的实现**逻辑正确、能通过自测**，就是对的。

---

## 卷一 · 编程地基（10 题）

### 题 1.1 内存转储（★）

**要求**：实现 `hexdump(const void *addr, size_t len)`，每行 16 字节，
格式为 `偏移  十六进制字节  可打印字符`。

**示例输出**：
```
0000  E8 03 00 00 41 42 43 00                          ....ABC.
```

**提示**：可打印字符指 ASCII 32~126，其余显示 `.`。

> [!question]- 参考答案
> ```c
> void hexdump(const void *addr, size_t len) {
>     const uint8_t *p = (const uint8_t *)addr;
>     for (size_t i = 0; i < len; i += 16) {
>         printf("%04zX  ", i);
>         // 十六进制部分
>         for (size_t j = 0; j < 16; j++) {
>             if (i + j < len) printf("%02X ", p[i + j]);
>             else             printf("   ");
>         }
>         printf(" ");
>         // 可打印字符部分
>         for (size_t j = 0; j < 16 && i + j < len; j++) {
>             const uint8_t c = p[i + j];
>             putchar((c >= 32 && c <= 126) ? c : '.');
>         }
>         putchar('\n');
>     }
> }
> ```
> **关键点**：① 用 `uint8_t*` 逐字节访问（避开对齐问题）② 末尾不足 16 字节要补空格对齐 ③ 可打印字符范围

### 题 1.2 结构体偏移计算器（★）

**要求**：给下面的结构体，**不运行程序**先手算每个字段的偏移和总大小，
然后写程序用 `offsetof` 验证。算出并解释每一段填充的来历。

```c
struct Mixed {
    char     a;
    double   b;
    short    c;
    int      d;
    char     e;
};
```

**提示**：对齐规则是"字段起始地址必须是自身大小的整数倍"，结构体总大小必须是最大成员大小的整数倍。

> [!question]- 参考答案
> ```
> a: 偏移 0        （char，任意对齐）
>   填充 7 字节     （为了让 double 对齐到 8）
> b: 偏移 8        （double，8 字节）
> c: 偏移 16       （short，2 的倍数）
>   填充 2 字节     （为了让 int 对齐到 4）
> d: 偏移 20       （int，4 字节）
> e: 偏移 24       （char）
>   填充 7 字节     （总大小要对齐到 8，即 max(sizeof)=8 的倍数）
> 总大小: 32
> ```
> ```c
> printf("a=%zu b=%zu c=%zu d=%zu e=%zu size=%zu\n",
>        offsetof(struct Mixed,a), offsetof(struct Mixed,b),
>        offsetof(struct Mixed,c), offsetof(struct Mixed,d),
>        offsetof(struct Mixed,e), sizeof(struct Mixed));
> // 0 8 16 20 24 32
> ```
> **重排优化**：把字段按大小降序排（`double; int; short; char; char;`）——
> `b(0-7) d(8-11) c(12-13) a(14) e(15)`，字段合计 16 字节，
> 16 已是 8 的倍数，**总大小 = 16**（不是 24）。
> 对比原来的 32 字节，**省了一半**。

### 题 1.3 指针链跟随（★★）

**要求**：实现 `follow_chain`，从基址开始按偏移数组逐级解引用。
任何一跳读到 0 就返回 0。**不要用外部进程**，用自己程序里的结构体模拟。

```c
uint64_t follow_chain(uint64_t base, const uint64_t *offsets, int count);
```

**提示**：`read_ptr(addr)` 就是 `*(uint64_t*)addr`。

> [!question]- 参考答案
> ```c
> static uint64_t read_ptr(uint64_t addr) {
>     return addr ? *(uint64_t *)addr : 0;
> }
>
> uint64_t follow_chain(uint64_t base, const uint64_t *offsets, int count) {
>     uint64_t cur = base;
>     for (int i = 0; i < count; i++) {
>         if (cur == 0) return 0;              // ★ 每跳判空
>         cur = read_ptr(cur + offsets[i]);
>         if (cur == 0) return 0;
>     }
>     return cur;
> }
> ```
> **关键点**：① 每跳判空（否则会从地址 0 读）② 循环外也判一次（base 可能为 0）

### 题 1.4 安全的数组读取器（★）

**要求**：实现 `SafeArray<T>`（C 或 C++），带 `count`/`max` 两个字段，
提供 `get(index)` 返回元素或 0（越界时），并实现 `is_valid()`。

**判据**：`data != 0 && count > 0 && count <= max && max > 0`

> [!question]- 参考答案
> ```cpp
> template <typename T>
> struct SafeArray {
>     uintptr_t data = 0;
>     int32_t   count = 0;
>     int32_t   max   = 0;
>
>     bool is_valid() const {
>         return data != 0 && count > 0 && count <= max && max > 0;
>     }
>
>     T get(int32_t index) const {
>         if (!is_valid()) return T{};
>         if (index < 0 || index >= count) return T{};   // ★ 用 count 不是 max
>         return *reinterpret_cast<const T *>(data + index * sizeof(T));
>     }
> };
> ```
> **关键点**：① 四条判据缺一不可 ② 边界用 `count`（实际元素数）而非 `max`（容量）

### 题 1.5 位字段打包/解包（★★）

> [!warning] 本题需要位运算基础
> 位运算的系统讲解在 **卷二第 24 章**。第 03 章只给了速查表（"先混个眼熟"）。
> **如果还没学到第 24 章，建议先跳过本题**，学完再回来做。
>
> **最小前置复习**（够做本题）：
> ```c
> // 把 3 个值打包进一个 uint16：
> //   bits 0-3   存 a（0~15）
> //   bits 4-9   存 b（0~63）
> //   bits 10-15 存 c（0~63）
> uint16_t pack(uint8_t a, uint8_t b, uint8_t c) {
>     return (a & 0x0F) | ((b & 0x3F) << 4) | ((c & 0x3F) << 10);
> }
> uint8_t unpack_a(uint16_t v) { return v & 0x0F; }
> uint8_t unpack_b(uint16_t v) { return (v >> 4) & 0x3F; }
> uint8_t unpack_c(uint16_t v) { return (v >> 10) & 0x3F; }
> ```
> **核心套路**：`& 掩码` 取字段，`<< 位数` 移到目标位置。

**要求**：把 4 个字段塞进一个 `uint32_t`：
`[31:24] type | [23:16] flags | [15:8] index | [7:0] count`。
实现 `pack` / `unpack`，并写断言测试。

**挑战**：再实现一版布局 `[31:28] type（4位）| [27:20] flags | [19:8] index（12位）| [7:0] count`。

> [!question]- 参考答案
> ```c
> uint32_t pack(uint8_t type, uint8_t flags, uint8_t index, uint8_t count) {
>     return ((uint32_t)type  << 24) |
>            ((uint32_t)flags << 16) |
>            ((uint32_t)index << 8)  |
>            ((uint32_t)count);
> }
>
> void unpack(uint32_t v, uint8_t *t, uint8_t *f, uint8_t *i, uint8_t *c) {
>     *t = (v >> 24) & 0xFF;
>     *f = (v >> 16) & 0xFF;
>     *i = (v >> 8)  & 0xFF;
>     *c = v & 0xFF;
> }
>
> // 挑战版（不等宽字段）
> uint32_t pack2(uint8_t type, uint8_t flags, uint16_t index, uint8_t count) {
>     return ((uint32_t)(type  & 0xF)  << 28) |
>            ((uint32_t)(flags & 0xFF) << 20) |
>            ((uint32_t)(index & 0xFFF)<< 8)  |
>            ((uint32_t)count & 0xFF);
> }
> ```
> **关键点**：① 先移位再或（不能用加，会进位）② 每个字段都要 `& mask` 防越界污染邻居

### 题 1.6 RAII 文件类（★★）

**要求**：写 `FileReader`，构造时打开文件、析构时自动关闭，
禁止拷贝，提供 `valid()` / `size()` / `read_all(vector&)`。

**提示**：`FileReader(const FileReader&) = delete;`

> [!question]- 参考答案
> ```cpp
> class FileReader {
> public:
>     explicit FileReader(const char *path) {
>         fp_ = fopen(path, "rb");
>         if (!fp_) { valid_ = false; return; }
>         fseek(fp_, 0, SEEK_END);
>         size_ = (size_t)ftell(fp_);
>         fseek(fp_, 0, SEEK_SET);
>         valid_ = true;
>     }
>     ~FileReader() { if (fp_) fclose(fp_); }
>
>     FileReader(const FileReader &) = delete;              // ★ 禁止拷贝
>     FileReader &operator=(const FileReader &) = delete;
>
>     bool valid() const { return valid_; }
>     size_t size() const { return size_; }
>
>     bool read_all(std::vector<uint8_t> &out) {
>         if (!valid_) return false;
>         out.resize(size_);
>         return fread(out.data(), 1, size_, fp_) == size_;
>     }
> private:
>     FILE *fp_ = nullptr;
>     size_t size_ = 0;
>     bool valid_ = false;
> };
> ```
> **关键点**：① 禁止拷贝（否则两个对象析构时 double close）② 构造失败时 `valid_ = false` 而不是抛异常（也可以用异常）

### 题 1.7 手写 `string_view`（★★★）

**要求**：实现一个简化版 `MyStringView`：
不拥有数据，提供 `size()` / `data()` / `substr()` / `startswith()` / `find()`。

> [!note] 本题在考什么
> 第 13 章讲了"怎么用 `string_view` 当参数"（零拷贝）。本题把它**拆开看内部**——
> 你会发现它其实就是**两个成员**：一个指针 + 一个长度。
>
> **为什么值得做**：做完这题你会彻底理解"view 不拥有数据"的含义——
> 也就理解了第 13 章「string_view 不拥有数据」那个警告——"不能返回局部变量的 view"。
>
> **本题需要你先知道**：
> - `SIZE_MAX` = 无符号 `size_t` 的最大值（"找不到"的约定返回值）
> - `memcmp` 按字节比较（不比 `\0`，适合可能不带结尾符的字符串）
> - `npos` 是"未找到"的惯用名（标准库 `std::string::npos` 同义）

**提示**：只需要两个成员：`const char *p_; size_t n_;`

> [!question]- 参考答案
> ```cpp
> class MyStringView {
> public:
>     MyStringView() = default;
>     MyStringView(const char *p, size_t n) : p_(p), n_(n) {}
>     MyStringView(const char *p) : p_(p), n_(p ? strlen(p) : 0) {}
>
>     const char *data() const { return p_; }
>     size_t size() const { return n_; }
>     bool empty() const { return n_ == 0; }
>
>     MyStringView substr(size_t pos, size_t len = SIZE_MAX) const {
>         if (pos >= n_) return {};
>         const size_t cnt = std::min(len, n_ - pos);
>         return {p_ + pos, cnt};
>     }
>
>     bool startswith(MyStringView prefix) const {
>         return n_ >= prefix.n_ && memcmp(p_, prefix.p_, prefix.n_) == 0;
>     }
>
>     size_t find(char c) const {
>         for (size_t i = 0; i < n_; i++) if (p_[i] == c) return i;
>         return npos;
>     }
>
>     static constexpr size_t npos = SIZE_MAX;
> private:
>     const char *p_ = nullptr;
>     size_t n_ = 0;
> };
> ```
> **关键点**：① **不拥有数据**，所以不能返回指向局部变量的 view ② `substr` 要处理 `len` 超出 ③ 用 `memcmp` 而不是 `strcmp`（可能没有 `\0`）

### 题 1.8 用 ASan 找 bug（★）

**要求**：下面的代码有 3 个内存错误，用 `-fsanitize=address` 跑一遍，
找出并修复。

```c
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

char *make_greeting(const char *name) {
    char buf[32];
    strcpy(buf, "Hello, ");
    strcat(buf, name);
    return buf;
}

int main(void) {
    char *g = make_greeting("World");
    printf("%s\n", g);
    char *arr = malloc(4);
    for (int i = 0; i <= 4; i++) arr[i] = (char)i;
    free(arr);
    printf("%d\n", arr[0]);
    return 0;
}
```

> [!question]- 参考答案
> **三个错误**：
> 1. `return buf` —— 返回局部数组地址（栈上，函数返回后失效）
> 2. `i <= 4` —— 越界写（`malloc(4)` 只有 0~3）
> 3. `arr[0]` after `free` —— use-after-free
>
> **修复**：
> ```c
> char *make_greeting(const char *name) {
>     const size_t need = strlen("Hello, ") + strlen(name) + 1;
>     char *buf = (char *)malloc(need);          // ★ 改堆分配
>     if (!buf) return NULL;                     // C 里用 NULL，不是 nullptr
>     strcpy(buf, "Hello, ");
>     strcat(buf, name);
>     return buf;                                // ★ 返回堆指针
> }
>
> int main(void) {
>     char *g = make_greeting("World");
>     printf("%s\n", g);
>     free(g);                                   // ★ 记得释放
>
>     char *arr = malloc(4);
>     for (int i = 0; i < 4; i++) arr[i] = (char)i;   // ★ i < 4
>     free(arr);
>     arr = NULL;                                // ★ 置空，防误用（C 里用 NULL）
>     return 0;
> }
> ```
> **ASan 会直接指出 2 和 3**；错误 1 编译器会警告 `-Wreturn-local-addr`。

### 题 1.9 简易 Makefile（★）

**要求**：给三个文件（`main.cpp`、`vec.cpp`、`utils.cpp`）写 Makefile：
支持 `make` / `make clean` / `make run`，改头文件能触发重编译。

> [!question]- 参考答案
> ```makefile
> CXX      := g++
> CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -MMD -MP
> TARGET   := app
> SRCS     := main.cpp vec.cpp utils.cpp
> OBJS     := $(SRCS:.cpp=.o)
> DEPS     := $(OBJS:.o=.d)
>
> all: $(TARGET)
>
> $(TARGET): $(OBJS)
> 	$(CXX) $(OBJS) -o $@
>
> %.o: %.cpp
> 	$(CXX) $(CXXFLAGS) -c $< -o $@
>
> run: $(TARGET)
> 	./$(TARGET)
>
> clean:
> 	rm -f $(OBJS) $(DEPS) $(TARGET)
>
> -include $(DEPS)      # ★ 头文件依赖
>
> .PHONY: all run clean
> ```
> **关键点**：`-MMD -MP` 生成 `.d` 文件，`-include` 把它们包含进来。**注意命令行前必须是 Tab。**

### 题 1.10 从 C 到 C++ 的重构（★★）

**要求**：把下面这段 C 代码重构成现代 C++：
用 `std::vector` 替代裸数组、`std::string` 替代 `char*`、
`unique_ptr` 替代裸指针，并加 `const`。

```c
typedef struct {
    char *name;
    int *scores;
    int count;
} Student;

Student *create_student(const char *name, int n) {
    Student *s = malloc(sizeof(Student));
    s->name = malloc(strlen(name) + 1);
    strcpy(s->name, name);
    s->scores = malloc(sizeof(int) * n);
    s->count = n;
    return s;
}
```

> [!question]- 参考答案
> ```cpp
> struct Student {
>     std::string name;
>     std::vector<int> scores;
>
>     Student(std::string n, size_t count)
>         : name(std::move(n)), scores(count, 0) {}   // ★ 初始化列表
> };
>
> // 用法：栈对象，自动管理
> Student s{"Alice", 5};
> s.scores[0] = 95;
>
> // 需要堆对象时
> auto sp = std::make_unique<Student>("Bob", 3);
> ```
> **关键点**：① 不需要 `create_student` 函数了——构造函数就是 ② `std::vector(count, 0)` 一次初始化 ③ 栈对象优先（第 14 章）④ 没有手动 `free`

---

## 卷二 · 系统与底层（8 题）

### 题 2.1 十六进制字符串转整数（★）

**要求**：不用 `strtoul`，自己实现 `hex_to_u64(const char *s)`，
支持大小写、支持 `0x` 前缀、遇非法字符停止。返回解析结果和停止位置。

**提示**：每个字符转 0-15，累加时 `result = result * 16 + digit`。

> [!question]- 参考答案
> ```c
> static int hex_digit(char c) {
>     if (c >= '0' && c <= '9') return c - '0';
>     if (c >= 'a' && c <= 'f') return c - 'a' + 10;
>     if (c >= 'A' && c <= 'F') return c - 'A' + 10;
>     return -1;
> }
>
> uint64_t hex_to_u64(const char *s, const char **end_out) {
>     if (!s) return 0;
>     if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
>
>     uint64_t result = 0;
>     while (*s) {
>         const int d = hex_digit(*s);
>         if (d < 0) break;                        // ★ 非法字符停止
>         result = (result << 4) | (uint64_t)d;    // ★ 相当于 *16 + d
>         s++;
>     }
>     if (end_out) *end_out = s;
>     return result;
> }
> ```
> **关键点**：`<< 4` 比 `* 16` 更直观地表达"十六进制"；用 `const char**` 返回停止位置（第 08 章的输出参数模式）

### 题 2.2 解析 maps 行（★★）

**要求**：写 `parse_map_line(const char *line, MapRegion *out)`，
解析 `/proc/pid/maps` 的一行。要求正确处理路径可能含空格的情况。

**格式**：`start-end perms offset dev inode [path]`

> [!question]- 参考答案
> ```c
> typedef struct {
>     uint64_t start, end, offset;
>     uint8_t  perms;      // 1=R 2=W 4=X
>     bool     is_private;
>     char     path[512];
> } MapRegion;
>
> bool parse_map_line(const char *line, MapRegion *out) {
>     if (!line || !out) return false;
>     memset(out, 0, sizeof(*out));
>
>     unsigned long long s = 0, e = 0, off = 0, ino = 0;
>     char perm[8] = {};
>     char dev[16] = {};
>     char path[512] = {};
>
>     // ★ %511[^\n] 捕获可能含空格的路径
>     const int n = sscanf(line, "%llx-%llx %7s %llx %15s %llu %511[^\n]",
>                          &s, &e, perm, &off, dev, &ino, path);
>     if (n < 6) return false;
>
>     out->start = s; out->end = e; out->offset = off;
>     if (perm[0] == 'r') out->perms |= 1;
>     if (perm[1] == 'w') out->perms |= 2;
>     if (perm[2] == 'x') out->perms |= 4;
>     out->is_private = (perm[3] == 'p');
>     if (n >= 7) snprintf(out->path, sizeof(out->path), "%s", path);
>     return true;
> }
> ```
> **关键点**：① `%511[^\n]` 而不是 `%s`（后者遇空格停止）② 判断 `n >= 7` 因为匿名区域没有路径 ③ 缓冲区大小要与格式串的数字匹配

### 题 2.3 按模块名找基址（★★）

**要求**：实现 `find_module_base(maps, name)`，
要求**后缀完全匹配**且**前面必须是 `/`**，避免命中 `libUE4.so.bak`。

> [!question]- 参考答案
> ```c
> uint64_t find_module_base(const MapRegion *regions, int count,
>                           const char *module_name) {
>     if (!module_name) return 0;
>     const size_t name_len = strlen(module_name);
>
>     for (int i = 0; i < count; i++) {
>         const char *path = regions[i].path;
>         if (!path[0]) continue;
>
>         const size_t path_len = strlen(path);
>         if (path_len < name_len) continue;
>
>         const size_t pos = path_len - name_len;
>         // ★ 前一个字符必须是 '/'（或 pos==0，说明整个路径就是模块名）
>         if (pos > 0 && path[pos - 1] != '/') continue;
>         // ★ 后缀必须完全相等
>         if (strcmp(path + pos, module_name) != 0) continue;
>
>         return regions[i].start;
>     }
>     return 0;
> }
> ```
> **关键点**：① 两条判据合起来才能避免误匹配 ② 用 `strcmp` 比后缀而不是 `strstr`（后者会命中中间）
> **反例测试**：`find_module_base(maps, "libUE4.so")` 不应命中 `.../libUE4.so.bak`

### 题 2.4 合并相邻区域（★★）

**要求**：给一个乱序的区间列表，合并所有重叠或相邻的区间，输出有序结果。

**示例**：`[1,3] [6,8] [2,5] [10,12]` → `[1,8] [10,12]`

> [!question]- 参考答案
> ```cpp
> struct Range { uint64_t start, end; };
>
> void merge_ranges(std::vector<Range> &rs) {
>     if (rs.empty()) return;
>
>     std::sort(rs.begin(), rs.end(),
>               [](const Range &a, const Range &b) {
>                   return a.start < b.start ||
>                          (a.start == b.start && a.end < b.end);
>               });
>
>     size_t w = 0;                                  // 写入位置
>     for (size_t i = 1; i < rs.size(); i++) {
>         if (rs[i].start <= rs[w].end) {            // ★ 重叠或相邻
>             rs[w].end = std::max(rs[w].end, rs[i].end);
>         } else {
>             rs[++w] = rs[i];
>         }
>     }
>     rs.resize(w + 1);
> }
> ```
> **关键点**：① 先排序（O(n log n)）② 原地合并（用一个写指针 `w`，省内存）③ 判据是 `<=` 而非 `<`（相邻也要合并）

### 题 2.5 读文件到 string（★）

**要求**：实现 `read_file(path) -> std::string`，要求：
① 处理打开失败 ② 正确判断文件大小 ③ 不依赖 `seek` 也行（用循环读）。

**提示**：可以用 `fseek`+`ftell` 拿大小，也可以用 `read` 循环追加。

> [!question]- 参考答案
> ```cpp
> // 方案一：先求大小（适合普通文件）
> std::optional<std::string> read_file(const char *path) {
>     std::ifstream f(path, std::ios::binary);
>     if (!f) return std::nullopt;
>
>     f.seekg(0, std::ios::end);
>     const std::streamoff size = f.tellg();
>     if (size < 0) return std::nullopt;
>     f.seekg(0, std::ios::beg);
>
>     std::string out(static_cast<size_t>(size), '\0');
>     if (size > 0 && !f.read(out.data(), size)) return std::nullopt;
>     return out;
> }
>
> // 方案二：循环读（适合管道/设备等大小未知的情况）
> std::optional<std::string> read_file_loop(const char *path) {
>     const int fd = open(path, O_RDONLY);
>     if (fd < 0) return std::nullopt;
>
>     std::string out;
>     char buf[4096];
>     ssize_t n;
>     while ((n = read(fd, buf, sizeof(buf))) > 0) {
>         out.append(buf, static_cast<size_t>(n));   // ★ 处理短读
>     }
>     close(fd);
>     return (n < 0) ? std::nullopt : std::make_optional(out);
> }
> ```
> **关键点**：① 用 `optional` 表达"可能失败"（第 13 章）② 循环读天然处理短读（第 22 章）③ `read` 返回 0 表示 EOF

### 题 2.6 解析 ELF 头（★★）

**要求**：读一个 ELF 文件，输出：类别（32/64 位）、字节序、类型、架构、入口地址、程序头数量。要检查魔数。

> [!question]- 参考答案
> ```c
> #include <elf.h>
>
> bool parse_elf_head(const char *path) {
>     FILE *fp = fopen(path, "rb");
>     if (!fp) return false;
>
>     Elf64_Ehdr eh;
>     if (fread(&eh, 1, sizeof(eh), fp) != sizeof(eh)) { fclose(fp); return false; }
>     fclose(fp);
>
>     // ★ 检查魔数
>     if (memcmp(eh.e_ident, ELFMAG, SELFMAG) != 0) {
>         printf("不是 ELF 文件\n");
>         return false;
>     }
>
>     printf("类别  : %s\n", eh.e_ident[EI_CLASS] == ELFCLASS64 ? "64 位" : "32 位");
>     printf("字节序: %s\n", eh.e_ident[EI_DATA] == ELFDATA2LSB ? "小端" : "大端");
>     printf("类型  : ");
>     switch (eh.e_type) {
>         case ET_REL:  printf("可重定位 (.o)\n"); break;
>         case ET_EXEC: printf("可执行文件\n");    break;
>         case ET_DYN:  printf("动态库/PIE\n");    break;
>         default:      printf("其它 (%d)\n", eh.e_type); break;
>     }
>     printf("架构  : 0x%X %s\n", eh.e_machine,
>            eh.e_machine == EM_AARCH64 ? "(AArch64)" :
>            eh.e_machine == EM_X86_64  ? "(x86_64)"  : "");
>     printf("入口  : 0x%llX\n", (unsigned long long)eh.e_entry);
>     printf("程序头: 数量 %d 每个 %d 字节\n", eh.e_phnum, eh.e_phentsize);
>     return true;
> }
> ```
> **关键点**：① 用系统 `<elf.h>` 的常量（`ELFMAG`/`ELFCLASS64`/`EM_AARCH64`）而不是记数字 ② `e_ident` 是 16 字节数组，前 4 字节是魔数

### 题 2.7 字节序转换（★）

**要求**：实现 `swap32` / `swap64`（不调用库函数），并写程序验证
"小端机上的 `0x12345678` 经 swap32 后变成 `0x78563412`"。

> [!question]- 参考答案
> ```c
> uint32_t swap32(uint32_t v) {
>     return ((v & 0xFF000000u) >> 24) |
>            ((v & 0x00FF0000u) >> 8)  |
>            ((v & 0x0000FF00u) << 8)  |
>            ((v & 0x000000FFu) << 24);
> }
>
> uint64_t swap64(uint64_t v) {
>     return ((v & 0xFF00000000000000ULL) >> 56) |
>            ((v & 0x00FF000000000000ULL) >> 40) |
>            ((v & 0x0000FF0000000000ULL) >> 24) |
>            ((v & 0x000000FF00000000ULL) >> 8)  |
>            ((v & 0x00000000FF000000ULL) << 8)  |
>            ((v & 0x0000000000FF0000ULL) << 24) |
>            ((v & 0x000000000000FF00ULL) << 40) |
>            ((v & 0x00000000000000FFULL) << 56);
> }
>
> // 验证
> assert(swap32(0x12345678u) == 0x78563412u);
> assert(swap64(0x0123456789ABCDEFULL) == 0xEFCDAB8967452301ULL);
> ```
> **关键点**：① 每字节先 `& mask` 再用 `<<`/`>>` 移到目标位置 ② 用 `u`/`ULL` 后缀防有符号溢出

### 题 2.8 位图实现（★★★）

**要求**：实现一个 `Bitmap`（位图），支持：
`set(i)` / `clear(i)` / `test(i)`，内部用 `uint64_t` 数组存储，**按位打包**。
再实现 `find_first_zero()`（找第一个 0 位）。

**提示**：`i / 64` 是字索引，`i % 64` 是位偏移。用 `1ULL << offset`。

> [!note] `__builtin_ctzll` 是什么（参考答案里会用到）
> **GCC/Clang 提供的内置函数**（不是 C 标准的一部分，但 Android 的 NDK 就是 GCC/Clang，能用）。
> - **`ctz`** = **C**ount **T**railing **Z**eros（数末尾有几个 0）
> - **`ll`** = long long（64 位）
> - `__builtin_ctzll(x)` 返回"x 的二进制里，从最低位起数，连续 0 的个数"
>
> 例：`__builtin_ctzll(8)` → 3（8 = `...1000`，末尾 3 个 0）
>     `__builtin_ctzll(1)` → 0（末尾 0 个 0）
>
> **不想用它？** 逐位检查也可以（慢一点但更易懂）：
> ```cpp
> for (unsigned b = 0; b < 64; b++)
>     if (!(words_[w] & (1ULL << b))) return w * 64 + b;
> ```

> [!question]- 参考答案
> ```cpp
> class Bitmap {
> public:
>     explicit Bitmap(size_t bits)
>         : words_((bits + 63) / 64, 0), bits_(bits) {}
>
>     void set(size_t i)   { if (i < bits_) words_[i>>6] |=  (1ULL << (i & 63)); }
>     void clear(size_t i) { if (i < bits_) words_[i>>6] &= ~(1ULL << (i & 63)); }
>     bool test(size_t i) const {
>         return i < bits_ && ((words_[i>>6] >> (i & 63)) & 1ULL);
>     }
>
>     // 找第一个 0 位，找不到返回 bits_
>     size_t find_first_zero() const {
>         for (size_t w = 0; w < words_.size(); w++) {
>             if (words_[w] == ~0ULL) continue;          // 全 1 跳过
>             // ★ 用 GCC 内置函数找第一个 0 位（比逐位快）
>             const unsigned bit = __builtin_ctzll(~words_[w]);
>             const size_t idx = w * 64 + bit;
>             return idx < bits_ ? idx : bits_;
>         }
>         return bits_;
>     }
>
>     size_t size() const { return bits_; }
> private:
>     std::vector<uint64_t> words_;
>     size_t bits_;
> };
> ```
> **关键点**：① `i >> 6` = `i / 64`，`i & 63` = `i % 64` ② 用 `1ULL`（无符号 64 位）避免移位溢出 ③ `__builtin_ctzll` 找最低位的 0（`~words_[w]` 后即"第一个 0 位"）

---

## 卷三 · Android 原生（10 题）

### 题 3.1 写 `Android.mk`（★）

**要求**：给一个含 3 个 `.cpp`（`main.cpp`、`log.cpp`、`util.cpp`）、
2 个头文件目录（`include/`、`include/util/`）的工程写构建脚本，
产物名 `myapp`，链接 `liblog` 和 `libandroid`，用 C++20。

> [!question]- 参考答案
> ```makefile
> # Android.mk
> LOCAL_PATH := $(call my-dir)
>
> include $(CLEAR_VARS)
> LOCAL_MODULE    := myapp
> LOCAL_CPPFLAGS  := -std=c++20 -Wall -Wextra -fexceptions -frtti
> LOCAL_C_INCLUDES := $(LOCAL_PATH)/include \
>                     $(LOCAL_PATH)/include/util
> LOCAL_SRC_FILES := src/main.cpp \
>                    src/log.cpp \
>                    src/util.cpp
> LOCAL_LDLIBS    := -llog -landroid
> include $(BUILD_EXECUTABLE)
> ```
> ```makefile
> # Application.mk
> APP_ABI      := arm64-v8a
> APP_PLATFORM := android-25
> APP_STL      := c++_static
> APP_OPTIM    := release
> ```
> **关键点**：① `$(call my-dir)` 必须在最前 ② `LOCAL_PATH` 拼路径 ③ `BUILD_EXECUTABLE` 决定产物类型

### 题 3.2 分级日志模块（★★）

**要求**：实现日志模块，满足：
① V/D/I/W/E 五级 ② 可自定义 tag ③ `NDEBUG` 时自动关掉 V/D
④ **`string_view` 打印不能崩** ⑤ 提供"每 N 次输出一次"的宏

> [!question]- 参考答案
> ```cpp
> // log.h
> #pragma once
> #include <android/log.h>
> #include <string_view>
>
> #ifndef LOG_MIN_LEVEL
> #  ifdef NDEBUG
> #    define LOG_MIN_LEVEL ANDROID_LOG_INFO
> #  else
> #    define LOG_MIN_LEVEL ANDROID_LOG_VERBOSE
> #  endif
> #endif
>
> #define LOG_AT(prio, tag, fmt, ...) do {                     \
>     if ((prio) >= LOG_MIN_LEVEL)                             \
>         __android_log_print(prio, tag, fmt, ##__VA_ARGS__);  \
> } while (0)
>
> #define LOGD_T(tag, fmt, ...) LOG_AT(ANDROID_LOG_DEBUG, tag, fmt, ##__VA_ARGS__)
> #define LOGI_T(tag, fmt, ...) LOG_AT(ANDROID_LOG_INFO,  tag, fmt, ##__VA_ARGS__)
> #define LOGE_T(tag, fmt, ...) LOG_AT(ANDROID_LOG_ERROR, tag, fmt, ##__VA_ARGS__)
>
> // ★ 限频：每 n 次输出一次
> #define LOG_EVERY_N(n, prio, tag, fmt, ...) do {             \
>     static int cnt_ = 0;                                     \
>     if (++cnt_ % (n) == 0)                                   \
>         __android_log_print(prio, tag, fmt, ##__VA_ARGS__);  \
> } while (0)
>
> // ★ 安全打印 string_view：用 %.*s + 精度参数
> #define SV_FMT(sv) static_cast<int>((sv).size()), (sv).data()
> // 用法：LOGI_T("T", "name=%.*s", SV_FMT(name));
> ```
> **关键点**：① `##__VA_ARGS__` 处理空参数 ② `do{}while(0)` 保证在 if 里安全 ③ 编译期常量条件让 `-O1` 直接删掉分支 ④ `%.*s` 是打印 `string_view` 的唯一正确方式

### 题 3.3 一键部署脚本（★）

**要求**：写脚本完成"编译 → push → chmod → 运行 → 抓日志"，
编译失败要立即停止。

> [!question]- 参考答案
> ```bash
> #!/bin/bash
> set -e                                    # ★ 任何命令失败立即退出
>
> TARGET=${1:-myapp}
> MODE=${2:-release}
> REMOTE=/data/local/tmp/$TARGET
>
> echo "==> 编译 ($MODE)"
> ndk-build -j8 APP_OPTIM=$MODE
>
> echo "==> 推送"
> adb push "libs/arm64-v8a/$TARGET" "$REMOTE"
> adb shell chmod 755 "$REMOTE"
>
> echo "==> 清日志并运行"
> adb logcat -c
> adb shell "$REMOTE" &
> sleep 1
>
> echo "==> 日志"
> adb logcat -d -s "$TARGET" | tail -30
> ```
> **关键点**：① `set -e` 让"编译失败不继续"，但注意它也会让 `adb` 失败时静默退出——`set -e` 的取舍见第 35 章「run.sh 的两个坑」 ② `adb logcat -d` 是 dump 后退出（不会阻塞） ③ `"$REMOTE"` 加引号防路径含空格

### 题 3.4 设备信息采集（★）

**要求**：写程序输出设备的：API 等级、Android 版本、ABI、是否 root、
SELinux 状态、几个关键进程的 PID。

> [!question]- 参考答案
> ```cpp
> #include <sys/system_properties.h>
> #include <unistd.h>
> #include <cstdio>
> #include <cstdlib>
>
> static std::string prop(const char *name) {
>     char buf[PROP_VALUE_MAX] = {0};
>     __system_property_get(name, buf);
>     return buf;
> }
>
> int main() {
>     printf("API 等级   : %s\n", prop("ro.build.version.sdk").c_str());
>     printf("Android    : %s\n", prop("ro.build.version.release").c_str());
>     printf("主板 ABI   : %s\n", prop("ro.product.cpu.abi").c_str());
>     printf("ABI 列表   : %s\n", prop("ro.product.cpu.abilist").c_str());
>     printf("型号       : %s\n", prop("ro.product.model").c_str());
>     printf("当前 uid   : %d (%s)\n", getuid(), getuid() == 0 ? "root" : "非 root");
>     return 0;
> }
> ```
> ```bash
> # SELinux 与进程状态用 shell 命令更快
> adb shell getenforce
> adb shell ps -A | grep -E 'surfaceflinger|zygote|system_server'
> ```
> **关键点**：① `PROP_VALUE_MAX` 是属性值缓冲区大小 ② `getuid() == 0` 判断 root（但还要考虑 SELinux）

### 题 3.5 体积测量脚本（★★）

**要求**：写脚本，对同一个工程做 5 次不同配置的构建，
输出每次的产物大小和相对上一行的差异。

> [!question]- 参考答案
> ```bash
> #!/bin/bash
> set -e
> OUT=size_report.txt
> : > $OUT                       # 清空
>
> build_and_measure() {
>     local label="$1"; shift
>     ndk-build clean >/dev/null 2>&1
>     ndk-build -j8 "$@" >/dev/null 2>&1 || { echo "$label 构建失败"; return; }
>     local sz=$(stat -c %s libs/arm64-v8a/myapp 2>/dev/null || echo 0)
>     echo "$label $sz" >> $OUT
> }
>
> build_and_measure "基线(-O2)"                    LOCAL_CPPFLAGS="-O2"
> build_and_measure "+function-sections"           LOCAL_CPPFLAGS="-O2 -ffunction-sections -fdata-sections"
> build_and_measure "+gc-sections"                 LOCAL_CPPFLAGS="-O2 -ffunction-sections -fdata-sections" LOCAL_LDFLAGS="-Wl,--gc-sections"
> build_and_measure "+lto"                         LOCAL_CPPFLAGS="-O2 -ffunction-sections -fdata-sections" LOCAL_LDFLAGS="-flto -Wl,--gc-sections"
> build_and_measure "+strip"                       LOCAL_CPPFLAGS="-O2 -ffunction-sections -fdata-sections" LOCAL_LDFLAGS="-flto -Wl,--gc-sections -s"
>
> echo "配置                        字节      相对上一行"
> prev=0
> while read -r label sz; do
>     if [ "$prev" -eq 0 ]; then
>         printf "%-26s %9d  —\n" "$label" "$sz"
>     else
>         printf "%-26s %9d  %+.1f%%\n" "$label" "$sz" \
>                "$(echo "scale=2; ($sz-$prev)*100/$prev" | bc)"
>     fi
>     prev=$sz
> done < $OUT
> ```
> **关键点**：① 用 `bc` 算百分比（bash 不支持浮点）② 每次都要 `clean`，否则增量构建结果不真实 ③ 命令行传参可覆盖 Makefile 变量

### 题 3.6 引入第三方库（★★）

**要求**：把一个自己编译的静态库 `libmylib.a`（含 `int double_it(int)`）
接入工程并调用。要求写出完整的 `.mk` 改动和调用代码。

> [!question]- 参考答案
> ```bash
> # ① 先编译库（用 NDK 的 clang）
> cat > mylib.c << 'EOF'
> int double_it(int x) { return x * 2; }
> EOF
>
> $NDK/toolchains/llvm/prebuilt/windows-x86_64/bin/aarch64-linux-android21-clang \
>     -c mylib.c -o mylib.o
> $NDK/toolchains/llvm/prebuilt/windows-x86_64/bin/llvm-ar rcs libmylib.a mylib.o
>
> # ② 放到工程
> mkdir -p jni/include/Mine
> cp libmylib.a jni/include/Mine/
> ```
> ```makefile
> # ③ Android.mk 里声明预编译库（放在主模块之前）
> include $(CLEAR_VARS)
> LOCAL_MODULE    := mylib_prebuilt
> LOCAL_SRC_FILES := include/Mine/libmylib.a
> include $(PREBUILT_STATIC_LIBRARY)
>
> # ④ 主模块引用（注意位置：被依赖的放右边）
> LOCAL_STATIC_LIBRARIES := mylib_prebuilt
> ```
> ```cpp
> // ⑤ 调用
> extern "C" int double_it(int x);      // ★ 因为是 C 函数，加 extern "C"
> LOGI("double_it(21) = %d", double_it(21));   // 42
> ```
> **关键点**：① 预编译库用 `PREBUILT_STATIC_LIBRARY` ② `extern "C"` 防止 C++ 名字修饰导致链接失败 ③ 库必须与目标 ABI 一致（arm64）

### 题 3.7 用 clang 直编 arm64（★）

**要求**：写一个 `hello.c`，用 NDK 的 clang 编译器**直接编译**成 arm64 可执行文件（不走 ndk-build），然后用 `file` 命令验证产物架构。

> [!question]- 参考答案
> ```c
> // hello.c
> #include <stdio.h>
> #include <android/log.h>
> #define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "HelloNDK", __VA_ARGS__)
> int main(void) {
>     printf("hello from arm64\n");
>     LOGI("logcat 也能看到我");
>     return 0;
> }
> ```
> ```bash
> $NDK/toolchains/llvm/prebuilt/windows-x86_64/bin/aarch64-linux-android21-clang.cmd \
>     hello.c -o hello_arm64 -llog
> file hello_arm64
> ```
> `file` 应输出 `ELF 64-bit LSB executable, ARM aarch64, ...`
> **关键点**：① 编译器名字里的 `21` 是最低 API ② `-llog` 才能用 `__android_log_print` ③ Windows 上 clang 带 `.cmd` 后缀

### 题 3.8 诊断三种权限拒绝（★★）

**要求**：制造并识别三种权限拒绝：DAC（没执行权限）、UID（非 root 访问受保护文件）、SELinux（root 仍被策略拦）。

> [!question]- 参考答案
> ```bash
> # 1. DAC 拒绝
> adb push hello /data/local/tmp/noperm
> adb shell /data/local/tmp/noperm     # → Permission denied
> # 识别：没有 avc: denied 日志
>
> # 2. UID 拒绝
> adb shell cat /proc/1/cmdline        # → Permission denied（非 root）
>
> # 3. SELinux 拒绝
> adb shell su -c "cat /proc/1/mem"    # → Permission denied（即使 root）
> adb shell su -c dmesg | grep avc     # → 看到 avc: denied
> ```
> **关键点**：区分三种拒绝的**唯一可靠方法**是看有没有 `avc: denied` 日志（第 37 章）。

### 题 3.9 复现交叉编译的坑（★★）

**要求**：故意制造"ABI 不匹配"（编译 armeabi-v7a 推到 arm64 设备），观察报错。

> [!question]- 参考答案
> ```bash
> # 坑：ABI 错
> ndk-build APP_ABI=armeabi-v7a        # 编出 32 位
> adb push libs/armeabi-v7a/app /data/local/tmp/
> adb shell /data/local/tmp/app        # → "not executable: 32-bit ELF"
> ```
> **关键点**：ABI 决定"能不能跑"，API level 决定"能不能用某个函数"（第 39 章）。

### 题 3.10 搭规范工程骨架（★）

**要求**：按第 44 章的目录约定，搭出 `include/` + `src/` 镜像结构的工程骨架，包含至少 2 个模块、`.gitignore`。

> [!question]- 参考答案
> ```
> myproject/
> ├── jni/
> │   ├── Android.mk
> │   ├── Application.mk
> │   ├── include/
> │   │   ├── My_Utils/Log.h
> │   │   └── Core/Engine.h
> │   └── src/
> │       ├── main.cpp
> │       ├── My_Utils/Log.cpp
> │       └── Core/Engine.cpp
> ├── .gitignore          # 排除 libs/ obj/
> └── build.sh            # 一键构建
> ```
> **关键点**：① `include/` 和 `src/` 子目录结构**镜像对应** ② `.gitignore` 排除 `libs/` `obj/`

---

## 卷四 · 3D 数学（10 题）

### 题 4.1 向量库（★）

**要求**：实现 `Vec3`，含：加减、数乘、点积、叉积、长度、归一化（防除零）、
距离平方。写断言验证 6 个性质。

> [!question]- 参考答案
> ```cpp
> struct Vec3 {
>     float x = 0, y = 0, z = 0;
>     Vec3() = default;
>     Vec3(float a, float b, float c) : x(a), y(b), z(c) {}
>
>     Vec3 operator+(const Vec3 &v) const { return {x+v.x, y+v.y, z+v.z}; }
>     Vec3 operator-(const Vec3 &v) const { return {x-v.x, y-v.y, z-v.z}; }
>     Vec3 operator*(float s)       const { return {x*s, y*s, z*s}; }
>
>     float Dot(const Vec3 &v)   const { return x*v.x + y*v.y + z*v.z; }
>     Vec3  Cross(const Vec3 &v) const {
>         return {y*v.z - z*v.y, z*v.x - x*v.z, x*v.y - y*v.x};
>     }
>     float LengthSq() const { return x*x + y*y + z*z; }
>     float Length()   const { return std::sqrt(LengthSq()); }
>     Vec3  Normalized() const {
>         const float len = Length();
>         return len < 1e-6f ? Vec3{} : Vec3{x/len, y/len, z/len};   // ★ 防除零
>     }
>     static float DistanceSq(const Vec3 &a, const Vec3 &b) {
>         return (b - a).LengthSq();
>     }
> };
>
> // 6 项断言
> assert(fabsf(Vec3(3,4,0).Length() - 5.0f) < 1e-5f);
> assert(fabsf(Vec3(3,4,0).Normalized().Length() - 1.0f) < 1e-5f);
> assert(Vec3(1,0,0).Dot(Vec3(0,1,0)) == 0.0f);
> const Vec3 cr = Vec3(1,0,0).Cross(Vec3(0,1,0));
> assert(cr.z == 1.0f && cr.Dot(Vec3(1,0,0)) == 0.0f && cr.Dot(Vec3(0,1,0)) == 0.0f);
> assert(Vec3(0,0,0).Normalized().LengthSq() == 0.0f);     // 零向量不产生 NaN
> assert(fabsf(Vec3::DistanceSq(Vec3(0,0,0), Vec3(3,4,0)) - 25.0f) < 1e-5f);
> ```

### 题 4.2 矩阵基本运算（★）

**要求**：实现 4×4 矩阵的：单位矩阵、乘法、转置、取平移分量。
验证"乘法不可交换"。

> [!question]- 参考答案
> ```cpp
> struct Mat4 {
>     float M[4][4] = {};
>     static Mat4 Identity() {
>         Mat4 m;
>         for (int i = 0; i < 4; i++) m.M[i][i] = 1.0f;
>         return m;
>     }
> };
>
> Mat4 Mul(const Mat4 &a, const Mat4 &b) {
>     Mat4 r;
>     for (int i = 0; i < 4; i++)
>         for (int j = 0; j < 4; j++)
>             for (int k = 0; k < 4; k++)
>                 r.M[i][j] += a.M[i][k] * b.M[k][j];
>     return r;
> }
>
> Mat4 Transpose(const Mat4 &m) {
>     Mat4 r;
>     for (int i = 0; i < 4; i++)
>         for (int j = 0; j < 4; j++)
>             r.M[i][j] = m.M[j][i];
>     return r;
> }
>
> Vec3 GetPosition(const Mat4 &m) { return {m.M[3][0], m.M[3][1], m.M[3][2]}; }
>
> // 验证不可交换
> Mat4 T = Translation(10, 0, 0);
> Mat4 S = Scale(2);
> assert(GetPosition(Mul(T, S)).x == 20.0f);   // T*S：平移被缩放放大
> assert(GetPosition(Mul(S, T)).x == 10.0f);   // S*T：平移不变  ← 不同！
> ```

### 题 4.3 四元数转矩阵（★★）

**要求**：实现 `QuatToMatrix(q, pos, scale)`，并验证：
① 单位四元数 → 单位矩阵 ② 绕 Z 转 90° → 把 (1,0,0) 变成 (0,1,0)
③ 结果矩阵的三个行向量长度都是 1（正交性）。

> [!question]- 参考答案
> ```cpp
> struct Quat { float x = 0, y = 0, z = 0, w = 1; };
>
> Mat4 QuatToMatrix(const Quat &q, const Vec3 &pos, const Vec3 &scale) {
>     Mat4 m;
>     const float x2 = q.x + q.x, y2 = q.y + q.y, z2 = q.z + q.z;
>     const float xx2 = q.x*x2, yy2 = q.y*y2, zz2 = q.z*z2;
>     const float yz2 = q.y*z2, wx2 = q.w*x2;
>     const float xy2 = q.x*y2, wz2 = q.w*z2;
>     const float xz2 = q.x*z2, wy2 = q.w*y2;
>
>     m.M[0][0] = (1 - (yy2+zz2)) * scale.x;
>     m.M[1][1] = (1 - (xx2+zz2)) * scale.y;
>     m.M[2][2] = (1 - (xx2+yy2)) * scale.z;
>     m.M[2][1] = (yz2 - wx2) * scale.z;  m.M[1][2] = (yz2 + wx2) * scale.y;
>     m.M[1][0] = (xy2 - wz2) * scale.y;  m.M[0][1] = (xy2 + wz2) * scale.x;
>     m.M[2][0] = (xz2 + wy2) * scale.z;  m.M[0][2] = (xz2 - wy2) * scale.x;
>     m.M[3][0] = pos.x; m.M[3][1] = pos.y; m.M[3][2] = pos.z; m.M[3][3] = 1.0f;
>     return m;
> }
>
> // 验证
> Mat4 id = QuatToMatrix({0,0,0,1}, {}, {1,1,1});
> assert(fabsf(id.M[0][0] - 1) < 1e-6f && fabsf(id.M[0][1]) < 1e-6f);
>
> const float s = std::sin(45.0f * PI / 180.0f);      // 半角
> Mat4 rot = QuatToMatrix({0, 0, s, s}, {}, {1,1,1}); // 绕 Z 转 90°
> // 应用旋转: x' = M[0][0]*x + M[0][1]*y = 0*1 + (-1)*0 ... → (0,1,0)
> assert(fabsf(rot.M[0][0]) < 1e-5f && fabsf(rot.M[0][1] + 1.0f) < 1e-5f);
> ```

### 题 4.4 视图矩阵（★★）

**要求**：实现 `MakeViewMatrix(camPos, pitch, yaw, fov)`，
验证：① 正前方 10 米的点深度 = 10 ② 侧面的点深度 = 0 ③ 后方的点深度 < 0。

> [!question]- 参考答案
> ```cpp
> struct Camera { float M[16] = {}; };
>
> void MakeViewMatrix(Camera &c, const Vec3 &pos, float pitchDeg, float yawDeg, float fovDeg) {
>     const float pitch = pitchDeg * PI / 180.0f;
>     const float yaw   = yawDeg   * PI / 180.0f;
>     const float sp = std::sin(pitch), cp = std::cos(pitch);
>     const float sy = std::sin(yaw),   cy = std::cos(yaw);
>     float t = std::tan(fovDeg * 0.5f * PI / 180.0f);
>     if (std::fabs(t) < 1e-6f) t = 1e-6f;
>
>     // 深度行（forward）
>     c.M[3]  = cy*cp;  c.M[7]  = sy*cp;  c.M[11] = sp;
>     c.M[15] = -(pos.x*c.M[3] + pos.y*c.M[7] + pos.z*c.M[11]);
>
>     // right 行
>     c.M[0]  = -sy/t;  c.M[4]  = cy/t;   c.M[8]  = 0.0f;
>     c.M[12] = -(pos.x*(-sy) + pos.y*cy) / t;
>
>     // up 行
>     c.M[1]  = (-sp*cy)/t;  c.M[5] = (-sp*sy)/t;  c.M[9] = cp/t;
>     c.M[13] = -(pos.x*(-sp*cy) + pos.y*(-sp*sy) + pos.z*cp) / t;
> }
>
> float Depth(const Camera &c, const Vec3 &p) {
>     return c.M[3]*p.x + c.M[7]*p.y + c.M[11]*p.z + c.M[15];
> }
>
> // 验证（相机在原点朝 +X）
> Camera c;
> MakeViewMatrix(c, {0,0,0}, 0, 0, 90);
> assert(fabsf(Depth(c, {10,0,0}) - 10.0f) < 1e-4f);   // 前
> assert(fabsf(Depth(c, {0,10,0})) < 1e-4f);           // 侧
> assert(Depth(c, {-10,0,0}) < 0.0f);                  // 后
> ```

### 题 4.5 W2S 完整实现（★★★）

**要求**：基于上一题的视图矩阵，实现 `WorldToScreen`，
验证：正前方投到屏幕中心、左右对称、距离翻倍偏移减半、后方返回 false。

> [!question]- 参考答案
> ```cpp
> bool WorldToScreen(const Camera &c, const Vec3 &world, Vec2 &out,
>                    float halfW, float halfH) {
>     const float w = c.M[3]*world.x + c.M[7]*world.y + c.M[11]*world.z + c.M[15];
>     if (w < 0.001f) return false;                        // ★ 近平面
>
>     const float cx = c.M[0]*world.x + c.M[4]*world.y + c.M[8] *world.z + c.M[12];
>     const float cy = c.M[1]*world.x + c.M[5]*world.y + c.M[9] *world.z + c.M[13];
>
>     out.x = (cx / w + 1.0f) * halfW;
>     out.y = (1.0f - cy / w) * halfH;                     // ★ Y 翻转
>     return std::isfinite(out.x) && std::isfinite(out.y);  // ★ 有限性检查
> }
>
> // 验证
> Camera c;
> MakeViewMatrix(c, {0,0,0}, 0, 0, 90);
> Vec2 s;
> assert(WorldToScreen(c, {100,0,0}, s, 1080, 1200));
> assert(fabsf(s.x - 1080) < 0.5f && fabsf(s.y - 1200) < 0.5f);   // 正中心
>
> Vec2 l, r;
> WorldToScreen(c, {100,-50,0}, l, 1080, 1200);
> WorldToScreen(c, {100, 50,0}, r, 1080, 1200);
> assert(fabsf((1080 - l.x) - (r.x - 1080)) < 0.5f);              // 对称
>
> Vec2 n2, f2;
> WorldToScreen(c, {100,50,0}, n2, 1080, 1200);
> WorldToScreen(c, {200,50,0}, f2, 1080, 1200);
> assert(fabsf((n2.x - 1080) / (f2.x - 1080) - 2.0f) < 0.01f);    // 透视
>
> assert(!WorldToScreen(c, {-100,0,0}, s, 1080, 1200));            // 后方
> ```

### 题 4.6 角度归一化与插值（★）

**要求**：实现 `NormalizeAngle` / `AngleDelta` / `LerpAngle`，
验证"从 350° 到 10° 的最短路径是 +20°而不是 −340°"。

> [!question]- 参考答案
> ```cpp
> inline float NormalizeAngle(float deg) {
>     while (deg >   180.0f) deg -= 360.0f;
>     while (deg <= -180.0f) deg += 360.0f;
>     return deg;
> }
>
> inline float AngleDelta(float from, float to) { return NormalizeAngle(to - from); }
>
> inline float LerpAngle(float from, float to, float t) {
>     return NormalizeAngle(from + AngleDelta(from, to) * t);
> }
>
> // 验证
> assert(fabsf(AngleDelta(350, 10) - 20.0f) < 1e-4f);      // ★ 不是 -340
> assert(fabsf(AngleDelta(10, 350) + 20.0f) < 1e-4f);
> assert(fabsf(NormalizeAngle(190) + 170.0f) < 1e-4f);
> assert(fabsf(NormalizeAngle(-190) - 170.0f) < 1e-4f);
>
> // 插值走最短路
> assert(fabsf(LerpAngle(350, 10, 0.5f) - 0.0f) < 1e-4f); // 中点是 0°
> ```

### 题 4.7 万向节死锁演示（★★）

**要求**：写程序证明"Pitch=90° 时，改变 Roll 和改变 Yaw 产生相同结果"。
比较 `RotatorToMatrix({90, Y, 0})` 和 `RotatorToMatrix({90, 0, Y})`。

> [!question]- 参考答案
> ```cpp
> void DemoGimbalLock() {
>     const float Y = 30.0f;
>
>     // 只改 Yaw
>     Mat4 m1 = RotatorToMatrix({90.0f, Y, 0.0f});
>     // 只改 Roll
>     Mat4 m2 = RotatorToMatrix({90.0f, 0.0f, Y});
>
>     printf("仅改 Yaw   : M10=%.6f M11=%.6f\n", m1.M[1][0], m1.M[1][1]);
>     printf("仅改 Roll  : M10=%.6f M11=%.6f\n", m2.M[1][0], m2.M[1][1]);
>
>     // 理论上两者都等于 sin(±Y) / cos(±Y)
>     printf("sin(Y)=%.6f  cos(Y)=%.6f\n", sinf(Y*PI/180), cosf(Y*PI/180));
> }
> ```
> **预期输出**（数值接近）：
> ```
> 仅改 Yaw   : M10=-0.500000 M11=0.866025
> 仅改 Roll  : M10= 0.500000 M11=0.866025
> ```
> **观察**：`M11` 完全相同，`M10` 只是符号差（因为 `Yaw=+30` 与 `Roll=+30` 对应 `(Roll-Yaw)` 的相反数）。
> **结论**：Pitch=90° 时两个自由度塌缩为一个 —— **这就是万向节死锁**。

### 题 4.8 断言测试框架（★★）

**要求**：实现一个简易测试框架（`TEST` / `CHECK` / `CHECK_NEAR` / 统计报告），
并用它测试题 4.1 的向量库。

> [!question]- 参考答案
> ```cpp
> static int g_run = 0, g_failed = 0;
>
> #define TEST(name) do { g_run++; printf("[ RUN ] %s\n", name); } while (0)
>
> #define CHECK(cond) do {                                              \
>     if (!(cond)) {                                                    \
>         g_failed++;                                                   \
>         printf("  [FAIL] %s:%d  %s\n", __FILE__, __LINE__, #cond);     \
>     }                                                                 \
> } while (0)
>
> #define CHECK_NEAR(a, b, eps) do {                                    \
>     const float va_ = (a), vb_ = (b);                                 \
>     if (std::fabs(va_ - vb_) > (eps)) {                               \
>         g_failed++;                                                   \
>         printf("  [FAIL] %s:%d  %s=%.6f 期望 %.6f\n",                  \
>                __FILE__, __LINE__, #a, va_, vb_);                      \
>     }                                                                 \
> } while (0)
>
> int Report() {
>     printf("\n共 %d 项，失败 %d 项\n", g_run, g_failed);
>     return g_failed == 0 ? 0 : 1;
> }
> ```
> **关键点**：`#cond` 是**字符串化**——直接打印表达式原文，出错时一眼看出哪条挂了

### 题 4.9 数值稳定性（★★）

**要求**：写程序演示三种浮点误差：① 大数吃小数 ② 抵消误差 ③ 累积误差。
并给出每种情况的规避方法。

> [!question]- 参考答案
> ```cpp
> void DemoFloats() {
>     // ① 大数吃小数
>     float big = 1e7f, before = big;
>     big += 0.05f;
>     printf("1e7+0.05 变化: %s\n", (big == before) ? "无（被吃掉）" : "有");
>
>     // 规避：先算小量，再统一加
>     float sum_offset = 0.05f;
>     float result = 1e7f + sum_offset;
>
>     // ② 抵消误差
>     float a = 1.2345678f, b = 1.2345677f;
>     printf("a-b = %.10f (真值 1e-7)\n", a - b);
>     // 规避：用 double 做中间计算
>     double da = 1.2345678, db = 1.2345677;
>     printf("double: %.10f\n", da - db);
>
>     // ③ 累积误差
>     float fs = 0.0f;
>     for (int i = 0; i < 1000000; i++) fs += 0.1f;
>     printf("float 累加: %.2f (真值 100000)\n", fs);
>     double ds = 0.0;
>     for (int i = 0; i < 1000000; i++) ds += 0.1;
>     printf("double 累加: %.2f\n", ds);
>     // 规避：累加用 double；或定期重置（如本项目自瞄"到位后直接赋值"）
>
>     // ④ NaN/Inf
>     float nan = 0.0f / 0.0f;
>     printf("nan == nan ? %d   isfinite=%d\n", nan == nan, std::isfinite(nan));
> }
> ```

### 题 4.10 剔除统计（★）

**要求**：给一批点，统计各级剔除筛掉了多少：深度剔除 / 距离剔除 / 屏幕外剔除 / 最终可见。

> [!question]- 参考答案
> ```cpp
> struct CullStats { int total = 0, depth = 0, distance = 0, offscreen = 0, visible = 0; };
>
> CullStats CullPoints(const Camera &c, const std::vector<Vec3> &pts,
>                      float maxDist, float halfW, float halfH) {
>     CullStats s;
>     Vec2 scr;
>     for (const auto &p : pts) {
>         s.total++;
>         // ① 深度（最便宜）
>         const float w = c.M[3]*p.x + c.M[7]*p.y + c.M[11]*p.z + c.M[15];
>         if (w < 0.001f) { s.depth++; continue; }
>         // ② 距离平方（不开方）
>         if (p.LengthSq() > maxDist * maxDist) { s.distance++; continue; }
>         // ③ 屏幕外
>         if (!WorldToScreen(c, p, scr, halfW, halfH)) { s.depth++; continue; }
>         if (scr.x < 0 || scr.x > halfW*2 || scr.y < 0 || scr.y > halfH*2) {
>             s.offscreen++; continue;
>         }
>         s.visible++;
>     }
>     return s;
> }
> ```
> **关键点**：**剔除顺序按"单次成本"从低到高**——深度（几次乘加）→ 距离平方 → 投影 → 屏幕范围。

---

## 卷五 · 图形与界面（11 题）

### 题 5.1 CPU 光栅化（★★）

**要求**：不用 GPU，把三角形填充到字符数组并打印（ASCII 三角形）。
用重心坐标判断点是否在三角形内。

> [!question]- 参考答案
> ```c
> #define W 60
> #define H 24
> static char canvas[H][W];
>
> static int inside(float ax,float ay,float bx,float by,float cx,float cy,float px,float py) {
>     const float d1 = (px-ax)*(by-ay) - (py-ay)*(bx-ax);
>     const float d2 = (px-bx)*(cy-by) - (py-by)*(cx-bx);
>     const float d3 = (px-cx)*(ay-cy) - (py-cy)*(ax-cx);
>     const int has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
>     const int has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
>     return !(has_neg && has_pos);      // 三个叉积同号 = 在内部
> }
>
> int main(void) {
>     memset(canvas, ' ', sizeof(canvas));
>     const float ax=4, ay=2, bx=55, by=10, cx=20, cy=22;
>     for (int y = 0; y < H; y++)
>         for (int x = 0; x < W; x++)
>             if (inside(ax,ay,bx,by,cx,cy, x+0.5f, y+0.5f)) canvas[y][x] = '*';
>     for (int y = 0; y < H; y++) { canvas[y][W-1]=0; printf("%s\n", canvas[y]); }
>     return 0;
> }
> ```
> **关键点**：① 用"三个叉积同号"判断点在三角形内（比面积法更直观）② 采样像素中心 `x+0.5f`

### 题 5.2 正交投影矩阵（★★）

**要求**：构造把屏幕坐标 `[0,W]×[0,H]` 映射到 NDC `[-1,1]×[-1,1]` 的矩阵
（注意 Y 要翻转），并用它验证屏幕中心映射到 (0,0)。

> [!question]- 参考答案
> ```cpp
> void MakeOrtho(float L, float R, float T, float B, float m[4][4]) {
>     m[0][0] =  2.0f / (R - L);   m[0][1] = 0;                 m[0][2] = 0;  m[0][3] = 0;
>     m[1][0] =  0;                m[1][1] = 2.0f / (T - B);    m[1][2] = 0;  m[1][3] = 0;
>     m[2][0] =  0;                m[2][1] = 0;                 m[2][2] = 1;  m[2][3] = 0;
>     m[3][0] = (R + L) / (L - R);
>     m[3][1] = (T + B) / (B - T);
>     m[3][2] = 0;                 m[3][3] = 1;
> }
>
> // 验证：屏幕中心 → NDC 原点
> float m[4][4];
> MakeOrtho(0, 1080, 0, 2400, m);          // T < B，实现 Y 翻转
> const float cx = 540, cy = 1200;
> const float nx = m[0][0]*cx + m[3][0];
> const float ny = m[1][1]*cy + m[3][1];
> assert(fabsf(nx) < 1e-4f && fabsf(ny) < 1e-4f);
> ```
> **关键点**：`2/(T-B)` 中 `T < B`（屏幕 y 向下）→ 分母为负 → 自然实现 Y 翻转

### 题 5.3 帧率限速（★★）

**要求**：实现限帧函数，要求**不会因渲染耗时波动而漂移**。
对比"固定 sleep"和"累积对齐"的差异。

> [!question]- 参考答案
> ```cpp
> class FrameLimiter {
> public:
>     explicit FrameLimiter(int fps) {
>         period_ = std::chrono::duration_cast<std::chrono::steady_clock::duration>(
>             std::chrono::duration<double>(1.0 / fps));
>         next_ = std::chrono::steady_clock::now();
>     }
>
>     void Wait() {
>         const auto now = std::chrono::steady_clock::now();
>         if (now < next_) std::this_thread::sleep_until(next_);
>
>         next_ += period_;                                  // ★ 累积，不是重赋值
>         if (next_ <= std::chrono::steady_clock::now()) {   // 落后太多就重对齐
>             next_ = std::chrono::steady_clock::now() + period_;
>         }
>     }
> private:
>     std::chrono::steady_clock::duration period_;
>     std::chrono::steady_clock::time_point next_;
> };
>
> // 对比实验
> void Compare() {
>     // 方案 A：固定 sleep（会漂移）
>     auto t0 = std::chrono::steady_clock::now();
>     for (int i = 0; i < 60; i++) { busy_work(5ms); std::this_thread::sleep_for(16ms); }
>     auto a = std::chrono::steady_clock::now() - t0;    // ≈ 60 × 21ms = 1.26s（不是 1s）
>
>     // 方案 B：累积对齐（准确）
>     FrameLimiter lim(60);
>     t0 = std::chrono::steady_clock::now();
>     for (int i = 0; i < 60; i++) { busy_work(5ms); lim.Wait(); }
>     auto b = std::chrono::steady_clock::now() - t0;    // ≈ 1.0s
> }
> ```
> **关键点**：`next_ += period_` 而不是 `next_ = now + period`（后者会累积渲染耗时的误差）

### 题 5.4 ASCII 线框立方体（★★）

**要求**：用 `WorldToScreen` 把立方体的 8 个顶点投影到屏幕，
在终端画出 12 条边（用字符画线，只需近似）。

> [!question]- 参考答案
> ```cpp
> void DrawCubeAscii(const Camera &c, float size, int W, int H) {
>     // 8 个顶点
>     Vec3 v[8];
>     int k = 0;
>     for (int i = 0; i < 2; i++)
>         for (int j = 0; j < 2; j++)
>             for (int l = 0; l < 2; l++)
>                 v[k++] = {(float)(i?size:-size), (float)(j?size:-size), (float)(l?size:-size)};
>
>     // 投影
>     Vec2 s[8];
>     bool ok[8];
>     for (int i = 0; i < 8; i++) ok[i] = WorldToScreen(c, v[i], s[i], W/2.0f, H/2.0f);
>
>     // 画布
>     std::vector<std::string> canvas(H, std::string(W, ' '));
>     auto line = [&](int x0, int y0, int x1, int y1) {
>         const int n = std::max(std::abs(x1-x0), std::abs(y1-y0)) + 1;
>         for (int i = 0; i < n; i++) {
>             const int x = x0 + (x1-x0)*i/n, y = y0 + (y1-y0)*i/n;
>             if (x >= 0 && x < W && y >= 0 && y < H) canvas[y][x] = '*';
>         }
>     };
>
>     // 12 条边（顶点索引按二进制位）
>     const int edges[12][2] = {
>         {0,1},{2,3},{4,5},{6,7},   // 沿 x 方向的 4 条
>         {0,2},{1,3},{4,6},{5,7},   // 沿 y 方向
>         {0,4},{1,5},{2,6},{3,7}    // 沿 z 方向
>     };
>     for (auto &e : edges)
>         if (ok[e[0]] && ok[e[1]]) line((int)s[e[0]].x, (int)s[e[0]].y,
>                                        (int)s[e[1]].x, (int)s[e[1]].y);
>
>     for (const auto &row : canvas) printf("%s\n", row.c_str());
> }
> ```
> **关键点**：① 顶点索引用二进制位编码（`i/j/l` 三个循环）② 12 条边按"每个轴向上 4 条"组织 ③ 任一端投影失败就跳过整条边

### 题 5.5 分层降级设计（★★）

**要求**：设计一个"渲染后端"抽象，要求：
① 有 Vulkan 实现 ② 有心跳失败时的降级路径 ③ 调用方不关心具体用哪个。

> [!question]- 参考答案
> ```cpp
> class IRenderer {
> public:
>     virtual ~IRenderer() = default;
>     virtual bool Init(ANativeWindow *win, int w, int h) = 0;
>     virtual void BeginFrame() = 0;
>     virtual void EndFrame() = 0;
>     virtual const char *Name() const = 0;
> };
>
> class VulkanRenderer : public IRenderer { /* ... */ };
> class GLESRenderer   : public IRenderer { /* ... */ };   // 降级方案
>
> // 工厂：按可用性选
> std::unique_ptr<IRenderer> CreateRenderer() {
>     if (VulkanAvailable()) return std::make_unique<VulkanRenderer>();
>     LOGW("Vulkan 不可用，降级到 GLES");
>     return std::make_unique<GLESRenderer>();
> }
>
> // 调用方
> auto r = CreateRenderer();
> LOGI("当前渲染后端: %s", r->Name());      // 不关心是谁
> ```
> **关键点**：① 抽象接口 + 工厂（第 11、80 章）② **降级要打日志**——否则用户不知道自己在用降级方案 ③ 返回 `unique_ptr` 表达所有权

### 题 5.6 触摸坐标变换（★★）

**要求**：实现"触摸坐标 → 屏幕坐标"的变换，支持 4 种屏幕方向。

> [!question]- 参考答案
> ```cpp
> // touch 是触摸屏原始坐标，maxX/maxY 是触摸屏最大值
> Vec2 TouchToScreen(int tx, int ty, int maxX, int maxY,
>                    int screenW, int screenH, int orientation) {
>     // 先归一化
>     const float nx = (float)tx / maxX;      // 0~1
>     const float ny = (float)ty / maxY;
>
>     switch (orientation) {
>         case 0: return {nx * screenW,        ny * screenH       };   // 竖屏
>         case 1: return {ny * screenW,        (1-nx) * screenH   };   // 横屏 90°
>         case 2: return {(1-nx) * screenW,    (1-ny) * screenH   };   // 倒竖
>         case 3: return {(1-ny) * screenW,    nx * screenH       };   // 横屏 270°
>         default: return {nx * screenW,        ny * screenH       };
>     }
> }
> ```
> **关键点**：① 先归一化再做方向变换（避免在多个分支里重复除法）② 每种方向就是"交换 x/y + 翻转某个轴" ③ **orientation 必须与系统给的值一致**（第 69 章）

### 题 5.7 性能分段计时（★）

**要求**：实现一个 RAII 计时器，自动累计各段耗时并输出报告。

> [!question]- 參考答案
> ```cpp
> class ProfileScope {
> public:
>     explicit ProfileScope(const char *name)
>         : name_(name), start_(std::chrono::steady_clock::now()) {}
>
>     ~ProfileScope() {
>         const auto ms = std::chrono::duration<float, std::milli>(
>             std::chrono::steady_clock::now() - start_).count();
>         auto &slot = GetTable()[name_];
>         slot.total += ms;
>         slot.calls++;
>     }
>
>     static void Report() {
>         printf("%-20s %10s %8s %10s\n", "段", "总(ms)", "次数", "平均(ms)");
>         for (const auto &[name, s] : GetTable()) {
>             printf("%-20s %10.2f %8d %10.3f\n",
>                    name.c_str(), s.total, s.calls, s.total / s.calls);
>         }
>         GetTable().clear();
>     }
> private:
>     struct Slot { float total = 0; int calls = 0; };
>     static std::map<std::string, Slot> &GetTable() {
>         static std::map<std::string, Slot> t;
>         return t;
>     }
>     const char *name_;
>     std::chrono::steady_clock::time_point start_;
> };
>
> // 用法
> {
>     ProfileScope s("ReadMemory");
>     UpdateGameData();
> }
> {
>     ProfileScope s("DrawESP");
>     DrawPlayer(draw);
> }
> ProfileScope::Report();
> ```
> **关键点**：① RAII——离开作用域自动记录 ② 用 `static` 局部变量存表格（不用全局变量）③ 结构化绑定遍历（`const auto &[name, s]`）

### 题 5.8 中文字体加载（★）

**要求**：把 TTF 字体以字节数组形式内嵌，并用 ImGui 加载（要求中文可显示）。

> [!question]- 参考答案
> ```bash
> # ① 用 xxd 把字体转成 C++ 数组
> xxd -i heiti.ttf > heiti_ttf.cpp
> # 生成: const unsigned char heiti_ttf[] = { 0x00, 0x01, ... };
> #       const unsigned int  heiti_ttf_len = 12345678;
> ```
> ```cpp
> // ② 加载（关键在 FontDataOwnedByAtlas）
> static ImFont *LoadChineseFont(float sizePx) {
>     ImGuiIO &io = ImGui::GetIO();
>     ImFontConfig cfg;
>     cfg.FontDataOwnedByAtlas = false;      // ★ 数据是静态数组，别让 ImGui free
>     cfg.SizePixels = sizePx;
>     cfg.OversampleH = 2;
>     cfg.OversampleV = 1;
>
>     ImFont *f = io.Fonts->AddFontFromMemoryTTF(
>         const_cast<unsigned char *>(heiti_ttf),
>         (int)heiti_ttf_len,
>         sizePx,
>         &cfg,
>         io.Fonts->GetGlyphRangesChineseFull());   // ★ 中文范围
>     if (f) io.FontDefault = f;
>     return f;
> }
> ```
> **关键点**：① `FontDataOwnedByAtlas = false` 必需（否则 free 静态内存崩溃）② `GetGlyphRangesChineseFull()` 覆盖常用汉字 ③ 字体数组约占 2.9 MB 体积，可用 `pyftsubset` 进一步裁剪

### 题 5.9 Vulkan 对象层级图（★）

**要求**：不写代码，画出 Vulkan 五个核心对象的关系图，并说明创建顺序与销毁顺序。

> [!question]- 参考答案
> ```
> VkInstance（全局，进程一个）
>   └─ VkPhysicalDevice（GPU，可能多个）
>        └─ VkDevice（逻辑设备，我们操作它）
>             ├─ VkQueue（提交命令的通道）
>             └─ VkCommandPool → VkCommandBuffer（具体任务清单）
> ```
> **创建顺序**：Instance → PhysicalDevice（枚举，不创建）→ Device → Queue（从 Device 取，不创建）→ CommandPool → CommandBuffer
> **销毁顺序**：与创建相反
> **关键点**：① `VkPhysicalDevice` 是枚举出来的，不需要创建 ② `VkQueue` 从 Device 取，也不需要创建

### 题 5.10 跨版本符号侦察器（★★）

**要求**：用 `dlopen`+`dlsym` 检查当前设备上有哪些 `libgui` 符号，输出"符号名 + 是否存在"的表格。

> [!question]- 参考答案
> ```cpp
> #include <dlfcn.h>
> #include <cstdio>
>
> int main(void) {
>     void *libgui = dlopen("libgui.so", RTLD_NOW);
>     if (!libgui) { printf("dlopen 失败: %s\n", dlerror()); return 1; }
>
>     const char *symbols[] = {
>         "_ZN7android14SurfaceControl10createTypeERKNS_2spINS_7IBinderEEERKNS_7String8Ejj",
>         "_ZN7android21SurfaceComposerClient13createSurfaceERKNS_7String8EjjiiRKNS_2spINS_7IBinderEEE",
>         "_ZN7android14SurfaceControl7setLayerEi",
>         nullptr
>     };
>
>     for (int i = 0; symbols[i]; i++) {
>         void *p = dlsym(libgui, symbols[i]);
>         printf("%-80s %s\n", symbols[i], p ? "存在" : "缺失");
>     }
>     dlclose(libgui);
>     return 0;
> }
> ```
> **关键点**：① `dlsym` 用 **mangled name**（`_ZN...`），不是 C++ 源名 ② 不同 Android 版本符号名可能不同 ③ 用 `nm -D --demangle libgui.so | grep createSurface` 可查真实符号名

### 题 5.11 overlay 最小骨架（★★★）

**要求**：写出一个最小透明覆盖层的骨架（不需要完整 Vulkan 渲染），
包含：① 创建透明图层 ② 设置 z-order 最高 ③ 用 `dumpsys` 验证图层存在。

> [!question]- 参考答案
> ```cpp
> // overlay.cpp —— 最小骨架（不含 Vulkan 渲染）
> #include <gui/SurfaceComposerClient.h>
> #include <gui/Surface.h>
> #include <ui/DisplayInfo.h>
>
> using namespace android;
>
> int main(void) {
>     // 1. 创建 SurfaceComposerClient（连接 SurfaceFlinger）
>     sp<SurfaceComposerClient> client = new SurfaceComposerClient();
>     if (client->initCheck() != OK) { printf("连接 SF 失败\n"); return 1; }
>
>     // 2. 获取主屏幕信息
>     DisplayInfo dinfo;
>     sp<IBinder> display = SurfaceComposerClient::getInternalDisplayToken();
>     SurfaceComposerClient::getDisplayInfo(display, &dinfo);
>
>     // 3. 创建透明图层（关键：RGBA_8888 格式）
>     sp<SurfaceControl> sc = client->createSurface(
>         String8("myOverlay"), dinfo.w, dinfo.h,
>         PIXEL_FORMAT_RGBA_8888, 0);
>
>     // 4. 设置 z-order 最高
>     SurfaceComposerClient::Transaction t;
>     t.setLayer(sc, INT_MAX - 100)
>      .setPosition(sc, 0, 0)
>      .show(sc)
>      .apply();
>
>     printf("图层已创建，用 dumpsys SurfaceFlinger | grep myOverlay 验证\n");
>     sleep(60);
>     return 0;
> }
> ```
> **验证**：`adb shell dumpsys SurfaceFlinger | grep -A2 myOverlay`
> **关键点**：① 图层格式必须 `RGBA_8888`（含 alpha）② z-order 用 `INT_MAX` 附近 ③ 透明靠"清屏 alpha=0"而非 `compositeAlpha`（第 59 章）

---

## 卷六 · 跨进程内存（8 题）

### 题 6.1 跨进程读取器（★★）

**要求**：实现 `MemReader`，支持读整数、读结构体、读字符串（限长）、
判断进程存活。用自己写的 target 进程测试。

> [!question]- 参考答案
> ```cpp
> class MemReader {
> public:
>     explicit MemReader(pid_t pid) : pid_(pid) {}
>
>     int Read(uint64_t addr, void *buf, size_t size) const {
>         if (!buf || !size || pid_ <= 0) return -1;
>         struct iovec liov{buf, size}, riov{(void *)addr, size};
>         size_t done = 0;
>         while (done < size) {
>             const ssize_t n = process_vm_readv(pid_, &liov, 1, &riov, 1, 0);
>             if (n <= 0) return done > 0 ? (int)done : -1;   // ★ 保留部分成功
>             done += (size_t)n;
>             liov.iov_base = (char *)buf + done;  liov.iov_len = size - done;
>             riov.iov_base = (char *)addr + done; riov.iov_len = size - done;
>         }
>         return (int)done;
>     }
>
>     template <typename T> T Read(uint64_t addr) const {
>         T v{};
>         if (Read(addr, &v, sizeof(T)) != (int)sizeof(T)) return T{};   // 失败返回零值
>         return v;
>     }
>
>     std::string ReadString(uint64_t addr, size_t maxLen = 128) const {
>         if (!addr) return {};
>         std::vector<char> buf(maxLen + 1, 0);
>         if (Read(addr, buf.data(), maxLen) <= 0) return {};
>         buf[maxLen] = '\0';                     // ★ 保证终止
>         return std::string(buf.data());
>     }
>
>     bool IsAlive() const {
>         char path[64];
>         snprintf(path, sizeof(path), "/proc/%d", pid_);
>         return access(path, F_OK) == 0;
>     }
> private:
>     pid_t pid_;
> };
> ```

### 题 6.2 批量读（★★）

**要求**：实现 `ReadBatch`：一次 `process_vm_readv` 读多个分散地址。
对比"逐个读 100 次"与"批量读 1 次"的耗时。

> [!question]- 参考答案
> ```cpp
> struct ReadReq { uint64_t remote; void *local; size_t size; };
>
> bool ReadBatch(pid_t pid, const std::vector<ReadReq> &reqs) {
>     if (reqs.empty() || reqs.size() > IOV_MAX) return false;
>
>     std::vector<struct iovec> liov(reqs.size()), riov(reqs.size());
>     for (size_t i = 0; i < reqs.size(); i++) {
>         liov[i].iov_base = reqs[i].local;
>         liov[i].iov_len  = reqs[i].size;
>         riov[i].iov_base = (void *)reqs[i].remote;
>         riov[i].iov_len  = reqs[i].size;
>     }
>     return process_vm_readv(pid, liov.data(), liov.size(),
>                                   riov.data(), riov.size(), 0) > 0;
> }
>
> // 对比实验
> void Benchmark(pid_t pid, uint64_t base, int n) {
>     std::vector<uint64_t> vals(n);
>
>     auto t0 = std::chrono::steady_clock::now();
>     for (int i = 0; i < n; i++)
>         process_vm_readv(pid, ...);              // 逐个读
>     auto t1 = std::chrono::steady_clock::now();
>
>     std::vector<ReadReq> reqs;
>     for (int i = 0; i < n; i++) reqs.push_back({base + i*8, &vals[i], 8});
>     auto t2 = std::chrono::steady_clock::now();
>     ReadBatch(pid, reqs);                        // 批量读
>     auto t3 = std::chrono::steady_clock::now();
>
>     printf("逐个: %.2f ms   批量: %.2f ms   加速: %.1f×\n",
>            ms(t0,t1), ms(t2,t3), ms(t0,t1)/ms(t2,t3));
> }
> ```
> **预期**：`n=100` 时批量快 **20~50 倍**（因为主要开销是系统调用本身，不是数据量）

### 题 6.3 memview 工具（★★）

**要求**：实现命令行工具 `memview <pid> [模块名]`：
输出进程的所有模块及基址、可扫描区域统计、指定模块的基址。

> [!question]- 参考答案
> **完整代码见 [[第76章-proc-pid-maps与模块基址]] 的「memview 主程序」一节**——
> 它由 `struct MapRegion` + 三个函数 + `main` 组成，可编译运行。
> 三个核心函数：
> ```cpp
> std::vector<MapRegion> ParseMaps(const std::string &text);       // 解析 maps
> uint64_t FindModuleBase(const std::vector<MapRegion>&, const std::string&);  // 找基址
> std::vector<std::pair<uint64_t,uint64_t>> GetScannableRegions(   // 筛可读写
>     const std::vector<MapRegion>&);
> ```
> **输出示例**：
> ```
> === 模块 ===
> 0x0000007a1c000000 0x0000007a1d000000 r-xp /system/lib64/libc.so
> ...
> === 可扫描区域: 45 个，共 128.4 MB ===
> 模块 libc.so 基址 = 0x0000007a1c000000
> ```
>
> **编译**：`g++ -std=c++17 memview.cpp -o memview`（用 `g++` 不是 `gcc`）
> **运行**：`./memview <pid> libc.so`

### 题 6.4 指针链跟随（跨进程版）（★★）

**要求**：实现 `FollowChain(pid, base, offsets)`，逐级解引用。
要求每跳判空，并返回"在第几跳失败"。

> [!question]- 参考答案
> ```cpp
> struct ChainResult {
>     bool     ok = false;
>     uint64_t value = 0;
>     int      failedAt = -1;             // 哪一跳失败（-1 表示没失败）
> };
>
> ChainResult FollowChain(const MemReader &mem, uint64_t base,
>                         std::initializer_list<uint64_t> offsets) {
>     ChainResult r;
>     uint64_t cur = base;
>     int step = 0;
>
>     for (const uint64_t off : offsets) {
>         if (!IsValidPtr(cur)) { r.failedAt = step; return r; }
>         cur = mem.Read<uint64_t>(cur + off);
>         if (cur == 0) { r.failedAt = step; return r; }   // ★ 链断
>         step++;
>     }
>     r.ok = true; r.value = cur;
>     return r;
> }
>
> // 用法：游戏更新后，failedAt 直接告诉你是哪一跳断了
> auto res = FollowChain(mem, libUE4, {0xB3EC650, 0x30, 0x98});
> if (!res.ok) LOGE("链在第 %d 跳断了（偏移 ±%d）", res.failedAt, res.failedAt);
> ```
> **关键点**：`failedAt` 字段是**排错的关键**——比"什么都不工作"有用得多

### 题 6.5 共享内存 RPC（★★★）

**要求**：实现"客户端-服务端"共享内存通信：
客户端发请求（读某个地址的数据），服务端处理并回填。

> [!question]- 参考答案
> ```cpp
> // common.h
> struct ShmRequest {
>     volatile bool kernel = false;      // 客户端置位：有新请求
>     volatile bool user   = false;      // 服务端置位：处理完成
>     volatile int  op     = 0;          // 0=读 1=写 99=退出
>     volatile int  status = 0;
>     uint64_t addr = 0;
>     int      size = 0;
>     uint8_t  buffer[4096] = {};
> };
> #define SHM_NAME "/klbq_rpc_demo"
>
> // 客户端
> bool CommitAndWait(ShmRequest *req, uint64_t addr, void *buf, int size, bool isWrite) {
>     if (isWrite) memcpy(req->buffer, buf, size);
>     req->op = isWrite ? 1 : 0;
>     req->addr = addr; req->size = size; req->status = 0;
>
>     req->kernel = true;                              // ★ 提交
>     while (!req->user) asm volatile("yield");        // ★ 等待（无超时是缺陷，见附录U）
>     req->user = false;                               // ★ 消费
>
>     if (req->status <= 0) return false;
>     if (!isWrite) memcpy(buf, req->buffer, req->status);
>     return true;
> }
>
> // 服务端
> void ServerLoop(ShmRequest *req, uint8_t *fakeMemory, size_t memSize) {
>     while (true) {
>         if (!req->kernel) { usleep(50); continue; }   // ★ 让出 CPU
>         switch (req->op) {
>             case 0:  // 读
>                 if (req->addr + req->size <= memSize) {
>                     memcpy(req->buffer, fakeMemory + req->addr, req->size);
>                     req->status = req->size;
>                 } else req->status = -1;
>                 break;
>             case 1:  // 写
>                 if (req->addr + req->size <= memSize) {
>                     memcpy(fakeMemory + req->addr, req->buffer, req->size);
>                     req->status = req->size;
>                 } else req->status = -1;
>                 break;
>             case 99: return;
>             default: req->status = -2; break;
>         }
>         req->kernel = false;
>         req->user = true;                             // ★ 通知完成
>     }
> }
> ```
> **关键点**：① 四个 `volatile` 字段的分工 ② `CommitAndWait` 的"置位→等待→消费"三步 ③ 服务端要主动让出 CPU ④ **生产环境要加超时**（附录 U 的 S1）

### 题 6.6 驱动抽象层（★★）

**要求**：设计 `IDriver` 抽象接口，支持两个后端（A：模拟；B：`process_vm`），
要求业务代码不关心用哪个。

> [!question]- 参考答案
> ```cpp
> class IDriver {
> public:
>     virtual ~IDriver() = default;
>     virtual int Read(uint64_t addr, void *buf, size_t size) = 0;
>     virtual int Write(uint64_t addr, void *buf, size_t size) = 0;
>     virtual int GetGlobalPid() = 0;
>     virtual void SetGlobalPid(int pid) = 0;
>
>     // ★ 非虚的便捷封装，所有后端共享
>     template <typename T> T Read(uint64_t addr) {
>         T v{};
>         if (Read(addr, &v, sizeof(T)) <= 0) v = T{};
>         return v;
>     }
>     std::string ReadString(uint64_t addr, size_t maxLen = 128) {
>         std::vector<char> buf(maxLen + 1, 0);
>         if (Read(addr, buf.data(), maxLen) <= 0) return {};
>         buf[maxLen] = '\0';
>         return std::string(buf.data());
>     }
> };
>
> class FakeDriver : public IDriver { /* 内存数组模拟 */ };
> class ProcVmDriver : public IDriver { /* process_vm_readv */ };
>
> // 业务代码
> void BusinessLogic(IDriver *dr) {
>     const uint64_t world = dr->Read<uint64_t>(0xB3EC650);   // 不关心后端
>     LOGI("world=0x%llX", (unsigned long long)world);
> }
> ```
> **关键点**：① 只有真正需要多态的方法才是虚的 ② 模板方法**定义在基类里**（所有后端自动获得）③ 加新后端不用改业务代码

### 题 6.7 合法性过滤（★★）

**要求**：实现 `PtrValidator`，含范围检查、对齐检查、maps 校验（二分查找），
并统计各原因的拒绝次数。

> [!question]- 参考答案
> ```cpp
> class PtrValidator {
> public:
>     struct Stats {
>         uint64_t total = 0, ok = 0, bad_range = 0, bad_align = 0, unmapped = 0;
>     };
>
>     void RefreshRegions(pid_t pid) {
>         regions_ = GetScannableRegions(ParseMaps(ReadFile(pid)));
>         std::sort(regions_.begin(), regions_.end());
>     }
>
>     bool Check(uint64_t addr) {
>         stat_.total++;
>         if (addr < 0x10000000ULL || addr > 0x10000000000ULL) { stat_.bad_range++; return false; }
>         if (addr % 4 != 0)                                   { stat_.bad_align++; return false; }
>         if (!regions_.empty() && !Contains(addr))            { stat_.unmapped++;  return false; }
>         stat_.ok++;
>         return true;
>     }
>
>     const Stats &stat() const { return stat_; }
>     void ResetStats() { stat_ = {}; }
>
> private:
>     bool Contains(uint64_t addr) const {
>         // 二分查找：找到最后一个 start <= addr 的区间
>         auto it = std::upper_bound(regions_.begin(), regions_.end(), addr,
>             [](uint64_t a, const auto &r) { return a < r.first; });
>         if (it == regions_.begin()) return false;
>         --it;
>         return addr < it->second;
>     }
>
>     std::vector<std::pair<uint64_t, uint64_t>> regions_;
>     Stats stat_{};
> };
> ```
> **诊断价值**：
> | 现象 | 说明 |
> |---|---|
> | `bad_range` 占比高 | 偏移错了（读到 0 或垃圾） |
> | `unmapped` 占比高 | 对象被释放了，或 maps 缓存过期 |
> | 全通过但数据不对 | 偏移对但**字段含义**变了 |

### 题 6.8 三种锁的性能对比（★★）

**要求**：写 benchmark 比较"无锁（错）/互斥锁 / 自旋锁 / 原子"四种自增
在**单线程**和**多线程**下的表现。

> [!question]- 参考答案
> ```cpp
> constexpr int N = 1000000;
> static std::atomic<unsigned char> spin_flag{0};
>
> void Bench(const char *name, void (*fn)()) {
>     const auto t0 = std::chrono::steady_clock::now();
>     fn();
>     const auto ms = std::chrono::duration<double, std::milli>(
>         std::chrono::steady_clock::now() - t0).count();
>     printf("%-12s %8.2f ms\n", name, ms);
> }
>
> int main() {
>     static volatile int c1 = 0;
>     static int c2 = 0; static std::mutex m;
>     static int c3 = 0;
>     static std::atomic<int> c4{0};
>
>     Bench("无锁", []{ for (int i=0;i<N;i++) c1++; });
>     Bench("互斥锁", []{ for (int i=0;i<N;i++) { std::lock_guard<std::mutex> l(m); c2++; } });
>     Bench("自旋锁", []{
>         for (int i=0;i<N;i++) {
>             while (spin_flag.exchange(1)) { /* spin */ }
>             c3++; spin_flag.store(0);
>         }
>     });
>     Bench("原子", []{ for (int i=0;i<N;i++) c4++; });
> }
> ```
> **典型结果（单线程）**：
> | 方式 | 耗时 | 说明 |
> |---|---|---|
> | 无锁 | ~2.5 ms | 最快但**结果错**（多线程下） |
> | 互斥锁 | ~25 ms | 每次加锁有系统调用开销 |
> | 自旋锁 | ~12 ms | 无系统调用，但持续占 CPU |
> | 原子 | ~6 ms | **最快的正确方案** |
>
> **结论**：能用原子就用原子；临界区极短用自旋锁；一般情况用互斥锁。

---

## 卷七 · 引擎数据模型（8 题）

### 题 7.1 demo 引擎（★★★）

**要求**：实现一个最小游戏引擎：
- `Actor`（含 `nameId`、`components` 链表）
- `TransformComponent`（位置）
- `MeshComponent`（骨骼数组）
- `Level`（Actor 指针数组 + count）
- `World`（指向 Level）
- `NameTable`（FName 池）

要求所有结构**有明确的偏移**，能被打分程序读取。

> [!question]- 参考答案
> 完整实现见 [[附录I-demo引擎完整源码]]（约 400 行，可直接编译）。
> 核心结构：
> ```cpp
> struct Actor {
>     uint64_t vtable;      // 0x00
>     uint32_t objectId;    // 0x08
>     uint32_t nameId;      // 0x0C  ← 打分程序读这个
>     uint64_t components;  // 0x10
>     uint64_t next;        // 0x18
> };
>
> struct Level {
>     uint64_t actors;        // 0x00  ← 数组首地址
>     int32_t  actorCount;    // 0x08
>     int32_t  actorCapacity; // 0x0C
> };
>
> struct World {
>     uint64_t level;           // 0x00  ← 入口
>     uint64_t playerController;
>     uint64_t gameState;
> };
> ```
> **关键点**：① 结构体要有明确的对齐（第 07 章）② 提供 `PrintTruth()` 打印真值供对照 ③ 让物体持续移动，验证"数据在变化"

### 题 7.2 对象数组遍历（★★）

**要求**：从 `World` 出发，遍历 `Level` 里所有 `Actor`，
输出每个对象的地址、名字、世界坐标。要求**一次读完整个指针数组**。

> [!question]- 参考答案
> ```cpp
> struct ActorInfo {
>     uint64_t address = 0;
>     uint32_t nameId = 0;
>     std::string name;
>     Vector3A position;
> };
>
> std::vector<ActorInfo> CollectActors(const MemReader &mem, uint64_t worldAddr) {
>     std::vector<ActorInfo> out;
>
>     // ① 世界 → 关卡
>     const uint64_t level = mem.Read<uint64_t>(worldAddr + 0x00);
>     if (!IsValidPtr(level)) return out;
>
>     // ② 数组与计数
>     const uint64_t arr   = mem.Read<uint64_t>(level + 0x00);
>     const int32_t  count = mem.Read<int32_t>(level + 0x08);
>     if (!IsValidPtr(arr) || count <= 0 || count > 10000) return out;   // ★ 上界
>
>     // ③ 一次读完整个指针数组
>     std::vector<uint64_t> ptrs(count);
>     if (mem.Read(arr, ptrs.data(), count * 8) != count * 8) return out;
>
>     // ④ 逐个读字段（可进一步批量化）
>     out.reserve(count);
>     for (const uint64_t p : ptrs) {
>         if (!IsValidPtr(p)) continue;                       // ★ 每个指针都校验
>         ActorInfo a;
>         a.address = p;
>         a.nameId  = mem.Read<uint32_t>(p + 0x0C);
>         a.name    = GetNameById(mem, GName, a.nameId);
>         const uint64_t root = mem.Read<uint64_t>(p + 0x168);
>         if (IsValidPtr(root)) {
>             mem.Read(root + 0x1F0, &a.position, sizeof(a.position));
>         }
>         out.push_back(a);
>     }
>     return out;
> }
> ```

### 题 7.3 FName 解析（★★）

**要求**：实现 `GetNameById(nameId)`，从 FName 池解析出字符串。
要求①正确拆位 ②检查长度上界 ③支持宽字符转 UTF-8。

> [!question]- 参考答案
> ```cpp
> static void AppendUtf8(std::string &out, char32_t c) {
>     if (c < 0x80) {
>         out += (char)c;
>     } else if (c < 0x800) {
>         out += (char)(0xC0 | (c >> 6));  out += (char)(0x80 | (c & 0x3F));
>     } else if (c < 0x10000) {
>         out += (char)(0xE0 | (c >> 12));
>         out += (char)(0x80 | ((c >> 6) & 0x3F));
>         out += (char)(0x80 | (c & 0x3F));
>     } else {
>         out += (char)(0xF0 | (c >> 18));
>         out += (char)(0x80 | ((c >> 12) & 0x3F));
>         out += (char)(0x80 | ((c >> 6) & 0x3F));
>         out += (char)(0x80 | (c & 0x3F));
>     }
> }
>
> std::string GetNameById(const MemReader &mem, uint64_t gnamePool, uint32_t nameId) {
>     const uint32_t blockIdx = nameId >> 16;         // ★ 高 16 位
>     const uint32_t blockOff = nameId & 0xFFFF;      // ★ 低 16 位
>
>     const uint64_t blockAddr = mem.Read<uint64_t>(gnamePool + 0x40 + blockIdx * 8);
>     if (!blockAddr) return {};
>
>     const uint64_t entryAddr = blockAddr + blockOff * 2;   // ★ ×2 还原字节偏移
>     uint16_t header = mem.Read<uint16_t>(entryAddr);
>
>     const uint32_t len = header >> 6;                // ★ 高 10 位是长度
>     const bool isWide  = header & 1;                 // ★ 最低位是标志
>     if (len == 0 || len > 1024) return {};           // ★ 上界检查
>
>     if (isWide) {
>         std::u32string u32(len, 0);
>         if (!mem.Read(entryAddr + 2, u32.data(), len * 4)) return {};
>         std::string s; s.reserve(len * 3);
>         for (char32_t c : u32) AppendUtf8(s, c);
>         return s;
>     }
>     std::string s(len, 0);
>     if (!mem.Read(entryAddr + 2, s.data(), len)) return {};
>     return s;
> }
> ```

### 题 7.4 读骨骼世界坐标（★★★）

**要求**：读出一根骨骼的世界坐标。要求：
① 一次读完整个骨骼数组（不是逐根读）
② 正确应用 `BONE_STRIDE` 和两级变换
③ 用 `isfinite` 校验结果

> [!question]- 参考答案
> ```cpp
> struct BoneTransform {          // 40 字节
>     Quat  rotation;             // 0x00
>     Vec3  translation;          // 0x10
>     Vec3  scale;                // 0x1C
> };
>
> bool ReadBoneWorld(const MemReader &mem, uint64_t meshAddr,
>                    const Matrix &c2w, int boneIndex, Vec3 &out) {
>     const uint64_t boneArray = mem.Read<uint64_t>(meshAddr + 0x578);
>     if (!IsValidPtr(boneArray)) return false;
>
>     // ★ 一次读完整个骨骼数组（56 × 48 = 2688 字节）
>     constexpr int kMaxBones = 256;
>     const size_t total = kMaxBones * BONE_STRIDE;
>     std::vector<uint8_t> raw(total);
>     if (mem.Read(boneArray, raw.data(), total) <= 0) return false;
>
>     if (boneIndex < 0 || boneIndex >= kMaxBones) return false;
>
>     // 从本地缓冲解析（零额外 I/O）
>     BoneTransform bt;
>     memcpy(&bt, raw.data() + boneIndex * BONE_STRIDE, sizeof(bt));
>
>     const Matrix local = TransformToMatrix(bt);        // 局部
>     const Matrix world = MatrixMulti(local, c2w);      // × 组件到世界
>     out = MatrixGetPosition(world);
>
>     return std::isfinite(out.x) && std::isfinite(out.y) && std::isfinite(out.z);
> }
> ```
> **关键点**：① **一次读整块**（第 81 章的批量优化：56 次 I/O → 1 次）② `BONE_STRIDE = 48 ≠ sizeof(BoneTransform) = 40`（第 07、86 章）③ 索引边界检查 ④ `isfinite` 防 NaN

### 题 7.5 偏移自检（★★）

**要求**：实现 `SelfTest`，逐级验证偏移是否正确，
每级失败返回**明确的错误原因**（而不是笼统的 false）。

> [!question]- 参考答案
> ```cpp
> enum class OffsetStatus {
>     Ok, NoProcess, BadBase, BadWorld, BadLevel, BadCount, BadCamera, BadName
> };
>
> OffsetStatus SelfTest(const MemReader &mem, uint64_t base) {
>     if (mem.GetPid() <= 0)          return OffsetStatus::NoProcess;
>     if (!IsValidPtr(base))          return OffsetStatus::BadBase;
>
>     const uint64_t world = mem.Read<uint64_t>(base + UWORLD);
>     if (!IsValidPtr(world))         return OffsetStatus::BadWorld;
>
>     const uint64_t level = mem.Read<uint64_t>(world + UWORLD_LEVEL);
>     if (!IsValidPtr(level))         return OffsetStatus::BadLevel;
>
>     const int32_t count = mem.Read<int32_t>(level + ULEVEL_COUNT);
>     if (count <= 0 || count > 10000) return OffsetStatus::BadCount;
>
>     const uint64_t pc = /* 五级链 */;
>     const uint64_t cam = mem.Read<uint64_t>(pc + PC_CAMERA);
>     if (!IsValidPtr(cam))           return OffsetStatus::BadCamera;
>     const float fov = mem.Read<float>(cam + CAM_FOV);
>     if (fov < 1.0f || fov > 179.0f) return OffsetStatus::BadCamera;
>
>     if (GetNameById(mem, base + GNAME_POOL, 1).empty())
>         return OffsetStatus::BadName;
>     return OffsetStatus::Ok;
> }
>
> const char *StatusText(OffsetStatus s) {
>     switch (s) {
>         case OffsetStatus::Ok:         return "偏移有效";
>         case OffsetStatus::NoProcess:  return "进程未运行";
>         case OffsetStatus::BadBase:    return "模块基址异常";
>         case OffsetStatus::BadWorld:   return "UWorld 读取失败";
>         case OffsetStatus::BadLevel:   return "ULevel 读取失败";
>         case OffsetStatus::BadCount:   return "Actor 数量异常";
>         case OffsetStatus::BadCamera:  return "相机数据异常";
>         case OffsetStatus::BadName:    return "名字表解析失败";
>     }
>     return "未知";
> }
> ```
> **价值**：游戏更新后，UI 上直接显示"UWorld 读取失败"——**比"什么都不工作"有用得多**

### 题 7.6 用扫描定位变量（★★）

**要求**：不用源码，通过"已知值 → 扫描 → 多次筛选"定位一个全局变量。
写扫描器和筛选逻辑。

> [!question]- 参考答案
> ```cpp
> // 在给定区域里找值等于 target 的 8 字节位置
> std::vector<uint64_t> ScanU64(const MemReader &mem,
>                               const std::vector<std::pair<uint64_t,uint64_t>> &regions,
>                               uint64_t target) {
>     std::vector<uint64_t> hits;
>     constexpr size_t CHUNK = 1 << 20;                 // 1 MB 一块
>     std::vector<uint8_t> buf(CHUNK);
>
>     for (const auto &[start, end] : regions) {
>         for (uint64_t addr = start; addr < end; addr += CHUNK) {
>             const size_t len = std::min<size_t>(CHUNK, end - addr);
>             if (mem.Read(addr, buf.data(), len) <= 0) continue;
>
>             for (size_t off = 0; off + 8 <= len; off += 8) {   // ★ 只扫对齐位置
>                 uint64_t v;
>                 memcpy(&v, buf.data() + off, 8);
>                 if (v == target) hits.push_back(addr + off);
>             }
>         }
>     }
>     return hits;
> }
>
> // 多次筛选：值变了之后，只保留"仍指向新值"的位置
> std::vector<uint64_t> Refine(const MemReader &mem,
>                              const std::vector<uint64_t> &candidates,
>                              uint64_t newValue) {
>     std::vector<uint64_t> out;
>     for (const uint64_t addr : candidates) {
>         if (mem.Read<uint64_t>(addr) == newValue) out.push_back(addr);
>     }
>     return out;
> }
>
> // 使用流程
> void LocateVariable(const MemReader &mem, const auto &regions) {
>     printf("记录当前值（如 g_world 地址），按回车继续\n");
>     uint64_t v1 = ReadFromTargetPrintf();  getchar();
>
>     auto hits = ScanU64(mem, regions, v1);
>     printf("第一次命中 %zu 个\n", hits.size());
>
>     printf("让目标改变这个值，按回车继续\n");  getchar();
>     uint64_t v2 = ReadFromTargetPrintf();
>
>     auto refined = Refine(mem, hits, v2);
>     printf("第二次命中 %zu 个\n", refined.size());
>     for (auto a : refined) printf("  候选: 0x%llX\n", (unsigned long long)a);
> }
> ```
> **关键点**：① 分块扫描（1 MB 一块，减少系统调用）② 只扫 8 字节对齐位置（省 7/8 的比较）③ **多次筛选是关键**——一次扫描命中几百个，改变值再筛能定位到唯一

### 题 7.7 时序竞态防护（★★）

**要求**：模拟"遍历时对象被释放"的场景，实现防护。
要求：宁可漏画，不可崩溃。

> [!question]- 参考答案
> ```cpp
> struct Snapshot {
>     uint64_t address = 0;
>     uint32_t nameId = 0;
>     Vector3A position;
>     bool     valid = false;
> };
>
> // 策略：一次读尽量多 → 缩短时间窗口 → 严格校验
> std::vector<Snapshot> SafeCollect(const MemReader &mem,
>                                   uint64_t arrayAddr, int32_t count) {
>     std::vector<Snapshot> out;
>     if (!IsValidPtr(arrayAddr) || count <= 0 || count > 10000) return out;
>
>     // ① 一次读完指针数组（一个时间点）
>     std::vector<uint64_t> ptrs(count);
>     if (mem.Read(arrayAddr, ptrs.data(), count * 8) != count * 8) return out;
>
>     // ② 批量读所有字段（另一个接近的时间点）
>     struct Raw { uint32_t nameId; uint64_t root; };
>     std::vector<Raw> raws(count);
>     std::vector<struct iovec> liov, riov;   // 构造批量请求...
>     // （实现见题 6.2 的 ReadBatch）
>
>     // ③ 逐个校验 + 组装
>     out.reserve(count);
>     for (int i = 0; i < count; i++) {
>         Snapshot s;
>         if (!IsValidPtr(ptrs[i])) continue;
>
>         s.address = ptrs[i];
>         s.nameId  = raws[i].nameId;
>         if (s.nameId == 0 || s.nameId > 0xFFFFFF) continue;   // 名字 ID 合理性
>
>         if (IsValidPtr(raws[i].root)) {
>             Vector3A pos;
>             if (mem.Read(raws[i].root + 0x1F0, &pos, sizeof(pos)) != (int)sizeof(pos))
>                 continue;                                     // ★ 读失败就跳过
>             // 坐标合理性
>             const float LIMIT = 1e7f;
>             if (!std::isfinite(pos.X) || std::fabs(pos.X) > LIMIT) continue;
>             if (!std::isfinite(pos.Y) || std::fabs(pos.Y) > LIMIT) continue;
>             if (!std::isfinite(pos.Z) || std::fabs(pos.Z) > LIMIT) continue;
>             s.position = pos;
>         } else {
>             continue;
>         }
>         s.valid = true;
>         out.push_back(s);
>    }
>    return out;
>}
>```
> **四个防护层次**：
> 1. **快照**：一次读整块，缩短时间窗口
> 2. **指针校验**：每个地址都过 `IsValidPtr`
> 3. **字段合理性**：`nameId` 范围、坐标范围
> 4. **读失败即跳过**：不试图修复
>
> **核心原则**：**宁可漏画一个对象，也不能崩。**

### 题 7.8 偏移文档化（★）

**要求**：把一组偏移整理成头文件，要求：
① 分类清晰 ② 有版本信息 ③ 有关键字段的注释 ④ 能被自检引用。

> [!question]- 参考答案
> ```cpp
> // offsets.h
> #pragma once
> #include <cstdint>
>
> // ============================================================
> // 游戏偏移定义
> // 适用版本: 1.0.0 / libUE4.so MD5: a1b2c3d4e5f6...
> // 最后更新: 2026-09-14
> // 更新方法: 见 docs/OFFSET_UPDATE.md
> // ============================================================
>
> struct Offsets {
>     // ===== 模块内静态偏移 =====
>     static constexpr uint64_t GNAME_POOL   = 0xB236B00;  // FName 池
>     static constexpr uint64_t UWORLD       = 0xB3EC650;  // 世界对象指针
>     static constexpr uint64_t MATRIX_CHAIN = 0xB3C5D00;  // 投影矩阵（3 级链）
>
>     // ===== UWorld =====
>     static constexpr uint64_t UWORLD_LEVEL      = 0x30;  // → ULevel
>     static constexpr uint64_t UWORLD_GAMEINST   = 0x188; // → GameInstance
>
>     // ===== ULevel =====
>     static constexpr uint64_t ULEVEL_ACTORS     = 0x98;  // Actor 指针数组
>     static constexpr uint64_t ULEVEL_COUNT      = 0xA0;  // 元素个数
>
>     // ===== PlayerController =====
>     static constexpr uint64_t PC_CONTROLROT     = 0x378; // 玩家输入朝向
>     static constexpr uint64_t PC_PAWN           = 0x390; // 自己的角色
>     static constexpr uint64_t PC_CAMERA         = 0x3A8; // CameraManager
>
>     // ===== CameraManager =====
>     static constexpr uint64_t CAM_LOCATION      = 0x2300; // Vec3 位置
>     static constexpr uint64_t CAM_ROTATION      = 0x230C; // Rotator 朝向
>     static constexpr uint64_t CAM_FOV           = 0x2318; // float 视场角
>
>     // ===== Actor =====
>     static constexpr uint64_t ACTOR_NAMEID      = 0x18;  // FName ID
>     static constexpr uint64_t ACTOR_ROOT        = 0x168; // RootComponent
>     static constexpr uint64_t ACTOR_MESH        = 0x370; // Mesh 组件
>
>     // ===== Mesh =====
>     static constexpr uint64_t MESH_C2W          = 0x2A0; // 组件→世界变换
>     static constexpr uint64_t MESH_BONES        = 0x578; // 骨骼数组
>
>     // ===== 常量 =====
>     static constexpr uint32_t BONE_STRIDE       = 48;    // 每根骨骼字节数
>
>     // ===== 元信息 =====
>     static constexpr const char *VERSION = "1.0.0";
>     static constexpr const char *UPDATED = "2026-09-14";
> };
> ```
> **关键点**：① 用 `constexpr` 而不是 `#define`（有类型、有作用域）② 每个字段一行注释说明"是什么" ③ **头部注释写清版本与更新方法** ④ 分类分组便于查找

---

## 卷八 · 物理与可见性（8 题）

### 题 8.1 盒子网格生成（★）

**要求**：生成一个盒子的 8 个顶点和 12 个三角形（36 个索引）。
要求：① 顶点的顺序要与索引匹配 ② 索引不越界 ③ 可选：应用变换。

> [!question]- 参考答案
> ```cpp
> struct Mesh { std::vector<Vec3> verts; std::vector<uint32_t> idx; };
>
> Mesh MakeBox(float hx, float hy, float hz, const Mat4 &xform = Mat4::Identity()) {
>     Mesh m;
>     // 8 个顶点（±hx, ±hy, ±hz）——顺序要与下面的索引对应
>     const Vec3 local[8] = {
>         {-hx,-hy,-hz}, { hx,-hy,-hz}, { hx, hy,-hz}, {-hx, hy,-hz},   // 0-3 底面
>         {-hx,-hy, hz}, { hx,-hy, hz}, { hx, hy, hz}, {-hx, hy, hz},   // 4-7 顶面
>     };
>     for (const Vec3 &v : local) m.verts.push_back(TransformPoint(xform, v));
>
>     // 12 个三角形（每个面 2 个）
>     static const uint32_t kIdx[36] = {
>         0,2,1, 0,3,2,   // 底
>         4,5,6, 4,6,7,   // 顶
>         0,1,5, 0,5,4,   // 前
>         3,7,6, 3,6,2,   // 后
>         0,4,7, 0,7,3,   // 左
>         1,2,6, 1,6,5,   // 右
>     };
>     m.idx.assign(kIdx, kIdx + 36);
>     return m;
> }
>
> // 验证：索引不越界
> bool Validate(const Mesh &m) {
>     for (uint32_t i : m.idx) if (i >= m.verts.size()) return false;
>     return m.idx.size() % 3 == 0;
> }
>
> // 验证体积（用行列式算 12 个四面体的有向体积和）
> float Volume(const Mesh &m);
> ```
> **关键点**：① 顶点顺序与索引表必须**严格对应**（错了画出来是乱线）② 12 个面 = 6 面 × 2 三角形 ③ 顶点缠绕顺序一致（第 94 章）

### 题 8.2 高度场网格化（★★）

**要求**：给一个 `rows × cols` 的高度采样数组，生成三角网格。

> [!question]- 参考答案
> ```cpp
> Mesh MakeHeightField(const std::vector<float> &heights,
>                      int rows, int cols,
>                      float rowScale, float colScale, float heightScale) {
>     Mesh m;
>     if (rows < 2 || cols < 2 ||
>         heights.size() < (size_t)(rows * cols)) return m;
>
>     // 顶点：每个采样点一个
>     m.verts.reserve(rows * cols);
>     for (int r = 0; r < rows; r++) {
>         for (int c = 0; c < cols; c++) {
>             m.verts.push_back(Vec3{
>                 r * rowScale,
>                 c * colScale,
>                 heights[r * cols + c] * heightScale
>             });
>         }
>     }
>
>     // 索引：每格 2 个三角形
>    m.idx.reserve((rows - 1) * (cols - 1) * 6);
>     for (int r = 0; r + 1 < rows; r++) {
>         for (int c = 0; c + 1 < cols; c++) {
>             const uint32_t a = r * cols + c;
>             const uint32_t b = a + 1;
>             const uint32_t d = a + cols;
>             const uint32_t e = d + 1;
>             m.idx.push_back(a); m.idx.push_back(d); m.idx.push_back(b);
>             m.idx.push_back(b); m.idx.push_back(d); m.idx.push_back(e);
>         }
>     }
>     return m;
>}
>```
> **关键点**：① 顶点数是 `rows × cols`，索引数是 `(rows-1)(cols-1)×6` ② 每格的索引关系：`a`(左上) `b`(右上) `d`(左下) `e`(右下) ③ 64×64 的地形 = 4096 顶点 + 7938 三角形

### 题 8.3 Möller–Trumbore 求交（★★★）

**要求**：实现 ray-triangle 求交，返回 t/u/v。
验证：① 射线穿过三角形中心命中 ② 射线从旁边过不命中 ③ 平行的射线不命中。

> [!question]- 参考答案
> ```cpp
> bool RayTriangle(const Vec3 &orig, const Vec3 &dir,
>                  const Vec3 &v0, const Vec3 &v1, const Vec3 &v2,
>                  float &outT, float &outU, float &outV) {
>     constexpr float EPS = 1e-6f;
>
>     const Vec3 e1 = v1 - v0;
>     const Vec3 e2 = v2 - v0;
>
>     const Vec3 pvec = dir.Cross(e2);
>    const float det = e1.Dot(pvec);
>    if (std::fabs(det) < EPS) return false;          // ★ 射线平行于三角形
>
>    const float invDet = 1.0f / det;
>
>    const Vec3 tvec = orig - v0;
>    const float u = tvec.Dot(pvec) * invDet;
>    if (u < 0.0f || u > 1.0f) return false;          // ★ 重心坐标约束 1
>
>    const Vec3 qvec = tvec.Cross(e1);
>    const float v = dir.Dot(qvec) * invDet;
>    if (v < 0.0f || u + v > 1.0f) return false;      // ★ 重心坐标约束 2
>
>    const float t = e2.Dot(qvec) * invDet;
>    if (t < EPS) return false;                       // ★ 在射线前方
>
>    outT = t; outU = u; outV = v;
>    return true;
>}
>
>// 验证
>const Vec3 v0{-5,-5,10}, v1{5,-5,10}, v2{0,5,10};
>float t, u, v;
>
>// ① 穿过中心
>assert(RayTriangle({0,0,0}, {0,0,1}, v0,v1,v2, t,u,v) && fabsf(t-10) < 1e-4f);
>
>// ② 旁边过（x=10 超出三角形范围）
>assert(!RayTriangle({10,0,0}, {0,0,1}, v0,v1,v2, t,u,v));
>
>// ③ 平行（沿 +X 方向）
>assert(!RayTriangle({0,0,0}, {1,0,0}, v0,v1,v2, t,u,v));
>```
> **关键点**：① 两个重心坐标约束必须都检查 ② `fabsf(det) < EPS` 处理平行 ③ `t < EPS` 排除射线背后的命中 ④ **总运算量：2 叉积 + 4 点积 + 1 除法**（没有三角函数和开方，适合 SIMD）

### 题 8.4 AABB 射线测试（★★）

**要求**：实现 slab 法，判断射线是否与轴对齐包围盒相交，
并返回进入/离开参数。

> [!question]- 参考答案
> ```cpp
> struct AABB { Vec3 min, max; };
>
> bool RayAABB(const Vec3 &orig, const Vec3 &dir, const AABB &box,
>              float &tEnter, float &tExit) {
>     float tmin = 0.0f, tmax = 1e30f;
>     const float o[3] = {orig.x, orig.y, orig.z};
>    const float d[3] = {dir.x,  dir.y,  dir.z};
>    const float lo[3] = {box.min.x, box.min.y, box.min.z};
>    const float hi[3] = {box.max.x, box.max.y, box.max.z};
>
>    for (int i = 0; i < 3; i++) {
>        if (std::fabs(d[i]) < 1e-8f) {
>            // 射线平行于这对平面：起点必须在板内
>            if (o[i] < lo[i] || o[i] > hi[i]) return false;
>            continue;
>        }
>        float t1 = (lo[i] - o[i]) / d[i];
>        float t2 = (hi[i] - o[i]) / d[i];
>        if (t1 > t2) std::swap(t1, t2);
>        tmin = std::max(tmin, t1);
>        tmax = std::min(tmax, t2);
>        if (tmin > tmax) return false;              // ★ 区间无交集
>    }
>    tEnter = tmin; tExit = tmax;
>    return true;
>}
>```
> **关键点**：① 三个轴独立求进入/离开参数，然后取交集 ② **必须处理除数为 0**（射线平行于该轴）③ 提前 `tmin > tmax` 退出（省后续计算）

### 题 8.5 手写 BVH（★★★）

**要求**：实现 BVH 的构建与遍历（选最长轴 + 中点划分），
并对比暴力遍历的加速比。**必须验证两者结果完全一致**。

> [!question]- 参考答案
> ```cpp
> struct BVHNode {
>     AABB  bounds;
>     int   left  = -1, right = -1;   // 内部节点
>     int   first = 0,  count = 0;    // 叶子节点（count > 0 表示叶子）
> };
>
> int BuildBVH(std::vector<BVHNode> &nodes, std::vector<Tri> &tris,
>              int start, int count, int depth = 0) {
>     const int self = (int)nodes.size();
>     nodes.push_back(BVHNode{});
>
>     // ① 自底向上：先算这一批三角形的包围盒
>     AABB b;
>     for (int i = start; i < start + count; i++) b.Expand(TriBounds(tris[i]));
>     nodes[self].bounds = b;
>
>     // ② 递归终止：三角形够少（或太深）就是叶子
>     if (count <= 4 || depth > 24) {
>         nodes[self].first = start;
>         nodes[self].count = count;
>         return self;
>     }
>
>     // ③ 选最长的轴划分
>     const Vec3 size = b.max - b.min;
>     const int axis = (size.x > size.y) ? ((size.x > size.z) ? 0 : 2)
>                                        : ((size.y > size.z) ? 1 : 2);
>
>     // ④ 按三角形中心在中点处一分为二（partition 是 O(n)，不排序）
>     const float mid = 0.5f * (b.min.axis(axis) + b.max.axis(axis));
>     auto it = std::partition(tris.begin() + start, tris.begin() + start + count,
>         [&](const Tri &t) { return TriCenter(t).axis(axis) < mid; });
>     int lcount = (int)(it - (tris.begin() + start));
>
>     // ⑤ 退化保护：全部落在同一侧 → 强制对半分，否则死递归爆栈
>     if (lcount == 0 || lcount == count) lcount = count / 2;
>
>     const int l = BuildBVH(nodes, tris, start, lcount, depth + 1);
>     const int r = BuildBVH(nodes, tris, start + lcount, count - lcount, depth + 1);
>     nodes[self].left  = l;
>     nodes[self].right = r;
>     return self;
> }
>
> // 遍历：与节点包围盒求交，递归下降
> bool Traverse(const std::vector<BVHNode> &nodes, const std::vector<Tri> &tris,
>               int self, const Ray &ray, float tmin, float tmax, Hit &hit) {
>     if (!RayAABB(ray, nodes[self].bounds, tmin, tmax)) return false;
>
>     if (nodes[self].count > 0) {          // 叶子：逐个精确求交
>         bool any = false;
>         for (int i = 0; i < nodes[self].count; i++) {
>             const Tri &t = tris[nodes[self].first + i];
>             float t_, u_, v_;
>             if (RayTriangle(ray, t, t_, u_, v_) && t_ > tmin && t_ < tmax) {
>                 if (t_ < hit.t) { hit.t = t_; hit.tri = &t; }
>                 any = true;
>             }
>         }
>         return any;
>     }
>     bool a = Traverse(nodes, tris, nodes[self].left,  ray, tmin, tmax, hit);
>     bool c = Traverse(nodes, tris, nodes[self].right, ray, tmin, hit.t, hit);
>     return a || c;
> }
> ```
> **关键点**：
> ① **退化必须处理**——全部分在同一侧会无限递归（栈溢出），`lcount = count / 2` 保证进展
> ② 递归时**用已找到的最近命中 `hit.t` 收紧 `tmax`**，能砍掉大量子树（`tmin` 不变）
> ③ 遍历顺序（先近后远）只影响速度，不影响正确性
> ④ **必须写"BVH 结果 == 暴力结果"的对照测试**——图形学最容易被静默 bug 坑的地方
>
> **加速比期望值**：10 万三角形场景，暴力约 `O(n)`，BVH 约 `O(log n)`，实测通常 **20–100 倍**。
> 若只有 2–3 倍，八成是包围盒算错（射线总与根节点相交），退化成暴力遍历。

### 题 8.6 从碰撞体还原网格（★★）

**要求**：把三种碰撞体（Box / Sphere / Capsule）转成可直接渲染的三角网格。
**关键**：碰撞体没有"网格"，必须按类型生成**近似**网格。

> [!question]- 参考答案
> ```cpp
> // ① 盒子：8 顶点 12 三角形（复用题 8.1 的 MakeBox）
> Mesh FromBox(const Vec3 &half, const Mat4 &x) { return MakeBox(half.x, half.y, half.z, x); }
>
> // ② 球：经纬度网格（UV sphere）
> Mesh FromSphere(float r, int rings = 16, int sectors = 24, const Mat4 &x = Mat4::Identity()) {
>     Mesh m;
>     for (int i = 0; i <= rings; i++) {
>         const float phi = PI * i / rings;                 // 0..π（纬度）
>         for (int j = 0; j <= sectors; j++) {
>             const float th = 2 * PI * j / sectors;        // 0..2π（经度）
>             Vec3 p{ r * std::sin(phi) * std::cos(th),
>                     r * std::cos(phi),
>                     r * std::sin(phi) * std::sin(th) };
>             m.verts.push_back(TransformPoint(x, p));
>         }
>     }
>     const int stride = sectors + 1;
>     for (int i = 0; i < rings; i++) {
>         for (int j = 0; j < sectors; j++) {
>             const int a = i * stride + j, b = a + stride;
>             m.idx.insert(m.idx.end(), { (uint32_t)a, (uint32_t)b, (uint32_t)(a + 1) });
>             m.idx.insert(m.idx.end(), { (uint32_t)(a + 1), (uint32_t)b, (uint32_t)(b + 1) });
>         }
>     }
>     return m;
> }
>
> // ③ 胶囊：圆柱侧面 + 两个半球
> Mesh FromCapsule(float r, float halfHeight, int seg = 24, int capRings = 8) {
>     Mesh m;
>     // 圆柱段（从 -halfHeight 到 +halfHeight）
>     for (int i = 0; i <= 1; i++) {
>         const float y = (i == 0 ? -halfHeight : halfHeight);
>         for (int j = 0; j <= seg; j++) {
>             const float th = 2 * PI * j / seg;
>             m.verts.push_back(Vec3{ r * std::cos(th), y, r * std::sin(th) });
>         }
>     }
>     // 顶/底各一个半球（同上球公式，phi 只取半个区间）
>     AppendCap(m, r, +halfHeight, true,  capRings, seg);
>     AppendCap(m, r, -halfHeight, false, capRings, seg);
>     return m;
> }
> ```
> **关键点**：
> ① 碰撞体是**数学形状**，渲染必须有网格 → 这类"还原"是纯几何生成，与物理引擎无关
> ② 顶点数要**限制**——`rings * sectors` 太大直接拖垮帧率，可视化用 16×24 足够
> ③ 索引排列要保证**缠绕方向一致**（否则相邻三角形明暗相反，看起来破面）
> ④ 加上项目里的**颜色编码**（盒子灰、球蓝、胶囊绿），一眼分辨碰撞体类型

### 题 8.7 视锥剔除（★★）

**要求**：从**视图投影矩阵**提取 6 个平面，对 AABB 做剔除测试。
**为什么不用包围球**：AABB 无需开方，且与 BVH 节点的包围盒天然复用。

> [!question]- 参考答案
> ```cpp
> struct Frustum { Vec4 planes[6]; };   // 平面方程 ax+by+cz+d=0（法线朝内）
>
> // Gribb–Hartmann 方法：直接从 VP 矩阵的"行"线性组合提取平面，无需手工构造
> Frustum FromViewProj(const Mat4 &vp) {
>     Frustum f;
>     // 行向量（vk 约定：矩阵按列存储，vp.row(i) 取第 i 行）
>     const Vec4 r0 = vp.Row(0), r1 = vp.Row(1), r2 = vp.Row(2), r3 = vp.Row(3);
>     f.planes[0] = r3 + r0;   // 左：  x ≥ -w
>     f.planes[1] = r3 - r0;   // 右：  x ≤  w
>     f.planes[2] = r3 + r1;   // 下：  y ≥ -w
>     f.planes[3] = r3 - r1;   // 上：  y ≤  w
>     f.planes[4] = r3 + r2;   // 近：  z ≥ -w（若投影为 [-1,1]）
>     f.planes[5] = r3 - r2;   // 远：  z ≤  w
>     for (auto &p : f.planes) p = p / Vec4{p.x, p.y, p.z, 0}.Length();  // 归一化
>     return f;
> }
>
> // AABB 与单个平面：取"最远正点"（p-顶点），若它都在平面外侧则整体剔除
> bool AABBOutside(const AABB &b, const Vec4 &p) {
>     const Vec3 far{
>         p.x >= 0 ? b.max.x : b.min.x,
>         p.y >= 0 ? b.max.y : b.min.y,
>         p.z >= 0 ? b.max.z : b.min.z,
>     };
>     return p.x * far.x + p.y * far.y + p.z * far.z + p.w < 0.0f;
> }
>
> bool Culled(const Frustum &f, const AABB &b) {
>     for (const Vec4 &p : f.planes) if (AABBOutside(b, p)) return true;  // 一个平面外即剔除
>     return false;
> }
> ```
> **关键点**：
> ① **取"最远点"（p-vertex）而非中心点**——用中心会误剔除靠近视锥边界的物体（物体在视野边缘忽然消失）
> ② 平面必须**归一化**，否则 `d` 项含义不一致，测试结果随机（这是最常见的错）
> ③ 平面法线方向取决于矩阵约定（行/列主序、手性），**必须实测验证**：放一个必在视野内的盒子，断言不被剔除
> ④ 剔除是**保守**的——宁可多留不可错杀；漏留只是变慢，错杀是画面缺物体

### 题 8.8 可见性缓存与脏标记（★★★）

**要求**：多物体共享同一份可见性计算结果，用**脏标记 + 版本号**避免重复计算。
**场景**：项目里 1000 个轨迹球，坐标每帧都变，但**只有一部分真的动过了**。

> [!question]- 参考答案
> ```cpp
> // 版本号：全局帧序号 + 每对象"最后修改帧"
> struct VisibleCache {
>     std::atomic<uint32_t> frame{0};        // 全局帧号，每帧 +1
>     float      cachedResult = 0.0f;        // 缓存的可见性分数
>     uint32_t   computedAt   = 0;           // 计算时的帧号
>     std::atomic<uint32_t> dirtyAt{0};      // 数据最后修改的帧号
> };
>
> // 每帧开始：全局帧号 +1
> inline void Tick(VisibleCache &c) { c.frame.fetch_add(1, std::memory_order_relaxed); }
>
> // 需要时才算，且只在"脏"或"过期"时算
> float GetVisibility(VisibleCache &c, const Actor &a, const Camera &cam) {
>     const uint32_t now = c.frame.load(std::memory_order_acquire);
>
>     // ① 命中条件：数据没改过（dirtyAt <= computedAt）且缓存未过期
>     const uint32_t dirty = c.dirtyAt.load(std::memory_order_acquire);
>     if (c.computedAt >= dirty && c.computedAt != 0 &&
>         now - c.computedAt < kMaxCacheAge) {      // kMaxCacheAge：如 60 帧后强制重算
>         return c.cachedResult;
>     }
>
>     // ② 重算（只在需要时执行）
>     float v = ComputeOcclusion(a, cam);           // 射线投射成本高
>     c.cachedResult = v;
>     c.computedAt   = now;
>     return v;
> }
>
> // 数据变更处：标脏（只写一个原子变量，无锁）
> inline void MarkDirty(VisibleCache &c) {
>     c.dirtyAt.store(c.frame.load(std::memory_order_relaxed), std::memory_order_release);
> }
>
> // 批量版：按"最后一帧是否变动"分桶，只重算变动过的
> void UpdateAll(std::vector<Actor> &actors) {
>     for (auto &a : actors) {
>         if (a.movedThisFrame) MarkDirty(a.cache);     // 只有真动过的才标脏
>     }
>     for (auto &a : actors) GetVisibility(a.cache, a, g_camera);  // 惰性重算
> }
> ```
> **关键点**：
> ① **版本号比布尔标志好**——布尔标志有"跨帧竞态"（清标志和读数据不在同一时刻），版本号是单调的，天然无此问题
> ② `kMaxCacheAge` 必须有：即使数据没变，相机移动后可见性也会变（**缓存键漏了相机**）
> ③ 缓存键必须包含**所有**影响结果的输入——对象位置、相机、遮挡物，漏一个就是画面错误
> ④ `MarkDirty` 用 `release`、读取用 `acquire`，保证"标脏"对计算线程可见（第 68 章的 acquire-release 语义）
> ⑤ 优化效果：1000 个物体里只有 50 个动过 → 射线投射从 1000 次降到 50 次，**20 倍**

---

## 结业自测

做完上面 73 题，用**不看答案**的方式完成下面三项，就算真正"能写出来"了。

| 项目 | 要求 | 对应题 |
|---|---|---|
| **综合 1** | 从零写一个 200 行以内的程序：读一个二进制文件的内存转储，解析出结构体数组并打印十六进制 + 十进制 + ASCII | 1.1 / 1.2 / 2.5 |
| **综合 2** | 用「向量 + 矩阵 + 四元数」库实现：给定相机与目标点，输出目标的屏幕坐标（含视锥判断） | 4.1 / 4.4 / 4.5 / 8.7 |
| **综合 3** | 实现一个跨进程读取器：解析 maps → 找基址 → 跟随 3 级指针链 → 读结构体 → 合法性过滤 | 2.2 / 2.3 / 6.1 / 6.4 / 6.7 |

**通过标准**：

- 综合 1/2/3 中**至少两项**能独立完成（允许查附录 A 命令表、附录 M 结构说明）
- 三项都能**说清每个设计选择的原因**（为什么用 `string_view`、为什么先分桶再计算、为什么版本号优于布尔标志）
- 出现编译错误能在**不搜索**的情况下独立定位

---

## 附：73 题知识点 ↔ 章节对照

| 卷 | 题号 | 关联章节 |
|---|---|---|
| 卷一 | 1.1–1.10 | 第 04 变量与内存 / 05 指针 / 06 数组 / 07 结构体 / 11 类与RAII / 13 模板与STL / 15 C++17-20 / 16 调试 / 29 Makefile |
| 卷二 | 2.1–2.8 | 第 19 ELF 格式 / 22 文件IO与 proc / 24 字节序与位运算 / 26 IEEE754 / 27 编译四步 / 30 nm-objdump-readelf |
| 卷三 | 3.1–3.6 | 第 34 Android.mk / 35 push 运行 / 36 adb 全套 / 38 logcat / 41 体积与符号裁剪 / 42 引入 ImGui |
| 卷四 | 4.1–4.10 | 第 45 向量 / 46 矩阵 / 47 旋转与四元数 / 49 视图矩阵 / 51 世界转屏幕 / 52 裁剪与剔除 / 54 数学库测试 / 55 浮点误差 / 56 三角函数的坑 |
| 卷五 | 5.1–5.11 | 第 50 投影矩阵 / 57 图形管线 / 61 ImGui DrawList / 62 后端生成三角形 / 63 中文字体 / 66 跨版本符号适配 / 69 触摸链路 / 71 触摸转发 / 72 帧率与性能 |
| 卷六 | 6.1–6.8 | 第 74 readv-writev / 75 封装读写类 / 76 maps 与基址 / 79 共享内存 RPC / 80 驱动抽象层 / 82 并发与锁 / 89 野指针与合法性过滤 |
| 卷七 | 7.1–7.8 | 第 83 Actor-Component / 84 对象数组 / 85 FName / 86 骨骼层级 / 87 自己找偏移 / 88 偏移失效 / 89 合法性过滤 / 92 数据结构文档化 |
| 卷八 | 8.1–8.8 | 第 93 碰撞体类型 / 94 三角形网格 / 95 光线投射 / 96 BVH / 97 Embree 入门 / 98 从碰撞体重建网格 / 99 增量更新与缓存 |

> [!tip] 做完题的下一步
> 打开 [[附录P-从零重建路线图]]，进入**阶段 1**。
> 练习题练的是"零件"，重建路线图练的是"组装"——两者合起来才是完整的项目能力。
