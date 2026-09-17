#include <cstdio>
#include <cmath>
#include <cassert>
#include <initializer_list>   // for (float fov : {…}) 需要它

constexpr float PI = 3.14159265358979323846f;
constexpr float D2R = PI / 180.0f;
constexpr float R2D = 180.0f / PI;

struct Rot { float P, Y; };

// 方向 → 角度（本项目算法）
Rot DirToRot(float x, float y, float z) {
    float h = sqrtf(x*x + y*y);
    return { atan2f(z, h) * R2D, atan2f(y, x) * R2D };
}

int main(void) {
    printf("=== 1. atan vs atan2 象限 ===\n");
    float pairs[][2] = {{1,1}, {-1,1}, {-1,-1}, {1,-1}};
    const char* names[] = {"右上", "左上", "左下", "右下"};
    for (int i = 0; i < 4; i++) {
        float dx = pairs[i][0], dy = pairs[i][1];
        float a1 = atanf(dy/dx) * R2D;
        float a2 = atan2f(dy, dx) * R2D;
        printf("%s(%.0f,%.0f): atan=%7.1f  atan2=%7.1f  %s\n",
               names[i], dx, dy, a1, a2,
               fabsf(a1-a2) < 0.01f ? "" : "← 不同！");
    }

    printf("\n=== 2. 垂直方向不除零 ===\n");
    printf("atan2(1, 0) = %.1f 度（atan(1/0) 会崩）\n", atan2f(1, 0) * R2D);

    printf("\n=== 3. 角度环绕 ===\n");
    auto norm = [](float d) {
        while (d > 180) d -= 360;
        while (d <= -180) d += 360;
        return d;
    };
    printf("350 → 10   原始差=%7.1f  归一化=%6.1f\n", 10-350.0f, norm(10-350));
    printf("-170 → 170 原始差=%7.1f  归一化=%6.1f\n", 170+170.0f, norm(170-(-170)));

    printf("\n=== 4. acos 的 nan 风险 ===\n");
    float bad = 1.0000001f;                 // 浮点误差产生
    printf("acos(1.0000001) = %f\n", acosf(bad));
    printf("acos(clamp(...))= %f\n", acosf(fminf(1.0f, bad)));

    printf("\n=== 5. tan(FOV/2) ===\n");
    for (float fov : {60, 90, 120, 170, 179})
        printf("FOV=%3.0f  t=%.3f\n", fov, tanf(fov * 0.5f * D2R));

    printf("\n=== 6. 往返一致性 ===\n");
    float dirs[][3] = {{1,0,0},{0,1,0},{1,1,0},{3,4,5},{-1,0,0},{0,0,1}};
    for (auto& d : dirs) {
        Rot r = DirToRot(d[0], d[1], d[2]);
        // 反推方向
        float cp = cosf(r.P*D2R), sp = sinf(r.P*D2R);
        float cy = cosf(r.Y*D2R), sy = sinf(r.Y*D2R);
        float bx = cp*cy, by = cp*sy, bz = sp;
        float len = sqrtf(d[0]*d[0]+d[1]*d[1]+d[2]*d[2]);
        float dot = (bx*d[0] + by*d[1] + bz*d[2]) / len;
        printf("(%2.0f,%2.0f,%2.0f) → P=%6.1f Y=%6.1f  还原点积=%.6f\n",
               d[0],d[1],d[2], r.P, r.Y, dot);
        assert(fabsf(dot - 1.0f) < 1e-4f);     // 方向必须一致
    }
    printf("\n全部通过\n");
    return 0;
}