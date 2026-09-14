---
tags: [教程, 附录, 源码, demo引擎]
aliases: [附录I, demoengine]
---

# 附录 I · demo 引擎完整源码

> [!abstract] 这是什么
> 第 83 章介绍了 demo 引擎的设计思想，但只给了片段。
> 这里是**完整可编译的版本**——它就是卷六、卷七、卷八所有读取练习的目标进程。
>
> 编译后运行，它会持续打印"真值"，
> 你的读取程序输出的结果必须与之完全一致。

## 一、文件结构

```
demo_engine/
├── jni/
│   ├── Android.mk
│   ├── Application.mk
│   └── src/
│       ├── demo_engine.h
│       ├── demo_engine.cpp
│       └── main.cpp
└── （可选）Makefile        本地 Linux/WSL 编译用
```

## 二、demo_engine.h

```cpp
// demo_engine.h
#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdio>

// ==================== 基础数学 ====================

struct Vec3 {
    float x = 0, y = 0, z = 0;
    Vec3() = default;
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
};

struct Quat {
    float x = 0, y = 0, z = 0, w = 1;      // 单位四元数
};

struct Rotator {
    float Pitch = 0, Yaw = 0, Roll = 0;    // 单位：度
};

// ==================== 骨骼 ====================

// 单个骨骼的变换（与真实引擎一致：四元数 + 位置 + 缩放）
struct BoneTransform {
    Quat rotation;      // 0x00
    Vec3 translation;   // 0x10
    Vec3 scale;         // 0x1C
};                      // 大小 40 字节

// 但步长是 48（对齐到 16 字节，模拟真实引擎）
#define BONE_STRIDE 48
#define BONE_COUNT  56

struct PaddedBone {
    BoneTransform t;
    uint8_t pad[8];     // 补齐到 48
};

// 骨骼索引（非连续，模拟真实骨架）
enum BoneIndex : int {
    BONE_PELVIS     = 1,
    BONE_CHEST      = 6,
    BONE_HEAD       = 7,
    BONE_L_SHOULDER = 8,
    BONE_L_ELBOW    = 10,
    BONE_L_WRIST    = 11,
    BONE_R_SHOULDER = 30,
    BONE_R_ELBOW    = 31,
    BONE_R_WRIST    = 32,
    BONE_L_THIGH    = 49,
    BONE_L_KNEE     = 50,
    BONE_L_ANKLE    = 51,
    BONE_R_THIGH    = 53,
    BONE_R_KNEE     = 54,
    BONE_R_ANKLE    = 55,
};

// ==================== 组件 ====================

enum ComponentType : uint32_t {
    COMP_NONE       = 0,
    COMP_TRANSFORM  = 1,
    COMP_MESH       = 2,
    COMP_CHARACTER  = 3,
    COMP_COLLIDER   = 4,
};

// 所有组件共享的头部（模拟引擎的 UObject 头）
struct ComponentHeader {
    uint32_t type;          // 组件类型
    uint32_t size;          // 整个组件的大小
    uint64_t next;          // 下一个组件（链表）
};

struct TransformComponent {
    ComponentHeader header;
    Vec3            location;       // 世界坐标（厘米）
    Quat            rotation;
    Vec3            scale;
};

struct MeshComponent {
    ComponentHeader header;
    uint64_t        boneArray;      // 骨骼数组指针
    int32_t         boneCount;
    float           capsuleRadius;
    float           capsuleHalfHeight;
    uint64_t        pad[4];         // 填充，模拟真实布局
    BoneTransform   componentToWorld;   // 组件到世界的变换
};

// 碰撞体类型
enum ColliderType : uint32_t {
    COL_BOX      = 0,
    COL_SPHERE   = 1,
    COL_CAPSULE  = 2,
};

struct ColliderComponent {
    ComponentHeader header;
    ColliderType    type;
    union {
        struct { float hx, hy, hz; } box;       // ★ 半长
        struct { float radius; } sphere;
        struct { float radius, halfHeight; } capsule;
    };
};

struct CharacterComponent {
    ComponentHeader header;
    int32_t         hp;
    int32_t         maxHp;
    uint32_t        nameId;
    uint32_t        teamId;
    uint32_t        flags;          // bit0 = 是否 AI
};

// ==================== Actor ====================

struct Actor {
    uint64_t vtable;        // 0x00  虚表（模拟 C++ 对象）
    uint32_t objectId;      // 0x08  唯一 ID
    uint32_t nameId;        // 0x0C  FName ID
    uint64_t components;    // 0x10  组件链表头
    uint64_t next;          // 0x18  下一个 Actor（链表）
};

// ==================== 关卡与世界 ====================

struct Level {
    uint64_t actors;            // Actor 指针数组
    int32_t  actorCount;
    int32_t  actorCapacity;
};

struct PlayerController {
    Vec3    controlRotation;    // 模拟：占位
    uint64_t pawn;              // 自己的角色
    uint64_t cameraManager;
};

struct CameraManager {
    uint8_t  pad[0x2300];       // 填充到真实偏移
    Vec3     cacheLocation;
    Rotator  cacheRotation;
    float    cacheFOV;
};

struct World {
    uint64_t level;             // 当前关卡
    uint64_t playerController;
    uint64_t gameState;
};

// ==================== FName 表 ====================

class NameTable {
public:
    uint32_t Intern(const std::string &name);
    std::string GetName(uint32_t id) const;

    uint64_t BlocksAddr() const { return (uint64_t)blocks_.data(); }
    uint32_t Count() const { return (uint32_t)entries_.size(); }

    static constexpr size_t BLOCK_SIZE = 65536;

    // 供外部程序读取的布局（与真实 FNamePool 一致）
    // 外部程序通过 blocks_ 的地址 + blockIdx*8 找块
    struct Block { uint8_t data[BLOCK_SIZE]; };

private:
    std::vector<Block*> blocks_;
    size_t nextOffset_ = 0;                      // 当前块内的下一个可用字节偏移
    std::unordered_map<std::string, uint32_t> index_;
};

// ==================== 对外接口 ====================

extern World *g_world;

// 构建世界（含若干角色、箱子、地形碰撞体）
void BuildWorld();

// 打印所有真值（供外部程序对照）
void PrintTruth();

// 每帧更新（让物体动起来，验证"数据在变"）
void TickWorld(float dt);

// 查找组件
template <typename T>
T* FindComponent(Actor *a, ComponentType type) {
    if (!a || !a->components) return nullptr;
    auto *cur = (ComponentHeader*)a->components;
    while (cur) {
        if (cur->type == type) return (T*)cur;
        cur = cur->next ? (ComponentHeader*)cur->next : nullptr;
    }
    return nullptr;
}

// 计算骨骼世界坐标（与外部程序的算法一致）
Vec3 GetBoneWorldPos(uint64_t boneArray, int index, float *outWorld);
```

