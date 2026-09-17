// memview.cpp
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>

struct MapRegion {
    uint64_t start;
    uint64_t end;
    uint8_t  perms;        // 位: 1=R 2=W 4=X
    bool     isPrivate;
    uint64_t offset;
    uint32_t devMajor, devMinor;
    uint64_t inode;
    std::string path;
};

std::vector<MapRegion> g_maps;

bool LoadMaps(pid_t pid) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/maps", pid);
    std::ifstream f(path);
    if (!f) { printf("打不开 %s（进程不存在或没权限）\n", path); return false; }
    std::stringstream ss;
    ss << f.rdbuf();
    g_maps = ParseMaps(ss.str());
    return !g_maps.empty();
}

void PrintModules() {
    printf("=== 模块列表 ===\n");
    printf("%-18s %-18s %-6s %s\n", "起始", "结束", "权限", "路径");
    std::string last;
    for (const auto &r : g_maps) {
        if (r.path.empty() || r.path == last) continue;
        last = r.path;
        char p[8] = "---";
        if (r.perms & 1) p[0] = 'r';
        if (r.perms & 2) p[1] = 'w';
        if (r.perms & 4) p[2] = 'x';
        printf("0x%016llX 0x%016llX %-6s %s\n",
               (unsigned long long)r.start, (unsigned long long)r.end,
               p, r.path.c_str());
    }
}

void PrintScannable() {
    auto regions = GetScannableRegions(g_maps);
    size_t total = 0;
    for (auto &r : regions) total += r.second - r.first;
    printf("\n=== 可扫描区域: %zu 个，共 %.1f MB ===\n",
           regions.size(), total / 1024.0 / 1024.0);
}

int main(int argc, char **argv) {
    if (argc < 2) { printf("用法: %s <pid> [模块名]\n", argv[0]); return 1; }

    pid_t pid = atoi(argv[1]);
    if (!LoadMaps(pid)) return 1;

    PrintModules();
    PrintScannable();

    if (argc >= 3) {
        uint64_t base = FindModuleBase(g_maps, argv[2]);
        if (base) printf("\n模块 %s 基址 = 0x%016llX\n", argv[2],
                         (unsigned long long)base);
        else       printf("\n未找到模块 %s\n", argv[2]);
    }
    return 0;
}