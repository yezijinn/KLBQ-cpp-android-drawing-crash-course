---
tags: [教程, 附录, 代码索引]
aliases: [附录E, codes]
---

# 附录 E · 代码清单与索引

> [!abstract] 怎么用
> 教程里所有**可运行代码**的索引。需要某个功能时，直接查表定位到章节。
> 所有代码都是完整可编译的（除标注"片段"的以外）。

## 一、完整可运行程序

| # | 程序 | 章节 | 行数级别 | 功能 | 编译命令 |
|---|---|---|---|---|---|
| 1 | `hello.c` | 02 | ~10 | 第一个 C 程序 | `gcc hello.c -o hello` |
| 2 | `hexdump` | 04 | ~25 | 十六进制转储 | `gcc hexdump.c -o hexdump` |
| 3 | 模拟指针链 | 05 | ~30 | 理解多级解引用 | `gcc ptr.c -o ptr` |
| 4 | 安全数组读取器 | 06 | ~40 | 边界检查与 TArray 校验 | `gcc arr.c -o arr` |
| 5 | 结构体对齐实验 | 07 | ~30 | `offsetof` 与 48 字节 | `gcc align.c -o align` |
| 6 | 栈地址观察 | 08 | ~20 | 证明栈向下生长 | `gcc stack.c -o stack` |
| 7 | RAII 文件类 | 11 | ~50 | 析构自动 fclose | `g++ -std=c++17 file.cpp` |
| 8 | `FakeDriver` | 13 | ~80 | 模拟跨进程读写的读写器 | `g++ -std=c++17 -Wall` |
| 9 | 裸指针改造 | 14 | ~40 | unique_ptr 演示 | `g++ -std=c++17` |
| 10 | `parse_pid` | 15 | ~30 | C++20 ranges + optional | `g++ -std=c++20 -Wall` |
| 11 | 打印自己的内存地图 | 17 | ~15 | 读 `/proc/self/maps` | `gcc maps.c -o maps` |
| 12 | mini ps | 18 | ~40 | 遍历 /proc 列进程 | `gcc ps.c -o ps` |
| 13 | ELF 头解析器 | 19 | ~80 | ELF 头 + 程序头表 | `gcc elfhead.c -o elfhead` |
| 14 | raw syscall 写文件 | 21 | ~25 | 用 `syscall()` 而非 glibc | `gcc sc.c -o sc` |
| 15 | maps 解析器 | 22 | ~40 | 筛出 r-x 段 | `gcc mapscan.c -o mapscan` |
| 16 | writer/reader 共享内存 | 23 | ~60 | POSIX shm 双进程 | `gcc x.c -o x -lrt` |
| 17 | pack/unpack 位字段 | 24 | ~40 | 位运算打包解包 | `gcc bits.c -o bits` |
| 18 | float 解析器 | 26 | ~50 | IEEE754 位级拆解 | `gcc f.c -o f -lm` |
| 19 | high/mid/low 三库实验 | 28 | — | 链接顺序验证 | `ar rcs` + `gcc` |
| 20 | 多目录 Makefile 工程 | 29 | ~40 | `-MMD` 依赖追踪 | `make` |
| 21 | 向量库 + 断言 | 45 | ~80 | 点积叉积归一化 | `g++ -std=c++17 -lm` |
| 22 | 矩阵实验 | 46 | ~80 | 验证不可交换 | `g++ -std=c++17` |
| 23 | 旋转互转验证 | 47 | ~70 | 欧拉角↔四元数↔矩阵 | `g++ -std=c++17 -lm` |
| 24 | 骨骼链手算 | 48 | ~60 | 局部×父级=世界 | `g++ -std=c++17` |
| 25 | 视图矩阵验证 | 49 | ~70 | `matrix[15]` 由来 | `g++ -std=c++17 -lm` |
| 26 | 投影验证（6 项） | 50 | ~80 | 透视与对称性 | `g++ -std=c++17 -lm` |
| 27 | **`w2s_demo`** ★ | 51 | ~100 | 完整 W2S 链路 | `gcc w2s_demo.c -o w2s_demo -lm` |
| 28 | 剔除统计器 | 52 | ~60 | 各级剔除数量 | `g++ -std=c++17` |
| 29 | **`mathlib.h`** ★ | 53 | ~300 | 自包含数学库 | 纯头文件 |
| 30 | **mathlib 测试** ★ | 54 | ~200 | 断言框架 + 全覆盖 | `g++ -std=c++17 mathlib_test.cpp` |
| 31 | 浮点精度验证 | 55 | ~50 | 四类误差演示 | `g++ -std=c++17` |
| 32 | 三角函数验证器 | 56 | ~80 | 六个坑验证 | `g++ -std=c++17 -lm` |
| 33 | CPU 光栅化 | 57 | ~50 | ASCII 三角形 | `gcc raster.c -o raster` |
| 34 | Vulkan 探针 | 58 | ~60 | 打印 GPU 与队列族 | NDK 编译 |
| 35 | 表面能力查询 | 59 | ~50 | 查 compositeAlpha | NDK 编译 |
| 36 | mini Vulkan wrapper | 60 | ~80 | dlopen + 宏加载 | NDK 编译 |
| 37 | DrawData 统计器 | 61 | ~40 | 打印顶点/命令结构 | NDK 编译 |
| 38 | **CPU 渲染后端** | 62 | ~150 | 把 ImGui 画到图片 | `g++ -std=c++17` |
| 39 | 字体验证器 | 63 | ~40 | 对比加中文前后 | `g++ -std=c++17` |
| 40 | 符号侦察器 | 65 | ~30 | 检查 libgui 符号 | NDK + 设备 |
| 41 | **`overlay`** ★ | 66 | ~150 | 透明覆盖层 | NDK + 设备 |
| 42 | 图层属性实验台 | 67 | ~80 | 命令行调属性 | NDK + 设备 |
| 43 | 屏幕信息解析器 | 68 | ~80 | 解析 dumpsys display | `g++ -std=c++17` |
| 44 | 触摸监视器 | 69 | ~60 | 读 /dev/input | NDK + root |
| 45 | 虚拟点击器 | 70 | ~120 | uinput 注入 | NDK + root |
| 46 | 带命中测试的转发器 | 71 | ~60 | 条件转发 | NDK + root |
| 47 | 性能面板 | 72 | ~60 | 帧时曲线 | NDK 编译 |
| 48 | target/reader | 74 | ~80 | 跨进程读写闭环 | `gcc x.c -o x` |
| 49 | **`MemReader`** ★ | 75 | ~150 | 封装读写类 | 头文件 + 测试 |
| 50 | **`memview`** ★ | 76 | ~150 | 模块与区域列表 | `gcc memview.cpp -std=c++17` |
| 51 | 符号查找器 | 77 | ~150 | 解析动态符号 | `gcc symfind.cpp -std=c++17` |
| 52 | 共享内存 RPC | 79 | ~180 | 客户端/服务端 | `g++ -O2 x.cpp -lrt` |
| 53 | 三个后端对比 | 80 | ~100 | IDriver 扩展 | `g++ -std=c++17` |
| 54 | 批量读 benchmark | 81 | ~120 | 朴素 vs 优化 | `g++ -O2 -std=c++17` |
| 55 | 三种锁性能对比 | 82 | ~80 | 无锁/互斥/自旋/原子 | `g++ -O2 -pthread` |
| 56 | **demo 引擎** ★ | 83 | ~200 | Actor/Component 世界 | `g++ -std=c++17` |
| 57 | demo 遍历程序 | 84 | ~80 | 解析对象数组 | `g++ -std=c++17` |
| 58 | 名字表扫描器 | 85 | ~70 | 遍历 FName | `g++ -std=c++17` |
| 59 | 读骨骼 + 火柴人 | 86 | ~150 | 端到端验证 | `g++ -std=c++17` |
| 60 | 内存扫描器 | 87 | ~100 | 找值/多次筛选 | `g++ -std=c++17` |
| 61 | 偏移自检 | 88/92 | ~60 | SelfTest | `g++ -std=c++17` |
| 62 | **`PtrValidator`** ★ | 89 | ~120 | 八层防御 + 统计 | `g++ -std=c++17` |
| 63 | 相机数据验证 | 90 | ~60 | UI 显示四项 | NDK 编译 |
| 64 | 矩阵对比工具 | 91 | ~80 | 引擎 vs 自算 | `g++ -std=c++17` |
| 65 | 碰撞体读取 | 93 | ~60 | 三种碰撞体 | `g++ -std=c++17` |
| 66 | 网格生成器 | 94 | ~150 | Box/Sphere/HeightField | `g++ -std=c++17` |
| 67 | **光线投射器** ★ | 95 | ~150 | Möller–Trumbore | `g++ -std=c++17 -lm` |
| 68 | **手写 BVH** ★ | 96 | ~200 | 构建 + 遍历 + 加速比 | `g++ -O2 -std=c++17` |
| 69 | Embree 完整示例 | 97 | ~80 | 四个查询用例 | NDK 编译（含 Embree） |
| 70 | 碰撞体重建 | 98 | ~200 | 五类几何 → 三角形 | `g++ -std=c++17` |
| 71 | 增量更新 benchmark | 99 | ~120 | 增量 vs 全量 | `g++ -O2 -std=c++17` |
| 72 | **`coverlay`** ★ | 100 | 全项目 | 毕业项目 | NDK 完整构建 |

