#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *make_name(const char *prefix) {
    char buf[64];                              // bug?
    strcpy(buf, prefix);
    return buf;
}

void process(int arr[], int n) {
    for (int i = 0; i <= n; i++)               // bug?
        arr[i] = i;
    printf("size = %zu\n", sizeof(arr));        // bug?
}

int main(void) {
    char *s = "hello";
    s[0] = 'H';                                 // bug?

    int *a = malloc(10 * sizeof(int));
    process(a, 10);
    free(a);
    printf("%d\n", a[0]);                       // bug?

    float f = 1.0f;
    int i = *(int*)&f;                          // bug?
    printf("%d\n", i);

    char *name = make_name("user");
    printf("%s\n", name);
    return 0;
}