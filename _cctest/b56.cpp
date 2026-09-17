#include <cstdio>
#include <memory>
#include <vector>

struct Mesh {
    int id;
    std::vector<float> verts;
    Mesh(int i) : id(i) { printf("Mesh %d 构造\n", id); }
    ~Mesh() { printf("Mesh %d 析构\n", id); }
    void add(float v) { verts.push_back(v); }
};

// 改造前：可能泄漏
void old_way(void) {
    Mesh *m = new Mesh(1);
    m->add(1.0f);
    if (m->verts.empty()) return;     // 泄漏
    delete m;
}

// 改造后
void new_way(void) {
    auto m = std::make_unique<Mesh>(2);
    m->add(1.0f);
    if (m->verts.empty()) return;     // 打印"Mesh 2 析构"
}

// 容器持有所有权，函数借用
void inspect(const Mesh *m) {          // 借用：不 delete
    printf("  mesh %d 有 %zu 个顶点\n", m->id, m->verts.size());
}

int main(void) {
    std::vector<std::unique_ptr<Mesh>> meshes;
    meshes.emplace_back(std::make_unique<Mesh>(10));
    meshes.emplace_back(std::make_unique<Mesh>(20));
    meshes.back()->add(3.14f);

    for (const auto &m : meshes)
        inspect(m.get());              // .get() 借用

    meshes.clear();                    // 全部析构
    return 0;
}