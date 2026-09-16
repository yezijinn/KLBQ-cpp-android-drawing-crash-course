---
tags: [教程, 附录, 错误速查]
aliases: [附录J, errors]
---

# 附录 J · 错误信息反查表

> [!abstract] 怎么用
> 把报错信息**原文**贴进来搜索（Ctrl+F），直接跳到根因和解法。
> 与 [[附录C-排错手册]] 的区别：附录 C 按"症状"查，本附录按"错误文本"查。
>
> 表中 `<...>` 是要替换的变量部分。

## 一、编译期

| 错误文本 | 含义 | 最可能的原因 | 解法 | 详见 |
|---|---|---|---|---|
| `fatal error: <x>.h: No such file or directory` | 找不到头文件 | `LOCAL_C_INCLUDES` 没配，或路径写错 | 用 `$(LOCAL_PATH)/...`；`ndk-build V=1` 看 `-I` | 34、39 |
| `error: '<x>' was not declared in this scope` | 名字没声明 | 忘了 include，或拼错 | 检查拼写 + 补 include | 09 |
| `error: expected ';' before '<x>'` | 缺分号 | 上一行末尾漏了 `;` | **看报错行的上一行** | 03 |
| `error: expected '}' at end of input` | 缺右大括号 | 括号没配对 | 从文件末尾往回数括号 | 03 |
| `error: stray '\357' in program` | 杂散字节 | 文件是 UTF-8 **带 BOM** | 去 BOM：`sed -i '1s/^\xEF\xBB\xBF//' f.cpp` | 15、44 |
| `error: stray '\<num>' in program` | 同上 | 中文全角字符混进代码 | 检查标点是否全角 | 15 |
| `error: 'xxx' does not name a type` | 不是类型名 | 结构体没定义/头文件顺序错 | 检查 include 顺序 | 09 |
| `error: invalid conversion from 'X*' to 'Y*'` | 指针类型不符 | 忘了 `static_cast` | 用 `static_cast` / `reinterpret_cast` | 12 |
| `error: passing 'const X' as 'this' discards qualifiers` | const 冲突 | const 对象调非 const 方法 | 方法加 `const` 或去掉对象的 const | 12 |
| `error: 'X' is private within this context` | 访问私有成员 | 访问权限不够 | 改 `public` 或用 getter | 11 |
| `error: no matching function for call to '<f>'` | 参数不匹配 | 参数类型/个数不对 | 看候选签名 | 08 |
| `error: variable 'x' set but not used` | 定义了没用 | 逻辑遗漏（警告） | 删除或用上 | 02 |
| `error: control reaches end of non-void function` | 有分支没 return | 缺 return | 补齐所有路径 | 08 |
| `error: array subscript is above array bounds` | 数组越界（编译期可判定） | 索引是常量且超界 | 修索引 | 06 |
| `error: dereferencing pointer to incomplete type` | 前向声明不够 | 只声明没定义就用成员 | 补 `#include` | 09 |
| `error: 'xxx' is unavailable: introduced in API <n>` | API 版本不够 | `APP_PLATFORM` 太低 | 提高 `APP_PLATFORM` 或用低版本 API | 39 |
| `error: static assertion failed` | 静态断言失败 | 模板/类型约束没满足 | 看断言的消息 | 13 |
| `error: unknown type name 'size_t'` | 缺头文件 | 忘了 `<cstddef>` / `<cstring>` | 补 include | 04 |
| `error: lvalue required as left operand` | 不是左值 | 对临时值赋值 | 用变量接收 | 12 |
| `error: cast from 'X*' to 'int' loses precision` | 指针转 int 丢精度 | 用了 `int` 存地址 | 改 `uintptr_t` / `uint64_t` | 04 |

### 运行时断言（编译通过但 `assert` 失败）

| 错误文本 | 原因 | 解法 | 详见 |
|---|---|---|---|
| `Assertion failed: (TexWidth <= 4096)` | ImGui 字体图集太大 | 减小字号或字符范围 | 63 |
| `Assertion failed: ImGui::GetCurrentContext() != NULL` | 没 `CreateContext` | 补初始化 | 42 |
| `Assertion failed: g.Initialized` | 没 `NewFrame` 就画 | 检查调用顺序 | 42 |
| `Assertion failed: <自定义消息>` | 你的断言 | 看消息定位 | 10 |

