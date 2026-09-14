---
tags: [教程, 附录, 数据结构, 字段级]
aliases: [附录M, structs]
---

# 附录 M · 核心数据结构全解（字段级）

> [!abstract] 这篇解决什么
> 前面的章节讲了"数据怎么读"，这篇讲**"每个结构体长什么样、每个字段干什么"**。
> 134 个类型定义，逐个字段说明设计意图、偏移、大小。
> **这是"完全理解项目"的最后一公里。**

## 一、类型总览

项目自有代码里有 **134 个类型定义**，分布：

| 文件 | 类型数 | 内容 |
|---|---|---|
| `src/Android_draw/driver.h` | 31 | 驱动通信的全部结构 |
| `include/Embree/PhysX.h` | 46 | PhysX 结构 + Embree 封装 |
| `include/My_Utils/VectorTools.h` | 6 | 基础数学类型 |
| `include/Android_draw/draw.h` | 3 | 绘制层向量 |
| `include/Android_draw/WorldToScreen.h` | 1 | 相机信息 |
| `include/ImGui/VectorStruct.h` | 5 | 触摸层向量 |
| `include/ImGui/AndroidImgui.h` | 5 | 渲染抽象接口 |
| `include/ImGui/TouchHelperA.h` | 2 | 触摸设备 |
| `include/Vulkan/VulkanGraphics.h` | 2 | Vulkan 后端 |
| `include/Hack/*.h` | 4 | 自瞄配置 |
| `include/My_Utils/ConfigManager.h` | 1 | 全局配置 |
| 其它 | 28 | 系统声明、辅助类型 |

## 二、驱动层结构（driver.h）

**这是本项目最核心的一组结构**，决定了"用户态怎么和内核说话"。

### 2.1 `SpinLock` —— 自旋锁

```cpp
class SpinLock {
    unsigned char locked = 0;        // 只有 1 字节！
public:
    void lock() noexcept {
        while (__atomic_exchange_n(&locked, 1, __ATOMIC_ACQUIRE)) {
            while (__atomic_load_n(&locked, __ATOMIC_RELAXED)) {
                asm volatile("yield");
            }
        }
    }
    void unlock() noexcept {
        __atomic_store_n(&locked, 0, __ATOMIC_RELEASE);
    }
};
```

| 字段/方法 | 说明 |
|---|---|
| `unsigned char locked` | **只用 1 字节**——因为只需要表示 0/1，且能减少缓存行占用 |
| `lock()` | 双层循环：外层 `exchange` 抢锁，内层 `load` 纯读等待 |
| `unlock()` | `RELEASE` 内存序，保证临界区内的写对下一个加锁者可见 |

**为什么不用 `std::atomic<bool>`**：`std::atomic` 可能引入额外约束，
而 GCC 内置 `__atomic_*` 能精确控制内存序且无额外开销（第 82 章详述）。

### 2.2 `env_params` —— 线程环境参数

```cpp
#define TLS_THREAD_NAME_LEN 16
struct env_params {
    char thread_name[TLS_THREAD_NAME_LEN];   // 16 字节：要查的线程名
    uint64_t tpidr_el0;                      // ARM64 的 TLS 基址寄存器值
    uint64_t pacga_lo;                       // 指针认证（PAC）参数
    uint64_t pacga_hi;
    int tls_status;                          // 查询结果状态
    int pacga_status;
};
```

| 字段 | 用途 |
|---|---|
| `thread_name[16]` | 输入：要查询哪个线程 |
| `tpidr_el0` | 输出：该线程的 TLS 基址（ARM64 的 `TPIDR_EL0` 寄存器） |
| `pacga_lo` / `pacga_hi` | 输出：指针认证码（Pointer Authentication） |
| `tls_status` / `pacga_status` | 输出：各项是否查询成功 |

> [!note] 这个结构的用途
> ARM64 的 **PAC**（指针认证）会在指针高位存一个签名。
> 如果要伪造/还原指针，必须知道签名密钥 —— 这个结构用来从内核获取相关参数。
> **这是很底层的技术**，本项目有接口但 UI 没用到。

### 2.3 硬件断点相关（4 个枚举 + 3 个结构）

**① 断点类型 `bp_type`**

```cpp
enum bp_type {
    BP_BREAKPOINT_EMPTY = 0,
    BP_BREAKPOINT_R = 1,        // 读断点
    BP_BREAKPOINT_W = 2,        // 写断点
    BP_BREAKPOINT_RW = 3,       // 读写
    BP_BREAKPOINT_X = 4,        // 执行断点
    BP_BREAKPOINT_INVALID = 7,
};
```

**② 断点长度 `bp_len`**

```cpp
enum bp_len {
    BP_BREAKPOINT_LEN_1 = 1,    // 监控 1 字节
    // ... 2 到 7
    BP_BREAKPOINT_LEN_8 = 8,    // 监控 8 字节
};
```

**③ 作用范围 `bp_scope`**

```cpp
enum bp_scope {
    BP_SCOPE_MAIN_THREAD,      // 只在主线程生效
    BP_SCOPE_OTHER_THREADS,    // 只在子线程生效
    BP_SCOPE_ALL_THREADS,      // 所有线程
};
```

**④ 寄存器索引 `bp_reg_idx`**（72 项，2 bit 一个）

```cpp
enum bp_reg_idx {
    IDX_PC = 0,            // 程序计数器
    IDX_HIT_COUNT,         // 命中次数
    IDX_LR,                // 返回地址寄存器
    IDX_SP,                // 栈指针
    IDX_ORIG_X0,           // 原始 x0
    IDX_SYSCALLNO,         // 系统调用号
    IDX_PSTATE,            // 处理器状态
    IDX_X0, IDX_X1, ..., IDX_X29,     // 通用寄存器（30 个）
    IDX_FPSR, IDX_FPCR,               // 浮点状态/控制寄存器
    IDX_Q0, IDX_Q1, ..., IDX_Q31,     // SIMD 寄存器（32 个）
    MAX_REG_COUNT          // 总数
};
```

