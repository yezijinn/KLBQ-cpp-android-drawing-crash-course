// w2s_demo.c —— 可离线运行的完整 W2S 验证程序
#include <stdio.h>
#include <math.h>
#include <stdbool.h>

#define PI 3.14159265358979323846

typedef struct { float x, y, z; } Vec3;
typedef struct { float Pitch, Yaw, Roll; } Rotator;

static float g_matrix[16];
static float g_px = 0, g_py = 0;      // 屏幕半宽半高
static float g_kx = 1.0f, g_ky = 1.0f; // 焦距微调

void cam_make_matrix(Vec3 loc, Rotator rot, float fovDeg) {
    if (fovDeg < 1.0f || fovDeg > 179.0f) return;
    if (loc.x == 0 && loc.y == 0 && loc.z == 0) return;

    float pitch = rot.Pitch * PI / 180.0f;
    float yaw   = rot.Yaw   * PI / 180.0f;
    float sp = sinf(pitch), cp = cosf(pitch);
    float sy = sinf(yaw),   cy = cosf(yaw);
    float t  = tanf(fovDeg * 0.5f * PI / 180.0f);
    if (t == 0.0f) t = 0.0001f;

    /* 深度行（forward） */
    g_matrix[3]  = cy*cp;
    g_matrix[7]  = sy*cp;
    g_matrix[11] = sp;
    g_matrix[15] = -(loc.x*g_matrix[3] + loc.y*g_matrix[7] + loc.z*g_matrix[11]);

    /* right 行 / t */
    g_matrix[0]  = -sy/t;
    g_matrix[4]  =  cy/t;
    g_matrix[8]  =  0.0f;
    g_matrix[12] = -(loc.x*(-sy) + loc.y*cy)/t;

    /* up 行 / t */
    g_matrix[1]  = (-sp*cy)/t;
    g_matrix[5]  = (-sp*sy)/t;
    g_matrix[9]  =  cp/t;
    g_matrix[13] = -(loc.x*(-sp*cy) + loc.y*(-sp*sy) + loc.z*cp)/t;

    g_matrix[2] = g_matrix[6] = g_matrix[10] = g_matrix[14] = 0.0f;
}

bool world_to_screen(Vec3 obj, float *outX, float *outY) {
    float w = g_matrix[3]*obj.x + g_matrix[7]*obj.y + g_matrix[11]*obj.z + g_matrix[15];
    if (w < 0.001f) return false;

    float cx = g_matrix[0]*obj.x + g_matrix[4]*obj.y + g_matrix[8]*obj.z + g_matrix[12];
    float cy = g_matrix[1]*obj.x + g_matrix[5]*obj.y + g_matrix[9]*obj.z + g_matrix[13];

    *outX = (cx / w * g_kx + 1.0f) * g_px;
    *outY = (1.0f - cy / w * g_ky) * g_py;
    return true;
}

int main(void) {
    /* 2160x2400 屏幕 → 半宽 1080，半高 1200 */
    g_px = 1080.0f;
    g_py = 1200.0f;

    Vec3 cam = {0, 0, 50};            /* 相机在 50 米高 */
    Rotator rot = {0, 0, 0};          /* 朝 +X，水平 */
    cam_make_matrix(cam, rot, 90.0f);

    printf("相机(%.0f,%.0f,%.0f) P=%.0f Y=%.0f FOV=90  屏幕半宽%.0f 半高%.0f\n\n",
           cam.x, cam.y, cam.z, rot.Pitch, rot.Yaw, g_px, g_py);

    struct { const char *name; Vec3 p; } cases[] = {
        {"正前方100m",      {100,   0, 50}},
        {"右前方(100,50)",  {100,  50, 50}},
        {"左前方(100,-50)", {100, -50, 50}},
        {"上方(100,0,100)", {100,   0, 100}},
        {"下方(100,0,0)",   {100,   0, 0}},
        {"远(200,50,50)",   {200,  50, 50}},
        {"后方(-100,0,50)", {-100,  0, 50}},
    };

    for (int i = 0; i < 7; i++) {
        float sx, sy;
        if (world_to_screen(cases[i].p, &sx, &sy))
            printf("%-18s → (%7.1f, %7.1f)%s\n", cases[i].name, sx, sy,
                   (sx < 0 || sx > 2160 || sy < 0 || sy > 2400) ? "  [屏幕外]" : "");
        else
            printf("%-18s → 不可见\n", cases[i].name);
    }
    return 0;
}