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

enum class AttackPattern {
    SINGLE,
    TRIPLE,
    RADIAL,
    SPIRAL
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
    AttackPattern attackPattern = AttackPattern::SINGLE;
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
    float poisonTimer = 0.0f;
    float poisonTickTimer = 0.0f;
    int poisonDamage = 0;
    float stickyTimer = 0.0f;
    float stickySpeedMultiplier = 1.0f;
    float spiralOffset = 0.0f;

    bool shielded = false;

    bool hasHomingShots = false;
    bool bouncesOffWalls = false;
    bool explodesOnTimer = false;
    bool splitsOnDeath = false;
    bool isSplitChild = false;   // guards against split-children re-splitting

    float fuseTimer = 0.0f;
    float fuseDuration = 1.2f;

    bool isCharging = false;      // used only when bouncesOffWalls is true
    Vec2 chargeDir = { 0.0f, 0.0f };
    float chargeTimeRemaining = 0.0f;

    EnemySpecialType specialType = EnemySpecialType::NONE;

    Rect GetRect() const { return { pos.x, pos.y, w, h }; }
};

namespace EnemyAI {
    void Update(Enemy& enemy, Vec2 playerPos, float dt, const std::vector<Enemy>& roomEnemies,
                std::vector<Enemy>& spawnedEnemies, std::vector<Projectile>& enemyProjectiles);
}