## 三、demo_engine.cpp

```cpp
// demo_engine.cpp
#include "demo_engine.h"
#include <cstdlib>
#include <cmath>

World *g_world = nullptr;
static NameTable g_nameTable;
static std::vector<Actor*> g_actors;
static uint32_t g_nextObjectId = 1;

// ==================== 名字表实现 ====================

uint32_t NameTable::Intern(const std::string &name) {
    // 已存在则直接返回（保证同一个名字只有一个 ID）
    auto it = index_.find(name);
    if (it != index_.end()) return it->second;

    if (name.empty() || name.size() > 1024) return 0;

    // 条目 = 2 字节头部 + 字符数据
    const size_t need = 2 + name.size();
    if (need > BLOCK_SIZE) return 0;

    // 当前块放不下就开新块
    if (blocks_.empty() || nextOffset_ + need > BLOCK_SIZE) {
        blocks_.push_back(new Block());
        nextOffset_ = 0;
    }

    const int blockIdx = (int)blocks_.size() - 1;
    const size_t offset = nextOffset_;       // 本条目在块内的字节偏移

    // 推进偏移，并保证是 2 的倍数（ID 里存的是 offset / 2）
    nextOffset_ += need;
    if (nextOffset_ & 1) nextOffset_++;

    // 写入条目：头部（bit6~15 = 长度，bit0 = 是否宽字符）
    uint8_t *entry = blocks_[blockIdx]->data + offset;
    const uint16_t header = (uint16_t)((name.size() << 6) | 0);   // 窄字符
    memcpy(entry, &header, 2);
    memcpy(entry + 2, name.data(), name.size());

    // ID = 块号（高 16 位）+ 块内偏移 / 2（低 16 位）
    const uint32_t id = ((uint32_t)blockIdx << 16) | (uint32_t)(offset / 2);
    index_[name] = id;
    return id;
}

std::string NameTable::GetName(uint32_t id) const {
    uint32_t blockIdx = id >> 16;
    uint32_t blockOff = id & 0xFFFF;

    if (blockIdx >= blocks_.size()) return {};

    const uint8_t *entry = blocks_[blockIdx]->data + blockOff * 2;
    uint16_t header = 0;
    memcpy(&header, entry, 2);

    uint32_t len = header >> 6;
    bool isWide  = header & 1;
    if (len == 0 || len > 1024) return {};

    if (isWide) return {};      // 本 demo 只用窄字符
    return std::string((const char*)(entry + 2), len);
}

// ==================== 组件与 Actor 创建 ====================

static void AttachComponent(Actor *a, ComponentHeader *c) {
    if (!a || !c) return;
    if (!a->components) { a->components = (uint64_t)c; return; }

    auto *cur = (ComponentHeader*)a->components;
    while (cur->next) cur = (ComponentHeader*)cur->next;
    cur->next = (uint64_t)c;
}

static TransformComponent* NewTransform(Vec3 loc) {
    auto *c = new TransformComponent();
    c->header = {COMP_TRANSFORM, sizeof(TransformComponent), 0};
    c->location = loc;
    c->rotation = Quat{0, 0, 0, 1};
    c->scale = Vec3{1, 1, 1};
    return c;
}

// 初始化人形骨架（15 个关键骨骼）
static void InitSkeleton(PaddedBone *bones, Vec3 root) {
    memset(bones, 0, BONE_COUNT * sizeof(PaddedBone));

    auto set = [&](int idx, float dx, float dy, float dz) {
        if (idx < 0 || idx >= BONE_COUNT) return;
        bones[idx].t.translation = Vec3(root.x + dx, root.y + dy, root.z + dz);
        bones[idx].t.rotation = Quat{0, 0, 0, 1};
        bones[idx].t.scale = Vec3{1, 1, 1};
    };

    // 单位：厘米（UE4 约定），身高约 165
    set(BONE_PELVIS,      0,    0,   90);
    set(BONE_CHEST,       0,    0,  130);
    set(BONE_HEAD,        0,    0,  165);
    set(BONE_L_SHOULDER,-20,  -15,  145);
    set(BONE_L_ELBOW,   -35,  -20,  120);
    set(BONE_L_WRIST,   -50,  -25,   95);
    set(BONE_R_SHOULDER,-20,   15,  145);
    set(BONE_R_ELBOW,   -35,   20,  120);
    set(BONE_R_WRIST,   -50,   25,   95);
    set(BONE_L_THIGH,     0,  -10,   85);
    set(BONE_L_KNEE,      0,  -12,   45);
    set(BONE_L_ANKLE,     0,  -12,    5);
    set(BONE_R_THIGH,     0,   10,   85);
    set(BONE_R_KNEE,      0,   12,   45);
    set(BONE_R_ANKLE,     0,   12,    5);
}

static void AttachCollider(Actor *a, ColliderType type,
                           float p1, float p2, float p3) {
    auto *c = new ColliderComponent();
    c->header = {COMP_COLLIDER, sizeof(ColliderComponent), 0};
    c->type = type;
    switch (type) {
    case COL_BOX:     c->box     = {p1, p2, p3}; break;
    case COL_SPHERE:  c->sphere  = {p1};         break;
    case COL_CAPSULE: c->capsule = {p1, p2};     break;
    }
    AttachComponent(a, (ComponentHeader*)c);
}

Actor* SpawnActor(const char *name, Vec3 loc, bool isCharacter, bool isAI) {
    auto *a = new Actor();
    a->vtable     = 0x400000 + (g_nextObjectId * 0x100);   // 假的虚表
    a->objectId   = g_nextObjectId++;
    a->nameId     = g_nameTable.Intern(name);
    a->components = 0;
    a->next       = 0;

    AttachComponent(a, (ComponentHeader*)NewTransform(loc));

    if (isCharacter) {
        // Mesh 组件 + 骨骼
        auto *m = new MeshComponent();
        m->header = {COMP_MESH, sizeof(MeshComponent), 0};
        m->boneCount = BONE_COUNT;
        m->capsuleRadius = 34.0f;
        m->capsuleHalfHeight = 88.0f;
        m->componentToWorld.translation = loc;
        m->componentToWorld.rotation = Quat{0, 0, 0, 1};
        m->componentToWorld.scale = Vec3{1, 1, 1};

        // 一次性分配骨骼数组（必须连续，外部程序按 stride 读）
        auto *bones = (PaddedBone*)malloc(BONE_COUNT * sizeof(PaddedBone));
        InitSkeleton(bones, loc);
        m->boneArray = (uint64_t)bones;

        AttachComponent(a, (ComponentHeader*)m);

        // 角色数据
        auto *ch = new CharacterComponent();
        ch->header = {COMP_CHARACTER, sizeof(CharacterComponent), 0};
        ch->hp = 100;
        ch->maxHp = 100;
        ch->nameId = a->nameId;
        ch->teamId = 1;
        ch->flags = isAI ? 1u : 0u;
        AttachComponent(a, (ComponentHeader*)ch);

        // 碰撞体（胶囊）
        AttachCollider(a, COL_CAPSULE, 34.0f, 88.0f);
    } else {
        // 静态物体：盒子碰撞体
        AttachCollider(a, COL_BOX, 50.0f, 50.0f, 50.0f);
    }

    g_actors.push_back(a);
    return a;
}

// ==================== 世界构建 ====================

void BuildWorld() {
    g_world = new World();

    auto *level = new Level();
    level->actorCapacity = 256;
    level->actorCount = 0;
    level->actors = (uint64_t)malloc(level->actorCapacity * sizeof(uint64_t));

    g_world->level = (uint64_t)level;

    // ---- 生成角色 ----
    SpawnActor("BP_Character_Player",   Vec3{0, 0, 0},        true,  false);
    SpawnActor("BP_Character_Enemy",    Vec3{500, 300, 0},    true,  false);
    SpawnActor("BP_Character_Enemy",    Vec3{-200, 800, 50},  true,  false);
    SpawnActor("BP_Character_JiaRen",   Vec3{1000, -400, 0},  true,  true);   // 人机
    SpawnActor("BP_Character_JiaRen",   Vec3{300, -900, 0},   true,  true);

    // ---- 生成静态物体（这些会参与遮挡判定）----
    SpawnActor("BP_Box_Static",         Vec3{200, 200, 0},    false, false);
    SpawnActor("BP_Box_Static",         Vec3{-500, 100, 0},   false, false);
    SpawnActor("BP_Box_Static",         Vec3{800, 500, 0},    false, false);

    // ---- 填充 Actor 数组 ----
    level->actorCount = (int32_t)g_actors.size();
    auto *arr = (uint64_t*)level->actors;
    for (size_t i = 0; i < g_actors.size(); i++)
        arr[i] = (uint64_t)g_actors[i];

    // ---- PlayerController（取第一个角色）----
    auto *pc = new PlayerController();
    pc->pawn = (uint64_t)g_actors[0];
    auto *cam = new CameraManager();
    cam->cacheLocation = Vec3{0, 0, 150};
    cam->cacheRotation = Rotator{0, 0, 0};
    cam->cacheFOV = 90.0f;
    pc->cameraManager = (uint64_t)cam;
    g_world->playerController = (uint64_t)pc;
}

// ==================== 真值打印 ====================

void PrintTruth() {
    printf("\n========== 真值 (TRUTH) ==========\n");
    printf("g_world         = %p\n", (void*)g_world);
    if (!g_world) return;

    printf("world.level     = %p\n", (void*)g_world->level);
    printf("world.playerCtrl= %p\n", (void*)g_world->playerController);

    auto *level = (Level*)g_world->level;
    printf("level.actors    = %p\n", (void*)level->actors);
    printf("level.actorCount= %d\n", level->actorCount);

    printf("NameTable.base  = %p  (count=%u)\n",
           (void*)g_nameTable.BlocksAddr(), g_nameTable.Count());

    printf("\n--- Actor 列表 ---\n");
    auto *arr = (uint64_t*)level->actors;
    for (int i = 0; i < level->actorCount; i++) {
        auto *a = (Actor*)arr[i];
        std::string name = g_nameTable.GetName(a->nameId);
        auto *tc = FindComponent<TransformComponent>(a, COMP_TRANSFORM);

        printf("[%d] addr=%p id=%u nameId=%u name=\"%s\" pos=(%.1f, %.1f, %.1f)\n",
               i, (void*)a, a->objectId, a->nameId, name.c_str(),
               tc ? tc->location.x : 0.0f,
               tc ? tc->location.y : 0.0f,
               tc ? tc->location.z : 0.0f);

        // 角色额外打印骨骼
        auto *mc = FindComponent<MeshComponent>(a, COMP_MESH);
        if (mc && mc->boneArray) {
            auto *bones = (PaddedBone*)mc->boneArray;
            printf("      boneArray=%p  boneCount=%d  stride=%d\n",
                   (void*)mc->boneArray, mc->boneCount, BONE_STRIDE);
            printf("      HEAD[%d] = (%.1f, %.1f, %.1f)\n", BONE_HEAD,
                   bones[BONE_HEAD].t.translation.x,
                   bones[BONE_HEAD].t.translation.y,
                   bones[BONE_HEAD].t.translation.z);
        }

        auto *cc = FindComponent<ColliderComponent>(a, COMP_COLLIDER);
        if (cc) {
            if (cc->type == COL_BOX)
                printf("      collider=BOX halfExtents=(%.1f, %.1f, %.1f)\n",
                       cc->box.hx, cc->box.hy, cc->box.hz);
            else if (cc->type == COL_CAPSULE)
                printf("      collider=CAPSULE r=%.1f halfH=%.1f\n",
                       cc->capsule.radius, cc->capsule.halfHeight);
        }
    }

    auto *pc = (PlayerController*)g_world->playerController;
    auto *cam = (CameraManager*)pc->cameraManager;
    printf("\n--- 相机 ---\n");
    printf("cameraManager   = %p\n", (void*)cam);
    printf("location        = (%.1f, %.1f, %.1f)\n",
           cam->cacheLocation.x, cam->cacheLocation.y, cam->cacheLocation.z);
    printf("rotation        = (P=%.1f, Y=%.1f, R=%.1f)\n",
           cam->cacheRotation.Pitch, cam->cacheRotation.Yaw, cam->cacheRotation.Roll);
    printf("fov             = %.1f\n", cam->cacheFOV);

    printf("\n--- 结构体大小（供对照）---\n");
    printf("sizeof(Actor)               = %zu\n", sizeof(Actor));
    printf("sizeof(TransformComponent)  = %zu\n", sizeof(TransformComponent));
    printf("sizeof(MeshComponent)       = %zu\n", sizeof(MeshComponent));
    printf("sizeof(CharacterComponent)  = %zu\n", sizeof(CharacterComponent));
    printf("sizeof(ColliderComponent)   = %zu\n", sizeof(ColliderComponent));
    printf("sizeof(BoneTransform)       = %zu\n", sizeof(BoneTransform));
    printf("BONE_STRIDE                 = %d\n", BONE_STRIDE);
    printf("==================================\n\n");
}

// ==================== 每帧更新 ====================

void TickWorld(float dt) {
    if (!g_world) return;
    auto *level = (Level*)g_world->level;
    auto *arr = (uint64_t*)level->actors;

    static float t = 0;
    t += dt;

    // 每个角色的骨骼"相对根节点"的初始偏移（只在第一帧记录一次）
    // 这样移动角色时骨架形状保持不变
    static std::vector<std::vector<Vec3>> s_boneOffsets;

    if (s_boneOffsets.size() < (size_t)level->actorCount)
        s_boneOffsets.resize(level->actorCount);

    for (int i = 0; i < level->actorCount; i++) {
        auto *a = (Actor*)arr[i];
        auto *tc = FindComponent<TransformComponent>(a, COMP_TRANSFORM);
        auto *mc = FindComponent<MeshComponent>(a, COMP_MESH);
        if (!tc) continue;

        if (!mc || !mc->boneArray) continue;

        auto *bones = (PaddedBone*)mc->boneArray;

        // ---- 首次：记录骨骼相对骨盆的偏移 ----
        if (s_boneOffsets[i].empty()) {
            const Vec3 root = bones[BONE_PELVIS].t.translation;
            s_boneOffsets[i].resize(BONE_COUNT);
            for (int b = 0; b < BONE_COUNT; b++) {
                s_boneOffsets[i][b] = Vec3{
                    bones[b].t.translation.x - root.x,
                    bones[b].t.translation.y - root.y,
                    bones[b].t.translation.z - root.z,
                };
            }
        }

        // ---- 移动角色：y 方向正弦摆动（模拟走位）----
        tc->location.y += sinf(t * 0.5f + (float)i) * 20.0f * dt;

        // ---- 按新位置重建骨骼（保持形状）----
        // 骨盆高度固定为 +90 厘米
        const Vec3 newPelvis{tc->location.x, tc->location.y, tc->location.z + 90.0f};
        for (int b = 0; b < BONE_COUNT; b++) {
            bones[b].t.translation = Vec3{
                newPelvis.x + s_boneOffsets[i][b].x,
                newPelvis.y + s_boneOffsets[i][b].y,
                newPelvis.z + s_boneOffsets[i][b].z,
            };
        }

        // 组件到世界的变换跟着更新
        mc->componentToWorld.translation = tc->location;
    }
}
```