★ = 教程承诺的六个成品之一或核心工具

## 二、关键代码片段索引

| 功能 | 章节 | 关键代码 |
|---|---|---|
| 十六进制转储 | 04 | `hexdump(addr, len)` |
| 内存里读一个指针 | 05 | `*(uint64_t*)addr` |
| 指针链跟随 | 05、75 | `FollowChain(base, {0x30, 0x98})` |
| TArray 有效性 | 06 | `base && count>0 && count<=max && max>0` |
| 结构体字段偏移 | 07 | `offsetof(struct, field)` |
| 位运算取字段 | 24、85 | `header >> 6` / `& 0x1` |
| 先清后置位修改 | 24 | `&= ~mask` 然后 `\|= value` |
| XOR 可逆加密 | 24 | `data[i] ^= KEY[i % 8]` |
| 平台条件编译 | 10 | `#if defined(__aarch64__)` |
| 输出参数模式 | 08 | `bool f(..., uint64_t *out)` |
| RAII 锁 | 82 | `std::scoped_lock<SpinLock> lock(m)` |
| 自旋等待 | 23、82 | `while(!flag) asm volatile("yield");` |
| 宏批量加载符号 | 60 | `#define LOAD(fn) pfn##fn = dlsym(...)` |
| 解析 maps 行 | 22、76 | `sscanf(line, "%llx-%llx %4s ...")` |
| 模块名后缀匹配 | 76 | 检查前一个字符是 `/` |
| 合并相邻区域 | 76 | 排序后遍历合并 |
| ELF 找 PT_DYNAMIC | 77 | `phdr.p_type == PT_DYNAMIC` |
| 从 hash 表推符号数 | 77 | 读第二个 uint32（nchain） |
| process_vm_readv | 74 | 构造 `iovec` 数组 |
| 批量读 | 74、81 | `liovcnt = N` |
| 字符串安全读取 | 75 | `buf[maxLen]='\0'` |
| UTF-32 → UTF-8 | 85 | `AppendUtf8(out, c)` |
| FName 解析 | 85 | `blockAddr + blockOff*2` |
| 四元数 → 矩阵 | 47、86 | `TransformToMatrix()` |
| 局部 × 父级 | 48 | `MatrixMulti(local, c2w)` |
| 视图矩阵 | 49 | `matrix[15] = -(camX*m[3]+...)` |
| 投影 `t` | 50 | `t = tanf(FOV*0.5f*DEG2RAD)` |
| W2S | 51 | `(clip/w + 1) * px` |
| Y 轴翻转 | 50 | `(1.0f - ndc_y) * py` |
| 角度归一化 | 47、56 | `归一角差(a,b)` |
| 方向 → 角度 | 47 | `atan2f(z, 水平)` + `atan2f(y,x)` |
| isfinite 防护 | 26、89 | `std::isfinite(v)` |
| NaN 检测 | 26、49 | `x != x` |
| 距离平方比较 | 45、52 | `dx*dx+dy*dy < maxSq` |
| 保留最近命中 | 95 | `if (t < best.t)` |
| AABB slab 测试 | 95、96 | `RayAABB()` |
| Möller–Trumbore | 95 | `RayTriangleIntersect()` |
| BVH 划分（最长轴） | 96 | 比较包围盒三个轴长度 |
| BVH 剪枝 | 96 | `if (tBox > hit.t) return` |
| Embree 查询 | 97 | `rtcIntersect1(scene, &ctx, &rh)` |
| 命中判断 | 97 | `geomID != RTC_INVALID_GEOMETRY_ID` |
| 凸包扇形三角化 | 94、98 | 从 `poly[0]` 出发连三角形 |
| 高度场网格化 | 94、98 | 每格两个三角形 |
| 增量变化检测 | 99 | 变换阈值 0.1 + mesh 指针 |
| 时间戳短路 | 99 | `ts != 0 && ts == last` |
| 设计模式：抽象接口 | 80 | 虚函数 + 非虚模板方法 |
| 设计模式：能力探测 | 80 | `if (kernel() == nullptr)` |

