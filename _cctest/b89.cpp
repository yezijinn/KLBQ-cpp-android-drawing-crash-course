int main(void) {
    std::vector<Vertex> verts;
    std::vector<uint32_t> idx;

    MakeBox(50, 50, 50, verts, idx);
    printf("盒子: %zu 顶点, %zu 索引 (%zu 三角形)\n",
           verts.size(), idx.size(), idx.size()/3);

    MakeSphere(30, 16, verts, idx);
    printf("球: %zu 顶点, %zu 索引 (%zu 三角形)\n",
           verts.size(), idx.size(), idx.size()/3);

    // 验证：所有索引必须在顶点范围内
    for (uint32_t i : idx) {
        if (i >= verts.size()) {
            printf("错误：索引 %u 超出顶点数 %zu\n", i, verts.size());
            return 1;
        }
    }
    printf("索引检查通过\n");
    return 0;
}