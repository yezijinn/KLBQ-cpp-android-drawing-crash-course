#include "Hack.h"
#include "driver.h"
#include "GameTools.h"
#include "draw.h"

mBase AppBase;

void pXthread() {
    if (!初始化 || dr->GetGlobalPid() <= 0 || AppBase.libUE4 <= 0x10000) {
        return;
    }
    AppBase.UWorld           = Uworld;
    AppBase.PlayerController = PlayerController;
    AppBase.AcknowledgedPawn = MySelf;
    AppBase.CameraManager    = 玩家相机;
    AppBase.CameraCache      = 玩家相机 + 0x2300;
    AppBase.PovPtr           = AppBase.CameraCache;
    AppBase.Location         = Vec3(相机位置.X, 相机位置.Y, 相机位置.Z);
    AppBase.Rotation         = Rotator{相机旋转.X, 相机旋转.Y, 相机旋转.Z};
    AppBase.Fov              = 相机FOV;
    GameTools::WorldMatrix   = GameTools::rotatorToMatrix(AppBase.Rotation);
}