**⑤ `bp_record` —— 命中现场记录（约 848 字节）**

```cpp
struct bp_record {
    uint8_t  mask[18];        // 位图：每个寄存器 2 bit 控制读/写（72 个寄存器 × 2 bit = 144 bit = 18 字节）
    uint64_t hit_count;       // 这个地址命中了几次
    uint64_t pc;              // 触发断点的指令地址
    uint64_t lr;              // X30（返回地址）
    uint64_t sp;              // 栈指针
    uint64_t orig_x0;         // 原始的 X0（系统调用时被覆盖前）
    int32_t  syscallno;       // 若在系统调用中触发，这里是调用号
    uint64_t pstate;          // 处理器状态
    uint64_t x0 ... x29;      // 30 个通用寄存器
    uint32_t fpsr, fpcr;      // 浮点状态/控制
    __uint128_t q0 ... q31;   // 32 个 128 位 SIMD 寄存器
};
```

**逐字段大小估算**：

| 区块 | 字节 |
|---|---|
| `mask[18]` + 对齐填充 | 24 |
| `hit_count` + `pc` + `lr` + `sp` + `orig_x0` | 40 |
| `syscallno` + 填充 | 8 |
| `pstate` | 8 |
| `x0`~`x29`（30 个） | 240 |
| `fpsr` + `fpcr` | 8 |
| `q0`~`q31`（32 × 16） | 512 |
| **合计（对齐到 16）** | **约 848** |

**为什么要记这么多寄存器**：断点触发时，那一刻的 CPU 状态全在这里。
可以从中还原出"谁在什么时候访问了这个地址、访问的值是多少"。

**⑥ `bp_point` —— 一个监控点**

```cpp
struct bp_point {
    void (*on_hit)(void *regs, void *fp_regs, void *hit_point);  // 命中回调
    enum bp_type  bt;           // 断点类型
    enum bp_len   bl;           // 断点长度
    enum bp_scope bs;           // 作用线程范围
    uint64_t hit_addr;          // 监控哪个地址
    int record_count;           // 已记录的不同 PC 数量
    struct bp_record records[BP_RECORD_MAX];   // 最多 16 个命中现场
};
```

| 字段 | 说明 |
|---|---|
| `on_hit` | **函数指针**——命中时调用的回调（第 05 章讲过函数指针） |
| `records[16]` | 同一个地址可能被多处代码访问，各记一份 |
| 大小 | 约 16 × 848 = **13.5 KB** |

**⑦ `break_point` —— 整体断点配置**

```cpp
struct break_point {
    uint64_t num_brps;      // 执行断点数量
    uint64_t num_wrps;      // 访问断点数量
    int tgid;               // 属于哪个进程
    struct bp_point points[BP_CONFIG_MAX];   // 最多 16 个监控点
};
```

**大小**：16 × 13.5 KB ≈ **216 KB** —— 这是 `request_obj` 里最大的一块之一。

### 2.4 虚拟输入（3 个结构）

```cpp
struct virtual_gnss {          // 虚拟定位
    int latitude_e7;           // 纬度 × 10^7（避免浮点）
    int longitude_e7;          // 经度 × 10^7
};

struct virtual_gyro {          // 虚拟陀螺仪
    int gyro_x_mrad_s;         // 角速度，单位 mrad/s（毫弧度每秒）
    int gyro_y_mrad_s;
    int gyro_z_mrad_s;
};

struct virtual_input {         // 虚拟触摸
    int request_virtual_slots; // 请求分配几个触摸槽
    int POSITION_X, POSITION_Y;// 触摸屏的 ABS 最大值（用于坐标归一化）
    int slot;                  // 当前操作的槽位
    int x, y;                  // 触摸坐标
};
```

> [!tip] 为什么都用 `int` 而不是 `float`
> **避免浮点**。内核态处理浮点很麻烦（需要保存/恢复 FPU 状态），
> 所以约定用"放大后的整数"：
> - 纬度 `×10^7`：`39.9042°` → `399042000`
> - 角速度 `×1000`：`1.5 rad/s` → `1500 mrad/s`
>
> **这是跨内核边界通信的常见技巧**。

### 2.5 内存信息（5 个结构）

```cpp
#define MAX_MODULES      1024
#define MAX_SCAN_REGIONS 16534
#define MOD_NAME_LEN     256
#define MAX_SEGS_PER_MODULE 512

struct segment_info {        // 一个段
    short    index;          // >=0: 普通段编号；-1: BSS 段
    uint8_t  prot;           // 权限位：1=R 2=W 4=X（RX = 5）
    // 5 字节填充（为了 uint64_t 对齐）
    uint64_t start;          // 起始地址
    uint64_t end;            // 结束地址
};                           // 24 字节

struct module_info {         // 一个模块（.so）
    char   name[MOD_NAME_LEN];        // 完整路径（256 字节）
    int    seg_count;                 // 有效段数
    // 4 字节填充
    struct segment_info segs[MAX_SEGS_PER_MODULE];   // 最多 512 段
};                           // 256 + 4 + 4 + 512×24 = 12552 字节

struct region_info {         // 一个可扫描区域
    uint64_t start;
    uint64_t end;
};                           // 16 字节

struct virtual_memory {      // 内存全景
    int module_count;                            // 模块数
    // 4 字节填充
    struct module_info modules[MAX_MODULES];     // 1024 × 12552 = 12,853,248
    int region_count;                            // 区域数
    // 4 字节填充
    struct region_info regions[MAX_SCAN_REGIONS];// 16534 × 16 = 264,544
};                           // 合计约 13.1 MB ★
```

