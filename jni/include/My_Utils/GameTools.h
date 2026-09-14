#pragma once

#include "VectorTools.h"

namespace GameTools {
    extern Vec2 ScreenSize;
    extern Matrix WorldMatrix;

    Matrix rotatorToMatrix(Rotator rotation);

    Vec3 GetForward( Rotator rotation);

    void WorldToScreen(Vec2 *bscreen, Vec3 *obj);

    void WorldToScreen(Vec2 *bscreen, Vec3 obj);

    Vec2 WorldToScreen(Vec3 obj);

    void WorldToScreen(Vec2* bscreen, float* height, Vec3 obj);

}