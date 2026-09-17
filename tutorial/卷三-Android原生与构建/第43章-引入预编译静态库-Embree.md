---
tags: [教程, 卷三, Android, Embree, 静态库]
day: 27
aliases: [ch43]
---

# 第 43 章 · 引入预编译静态库：Embree

> [!abstract] 本章目标
> 把 6 个 `.a` 文件正确链接进工程，并跑通第一个 Embree 光线投射。
> 本项目最复杂的第三方依赖，第 97 章会正式用它。

> [!note] 承上
> 上一章引入了"源码形式"的第三方库（ImGui）。
> 本章引入"**预编译静态库**"形式——Embree（卷八做遮挡判定用它）。

## 先看 Embree 是什么

**Intel Embree**：高性能光线追踪内核库。
给它一堆三角形，它构建 BVH 加速结构，然后你可以问：
"从 A 点射向 B 点，撞到什么了吗？"

> [!note] 两个缩写
> - **BVH = Bounding Volume Hierarchy**（包围体层次结构）：把一堆三角形组织成树，让"射线撞到谁"不用逐个三角形试，快很多。
> - **RTC = Ray Tracing Core**：Embree 所有 API 的前缀（`rtcNewDevice`、`rtcIntersect1`……），表示"光线追踪核心"。

```cpp
RTCDevice device = rtcNewDevice(nullptr);
RTCScene  scene  = rtcNewScene(device);
// 添加几何...
rtcCommitScene(scene);

RTCRayHit rayhit;
/* 设置起点、方向 */
rtcIntersect1(scene, &rayhit, nullptr);

if (rayhit.hit.geomID != RTC_INVALID_GEOMETRY_ID) {
    // 撞到了
}
```

本项目用它做**遮挡判定**：从相机向目标发一条射线，
命中说明被墙挡住（第 95~99 章）。

## 6 个库的关系

```
libembree4.a      核心：BVH 构建、遍历、几何管理    ← 最上层（用其它库）
liblexers.a       解析器（.obj 加载等）
libmath.a         数学工具
libsys.a          系统抽象（线程、文件、内存）
libtasking.a      任务调度（多线程并行构建）
libsimd.a         SIMD 抽象（SSE/AVX/NEON）          ← 最底层（被其它库用）
```

依赖方向：**上层在前、下层在后；被依赖的库放右边**。上面的排列与下方 `LOCAL_STATIC_LIBRARIES` 的顺序一致。

```makefile
LOCAL_STATIC_LIBRARIES := \
    embree_prebuilt \
    lexers_prebuilt \
    math_prebuilt \
    sys_prebuilt \
    task_prebuilt \
    simd_prebuilt
```

（第 28 章讲过：被依赖的库放右边。）

## 在 Android.mk 中声明

```makefile
include $(CLEAR_VARS)
LOCAL_MODULE := embree_prebuilt
LOCAL_SRC_FILES := include/Embree/libembree4.a
include $(PREBUILT_STATIC_LIBRARY)
```

重复 6 次（每个库一个块）。然后在主模块引用：

```makefile
LOCAL_STATIC_LIBRARIES := embree_prebuilt lexers_prebuilt ...
```

头文件路径：

```makefile
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include/Embree
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include/Embree/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include/Embree/foundation
```

## 头文件结构

Embree 的公共 API 在 `include/rtcore*.h`：

| 头文件 | 内容 |
|---|---|
| `rtcore.h` | 主头文件（包含下面全部） |
| `rtcore_common.h` | 基础类型、`RTCDevice`、`rtcNewDevice` |
| `rtcore_device.h` | 设备属性 |
| `rtcore_scene.h` | 场景管理 |
| `rtcore_geometry.h` | 几何体（三角形、四边形、实例） |
| `rtcore_ray.h` | 射线结构 `RTCRay` `RTCRayHit` |
| `rtcore_buffer.h` | 共享缓冲区 |
| `rtcore_builder.h` | BVH 构建参数（Embree 4 新增） |
| `rtcore_quaternion.h` | 四元数（用于方向） |

`foundation/` 是 PhysX 风格的数学基础（本项目 `PhysX.h` 也引用了它）。

## 第一个 Embree 程序

