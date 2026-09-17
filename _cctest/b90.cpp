int main(void) {
    std::vector<TriangleMeshData> scene;

    // 场景：一个盒子当"墙"
    TriangleMeshData wall;
    MakeBox(20, 200, 100, wall.Vertices, wall.Indices);
    // 平移到 x=100
    for (auto &v : wall.Vertices) v.x += 100;
    scene.push_back(wall);

    // 从原点射向 (200, 0, 0) —— 应该被墙挡住
    Vec3 from = {0, 0, 0};
    Vec3 to   = {200, 0, 0};

    printf("被遮挡? %s\n", IsOccluded(from, to, scene) ? "是" : "否");

    // 射向 (50, 0, 0) —— 墙在 100，还没到
    to = {50, 0, 0};
    printf("被遮挡? %s\n", IsOccluded(from, to, scene) ? "是" : "否");

    // 从侧面绕过去 (100, 300, 0)
    to = {100, 300, 0};
    printf("被遮挡? %s\n", IsOccluded(from, to, scene) ? "是" : "否");

    return 0;
}