## 四、main.cpp

```cpp
// main.cpp
#include "demo_engine.h"
#include <unistd.h>
#include <cstdio>
#include <chrono>

int main(void) {
    printf("=== demo engine 启动 ===\n");
    printf("pid = %d\n", getpid());

    BuildWorld();
    PrintTruth();

    printf("\n开始运行（物体将持续移动，验证数据在变化）\n");
    printf("用 memview / 你的读取程序来读上面的地址\n\n");
    fflush(stdout);

    auto last = std::chrono::steady_clock::now();
    int tick = 0;

    while (true) {
        usleep(100000);     // 100ms

        auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - last).count();
        last = now;

        TickWorld(dt);

        // 每 5 秒打印一次当前位置，供外部程序对照
        if (++tick % 50 == 0) {
            auto *level = (Level*)g_world->level;
            auto *arr = (uint64_t*)level->actors;
            printf("[tick %d] ", tick);
            for (int i = 0; i < level->actorCount && i < 6; i++) {
                auto *a = (Actor*)arr[i];
                auto *tc = FindComponent<TransformComponent>(a, COMP_TRANSFORM);
                if (tc) printf("(%.0f,%.0f) ", tc->location.x, tc->location.y);
            }
            printf("\n");
            fflush(stdout);
        }
    }
    return 0;
}
```

