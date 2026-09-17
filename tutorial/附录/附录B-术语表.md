---
tags: [教程, 附录, 术语]
aliases: [附录B, glossary]
---

# 附录 B · 术语表

> [!abstract] 怎么用
> 119 个术语的通俗解释，按主题分类。遇到不懂的名词回来查（另有 19 条缩写速查）。

## 内存与地址

| 术语 | 解释 |
|---|---|
| **虚拟地址** | 程序看到的地址，经 MMU 翻译成物理地址 |
| **物理地址** | 内存条上的真实地址 |
| **页（Page）** | 内存管理的最小单位，通常 4096 字节 |
| **页表** | 虚拟页号 → 物理页框号的映射表 |
| **MMU** | 内存管理单元，CPU 里负责地址翻译的硬件 |
| **ASLR** | 地址空间随机化，每次运行基址都变（安全机制） |
| **模块基址** | 一个 .so 被加载到内存的起始地址 |
| **偏移（Offset）** | 相对模块基址的距离，**编译后固定不变** |
| **段（Segment）** | 加载视角的划分（PT_LOAD 等），给内核看 |
| **节（Section）** | 链接视角的划分（.text/.data 等），给链接器看 |
| **BSS** | 未初始化的全局变量区，文件里不占空间 |
| **堆（Heap）** | 动态分配的内存，向上生长 |
| **栈（Stack）** | 函数调用用，向下生长 |
| **野指针** | 指向已释放或无效内存的指针 |
| **段错误（SIGSEGV）** | 访问了不该访问的地址 |

## 进程与系统

| 术语 | 解释 |
|---|---|
| **PID** | 进程/线程 ID |
| **TGID** | 线程组 ID，等于主线程 PID |
| **UID** | 用户 ID，Android 上每个 App 一个 |
| **Capability** | Linux 把 root 特权拆成的独立开关 |
| **SELinux** | 强制访问控制，即使 root 也可能被拒 |
| **系统调用** | 用户态请求内核服务的唯一入口 |
| **IPC** | 进程间通信 |
| **ptrace** | 调试接口，可读写目标进程内存（但会暂停它） |
| **process_vm_readv** | 批量读目标进程内存，**不暂停目标** |
| **uinput** | 内核机制，让用户态创建虚拟输入设备 |
| **内核模块（LKM）** | 可动态加载的内核代码（.ko） |
| **ELF** | Linux 的可执行文件格式 |
| **GOT/PLT** | 动态链接用的跳转表和数据表 |
| **延迟绑定** | 函数第一次调用时才解析真实地址 |
| **mangled name** | C++ 符号被编码后的名字（`_ZN...`） |
| **PIE** | 位置无关可执行文件，Android 5.0 起强制 |

## 图形与渲染

| 术语 | 解释 |
|---|---|
| **顶点（Vertex）** | 三角形的一个角，含位置和其它属性 |
| **索引（Index）** | 指向顶点的编号，三个一组构成三角形 |
| **光栅化** | 把三角形变成一堆像素 |
| **片元（Fragment）** | 待写入的像素（可能最终被丢弃） |
| **着色器** | 运行在 GPU 上的小程序 |
| **管线（Pipeline）** | 从顶点到像素的完整流程 |
| **交换链** | 多个缓冲轮转，避免画面撕裂 |
| **NDC** | 归一化设备坐标，范围 [-1,1] |
| **裁剪空间** | 顶点着色器输出的坐标空间 |
| **深度缓冲** | 记录每个像素的最近深度，用于遮挡 |
| **背面剔除** | 丢弃背对相机的三角形 |
| **Alpha 混合** | 半透明怎么与背景叠加 |
| **图集（Atlas）** | 多个小图拼成一张大纹理 |
| **SurfaceFlinger** | Android 的屏幕合成服务 |
| **BufferQueue** | 生产者-消费者模型的图形缓冲队列 |
| **ANativeWindow** | NDK 的窗口抽象，BufferQueue 的生产者端 |
| **z-order** | 图层的叠放顺序，越大越靠上 |
| **vsync** | 垂直同步信号，屏幕刷新一次发一次 |

## 数学

| 术语 | 解释 |
|---|---|
| **点积（Dot）** | 两向量的"同向程度"，垂直时为 0 |
| **叉积（Cross）** | 得到垂直于两向量的新向量 |
| **归一化** | 让向量长度变成 1，只保留方向 |
| **齐次坐标** | 加第 4 个分量 w，让平移能塞进矩阵 |
| **行主序/列主序** | 二维数组在内存里的两种排布方式 |
| **四元数** | 用 4 个数表示旋转，无万向节死锁 |
| **欧拉角** | Pitch/Yaw/Roll 三个角度，直观但有死锁 |
| **万向节死锁** | 欧拉角在某些角度丢失一个自由度 |
| **视图矩阵** | 把世界坐标变换到相机坐标 |
| **投影矩阵** | 把相机坐标变换到裁剪空间 |
| **透视除法** | 除以 w，产生近大远小 |
| **FOV** | 视野角度，越大看得越广 |
| **包围盒（AABB）** | 包住物体的长方体，用于快速排除 |
| **重心坐标** | 用三个数表示三角形内一点的位置 |
| **NaN** | 非数字，任何与它的比较都是 false |
| **Inf** | 无穷大 |