> [!danger] 这个结构有 13 MB
> 计算过程：
> ```
> modules[1024]  = 1024 × 12552 = 12,853,248 字节
> regions[16534] = 16534 × 16   =    264,544 字节
> 小计                             12,853,248 + 264,544 ≈ 12.5 MB
> 加上其它字段                        ≈ 13.1 MB
> ```
>
> **这解释了本项目的两个设计决策**：
> 1. **为什么要用固定地址 mmap**——13 MB 的内存在内核和用户态之间必须
>    映射到同一个虚拟地址，动态分配难以对齐
> 2. **为什么驱动模式比 syscall 模式快**——一次请求可以带回来整个内存全景，
>    而 syscall 模式每次都要重新解析 `/proc/pid/maps`
>
> **但也要注意代价**：这 13 MB 常驻共享内存，
> 且每次获取内存信息都要传输一遍（虽然只是内存拷贝）。

```cpp
struct virtual_memoryrw {    // 单次读写请求
    uint64_t rw_addr;             // 目标地址
    uint8_t  user_buffer[0x1000]; // 4 KB 数据缓冲
    int      size;                // 本次传输长度
};                           // 8 + 4096 + 4 = 4108 → 对齐后 4112
```

**`user_buffer` 是 4096 字节**（一个页），这就是第 79 章"分块传输"的
"每块 4096"的来源。

### 2.6 `request_op` —— 操作类型枚举

```cpp
enum request_op {
    request_op_none,                    // 空操作
    request_op_vmem_read,               // 读内存 ★
    request_op_vmem_write,              // 写内存 ★
    request_op_vmem_info,               // 获取内存全景

    request_op_touch_init,              // 初始化触摸
    request_op_touch_down,              // 按下
    request_op_touch_move,              // 移动
    request_op_touch_up,                // 抬起

    request_op_gyro_init,               // 初始化陀螺仪
    request_op_gyro_report,             // 上报陀螺仪数据

    request_op_gnss_init,               // 初始化虚拟定位
    request_op_gnss_report,             // 上报定位

    request_op_hwbp_set,                // 设置硬件断点
    request_op_hwbp_remove,             // 删除硬件断点
    request_op_ptebp_set,               // 设置 PTE UXN 断点
    request_op_ptebp_remove,
    request_op_stepbp_set,              // 设置单步断点
    request_op_stepbp_remove,
    request_op_syscall_monitor_set,     // 监控系统调用
    request_op_syscall_monitor_remove,
    request_op_cntvct_monitor_set,      // 监控读 CNTVCT_EL0（时间计数器）
    request_op_cntvct_monitor_remove,
    request_op_env_get_params,          // 获取线程环境参数
    request_op_kernel_exit,             // 让内核线程退出
};
```

**这个枚举就是"内核驱动提供的能力清单"**。逐项对照第 78 章的表格：

| 能力 | 用户态能替代吗 |
|---|---|
| `vmem_read` / `vmem_write` | ✓ `process_vm_readv` |
| `vmem_info` | ✓ 解析 `/proc/pid/maps` |
| `touch_*` | ✓ `uinput` |
| `gyro_*` | ✗ 用户态无法伪造硬件传感器 |
| `gnss_*` | ✗ 同上 |
| `hwbp_*` / `ptebp_*` / `stepbp_*` | ✗ 硬件断点需内核 |
| `syscall_monitor` | ✗ |
| `cntvct_monitor` | ✗ |
| `env_get_params` | ✗ |

### 2.7 `request_obj` —— 请求对象（共享内存的全部内容）

```cpp
struct request_obj {
    volatile bool kernel;      // 用户态 → 内核：有新请求
    volatile bool user;        // 内核 → 用户态：处理完成
    volatile enum request_op op;   // 请求类型
    volatile int status;       // 返回状态

    int tgid;                  // 目标进程 TGID

    struct virtual_memoryrw vmemrw_info;   // 读写请求（4 KB 缓冲）★
    struct virtual_memory   vmem_info;     // 内存全景（13 MB）★
    struct virtual_input    vinput_info;   // 触摸
    struct virtual_gyro     vgyro_info;    // 陀螺仪
    struct virtual_gnss     vgnss_info;    // 定位
    struct break_point      bp_info;       // 断点（216 KB）
    struct env_params       env_info;      // 环境参数
};
```

**这是整个共享内存的布局图。** 大小估算：

| 成员 | 大小 |
|---|---|
| 4 个控制字段 | 约 12 字节 |
| `tgid` | 4 |
| `vmemrw_info` | 4,112 |
| `vmem_info` | **约 13.1 MB** |
| `vinput_info` / `vgyro_info` / `vgnss_info` | 各约 24~40 |
| `bp_info` | **约 216 KB** |
| `env_info` | 约 56 |
| **合计** | **约 13.3 MB** |

**这就是那行代码背后的真相**：

```cpp
req = (request_obj *)mmap((void *)0x2025827000, sizeof(request_obj),
                          PROT_READ | PROT_WRITE,
                          MAP_SHARED | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
```

`sizeof(request_obj)` ≈ **13.3 MB** —— 映射这么大一块共享内存，
地址必须固定（`0x2025827000`），因为内核和用户态要访问同一块。

> [!warning] 固定地址映射的风险
> `0x2025827000` 这个地址是**写死的**。如果目标进程或系统库
> 恰好占用了这个地址段，`mmap` 会失败（`MAP_FIXED_NOREPLACE` 保护，不会覆盖）。
>
> **业界更稳妥的做法**：内核创建一个字符设备或 procfs 入口，
> 用户态一次系统调用拿到共享内存的地址（不需要固定地址）。

## 三、业务层结构

### 3.1 `mBase` —— 相机数据汇总（Hack.h）

```cpp
struct mBase {
    uintptr_t libUE4{};             // 模块基址
    uintptr_t UWorld{};             // 世界对象
    uintptr_t PlayerController{};   // 玩家控制器
    uintptr_t AcknowledgedPawn{};   // 自己的角色
    uintptr_t CameraManager{};      // 相机管理器
    uintptr_t CameraCache{};        // POV 缓存地址（CameraManager + 0x2300）
    uintptr_t PovPtr{};             // 同 CameraCache（历史遗留的别名）
    Vec3      Location;             // 相机位置 ★
    Rotator   Rotation{};           // 相机朝向 ★
    float     Fov{};                // 视场角 ★
};
```

