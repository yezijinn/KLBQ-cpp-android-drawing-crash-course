---
tags: [教程, 卷七, 引擎, 架构]
day: 53
aliases: [ch83]
---

# 第 83 章 · Actor / Component 模型

> [!abstract] 本章目标
> 理解现代游戏引擎的对象组织方式，并**写一个自己的 demo 引擎**——
> 它就是卷六、卷七所有读取练习的目标进程。

> [!note] 承上
> 卷六解决了"能读内存"。从卷七起解决"**读出来的字节怎么还原成有意义的对象**"。
> 本章先**自己写一个 demo 引擎**——它是后面所有读取练习的靶子。

## 为什么先自己写一个

> [!tip] 这是本卷最重要的方法
> 与其去猜别人的数据结构，不如**自己设计一个**，
> 然后把它加载到内存里，再用卷六的技术去读。
>
> 好处：
> 1. 你知道每个字段的准确偏移（因为是你的代码）
> 2. 你能验证读出来的对不对（知道真值）
> 3. 完全合法（读自己的进程）
> 4. 理解了"为什么引擎要这么设计"，再去读别人的就触类旁通

## 先看引擎要解决什么问题

一个游戏场景里有：玩家、敌人、箱子、门、子弹、灯光……

朴素的写法：一个大结构体塞所有东西：

```cpp
struct GameObject {
    Vec3 position;
    Rotator rotation;
    bool isCharacter;
    int hp;
    bool isWeapon;
    int ammo;
    bool isLight;
    float intensity;
    // ... 几十个字段，大部分用不上
};
```

**问题**：浪费内存、耦合严重、加新类型要改所有代码。

## Component 模式：组合优于继承

把功能拆成独立的"组件"，对象按需挂载：

```
Actor（容器，只有基础信息）
  ├─ TransformComponent    位置/旋转/缩放（几乎都有）
  ├─ MeshComponent         模型（可见的才有）
  ├─ CharacterComponent   血量/状态（角色才有）
  ├─ WeaponComponent       弹药/伤害（武器才有）
  └─ ...
```

```
玩家:  Transform + Mesh + Character + Weapon
箱子:  Transform + Mesh
子弹:  Transform + Mesh + Projectile
灯光:  Transform + Light
```

**这就是 UE4 的 Actor/Component 模型，Unity 的 GameObject/Component 也一样。**

## 我们的 demo 引擎设计

```cpp
// demo_engine.h
#pragma once
#include <cstdint>

struct Vec3 { float x, y, z; };
struct Quat { float x, y, z, w; };
struct Rotator { float Pitch, Yaw, Roll; };

// 组件类型
enum ComponentType : uint32_t {
    COMP_NONE       = 0,
    COMP_TRANSFORM  = 1,      // 变换
    COMP_MESH       = 2,      // 模型
    COMP_CHARACTER  = 3,      // 角色数据
    COMP_PROJECTILE = 4,      // 投掷物
};

// 组件基类：所有组件共享一个头部
struct ComponentHeader {
    uint32_t type;            // 组件类型
    uint32_t size;            // 整个组件大小
    uint64_t next;            // 下一个组件（链表）
};

struct TransformComponent {
    ComponentHeader header;
    Vec3     location;        // 世界坐标
    Quat     rotation;        // 旋转
    Vec3     scale;           // 缩放
};

struct MeshComponent {
    ComponentHeader header;
    uint64_t boneArray;       // 骨骼数组指针
    int32_t  boneCount;
    float    capsuleRadius;
    float    capsuleHalfHeight;
};

struct CharacterComponent {
    ComponentHeader header;
    int32_t  hp;
    int32_t  maxHp;
    uint32_t nameId;          // 名字 ID（第 85 章）
    uint32_t teamId;
    bool     isAI;
};

// 骨骼变换：一根骨骼的旋转 + 位置 + 缩放
struct BoneTransform {
    Quat rotation;            // 16 字节
    Vec3 translation;         // 12 字节
    Vec3 scale;               // 12 字节
};                            // 合计 40 字节

// ★ 关键：引擎按 48 字节步长存骨骼，不是 sizeof(BoneTransform)=40
// 多出的 8 字节是填充（对齐到 16，支持 SIMD），详见第 07、86 章
#define BONE_STRIDE 48
#define BONE_COUNT  56
```

## Actor 结构

