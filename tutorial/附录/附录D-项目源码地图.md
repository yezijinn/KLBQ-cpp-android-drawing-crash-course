---
tags: [教程, 附录, 源码地图]
aliases: [附录D, codemap]
---

# 附录 D · KLBQ 项目源码地图

> [!abstract] 怎么用
> 本教程分析的项目（KLBQ）每个文件的作用速查。
> 学到某一章时，回来对照真实代码看。
>
> **需要"106 个文件逐个确认无遗漏"** → 用 [[附录L-项目文件全覆盖手册]]。
> 本附录按功能分组给"看什么该翻哪个文件"，附录 L 按文件给"这个文件由哪一章讲"。

## 目录总览

```
KLBQ/
├── jni/
│   ├── Android.mk              构建脚本（模块定义）
│   ├── Application.mk          全局构建配置
│   ├── include/                所有头文件
│   └── src/                    所有源文件
├── libs/arm64-v8a/             产物：Android_imgui_Vulkan.rc
├── obj/local/arm64-v8a/        中间产物（.o / .d）
└── .vscode/                    编辑器配置
```

## 构建层

| 文件 | 作用 | 对应章节 |
|---|---|---|
| `jni/Android.mk` | 模块定义：源文件、include 路径、链接库 | 29、34 |
| `jni/Application.mk` | ABI / API / STL / 优化选项 | 29、34、41 |
| `.vscode/c_cpp_properties.json` | IntelliSense 配置 | 39 |

## 入口与主循环

| 文件 | 作用 | 章节 |
|---|---|---|
| `jni/src/main.cpp` | 主函数：初始化 → 主循环 → 清理 | 03、61、100 |

主循环结构：

```
ConfigManager::LoadConfig()          配置
GraphicsManager::getGraphicsInterface()  渲染后端
screen_config()                      屏幕信息
ANativeWindowCreator::Create()       创建窗口
graphics->Init_Render()              Vulkan 初始化
Touch::Init()                        触摸
init_My_drawdata()                   字体与样式
while: drawBegin → UpdateImGuiInput → NewFrame
       → Layout_tick_UI → pXthread
       → DrawPlayer → DrawESP → EndFrame
```

## 渲染层（卷五）

| 文件 | 作用 | 章节 |
|---|---|---|
| `include/ImGui/AndroidImgui.h` | 渲染后端抽象接口（抽象类） | 58 |
| `src/ImGui/AndroidImgui.cpp` | 基类实现（Init/NewFrame/EndFrame） | 58 |
| `include/Vulkan/GraphicsManager.h` | 工厂 | 58 |
| `src/Vulkan/GraphicsManager.cpp` | 返回 `VulkanGraphics` 实例 | 58 |
| `include/Vulkan/VulkanGraphics.h` | Vulkan 后端声明 | 58、59 |
| `src/Vulkan/VulkanGraphics.cpp` | Vulkan 初始化、交换链、渲染 | 58、59、62 |
| `include/Vulkan/vulkan_wrapper.h` | 动态加载函数指针声明 | 60 |
| `src/Vulkan/vulkan_wrapper.cpp` | dlopen + dlsym 实现 | 60 |
| `include/ImGui/ANativeWindowCreator.h` | 窗口创建（dlsym libgui）★ | 64、65、66、67、68 |
| `include/ImGui/my_imgui_impl_android.h` | ImGui 的 Android 适配 | 61 |
| `src/ImGui/my_imgui_impl_android.cpp` | 同上实现 | 61 |
| `include/ImGui/TouchHelperA.h` | 触摸辅助 | 69、70、71 |
| `src/ImGui/TouchHelperA.cpp` | /dev/input + uinput 实现 | 69、70、71 |
| `src/ImGui/heiti_ttf.cpp` | 内嵌黑体字体数组 | 63 |
| `include/ImGui/embedded_assets.h` | 字体数组的声明 | 63 |
| `src/ImGui/imgui*.cpp`（4 个） | Dear ImGui 源码 | 42 |
| `src/ImGui/imgui_impl_vulkan.cpp` | ImGui Vulkan 后端 | 62 |
| `src/ImGui/stb_image.cpp` | 图像加载 | 42 |

## 数据获取层（卷六）

| 文件 | 作用 | 章节 |
|---|---|---|
| `src/Android_draw/driver.h` | **驱动统一接口 `IDriver` + 内核 `Driver`** ★ | 79、80 |
| `include/My_Utils/SysHal.h` | 系统调用后端（process_vm_readv） | 74、75 |
| `src/My_Utils/SysHal.cpp` | 保证 inline 实例唯一 | 75 |
| `include/My_Utils/MemDriver.h` | 双后端选择器 | 80 |
| `include/My_Utils/ConfigManager.h` | 配置结构 | 12 |
| `src/My_Utils/ConfigManager.cpp` | 配置存取（XOR + /data/Config） | 22、24 |

`driver.h` 里的关键结构：

```
SpinLock                自旋锁
request_op              操作类型枚举
virtual_memoryrw        读写请求（地址 + 4KB 缓冲）
virtual_memory          模块与区域信息
bp_point / bp_record    硬件断点
request_obj             共享内存请求对象 ★
IDriver                 抽象接口
Driver                  内核驱动实现
```

## 业务层（卷七）