## 三、按依赖关系的学习路径

```
必须先做的（后面都依赖）
├─ 05 指针            → 后面全部
├─ 07 结构体对齐       → 83、85、86、98
├─ 17 虚拟地址空间     → 73-82
├─ 22 maps 解析       → 76、89
├─ 45-46 向量矩阵      → 47-56、95、96
├─ 51 W2S             → 全部绘制
├─ 53 mathlib          → 54-56、86、95
└─ 76 memview          → 77、83-92

可以先跳过的（遇到再回来）
├─ 20 GOT/PLT          → 只在 60、65 用到
├─ 25 ARM64 汇编       → 只在 87 特征码用到
├─ 26 IEEE754          → 只在排查"数据是天文数字"时用到
├─ 55 浮点误差         → 只在数值异常时用到
└─ 78 内核模块         → 只在想上内核方案时用到
```

## 四、按"我现在想做什么"查

| 我想… | 看这一章 |
|---|---|
| 让程序跑在手机上 | 33、35 |
| 让画面显示出来 | 58、59 |
| 画一个方框 | 61（API）+ 51（坐标） |
| 显示中文 | 63 |
| 让菜单能点 | 71 |
| 读另一个进程的数据 | 74、75 |
| 找到模块基址 | 76 |
| 遍历所有对象 | 84 |
| 读角色名字 | 85 |
| 读骨骼位置 | 86 |
| 判断被墙挡住 | 95、97、98 |
| 提高帧率 | 72、81、99 |
| 程序老崩 | 89 |
| 游戏更新后失效了 | 87、88、92 |
| 不知道屏幕坐标为什么错 | 49-51 |

→ 返回 [[00-开始之前]]