```cpp
struct Actor {
    uint64_t vtable;          // 虚函数表（C++ 对象的第一个字段）
    uint32_t objectId;        // 唯一 ID
    uint32_t nameId;          // 名字 ID
    uint64_t components;      // 组件链表头指针
    uint64_t next;            // 下一个 Actor
};
```

**关键字段**：

| 偏移 | 字段 | 说明 |
|---|---|---|
| 0x00 | `vtable` | 虚函数表指针（C++ 对象特征） |
| 0x08 | `objectId` | 唯一标识 |
| 0x0C | `nameId` | 名字索引 |
| 0x10 | `components` | 组件链表 |
| 0x18 | `next` | 链表下一个 |

> [!note] 为什么第一个字段是 vtable
> C++ 中有虚函数的类，对象开头会有一个指向虚函数表的指针。
> 这是识别"C++ 对象"的重要特征，也是逆向时的线索。

## 世界结构

```cpp
struct Level {
    uint64_t actors;          // Actor 指针数组
    int32_t  actorCount;
    int32_t  actorCapacity;
};

struct World {
    uint64_t level;           // 当前关卡
    uint64_t playerController;
    uint64_t gameState;
};

// 全局单例（模拟引擎的静态指针）
extern World *g_world;
```

## 完整实现

```cpp
// demo_engine.cpp
#include "demo_engine.h"
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <vector>

World *g_world = nullptr;
static std::vector<Actor*> g_actorPool;

static uint32_t g_nextObjectId = 1;

// 创建组件
static TransformComponent* NewTransform(Vec3 loc) {
    auto *c = new TransformComponent();
    c->header.type = COMP_TRANSFORM;
    c->header.size = sizeof(TransformComponent);
    c->header.next = 0;
    c->location = loc;
    c->rotation = {0, 0, 0, 1};
    c->scale = {1, 1, 1};
    return c;
}

static void AttachComponent(Actor *a, ComponentHeader *c) {
    if (!a->components) {
        a->components = (uint64_t)c;
        return;
    }
    // 挂到链表尾部
    auto *cur = (ComponentHeader*)a->components;
    while (cur->next) cur = (ComponentHeader*)cur->next;
    cur->next = (uint64_t)c;
}

Actor* SpawnActor(const char *name, Vec3 loc, bool isCharacter) {
    auto *a = new Actor();
    a->vtable  = 0x1000;                    // 假的虚表
    a->objectId = g_nextObjectId++;
    a->nameId   = NameTable_Intern(name);   // 第 85 章
    a->components = 0;
    a->next = 0;

    AttachComponent(a, (ComponentHeader*)NewTransform(loc));

    if (isCharacter) {
        auto *m = new MeshComponent();
        m->header = {COMP_MESH, sizeof(MeshComponent), 0};
        // ★ 按 BONE_STRIDE(48) 分配，不是 sizeof(BoneTransform)(40)
        // 用 sizeof 会少分配 56*8=448 字节，读第 55 根骨骼时越界（第 86 章）
        m->boneArray = (uint64_t)malloc(BONE_COUNT * BONE_STRIDE);   // 56 × 48 = 2688 字节
        m->boneCount = BONE_COUNT;
        m->capsuleRadius = 34.0f;
        m->capsuleHalfHeight = 88.0f;
        AttachComponent(a, (ComponentHeader*)m);

        auto *ch = new CharacterComponent();
        ch->header = {COMP_CHARACTER, sizeof(CharacterComponent), 0};
        ch->hp = 100; ch->maxHp = 100;
        ch->nameId = a->nameId;
        ch->teamId = 1;
        ch->isAI = false;
        AttachComponent(a, (ComponentHeader*)ch);
    }

    g_actorPool.push_back(a);
    return a;
}

void BuildWorld() {
    g_world = new World();

    // Actor 数组（连续内存，模拟引擎的 TArray）
    auto *level = new Level();
    level->actorCount    = 0;
    level->actorCapacity = 256;
    level->actors = (uint64_t)malloc(level->actorCapacity * sizeof(uint64_t));

    g_world->level = (uint64_t)level;
    g_world->playerController = 0;
    g_world->gameState = 0;

    // 生成一些角色
    SpawnActor("BP_Character_Player", {0, 0, 0}, true);
    SpawnActor("BP_Character_Enemy",  {500, 300, 0}, true);
    SpawnActor("BP_Character_Enemy",  {-200, 800, 50}, true);
    SpawnActor("BP_Character_JiaRen", {1000, -400, 0}, true);   // "人机"
    SpawnActor("BP_Box_Static",       {200, 200, 0}, false);
    SpawnActor("BP_Box_Static",       {-500, 100, 0}, false);

    // 填充 Actor 数组
    level->actorCount = (int32_t)g_actorPool.size();
    for (size_t i = 0; i < g_actorPool.size(); i++)
        ((uint64_t*)level->actors)[i] = (uint64_t)g_actorPool[i];
}

void PrintWorld() {
    printf("g_world = %p\n", (void*)g_world);
    if (!g_world) return;
    auto *level = (Level*)g_world->level;
    printf("level = %p\n", (void*)level);
    printf("actors = %p  count = %d\n", (void*)level->actors, level->actorCount);
}
```

