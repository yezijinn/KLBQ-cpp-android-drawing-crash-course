#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

void parse_float(float f) {
    uint32_t bits;
    memcpy(&bits, &f, 4);

    int sign     = (bits >> 31) & 1;
    int exponent = (bits >> 23) & 0xFF;
    uint32_t mant = bits & 0x7FFFFF;

    printf("%.6f  =  0x%08X\n", f, bits);
    printf("  符号  : %d (%s)\n", sign, sign ? "负" : "正");
    printf("  指数  : %d (实际 %d)\n", exponent, exponent - 127);
    printf("  尾数  : 0x%06X  (1 + %u/%u = %.9f)\n",
           mant, mant, 1u<<23, 1.0 + (double)mant / (1u<<23));

    if (exponent == 255)
        printf("  → %s\n", mant ? "NaN" : (sign ? "-Inf" : "+Inf"));
    else if (exponent == 0)
        printf("  → 非规格化数或 0\n");
    else
        printf("  → 值 = %+.9f\n",
               (sign ? -1.0 : 1.0) * (1.0 + (double)mant / (1u<<23))
               * pow(2.0, exponent - 127));
}

int main(void) {
    parse_float(3.14f);
    parse_float(1.0f);
    parse_float(0.0f);
    parse_float(-2.5f);
    parse_float(1.0f / 0.0f);
    parse_float(0.0f / 0.0f);
    return 0;
}