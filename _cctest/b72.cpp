#include <cstdio>
#include <cmath>
#include <cassert>

constexpr float PI = 3.14159265358979323846f;
float matrix[16] = {};

void camMakeMatrix(float camX, float camY, float camZ,
                   float pitchDeg, float yawDeg, float fovDeg) {
    float pitch = pitchDeg * PI / 180.0f;
    float yaw   = yawDeg   * PI / 180.0f;
    float sp = sinf(pitch), cp = cosf(pitch);
    float sy = sinf(yaw),   cy = cosf(yaw);
    float t  = tanf(fovDeg * 0.5f * PI / 180.0f);
    if (t == 0.0f) t = 0.0001f;

    matrix[3]  = cy*cp;  matrix[7]  = sy*cp;  matrix[11] = sp;
    matrix[15] = -(camX*matrix[3] + camY*matrix[7] + camZ*matrix[11]);

    matrix[0]  = -sy/t;  matrix[4]  = cy/t;   matrix[8]  = 0.0f;
    matrix[12] = -(camX*(-sy) + camY*cy) / t;

    matrix[1]  = (-sp*cy)/t; matrix[5] = (-sp*sy)/t; matrix[9] = cp/t;
    matrix[13] = -(camX*(-sp*cy) + camY*(-sp*sy) + camZ*cp) / t;

    matrix[2] = matrix[6] = matrix[10] = matrix[14] = 0.0f;
}

// 返回相机空间深度（>0 表示在相机前方）
float depth(float x, float y, float z) {
    return matrix[3]*x + matrix[7]*y + matrix[11]*z + matrix[15];
}

int main(void) {
    // 相机在原点，朝 +X（pitch=0, yaw=0），FOV=90
    camMakeMatrix(0, 0, 0, 0, 0, 90);

    printf("正前方 10 米处深度 = %.2f\n", depth(10, 0, 0));    // 期望 10
    printf("正右方 10 米处深度 = %.2f\n", depth(0, 10, 0));    // 期望 0
    printf("正后方 10 米处深度 = %.2f\n", depth(-10, 0, 0));   // 期望 -10

    assert(fabsf(depth(10,0,0) - 10.0f) < 1e-4f);
    assert(fabsf(depth(0,10,0)) < 1e-4f);          // 侧面深度为 0
    assert(fabsf(depth(-10,0,0) + 10.0f) < 1e-4f); // 后方为负

    // 相机移到 (100,0,0)，同一点的相对深度应变小
    camMakeMatrix(100, 0, 0, 0, 0, 90);
    printf("相机移到(100,0,0)后，(110,0,0) 深度 = %.2f\n", depth(110,0,0));
    // 期望 10（相对距离没变）
    assert(fabsf(depth(110,0,0) - 10.0f) < 1e-4f);

    // yaw=90：相机朝 +Y
    camMakeMatrix(0, 0, 0, 0, 90, 90);
    printf("yaw=90 时，(0,10,0) 深度 = %.2f\n", depth(0,10,0));   // 期望 10
    assert(fabsf(depth(0,10,0) - 10.0f) < 1e-4f);

    printf("全部通过\n");
    return 0;
}