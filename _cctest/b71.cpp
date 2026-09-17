#include <cstdio>
#include <cmath>
#include <cassert>

struct Vec3 { float x=0,y=0,z=0; };
struct Mat4 { float M[4][4] = {}; };

Mat4 Identity() {
    Mat4 m; for (int i=0;i<4;i++) m.M[i][i] = 1.0f; return m;
}
Mat4 Translation(float x, float y, float z) {
    Mat4 m = Identity();
    m.M[3][0]=x; m.M[3][1]=y; m.M[3][2]=z; return m;
}
Mat4 Mul(const Mat4 &a, const Mat4 &b) {
    Mat4 r;
    for (int i=0;i<4;i++) for (int j=0;j<4;j++) for (int k=0;k<4;k++)
        r.M[i][j] += a.M[i][k]*b.M[k][j];
    return r;
}
Vec3 Pos(const Mat4 &m) { return {m.M[3][0], m.M[3][1], m.M[3][2]}; }

int main(void) {
    // 角色在世界 (100, 200, 0)
    Mat4 actorToWorld = Translation(100, 200, 0);

    // 骨盆：相对角色 +10 高度
    Mat4 pelvisLocal = Translation(0, 0, 10);
    // 脊柱：相对骨盆 +40
    Mat4 spineLocal  = Translation(0, 0, 40);
    // 头：相对脊柱 +20
    Mat4 headLocal   = Translation(0, 0, 20);

    // 逐级累乘：world = parent × local（父级在前，见正文说明）
    // 本 demo 是纯平移，结果与存储约定无关，怎么数都对
    Mat4 pelvisWorld = Mul(actorToWorld, pelvisLocal);
    Mat4 spineWorld  = Mul(pelvisWorld,  spineLocal);
    Mat4 headWorld   = Mul(spineWorld,   headLocal);

    Vec3 pHead = Pos(headWorld);
    printf("头世界坐标: (%.0f, %.0f, %.0f)\n", pHead.x, pHead.y, pHead.z);
    // 期望 (100, 200, 70)
    assert(pHead.z == 70.0f);

    // 角色移动到 (500, 500, 0)，只需改根节点
    Mat4 moved = Translation(500, 500, 0);
    Mat4 headMoved = Mul(Mul(Mul(moved, pelvisLocal), spineLocal), headLocal);
    Vec3 p2 = Pos(headMoved);
    printf("移动后头坐标: (%.0f, %.0f, %.0f)\n", p2.x, p2.y, p2.z);
    // 期望 (500, 500, 70) —— 局部变换完全没变
    assert(p2.x == 500 && p2.z == 70);

    printf("全部通过\n");
    return 0;
}