```cpp
#include <embree4/rtcore.h>
#include <cstdio>
#include <cmath>

int main(void) {
    // 1. 创建设备
    RTCDevice device = rtcNewDevice(nullptr);
    if (!device) { printf("创建设备失败\n"); return 1; }

    // 2. 创建场景
    RTCScene scene = rtcNewScene(device);
    rtcSetSceneBuildQuality(scene, RTC_BUILD_QUALITY_MEDIUM);

    // 3. 创建一个三角形几何
    RTCGeometry geom = rtcNewGeometry(device, RTC_GEOMETRY_TYPE_TRIANGLE);

    // 顶点：一个在 z=10 的三角形（当作"墙"）
    struct Vertex { float x, y, z, r; };
    Vertex *verts = (Vertex*)rtcSetNewGeometryBuffer(
        geom, RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3, sizeof(Vertex), 3);
    verts[0] = {-5, -5, 10, 0};
    verts[1] = { 5, -5, 10, 0};
    verts[2] = { 0,  5, 10, 0};

    // 索引：一个三角形
    unsigned *idx = (unsigned*)rtcSetNewGeometryBuffer(
        geom, RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT3, sizeof(unsigned)*3, 1);
    idx[0] = 0; idx[1] = 1; idx[2] = 2;

    rtcCommitGeometry(geom);
    rtcAttachGeometry(scene, geom);
    rtcReleaseGeometry(geom);
    rtcCommitScene(scene);

    // 4. 发一条射线：从原点朝 +Z
    RTCRayHit rayhit;
    rayhit.ray.org_x = 0; rayhit.ray.org_y = 0; rayhit.ray.org_z = 0;
    rayhit.ray.dir_x = 0; rayhit.ray.dir_y = 0; rayhit.ray.dir_z = 1;
    rayhit.ray.tnear = 0;
    rayhit.ray.tfar  = 1000;
    rayhit.ray.mask  = -1;
    rayhit.ray.flags = 0;
    rayhit.hit.geomID = RTC_INVALID_GEOMETRY_ID;

    RTCIntersectArguments args;
    rtcInitIntersectArguments(&args);        // Embree 4：初始化参数结构体
    rtcIntersect1(scene, &rayhit, &args);    // Embree 4：scene, rayhit, args

    // 5. 看结果
    if (rayhit.hit.geomID != RTC_INVALID_GEOMETRY_ID) {
        printf("命中！距离 = %.2f\n", rayhit.ray.tfar);
    } else {
        printf("未命中\n");
    }

    rtcReleaseScene(scene);
    rtcReleaseDevice(device);
    return 0;
}
```

预期输出：`命中！距离 = 10.00`

## 三个易错点

**1. `rtcIntersect1` 的参数顺序（Embree 4）**

Embree 4 **改了 `rtcIntersect1` 的签名**，上下文类型从 `RTCIntersectContext` 换成了 `RTCIntersectArguments`，参数顺序也变了：

```cpp
// —— Embree 4（本项目用的版本）——
RTCIntersectArguments args;
rtcInitIntersectArguments(&args);
rtcIntersect1(scene, &rayhit, &args);    // 顺序：scene, rayhit, args

// 不需要高级参数时，args 可以传 NULL：
rtcIntersect1(scene, &rayhit, nullptr);

// —— Embree 3（旧版本，不要照抄）——
// RTCIntersectContext ctx;
// rtcInitIntersectContext(&ctx);
// rtcIntersect1(scene, &ctx, &rayhit);  // 旧顺序：scene, context, rayhit
```

> [!danger] 别把两代 API 混用
> - Embree 3：类型 `RTCIntersectContext`，函数 `rtcInitIntersectContext`，调用 `rtcIntersect1(scene, &ctx, &rayhit)`
> - **Embree 4：类型 `RTCIntersectArguments`，函数 `rtcInitIntersectArguments`，调用 `rtcIntersect1(scene, &rayhit, &args)`**
>
> 混用会编译报错（类型不匹配）或行为错误。**认准版本，看 `rtcore_ray.h` / `rtcore_scene.h` 里的声明。**

> [!note] 本项目实际怎么调的
> 本项目的遮挡判定不需要 filter 等高级参数，所以直接传 `nullptr`（`jni/include/Embree/PhysX.h`）：
> ```cpp
> rtcIntersect1(this->scene, &rayhit, nullptr);   // 第 3 参传 NULL 即可
> ```
> 上面的 `RTCIntersectArguments` 写法是"需要高级参数时"的完整形式，两者签名一致。

**2. 必须先 `rtcCommitGeometry` 再 `rtcCommitScene`**

```cpp
rtcCommitGeometry(geom);      // 提交几何
rtcAttachGeometry(scene, geom);
rtcCommitScene(scene);        // 构建 BVH
```

改了顶点数据后要重新 commit。

**3. `RTCRayHit` 要先初始化 `hit.geomID`**

```cpp
rayhit.hit.geomID = RTC_INVALID_GEOMETRY_ID;
```

不初始化的话，即使没命中也是垃圾值。

## 缓冲区的格式

```cpp
rtcSetNewGeometryBuffer(geom, type, slot, format, byteStride, itemCount);
```

| 参数 | 值 |
|---|---|
| `type` | `RTC_BUFFER_TYPE_VERTEX` / `RTC_BUFFER_TYPE_INDEX` |
| `slot` | 通常 0（多套 UV 时才用 1、2...） |
| `format` | `RTC_FORMAT_FLOAT3` / `RTC_FORMAT_UINT3` |
| `byteStride` | 每个元素的字节数（可以是结构体大小） |
| `itemCount` | 元素个数 |

**本项目用的是 `RTC_FORMAT_FLOAT3` + 自定义 stride**。

> [!note] stride 可以大于格式要求
> 顶点结构里可以带额外字段（如 `r` 填充），
> 只要 `byteStride = sizeof(Vertex)` 正确，Embree 会按 stride 跳着读。

