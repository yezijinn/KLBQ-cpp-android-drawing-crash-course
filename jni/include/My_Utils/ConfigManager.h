#pragma once

#include <cstdint>

struct Config {
    int RunFPS = 90;
    bool PhysX = false;
    int PhysXType = 0;

    bool syscall驱动 = false;

    bool 过录制 = false;
    bool 方框 = false;
    bool 射线 = false;
    bool 骨骼 = false;
    bool 骨骼索引 = false;
    bool 胶囊体大小 = false;
    bool 类名 = false;
    bool 人数 = true;
    bool 子弹 = false;
    bool 人机 = false;
    int gui_frame_rate = 60;

    float 骨骼偏移X = 0.0f;
    float 骨骼偏移Y = 0.0f;
    float 骨骼偏移Z = 0.0f;
    float 垂直焦距系数 = 1.0f;
    float 水平焦距系数 = 1.0f;
    bool  矩阵W2S      = false;

    bool  自瞄启用     = false;
    bool  自瞄无视遮挡 = false;
    bool  自瞄包含人机 = true;
    bool  自瞄画目标点 = true;
    float 自瞄视野角度 = 8.0f;
    float 自瞄最大距离 = 50000.f;
    int   自瞄骨骼部位 = 0;
    float 自瞄高度偏移 = 90.0f;
    int   自瞄平滑度   = 1;
    float 自瞄每帧限速 = 360.0f;
    bool  自瞄粘滞     = true;
    float 自瞄扩大角度 = 15.0f;
    float 自瞄粘滞权重 = 0.6f;
    bool  自瞄写相机POV = false;

    bool  触摸自瞄启用 = false;
    int   触摸自瞄模式 = 1;
    int   触摸自瞄槽   = 0;
    int   触摸自瞄平滑 = 4;
    int   触摸自瞄停留 = 2;
    int   触摸自瞄冷却 = 0;
    int   触摸起手X    = -1;
    int   触摸起手Y    = -1;

    bool  视角自瞄启用     = false;
    int   视角自瞄平滑     = 3;
    bool  视角自瞄抑制输入 = true;
    bool  视角自瞄锁定停手 = false;
};

namespace ConfigManager {
    extern Config Settings;

    void CryptoProcess(uint8_t* data, size_t length);
    bool LoadConfig();
    bool SaveConfig();
}