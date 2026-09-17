int main(void) {
    FILE *fp = popen("dumpsys display", "r");
    if (!fp) { printf("popen 失败\n"); return 1; }

    std::string text;
    char buf[4096];
    while (fgets(buf, sizeof(buf), fp)) text += buf;
    pclose(fp);

    auto displays = ParseDisplays(text);
    printf("发现 %zu 个显示设备:\n", displays.size());
    for (auto &d : displays) {
        printf("  [%d] %-16s layerStack=%d  %dx%d  rot=%d  type=%s\n",
               d.displayId, d.uniqueId.c_str(), d.layerStack,
               d.width, d.height, d.rotation, d.type.c_str());
    }
    return 0;
}