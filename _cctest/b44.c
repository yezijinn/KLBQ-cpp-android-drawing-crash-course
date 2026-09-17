#include <stdio.h>
#include <math.h>

struct Vec3 { float x, y, z; };

float length(struct Vec3 v) { return sqrtf(v.x*v.x + v.y*v.y + v.z*v.z); }
struct Vec3 normalize(struct Vec3 v) {
    float l = length(v);
    struct Vec3 r = {v.x/l, v.y/l, v.z/l};
    return r;
}

int main(void) {
    struct Vec3 v = {3, 4, 0};
    struct Vec3 n = normalize(v);
    printf("len=%f  n=(%f,%f,%f)\n", length(v), n.x, n.y, n.z);
}