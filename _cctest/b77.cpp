// mathlib_test.cpp
#include "mathlib.h"
#include "test_framework.h"
#include <cstdio>

using namespace ml;

void TestVec3() {
    TEST("Vec3 基础运算");
    // 注意：CHECK_NEAR 只能比较两个"数"，不能直接比较两个向量。
    // 别写成 CHECK_NEAR(Vec3{...} + Vec3{...}, Vec3{...}.Length())——那样编译不过。
    // 正确做法是逐分量检查：
    Vec3 sum = Vec3{1,2,3} + Vec3{10,20,30};
    CHECK_NEAR(sum.x, 11); CHECK_NEAR(sum.y, 22); CHECK_NEAR(sum.z, 33);

    Vec3 diff = Vec3{10,20,30} - Vec3{1,2,3};
    CHECK_NEAR(diff.x, 9); CHECK_NEAR(diff.y, 18); CHECK_NEAR(diff.z, 27);

    TEST("Vec3 长度与归一化");
    CHECK_NEAR(Vec3(3,4,0).Length(), 5.0f);
    CHECK_NEAR(Vec3(1,0,0).Length(), 1.0f);
    CHECK_NEAR(Vec3(3,4,0).Normalized().Length(), 1.0f);
    CHECK_NEAR(Vec3(0,0,0).Normalized().LengthSq(), 0.0f);   // 零向量不产生 NaN

    TEST("Vec3 点积与叉积");
    CHECK_NEAR(Vec3(1,0,0).Dot(Vec3(0,1,0)), 0.0f);          // 垂直
    CHECK_NEAR(Vec3(1,0,0).Dot(Vec3(1,0,0)), 1.0f);
    CHECK_NEAR(Vec3(2,0,0).Dot(Vec3(3,0,0)), 6.0f);

    Vec3 cr = Vec3{1,0,0}.Cross(Vec3{0,1,0});
    CHECK_NEAR(cr.x, 0); CHECK_NEAR(cr.y, 0); CHECK_NEAR(cr.z, 1);  // 叉积代数定义：x̂ × ŷ = ẑ
    CHECK_NEAR(cr.Dot(Vec3{1,0,0}), 0.0f);     // 与两边都垂直
    CHECK_NEAR(cr.Dot(Vec3{0,1,0}), 0.0f);

    TEST("Vec3 距离");
    CHECK_NEAR(Vec3::Distance({0,0,0}, {3,4,0}), 5.0f);
    CHECK_NEAR(Vec3::DistanceSq({0,0,0}, {3,4,0}), 25.0f);
}

void TestAngle() {
    TEST("角度归一化");
    CHECK_NEAR(NormalizeAngle(0), 0);
    CHECK_NEAR(NormalizeAngle(180), 180);
    CHECK_NEAR(NormalizeAngle(190), -170);
    CHECK_NEAR(NormalizeAngle(-190), 170);
    CHECK_NEAR(NormalizeAngle(720), 0);

    TEST("角度差走最短路");
    CHECK_NEAR(AngleDelta(350, 10), 20);      // 不是 -340
    CHECK_NEAR(AngleDelta(10, 350), -20);
    CHECK_NEAR(AngleDelta(0, 90), 90);
    CHECK_NEAR(AngleDelta(-170, 170), -20);   // 跨 ±180 边界
}

void TestRotator() {
    TEST("前方向");
    Vec3 f0 = RotatorToForward({0, 0, 0});
    CHECK_NEAR(f0.x, 1); CHECK_NEAR(f0.y, 0); CHECK_NEAR(f0.z, 0);

    Vec3 f90 = RotatorToForward({0, 90, 0});
    CHECK_NEAR(f90.x, 0); CHECK_NEAR(f90.y, 1); CHECK_NEAR(f90.z, 0);

    Vec3 fUp = RotatorToForward({90, 0, 0});
    CHECK_NEAR(fUp.x, 0); CHECK_NEAR(fUp.y, 0); CHECK_NEAR(fUp.z, 1);

    TEST("方向 ↔ 角度 往返");
    Vec3 dirs[] = {{1,0,0}, {0,1,0}, {1,1,0}, {3,4,5}, {-1,0,0}};
    for (const auto& d : dirs) {
        Rotator r = DirectionToRotator(d);
        Vec3 back = RotatorToForward(r);
        Vec3 nd = d.Normalized();
        // 方向应一致（点积接近 1）
        CHECK_NEAR_EPS(nd.Dot(back), 1.0f, 1e-4f);
    }

    TEST("俯仰角符号");
    Rotator up = DirectionToRotator({1, 0, 1});    // 前上方
    CHECK(up.Pitch > 0);
    Rotator down = DirectionToRotator({1, 0, -1}); // 前下方
    CHECK(down.Pitch < 0);
}

