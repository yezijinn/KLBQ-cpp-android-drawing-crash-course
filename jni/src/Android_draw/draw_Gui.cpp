#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

#include "draw.h"
#include "embedded_assets.h"
#include "driver.h"
#include "MemDriver.h"
#include "variable.h"

#include "ConfigManager.h"
#include "GameTools.h"
#include "Hack.h"
#include "PhysX.h"
#include "SilentAim.h"
#include "TouchAim.h"
#include "PovAim.h"

static MemDriver g_driver;
IDriver *dr = &g_driver;

bool M_Android_LoadFont(float SizePixels) {
    ImGuiIO &io = ImGui::GetIO();
    ImFontConfig config;
    config.FontDataOwnedByAtlas = false;
    config.SizePixels = SizePixels;
    config.OversampleH = 2;
    config.OversampleV = 1;
    zh_font = io.Fonts->AddFontFromMemoryTTF(
        const_cast<unsigned char *>(g_heiti_ttf_data),
        static_cast<int>(EmbeddedAssets::HeitiSize()),
        SizePixels,
        &config,
        io.Fonts->GetGlyphRangesChineseFull());
    if (zh_font == nullptr) return false;
    io.FontDefault = zh_font;
    return true;
}

void init_My_drawdata() {
    ImGui::StyleColorsLight();
    M_Android_LoadFont(25.0f);
    ImGui::GetStyle().ScaleAllSizes(3.0f);
}