## 五、构建配置

### jni/Android.mk

```makefile
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := demo_engine
LOCAL_CPPFLAGS := -std=c++17 -Wall -O1
LOCAL_SRC_FILES := src/demo_engine.cpp src/main.cpp
LOCAL_LDLIBS := -llog
include $(BUILD_EXECUTABLE)
```

### jni/Application.mk

```makefile
APP_ABI := arm64-v8a
APP_PLATFORM := android-25
APP_STL := c++_static
APP_OPTIM := debug
```

> [!note] 为什么用 `debug`
> 因为要保留符号，方便对照 `sizeof` 和地址。
> 而且这个程序不需要性能。

### 本地编译（Linux / WSL）

```bash
g++ -std=c++17 -O1 -Wall \
    jni/src/demo_engine.cpp jni/src/main.cpp \
    -o demo_engine
./demo_engine
```

**本地更方便调试**——先用本地版本搞清楚数据结构，再到设备上跑。

## 六、运行与验证

### 在设备上跑

```bash
ndk-build
adb push libs/arm64-v8a/demo_engine /data/local/tmp/
adb shell chmod 755 /data/local/tmp/demo_engine
adb shell su -c /data/local/tmp/demo_engine
```

输出会包含所有地址和真值。**记录下来**，等下要对照。

