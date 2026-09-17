#include <cstdio>
#include <cmath>
#include <cassert>

constexpr float PI = 3.14159265358979323846f;

// 简化的 W2S：相机在原点朝 +X，只做透视除法
struct Result { float x, y; bool ok; };

Result project(float camX, float camY, float camZ,
               float px, float py, float fovDeg,
               float kx=1.0f, float ky=1.0f) {
    // 相机空间：x 前、y 右、z 上（相机已在原点）
    float w = camX;                  // 深度
    if (w < 0.001f) return {0,0,false};

    float t = tanf(fovDeg * 0.5f * PI / 180.0f);
    float ndc_y_right = (camY / t) / w;      // 右方向
    float ndc_y_up    = (camZ / t) / w;      // 上方向

    return {
        (ndc_y_right * kx + 1.0f) * px,
        (1.0f - ndc_y_up * ky) * py,
        true
    };
}

int main(void) {
    const float px = 1080.0f;   // 半宽
    const float py = 1200.0f;   // 半高

    // 1. 正前方 → 屏幕中心
    auto c = project(100, 0, 0, px, py, 90);
    printf("正前方: (%.1f, %.1f)  期望 (%.1f, %.1f)\n", c.x, c.y, px, py);
    assert(fabsf(c.x - px) < 0.5f && fabsf(c.y - py) < 0.5f);

    // 2. 在相机后方 → 失败
    auto b = project(-100, 0, 0, px, py, 90);
    printf("后方 ok=%d\n", b.ok);
    assert(!b.ok);

    // 3. 右方：应偏右（x > px），且在竖直中心
    auto r = project(100, 100, 0, px, py, 90);
    printf("右前方: (%.1f, %.1f)\n", r.x, r.y);
    assert(r.x > px && fabsf(r.y - py) < 0.5f);

    // 4. 上方：应偏上（y < py），且水平居中
    auto u = project(100, 0, 100, px, py, 90);
    printf("上前方: (%.1f, %.1f)\n", u.x, u.y);
    assert(u.y < py && fabsf(u.x - px) < 0.5f);

    // 5. 距离加倍 → 偏移量减半（透视）
    auto near_ = project(100, 100, 0, px, py, 90);
    auto far_  = project(200, 100, 0, px, py, 90);
    float offNear = near_.x - px, offFar = far_.x - px;
    printf("近偏移=%.1f  远偏移=%.1f  比值=%.2f\n",
           offNear, offFar, offNear / offFar);
    assert(fabsf(offNear / offFar - 2.0f) < 0.01f);   // 应恰好 2 倍

    // 6. FOV 加倍 → 偏移量变小（视野变广）
    auto fov90 = project(100, 100, 0, px, py, 90);
    auto fov120 = project(100, 100, 0, px, py, 120);
    printf("FOV90偏移=%.1f  FOV120偏移=%.1f\n",
           fov90.x - px, fov120.x - px);
    assert((fov120.x - px) < (fov90.x - px));

    printf("全部通过\n");
    return 0;
}