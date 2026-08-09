#pragma once
#include "enemy.h"

constexpr int MAX_ENEMY_TEMPLATES = 32;

struct EnemyTemplate {
    char name[32] = {};
    AIType aiType = AIType::CHASER;
    int hp = 30;
    float speed = 40.0f;
    float w = 12.0f, h = 12.0f;
    float shootCooldown = 1.5f;
    float shootRange = 110.0f;
    float preferredDistance = 70.0f;
    float shotSpeed = 80.0f;
};

namespace EnemyDatabase {
    bool Load(const char* path);
    bool Exists(const char* name);
    Enemy Spawn(const char* name, Vec2 pos);
}