| 字段 | 说明 |
|---|---|
| 7 个 `uintptr_t` | 各级指针（**注意用 `uintptr_t` 而不是 `long`**） |
| `Location/Rotation/Fov` | **实际参与计算的三个值**（第 90 章） |
| `PovPtr` | 与 `CameraCache` 重复——历史遗留，可以删 |

> [!note] `{}` 初始化列表的作用
> `uintptr_t libUE4{};` 等价于 `= 0`（C++11 起的统一初始化）。
> 好处是**避免未初始化**（第 89 章的野指针来源之一）。
> **注意 `Vec3 Location;` 没有 `{}`**——这一处是遗漏，
> 虽然 `Vec3` 的默认构造函数会把字段置 0，但风格不一致。

### 3.2 三套自瞄配置

```cpp
// SilentAim.h
struct AimConfig {
    bool  启用 = false;
    bool  无视遮挡 = false;
    bool  包含人机 = true;
    float 视野角度 = 8.0f;         // FOV 范围（度）
    float 最大距离 = 50000.f;      // 厘米
    int   骨骼部位 = 0;            // 0=根节点，其它=骨骼索引
    float 高度偏移 = 90.0f;        // 根节点模式下的瞄准高度
    int   平滑度 = 1;              // 1=瞬移，越大越慢
    float 每帧限速 = 360.0f;       // 度/帧
    bool  粘滞 = true;             // 目标粘滞
    float 扩大角度 = 15.0f;        // 粘滞时的额外范围
    float 粘滞权重 = 0.6f;         // 粘滞目标的屏距权重
    bool  开火连发 = false;        // （未使用）
    bool  画目标点 = true;
    bool  写相机POV = false;       // 是否同时写 CameraCache
};

struct AimTarget {
    bool     有效 = false;
    uint64_t actor = 0;            // 目标对象地址
    Vec3     世界位置{};
    float    屏幕距离 = 0.f;       // 到屏幕中心的距离
    float    世界距离 = 0.f;
    bool     被遮挡 = false;
};
```

```cpp
// PovAim.h
struct PovAimConfig {
    bool  enable = false;
    int   smooth = 3;
    bool  suppressInput = true;    // 用中心触摸屏蔽玩家输入
    bool  holdOnTarget = false;    // 到位后停手
    float arriveEps = 1.5f;        // 到位判定（度）
    float stepMax = 90.0f;         // 每帧最大转角
    int   touchSlot = 0;
    int   cooldown = 2;            // 释放后的冷却帧数
};
```

```cpp
// TouchAim.h
struct TouchAimConfig {
    bool 启用 = false;
    int  模式 = 1;                 // 0=绝对拖拽，1=相对增量
    int  触摸槽 = 0;
    int  平滑 = 4;
    int  起手X = -1;               // -1 表示自动
    int  起手Y = -1;
    int  停留帧 = 2;
    int  冷却帧 = 0;
};
```

**三个配置的共同点**：全部字段有默认值（`= xxx`），
这样即使配置文件损坏也能用默认值工作（第 12 章的配置兼容策略）。

### 3.3 `Config` —— 全局配置（ConfigManager.h）

```cpp
struct Config {
    int  RunFPS = 90;              // （未使用）
    bool PhysX = false;            // 是否绘制物理网格
    int  PhysXType = 0;            // 0=全量，1=只画命中
    bool syscall驱动 = false;      // ★ 后端选择

    bool 过录制 = false;           // ★ 跳过截图
    bool 方框 = false;
    bool 射线 = false;
    bool 骨骼 = false;
    bool 骨骼索引 = false;
    bool 胶囊体大小 = false;       // （未使用）
    bool 类名 = false;
    bool 人数 = true;
    bool 子弹 = false;             // （未使用）
    bool 人机 = false;
    int  gui_frame_rate = 60;

    float 骨骼偏移X/Y/Z = 0.0f;
    float 垂直焦距系数 = 1.0f;     // ★ 投影微调
    float 水平焦距系数 = 1.0f;
    bool  矩阵W2S = false;         // ★ 投影矩阵来源选择

    // 自瞄（13 项）
    bool  自瞄启用 = false;
    bool  自瞄无视遮挡 = false;
    // ... 与 AimConfig 对应
    
    // 触摸自瞄（8 项）
    // 视角自瞄（4 项）
};
```

**观察**：`Config` 是"SFO（Single Flat Object）"设计——
所有配置拍平成一个结构体，方便整体 XOR 加密后落盘（第 12、24 章）。

**代价**：

| 问题 | 说明 |
|---|---|
| 版本兼容 | 加/删字段会导致旧配置文件读错位（第 12 章的"短文件补默认值"是一种缓解） |
| 结构耦合 | 三套自瞄的配置都摊在这里，`draw_Gui.cpp` 要逐个搬运到各自的 `Cfg` |
| 无用字段 | `RunFPS`、`胶囊体大小`、`子弹` 定义了但没用 |

**更现代的做法**：用 JSON/TOML 存配置，按名字读取（加字段不破坏兼容）。

## 四、数学层结构

### 4.1 基础类型（VectorTools.h）

```cpp
struct Vec2 {
    float x, y;                     // 8 字节
    Vec2(); Vec2(float, float);
    operator+ - * /  == !=
    static Vec2 Zero();
    static float Dot(Vec2, Vec2);
};

struct Vec3 {
    float x, y, z;                  // 12 字节
    Vec3(); Vec3(float, float, float);
    operator+ - * /  == !=
    static Vec3 Zero();
    static float Dot(Vec3, Vec3);
};

struct Rotator {
    float Pitch, Yaw, Roll;         // 12 字节（单位：度）
};

struct Matrix {
    float M[4][4];                  // 64 字节
    float *operator[](int index) { return M[index]; }
};

struct Quat {
    float x, y, z, w;               // 16 字节
};

struct Transform {
    Quat  Rotation{};               // 16 字节
    Vec3  Translation;              // 12 字节
    float chunk{};                  // 4 字节填充 ★
    Vec3  Scale3D;                  // 12 字节
};                                  // 48 字节！
```

