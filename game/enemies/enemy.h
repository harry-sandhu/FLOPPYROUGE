#pragma once
#include "../../engine/core/types.h"
#include "../player/projectile.h"
#include <vector>

enum class AIType {
    CHASER,
    SHOOTER,
    CHARGER,
    SUMMONER,
    EXPLODER
};

enum class EnemySpecialType {
    NONE,
    REINFORCER,
    CREEPER,
    DEATH_RING
};

struct Enemy {
    Vec2 pos;
    float w = 12.0f, h = 12.0f;
    float speed = 40.0f;
    int hp = 30;
    int maxHp = 30;
    AIType aiType = AIType::CHASER;
    bool alive = true;
    char templateName[32] = {};

    float shootTimer = 0.0f;
    float shootCooldown = 1.5f;
    float shootRange = 110.0f;
    float preferredDistance = 70.0f;
    float shotSpeed = 80.0f;
    float spawnDelayRemaining = 0.0f;
    float creepDropTimer = 0.0f;
    float creepDropInterval = 0.30f;

    EnemySpecialType specialType = EnemySpecialType::NONE;

    Rect GetRect() const { return { pos.x, pos.y, w, h }; }
};

namespace EnemyAI {
    void Update(Enemy& enemy, Vec2 playerPos, float dt, const std::vector<Enemy>& roomEnemies,
                std::vector<Enemy>& spawnedEnemies, std::vector<Projectile>& enemyProjectiles);
}