主程序：

```cpp
// main.cpp
#include "demo_engine.h"
#include <unistd.h>
#include <cstdio>

int main(void) {
    BuildWorld();
    PrintWorld();

    printf("\npid = %d\n", getpid());
    printf("运行中，用 memview / reader 来读我...\n");
    fflush(stdout);

    // 让数据动起来，验证"每帧变化"
    int tick = 0;
    while (true) {
        auto *level = (Level*)g_world->level;
        for (int i = 0; i < level->actorCount; i++) {
            auto *a = (Actor*)((uint64_t*)level->actors)[i];
            auto *t = FindComponent<TransformComponent>(a, COMP_TRANSFORM);
            if (t) {
                t->location.x += (tick % 2) ? 1.0f : -1.0f;   // 左右移动
            }
        }
        tick++;
        usleep(100000);      // 10 Hz
    }
    return 0;
}
```

## 查找组件

```cpp
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
```

**在外部程序里怎么做**：沿着 `components` 链表走，
每一步读 `type` 和 `next`——这就是组件遍历的通用方法。

## 为什么用链表而不是数组

| | 链表 | 数组 |
|---|---|---|
| 增删 | O(1) | O(n) |
| 查找 | O(n) | O(n)（但 cache 友好） |
| 内存 | 分散 | 连续 |

UE4 实际用的是 TArray（数组）+ 内联存储，
但链表更直观，适合教学。**原理相同：从 Actor 找到它的组件。**

## 动手：跑起来并读它

1. 编译运行 demo 引擎，记下 `pid` 和 `g_world` 地址
2. 用第 76 章的 `memview` 看它的内存布局
3. 用第 75 章的 `MemReader`：
   - 读 `g_world` → 得到 `World`
   - 读 `World.level` → 得到 `Level`
   - 读 `Level.actors` 和 `actorCount`
   - 遍历数组，读出每个 Actor
   - 对每个 Actor 遍历组件链表

**读出第一个 Actor 的名字和坐标，和 demo 打印的对比**——
这就是"跨进程解析数据结构"的完整闭环。

## 动手验证清单

- [ ] **建 demo 引擎**：定义 `World` / `Level` / `Actor` / `Component` 结构体（带偏移注释）
- [ ] **分配对象数组**：`Actor** actors = malloc(N * sizeof(Actor*))`
- [ ] **挂组件链表**：每个 Actor 用 `next` 指针串起多个组件
- [ ] **打印结构**：遍历 World → Level → Actor → 组件 → 打印每个对象的地址和字段
- [ ] **画结构图**：在纸上画出四层结构，标出每层的指针偏移
- [ ] **跑通 demo**：`./demo` → 输出对象信息

> [!tip] 为什么要造 demo
> 直接读真实游戏数据前，先在自己造的"可控内存"上跑通遍历逻辑，
> 调试时你知道每个字段应该是什么值——这是卷七的核心学习方法。

## 验收清单

- [ ] 理解 Actor/Component 为什么比"大结构体"好（说出"复用、解耦、按需组合"）
- [ ] 知道组件是通过指针/链表挂在 Actor 上的（说出查找组件的方式）
- [ ] 知道 C++ 对象第一个字段通常是 vtable（说出虚函数的实现）
- [ ] 写出了自己的 demo 引擎并跑起来 ★（编译 demo，运行输出对象信息）
- [ ] 能画出 World → Level → Actor 数组 → Actor → 组件链表 的结构图（画在纸上并标注每层指针偏移）
- [ ] 用 memview 观察过它的内存布局（用 memview 查看 demo 的 World 指针和对象）

→ 下一章：[[第84章-对象数组与遍历]]　—— 掌握从"数组首地址 + 个数"遍历所有对象的完整流程，以及每一步必须做的防御。