> [!tip] `Transform` 正好是 48 字节
> ```
> Quat  Rotation       16  (0x00)
> Vec3  Translation    12  (0x10)
> float chunk           4  (0x1C)  ← 显式填充
> Vec3  Scale3D        12  (0x20)
>                      4  (0x2C)  ← 隐式填充
> 总计                 48  (0x30)
> ```
>
> **这就是 `BONE_STRIDE = 48` 的来源！**
> 本项目定义了 `Transform`（48 字节，含填充），
> 但在 `variable.h` 里又定义了一个 `BoneTransform`（40 字节）用于读取。
>
> 两个结构**布局不同**：
> - `Transform`（48 字节）：`Quat → Vec3 → float → Vec3`
> - `BoneTransform`（40 字节）：`Quat → Vec3 → Vec3`
>
> **读取时必须确认目标引擎用的是哪一种布局**。
> 本项目 `variable.h` 用的是 40 字节布局 + 48 字节步长（第 07、86 章）。

### 4.2 绘制层（draw.h）

```cpp
struct Vector2A { float X, Y; };    // ★ 大写字段名
struct Vector3A { float X, Y, Z; };
struct Last_ImRect {                // 记住的 UI 窗口位置
    float Pos_x, Pos_y;
    float Size_x, Size_y;
};
```

**为什么大写字段**：这属于"三套向量"里的第二套（第 3 章附录L 讲过），
命名风格与另两套不同（`X` vs `x`）。

### 4.3 相机信息（WorldToScreen.h）

```cpp
struct MinimalViewInfo {            // "最小"的相机信息
    Vec3    Location;
    Rotator Rotation;
    float   FOV = 0.0f;
};
```

**只有三个字段** —— 这个名字取得很准：
`camMakeMatrix` 只需要这三项就能构建投影矩阵（第 49 章）。

### 4.4 触摸层（VectorStruct.h）

```cpp
struct My_Vector2 { float x, y; /* 全套运算符重载 */ };
struct My_Vector3 { float x, y, z; };
struct My_Vector4 { float x, y, z, w; };
struct _Int2 { int x, y; };
struct _Int3 { int x, y, z; };
```

**五个类型**——比另两套都全（还有 4D 和整数版本）。
`_Int2/_Int3` 用下划线开头，表示"内部使用"（但实际没有特殊含义，
下划线开头的标识符在全局作用域是保留给编译器的）。

## 五、渲染层结构

### 5.1 `AndroidImgui` —— 渲染后端抽象

```cpp
struct BaseTexData {            // 纹理基类
    void *DS = nullptr;         // 后端私有数据（Vulkan 里是 VkImageView 等）
    int Width = 0, Height = 0, Channels = 0;
};

struct TextureInfo {            // 纹理句柄（返回值）
    unsigned long long DS = 0;  // 后端句柄
    int w, h;
};

struct TextureInfo_gif {        // GIF 纹理（多帧）
    unsigned long long *DS = nullptr;   // 每帧一个句柄
    int *delays;                        // 每帧延迟
    int frames, w, h;
};

class AndroidImgui {
protected:
    ANativeWindow *m_Window;
    float m_Width, m_Height;
    std::vector<BaseTexData *> m_Textures;
public:
    char RenderName[16];        // "Vulkan"
    bool Init_Render(...);
    void NewFrame(bool resize = false);
    void EndFrame();
    void Shutdown();
private:
    virtual bool Create() = 0;                                  // 纯虚
    virtual void Setup() = 0;
    virtual void PrepareFrame(bool) = 0;
    virtual void Render(ImDrawData *) = 0;
    virtual void PrepareShutdown() = 0;
    virtual void Cleanup() = 0;
    virtual BaseTexData *LoadTexture(BaseTexData *, void *) = 0;
    virtual void RemoveTexture(BaseTexData *) = 0;
};
```

**设计要点**：

| 点 | 说明 |
|---|---|
| **非虚公有 + 虚私有** | 公有方法是模板（固定流程），私有虚函数是钩子（可变实现）——**模板方法模式** |
| `RenderName[16]` | 用 `char` 数组而不是 `std::string`（省内存，且能直接传给 `snprintf`） |
| `m_Textures` 存裸指针 | 用 `vector<BaseTexData*>`——多态需要指针（第 14 章） |
| 8 个纯虚函数 | 定义了"任何渲染后端必须实现的 8 件事" |

### 5.2 `VulkanGraphics`

```cpp
class VulkanGraphics : public AndroidImgui {
    struct VulkanTextureData : BaseTexData {   // 继承基类
        VkImageView  ImageView;
        VkImage      Image;
        VkDeviceMemory ImageMemory;
        VkSampler    Sampler;
        VkBuffer     UploadBuffer;
        VkDeviceMemory UploadBufferMemory;
    };

    VkAllocationCallbacks *m_Allocator = nullptr;
    VkInstance       m_Instance       = VK_NULL_HANDLE;
    VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
    VkDevice         m_Device         = VK_NULL_HANDLE;
    uint32_t         m_QueueFamily    = (uint32_t)-1;
    VkQueue          m_Queue          = VK_NULL_HANDLE;
    VkDebugReportCallbackEXT m_DebugReport = VK_NULL_HANDLE;
    VkPipelineCache  m_PipelineCache  = VK_NULL_HANDLE;
    VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
    std::unique_ptr<ImGui_ImplVulkanH_Window> wd{};   // 交换链数据
    int  m_MinImageCount = 2;
    bool m_SwapChainRebuild = false;
    int  m_LastWidth = 0, m_LastHeight = 0;
};
```

**两个关键点**：

