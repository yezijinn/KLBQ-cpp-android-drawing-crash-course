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