## 检查链接是否成功

```bash
# 看符号有没有进来
aarch64-linux-android-nm -C app | grep -c 'rtc'

# 如果为 0，说明库没链进去，检查：
# 1. LOCAL_STATIC_LIBRARIES 里有没有
# 2. 路径对不对
# 3. 链接顺序对不对
```

## 常见链接错误

| 报错 | 原因 | 解法 |
|---|---|---|
| `undefined reference to rtcNewDevice` | 没链 `libembree4.a` | 检查 `LOCAL_STATIC_LIBRARIES` |
| `undefined reference to TaskScheduler::...` | 没链 `libtasking.a` | 补库 |
| `undefined reference to __builtin_ia32_...` | 架构不匹配（x86 指令） | 确认 .a 是 arm64 版 |
| `file not recognized: File format not recognized` | .a 损坏或架构不对 | `file libembree4.a` 检查 |
| `skipping incompatible libembree4.a` | ABI 不匹配 | 换 arm64 版本 |

## 确认 .a 的架构

```bash
# Windows 上用 NDK 的 readelf
aarch64-linux-android-readelf -h libembree4.a | grep Machine | sort -u
# Machine: AArch64

# 或者用 nm（不指定架构也能猜）
nm libembree4.a 2>&1 | head -3
```

如果输出 `File format not recognized`，说明架构不对。

## 线程数控制

Embree 默认用所有核心构建 BVH。在手机上可能过热或抢占游戏进程的 CPU：

```cpp
// 限制线程数（Embree 4）
rtcSetDeviceProperty(device, RTC_DEVICE_PROPERTY_TASKING_SYSTEM, ...);

// 或者在创建时通过配置字符串
RTCDevice device = rtcNewDevice("threads=2,verbose=0");
```

本项目创建了 3 个后台线程分别更新 3 个场景，
如果 Embree 内部再开多线程，线程数会爆炸——**限制线程数是必要的优化**。

## 动手：从零引入一个静态库

任务：自己做一个 `.a`，按本章的方式引入。

```bash
# 1. 写库
cat > mylib.c << 'EOF'
int double_it(int x) { return x * 2; }
EOF

aarch64-linux-android21-clang -c mylib.c -o mylib.o
ar rcs libmylib.a mylib.o

# 2. 放到工程
mkdir -p jni/include/Mine
cp libmylib.a jni/include/Mine/

# 3. Android.mk
# include $(CLEAR_VARS)
# LOCAL_MODULE := mylib_prebuilt
# LOCAL_SRC_FILES := include/Mine/libmylib.a
# include $(PREBUILT_STATIC_LIBRARY)
# 
# 主模块: LOCAL_STATIC_LIBRARIES += mylib_prebuilt

# 4. 编译，故意不加 LOCAL_STATIC_LIBRARIES，看报错
# 5. 加上，编译通过
```

体会"少了库 → undefined reference → 补上"的完整流程。
Embree 的 6 个库也是同样的道理，只是数量多、顺序有讲究。

## 动手验证清单

- [ ] **确认 6 个 .a 都在**：`ls include/Embree/*.a` → 6 个文件
- [ ] **声明预编译库**：`Android.mk` 里 6 个 `PREBUILT_STATIC_LIBRARY` 块
- [ ] **按序引用**：`LOCAL_STATIC_LIBRARIES` 里 simd 在最后（被依赖的放右边）
- [ ] **编译**：`ndk-build` → 无满屏 `undefined reference`
- [ ] **故意打乱顺序**：把 embree4 移到最右 → 观察链接失败
- [ ] **恢复正确顺序**：embree4 → lexers → math → sys → task → simd

> [!danger] 顺序错了会满屏 undefined reference
> 若确实顺序正确还报错，用 `-Wl,--start-group ... --end-group` 包住，或 `nm -u libembree4.a` 查它还需要什么。

## 验收清单

- [ ] 知道 Embree 是干什么的（BVH + 光线求交）（说出"构建 BVH，回答射线撞到什么"）
- [ ] 能说出 6 个库的分工和依赖方向（embree/lexers/math/sys/tasking/simd 各管什么）
- [ ] 会在 Android.mk 里声明预编译静态库（写一个 PREBUILT_STATIC_LIBRARY 块）
- [ ] 知道 `PREBUILT_STATIC_LIBRARY` 的三要素（LOCAL_MODULE/LOCAL_SRC_FILES/include 指令）
- [ ] 会写第一个 Embree 程序：建设备/场景/三角形/射线 ★（运行输出"命中！距离 = 10.00"）
- [ ] 知道 `rtcCommitGeometry` → `rtcCommitScene` 的顺序（说出"先提交几何再提交场景构 BVH"）
- [ ] 知道要先初始化 `hit.geomID = RTC_INVALID_GEOMETRY_ID`（说出"否则没命中也是垃圾值"）

→ 下一章：[[第44章-工程目录与include约定]]　—— 建立一套能长期维护的目录规范，并复刻本项目的组织结构。
