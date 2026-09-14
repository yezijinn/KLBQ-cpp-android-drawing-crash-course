#include "draw.h"    //绘制套
#include "AndroidImgui.h"     //创建绘制套
#include "ConfigManager.h"
#include "Draw_ESP.h"
#include "GraphicsManager.h" //获取 当前渲染模式
#include "Hack.h"



int main(int argc, char *argv[]) {
    // 启动先读取已保存配置 (全量 GUI 设置, 存于 /data/Config)
    ConfigManager::LoadConfig();

    ::graphics = GraphicsManager::getGraphicsInterface();

    //获取屏幕信息    
    ::screen_config(); 

    ::native_window_screen_x = ::displayInfo.width;
    ::native_window_screen_y = ::displayInfo.height;
    ::abs_ScreenX = ::displayInfo.width;
    ::abs_ScreenY = ::displayInfo.height;

    ::window = android::ANativeWindowCreator::Create("test", native_window_screen_x, native_window_screen_y, ConfigManager::Settings.过录制);
    ::graphics->Init_Render(::window, native_window_screen_x, native_window_screen_y);
    
    Touch::Init({(float)::abs_ScreenX, (float)::abs_ScreenY}, true);
    Touch::setOrientation(displayInfo.orientation);

    ::init_My_drawdata(); 
    static bool flag = true;
    while (flag) {
        drawBegin();
        Touch::UpdateImGuiInput();
        graphics->NewFrame(true);
        
        Layout_tick_UI(&flag);
        pXthread();
        DrawPlayer(ImGui::GetForegroundDrawList());
        DrawESP(ImGui::GetForegroundDrawList());

        graphics->EndFrame();        
    }
    
    Touch::Close();
    graphics->Shutdown();
    android::ANativeWindowCreator::Destroy(::window);
    return 0;
}
