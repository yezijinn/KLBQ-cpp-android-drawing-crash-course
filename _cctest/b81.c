#include <stdio.h>
#include <string.h>

#define W 40
#define H 20
static char canvas[H][W];

// 判断点 p 是否在三角形 abc 内（重心坐标法）
static float sign(float ax, float ay, float bx, float by, float px, float py) {
    return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
}

static int in_triangle(float ax, float ay, float bx, float by,
                       float cx, float cy, float px, float py) {
    float d1 = sign(ax,ay, bx,by, px,py);
    float d2 = sign(bx,by, cx,cy, px,py);
    float d3 = sign(cx,cy, ax,ay, px,py);
    int has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    int has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
    return !(has_neg && has_pos);
}

int main(void) {
    memset(canvas, ' ', sizeof(canvas));

    // 三角形（屏幕坐标，y 向下）
    float ax = 5,  ay = 2;
    float bx = 35, by = 8;
    float cx = 15, cy = 18;

    // 遍历每个像素中心
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            float px = x + 0.5f, py = y + 0.5f;
            if (in_triangle(ax,ay, bx,by, cx,cy, px,py))
                canvas[y][x] = '*';
        }
    }

    for (int y = 0; y < H; y++) {
        canvas[y][W-1] = 0;
        printf("%s\n", canvas[y]);
    }
    return 0;
}