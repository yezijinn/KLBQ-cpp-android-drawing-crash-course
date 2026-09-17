#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int find_marker(const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len - 4; i++) {
        if (data[i] == 0xDE && data[i+1] == 0xAD &&
            data[i+2] == 0xBE && data[i+3] == 0xEF)
            return (int)i;
    }
    return -1;
}

int main(int argc, char **argv) {
    size_t n = argc > 1 ? atoi(argv[1]) : 16;
    uint8_t *buf = malloc(n);
    memset(buf, 0, n);
    if (n >= 4) { buf[n-4] = 0xDE; buf[n-3] = 0xAD; buf[n-2] = 0xBE; buf[n-1] = 0xEF; }
    printf("found at %d\n", find_marker(buf, n));
    free(buf);
    return 0;
}