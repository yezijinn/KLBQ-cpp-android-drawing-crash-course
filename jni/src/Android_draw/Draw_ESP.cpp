#include "Draw_ESP.h"
#include "ConfigManager.h"
#include "GameTools.h"
#include "Hack.h"
#include "draw.h"
#include "WorldToScreen.h"
#include "PhysX.h"

void drawMesh(TriangleMeshData* mesh, ImDrawList* Draw) {
    if (!mesh) return;

    for (auto i = 0; i < mesh->Indices.size(); i += 3) {

        const auto v0 = mesh->Vertices[mesh->Indices[i]];
        const auto v1 = mesh->Vertices[mesh->Indices[i + 1]];
        const auto v2 = mesh->Vertices[mesh->Indices[i + 2]];

        Vec2 s0 = GameTools::WorldToScreen(Vec3(v0.x, v0.y, v0.z));
        Vec2 s1 = GameTools::WorldToScreen(Vec3(v1.x, v1.y, v1.z));
        Vec2 s2 = GameTools::WorldToScreen(Vec3(v2.x, v2.y, v2.z));

        if (s0.x > 0 && s0.x < abs_ScreenX && s0.y > 0 && s0.y < abs_ScreenY ||
            s1.x > 0 && s1.x < abs_ScreenX && s1.y > 0 && s1.y < abs_ScreenY ||
            s2.x > 0 && s2.x < abs_ScreenX && s2.y > 0 && s2.y < abs_ScreenY) {
            Draw->AddTriangle(
                ImVec2(s0.x, s0.y),
                ImVec2(s1.x, s1.y),
                ImVec2(s2.x, s2.y),
                ImColor(255, 255, 255, 255),
                0.5f
            );
        }
    }
}

void DrawESP(ImDrawList *Draw) {

    // 统一矩阵投影: 每帧用当前相机构建一次 (WorldToScreen.h)
    MinimalViewInfo camViewInfo;
    camViewInfo.Location = AppBase.Location;
    camViewInfo.Rotation = AppBase.Rotation;
    camViewInfo.FOV = AppBase.Fov;
    camMakeMatrix(camViewInfo);

    if (ConfigManager::Settings.PhysX) {

        if (ConfigManager::Settings.PhysXType == 0) {
            if (DynamicLoadScene) {
                auto HitMesh = DynamicLoadScene->GetMeshDatas();
                for (const auto& Mesh : HitMesh) {
                    drawMesh(Mesh.get(), Draw);
                }
            }
            if (HeightFieldScene) {
                auto HitMesh = HeightFieldScene->GetMeshDatas();
                for (const auto& Mesh : HitMesh) {
                    drawMesh(Mesh.get(), Draw);
                }
            }
            if (DynamicRigidScene) {
                auto HitMesh = DynamicRigidScene->GetMeshDatas();
                for (const auto& Mesh : HitMesh) {
                    drawMesh(Mesh.get(), Draw);
                }
            }
        } else {
            Vec3 Forward = GameTools::GetForward(AppBase.Rotation);
            Vec3 End = AppBase.Location + Forward * 100000.0f; // forward 10000 speed
            physx::PxVec3 origin(AppBase.Location.x, AppBase.Location.y, AppBase.Location.z);
            physx::PxVec3 target(End.x, End.y, End.z);

            if (DynamicLoadScene) {
                auto rayHit = DynamicLoadScene->Raycast(origin, target);
                if (rayHit.hit.geomID != RTC_INVALID_GEOMETRY_ID) {
                    auto HitMesh = DynamicLoadScene->GetGeomeoryData(rayHit.hit.geomID);
                    drawMesh(HitMesh, Draw);
                }
            }
            if (HeightFieldScene) {
                auto rayHit = HeightFieldScene->Raycast(origin, target);
                if (rayHit.hit.geomID != RTC_INVALID_GEOMETRY_ID) {
                    auto HitMesh = HeightFieldScene->GetGeomeoryData(rayHit.hit.geomID);
                    drawMesh(HitMesh, Draw);
                }
            }
            if (DynamicRigidScene) {
                auto rayHit = DynamicRigidScene->Raycast(origin, target);
                if (rayHit.hit.geomID != RTC_INVALID_GEOMETRY_ID) {
                    auto HitMesh = DynamicRigidScene->GetGeomeoryData(rayHit.hit.geomID);
                    drawMesh(HitMesh, Draw);
                }
            }
        }
    }
}