| 点 | 说明 |
|---|---|
| **全部初始化为 `VK_NULL_HANDLE`** | Vulkan 句柄是 `uint64_t`，不是指针；用 `VK_NULL_HANDLE` 而不是 `nullptr` |
| `m_SwapChainRebuild` 标志 | 延迟重建：尺寸变化时置位，下一帧才重建（避免帧中重建导致闪烁） |

## 六、触摸层结构（TouchHelperA.h）

```cpp
struct touchObj {
    My_Vector2 pos{};       // 位置
    int  id = 0;            // 触点 ID（对应 ABS_MT_TRACKING_ID）
    bool isDown = false;    // 是否按下
};

struct Device {
    int   fd;                       // /dev/input/eventN 的文件描述符
    float S2TX;                     // 屏幕 → 触摸坐标的缩放系数 X
    float S2TY;
    input_absinfo absX, absY;       // X/Y 轴的取值范围（来自 EVIOCGABS）
    touchObj Finger[10];            // 最多 10 个手指 ★

    Device() { memset((void *)this, 0, sizeof(*this)); }   // ★ 构造时清零
};
```

**注意 `Device` 的构造函数**：

```cpp
Device() { memset((void *)this, 0, sizeof(*this)); }
```

这是一种"粗暴但有效"的初始化方式。**为什么不推荐**：

| 问题 | 说明 |
|---|---|
| 对非 POD 类型是 UB | 如果将来加了 `std::string` 字段，`memset` 会破坏它 |
| 绕过了构造函数 | 成员自己的构造函数不会执行 |
| 风格问题 | C++ 应该用成员初始化列表或 `= {}` |

**更安全的写法**：

```cpp
struct Device {
    int   fd        = -1;
    float S2TX      = 0.0f;
    float S2TY      = 0.0f;
    input_absinfo absX{};
    input_absinfo absY{};
    touchObj Finger[10]{};
};
```

## 七、物理层结构（PhysX.h）

**46 个类型，分四组**：

### 7.1 容器与工具（4 个）

```cpp
template <typename T>
struct TArray {                     // 模拟引擎的 TArray
    uintptr_t base;                 // 数组首地址
    int32_t   count;                // 元素个数
    int32_t   max;                  // 容量
    // ... ToVec() / operator[] / IsValid()
};                                  // 16 字节

struct FIntVector2D { int X, Y; };  // 二维整数坐标（高度场用）

struct FilterDataT {                // 碰撞过滤数据
    uint32_t word0, word1, word2, word3;    // 4 个 32 位字
};                                  // 16 字节

struct PrunerPayload {              // 唯一键（第 99 章的缓存键）
    uint64_t Shape;
    uint64_t Actor;
    bool operator==(const PrunerPayload&) const;
};

struct PrunerPayloadHash {          // 为 PrunerPayload 提供哈希
    size_t operator()(const PrunerPayload& p) const {
        return std::hash<uint64_t>()(p.Shape) ^ (std::hash<uint64_t>()(p.Actor) << 1);
    }
};

struct Int64Hash { /* ... */ };     // uint64_t 的哈希
```

**`PrunerPayloadHash` 的哈希算法**：`Shape 的哈希 XOR (Actor 的哈希 << 1)`。

> [!note] 为什么左移 1
> 直接 XOR 时，如果 `Shape` 和 `Actor` 的哈希分布相似，容易冲突。
> 左移一位能打散位模式，降低冲突率。
> **标准库的哈希组合技巧**（更完善的写法会加黄金比例常数）。

### 7.2 几何类型（10 个）

```cpp
struct PxBoxGeometry {              // 盒子
    PxGeometryType type;            // = eBOX
    PxVec3 halfExtents;             // ★ 半长（第 93 章强调过）
};

struct PxSphereGeometryT {          // 球
    PxGeometryType type;
    float radius;
};

struct PxCapsuleGeometryT {         // 胶囊
    PxGeometryType type;
    float radius;
    float halfHeight;               // 圆柱部分的半高
};

struct PxMeshScale {                // 网格缩放
    PxQuat rotation;
    PxVec3 scale;
};

struct CenterExtentsT {             // 中心 + 半长（另一种表示）
    PxVec3 center;
    PxVec3 extents;
};

struct PxPlaneT { PxVec3 n; float d; };   // 平面：n·p + d = 0

struct HullPolygonDataT {           // 凸包的一个多边形面
    uint16_t indexBase;             // 顶点索引起点
    uint8_t  numVerts;              // 顶点数
    uint8_t  flags;
};

struct ConvexHullDataT { /* 顶点 + 多边形表 */ };

struct PxConvexMeshGeometryT {      // 凸包几何
    PxGeometryType type;
    ConvexMeshT *convexMesh;        // 指向网格数据
    PxMeshScale scale;
};

struct PxTriangleMeshGeometryT {    // 三角网格几何
    PxGeometryType type;
    TriangleMeshT *triangleMesh;
    PxMeshScale    scale;
    // ...
};

struct PxHeightFieldGeometryT {     // 高度场几何
    PxGeometryType type;
    HeightFieldT *heightField;
    float heightScale;
    float rowScale;
    float columnScale;
};
```

**注意 `PxMeshScale` 里同时有旋转和缩放**——
说明网格数据可以带自己的变换（不只是 Actor 的变换）。

### 7.3 内部对象结构（14 个）

这一组是 PhysX 内部的内存布局（`Px*T` 后缀的模板）：

```cpp
struct PxsRigidCoreT { /* 刚体核心 */ };
struct BodyCoreT     { /* 刚体核心数据 */ };
struct BodyT         { /* 刚体 */ };
struct PxActorT      { /* 基类 */ };
struct PxShapeCoreT  { /* 形状核心 */ };
struct ShapeCoreT    { /* 形状核心数据 */ };
struct ShapeT        { /* 形状 */ };
struct PruningPoolT  { /* 修剪池（BVH 内部） */ };
struct PrunerExtT    { /* 修剪器扩展 */ };
struct NpSceneT      { /* 场景（新式 PhysX 内部名） */ };
struct ShapeDataT    { /* 形状数据 */ };
struct PxGeometryT   { /* 几何基类 */ };
class  PxBitAndDataT { /* 指针低位复用 */ };
struct PxPadding { /* 填充 */ };
struct PxBounds3 { PxVec3 minimum, maximum; };   // ★ 包围盒
```

