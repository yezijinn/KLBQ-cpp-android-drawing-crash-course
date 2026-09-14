#pragma once

#include <cmath>
#include <cstdint>

#include "Hack.h"
#include "SilentAim.h"
#include "GameTools.h"
#include "driver.h"

namespace PovAim {

    struct PovAimConfig {
        bool  enable      = false;
        int   smooth      = 3;
        bool  suppressInput = true;
        bool  holdOnTarget  = false;
        float arriveEps     = 1.5f;
        float stepMax       = 90.0f;
        int   touchSlot     = 0;
        int   cooldown      = 2;
    };
    inline PovAimConfig Cfg;

    inline Rotator 累积角度{};
    inline bool    累积有效 = false;

    inline bool holdActive = false;
    inline int  cdCounter  = 0;
    inline bool onTarget   = false;

    inline Driver *g_kernel = nullptr;
    inline Driver *GetKernel() { return g_kernel; }

    inline Rotator 真实朝向{};
    inline bool    真实朝向有效 = false;

    inline int 上次屏宽 = 0;
    inline int 上次屏高 = 0;
    inline int 上次点X  = 0;
    inline int 上次点Y  = 0;

    inline int 屏幕宽 = 0;
    inline int 屏幕高 = 0;

    inline void 设置屏幕(int w, int h) {
        屏幕宽 = w;
        屏幕高 = h;
    }

    inline void Release() {
        if (!holdActive) return;
        Driver *k = GetKernel();
        if (k != nullptr) k->TouchUp(Cfg.touchSlot);
        holdActive = false;
        cdCounter  = Cfg.cooldown;
    }

    inline bool HoldStill(int sw, int sh) {
        if (holdActive) return true;
        if (cdCounter > 0) { --cdCounter; return false; }
        Driver *k = GetKernel();
        if (k == nullptr) return false;

        const int x = sw / 2;
        const int y = sh / 2;
        上次屏宽 = sw;
        上次屏高 = sh;
        上次点X  = x;
        上次点Y  = y;
        k->TouchDown(Cfg.touchSlot, x, y, sw, sh);
        k->TouchMove(Cfg.touchSlot, x, y, sw, sh);
        holdActive = true;
        return true;
    }

    inline bool WriteAngles(const Rotator &r) {
        if (!dr || dr->GetGlobalPid() <= 0) return false;
        if (AppBase.PlayerController == 0 || AppBase.CameraManager == 0) return false;

        dr->Write<float>(AppBase.PlayerController + PC_CONTROLROTATION + 0x0, r.Pitch);
        dr->Write<float>(AppBase.PlayerController + PC_CONTROLROTATION + 0x4, r.Yaw);
        dr->Write<float>(AppBase.PlayerController + PC_CONTROLROTATION + 0x8, r.Roll);

        dr->Write<float>(AppBase.CameraManager + POV_ROTATION + 0x0, r.Pitch);
        dr->Write<float>(AppBase.CameraManager + POV_ROTATION + 0x4, r.Yaw);
        dr->Write<float>(AppBase.CameraManager + POV_ROTATION + 0x8, r.Roll);

        return true;
    }

    inline bool Run() {
        if (!Cfg.enable) { Release(); onTarget = false; 累积有效 = false; return false; }

        if (!SilentAim::当前目标.有效) {
            Release();
            onTarget = false;
            return false;
        }
        if (AppBase.PlayerController == 0 || AppBase.CameraManager == 0) { Release(); return false; }

        const Rotator aimAng = SilentAim::向量转角度(
            Vec3(SilentAim::当前目标.世界位置.x - AppBase.Location.x,
                 SilentAim::当前目标.世界位置.y - AppBase.Location.y,
                 SilentAim::当前目标.世界位置.z - AppBase.Location.z));

        if (Cfg.suppressInput) {
            const int sw = 屏幕宽;
            const int sh = 屏幕高;
            if (sw > 0 && sh > 0) HoldStill(sw, sh);
        } else {
            Release();
        }

        if (!累积有效) {
            累积角度 = AppBase.Rotation;
            累积有效 = true;
        }

        const float dPitch = SilentAim::归一角差(aimAng.Pitch, 累积角度.Pitch);
        const float dYaw   = SilentAim::归一角差(aimAng.Yaw,   累积角度.Yaw);

        if (std::fabs(dPitch) <= Cfg.arriveEps && std::fabs(dYaw) <= Cfg.arriveEps) {
            累积角度 = aimAng;
            onTarget = true;
        } else {
            onTarget = false;

            if (Cfg.smooth <= 1) {
                累积角度 = aimAng;
            } else {
                const float t = 1.0f / static_cast<float>(Cfg.smooth);
                float sp = dPitch * t;
                float sy = dYaw   * t;
                if (Cfg.stepMax > 0.0f) {
                    if (sp >  Cfg.stepMax) sp =  Cfg.stepMax;
                    if (sp < -Cfg.stepMax) sp = -Cfg.stepMax;
                    if (sy >  Cfg.stepMax) sy =  Cfg.stepMax;
                    if (sy < -Cfg.stepMax) sy = -Cfg.stepMax;
                }
                if (std::fabs(sp) < 0.05f) sp = (dPitch > 0.0f ? 0.05f : -0.05f);
                if (std::fabs(sy) < 0.05f) sy = (dYaw   > 0.0f ? 0.05f : -0.05f);
                累积角度.Pitch += sp;
                累积角度.Yaw   += sy;
            }
        }
        累积角度.Roll = 0.0f;
        if (累积角度.Yaw >  180.0f) 累积角度.Yaw -= 360.0f;
        if (累积角度.Yaw < -180.0f) 累积角度.Yaw += 360.0f;

        if (Cfg.holdOnTarget && onTarget) return true;

        WriteAngles(累积角度);
        return true;
    }

}