void TestQuat() {
    TEST("单位四元数");
    Quat q;   // 默认 (0,0,0,1)
    Mat4 m = QuatToMatrix(q, Vec3{}, Vec3{1,1,1});
    CHECK_NEAR(m.M[0][0], 1); CHECK_NEAR(m.M[1][1], 1); CHECK_NEAR(m.M[2][2], 1);

    TEST("绕 Z 转 90 度");
    Quat q90 = QuatFromAxisAngle({0,0,1}, 90);
    CHECK_NEAR(q90.z, 0.70710678f);
    CHECK_NEAR(q90.w, 0.70710678f);
    CHECK_NEAR(q90.x, 0); CHECK_NEAR(q90.y, 0);

    TEST("四元数矩阵是正交的（旋转矩阵三个行向量长度都为 1）");
    Quat q37 = QuatFromAxisAngle({1,2,3}, 37);
    Mat4 mm = QuatToMatrix(q37, Vec3{}, Vec3{1,1,1});
    for (int i = 0; i < 3; i++) {
        Vec3 row{mm.M[i][0], mm.M[i][1], mm.M[i][2]};
        CHECK_NEAR(row.Length(), 1.0f);
    }
}

void TestMatrix() {
    TEST("单位矩阵");
    Mat4 I = Mat4::Identity();
    CHECK_NEAR(I.M[0][0], 1); CHECK_NEAR(I.M[1][2], 0); CHECK_NEAR(I.M[3][3], 1);

    TEST("平移与取位置");
    Mat4 t = Mat4Translation({10, 20, 30});
    Vec3 p = Mat4GetPosition(t);
    CHECK_NEAR(p.x, 10); CHECK_NEAR(p.y, 20); CHECK_NEAR(p.z, 30);

    TEST("乘法不可交换");
    Mat4 A = Mat4Translation({10,0,0});
    Mat4 B = Mat4Scale({2,2,2});
    // 只看结果的平移分量（相当于点取原点）：
    //   Multiply(A,B) = A*B = T*S：A 的平移被 B 的缩放放大 → 10*2 = 20
    //   Multiply(B,A) = B*A = S*T：B 是纯缩放，平移分量不变 → 10
    // 两者不同，即证明不可交换。
    Vec3 p1 = Mat4GetPosition(Mat4Multiply(A, B));
    Vec3 p2 = Mat4GetPosition(Mat4Multiply(B, A));
    CHECK_NEAR(p1.x, 20);    // T*S：平移被缩放放大
    CHECK_NEAR(p2.x, 10);    // S*T：平移不变

    TEST("转置两次回到原矩阵");
    Mat4 src = Mat4Translation({1,2,3});
    Mat4 tt = Mat4Transpose(Mat4Transpose(src));
    CHECK_NEAR(tt.M[3][0], 1); CHECK_NEAR(tt.M[3][1], 2); CHECK_NEAR(tt.M[3][2], 3);
}

void TestCamera() {
    Camera cam;
    cam.Position   = {0, 0, 50};
    cam.Rotation   = {0, 0, 0};
    cam.FOV        = 90.0f;
    cam.HalfWidth  = 1080.0f;
    cam.HalfHeight = 1200.0f;
    CHECK(cam.BuildMatrix());

    Vec2 s;

    TEST("W2S 正前方在屏幕中心");
    CHECK(cam.WorldToScreen({100, 0, 50}, s));
    CHECK_NEAR(s.x, 1080); CHECK_NEAR(s.y, 1200);

    TEST("W2S 右方偏右、左方偏左（对称）");
    CHECK(cam.WorldToScreen({100, 50, 50}, s));
    CHECK_NEAR(s.x, 1620); CHECK_NEAR(s.y, 1200);
    CHECK(cam.WorldToScreen({100, -50, 50}, s));
    CHECK_NEAR(s.x, 540);  CHECK_NEAR(s.y, 1200);

    TEST("W2S 上方偏上、下方偏下（对称）");
    CHECK(cam.WorldToScreen({100, 0, 100}, s));
    CHECK_NEAR(s.x, 1080); CHECK_NEAR(s.y, 600);
    CHECK(cam.WorldToScreen({100, 0, 0}, s));
    CHECK_NEAR(s.x, 1080); CHECK_NEAR(s.y, 1800);

    TEST("W2S 透视：距离翻倍偏移减半");
    Vec2 nearP, farP;
    cam.WorldToScreen({100, 50, 50}, nearP);
    cam.WorldToScreen({200, 50, 50}, farP);
    CHECK_NEAR((nearP.x - 1080) / (farP.x - 1080), 2.0f);

    TEST("W2S 后方不可见");
    CHECK(!cam.WorldToScreen({-100, 0, 50}, s));

    TEST("W2S 焦距系数生效");
    cam.Kx = 2.0f;
    cam.WorldToScreen({100, 50, 50}, s);
    CHECK_NEAR(s.x, 1080 + 540*2);      // 偏移加倍
    cam.Kx = 1.0f;

    TEST("无效相机被拒绝");
    Camera bad;
    bad.Position = {0,0,0};             // 位置为 0
    CHECK(!bad.BuildMatrix());
    Camera bad2;
    bad2.Position = {1,1,1};
    bad2.FOV = 200;                     // FOV 超范围
    CHECK(!bad2.BuildMatrix());
}

int main(void) {
    TestVec3();
    TestAngle();
    TestRotator();
    TestQuat();
    TestMatrix();
    TestCamera();
    return ReportTests();
}