void DrawInit() {
    if (!g_driver.IsStarted()) {
        const int mode = ConfigManager::Settings.syscall驱动 ? MemDriver::MODE_SYSCALL : MemDriver::MODE_KERNEL;
        if (!g_driver.Open(mode)) {
            __android_log_print(ANDROID_LOG_ERROR, "Driver", "初始化失败: %s (%s)",
                                g_driver.CurrentModeName(), g_driver.LastError());
            return;
        }
        __android_log_print(ANDROID_LOG_INFO, "Driver", "已选择驱动: %s", g_driver.CurrentModeName());

        TouchAim::g_kernel = g_driver.kernel();

        PovAim::g_kernel = g_driver.kernel();
    }

    int gamePid = dr->GetPid("com.idreamsky.klbqm");
    if (gamePid <= 0) {
        printf("游戏未启动\n");
        return;
    }
    dr->SetGlobalPid(gamePid);

    uint64_t moduleBase = 0;
    if (!dr->GetModuleAddress("libUE4.so", 0, &moduleBase, true)) {
        printf("libUE4.so 未找到\n");
        return;
    }
    libUE4 = static_cast<long int>(moduleBase);
    AppBase.libUE4 = libUE4;
    初始化 = true;
}
void UpdateGameData() {
    if (!初始化) return;

    GName = (libUE4 + 0xb236b00);
    MatrixPtr = static_cast<long int>(dr->Read<uint64_t>(dr->Read<uint64_t>(libUE4 + 0xB3C5D00) + 0x20) + 0x270);

    MatrixData = Matrix{};
    if (MatrixPtr != 0) {
        dr->Read(static_cast<uintptr_t>(MatrixPtr), &MatrixData, sizeof(MatrixData));
    }

    Uworld = static_cast<long int>(dr->Read<uint64_t>(libUE4 + 0xB3EC650));
    if (Uworld == 0) return;

    Uleve = static_cast<long int>(dr->Read<uint64_t>(Uworld + 0x30));
    if (Uleve == 0) return;

    Arrayaddr = static_cast<long int>(dr->Read<uint64_t>(Uleve + 0x98));
    Count = dr->Read<int>(Uleve + 0xa0);

    PlayerController = static_cast<long int>(dr->Read<uint64_t>(dr->Read<uint64_t>(dr->Read<uint64_t>(dr->Read<uint64_t>(Uworld + 0x188) + 0x38) + 0x0) + 0x30));

    if (PlayerController == 0) return;

    MySelf = static_cast<long int>(dr->Read<uint64_t>(PlayerController + 0x390));
    玩家相机 = static_cast<long int>(dr->Read<uint64_t>(PlayerController + 0x3A8));

     uint64_t CachedPMCharacter = dr->Read<uint64_t>(PlayerController + 0xB38);
     if (CachedPMCharacter == 0) return;

     uint64_t InventoryComponent = dr->Read<uint64_t>(CachedPMCharacter + 0x7B0);
     if (InventoryComponent == 0) return;

     uint64_t CurrentWeapon = dr->Read<uint64_t>(InventoryComponent + 0x15C0);
     if (CurrentWeapon == 0) return;

     uint64_t AttackComponent = dr->Read<uint64_t>(CurrentWeapon + 0x848);
     if (AttackComponent == 0) return;

     ProjectilesData  = dr->Read<uint64_t>(AttackComponent + 0x280);
     ProjectilesCount = dr->Read<int>(AttackComponent + 0x288);

    相机位置 = Vector3A();
    相机旋转 = Vector3A();
    相机FOV = 0.0f;

    if (玩家相机 != 0) {
        dr->Read(static_cast<uintptr_t>(玩家相机) + 0x2300, &相机位置, sizeof(相机位置));
        dr->Read(static_cast<uintptr_t>(玩家相机) + 0x230C, &相机旋转, sizeof(相机旋转));
        相机FOV = dr->Read<float>(static_cast<uintptr_t>(玩家相机) + 0x2318);
    }

    AppBase.PlayerController = PlayerController;
    AppBase.CameraManager    = 玩家相机;
    AppBase.AcknowledgedPawn = MySelf;
    AppBase.CameraCache      = 玩家相机 + 0x2300;
    AppBase.PovPtr           = 玩家相机 + 0x2300;
    AppBase.Location         = Vec3(相机位置.X, 相机位置.Y, 相机位置.Z);
    AppBase.Rotation         = Rotator{相机旋转.X, 相机旋转.Y, 相机旋转.Z};
    AppBase.Fov              = 相机FOV;

    PovAim::真实朝向 = Rotator{相机旋转.X, 相机旋转.Y, 相机旋转.Z};
    if (!PovAim::holdActive && !PovAim::onTarget) {
        PovAim::真实朝向有效 = true;
    }

    SilentAim::Cfg.启用     = ConfigManager::Settings.自瞄启用;
    SilentAim::Cfg.无视遮挡 = ConfigManager::Settings.自瞄无视遮挡;
    SilentAim::Cfg.包含人机 = ConfigManager::Settings.自瞄包含人机;
    SilentAim::Cfg.画目标点 = ConfigManager::Settings.自瞄画目标点;
    SilentAim::Cfg.视野角度 = ConfigManager::Settings.自瞄视野角度;
    SilentAim::Cfg.最大距离 = ConfigManager::Settings.自瞄最大距离;
    SilentAim::Cfg.骨骼部位 = ConfigManager::Settings.自瞄骨骼部位;
    SilentAim::Cfg.高度偏移 = ConfigManager::Settings.自瞄高度偏移;
    SilentAim::Cfg.平滑度   = ConfigManager::Settings.自瞄平滑度;
    SilentAim::Cfg.每帧限速 = ConfigManager::Settings.自瞄每帧限速;
    SilentAim::Cfg.粘滞     = ConfigManager::Settings.自瞄粘滞;
    SilentAim::Cfg.扩大角度 = ConfigManager::Settings.自瞄扩大角度;
    SilentAim::Cfg.粘滞权重 = ConfigManager::Settings.自瞄粘滞权重;
    SilentAim::Cfg.写相机POV = ConfigManager::Settings.自瞄写相机POV;

    TouchAim::Cfg.启用   = ConfigManager::Settings.触摸自瞄启用;
    TouchAim::Cfg.模式   = ConfigManager::Settings.触摸自瞄模式;
    TouchAim::Cfg.触摸槽 = ConfigManager::Settings.触摸自瞄槽;
    TouchAim::Cfg.平滑   = ConfigManager::Settings.触摸自瞄平滑;
    TouchAim::Cfg.停留帧 = ConfigManager::Settings.触摸自瞄停留;
    TouchAim::Cfg.冷却帧 = ConfigManager::Settings.触摸自瞄冷却;
    TouchAim::Cfg.起手X  = ConfigManager::Settings.触摸起手X;
    TouchAim::Cfg.起手Y  = ConfigManager::Settings.触摸起手Y;

    PovAim::Cfg.enable        = ConfigManager::Settings.视角自瞄启用;
    PovAim::Cfg.smooth        = ConfigManager::Settings.视角自瞄平滑;
    PovAim::Cfg.suppressInput = ConfigManager::Settings.视角自瞄抑制输入;
    PovAim::Cfg.holdOnTarget  = ConfigManager::Settings.视角自瞄锁定停手;
}

static bool IsValidObject(uint64_t address) {
    return address > 0x10000000ULL && address < 0x10000000000ULL &&
           (address % 4) == 0;
}

static bool IsFiniteScreenPoint(const Vector2A &point) {
    return std::isfinite(point.X) && std::isfinite(point.Y);
}

static bool 射线被遮挡(const Vector3A &from, const Vector3A &to) {
    if (DynamicLoadScene == nullptr || HeightFieldScene == nullptr) return false;
    physx::PxVec3 origin(from.X, from.Y, from.Z);
    physx::PxVec3 target(to.X, to.Y, to.Z);
    if (DynamicLoadScene->Raycast(origin, target).hit.geomID != RTC_INVALID_GEOMETRY_ID) return true;
    if (HeightFieldScene->Raycast(origin, target).hit.geomID != RTC_INVALID_GEOMETRY_ID) return true;
    return false;
}

