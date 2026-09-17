int main(int argc, char **argv) {
    pid_t pid = atoi(argv[1]);
    uint64_t gname = strtoull(argv[2], nullptr, 16);

    MemReader mem(pid);

    printf("遍历名字表:\n");
    for (uint32_t id = 0; id < 200; id++) {
        std::string n = ReadNameFromProcess(mem, gname, id);
        if (!n.empty() && isprint(n[0]))
            printf("  [%u] %s\n", id, n.c_str());
    }
    return 0;
}