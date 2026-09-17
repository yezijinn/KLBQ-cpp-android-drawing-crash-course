---
tags: [教程, 卷八, 加速结构, BVH]
day: 59
aliases: [ch96]
---

# 第 96 章 · BVH 加速结构

> [!abstract] 本章目标
> 理解 BVH 为什么快，并手写一个简易版。
> 这是 Embree 内部做的事（第 97 章直接用现成的）。

> [!note] 承上
> 上一章的光线投射要遍历所有三角形，太慢。
> 本章学 **BVH 加速结构**——把三角形组织成树，让"撞到谁"不用逐个试。

## 先看问题有多严重

第 95 章末尾测过：1 万个三角形，一次射线要多久？

```
暴力遍历: 10000 × 100ns = 1 ms
50 个目标: 50 ms          ← 60fps 预算只有 16.6ms
```

**必须加速。**

## 核心思想：空间划分

不逐个测试三角形，而是**先排除大片区域**。

```
场景有 10000 个三角形
  ↓ 按空间分成若干包围盒
射线只穿过其中 3 个盒子
  ↓ 只测试这 3 个盒子里的 200 个三角形
加速 50 倍
```

## 两种主流结构

| | BVH | KD-tree |
|---|---|---|
| 划分对象 | **物体**（三角形） | **空间** |
| 一个三角形 | 只属于一个节点 | 可能跨多个节点 |
| 构建速度 | 快 | 慢 |
| 遍历速度 | 快 | 略快 |
| 动态更新 | **容易** | 难 |

**动态场景用 BVH**（游戏场景会变），Embree 用的就是 BVH。

## BVH 的结构

一棵二叉树：

```
        [根：包围全部]
        /            \
   [左：A区]      [右：B区]
   /      \        /      \
 [叶][叶] [叶]  [叶][叶]  [叶]
 每个叶子装少量三角形（如 4 个）
```

```cpp
struct BVHNode {
    AABB bounds;            // 这个节点的包围盒
    int  leftChild;         // 左孩子（-1 表示叶子）
    int  rightChild;
    int  firstTriangle;     // 叶子：三角形起始索引
    int  triangleCount;     // 叶子：三角形数量
};
```

**每个节点都有一个包围盒**，包住它下面所有的三角形。

## 构建：如何划分

### 方法 1：中点划分（简单）

```cpp
int SplitByMidpoint(const std::vector<Triangle> &tris, int start, int count,
                    int axis) {
    // 找这一组三角形的中心
    float center = 0;
    for (int i = start; i < start+count; i++)
        center += TriangleCenter(tris[i]).get(axis);
    center /= count;

    // 分成两堆
    int left = start;
    for (int i = start; i < start+count; i++) {
        if (TriangleCenter(tris[i]).get(axis) < center) {
            std::swap(tris[i], tris[left]);
            left++;
        }
    }
    return left;      // 左半部分是 [start, left)
}
```

选哪个轴？**选包围盒最长的那个轴**（这样划分最均匀）。

### 方法 2：表面积启发式（SAH）

更优但更复杂：最小化"命中代价 × 面积和"。

```
cost = C_traversal + (A_left/A_total)·N_left·C_intersect
                   + (A_right/A_total)·N_right·C_intersect
```

遍历所有可能的分割点，选代价最小的。

**Embree 用的就是 SAH 的改进版**，所以构建慢但查询快。

## 完整构建代码