### 用外部程序读它

```bash
# 1. 找 pid
adb shell ps -A | grep demo_engine

# 2. 列模块（应该能看到基址）
./memview <pid>

# 3. 读 g_world —— 用刚才记录的地址
./reader <pid> <g_world 地址>

# 4. 沿着链走
#    g_world → level → actors → 逐个 Actor
```

### 对照检查表

| 你要读出的 | demo 打印的 | 对应章节 |
|---|---|---|
| `g_world` 指向的 World | `g_world = 0x...` | 83 |
| `level.actorCount` = 8 | `level.actorCount= 8` | 84 |
| 每个 Actor 的 name | `name="BP_Character_Enemy"` | 85 |
| 骨骼数组地址 | `boneArray=0x...` | 86 |
| HEAD 骨骼坐标 | `HEAD[7] = (...)` | 86 |
| 相机 FOV = 90 | `fov = 90.0` | 90 |
| 盒子半长 = 50 | `collider=BOX halfExtents=(50, 50, 50)` | 93/98 |

**全部对上，说明你的解析完全正确。**

## 七、可以用这个 demo 做的练习

| 练习 | 对应章节 | 怎么做 |
|---|---|---|
| 解析对象数组 | 84 | 读 `level.actors` + `actorCount`，一次读完整数组 |
| 名字解析 | 85 | 用 `NameTable.base` + `nameId` 解析 |
| 骨骼读取 | 86 | 读 `boneArray`，按 48 步长解析 15 根骨骼 |
| 批量读优化 | 81 | 对比逐个读 vs 批量读的耗时 |
| 合法性过滤 | 89 | 故意传错误地址，看 `PtrValidator` 拒绝 |
| 相机投影 | 51/90 | 用读出的相机参数把 Actor 投影到屏幕 |
| 碰撞体重建 | 98 | 读 `ColliderComponent`，生成三角形 |
| 遮挡判定 | 95-97 | 用重建的盒子做 ray-triangle 求交 |
| 找偏移 | 87 | **不看源码**，用扫描定位 `g_world` |
| 版本适配 | 88 | 改结构体（加字段），重新定位偏移 |
| 增量更新 | 99 | 观察物体移动时哪些数据变了 |

