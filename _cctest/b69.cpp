#include <cstdio>
#include <cmath>
#include <cassert>

struct Vec3 { float x=0,y=0,z=0; };
struct Mat4 { float M[4][4] = {}; };

Mat4 Identity() {
    Mat4 m;
    for (int i = 0; i < 4; i++) m.M[i][i] = 1.0f;
    return m;
}

Mat4 Translation(float x, float y, float z) {
    Mat4 m = Identity();
    m.M[3][0] = x; m.M[3][1] = y; m.M[3][2] = z;
    return m;
}

Mat4 Scale(float s) {
    Mat4 m = Identity();
    m.M[0][0] = m.M[1][1] = m.M[2][2] = s;
    return m;
}

Mat4 Mul(const Mat4 &a, const Mat4 &b) {
    Mat4 r;
    for (int i=0;i<4;i++) for (int j=0;j<4;j++) for (int k=0;k<4;k++)
        r.M[i][j] += a.M[i][k] * b.M[k][j];
    return r;
}

Vec3 ApplyPoint(const Mat4 &m, const Vec3 &v) {
    return {
        m.M[0][0]*v.x + m.M[0][1]*v.y + m.M[0][2]*v.z + m.M[3][0],
        m.M[1][0]*v.x + m.M[1][1]*v.y + m.M[1][2]*v.z + m.M[3][1],
        m.M[2][0]*v.x + m.M[2][1]*v.y + m.M[2][2]*v.z + m.M[3][2]
    };
}

int main(void) {
    // 1. 单位矩阵不改变向量
    Vec3 p{1,2,3};
    Vec3 r0 = ApplyPoint(Identity(), p);
    assert(r0.x==1 && r0.y==2 && r0.z==3);

    // 2. 平移
    Vec3 r1 = ApplyPoint(Translation(10,20,30), p);
    printf("平移后: (%.0f,%.0f,%.0f)\n", r1.x, r1.y, r1.z);   // (11,22,33)

    // 3. 缩放
    Vec3 r2 = ApplyPoint(Scale(2), p);
    printf("缩放后: (%.0f,%.0f,%.0f)\n", r2.x, r2.y, r2.z);   // (2,4,6)

    // 4. 顺序很重要：两种顺序结果不同
    // 本 demo 用"平移写在第 4 行"的行向量约定。为便于对照，
    // 结论直接以实测输出为准（下面注释就是程序真实输出）：
    Vec3 a = ApplyPoint(Mul(Translation(10,0,0), Scale(2)), p);   // 等效"先平移后缩放" → (22,4,6)
    Vec3 b = ApplyPoint(Mul(Scale(2), Translation(10,0,0)), p);   // 等效"先缩放后平移" → (12,4,6)
    printf("T*S: (%.0f,%.0f,%.0f)\n", a.x, a.y, a.z);   // (22,4,6)
    printf("S*T: (%.0f,%.0f,%.0f)\n", b.x, b.y, b.z);   // (12,4,6)
    assert(a.x != b.x);    // ★ 证明不可交换

    printf("全部通过\n");
    return 0;
}