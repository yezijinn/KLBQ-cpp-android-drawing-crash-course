#ifndef NATIVESURFACE_DRAW_H
#define NATIVESURFACE_DRAW_H

#include <stdio.h>
#include <stdlib.h>

#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"
#include "ANativeWindowCreator.h"

#include "AndroidImgui.h"
#include "TouchHelperA.h"//触摸
#include "VectorTools.h"

//3d转2d 类型 (由 draw_Gui.cpp 使用, 定义于此供各文件共享)
struct Vector2A {
    float X;
    float Y;

    Vector2A() : X(0.0f), Y(0.0f) {}
    Vector2A(float x, float y) : X(x), Y(y) {}
};

struct Vector3A {
    float X;
    float Y;
    float Z;

    Vector3A() : X(0.0f), Y(0.0f), Z(0.0f) {}
    Vector3A(float x, float y, float z) : X(x), Y(y), Z(z) {}
};

// 游戏数据: 统一在 draw_Gui.cpp 的 UpdateGameData() 里获取
extern long int libUE4;
extern long int Uworld;
extern long int MySelf;
extern long int PlayerController;
extern long int 玩家相机;
extern long int GName;// GNames/FNamePool 静态指针 (libUE4 + 0xb236b00 读出)
extern long int MatrixPtr;// 矩阵指针链最终地址 (libUE4 + 0xB3C5D00 -> +0x20 -> +0x270 读出)
extern Matrix MatrixData;// MatrixPtr 处读出的 4x4 矩阵
extern Vector3A 相机位置;
extern Vector3A 相机旋转;
extern float 相机FOV;

extern std::unique_ptr<AndroidImgui> graphics;
extern ANativeWindow *window;
extern android::ANativeWindowCreator::DisplayInfo displayInfo;// 屏幕信息
extern ImGuiWindow *g_window;// 窗口信息

extern int abs_ScreenX, abs_ScreenY;// 绝对屏幕X _ Y
extern int native_window_screen_x, native_window_screen_y;


// 上次UI位置
struct Last_ImRect {
    float Pos_x;
    float Pos_y;
    float Size_x;
    float Size_y;
};
extern struct Last_ImRect LastCoordinate;
//是否过录制
extern bool permeate_record_ini;
extern float FPS;

// 准星目标物体 (Hit 模式: 屏幕中心点到目标物体的射线)
extern Vec3 目标物体位置;
extern bool 有目标物体;

// PhysX 是否已初始化 (点击 "physx初始化" 按钮后才拉取 PhysX 数据)
extern bool physx已初始化;


extern void screen_config();// 获取屏幕信息
extern void drawBegin();// 布局UI
extern void Layout_tick_UI(bool *main_thread_flag);
extern void init_My_drawdata();// 初始化绘制数据
extern void DrawPlayer(ImDrawList *draw);




#endif //NATIVESURFACE_DRAW_H