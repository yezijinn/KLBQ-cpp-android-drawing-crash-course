// src/main.cpp
#include "vec.h"
#include <cstdio>
int main() {
    const Vec3 v{3, 4, 0};
    const Vec3 n = v.Normalized();
    printf("len=%.3f n=(%.3f,%.3f,%.3f)\n", v.Length(), n.x, n.y, n.z);
    return n.Length() > 0.999f && n.Length() < 1.001f ? 0 : 1;
}