void DrawPlayer(ImDrawList *draw) {
    if (!初始化 || draw == nullptr || MySelf == 0 || 玩家相机 == 0 ||
        Arrayaddr == 0 || Count <= 0 || 相机FOV <= 0.0f) {
        TouchAim::Release();
        PovAim::Release();
        return;
    }

    MinimalViewInfo camViewInfo;
    camViewInfo.Location = Vec3(相机位置.X, 相机位置.Y, 相机位置.Z);
    camViewInfo.Rotation = Rotator{相机旋转.X, 相机旋转.Y, 相机旋转.Z};
    camViewInfo.FOV = 相机FOV;
    camMakeMatrix(camViewInfo);

    人数值=0;
    有目标物体 = false;
    float 最近目标距离 = 1e30f;

    SilentAim::本帧有自瞄启用 = (SilentAim::Cfg.启用 || TouchAim::Cfg.启用 || PovAim::Cfg.enable);
    SilentAim::刷新基准朝向();
    SilentAim::重置目标();

    for (int i = 0; i < Count; ++i) {
        const uint64_t objectAddress = dr->Read<uint64_t>(
            static_cast<uintptr_t>(Arrayaddr) + 0x8ULL * i);
        if (!IsValidObject(objectAddress) ||
            objectAddress == static_cast<uint64_t>(MySelf)) {
            continue;
        }

        std::string name_ = GetNameById(dr->Read<uint32_t>(objectAddress + 0x18));

        const float 胶囊体数值 = dr->Read<float>(dr->Read<uint64_t>(objectAddress + 0x380) + 0x54C);

        const uint64_t rootComponent = dr->Read<uint64_t>(objectAddress + 0x168);
        if (!IsValidObject(rootComponent)) continue;

        Vector3A rootWorld;
        if (!dr->Read(rootComponent + 0x1F0, &rootWorld, sizeof(rootWorld))) {
            continue;
        }

        const bool 被遮挡 = (physx已初始化 &&
                            (ConfigManager::Settings.方框 || ConfigManager::Settings.射线)) &&
                            射线被遮挡(相机位置,
                                      Vector3A(rootWorld.X, rootWorld.Y, rootWorld.Z + 100.0f));

        Vector2A rootScreen;
        if (!WorldToScreen(rootWorld, rootScreen)) continue;
        Vector2A topScreen;
        if (!WorldToScreen(
                Vector3A(rootWorld.X, rootWorld.Y, rootWorld.Z + 165.0f),
                topScreen)) continue;
        Vector2A bottomScreen;
        if (!WorldToScreen(
                Vector3A(rootWorld.X, rootWorld.Y, rootWorld.Z - 5.0f),
                bottomScreen)) continue;

        if (!IsFiniteScreenPoint(rootScreen) ||
            !IsFiniteScreenPoint(topScreen) ||
            !IsFiniteScreenPoint(bottomScreen)) {
            continue;
        }

        const float 目标dx = rootScreen.X - px;
        const float 目标dy = rootScreen.Y - py;
        const float 目标距离 = 目标dx * 目标dx + 目标dy * 目标dy;
        if (目标距离 < 最近目标距离) {
            最近目标距离 = 目标距离;
            目标物体位置 = Vec3(rootWorld.X, rootWorld.Y, rootWorld.Z);
            有目标物体 = true;
        }

        const float boxSpan = bottomScreen.Y - topScreen.Y;
        if (!std::isfinite(boxSpan) || boxSpan <= 1.0f) continue;

        const float boxX = rootScreen.X - boxSpan / 4.0f;
        const float boxY = bottomScreen.Y;
        const float boxWidth = boxSpan / 2.0f;
        const float boxTop = boxY - boxWidth;
        const float boxBottom = boxY + boxWidth;
        const float boxRight = boxX + boxWidth;
        float labelY = boxTop - 20.0f;
        if (!std::isfinite(boxWidth) || boxWidth <= 1.0f ||
            boxWidth > static_cast<float>(abs_ScreenX) ||
            boxRight < 0.0f || boxX > static_cast<float>(abs_ScreenX) ||
            boxBottom < 0.0f || boxTop > static_cast<float>(abs_ScreenY)) {
            continue;
        }

        if (ConfigManager::Settings.类名) {
            ImVec2 textPos = ImVec2(boxX, labelY);
            name_ += std::to_string(objectAddress);
            draw->AddText(textPos, ImColor(255, 255, 0, 255), name_.c_str());
            labelY -= 20.0f;
        }

        if(name_.find("BP_Character") == std::string::npos) {
               continue;
        }
     人数值++;

     if (SilentAim::Cfg.启用 || TouchAim::Cfg.启用 || PovAim::Cfg.enable) {
         const bool 是人机 = (name_.find("BP_Character_JiaRen") != std::string::npos);
         if (SilentAim::Cfg.包含人机 || !是人机) {

             Vec3 瞄准点(rootWorld.X, rootWorld.Y, rootWorld.Z + SilentAim::Cfg.高度偏移);
             bool 骨骼取到 = false;
             if (SilentAim::Cfg.骨骼部位 != 0) {
                 const uint64_t mesh = dr->Read<uint64_t>(objectAddress + 0x370);
                 if (IsValidObject(mesh)) {
                     const uint64_t bone  = dr->Read<uint64_t>(mesh + 0x578);
                    if (IsValidObject(bone)) {
                        const Matrix c2w = TransformToMatrix(getBone(mesh + 0x2A0));
                         const Vector3A bw = GetBoneWorldPos(bone, SilentAim::Cfg.骨骼部位, c2w);
                         if (std::isfinite(bw.X) && std::isfinite(bw.Y) && std::isfinite(bw.Z)) {
                             瞄准点 = Vec3(bw.X, bw.Y, bw.Z);
                             骨骼取到 = true;
                         }
                     }
                 }
             }
             (void)骨骼取到;

             bool 瞄准点被遮挡 = false;
             if (physx已初始化 && !SilentAim::Cfg.无视遮挡) {
                 瞄准点被遮挡 = 射线被遮挡(相机位置,
                     Vector3A(瞄准点.x, 瞄准点.y, 瞄准点.z));
             }
             SilentAim::提交候选(objectAddress, 瞄准点, 瞄准点被遮挡);
         }
     }

     if(ConfigManager::Settings.人机) {
        if (name_.find("BP_Character_JiaRen") != std::string::npos)
        {
            name_ = "人机";
        } else {
            name_ = "真人";
       }
     }
        if (ConfigManager::Settings.方框) {
            draw->AddRect(ImVec2(boxX, boxTop), ImVec2(boxRight, boxBottom),
                          被遮挡 ? BlockBoxColor : BoxColor, 0.0f, 0, 1.0f);
          if(name_ == "人机" || name_ == "真人") {
            ImVec2 textPos = ImVec2(boxX, boxTop);
            draw->AddText(textPos, ImColor(255, 255, 0, 255), name_.c_str());
            }
        }

        if (ConfigManager::Settings.射线) {
            draw->AddLine(ImVec2(px, 130.0f), ImVec2(rootScreen.X, boxTop),
                          被遮挡 ? BlockLineColor : LineColor, 1.0f);
        }

        if (ConfigManager::Settings.骨骼 || ConfigManager::Settings.骨骼索引) {
            const uint64_t mesh = dr->Read<uint64_t>(objectAddress + 0x370);
            if (IsValidObject(mesh)) {
                const uint64_t bone = dr->Read<uint64_t>(mesh + 0x578);
                if (IsValidObject(bone)) {
                    const Matrix c2w = TransformToMatrix(getBone(mesh + 0x2A0));

                    const bool doBoneVis = ConfigManager::Settings.骨骼 && physx已初始化;
                    const int boneIdx[15] = {
                        BONE_HEAD, BONE_CHEST, BONE_PELVIS,
                        BONE_L_SHOULDER, BONE_L_ELBOW, BONE_L_WRIST,
                        BONE_R_SHOULDER, BONE_R_ELBOW, BONE_R_WRIST,
                        BONE_L_THIGH, BONE_L_KNEE, BONE_L_ANKLE,
                        BONE_R_THIGH, BONE_R_KNEE, BONE_R_ANKLE
                    };

                    const int segA[14] = {1, 1, 1, 3, 4, 1, 6, 7, 2, 9, 10, 2, 12, 13};
                    const int segB[14] = {0, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};

                    struct BonePoint { Vector2A screen; bool visible; };
                    BonePoint pt[15];
                    for (int k = 0; k < 15; ++k) {
                        Vector3A w = GetBoneWorldPos(bone, boneIdx[k], c2w);
                        w.X += ConfigManager::Settings.骨骼偏移X;
                        w.Y += ConfigManager::Settings.骨骼偏移Y;
                        w.Z += ConfigManager::Settings.骨骼偏移Z;
                        pt[k].visible = !doBoneVis || !射线被遮挡(相机位置, w);
                        Vector2A s;
                        pt[k].screen = WorldToScreen(w, s) ? s : Vector2A(NAN, NAN);
                    }

                    if (ConfigManager::Settings.骨骼) {
                        const auto finite = [](const Vector2A &p) {
                            return std::isfinite(p.X) && std::isfinite(p.Y);
                        };
                        for (int s = 0; s < 14; ++s) {
                            const BonePoint &a = pt[segA[s]];
                            const BonePoint &b = pt[segB[s]];
                            if (finite(a.screen) && finite(b.screen)) {
                                draw->AddLine(ImVec2(a.screen.X, a.screen.Y),
                                              ImVec2(b.screen.X, b.screen.Y),
                                              (a.visible || b.visible) ? BoneColor : BlockBoxColor,
                                              2.5f);
                            }
                        }
                        if (finite(pt[0].screen)) {
                            draw->AddCircle(ImVec2(pt[0].screen.X, pt[0].screen.Y),
                                            boxWidth / 5.0f,
                                            pt[0].visible ? BoneColor : BlockBoxColor, 0);
                        }
                    }

                    if (ConfigManager::Settings.骨骼索引) {
                        for (int j = 0; j < 120; ++j) {
                            Vector2A s;
                            if (WorldToScreen(GetBoneWorldPos(bone, j, c2w), s) &&
                                std::isfinite(s.X) && std::isfinite(s.Y) &&
                                s.X > 0 && s.Y > 0) {
                                draw->AddCircleFilled(ImVec2(s.X, s.Y), 3.0f,
                                                      ImColor(255, 0, 0));
                                char idx[8];
                                snprintf(idx, sizeof(idx), "%d", j);
                                draw->AddText(NULL, 14.0f, ImVec2(s.X + 5, s.Y - 5),
                                              ImColor(255, 255, 0), idx);
                            }
                        }
                    }
                }
            }
        }
    }

    bool 本帧瞄准 = false;
    PovAim::设置屏幕((int)displayInfo.width, (int)displayInfo.height);
    if (PovAim::Cfg.enable) {
        本帧瞄准 = PovAim::Run();
        TouchAim::Release();
    } else if (TouchAim::Cfg.启用) {
        本帧瞄准 = TouchAim::Run();
        PovAim::Release();
    } else {
        本帧瞄准 = SilentAim::执行();
        PovAim::Release();
        TouchAim::Release();
    }

    if (本帧瞄准) {

        if (SilentAim::Cfg.画目标点 && SilentAim::当前目标.有效) {
            Vec2 屏{};
            GameTools::WorldToScreen(&屏, SilentAim::当前目标.世界位置);
            if (std::isfinite(屏.x) && std::isfinite(屏.y)) {
                draw->AddCircle(ImVec2(屏.x, 屏.y), 12.0f,
                                SilentAim::当前目标.被遮挡
                                    ? ImColor(0, 255, 0, 255)
                                    : ImColor(255, 0, 255, 255),
                                0, 2.5f);
                draw->AddLine(ImVec2(px, py), ImVec2(屏.x, 屏.y),
                              ImColor(255, 0, 255, 160), 1.5f);
            }
        }
    }

         if (ConfigManager::Settings.人数) {
            char countBuf[64];
            snprintf(countBuf, sizeof(countBuf), "%d", 人数值);
            ImVec2 textPos = ImVec2(px - 60.0f, 95.0f);
            draw->AddText(ImGui::GetFont(), 50.0f, textPos, ImColor(255, 255, 255, 255), countBuf);
   }
}