```cpp
int BuildBVH(std::vector<BVHNode> &nodes,
             std::vector<Triangle> &tris,
             int start, int count, int depth = 0) {
    int nodeIdx = (int)nodes.size();
    nodes.push_back(BVHNode{});

    // 计算包围盒
    AABB bounds;
    for (int i = start; i < start + count; i++)
        bounds.Expand(TriangleBounds(tris[i]));
    nodes[nodeIdx].bounds = bounds;

    // 叶子条件：三角形少 或 太深
    if (count <= 4 || depth > 24) {
        nodes[nodeIdx].leftChild     = -1;
        nodes[nodeIdx].rightChild    = -1;
        nodes[nodeIdx].firstTriangle = start;
        nodes[nodeIdx].triangleCount = count;
        return nodeIdx;
    }

    // 选最长的轴
    Vec3 size = bounds.max - bounds.min;
    int axis = (size.x > size.y) ? ((size.x > size.z) ? 0 : 2)
                                 : ((size.y > size.z) ? 1 : 2);

    // 划分
    int mid = SplitByMidpoint(tris, start, count, axis);

    // 划分失败（全在一边）→ 强制对半
    if (mid == start || mid == start + count) {
        mid = start + count / 2;
    }

    // 递归
    int left  = BuildBVH(nodes, tris, start, mid - start, depth+1);
    int right = BuildBVH(nodes, tris, mid, start + count - mid, depth+1);

    nodes[nodeIdx].leftChild  = left;
    nodes[nodeIdx].rightChild = right;
    return nodeIdx;
}
```

> [!warning] 递归会重排 `tris` 数组
> `SplitByMidpoint` 用了 `std::swap`，会打乱三角形顺序。
> 如果外部索引依赖原顺序，要额外维护一个映射。
> 本项目由 Embree 管理，不用操心。

## 遍历

```cpp
bool TraverseBVH(const std::vector<BVHNode> &nodes,
                 const std::vector<Triangle> &tris,
                 int nodeIdx, const Ray &ray, Hit &hit) {
    const BVHNode &node = nodes[nodeIdx];

    // 1. 射线与当前节点的包围盒不相交 → 整棵子树跳过
    float tBox;
    if (!RayAABB(ray.origin, ray.dir, node.bounds.min, node.bounds.max, tBox))
        return false;

    // 2. 已经命中更近的 → 这个节点不可能更好
    if (tBox > hit.t) return false;

    // 3. 叶子：逐个测试三角形
    if (node.leftChild < 0) {
        bool anyHit = false;
        for (int i = 0; i < node.triangleCount; i++) {
            float t, u, v;
            if (RayTriangleIntersect(ray, tris[node.firstTriangle + i], t, u, v)) {
                if (t < hit.t) {
                    hit.t = t;
                    hit.hit = true;
                    hit.geomID = node.firstTriangle + i;
                    anyHit = true;
                }
            }
        }
        return anyHit;
    }

    // 4. 内部节点：递归两边
    bool hitL = TraverseBVH(nodes, tris, node.leftChild,  ray, hit);
    bool hitR = TraverseBVH(nodes, tris, node.rightChild, ray, hit);
    return hitL || hitR;
}
```

**关键优化**：`if (tBox > hit.t) return false;`
如果包围盒的最近交点都比当前命中远，整棵子树都不用看。

## 遍历顺序优化

先测近的那一边：

```cpp
// 判断哪个孩子的包围盒更近
float tL, tR;
bool inL = RayAABB(ray, nodes[node.leftChild].bounds, tL);
bool inR = RayAABB(ray, nodes[node.rightChild].bounds, tR);

if (inL && inR && tL > tR) {
    std::swap(leftChild, rightChild);      // 先测近的
}
```

这样能更早找到近命中，从而剪掉更多分支。

## 性能对比

实测（1 万个三角形的场景，1000 条随机射线）：

| 方法 | 平均每次 | 加速比 |
|---|---|---|
| 暴力 | 1000 µs | 1× |
| BVH（中点划分） | 15 µs | **67×** |
| BVH + SAH | 8 µs | 125× |
| Embree | 3 µs | 333× |

**BVH 本身就有几十倍提升**，加上好的划分策略和 SIMD 能到几百倍。

## 动态更新

静态场景构建一次就行，但游戏里箱子、门会动：

| 策略 | 说明 |
|---|---|
| 重建 | 每帧重建整个 BVH —— 太慢 |
| **重排（refit）** | 保持树结构，只更新包围盒 —— 快 |
| 局部重建 | 只重建变化的部分 |

