#pragma once
#include <cmath>

#include "draw.h"          // Vector2A / Vector3A
#include "VectorTools.h"   // Vec3 / Rotator
#include "ConfigManager.h" // Settings (焦距微调系数)

#ifndef PI
#define PI 3.14159265358979323846
#endif

// ==================== 相机投影统一管线 ====================
// draw_Gui / GameTools / Draw_ESP 共用同一套矩阵投影算法。
// 用法: 相机变化时调用一次 camMakeMatrix, 然后对每个世界坐标调用 WorldToScreen。

// 屏幕半宽/半高 (由 screen_config() 设置)
inline float px = 0.0f;
inline float py = 0.0f;

// 视图×投影矩阵 (camMakeMatrix 构建, WorldToScreen 使用)
inline float matrix[16] = {0.0f};

// 相机信息 (位置/旋转/视场角)
struct MinimalViewInfo {
    Vec3 Location;
    Rotator Rotation;
    float FOV = 0.0f;
};

// 由相机信息构建 视图×投影 矩阵 (相机变化时调用一次)
static inline void camMakeMatrix(const MinimalViewInfo& camViewInfo) {
    if (camViewInfo.FOV < 1.0f || camViewInfo.FOV > 179.0f) return;
    float camX = camViewInfo.Location.x;
    float camY = camViewInfo.Location.y;
    float camZ = camViewInfo.Location.z;
    if (camX == 0.0f && camY == 0.0f && camZ == 0.0f) return;
    if (camX != camX || camY != camY || camZ != camZ) return;

    float pitch = camViewInfo.Rotation.Pitch * PI / 180.0f;
    float yaw   = camViewInfo.Rotation.Yaw   * PI / 180.0f;
    float sp = sinf(pitch), cp = cosf(pitch);
    float sy = sinf(yaw),   cy = cosf(yaw);

    float t = tanf(camViewInfo.FOV * 0.5f * PI / 180.0f);
    if (t == 0.0f) t = 0.0001f;

    // 行布局(列主序存储)与投影公式:
    //   camera = matrix[3]*x + matrix[7]*y + matrix[11]*z + matrix[15]        (forward 深度)
    //   screenX = px + row0(x) / camera * px                                   (水平焦距 px/t)
    //   screenY = py - row1(x) / camera * py                                   (垂直焦距 py/t)
    // 上行不乘 ratio, 垂直焦距由 WorldToScreen 的 *py 决定 (与 POV 矩阵一致)

    matrix[3]  = cy * cp;
    matrix[7]  = sy * cp;
    matrix[11] = sp;
    matrix[15] = -(camX * matrix[3] + camY * matrix[7] + camZ * matrix[11]);

    matrix[0]  = -sy / t;
    matrix[4]  =  cy / t;
    matrix[8]  =  0.0f;
    matrix[12] = -(camX * (-sy) + camY * cy) / t;

    matrix[1]  = (-sp * cy) / t;
    matrix[5]  = (-sp * sy) / t;
    matrix[9]  = (cp)       / t;
    matrix[13] = -(camX * (-sp * cy) + camY * (-sp * sy) + camZ * cp) / t;

    matrix[2]  = 0.0f;
    matrix[6]  = 0.0f;
    matrix[10] = 0.0f;
    matrix[14] = 0.0f;
}

// 世界坐标 → 屏幕坐标 (需先调用 camMakeMatrix)
inline bool WorldToScreen(const Vector3A& obj, Vector2A& screen) {
    if (ConfigManager::Settings.矩阵W2S && MatrixPtr != 0) {
        const float* m = &MatrixData.M[0][0];
        const float w = m[3] * obj.X + m[7] * obj.Y + m[11] * obj.Z + m[15];
        if (w < 0.001f) return false;
        const float cx = m[0] * obj.X + m[4] * obj.Y + m[8]  * obj.Z + m[12];
        const float cy = m[1] * obj.X + m[5] * obj.Y + m[9]  * obj.Z + m[13];
        screen.X = (cx / w + 1.0f) * px;
        screen.Y = (1.0f - cy / w) * py;
        return std::isfinite(screen.X) && std::isfinite(screen.Y);
    }

    float w = matrix[3] * obj.X + matrix[7] * obj.Y + matrix[11] * obj.Z + matrix[15];
    if (w < 0.001f) return false;

    float clip_x = matrix[0] * obj.X + matrix[4] * obj.Y + matrix[8] * obj.Z + matrix[12];
    float clip_y = matrix[1] * obj.X + matrix[5] * obj.Y + matrix[9] * obj.Z + matrix[13];

    float ndc_x = clip_x / w;
    float ndc_y = clip_y / w;

    screen.X = (ndc_x * ConfigManager::Settings.水平焦距系数 + 1.0f) * px;
    screen.Y = (1.0f - ndc_y * ConfigManager::Settings.垂直焦距系数) * py;

    return true;
}