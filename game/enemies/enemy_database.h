#pragma once
#include "enemy.h"

constexpr int MAX_ENEMY_TEMPLATES = 32;

struct EnemyTemplate {
    char name[32] = {};
    AIType aiType = AIType::CHASER;
    AttackPattern attackPattern = AttackPattern::SINGLE;
    bool shielded = false;
    int hp = 30;
    float speed = 40.0f;
    float w = 12.0f, h = 12.0f;
    float shootCooldown = 1.5f;
    float shootRange = 110.0f;
    float preferredDistance = 70.0f;
    float shotSpeed = 80.0f;
    bool hasHomingShots = false;
    bool bouncesOffWalls = false;
    bool explodesOnTimer = false;
    bool splitsOnDeath = false;
    float fuseDuration = 1.2f;
    int tier = 1;
};

namespace EnemyDatabase {
    bool Load(const char* path);
    int Count();
    bool Exists(const char* name);
    Enemy Spawn(const char* name, Vec2 pos);
    Enemy Spawn(int index, Vec2 pos);
    int Tier(int index);
}