### 特别推荐：第 87 章的"盲找"练习

1. **先不看源码**，记录 demo 打印的 `g_world` 地址
2. 关闭 demo 的 `PrintTruth`（注释掉）
3. 重新编译运行
4. 用第 87 章的方法（扫描 + 多次筛选）找到 `g_world`
5. 对比你找到的地址与实际的

**这个练习的价值**：它是"从零开始找偏移"的完整演练，
而且答案已知，你能立刻知道自己对不对。

## 八、常见问题

### Q：地址每次运行都变？

是的，堆分配（ASLR）。所以：
- `g_world` 的**地址**每次变
- 但 `g_world` 里的**内容结构**不变
- 真实项目里要找的是"静态指针 + 偏移"，而不是绝对地址

**练习方法**：把 `g_world` 的地址写进一个全局变量，
它相对代码段的位置就固定了（可以在 demo 里加一个 `g_worldPtr` 全局变量）。

### Q：怎么让地址可预测？

```cpp
// 在 demo_engine.cpp 里加
World *g_world = nullptr;
World **g_worldPtr = &g_world;      // ★ 这个全局变量的地址是静态的
```

然后外部程序可以通过"扫描指向 `g_world` 的指针"找到 `g_worldPtr`——
这就是第 87 章"指针链发现"的实践。

