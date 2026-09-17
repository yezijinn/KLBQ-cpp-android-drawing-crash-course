int global_hp = 100;      // 写在所有函数外面

int main(void) {
    printf("%d\n", global_hp);   // 任何函数里都能访问
    return 0;
}