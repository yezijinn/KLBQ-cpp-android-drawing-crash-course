#pragma once

#include <cmath>
#include <cstdint>
#include <string>

#include "VectorTools.h"
#include "Hack.h"
#include "GameTools.h"
#include "driver.h"

#ifndef PI_F
#define PI_F 3.14159265358979323846f
#endif

#define PC_CONTROLROTATION 0x378
#define POV_LOCATION       0x2300
#define POV_ROTATION       0x230C
#define POV_FOV            0x2318

namespace SilentAim {

    struct AimConfig {
        bool  启用        = false;
        bool  无视遮挡    = false;
        bool  包含人机    = true;
        float 视野角度    = 8.0f;
        float 最大距离    = 50000.f;
        int   骨骼部位    = 0;
        float 高度偏移    = 90.0f;
        int   平滑度      = 1;
        float 每帧限速    = 360.0f;
        bool  粘滞        = true;
        float 扩大角度    = 15.0f;
        float 粘滞权重    = 0.6f;
        bool  开火连发    = false;
        bool  画目标点    = true;
        bool  写相机POV   = false;
    };
    inline AimConfig Cfg;

    struct AimTarget {
        bool     有效     = false;
        uint64_t actor    = 0;
        Vec3     世界位置{};
        float    屏幕距离 = 0.f;
        float    世界距离 = 0.f;
        bool     被遮挡   = false;
    };
    inline AimTarget 当前目标;

    inline Rotator 基准朝向{};
    inline bool    基准朝向有效 = false;

    inline Rotator 累积角度{};
    inline bool    累积有效 = false;

    inline float 长度(const Vec3 &v) {
        return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    }

    inline Rotator 向量转角度(const Vec3 &v) {
        const float 水平 = std::sqrt(v.x * v.x + v.y * v.y);
        Rotator r{};
        r.Pitch = std::atan2(v.z, 水平) * 180.0f / PI_F;
        r.Yaw   = std::atan2(v.y, v.x)  * 180.0f / PI_F;
        r.Roll  = 0.0f;
        return r;
    }

    inline float 归一角差(float a, float b) {
        float d = a - b;
        while (d >  180.0f) d -= 360.0f;
        while (d < -180.0f) d += 360.0f;
        return d;
    }

    inline const Rotator &选目标基准朝向() {
        if (基准朝向有效) return 基准朝向;
        return AppBase.Rotation;
    }

    inline void 重置目标() {
        当前目标.有效     = false;
        当前目标.actor    = 0;
        当前目标.屏幕距离 = 1e30f;
        当前目标.世界距离 = 0.f;
        当前目标.被遮挡   = false;
    }

    inline void 提交候选(uint64_t actor,
                         const Vec3  &世界位置,
                         bool        被遮挡) {
        if (AppBase.PlayerController == 0 || AppBase.CameraManager == 0) return;
        if (!Cfg.无视遮挡 && 被遮挡) return;

        const Vec3 方向 = Vec3(世界位置.x - AppBase.Location.x,
                               世界位置.y - AppBase.Location.y,
                               世界位置.z - AppBase.Location.z);
        const float 世界距离 = 长度(方向);
        if (世界距离 <= 1.0f || 世界距离 > Cfg.最大距离) return;

        const Rotator 目标角 = 向量转角度(方向);
        const float 偏航 = std::fabs(归一角差(选目标基准朝向().Yaw,   目标角.Yaw));
        const float 俯仰 = std::fabs(归一角差(选目标基准朝向().Pitch, 目标角.Pitch));

        Vec2 屏{};
        GameTools::WorldToScreen(&屏, 世界位置);
        if (!std::isfinite(屏.x) || !std::isfinite(屏.y)) return;

        const float dx = 屏.x - GameTools::ScreenSize.x;
        const float dy = 屏.y - GameTools::ScreenSize.y;
        const float 屏距 = std::sqrt(dx * dx + dy * dy);

        const bool 是小框 = (偏航 <= Cfg.视野角度 && 俯仰 <= Cfg.视野角度);
        const bool 是粘滞目标 = (Cfg.粘滞 && 当前目标.有效 && actor == 当前目标.actor);
        const bool 是大框 = (偏航 <= Cfg.视野角度 + Cfg.扩大角度 &&
                            俯仰 <= Cfg.视野角度 + Cfg.扩大角度);

        if (是粘滞目标) {
            当前目标.世界位置 = 世界位置;
            当前目标.屏幕距离 = 屏距 * Cfg.粘滞权重;
            当前目标.世界距离 = 世界距离;
            当前目标.被遮挡   = 被遮挡;
            return;
        }

        const bool 在选目标范围 = 是小框 || (Cfg.粘滞 && 是大框);
        if (!在选目标范围) return;

        if (屏距 < 当前目标.屏幕距离) {
            当前目标.有效     = true;
            当前目标.actor    = actor;
            当前目标.世界位置 = 世界位置;
            当前目标.屏幕距离 = 屏距;
            当前目标.世界距离 = 世界距离;
            当前目标.被遮挡   = 被遮挡;
        }
    }

