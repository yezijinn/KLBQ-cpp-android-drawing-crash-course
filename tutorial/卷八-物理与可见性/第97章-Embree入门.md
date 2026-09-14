---
tags: [教程, 卷八, Embree, 光线追踪]
day: 59
aliases: [ch97]
---

# 第 97 章 · Embree 入门

> [!abstract] 本章目标
> 掌握 Embree 的四个核心对象和标准流程。
> 本项目用它做遮挡判定（第 43 章已把库接进工程）。

## 先看四个核心对象

| 对象 | 作用 | 类比 |
|---|---|---|
| `RTCDevice` | 全局上下文，管理资源 | 进程单例 |
| `RTCScene` | 一个场景（含 BVH） | 一个世界 |
| `RTCGeometry` | 一个几何体 | 一个物体 |
| `RTCRayHit` | 射线 + 命中结果 | 一次查询 |

关系：

```
RTCDevice
  └─ RTCScene
       ├─ RTCGeometry (id=0)
       ├─ RTCGeometry (id=1)
       └─ ...
       （内部构建 BVH）

查询：rtcIntersect1(scene, &ctx, &rayhit)
```

## 完整流程

```
1. rtcNewDevice()              创建设备
2. rtcNewScene(device)         创建场景
3. rtcNewGeometry()            创建几何
4. rtcSetNewGeometryBuffer()   填顶点/索引
5. rtcCommitGeometry()         提交几何
6. rtcAttachGeometry()         挂到场景
7. rtcCommitScene()            构建 BVH ★
8. rtcIntersect1()             查询
9. rtcReleaseScene/Device()    清理
```

## 步骤 1：创建设备

```cpp
#include <embree4/rtcore.h>

RTCDevice device = rtcNewDevice(nullptr);
if (!device) {
    printf("创建设备失败\n");
    return 1;
}
```

参数可以是配置字符串：

```cpp
RTCDevice device = rtcNewDevice("threads=2,verbose=0");
```

| 选项 | 含义 |
|---|---|
| `threads=N` | 构建/查询使用的线程数 |
| `verbose=N` | 日志详细程度（0=静默） |
| `set_affinity=1` | 绑定 CPU 核心 |

> [!tip] 手机上限制线程数
> 游戏本身要用 CPU，Embree 再占满会互相抢。
> 本项目创建了 3 个更新线程，如果 Embree 内部再多线程，
> 线程数会爆炸。建议 `threads=1` 或 `2`。

### 错误处理回调

```cpp
void ErrorHandler(void* userPtr, RTCError code, const char* str) {
    printf("Embree 错误 [%d]: %s\n", code, str);
}
rtcSetDeviceErrorFunction(device, ErrorHandler, nullptr);
```

## 步骤 2：创建场景

```cpp
RTCScene scene = rtcNewScene(device);
rtcSetSceneBuildQuality(scene, RTC_BUILD_QUALITY_MEDIUM);
```

构建质量：

| 质量 | 构建速度 | 查询速度 | 适用 |
|---|---|---|---|
| `LOW` | 最快 | 最慢 | 快速预览 |
| `MEDIUM` | 中 | 中 | **动态场景** |
| `HIGH` | 慢 | 快 | 静态场景 |
| `REFIT` | 极快 | 视情况 | 只更新包围盒 |

## 步骤 3-4：创建几何并填数据

```cpp
RTCGeometry geom = rtcNewGeometry(device, RTC_GEOMETRY_TYPE_TRIANGLE);

// 顶点缓冲
struct Vertex { float x, y, z, r; };
Vertex *verts = (Vertex*)rtcSetNewGeometryBuffer(
    geom,
    RTC_BUFFER_TYPE_VERTEX,      // 顶点缓冲
    0,                            // slot
    RTC_FORMAT_FLOAT3,            // 每顶点 3 个 float
    sizeof(Vertex),               // stride（可以是结构体大小）
    vertexCount);                 // 顶点数

// 填数据
verts[0] = {0, 0, 0, 0};
/* ... */

// 索引缓冲
unsigned *idx = (unsigned*)rtcSetNewGeometryBuffer(
    geom,
    RTC_BUFFER_TYPE_INDEX,
    0,
    RTC_FORMAT_UINT3,             // 每三角形 3 个 uint
    sizeof(unsigned) * 3,
    triangleCount);

idx[0] = 0; idx[1] = 1; idx[2] = 2;
/* ... */
```