## 二、链接期

| 错误文本 | 含义 | 最可能的原因 | 解法 | 详见 |
|---|---|---|---|---|
| `undefined reference to '<sym>'` | 符号找不到 | ① 源文件没加 ② 库没链 ③ **顺序错** | `nm -u` 查需求，`nm -A` 查定义 | 27、28 |
| `undefined reference to 'std::__ndk1::<x>'` | STL 符号缺失 | STL 不一致 | 统一 `APP_STL` | 40 |
| `undefined reference to 'rtc<x>'` | Embree 符号缺失 | `.a` 没链或顺序错 | 检查 `LOCAL_STATIC_LIBRARIES` 顺序 | 43 |
| `undefined reference to 'vk<x>'` | Vulkan 符号缺失 | 没用动态加载也没链接 | 加 `IMGUI_IMPL_VULKAN_NO_PROTOTYPES` + `LoadFunctions` | 60 |
| `undefined reference to '__android_log_print'` | 缺 `-llog` | `LOCAL_LDLIBS` 没有 `-llog` | 补上 | 38 |
| `undefined reference to 'sinf'`（本地） | 缺 `-lm` | 数学库没链 | 加 `-lm` | 03 |
| `undefined reference to 'main'` | 没有 main | 拼错或签名不对 | 检查 `int main` | 02 |
| `multiple definition of '<sym>'` | 重复定义 | 头文件里定义了变量（非 inline/extern） | 改 `extern` 或 `inline` | 09、15 |
| `duplicate symbol '<sym>' in: a.o b.o` | 同名符号 | C 函数在两个 `.o` 定义 | `nm -A \| grep` 定位 | 27 |
| `cannot find -l<x>` | 找不到库文件 | 搜索路径不对 | 加 `-L` 或检查库名 | 28 |
| `skipping incompatible <lib>.a` | 架构不符 | ABI 不匹配 | 换 arm64 版本 | 39、43 |
| `file not recognized: File format not recognized` | 文件损坏/非对象文件 | 传了错的文件 | `file <path>` 检查 | 43 |
| `relocation truncated to fit: R_AARCH64_CALL26` | 跳转超范围 | 代码太大（±128MB） | 分模块，或 `-mcmodel=large` | 深挖B |
| `error: linker command failed with exit code 1` | 链接失败（笼统） | 看上面几条具体信息 | — | 27 |
| `hidden symbol '<x>' is referenced by DSO` | 引用了 hidden 符号 | `-fvisibility=hidden` 影响了导出 | 给该符号加 `visibility("default")` | 深挖B |
| `LTO 相关: plugin needed to handle lto object` | LTO 对象不被识别 | 工具链不一致 | 检查是否用了同一套 NDK | 41 |

## 三、加载期（程序启动）

| 错误文本 | 含义 | 原因 | 解法 | 详见 |
|---|---|---|---|---|
| `CANNOT LINK EXECUTABLE: library "<x>" not found` | 缺动态库 | 库不在设备上 | `readelf -d` 看 NEEDED，补推库 | 40 |
| `library "<x>" needed or dlopened by ... is not accessible for the namespace` | 命名空间限制 | App 不能 dlopen 私有库 | 改成独立可执行文件 | 32、深挖C |
| `cannot locate symbol "<x>" referenced by "<lib>"` | 符号不存在 | 库版本不符 | 检查设备上该库是否导出该符号 | 65 |
| `has text relocations` | 有需要改代码段的重定位 | 没编译成 PIC | 加 `-fPIC` | 深挖B |
| `only position independent executables (PIE) are supported` | 缺 PIE | 手动关了 PIE | 加 `-fPIE -pie` | 39 |
| `not executable: 64-bit ELF` | 架构不符 | ABI 错 | 确认 `APP_ABI` | 39 |
| `exec format error` | 格式不对 | 文件损坏或架构错 | `file` 检查 | 39 |
| `Permission denied`（执行时） | 没有执行权限 | 忘了 chmod，或在 noexec 分区 | `chmod 755`，放 `/data/local/tmp` | 35 |
| `No such file or directory`（文件明明在） | 解释器缺失 | `PT_INTERP` 指向的解释器不存在 | `readelf -l \| grep INTERP` | 深挖B |

