// src/target.cpp —— 靶子进程（自己的）
struct Player {
    int   hp = 100;
    float x = 0, y = 0, z = 0;
    char  name[32] = "Player1";
};

Player g_players[8];          // 全局数组，便于扫描定位
Player *g_current = &g_players[0];

int main() {
    printf("pid=%d\ng_players=%p\ng_current=%p\n", getpid(), (void*)g_players, (void*)g_current);
    fflush(stdout);
    int tick = 0;
    while (true) {
        g_players[0].x = (float)(tick % 100);       // 让数据变化，便于验证
        sleep(1); tick++;
    }
}