> [!note] 为什么要自定义这些结构
> 这些是 PhysX **内部**的结构（不是公开 API）。
> 本项目通过逆向得到它们的布局，才敢直接读内存。
>
> **风险**：PhysX 版本升级可能改布局 → 读出来的全是垃圾。
> **这正是本项目硬编码偏移的脆弱之处**（第 88 章）。

### 7.4 Embree 封装（3 个 + 1 个工具）

```cpp
struct TriangleMeshData {           // ★ 一个待提交的网格
    std::vector<PxVec3>  Vertices{};      // 顶点
    std::vector<uint32_t> Indices{};      // 索引
    uint8_t      Flags{};
    FilterDataT  QueryFilterData{};       // 查询过滤（射线用）
    FilterDataT  SimulationFilterData{};  // 模拟过滤（碰撞用）
    PrunerPayload UniqueKey1;             // ★ 唯一键（缓存用）
    uint64_t     UniqueKey2;
    PrunerPayload UniqueKey3;
    PxGeometryType Type{};                // 来自哪种几何
    physx::PxTransform Transform;         // 世界变换
};

template <typename T, typename Hash>
class VisibleScene {                 // ★ Embree 场景封装
public:
    using KeyExtractor = T(*)(const TriangleMeshData&);   // ★ 键提取函数指针

    VisibleScene(KeyExtractor keyExtractor);
    ~VisibleScene();

    const std::vector<std::shared_ptr<TriangleMeshData>>& GetMeshDatas() const;
    void UpdateMesh(const vector<TriangleMeshData> &willAddMeshs,
                    const set<T> &RemoveKey);
    // ... Raycast 等

private:
    RTCDevice device;
    RTCScene  scene;
    std::vector<std::shared_ptr<TriangleMeshData>> mesh_datas;   // ★ 网格数据
    std::unordered_map<T, uint32_t> geometry_id_map;   // 键 → geometry ID ★
    std::set<uint32_t> disabled_geometry_ids;          // ★ ID 回收池
    T(*getKey)(const TriangleMeshData&);
};

class Throttler { /* 节流器，第 99 章 */ };
```

**`VisibleScene` 的三个关键设计**（第 99 章讲的"增量更新"的实现）：

| 成员 | 作用 |
|---|---|
| `geometry_id_map<T, uint32_t>` | 键 → Embree geometry ID 的映射，**判断"这个物体是否已存在"** |
| `disabled_geometry_ids` | **被禁用但未删除的 ID 池**——新增物体时优先复用 |
| `std::shared_ptr<TriangleMeshData>` | **共享指针**——多处引用同一份网格数据 |

**`UpdateMesh` 的三段逻辑**（从源码看到的确切流程）：

```cpp
void UpdateMesh(const vector<TriangleMeshData> &willAddMeshs,
                const set<T> &RemoveKey) {
    // ① 移除：用 rtcDisableGeometry（不是删除！）
    for (auto &key : RemoveKey) {
        if (geometry_id_map.find(key) != geometry_id_map.end()) {
            auto geometry_id = geometry_id_map[key];
            auto geometry = rtcGetGeometry(scene, geometry_id);
            rtcDisableGeometry(geometry);              // ★ 禁用
            disabled_geometry_ids.insert(geometry_id); // ★ 回收 ID
            geometry_id_map.erase(key);
        }
    }

    // ② 从 mesh_datas 里真正删掉数据（erase-remove 惯用法）
    if (!mesh_datas.empty()) {
        mesh_datas.erase(
            remove_if(mesh_datas.begin(), mesh_datas.end(),
                      [this, &RemoveKey](const shared_ptr<TriangleMeshData>& mesh) {
                          return RemoveKey.find(this->getKey(*mesh)) != RemoveKey.end();
                      }),
            mesh_datas.end());
    }

    // ③ 新增：优先复用被禁用的 ID
    for (auto &mesh : willAddMeshs) {
        if (mesh.Vertices.empty() || mesh.Indices.empty()) continue;   // ★ 跳过空网格
        RTCGeometry geom;
        bool should_release = false;
        uint32_t geometry_id = 0;
        auto mesh_copy = make_shared<TriangleMeshData>(mesh);
        mesh_datas.push_back(mesh_copy);

        if (!disabled_geometry_ids.empty()) {
            geometry_id = *disabled_geometry_ids.begin();     // ★ 复用
            disabled_geometry_ids.erase(disabled_geometry_ids.begin());
            geom = rtcGetGeometry(scene, geometry_id);
            rtcEnableGeometry(geom);                          // ★ 启用
        } else {
            geom = rtcNewGeometry(device, RTC_GEOMETRY_TYPE_TRIANGLE);  // 新建
            should_release = true;
        }
        // ... 填充顶点/索引缓冲
    }
}
```

**这三段就是第 99 章"增量更新"的完整实现**。三个可学的技巧：

| 技巧 | 收益 |
|---|---|
| `rtcDisableGeometry` 而非删除 | 避免重建 BVH 节点，快得多 |
| ID 回收池（`set<uint32_t>`） | 避免 geometry ID 无限增长 |
| `should_release` 标记 | 只在"新建"时 release，复用时不 release（否则会释放正在用的） |

> [!warning] `should_release` 这个标记很关键
> ```cpp
> bool should_release = false;
> // 复用分支：不设置 → 不 release
> // 新建分支：should_release = true → 要 release
> ```
> 如果无脑 release，会把**正在使用的几何体**释放掉 → 崩溃或画面异常。
> **这是"资源所有权"的典型场景**（第 14 章）：谁创建，谁负责释放。

## 八、结构体大小与对齐汇总

