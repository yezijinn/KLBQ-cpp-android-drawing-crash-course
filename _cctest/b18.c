#include <stdio.h>

int main(void) {
    int scores[5] = {90, 85, 88, 92, 78};

    for (int i = 0; i < 5; i++) {
        printf("scores[%d] = %d\n", i, scores[i]);
    }
    return 0;
}