#pragma once

#include <cmath>
#include <cstdint>

#include "Hack.h"
#include "SilentAim.h"
#include "GameTools.h"
#include "driver.h"

namespace TouchAim {

    struct TouchAimConfig {
        bool 启用   = false;
        int  模式   = 1;
        int  触摸槽 = 0;
        int  平滑   = 4;
        int  起手X  = -1;
        int  起手Y  = -1;
        int  停留帧 = 2;
        int  冷却帧 = 0;
    };
    inline TouchAimConfig Cfg;

    inline bool  pressed   = false;
    inline int   slot      = 0;
    inline int   startX    = 0;
    inline int   startY    = 0;
    inline int   stayCount = 0;
    inline int   cdCount   = 0;
    inline float draggedX  = 0.f;
    inline float draggedY  = 0.f;

    inline Driver *g_kernel = nullptr;

    inline Driver *GetKernel() { return g_kernel; }

    inline void ScreenSize(int &w, int &h) {
        w = static_cast<int>(GameTools::ScreenSize.x);
        h = static_cast<int>(GameTools::ScreenSize.y);
    }

    inline void Release() {
        if (!pressed) return;
        Driver *k = GetKernel();
        if (k != nullptr) k->TouchUp(slot);
        pressed   = false;
        stayCount = 0;
        cdCount   = Cfg.冷却帧;
    }

    inline bool Run() {
        if (!Cfg.启用) { Release(); return false; }

        Driver *k = GetKernel();
        if (k == nullptr) return false;

        int sw = 0, sh = 0;
        ScreenSize(sw, sh);
        if (sw <= 0 || sh <= 0) { Release(); return false; }

        if (!SilentAim::当前目标.有效) { Release(); return false; }

        Vec2 sp{};
        GameTools::WorldToScreen(&sp, SilentAim::当前目标.世界位置);
        if (!std::isfinite(sp.x) || !std::isfinite(sp.y)) { Release(); return false; }

        const float cx = GameTools::ScreenSize.x;
        const float cy = GameTools::ScreenSize.y;

        if (!pressed) {
            if (cdCount > 0) { --cdCount; return false; }
            slot     = Cfg.触摸槽;
            startX   = (Cfg.起手X >= 0) ? Cfg.起手X : static_cast<int>(sw * 0.85f);
            startY   = (Cfg.起手Y >= 0) ? Cfg.起手Y : static_cast<int>(sh * 0.85f);
            draggedX = 0.f;
            draggedY = 0.f;
            k->TouchDown(slot, startX, startY, sw, sh);
            pressed   = true;
            stayCount = 0;
        }

        int tx = startX;
        int ty = startY;

        if (Cfg.模式 == 0) {
            tx = static_cast<int>(sp.x);
            ty = static_cast<int>(sp.y);
        } else {
            const float offX = sp.x - cx;
            const float offY = sp.y - cy;
            const float step = static_cast<float>(Cfg.平滑 < 1 ? 1 : Cfg.平滑);
            if (draggedX < offX) draggedX += (offX - draggedX) / step + 0.5f;
            else if (draggedX > offX) draggedX += (offX - draggedX) / step - 0.5f;
            if (draggedY < offY) draggedY += (offY - draggedY) / step + 0.5f;
            else if (draggedY > offY) draggedY += (offY - draggedY) / step - 0.5f;
            tx = startX + static_cast<int>(draggedX);
            ty = startY + static_cast<int>(draggedY);
        }

        if (tx < 0) tx = 0;
        if (ty < 0) ty = 0;
        if (tx > sw) tx = sw;
        if (ty > sh) ty = sh;

        k->TouchMove(slot, tx, ty, sw, sh);

        ++stayCount;
        return true;
    }

}