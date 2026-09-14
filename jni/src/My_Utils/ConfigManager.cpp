#include "ConfigManager.h"
#include <unistd.h>
#include <fcntl.h>
#include <cstring>

namespace ConfigManager {
    int fd;
    Config Settings;

     uint8_t CRYPTO_KEY[] = {0x8B, 0x8F, 0xD3, 0xDF, 0x7B, 0xF7, 0xD, 0x9F};
     size_t KEY_LENGTH = sizeof(CRYPTO_KEY);

    void CryptoProcess(uint8_t* data, const size_t length) {
        for (size_t i = 0; i < length; ++i) {
            data[i] ^= CRYPTO_KEY[i % KEY_LENGTH];
        }
    }

    bool LoadConfig() {
        fd = open("/data/Config", O_RDONLY);
        if (fd > 0) {
            uint8_t raw[sizeof(Config)];
            memset(raw, 0, sizeof(raw));
            ssize_t bytesRead = read(fd, raw, sizeof(raw));
            close(fd);

            if (bytesRead > 0) {
                CryptoProcess(raw, static_cast<size_t>(bytesRead));

                Config loaded;
                memset(&loaded, 0, sizeof(loaded));
                const size_t copyLen = (static_cast<size_t>(bytesRead) < sizeof(Config))
                                       ? static_cast<size_t>(bytesRead)
                                       : sizeof(Config);
                memcpy(&loaded, raw, copyLen);

                if (static_cast<size_t>(bytesRead) < sizeof(Config)) {
                    uint8_t* dst = reinterpret_cast<uint8_t*>(&loaded);
                    uint8_t* def = reinterpret_cast<uint8_t*>(&Settings);
                    memcpy(dst + copyLen, def + copyLen, sizeof(Config) - copyLen);
                }

                Settings = loaded;
                return true;
            }
        }
        return false;
    }

    bool SaveConfig() {
        Config ConfigSave = Settings;
        CryptoProcess(reinterpret_cast<uint8_t*>(&ConfigSave), sizeof(Config));

        fd = open("/data/Config", O_WRONLY | O_CREAT | O_TRUNC, 0660);
        if (fd > 0) {
            write(fd, &ConfigSave, sizeof(Config));
            close(fd);
            return true;
        }
        return false;
    }
}