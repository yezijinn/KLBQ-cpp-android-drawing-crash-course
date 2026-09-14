APP_ABI := arm64-v8a
APP_PLATFORM := android-25
APP_STL := c++_static
APP_OPTIM := release
APP_SHORT_COMMANDS := true

APP_CPPFLAGS := \
    -std=c++17 \
    -fexceptions \
    -frtti \
    -fvisibility=hidden \
    -fdata-sections \
    -ffunction-sections \
    -funwind-tables \
    -fstack-protector-strong

APP_LDFLAGS := -flto -Wl,--gc-sections -s