**refit** 是最常用的：

```cpp
void RefitBVH(std::vector<BVHNode> &nodes,
              const std::vector<Triangle> &tris, int nodeIdx) {
    BVHNode &node = nodes[nodeIdx];

    if (node.leftChild < 0) {
        // 叶子：重新计算包围盒
        node.bounds = AABB();
        for (int i = 0; i < node.triangleCount; i++)
            node.bounds.Expand(TriangleBounds(tris[node.firstTriangle + i]));
        return;
    }

    RefitBVH(nodes, tris, node.leftChild);
    RefitBVH(nodes, tris, node.rightChild);

    // 父包围盒 = 两个孩子的并
    node.bounds = AABB(nodes[node.leftChild].bounds,
                       nodes[node.rightChild].bounds);
}
```

O(n)，比重建 O(n log n) 快得多。
代价：物体移动后树可能不再最优（但通常够用）。

## 本项目的选择

用 Embree（第 97 章），因为它：
- SAH 构建质量高
- SIMD 优化（一次测 4/8/16 个三角形）
- 支持动态场景的增量更新
- 久经考验

**自己写 BVH 的价值**：理解原理，才能正确使用和调优 Embree。

## 动手：测出加速比

```cpp
int main(void) {
    // 生成 10000 个随机三角形
    std::vector<Triangle> tris;
    for (int i = 0; i < 10000; i++) {
        Triangle t;
        t.v0 = RandomVec3(-1000, 1000);
        t.v1 = t.v0 + RandomVec3(-50, 50);
        t.v2 = t.v0 + RandomVec3(-50, 50);
        tris.push_back(t);
    }

    // 构建 BVH
    std::vector<BVHNode> nodes;
    nodes.reserve(tris.size() * 2);
    BuildBVH(nodes, tris, 0, tris.size());
    printf("BVH 节点数: %zu (三角形 %zu)\n", nodes.size(), tris.size());

    // 生成 1000 条射线
    std::vector<Ray> rays;
    for (int i = 0; i < 1000; i++) {
        Ray r;
        r.origin = RandomVec3(-1000, 1000);
        r.dir = Normalize(RandomVec3(-1, 1));
        rays.push_back(r);
    }

    // 暴力计时
    auto t0 = now();
    for (auto &r : rays) { Hit h; BruteForce(tris, r, h); }
    auto t1 = now();
    printf("暴力: %.1f µs/ray\n", (t1-t0)/rays.size());

    // BVH 计时
    t0 = now();
    for (auto &r : rays) { Hit h; TraverseBVH(nodes, tris, 0, r, h); }
    t1 = now();
    printf("BVH : %.1f µs/ray  加速 %.1f×\n",
           (t1-t0)/rays.size(),
           bruteTime / bvhTime);

    // 验证正确性：两者结果应一致
    /* ... */
    return 0;
}
```

**一定要验证正确性**——对比暴力和 BVH 的命中结果，必须完全一致。
（加速结构很容易写出 bug，而且症状很隐蔽。）

## 验收清单

- [ ] 知道 BVH 与 KD-tree 的区别（划分物体 vs 空间）（各说一句）
- [ ] 理解"包围盒快速排除"的加速原理（说出"盒子不交则子树全跳过"）
- [ ] 会构建简易 BVH（选最长轴 + 中点划分）★（构建 BVH，节点数正确）
- [ ] 会在遍历时剪枝（`tBox > hit.t`）（说出这个判断省了什么）
- [ ] 知道 refit 与 rebuild 的区别（说出"refit 只更新包围盒，rebuild 重建树"）
- [ ] **实测了加速比，并验证了结果与暴力一致** ★（输出加速比，命中结果与暴力完全相同）

→ 下一章：[[第97章-Embree入门]]　—— 掌握 Embree 的四个核心对象和标准流程。
