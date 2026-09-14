LOCAL_PATH := $(call my-dir)

# Prebuilt libraries used by PhysX/Embree.
include $(CLEAR_VARS)
LOCAL_MODULE := embree_prebuilt
LOCAL_SRC_FILES := include/Embree/libembree4.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := lexers_prebuilt
LOCAL_SRC_FILES := include/Embree/liblexers.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := math_prebuilt
LOCAL_SRC_FILES := include/Embree/libmath.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := simd_prebuilt
LOCAL_SRC_FILES := include/Embree/libsimd.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := sys_prebuilt
LOCAL_SRC_FILES := include/Embree/libsys.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := task_prebuilt
LOCAL_SRC_FILES := include/Embree/libtasking.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := Android_imgui_Vulkan.rc

LOCAL_CFLAGS := -std=c17
LOCAL_CFLAGS += -fvisibility=hidden
LOCAL_CPPFLAGS := -std=c++20
LOCAL_CPPFLAGS += -fvisibility=hidden
LOCAL_CPPFLAGS += -fexceptions
LOCAL_CPPFLAGS += -frtti

LOCAL_CPPFLAGS += -DVK_USE_PLATFORM_ANDROID_KHR
LOCAL_CPPFLAGS += -DIMGUI_IMPL_VULKAN_NO_PROTOTYPES
LOCAL_CPPFLAGS += -DIMGUI_DISABLE_DEBUG_TOOLS #禁用imgui调试工具
LOCAL_ASFLAGS += -I$(LOCAL_PATH)/include

#引入头文件到全局#
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include/Android_draw
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include/ImGui
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include/Vulkan
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include/Embree
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include/Embree/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include/Embree/foundation
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include/Hack
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include/My_Utils
LOCAL_C_INCLUDES += $(LOCAL_PATH)/src/Android_draw

LOCAL_SRC_FILES := src/main.cpp
LOCAL_SRC_FILES += src/Android_draw/draw_Gui.cpp
LOCAL_SRC_FILES += src/Android_draw/Draw_ESP.cpp
LOCAL_SRC_FILES += src/Hack/Hack.cpp
LOCAL_SRC_FILES += src/My_Utils/GameTools.cpp
LOCAL_SRC_FILES += src/My_Utils/ConfigManager.cpp
LOCAL_SRC_FILES += src/My_Utils/SysHal.cpp
LOCAL_SRC_FILES += src/ImGui/TouchHelperA.cpp
LOCAL_SRC_FILES += src/ImGui/AndroidImgui.cpp
LOCAL_SRC_FILES += src/ImGui/my_imgui_impl_android.cpp
LOCAL_SRC_FILES += src/ImGui/stb_image.cpp
LOCAL_SRC_FILES += src/Vulkan/GraphicsManager.cpp
LOCAL_SRC_FILES += src/Vulkan/VulkanGraphics.cpp
LOCAL_SRC_FILES += src/Vulkan/vulkan_wrapper.cpp
LOCAL_SRC_FILES += src/ImGui/imgui.cpp
LOCAL_SRC_FILES += src/ImGui/imgui_draw.cpp
LOCAL_SRC_FILES += src/ImGui/imgui_tables.cpp
LOCAL_SRC_FILES += src/ImGui/imgui_widgets.cpp
LOCAL_SRC_FILES += src/ImGui/imgui_impl_vulkan.cpp
LOCAL_SRC_FILES += src/ImGui/heiti_ttf.cpp

LOCAL_LDLIBS := -llog -landroid -lz
LOCAL_STATIC_LIBRARIES := \
    embree_prebuilt \
    lexers_prebuilt \
    math_prebuilt \
    sys_prebuilt \
    task_prebuilt \
    simd_prebuilt

include $(BUILD_EXECUTABLE) #可执行文件
