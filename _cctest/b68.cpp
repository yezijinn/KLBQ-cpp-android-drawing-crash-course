#include <cmath>
#include <cstdio>
#include <cassert>

struct Vec3 {
    float x = 0, y = 0, z = 0;
    Vec3() = default;
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    Vec3 operator+(const Vec3 &v) const { return {x+v.x, y+v.y, z+v.z}; }
    Vec3 operator-(const Vec3 &v) const { return {x-v.x, y-v.y, z-v.z}; }
    Vec3 operator*(float s) const { return {x*s, y*s, z*s}; }
    Vec3 operator/(float s) const { return {x/s, y/s, z/s}; }

    float Dot(const Vec3 &v) const { return x*v.x + y*v.y + z*v.z; }
    Vec3  Cross(const Vec3 &v) const {
        return {y*v.z - z*v.y, z*v.x - x*v.z, x*v.y - y*v.x};
    }
    float LengthSq() const { return x*x + y*y + z*z; }
    float Length() const { return sqrtf(LengthSq()); }
    Vec3  Normalized() const {
        float len = Length();
        return len < 1e-6f ? Vec3{} : (*this) / len;
    }
};

int main(void) {
    // 1. 方向 + 距离（开篇的例子）
    Vec3 me{100, 200, 50}, enemy{400, 600, 50};
    Vec3 dir = enemy - me;
    printf("方向=(%.0f,%.0f,%.0f) 距离=%.0f\n",
           dir.x, dir.y, dir.z, dir.Length());
    assert(fabsf(dir.Length() - 500.0f) < 0.001f);

    // 2. 归一化后长度必为 1
    Vec3 n = dir.Normalized();
    printf("单位方向=(%.2f,%.2f,%.2f) 模=%.6f\n", n.x, n.y, n.z, n.Length());
    assert(fabsf(n.Length() - 1.0f) < 1e-5f);

    // 3. 点积：垂直为 0
    Vec3 a{1,0,0}, b{0,1,0}, c{1,1,0};
    assert(a.Dot(b) == 0.0f);                      // 垂直
    assert(fabsf(a.Dot(a) - 1.0f) < 1e-6f);        // 自己点自己 = 长度平方

    // 4. 夹角
    float cosA = a.Dot(c.Normalized());
    printf("a 与 c 夹角 = %.1f 度\n", acosf(cosA) * 180.0f / 3.14159265f);

    // 5. 叉积：垂直且与两边都垂直
    Vec3 cr = a.Cross(b);
    printf("a×b = (%.0f,%.0f,%.0f)\n", cr.x, cr.y, cr.z);   // (0,0,1)
    assert(fabsf(cr.Dot(a)) < 1e-6f && fabsf(cr.Dot(b)) < 1e-6f);

    // 6. 零向量安全
    Vec3 zero{0,0,0};
    assert(zero.Normalized().LengthSq() == 0.0f);   // 不崩、不产生 nan
    printf("全部通过\n");
    return 0;
}