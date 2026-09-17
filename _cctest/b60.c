#include <stdio.h>
#include <stdlib.h>

int global_var = 42;

int main(void) {
    int  stack_var = 7;
    int *heap_var  = malloc(sizeof(int));

    printf("代码 main   : %p\n", (void*)main);
    printf("global_var  : %p\n", (void*)&global_var);
    printf("heap_var    : %p\n", (void*)heap_var);
    printf("stack_var   : %p\n", (void*)&stack_var);
    return 0;
}