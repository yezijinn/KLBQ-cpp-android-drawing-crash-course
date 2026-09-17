#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>
#include <string>
#include <optional>

// 模拟"另一个进程的内存"
std::vector<uint8_t> g_memory;

class FakeDriver {
public:
    int Read(uint64_t addr, void *buf, size_t size) {
        if (addr + size > g_memory.size()) return -1;
        memcpy(buf, g_memory.data() + addr, size);
        return (int)size;
    }

    template <typename T>
    T Read(uint64_t addr) {
        T v = {};
        if (Read(addr, &v, sizeof(T)) <= 0) v = T{};
        return v;
    }

    template <typename T>
    std::vector<T> ReadArray(uint64_t addr, size_t count) {
        std::vector<T> out(count);
        for (size_t i = 0; i < count; i++)
            out[i] = Read<T>(addr + i * sizeof(T));
        return out;
    }

    std::optional<std::string> ReadString(uint64_t addr, size_t maxLen = 128) {
        if (addr == 0 || addr >= g_memory.size()) return std::nullopt;
        std::string s;
        for (size_t i = 0; i < maxLen && addr + i < g_memory.size(); i++) {
            char c = Read<char>(addr + i);
            if (c == 0) break;
            s += c;
        }
        return s;
    }
};

int main(void) {
    g_memory.resize(1024, 0);
    // 在偏移 0x100 放一个 int32 = 1234
    int32_t v = 1234;
    memcpy(g_memory.data() + 0x100, &v, 4);
    // 在 0x200 放字符串
    strcpy((char*)g_memory.data() + 0x200, "BP_Character");
    // 在 0x300 放 5 个 uint64
    for (int i = 0; i < 5; i++) {
        uint64_t a = 0x1000 * (i + 1);
        memcpy(g_memory.data() + 0x300 + i * 8, &a, 8);
    }

    FakeDriver dr;
    printf("int32  = %d\n", dr.Read<int32_t>(0x100));
    // value_or：optional 有值就返回它，没值就返回括号里的默认值
    // c_str()：把 std::string 转成 C 风格的 const char*，好给 printf 用
    printf("string = %s\n", dr.ReadString(0x200).value_or("<null>").c_str());

    auto arr = dr.ReadArray<uint64_t>(0x300, 5);
    for (const auto &a : arr) printf("  0x%llX\n", (unsigned long long)a);

    printf("越界读 = %d\n", dr.Read<int32_t>(0x9999));   // 应输出 0
    return 0;
}