**stride 可以大于格式要求**：
`RTC_FORMAT_FLOAT3` 需要 12 字节，但 stride 传 `sizeof(Vertex)`=16，
Embree 会按 16 跳着读，第 4 个 float 被忽略。

> [!danger] 缓冲必须在 commit 前填完
> `rtcSetNewGeometryBuffer` 返回的指针在 `rtcCommitGeometry`
> 之前有效。commit 后不要再用它写数据
> （要改就用 `rtcGetGeometryBufferData` 重新拿）。

## 步骤 5-7：提交与构建

```cpp
rtcCommitGeometry(geom);                 // 提交几何
unsigned int geomID = rtcAttachGeometry(scene, geom);   // 挂到场景，返回 ID
rtcReleaseGeometry(geom);                // 释放本地引用（场景内部还有一份）
rtcCommitScene(scene);                   // ★ 构建 BVH
```

`geomID` 很重要——**命中时用它知道撞到了哪个物体**。

本项目用它取回对应的网格：

```cpp
auto HitMesh = DynamicLoadScene->GetGeomeoryData(rayHit.hit.geomID);
drawMesh(HitMesh, Draw);
```

## 步骤 8：查询

```cpp
RTCRayHit rayhit;
rayhit.ray.org_x = origin.x;
rayhit.ray.org_y = origin.y;
rayhit.ray.org_z = origin.z;
rayhit.ray.dir_x = dir.x;
rayhit.ray.dir_y = dir.y;
rayhit.ray.dir_z = dir.z;
rayhit.ray.tnear = 0.0f;              // 起始距离
rayhit.ray.tfar  = maxDist;           // 最大距离 ★
rayhit.ray.mask  = 0xFFFFFFFF;        // 掩码（-1 = 全部）
rayhit.ray.flags = 0;
rayhit.hit.geomID = RTC_INVALID_GEOMETRY_ID;    // ★ 必须初始化

RTCIntersectContext ctx;
rtcInitIntersectContext(&ctx);
rtcIntersect1(scene, &ctx, &rayhit);

if (rayhit.hit.geomID != RTC_INVALID_GEOMETRY_ID) {
    printf("命中! t=%f, geomID=%u\n", rayhit.ray.tfar, rayhit.hit.geomID);
}
```

### 四个必填项

| 字段 | 说明 |
|---|---|
| `dir` | 方向（**不必归一化**，t 就是实际距离） |
| `tnear` / `tfar` | 有效距离区间 |
| `mask` | 过滤掩码 |
| `hit.geomID` | 必须初始化为 `RTC_INVALID_GEOMETRY_ID` |

> [!danger] 忘了初始化 hit.geomID
> 即使没命中，`geomID` 也是垃圾值。
> 判断"是否命中"就是看它是否等于 `RTC_INVALID_GEOMETRY_ID`，
> 所以**每次查询前必须重置**。
> 这是本项目代码里反复出现的模式。

### 命中信息

```cpp
rayhit.ray.tfar         // 命中距离
rayhit.hit.geomID       // 命中的几何体 ID
rayhit.hit.primID       // 命中的三角形索引
rayhit.hit.u, .v        // 重心坐标
rayhit.hit.Ng_x/y/z     // 几何法线（未归一化）
```

## 只判断遮挡：rtcOccluded1

如果只关心"有没有被挡住"，不关心撞到哪：

```cpp
RTCRay ray;
/* 设置 ray */
rtcOccluded1(scene, &ctx, &ray);
// 命中则 ray.tfar 被设为负数
bool occluded = (ray.tfar < 0);
```

**比 `rtcIntersect1` 快**——找到第一个命中就停，不算完整的命中信息。

> [!tip] 本项目应该用 Occluded
> 遮挡判定只需要"是/否"，用 `rtcOccluded1` 能快 2~3 倍。
> 本项目用的是 `Intersect`（因为有时还要画命中的网格），
> 但纯遮挡场景换成 Occluded 是明显的优化点。

## 步骤 9：清理

```cpp
rtcReleaseScene(scene);
rtcReleaseDevice(device);
```

顺序：先场景，后设备。

## 修改与更新

改了几何数据后：

```cpp
// 重新拿缓冲
Vertex *v = (Vertex*)rtcGetGeometryBufferData(geom, RTC_BUFFER_TYPE_VERTEX, 0);
/* 改数据 */
rtcCommitGeometry(geom);        // 重新提交
rtcCommitScene(scene);          // 重建 BVH
```

只改了物体位置（不改顶点）→ 用 `rtcSetGeometryTransform`，更快。

## 完整示例