| 文件 | 作用 | 章节 |
|---|---|---|
| `src/Android_draw/draw_Gui.cpp` | **核心：UpdateGameData + DrawPlayer + UI** ★ | 84、85、86、90 |
| `include/Android_draw/draw.h` | 全局数据声明（Vector2A/3A 等） | 45 |
| `src/Android_draw/variable.h` | 全局变量定义 + 骨骼工具函数 | 85、86 |
| `include/Android_draw/WorldToScreen.h` | 投影管线（camMakeMatrix + W2S）★ | 49、50、51、91 |
| `include/Android_draw/Draw_ESP.h` | ESP 绘制声明 | 93 |
| `src/Android_draw/Draw_ESP.cpp` | PhysX 网格绘制 | 94、98 |
| `src/Hack/Hack.cpp` | 刷新 `AppBase`（相机/POV） | 90 |
| `include/Hack/Hack.h` | `mBase` 结构定义 | 90 |
| `include/Hack/SilentAim.h` | 静默自瞄 | 47、56 |
| `include/Hack/PovAim.h` | 视角自瞄 | 49、90 |
| `include/Hack/TouchAim.h` | 触摸自瞄 | 70 |
| `include/My_Utils/GameTools.h` | 数学工具 | 47 |
| `src/My_Utils/GameTools.cpp` | rotatorToMatrix / GetForward / W2S 封装 | 47、51 |
| `include/My_Utils/VectorTools.h` | Vec2/Vec3/Rotator/Matrix/Quat | 45、46 |

## 物理与可见性层（卷八）

| 文件 | 作用 | 章节 |
|---|---|---|
| `include/Embree/PhysX.h` | **PhysX 结构 + Embree 场景 + 几何重建** ★ | 93、94、96、97、98、99 |
| `include/Embree/rtcore*.h` | Embree 公共 API | 97 |
| `include/Embree/foundation/` | PhysX 数学基础 | 47 |
| `include/Embree/*.a`（6 个） | Embree 预编译静态库 | 43 |

`PhysX.h` 里的关键内容：

```
PxGeometryType          七种几何类型枚举
TArray<T>               引擎通用数组
PrunerPayload           Shape/Actor 唯一键
PxTransform             位置 + 四元数
TriangleMeshData        顶点 + 索引
VisibleScene            Embree 场景封装（Raycast / UpdateMesh）
LineTrace               射线追踪工具
VisibleCheck            三个场景的更新线程入口
Throttler               节流器
```

## 全局数据流

```
main.cpp
  │
  ├─ ConfigManager::LoadConfig()          /data/Config
  ├─ GraphicsManager                      → VulkanGraphics
  ├─ ANativeWindowCreator::Create()       → ANativeWindow
  ├─ Touch::Init()                        → /dev/input + uinput
  │
  └─ while:
       ├─ drawBegin()                     帧率 + 尺寸变化
       ├─ Touch::UpdateImGuiInput()       → ImGuiIO
       ├─ graphics->NewFrame()
       │
       ├─ Layout_tick_UI()
       │    └─ UpdateGameData()            ← dr->Read(...)
       │         ├─ GName / MatrixPtr / UWorld
       │         ├─ ULevel → Actor 数组
       │         ├─ PlayerController → Pawn / CameraManager
       │         └─ CameraCache (位置/朝向/FOV)
       │
       ├─ pXthread()                      → AppBase（相机汇总）
       │    └─ GameTools::WorldMatrix
       │
       ├─ DrawPlayer()
       │    ├─ camMakeMatrix()            视图×投影
       │    ├─ 遍历 Actor
       │    │    ├─ GetNameById()         名字
       │    │    ├─ IsValidObject()       过滤
       │    │    ├─ WorldToScreen()       投影
       │    │    ├─ 射线被遮挡()           Embree 查询
       │    │    └─ GetBoneWorldPos()     骨骼
       │    └─ draw->AddRect/Line/Circle
       │
       ├─ DrawESP()                        PhysX 线框
       └─ graphics->EndFrame()
```

## 关键文件速查（按想看什么）

| 想看… | 看这个文件 |
|---|---|
| 主循环怎么组织 | `src/main.cpp` |
| 怎么读游戏数据 | `src/Android_draw/draw_Gui.cpp` 的 `UpdateGameData()` |
| 怎么算屏幕坐标 | `include/Android_draw/WorldToScreen.h` |
| 怎么画方框骨骼 | `src/Android_draw/draw_Gui.cpp` 的 `DrawPlayer()` |
| 怎么跨进程读写 | `src/Android_draw/driver.h` 的 `IDriver` |
| 系统调用后端 | `include/My_Utils/SysHal.h` |
| 怎么创建悬浮窗 | `include/ImGui/ANativeWindowCreator.h` |
| 怎么处理触摸 | `src/ImGui/TouchHelperA.cpp` |
| 怎么判断遮挡 | `include/Embree/PhysX.h` 的 `VisibleScene::Raycast` |
| 怎么加中文 | `src/main.cpp` 附近的 `M_Android_LoadFont` |
| 配置怎么存 | `src/My_Utils/ConfigManager.cpp` |
| 构建怎么配 | `jni/Android.mk` + `jni/Application.mk` |

## 与教程章节的对应

| 卷 | 主要对应文件 |
|---|---|
| 卷一 编程地基 | （基础，无对应） |
| 卷二 系统与底层 | （原理，无对应） |
| 卷三 Android 原生 | `Android.mk` `Application.mk` |
| 卷四 3D 数学 | `VectorTools.h` `GameTools.cpp` `WorldToScreen.h` |
| 卷五 图形与界面 | `VulkanGraphics.*` `ANativeWindowCreator.h` `TouchHelperA.cpp` |
| 卷六 跨进程内存 | `driver.h` `SysHal.h` `MemDriver.h` |
| 卷七 引擎数据模型 | `draw_Gui.cpp` `variable.h` `Hack.*` |
| 卷八 物理与可见性 | `PhysX.h` `Draw_ESP.cpp` |

→ 返回 [[00-开始之前]]