## 四、运行期崩溃

### SIGSEGV（段错误）

| 症状 | 原因 | 定位方法 | 详见 |
|---|---|---|---|
| `fault addr 0x30` 这类**小地址** | 空指针 + 偏移 | 反推是哪一跳没判空 | 16、89 |
| `fault addr 0x0` | 直接解引用空指针 | 找 `if (x == 0)` 缺失点 | 05 |
| `fault addr` 是**巨大随机值** | 野指针 | 加合法性过滤 | 89 |
| 崩溃在 `memcpy` 里 | 缓冲区越界 | 检查 size 与缓冲大小 | 08 |
| 崩溃在字符串函数里 | 字符串无 `\0` 终止 | `ReadString` 加长度上限 | 06、75 |
| 偶发崩溃（不固定位置） | 数据竞争 | 加锁；`-fsanitize=thread` | 82 |
| 加了 printf 就不崩 | 竞态（时序被改变） | 用线程检查器而非 printf | 82 |
| 只在 `-O2` 下崩 | 严格别名/UB | 用 `memcpy` 替代指针转换 | 深挖A |
| 栈溢出（递归深） | 递归太深或大局部数组 | 改迭代；大数组放堆 | 08 |
| 崩溃在 `new`/`malloc` | 堆被破坏 | `-fsanitize=address` | 16 |

```bash
# 定位流程
adb logcat | grep -E 'SIGSEGV|fault addr|backtrace'
aarch64-linux-android-addr2line -e app -f -C 0x<地址>
adb shell cat /data/tombstones/tombstone_00 | head -40
```

### SIGABRT

| 症状 | 原因 | 解法 | 详见 |
|---|---|---|---|
| `abort message: '<自定义>'` | 你的 `abort()` 或断言 | 看消息 | 16 |
| `assertion failed` | `assert` 失败 | 检查前提条件；发布版应定义 `NDEBUG` | 10 |
| `terminate called after throwing an instance of '<异常>'` | 异常没被捕获 | 加 `try/catch`；静态/动态 STL 混用会跨库边界丢失 | 40 |
| `pure virtual method called` | 构造/析构期间调虚函数 | 检查对象生命周期，别在构造里调虚函数 | 11 |
| `free(): invalid pointer` | 释放了非 `malloc` 的指针 | 检查所有权；优先用 `unique_ptr` | 14 |
| `double free or corruption` | 重复释放 | 检查拷贝语义（浅拷贝会 double free） | 11 |

### SIGBUS

| 症状 | 原因 | 解法 | 详见 |
|---|---|---|---|
| 访问未对齐地址（ARM） | 用指针强转做未对齐访问 | 改用 `memcpy` | 深挖A |
| 访问 mmap 之外的文件区域 | 文件被截断 | 检查文件大小 | 17 |

## 五、逻辑错误（不崩但不对）

### 数据全为 0

| 可能原因 | 检查方法 | 详见 |
|---|---|---|
| PID 不对 | `ps -A \| grep <name>` | 18 |
| 模块基址不对 | 用 `memview` 看 | 76 |
| 偏移不对 | 对照 `offsets.h` | 92 |
| 读取大小不对 | float 4 字节、指针 8 字节 | 04 |
| 进程已退出 | 检查 `ESRCH` | 74 |
| 地址计算时整型溢出 | 用 `0x8ULL * i` 而非 `8 * i` | 12 |

### 数据是天文数字

| 可能原因 | 例子 | 修复 |
|---|---|---|
| float 当 int 读 | `1078523331` 其实是 `3.14f` | 用正确的模板类型 |
| int 当 float 读 | 极小的科学计数法数 | 同上 |
| 4 字节当 8 字节读 | 高位夹杂相邻字段 | 检查 `sizeof` |
| 结构体对齐不符 | 偏移全错 | 用 `offsetof` 验证 |
| 字节序问题（跨平台） | 值被字节交换 | 只在跨平台时需要转换 |

