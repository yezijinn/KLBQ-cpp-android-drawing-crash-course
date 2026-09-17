#include "mathlib.h"
#include <cstdio>

int main(void) {
    using namespace ml;

    Camera cam;
    cam.Position  = {0, 0, 50};
    cam.Rotation  = {0, 0, 0};
    cam.FOV       = 90.0f;
    cam.HalfWidth = 1080.0f;
    cam.HalfHeight= 1200.0f;

    if (!cam.BuildMatrix()) { printf("相机构建失败\n"); return 1; }

    Vec2 s;
    Vec3 targets[] = {
        {100, 0, 50},      // 正前方
        {100, 50, 50},     // 右前
        {-100, 0, 50},     // 后方
    };
    for (const auto& t : targets) {
        if (cam.WorldToScreen(t, s))
            printf("(%.0f,%.0f,%.0f) → (%.1f, %.1f)\n", t.x,t.y,t.z, s.x, s.y);
        else
            printf("(%.0f,%.0f,%.0f) → 不可见\n", t.x,t.y,t.z);
    }
    return 0;
}