```cpp
#include <embree4/rtcore.h>
#include <cstdio>
#include <cmath>

int main(void) {
    RTCDevice device = rtcNewDevice("threads=1,verbose=0");
    RTCScene  scene  = rtcNewScene(device);

    // 一个"墙"：z=10 处的三角形
    RTCGeometry geom = rtcNewGeometry(device, RTC_GEOMETRY_TYPE_TRIANGLE);

    struct V { float x, y, z, r; };
    V *verts = (V*)rtcSetNewGeometryBuffer(geom, RTC_BUFFER_TYPE_VERTEX, 0,
                                           RTC_FORMAT_FLOAT3, sizeof(V), 3);
    verts[0] = {-5,-5,10,0};
    verts[1] = { 5,-5,10,0};
    verts[2] = { 0, 5,10,0};

    unsigned *idx = (unsigned*)rtcSetNewGeometryBuffer(geom, RTC_BUFFER_TYPE_INDEX, 0,
                                                       RTC_FORMAT_UINT3, 12, 1);
    idx[0]=0; idx[1]=1; idx[2]=2;

    rtcCommitGeometry(geom);
    rtcAttachGeometry(scene, geom);
    rtcReleaseGeometry(geom);
    rtcCommitScene(scene);
    printf("场景构建完成\n");

    // 查询：从原点朝 +Z 射 100 米
    auto shoot = [&](float dx, float dy, float dz) {
        RTCRayHit rh;
        rh.ray.org_x=0; rh.ray.org_y=0; rh.ray.org_z=0;
        rh.ray.dir_x=dx; rh.ray.dir_y=dy; rh.ray.dir_z=dz;
        rh.ray.tnear=0; rh.ray.tfar=100;
        rh.ray.mask=-1; rh.ray.flags=0;
        rh.hit.geomID = RTC_INVALID_GEOMETRY_ID;

        RTCIntersectContext ctx;
        rtcInitIntersectContext(&ctx);
        rtcIntersect1(scene, &ctx, &rh);

        if (rh.hit.geomID != RTC_INVALID_GEOMETRY_ID)
            printf("  命中: t=%.2f geomID=%u primID=%u\n",
                   rh.ray.tfar, rh.hit.geomID, rh.hit.primID);
        else
            printf("  未命中\n");
    };

    printf("朝 +Z (应命中 t=10):\n");   shoot(0, 0, 1);
    printf("朝 +X (应未命中):\n");      shoot(1, 0, 0);
    printf("朝 -Z (应未命中):\n");      shoot(0, 0, -1);
    printf("朝 (0.1,0,1) 归一化:\n");   { float l=sqrtf(1.01f); shoot(0.1f/l, 0, 1/l); }

    rtcReleaseScene(scene);
    rtcReleaseDevice(device);
    return 0;
}
```

预期输出：命中 t=10 / 未命中 / 未命中 / 命中 t≈10.05。

## 常见错误

| 错误 | 症状 | 解法 |
|---|---|---|
| 没初始化 `hit.geomID` | 永远"命中" | 每次查询前重置 |
| `tfar` 设太大 | 撞到目标背后的东西也算 | 设为到目标的距离 |
| commit 后写缓冲 | 崩溃或数据错 | 用 `rtcGetGeometryBufferData` |
| 忘记 `rtcCommitScene` | 查询结果为空 | 改数据后必须 commit |
| stride 写错 | 顶点位置错乱 | 用 `sizeof(顶点结构体)` |

## 动手：性能测量

```cpp
// 建 10000 个三角形的场景
// 测 1000 次 rtcIntersect1 的耗时
// 对比第 96 章自己写的 BVH
```

预期：Embree 比手写 BVH 快 2~5 倍（SIMD + SAH 的功劳）。

再测 `rtcOccluded1` —— 应该比 `rtcIntersect1` 快 2~3 倍。

## 验收清单

- [ ] 知道四个核心对象的关系
- [ ] 会走完九步流程 ★
- [ ] 知道 `geomID` 的用途（取回对应网格）
- [ ] **知道每次查询前必须重置 `hit.geomID`** ★
- [ ] 知道 `rtcOccluded1` 比 `rtcIntersect1` 快
- [ ] 知道 `threads` 参数在手机上要限制
- [ ] 跑通了完整示例，四个查询结果正确
- [ ] 实测了性能，与手写 BVH 对比过

→ 下一章：[[第98章-从碰撞体重建网格]]　—— 把第 93 章的碰撞体变成第 94 章的三角形网格。
