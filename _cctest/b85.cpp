int main(int argc, char **argv) {
    if (argc < 4) {
        printf("用法: %s <pid> <模块名> <符号名>\n", argv[0]);
        return 1;
    }
    pid_t pid = atoi(argv[1]);

    MemReader mem(pid);

    // 1. 找模块基址
    auto maps = LoadMaps(pid);
    uint64_t base = FindModuleBase(maps, argv[2]);
    if (!base) { printf("未找到模块 %s\n", argv[2]); return 1; }
    printf("模块 %s 基址 = 0x%llX\n", argv[2], (unsigned long long)base);

    // 2. 解析 dynamic
    ModuleInfo mod;
    if (!ParseDynamic(mem, base, mod)) {
        printf("解析 dynamic 失败（可能已 strip 或无 PT_DYNAMIC）\n");
        return 1;
    }
    printf("符号表 = 0x%llX  字符串表 = 0x%llX  符号数 = %zu\n",
           (unsigned long long)mod.symtab,
           (unsigned long long)mod.strtab, mod.symcount);

    // 3. 查找
    uint64_t addr = FindSymbol(mem, mod, argv[3]);
    if (addr)
        printf("%s → 0x%llX (偏移 0x%llX)\n", argv[3],
               (unsigned long long)addr, (unsigned long long)(addr - base));
    else
        printf("未找到符号 %s\n", argv[3]);
    return 0;
}