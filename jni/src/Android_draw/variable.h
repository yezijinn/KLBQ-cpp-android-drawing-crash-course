#pragma once
#include <cmath>
#include "draw.h"        
#include "VectorTools.h"  
#include "driver.h"       

#ifndef PI
#define PI 3.14159265358979323846
#endif

//3d转2d 
#include "WorldToScreen.h"


//变量 (GUI 开关/微调已移入 ConfigManager::Settings, 保存配置可全量持久化)


bool permeate_record_ini = false;
struct Last_ImRect LastCoordinate = {0, 0, 0, 0};
float FPS = 120.0f;

std::unique_ptr<AndroidImgui> graphics;
ANativeWindow *window = NULL;
android::ANativeWindowCreator::DisplayInfo displayInfo;
ImGuiWindow *g_window = NULL;
int abs_ScreenX = 0, abs_ScreenY = 0;
int native_window_screen_x = 0, native_window_screen_y = 0;

ImFont *zh_font = NULL;

long int libUE4 = 0;
long int Uworld = 0;
long int Arrayaddr = 0;
long int MySelf = 0;
long int Uleve = 0;
long int Gname = 0;
long int PlayerController = 0;
long int 玩家相机 = 0;
long int GName = 0;
long int MatrixPtr = 0;
Matrix MatrixData = {};
uint64_t ProjectilesData = 0;
int ProjectilesCount = 0;
int Count = 0;
int 人数值 = 0;

bool 初始化 = false;

ImColor BoxColor = ImColor(255, 0, 0, 255);
ImColor LineColor = ImColor(255, 0, 0, 255);
ImColor BoneColor = ImColor(255, 0, 0, 255);
ImColor BlockBoxColor = ImColor(0, 255, 0, 255);   // 掩体后(被遮挡) 方框绿色
ImColor BlockLineColor = ImColor(0, 255, 0, 255);  // 掩体后(被遮挡) 射线绿色
Vector3A 相机位置;
Vector3A 相机旋转;
float 相机FOV = 0.0f;

Vec3 目标物体位置;
bool 有目标物体 = false;

bool physx已初始化 = false;


enum BoneIndex : int {
    BONE_PELVIS     = 1,   // 骨盆
    BONE_CHEST      = 6,   // 胸口
    BONE_HEAD       = 7,   // 头
    BONE_L_SHOULDER = 8,   // 左肩
    BONE_L_ELBOW    = 10,  // 左肘
    BONE_L_WRIST    = 11,  // 左手
    BONE_R_SHOULDER = 30,  // 右肩
    BONE_R_ELBOW    = 31,  // 右肘
    BONE_R_WRIST    = 32,  // 右手
    BONE_L_THIGH    = 49,  // 左大腿
    BONE_L_KNEE     = 50,  // 左膝
    BONE_L_ANKLE    = 51,  // 左踝
    BONE_R_THIGH    = 53,  // 右大腿
    BONE_R_KNEE     = 54,  // 右膝
    BONE_R_ANKLE    = 55,  // 右踝
};

#define BONE_STRIDE 48

struct BoneTransform {
    Quat Rotation;     // 0x00
    Vec3 Translation;  // 0x10
    Vec3 Scale3D;      // 0x1C
};

inline BoneTransform getBone(uint64_t addr) {
    BoneTransform t;
    if (addr == 0) return t;
    dr->Read(addr, &t, sizeof(BoneTransform));
    return t;
}