void screen_config() {
    static std::chrono::steady_clock::time_point lastTime{};
    const auto now = std::chrono::steady_clock::now();
    if (now - lastTime < std::chrono::milliseconds(250)) return;

    const auto nextDisplayInfo = android::ANativeWindowCreator::GetDisplayInfo();
    if (nextDisplayInfo.width > 0 && nextDisplayInfo.height > 0) {
        displayInfo = nextDisplayInfo;
    }
    if (!ConfigManager::Settings.过录制) {
        android::ANativeWindowCreator::ProcessMirrorDisplay();
    }
    lastTime = now;
}

static void limit_gui_frame_rate() {
    static std::chrono::steady_clock::time_point nextFrame{};
    const auto now = std::chrono::steady_clock::now();
    const int targetFps = std::clamp(ConfigManager::Settings.gui_frame_rate, 60, 185);
    const auto framePeriod = std::chrono::duration_cast<std::chrono::steady_clock::duration>(
        std::chrono::duration<double>(1.0 / targetFps));

    if (nextFrame == std::chrono::steady_clock::time_point{}) nextFrame = now;
    if (now < nextFrame) std::this_thread::sleep_until(nextFrame);

    const auto frameStart = std::chrono::steady_clock::now();
    nextFrame += framePeriod;
    if (nextFrame <= frameStart) nextFrame = frameStart + framePeriod;
}

