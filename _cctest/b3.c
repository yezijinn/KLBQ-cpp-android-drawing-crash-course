#include <stdio.h>          // ← 预处理指令：把 printf 的说明书拿过来

int main(void) {            // ← 程序入口：int 返回值 / main 名字 / void 无参数
                            //   { 从这里开始，是 main 的函数体
    printf("Hello, KLBQ\n"); // ← 一条语句：调用 printf，输出字符串
    return 0;                // ← 一条语句：返回 0 给操作系统，表示"成功"
}                            // ← } 函数体结束