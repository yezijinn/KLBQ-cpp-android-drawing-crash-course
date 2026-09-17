#include <stdio.h>

int main(void) {
    int age = 25;
    float height = 1.75f;
    char grade = 'A';

    printf("年龄: %d\n", age);
    printf("身高: %.2f 米\n", height);
    printf("评级: %c\n", grade);
    printf("全部: age=%d height=%.2f grade=%c\n", age, height, grade);
    return 0;
}