void drawBegin() {
    limit_gui_frame_rate();
    screen_config();

    const bool validDisplaySize = displayInfo.width > 0 && displayInfo.height > 0;
    const bool displaySizeChanged = validDisplaySize &&
        (native_window_screen_x != displayInfo.width ||
         native_window_screen_y != displayInfo.height);

    if (::permeate_record_ini || displaySizeChanged) {
        if (g_window != nullptr) {
            LastCoordinate.Pos_x = g_window->Pos.x;
            LastCoordinate.Pos_y = g_window->Pos.y;
            LastCoordinate.Size_x = g_window->Size.x;
            LastCoordinate.Size_y = g_window->Size.y;
        }

        if (displaySizeChanged) {
            native_window_screen_x = displayInfo.width;
            native_window_screen_y = displayInfo.height;
            abs_ScreenX = displayInfo.width;
            abs_ScreenY = displayInfo.height;
        }

        graphics->Shutdown();
        android::ANativeWindowCreator::Destroy(window);
        window = android::ANativeWindowCreator::Create(
            "test_sysGui", native_window_screen_x, native_window_screen_y, ConfigManager::Settings.过录制);
        graphics->Init_Render(window, native_window_screen_x, native_window_screen_y);
        init_My_drawdata();
        g_window = nullptr;
        permeate_record_ini = true;
        Touch::UpdateDisplaySize({static_cast<float>(native_window_screen_x),
                                  static_cast<float>(native_window_screen_y)});
    }

    static int32_t orientation = -1;
    if (orientation != displayInfo.orientation) {
        orientation = displayInfo.orientation;
        Touch::setOrientation(displayInfo.orientation);
    }

    GameTools::ScreenSize.x = abs_ScreenX / 2;
    GameTools::ScreenSize.y = abs_ScreenY / 2;

    px = abs_ScreenX * 0.5f;
    py = abs_ScreenY * 0.5f;
}