```cpp
// 排查方法：hexdump 原始字节
hexdump((void*)addr, 32);
// 对照 IEEE754 或已知值判断
```

### 坐标/位置错误

| 症状 | 原因 | 详见 |
|---|---|---|
| 方框整体偏移一半 | `px/py` 用了全屏而非半屏 | 51 |
| 画面上下颠倒 | 少了 `1 - ndc_y` | 50 |
| 远近一样大 | 忘了透视除法（除以 w） | 50 |
| 左右反了 | 手性/坐标系约定不符 | 47 |
| 只有远处错 | 大坐标精度损失 | 55 |
| 开镜时错 | FOV 没每帧读 | 90 |
| 骨骼位置全错 | `BONE_STRIDE` 用了 40 | 07、86 |
| 骨骼形状错乱 | 变换顺序错（父级×局部，写反了） | 48 |
| 网格物体全堆在原点 | 忘了应用 Actor 变换 | 98 |
| 遮挡判定全错 | `tMax` 不是"到目标距离" | 95、深挖H |
| 遮挡永远为真 | 没留余量，命中自己 | 深挖H |
| 画出了背后的人 | 没做 `w < 0.001` 检查 | 51 |

### 性能问题

| 症状 | 首要检查 | 详见 |
|---|---|---|
| 帧率低 | 分段计时找瓶颈 | 72 |
| 每帧卡顿 | 是不是每帧全量重建 BVH | 99 |
| 内存持续增长 | 缓存没清理 / fd 泄漏 | 99、22 |
| 手机发热 | 帧率限制太松 / Embree 线程太多 | 72、97 |
| 第一次运行慢 | 静态场景首次构建（几十万三角形） | 98 |
| 只有近距离卡 | 没做距离剔除 | 52 |

## 六、Android / adb 环境

| 错误文本 | 原因 | 解法 | 详见 |
|---|---|---|---|
| `adb: no devices/emulators found` | 没连上 | `adb kill-server && adb start-server`；检查线/授权 | 36 |
| `error: device unauthorized` | 手机上没授权 | 拔插，点"允许" | 36 |
| `error: more than one device/emulator` | 多设备 | `-s <serial>` 或 `ANDROID_SERIAL` | 36 |
| `error: device offline` | adbd 卡了 | 重启 adb | 36 |
| `read-only file system` | 推到了只读分区 | 用 `/data/local/tmp` | 35 |
| `No space left on device` | 空间不足 | 清理 `/data/local/tmp` | 36 |
| `Permission denied`（写文件） | DAC 或 SELinux | `ls -l` 看权限；`dmesg \| grep avc` | 37 |
| `avc: denied { read }` | SELinux 拒绝 | 换位置/换域；`setenforce 0` 仅诊断 | 37、深挖C |
| `exec: <cmd>: not found` | Android shell 命令子集 | 用 toybox 里有的命令 | 36 |
| `su: not found` | 没 root | 检查 `su -c id` | 37 |
| `/dev/uinput: No such file or directory` | 内核没编译该模块 | 换设备，或用内核驱动方案 | 70、深挖C |
| `getevent: not found` | 缺工具 | 用 `dumpsys input` 替代 | 69 |
| 中文输出乱码 | 终端编码 | `chcp 65001`（Windows） | 02 |
| 脚本报 `$'\r': command not found` | CRLF 换行 | `dos2unix`，Git 配 `core.autocrlf input` | 39 |

## 七、Vulkan 专项