inline Matrix TransformToMatrix(const BoneTransform& transform) {
    Matrix matrix = {};

    matrix.M[3][0] = transform.Translation.x;
    matrix.M[3][1] = transform.Translation.y;
    matrix.M[3][2] = transform.Translation.z;

    const float x2 = transform.Rotation.x + transform.Rotation.x;
    const float y2 = transform.Rotation.y + transform.Rotation.y;
    const float z2 = transform.Rotation.z + transform.Rotation.z;

    const float xx2 = transform.Rotation.x * x2;
    const float yy2 = transform.Rotation.y * y2;
    const float zz2 = transform.Rotation.z * z2;

    matrix.M[0][0] = (1 - (yy2 + zz2)) * transform.Scale3D.x;
    matrix.M[1][1] = (1 - (xx2 + zz2)) * transform.Scale3D.y;
    matrix.M[2][2] = (1 - (xx2 + yy2)) * transform.Scale3D.z;

    const float yz2 = transform.Rotation.y * z2;
    const float wx2 = transform.Rotation.w * x2;
    matrix.M[2][1] = (yz2 - wx2) * transform.Scale3D.z;
    matrix.M[1][2] = (yz2 + wx2) * transform.Scale3D.y;

    const float xy2 = transform.Rotation.x * y2;
    const float wz2 = transform.Rotation.w * z2;
    matrix.M[1][0] = (xy2 - wz2) * transform.Scale3D.y;
    matrix.M[0][1] = (xy2 + wz2) * transform.Scale3D.x;

    const float xz2 = transform.Rotation.x * z2;
    const float wy2 = transform.Rotation.w * y2;
    matrix.M[2][0] = (xz2 + wy2) * transform.Scale3D.z;
    matrix.M[0][2] = (xz2 - wy2) * transform.Scale3D.x;

    matrix.M[0][3] = 0;
    matrix.M[1][3] = 0;
    matrix.M[2][3] = 0;
    matrix.M[3][3] = 1;
    return matrix;
}

inline Matrix MatrixMulti(const Matrix& m1, const Matrix& m2) {
    Matrix result = {};
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            for (int k = 0; k < 4; ++k) {
                result.M[i][j] += m1.M[i][k] * m2.M[k][j];
            }
        }
    }
    return result;
}

inline Vector3A MarixToVector(const Matrix& matrix) {
    return Vector3A(matrix.M[3][0], matrix.M[3][1], matrix.M[3][2]);
}

inline Vector3A GetBoneWorldPos(uint64_t boneArray, int index, const Matrix& c2w) {
    return MarixToVector(MatrixMulti(TransformToMatrix(getBone(boneArray + index * BONE_STRIDE)), c2w));
}

static void AppendUtf8(std::string& out, char32_t c) {
    if (c < 0x80) {
        out += (char)c;
    } else if (c < 0x800) {
        out += (char)(0xC0 | (c >> 6));
        out += (char)(0x80 | (c & 0x3F));
    } else if (c < 0x10000) {
        out += (char)(0xE0 | (c >> 12));
        out += (char)(0x80 | ((c >> 6) & 0x3F));
        out += (char)(0x80 | (c & 0x3F));
    } else {
        out += (char)(0xF0 | (c >> 18));
        out += (char)(0x80 | ((c >> 12) & 0x3F));
        out += (char)(0x80 | ((c >> 6) & 0x3F));
        out += (char)(0x80 | (c & 0x3F));
    }
}

std::string GetNameById(uint32_t nameId)
{

    uint32_t blockIdx = nameId >> 16;
    uint32_t blockOff = nameId & 0xFFFF;

    uint64_t blockAddr = 0;
    if (!dr->Read(GName + 0x40 + blockIdx * 8, &blockAddr, 8) || !blockAddr) {
        return {};
    }
    uint64_t entryAddr = blockAddr + blockOff * 2;
    uint16_t header = 0;
    if (!dr->Read(entryAddr, &header, 2)) {
        return {};
    }

    uint32_t len = header >> 6;
    bool  isWide = header & 1;

    if (len == 0 || len > 1024) {
        return {};
    }


    if (isWide) {
        std::u32string u32(len, 0);
        if (!dr->Read(entryAddr + 2, u32.data(), len * 4)) {
        }
        
        std::string s;
        s.reserve(len * 3);
        for (char32_t c : u32) {
            AppendUtf8(s, c);
        }


        return s;
    } else {
        std::string s(len, 0);
        if (!dr->Read(entryAddr + 2, s.data(), len)) {
            return {};
        }

        return s;
    }
}