## 引擎与数据

| 术语 | 解释 |
|---|---|
| **Actor** | 引擎里的一个对象（角色、箱子等） |
| **Component** | 挂在 Actor 上的功能模块 |
| **FName** | UE4 的名字系统，用整数 ID 代替字符串 |
| **UWorld** | UE4 的世界对象，场景的根 |
| **ULevel** | 关卡，包含 Actor 数组 |
| **Pawn** | 可被控制的角色 |
| **PlayerController** | 玩家控制器，持有 Pawn 和相机 |
| **CameraManager** | 相机管理器，含 CameraCache |
| **ControlRotation** | 玩家输入的朝向（意图） |
| **POV / CameraCache** | 相机的实际位置和朝向（结果） |
| **骨骼（Bone）** | 角色骨架上的一根骨 |
| **骨骼数组** | 所有骨骼的变换数据，步长固定 |
| **C2W** | Component To World，组件到世界的变换矩阵 |
| **碰撞体（Collider）** | 物理引擎使用的简化形状 |
| **BVH** | 层次包围盒树，加速光线求交 |
| **SAH** | 表面积启发式，构建高质量 BVH 的算法 |
| **Embree** | Intel 的高性能光线追踪库 |
| **遮挡（Occlusion）** | 目标被其它物体挡住 |

## 工程

| 术语 | 解释 |
|---|---|
| **交叉编译** | 在 A 机器上编译出能在 B 机器运行的程序 |
| **ABI** | 应用二进制接口（指令集 + 调用约定） |
| **API level** | Android 版本号（如 25 = 7.1） |
| **STL** | C++ 标准库实现（Android 用 libc++） |
| **静态链接** | 库代码编进产物（自包含但大） |
| **动态链接** | 运行时加载 .so（小但依赖） |
| **dlopen/dlsym** | 运行时手动加载库并取符号 |
| **LTO** | 链接时优化，能减小体积 |
| **strip** | 删除符号表，减小体积 |
| **内联（inline）** | 把函数代码直接展开到调用处 |
| **RAII** | 用对象生命周期管理资源 |
| **虚函数** | 运行期决定调用哪个实现（多态） |
| **模板** | 编译期生成代码的"配方" |
| **互斥锁** | 拿不到就睡眠的锁 |
| **自旋锁** | 拿不到就死循环的锁（临界区极短时用） |
| **原子操作** | 不可被打断的单步操作 |
| **数据竞争** | 多线程同时读写同一数据且无同步 |
| **死锁** | 多个线程互相等对方释放锁 |
| **节流（Throttle）** | 限制操作执行频率 |
| **增量更新** | 只处理变化的部分 |

## 触摸与输入

| 术语 | 解释 |
|---|---|
| **/dev/input/eventN** | 每个输入设备的字符设备节点，读它拿到原始事件 |
| **getevent** | adb 命令，dump 输入设备的事件流 |
| **ABS_MT_\*** | 多点触控的绝对坐标事件（POSITION_X/Y、SLOT、TRACKING_ID） |
| **SYN_REPORT** | 一帧事件的"提交"标记，没有它事件不生效 |
| **EVIOCGRAB** | ioctl，独占抓取输入设备（抓取后系统收不到） |
| **uinput** | 内核机制，让用户态创建虚拟输入设备 |
| **InputReader** | Android 从 /dev/input 读事件的系统服务 |
| **InputDispatcher** | 把事件分发给 App 的系统服务 |
| **pointer_location** | 开发者选项，屏幕上显示触摸点位置 |

## 性能

| 术语 | 解释 |
|---|---|
| **帧时间（frame time）** | 渲染一帧耗时，60fps = 16.67ms |
| **vsync** | 屏幕刷新同步信号，限制帧率上限 |
| **atrace** | Android 的 systrace 抓取工具 |
| **perfetto** | 新一代系统 trace 工具（替代 systrace） |
| **simpleperf** | Android 上的 CPU 采样剖析器 |
| **Framerate** | ImGui 内置的帧率统计（`io.Framerate`） |
| **节流（Throttle）** | 限制操作执行频率（本项目用 16ms 节流） |
## 缩写速查

| 缩写 | 全称 |
|---|---|
| W2S | World To Screen，世界坐标转屏幕坐标 |
| ESP | 在游戏辅助语境指"透视显示"（本教程泛指信息显示） |
| FOV | Field Of View |
| AABB | Axis-Aligned Bounding Box |
| BVH | Bounding Volume Hierarchy |
| NDC | Normalized Device Coordinates |
| PIE | Position Independent Executable |
| PIC | Position Independent Code |
| IPC | Inter-Process Communication |
| LKM | Loadable Kernel Module |
| PSS | Proportional Set Size（实际内存占用） |
| RSS | Resident Set Size |
| TLS | Thread Local Storage |
| SIMD | Single Instruction Multiple Data |
| MT | Multi-Touch（多点触控） |
| fps | frames per second |
| W2S | World To Screen |
| MT | Multi-Touch |
| fps | frames per second |

→ 返回 [[00-开始之前]]
