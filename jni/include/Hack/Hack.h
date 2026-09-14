#pragma once

#include "VectorTools.h"

#include <unistd.h>

struct mBase {
    uintptr_t libUE4{};
    uintptr_t UWorld{};
    uintptr_t PlayerController{};
    uintptr_t AcknowledgedPawn{};
    uintptr_t CameraManager{};
    uintptr_t CameraCache{};
    uintptr_t PovPtr{};
    Vec3 Location;
    Rotator Rotation{};
    float Fov{};
};

extern mBase AppBase;
extern bool 初始化;
extern void pXthread();