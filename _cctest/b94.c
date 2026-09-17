#include <stdlib.h>
#include <string.h>
#include <stdio.h>

char *make_greeting(const char *name) {
    char buf[32];
    strcpy(buf, "Hello, ");
    strcat(buf, name);
    return buf;
}

int main(void) {
    char *g = make_greeting("World");
    printf("%s\n", g);
    char *arr = malloc(4);
    for (int i = 0; i <= 4; i++) arr[i] = (char)i;
    free(arr);
    printf("%d\n", arr[0]);
    return 0;
}