    inline bool 本帧有自瞄启用 = false;

    inline void 刷新基准朝向() {
        if (!本帧有自瞄启用 || !当前目标.有效) {
            if (!AppBase.PlayerController || !AppBase.CameraManager) return;
            基准朝向     = AppBase.Rotation;
            基准朝向有效 = true;
        }
    }

    inline bool 写入角度(const Rotator &角) {
        if (!dr || dr->GetGlobalPid() <= 0) return false;
        if (AppBase.PlayerController == 0 || AppBase.CameraManager == 0) return false;

        dr->Write<float>(AppBase.PlayerController + PC_CONTROLROTATION + 0x0, 角.Pitch);
        dr->Write<float>(AppBase.PlayerController + PC_CONTROLROTATION + 0x4, 角.Yaw);
        dr->Write<float>(AppBase.PlayerController + PC_CONTROLROTATION + 0x8, 角.Roll);

        if (Cfg.写相机POV) {
            dr->Write<float>(AppBase.CameraManager + POV_ROTATION + 0x0, 角.Pitch);
            dr->Write<float>(AppBase.CameraManager + POV_ROTATION + 0x4, 角.Yaw);
            dr->Write<float>(AppBase.CameraManager + POV_ROTATION + 0x8, 角.Roll);
        }

        return true;
    }

    inline bool 执行() {
        if (!Cfg.启用) { 累积有效 = false; return false; }
        if (!当前目标.有效) { 累积有效 = false; return false; }
        if (AppBase.PlayerController == 0 || AppBase.CameraManager == 0) { 累积有效 = false; return false; }

        const Rotator 目标角 = 向量转角度(
            Vec3(当前目标.世界位置.x - AppBase.Location.x,
                 当前目标.世界位置.y - AppBase.Location.y,
                 当前目标.世界位置.z - AppBase.Location.z));

        if (Cfg.平滑度 <= 1) {
            写入角度(目标角);
            累积角度 = 目标角;
            累积有效 = true;
            return true;
        }

        if (!累积有效) {
            累积角度 = AppBase.Rotation;
            累积有效 = true;
        }

        const float t = 1.0f / static_cast<float>(Cfg.平滑度);
        const float dp = 归一角差(目标角.Pitch, 累积角度.Pitch);
        const float dy = 归一角差(目标角.Yaw,   累积角度.Yaw);

        if (std::fabs(dp) <= 0.05f && std::fabs(dy) <= 0.05f) {
            累积角度 = 目标角;
            写入角度(累积角度);
            return true;
        }

        const float 步上限 = (Cfg.每帧限速 > 0.0f) ? Cfg.每帧限速 : 1e9f;
        float sp = dp * t;
        float sy = dy * t;
        if (sp >  步上限) sp =  步上限;
        if (sp < -步上限) sp = -步上限;
        if (sy >  步上限) sy =  步上限;
        if (sy < -步上限) sy = -步上限;

        累积角度.Pitch += sp;
        累积角度.Yaw   += sy;
        累积角度.Roll   = 0.0f;
        if (累积角度.Yaw >  180.0f) 累积角度.Yaw -= 360.0f;
        if (累积角度.Yaw < -180.0f) 累积角度.Yaw += 360.0f;

        写入角度(累积角度);
        return true;
    }

}