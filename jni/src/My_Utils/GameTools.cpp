#include "GameTools.h"
#include <fcntl.h>
#include <cmath>
#include "Hack.h"
#include "WorldToScreen.h"

namespace GameTools {
    Vec2 ScreenSize;
    Matrix WorldMatrix;

    Matrix rotatorToMatrix(const Rotator rotation) {
        float radPitch = rotation.Pitch * ((float) M_PI / 180.0f);
        float radYaw = rotation.Yaw * ((float) M_PI / 180.0f);
        float radRoll = rotation.Roll * ((float) M_PI / 180.0f);

        float SP = sinf(radPitch);
        float CP = cosf(radPitch);
        float SY = sinf(radYaw);
        float CY = cosf(radYaw);
        float SR = sinf(radRoll);
        float CR = cosf(radRoll);

        Matrix matrix;

        matrix[0][0] = (CP * CY);
        matrix[0][1] = (CP * SY);
        matrix[0][2] = (SP);
        matrix[0][3] = 0;

        matrix[1][0] = (SR * SP * CY - CR * SY);
        matrix[1][1] = (SR * SP * SY + CR * CY);
        matrix[1][2] = (-SR * CP);
        matrix[1][3] = 0;

        matrix[2][0] = (-(CR * SP * CY + SR * SY));
        matrix[2][1] = (CY * SR - CR * SP * SY);
        matrix[2][2] = (CR * CP);
        matrix[2][3] = 0;

        matrix[3][0] = 0;
        matrix[3][1] = 0;
        matrix[3][2] = 0;
        matrix[3][3] = 1;

        return matrix;
    }

    Vec3 GetForward(const Rotator rotation) {
        float SP = sinf(rotation.Pitch * 3.1415926f / 180.0f);
        float CP = cosf(rotation.Pitch * 3.1415926f / 180.0f);
        float SY = sinf(rotation.Yaw * 3.1415926f / 180.0f);
        float CY = cosf(rotation.Yaw * 3.1415926f / 180.0f);
        return {CP * CY, CP * SY, SP};
    }

    // 统一走矩阵投影管线 (WorldToScreen.h), 需先调用 camMakeMatrix
    void WorldToScreen(Vec2 *bscreen, Vec3 *obj)
    {
        Vector2A s;
        if (::WorldToScreen(Vector3A(obj->x, obj->y, obj->z), s)) {
            bscreen->x = s.X;
            bscreen->y = s.Y;
        } else {
            bscreen->x = NAN;
            bscreen->y = NAN;
        }
    }

    void WorldToScreen(Vec2 *bscreen, Vec3 obj)
    {
        WorldToScreen(bscreen, &obj);
    }

    Vec2 WorldToScreen(Vec3 obj)
    {
        Vec2 bscreen;
        WorldToScreen(&bscreen, obj);
        return bscreen;
    }

    void WorldToScreen(Vec2* bscreen, float* height, Vec3 obj)
    {
        WorldToScreen(bscreen, obj);
        Vec3 headPos = obj;
        headPos.z += 165.0f;

        Vec2 headScreen;
        WorldToScreen(&headScreen, headPos);

        if (height) {
            *height = fabsf(headScreen.y - bscreen->y);
        }
    }
}