| 错误文本 | 原因 | 解法 | 详见 |
|---|---|---|---|
| `vkCreateInstance = -3` | `INITIALIZATION_FAILED` | 检查扩展名拼写 | 58 |
| `vkCreateInstance = -9` | `INCOMPATIBLE_DRIVER` | 设备不支持 Vulkan | 58 |
| `vkCreateDevice = -7` | `EXTENSION_NOT_PRESENT` | 设备扩展没启用（通常缺 `VK_KHR_swapchain`） | 58 |
| `vkCreateSwapchainKHR = -9` | 参数不符规范 | 检查 `imageExtent` / `compositeAlpha` | 59 |
| `VK_ERROR_DEVICE_LOST` | GPU 挂了 | 检查命令缓冲录制逻辑 | 深挖E |
| 验证层报 `sType is invalid` | 结构体没填 `sType` | 每个 CreateInfo 都要填 | 58、深挖E |
| 验证层报 `was not destroyed` | 资源泄漏 | 检查清理函数 | 58 |
| 悬浮窗黑底 | 图层格式非 RGBA_8888，或清屏色 alpha≠0 | 查图层格式 + `ClearValue` | 59 |
| 画面撕裂 | 呈现模式不对 | 用 `FIFO` | 59 |
| 旋转屏幕后错位 | 没重建交换链 | 检测尺寸变化 | 59 |
| 渲染卡住不退出 | 同步对象用错 | 检查信号量配对 | 深挖E |

## 八、dlsym / 私有库

| 症状 | 原因 | 解法 | 详见 |
|---|---|---|---|
| `dlsym` 返回 null | 符号名不匹配（版本差异） | 用"按版本优先 + 失败全试" | 66 |
| `dlerror()` 返回 `undefined symbol` | 库没加载或符号不存在 | 先确认 `dlopen` 成功 | 65 |
| `dlopen` 成功但调用崩溃 | 结构体布局不符 | 核对对象大小与版本 | 65、深挖C |
| 换设备就失效 | Android 版本不同 | 维护版本对照表 | 66 |
| 符号名太长记不住 | mangling | 用 `c++filt` 解修饰 | 65、深挖B |

## 九、上游工具链

| 错误文本 | 原因 | 解法 | 详见 |
|---|---|---|---|
| `Android NDK: Could not find application project directory` | 不在工程根目录 | `cd` 到有 `jni/` 的目录 | 33 |
| `*** missing separator. Stop.` | Makefile 用空格缩进 | 改成 Tab | 29 |
| `make: *** No rule to make target` | 目标文件不存在且无规则 | 检查文件名/路径 | 29 |
| `clang: error: no such file or directory: '<x>'` | 源文件路径错 | 相对 `LOCAL_PATH` 写 | 34 |
| `cannot find -landroid` | 链接器找不到系统库 | 确认 NDK sysroot 里有 | 39 |
| `Failed to create a process: 206` | Windows 命令行太长 | 设 `APP_SHORT_COMMANDS := true` | 29 |
| `ninja: build stopped` | 构建失败 | 往上看第一条错误 | 33 |

## 十、按"错误出现时机"的决策树

```
编译失败
  ├─ 找不到文件/头文件     → LOCAL_C_INCLUDES / 路径
  ├─ 语法/类型错误         → 看第一行，别被连锁反应干扰
  └─ 宏展开问题            → gcc -E 看展开结果

链接失败
  ├─ undefined reference    → nm -u 看谁需要；检查顺序
  ├─ multiple definition    → 头文件里定义变量了
  └─ 库不兼容              → 检查 ABI / STL

程序启动失败
  ├─ library not found      → readelf -d 查 NEEDED
  ├─ cannot locate symbol   → 库版本不符
  └─ Permission denied      → chmod / SELinux

程序崩溃
  ├─ SIGSEGV + 小地址       → 空指针（某跳没判空）
  ├─ SIGSEGV + 随机值       → 野指针（缺合法性过滤）
  ├─ SIGABRT + assertion    → 断言消息就是答案
  └─ 偶发崩溃              → 数据竞争

程序不崩但数据不对
  ├─ 全 0        → PID/基址/偏移/类型大小
  ├─ 天文数字    → 类型错了（float/int 混淆）
  ├─ 位置错      → 投影公式或坐标系
  └─ 部分错      → 大坐标精度 / 某个偏移变了
```

> [!tip] 一个实用习惯
> 把每次遇到的错误和解法记到一个自己的 `errors.md` 里。
> **三个月后，这份私人错误库比这篇通用表更有用**——
> 因为里面全是你在自己的项目上踩过的坑。

→ 返回 [[00-开始之前]]
