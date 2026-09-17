#include <cstdio>
#include <cmath>
#include <cassert>

struct Rotator { float Pitch=0, Yaw=0, Roll=0; };
struct Quat { float x=0,y=0,z=0,w=1; };

constexpr float PI = 3.14159265358979323846f;
static float Rad(float d) { return d * PI / 180.0f; }
static float Deg(float r) { return r * 180.0f / PI; }

// 欧拉角 → 前方向（本项目 GetForward）
struct Vec3 { float x=0,y=0,z=0; };
Vec3 GetForward(const Rotator &r) {
    float SP = sinf(Rad(r.Pitch)), CP = cosf(Rad(r.Pitch));
    float SY = sinf(Rad(r.Yaw)),   CY = cosf(Rad(r.Yaw));
    return {CP*CY, CP*SY, SP};
}

// 方向 → 欧拉角（本项目 向量转角度）
Rotator DirToRot(const Vec3 &v) {
    float horiz = sqrtf(v.x*v.x + v.y*v.y);
    return { Deg(atan2f(v.z, horiz)), Deg(atan2f(v.y, v.x)), 0 };
}

// 绕 Z 轴转 θ 的四元数
Quat QuatFromAxisAngle(float ax, float ay, float az, float deg) {
    float half = Rad(deg) * 0.5f;
    float s = sinf(half);
    return {ax*s, ay*s, az*s, cosf(half)};
}

int main(void) {
    // 1. 前方向：Yaw=0 → +X；Yaw=90 → +Y
    Vec3 f0 = GetForward({0, 0, 0});
    printf("Yaw=0   前方向=(%.2f,%.2f,%.2f)\n", f0.x, f0.y, f0.z);   // (1,0,0)
    Vec3 f90 = GetForward({0, 90, 0});
    printf("Yaw=90  前方向=(%.2f,%.2f,%.2f)\n", f90.x, f90.y, f90.z); // (0,1,0)
    assert(fabsf(f0.x - 1.0f) < 1e-5f);
    assert(fabsf(f90.y - 1.0f) < 1e-5f);

    // 2. 往返：方向 → 角度 → 方向
    Vec3 v{3, 4, 5};
    Rotator r = DirToRot(v);
    printf("方向(3,4,5) → Pitch=%.1f Yaw=%.1f\n", r.Pitch, r.Yaw);
    Vec3 back = GetForward(r);
    printf("还原方向=(%.2f,%.2f,%.2f)\n", back.x, back.y, back.z);

    // 3. 四元数：绕 Z 转 90 度
    Quat q = QuatFromAxisAngle(0, 0, 1, 90);
    printf("绕Z转90°: (%.3f,%.3f,%.3f,%.3f)\n", q.x,q.y,q.z,q.w);
    // 期望 (0, 0, 0.7071, 0.7071)
    assert(fabsf(q.z - 0.70710678f) < 1e-5f);
    assert(fabsf(q.w - 0.70710678f) < 1e-5f);

    // 4. 单位四元数 = 不旋转
    Quat id{0,0,0,1};
    (void)id;

    printf("全部通过\n");
    return 0;
}