### Q：骨骼数组为什么用 `malloc` 而不是 `new[]`？

因为要保证**连续内存**且**不被优化掉**。
用 `malloc` 更明确，也更容易在调试时观察。

### Q：`TickWorld` 里的骨骼更新是错的？

是有意的简化——只做整体平移，不做真正的骨骼动画。
如果你要做完整的，需要：
1. 每根骨骼计算新的局部变换（含旋转）
2. 从根节点开始逐级累乘得到世界变换

这是第 48、86 章的练习内容。

## 九、扩展建议

| 扩展 | 目的 |
|---|---|
| 加 `g_worldPtr` 全局变量 | 练习指针链发现（第 87 章） |
| 加一个 `NameTable` 的宽字符条目 | 练习 UTF-32 转换（第 85 章） |
| 加地形（高度场） | 练习高度场重建（第 98 章） |
| 加 100 个物体 | 练习批量读与剔除（第 81、52 章） |
| 加一个"版本号"全局变量 | 练习版本检测（第 88 章） |
| 让某个物体周期性销毁重建 | 练习 TOCTOU 防护（深挖 F） |

> [!tip] 这个 demo 引擎就是你的"靶场"
> 它的全部代码你都能看到、能改、能验证。
> **在它身上把整套技术练熟，比直接去猜任何真实程序都高效**——
> 因为你知道正确答案。
>
> 真实程序的结构会比它复杂 100 倍，但**解析方法完全相同**。

→ 返回 [[卷七-本卷导航]]