| 结构 | 大小 | 关键点 |
|---|---|---|
| `Vec2` | 8 | 2 × float |
| `Vec3` | 12 | 3 × float |
| `Rotator` | 12 | 3 × float（度） |
| `Quat` | 16 | 4 × float |
| `Matrix` | 64 | 4×4 float |
| `Transform` | **48** | 含显式填充，对应 `BONE_STRIDE` |
| `BoneTransform` | **40** | 无填充，读取时配 48 步长 |
| `Vector2A/3A` | 8 / 12 | 大写字段名 |
| `My_Vector2/3/4` | 8 / 12 / 16 | 第三套 |
| `TArray<T>` | 16 | base + count + max |
| `FilterDataT` | 16 | 4 × uint32 |
| `PrunerPayload` | 16 | Shape + Actor |
| `segment_info` | 24 | 含 5 字节填充 |
| `module_info` | **12552** | 256 + 4 + 512×24 |
| `region_info` | 16 | 2 × uint64 |
| `virtual_memory` | **约 13.1 MB** | 1024 模块 + 16534 区域 |
| `virtual_memoryrw` | 4112 | 含 4 KB 缓冲 |
| `bp_record` | **约 848** | 含 32 个 SIMD 寄存器 |
| `bp_point` | **约 13.5 KB** | 16 × 848 |
| `break_point` | **约 216 KB** | 16 × 13.5 KB |
| `request_obj` | **约 13.3 MB** | 共享内存的全部内容 |
| `mBase` | 约 88 | 7 指针 + 12 + 12 + 4 |
| `Config` | 约 200 | 40+ 字段拍平 |
| `TriangleMeshData` | 约 200 | 两个 vector + 元数据 |
| `Device`（触摸） | 约 200 | 含 10 个 touchObj |

**验证方法**（第 07 章讲过）：

```cpp
printf("sizeof(request_obj) = %zu (%.1f MB)\n",
       sizeof(request_obj), sizeof(request_obj) / 1024.0 / 1024.0);
```

**在真机上跑一遍**，对照上表——如果差异很大，说明你的理解或编译器配置不同。

## 九、数据流：结构体如何串联

```
【用户态】
Config ──────────→ 控制三套自瞄的 Cfg（AimConfig / PovAimConfig / TouchAimConfig）
  │
  └→ Settings.syscall驱动 ──→ MemDriver::Open(mode)
                                    │
【驱动层】                          ↓
request_obj（13.3 MB 共享内存）←→ 内核
  ├─ vmemrw_info ──→ 一次读写的地址与数据（4 KB）
  ├─ vmem_info ────→ 内存全景（模块 + 区域）
  ├─ vinput_info ──→ 触摸坐标
  ├─ vgyro_info ───→ 陀螺仪
  ├─ vgnss_info ───→ 定位
  ├─ bp_info ──────→ 断点配置与命中记录
  └─ env_info ─────→ 线程环境参数
                                    │
【业务层】                          ↓
mBase（AppBase）  ←── 相机三要素（Location / Rotation / Fov）
  │
  ├→ MinimalViewInfo ──→ camMakeMatrix() ──→ Matrix（视图×投影）
  │                                              │
  ├→ AimTarget ──→ SilentAim/PovAim/TouchAim ──→ 写回 PC + 0x378
  │
  └→ 每个 Actor 的 Transform 数据 ──→ WorldToScreen ──→ Vector2A（屏幕坐标）
                                                          │
【物理层】                                                ↓
TriangleMeshData[] ──→ VisibleScene::UpdateMesh ──→ Embree BVH
                                                        │
                          IsOccluded(from, to) ─────────┘
                                                        │
【渲染层】                                              ↓
AndroidImgui（抽象） ──→ VulkanGraphics ──→ ImDrawData ──→ 交换链 ──→ 屏幕
```

**一句话总结这张图**：
**配置控制行为 → 驱动取数据 → 业务算坐标 → 物理判遮挡 → 渲染画出来。**

## 十、结构体设计的三条经验

从这 134 个类型里，可以总结出三条通用经验：

### 经验 1：跨边界通信要用"扁平 + 定长"结构

```cpp
struct request_obj {
    // 全部是定长数组，没有指针、没有 std::string
    uint8_t  user_buffer[0x1000];
    struct module_info modules[1024];
    // ...
};
```

**为什么**：

| 原因 | 说明 |
|---|---|
| 内核态不能解引用用户态指针 | 共享内存里放指针毫无意义 |
| 不需要序列化 | 定长结构直接按字节拷贝 |
| 内核态不能用 STL | `std::vector` 在内核里不可用 |

**代价**：13 MB 的固定开销（即使只用其中 4 KB）。

### 经验 2：配置用"拍平 + 默认值"

```cpp
struct Config {
    int  RunFPS = 90;
    bool PhysX = false;
    // ... 40 多个字段，全部有默认值
};
```

| 优点 | 缺点 |
|---|---|
| 整体 XOR 加密简单 | 加字段破坏旧文件兼容 |
| 读写一次搞定 | 无用字段也占用空间 |
| 默认值保证能跑 | 结构耦合（三套自瞄摊在一起） |

**改进方向**：换 JSON/TOML（按名读取，天然兼容）。

### 经验 3：内部结构要标注"来源与版本"

```cpp
// PhysX.h 里的这些结构是逆向得到的内部布局
struct PxsRigidCoreT { /* ... */ };   // ← 应该加注释说明"适用于 PhysX x.x.x"
```

**本项目的问题**：这些结构没有版本标注。
一旦 PhysX 升级，维护者不知道哪些结构需要重新确认。

**正确的做法**：

```cpp
// ===== PhysX 4.1 内部结构（逆向所得，升级时需重新验证）=====
struct PxsRigidCoreT { /* ... */ };
```

> [!tip] 这一节的价值
> 结构体设计的三条经验，是你**从"能看懂"到"能设计"**的分水岭。
> 下次你自己写跨进程/跨内核的通信结构时，
> 会本能地想到"扁平、定长、有版本标注"。

→ 返回 [[00-开始之前]]