void Layout_tick_UI(bool *main_thread_flag) {
    UpdateGameData();

    static int styleIdx = 0;
    ImGui::Begin("晚宀是男娘", main_thread_flag);
    ImGui::Text("晚宀是雨生的星奴力");
    if (permeate_record_ini) {
        const float restoredWidth = std::min(LastCoordinate.Size_x,
                                             static_cast<float>(displayInfo.width));
        const float restoredHeight = std::min(LastCoordinate.Size_y,
                                              static_cast<float>(displayInfo.height));
        const float maxX = std::max(0.0f, static_cast<float>(displayInfo.width) - restoredWidth);
        const float maxY = std::max(0.0f, static_cast<float>(displayInfo.height) - restoredHeight);
        ImGui::SetWindowSize({restoredWidth, restoredHeight});
        ImGui::SetWindowPos({std::clamp(LastCoordinate.Pos_x, 0.0f, maxX),
                             std::clamp(LastCoordinate.Pos_y, 0.0f, maxY)});
        permeate_record_ini = false;
    }

    ImGui::Text("渲染接口 : %s, gui版本 : %s", graphics->RenderName, ImGui::GetVersion());
    if (ImGui::Checkbox("过录制", &ConfigManager::Settings.过录制)) permeate_record_ini = true;

    if (!ImGui::BeginTabBar("MainTabs")) {
        ImGui::End();
        return;
    }

    if (ImGui::BeginTabItem("主功能")) {

    if (ImGui::Combo("##主题", &styleIdx, "白色主题\0蓝色主题\0紫色主题\0")) {
        switch (styleIdx) {
            case 0: ImGui::StyleColorsLight(); break;
            case 1: ImGui::StyleColorsDark(); break;
            case 2: ImGui::StyleColorsClassic(); break;
        }
    }

    int driverIdx = ConfigManager::Settings.syscall驱动 ? 1 : 0;
    ImGui::BeginDisabled(!g_driver.CanSwitch());
    if (ImGui::Combo("驱动选择", &driverIdx, "lsdriver\0syscall\0")) {
        ConfigManager::Settings.syscall驱动 = (driverIdx == 1);
        ConfigManager::SaveConfig();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextColored(g_driver.CanSwitch() ? ImVec4(0.0f, 0.6f, 0.0f, 1.0f) : ImVec4(0.8f, 0.4f, 0.0f, 1.0f),
                       g_driver.CanSwitch() ? "未启动" : "已启动");

    if (ImGui::Button("初始化绘制", ImVec2(ImGui::GetContentRegionAvail().x, 50))) {
        DrawInit();
    }

    if (ImGui::Button("physx初始化", ImVec2(ImGui::GetContentRegionAvail().x, 50))) {
        if (!physx已初始化) {
            if (LineTrace::initPhysX() == true) {
                thread(&VisibleCheck::UpdateSceneByRange).detach();
                thread(&VisibleCheck::UpdateDynamicHeightField).detach();
                thread(&VisibleCheck::UpdateDynamicRigid).detach();
                physx已初始化 = true;
            }
        }
    }

    ImGui::Checkbox("显示方框", &ConfigManager::Settings.方框);
    ImGui::SameLine(0, 40);
    ImGui::Checkbox("显示射线", &ConfigManager::Settings.射线);
    ImGui::Checkbox("类名", &ConfigManager::Settings.类名);
    ImGui::SameLine(0, 40);
    ImGui::Checkbox("人数", &ConfigManager::Settings.人数);
    ImGui::SameLine(0, 40);
    ImGui::Checkbox("人机判断", &ConfigManager::Settings.人机);
    ImGui::Checkbox("显示骨骼", &ConfigManager::Settings.骨骼);
    ImGui::SameLine(0, 40);
    ImGui::Checkbox("骨骼索引", &ConfigManager::Settings.骨骼索引);
    ImGui::SliderFloat("骨骼偏移X", &ConfigManager::Settings.骨骼偏移X, -50.0f, 50.0f, "%.1f");
    ImGui::SliderFloat("骨骼偏移Y", &ConfigManager::Settings.骨骼偏移Y, -50.0f, 50.0f, "%.1f");
    ImGui::SliderFloat("骨骼偏移Z", &ConfigManager::Settings.骨骼偏移Z, -50.0f, 50.0f, "%.1f");
    ImGui::SliderFloat("垂直焦距系数", &ConfigManager::Settings.垂直焦距系数, 0.2f, 2.5f, "%.3f");
    ImGui::SliderFloat("水平焦距系数", &ConfigManager::Settings.水平焦距系数, 0.2f, 2.5f, "%.3f");
    ImGui::Checkbox("矩阵投影W2S", &ConfigManager::Settings.矩阵W2S);
    ImGui::SameLine();
    ImGui::TextColored(MatrixPtr != 0 ? ImVec4(0.0f, 0.8f, 0.0f, 1.0f) : ImVec4(0.8f, 0.0f, 0.0f, 1.0f),
                       "矩阵:%s", MatrixPtr != 0 ? "有效" : "无效");

    ImGui::Checkbox("PhysX", &ConfigManager::Settings.PhysX);
    ImGui::SliderInt("GUI 帧率", &ConfigManager::Settings.gui_frame_rate, 60, 185, "%d FPS");

    ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("自瞄")) {

    ImGui::Separator();
    ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), "—— 视角自瞄 (第2类/平滑转动) ——");
    ImGui::Checkbox("启用视角自瞄", &ConfigManager::Settings.视角自瞄启用);
    ImGui::SameLine(0, 40);
    if (PovAim::g_kernel == nullptr)
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "无内核: 仅写角度(不抑制输入)");
    else
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "内核已就绪(可抑制输入)");

    ImGui::SliderInt("视角平滑(1=瞬移)", &ConfigManager::Settings.视角自瞄平滑, 1, 15, "%d");
    ImGui::Checkbox("抑制玩家输入", &ConfigManager::Settings.视角自瞄抑制输入);
    ImGui::SameLine(0, 40);
    ImGui::Checkbox("到位后停手", &ConfigManager::Settings.视角自瞄锁定停手);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("勾选: 转到目标后停止写角度(防微抖)\n不勾: 持续跟随目标");
    ImGui::Text("视角状态: %s  抑制指=%s", PovAim::onTarget ? "已对准" : "转动中",
                PovAim::holdActive ? "按下" : "抬起");

    ImGui::Separator();
    ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "—— 静默自瞄 ——");
    ImGui::Checkbox("启用自瞄", &ConfigManager::Settings.自瞄启用);
    ImGui::SameLine(0, 40);
    ImGui::Checkbox("无视遮挡", &ConfigManager::Settings.自瞄无视遮挡);
    ImGui::SameLine(0, 40);
    ImGui::Checkbox("包含人机", &ConfigManager::Settings.自瞄包含人机);
    ImGui::Checkbox("画目标点", &ConfigManager::Settings.自瞄画目标点);
    ImGui::SameLine(0, 40);
    ImGui::Checkbox("写相机POV(会强锁!)", &ConfigManager::Settings.自瞄写相机POV);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("勾选=同时改相机POV, 屏幕视角会跟着动(强锁)\n不勾=只改ControlRotation, 真静默");

    ImGui::SliderFloat("自瞄视野(度)", &ConfigManager::Settings.自瞄视野角度, 1.0f, 60.0f, "%.1f");
    ImGui::SliderFloat("最大距离", &ConfigManager::Settings.自瞄最大距离, 1000.0f, 100000.0f, "%.0f");
    ImGui::SliderInt("平滑度(1=静默)", &ConfigManager::Settings.自瞄平滑度, 1, 10, "%d");
    ImGui::SliderFloat("每帧限速(度)", &ConfigManager::Settings.自瞄每帧限速, 0.0f, 180.0f, "%.0f");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("0 = 不限速(跟随最快)\n越小越平滑, 但不跟手");
    ImGui::Checkbox("目标粘滞", &ConfigManager::Settings.自瞄粘滞);
    ImGui::SameLine(0, 20);
    ImGui::SliderFloat("扩大角度", &ConfigManager::Settings.自瞄扩大角度, 0.0f, 45.0f, "%.0f");
    ImGui::SliderFloat("高度偏移(根节点模式)", &ConfigManager::Settings.自瞄高度偏移, -100.0f, 200.0f, "%.0f");

    static int 瞄准部位Idx = -1;
    if (瞄准部位Idx < 0) {
        switch (ConfigManager::Settings.自瞄骨骼部位) {
            case BONE_HEAD:   瞄准部位Idx = 0; break;
            case BONE_CHEST:  瞄准部位Idx = 1; break;
            case BONE_PELVIS: 瞄准部位Idx = 2; break;
            default:          瞄准部位Idx = 3; break;
        }
    }
    const char *瞄准部位项 = "头部\0胸部\0骨盆\0根节点\0";
    if (ImGui::Combo("瞄准部位", &瞄准部位Idx, 瞄准部位项)) {
        switch (瞄准部位Idx) {
            case 0: ConfigManager::Settings.自瞄骨骼部位 = BONE_HEAD;   break;
            case 1: ConfigManager::Settings.自瞄骨骼部位 = BONE_CHEST;  break;
            case 2: ConfigManager::Settings.自瞄骨骼部位 = BONE_PELVIS; break;
            default: ConfigManager::Settings.自瞄骨骼部位 = 0;          break;
        }
    }

    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "—— 触摸自瞄 (内核) ——");
    ImGui::Checkbox("启用触摸自瞄", &ConfigManager::Settings.触摸自瞄启用);
    ImGui::SameLine(0, 40);
    if (TouchAim::g_kernel == nullptr)
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "无内核驱动!");
    else
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "内核已就绪");

    int 触摸模式Idx = ConfigManager::Settings.触摸自瞄模式;
    const char *触摸模式项 = "绝对拖拽\0相对增量\0";
    if (ImGui::Combo("触摸模式", &触摸模式Idx, 触摸模式项)) {
        ConfigManager::Settings.触摸自瞄模式 = 触摸模式Idx;
    }
    ImGui::SliderInt("触摸平滑(大=慢)", &ConfigManager::Settings.触摸自瞄平滑, 1, 20, "%d");
    ImGui::SliderInt("触摸槽位", &ConfigManager::Settings.触摸自瞄槽, 0, 9, "%d");
    ImGui::SliderInt("冷却帧", &ConfigManager::Settings.触摸自瞄冷却, 0, 30, "%d");
    ImGui::InputInt("起手X(-1自动)", &ConfigManager::Settings.触摸起手X);
    ImGui::InputInt("起手Y(-1自动)", &ConfigManager::Settings.触摸起手Y);
    ImGui::Text("触摸状态: %s  槽=%d", TouchAim::pressed ? "按下中" : "抬起", TouchAim::slot);

    if (SilentAim::当前目标.有效) {
        ImGui::TextColored(ImVec4(0.0f, 0.8f, 0.0f, 1.0f),
                           "锁定: 0x%llx  屏距 %.1f  世界距 %.1f%s",
                           (unsigned long long)SilentAim::当前目标.actor,
                           SilentAim::当前目标.屏幕距离,
                           SilentAim::当前目标.世界距离,
                           SilentAim::当前目标.被遮挡 ? "  [遮挡]" : "");
    } else {
        ImGui::TextDisabled("未锁定目标");
    }

    ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("信息")) {

    ImGui::Separator();
    ImGui::Text("GName: 0x%llx", static_cast<unsigned long long>(GName));
    ImGui::Text("MatrixPtr: 0x%llx", static_cast<unsigned long long>(MatrixPtr));
    ImGui::Text("Matrix:");
    for (int row = 0; row < 4; ++row) {
        ImGui::Text("  [%d] %.2f  %.2f  %.2f  %.2f", row,
                    MatrixData.M[row][0], MatrixData.M[row][1],
                    MatrixData.M[row][2], MatrixData.M[row][3]);
    }
    ImGui::Text("Uworld: 0x%llx", static_cast<unsigned long long>(Uworld));
    ImGui::Text("Uleve: 0x%llx", static_cast<unsigned long long>(Uleve));
    ImGui::Text("Arrayaddr: 0x%llx", static_cast<unsigned long long>(Arrayaddr));
    ImGui::Text("Count: %d", Count);
    ImGui::Text("PlayerController: 0x%llx", static_cast<unsigned long long>(PlayerController));
    ImGui::Text("MySelf: 0x%llx", static_cast<unsigned long long>(MySelf));
    ImGui::Text("玩家相机: 0x%llx", static_cast<unsigned long long>(玩家相机));
    ImGui::Text("相机位置: X %.2f  Y %.2f  Z %.2f", 相机位置.X, 相机位置.Y, 相机位置.Z);
    ImGui::Text("相机旋转: X %.2f  Y %.2f  Z %.2f", 相机旋转.X, 相机旋转.Y, 相机旋转.Z);
    ImGui::Text("FOV: %.2f", 相机FOV);
    ImGui::Text("驱动: %s", g_driver.CurrentModeName());
    ImGui::Text("PID: %d", dr ? dr->GetGlobalPid() : -1);

    ImGui::Text("mBase : %lu", AppBase.libUE4);
    ImGui::Text("PhysX Dynamic Count : %zu", DynamicRigidScene ? DynamicRigidScene->GetMeshDatas().size() : 0);
    ImGui::Text("PhysX Height Count : %zu", HeightFieldScene ? HeightFieldScene->GetMeshDatas().size() : 0);
    ImGui::Text("PhysX Static Count : %zu", DynamicLoadScene ? DynamicLoadScene->GetMeshDatas().size() : 0);

    if (ImGui::Button("All")) {
        ConfigManager::Settings.PhysXType = 0;
    }
    ImGui::SameLine();
    if (ImGui::Button("Hit")) {
        ConfigManager::Settings.PhysXType = 1;
    }

    if (ImGui::Button("保存配置", ImVec2(-1, 50))) {
        ConfigManager::SaveConfig();
    }

    ImGui::TextColored(ImVec4(1.0f, 0.0f, 1.0f, 1.0f),
                       "应用平均 %.3f ms/frame (%.1f FPS)",
                       1000.0f / ImGui::GetIO().Framerate,
                       ImGui::GetIO().Framerate);

    ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
    g_window = ImGui::GetCurrentWindow();
    ImGui::End();
}