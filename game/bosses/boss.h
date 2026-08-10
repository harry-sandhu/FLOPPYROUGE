#pragma once
#include "../../engine/core/types.h"
#include "../player/projectile.h"
#include "../enemies/enemy.h"
#include <vector>

enum class BossAttackType {
    SPREAD_SHOT,
    RADIAL_BURST,
    CHARGE,
    LASER_SWEEP,
    SUMMON_WAVE,
    FLOOR_HAZARD,
    MIRROR_SHOT
};



// A persistent damage zone dropped by FLOOR_HAZARD. `timeRemaining` is
// owned/decremented by BossAI::Update (expiry); `tickTimer` is owned by
// whoever checks player overlap each frame (main.cpp) and decrements it
// while the player stands inside `radius`.
struct BossHazard {
    Vec2 pos = { 0.0f, 0.0f };
    float radius = 16.0f;
    float timeRemaining = 3.0f;
    float tickTimer = 0.0f;
    int tickDamage = 1;
};

struct Boss {
    Vec2 pos;
    float w = 28.0f, h = 28.0f;
    int hp = 300;
    int maxHp = 300;
    int phase = 1;
    int variant = 0;
    bool alive = true;

    float driftSpeed = 15.0f;

    float attackTimer = 2.0f;
    float attackCooldownPhase1 = 2.5f;
    float attackCooldownPhase2 = 1.5f;
    int attackIndex = 0;

    bool isCharging = false;
    Vec2 chargeDir = { 0.0f, 0.0f };
    float chargeTimeRemaining = 0.0f;
    float chargeDuration = 0.5f;
    float chargeSpeed = 220.0f;
    float spawnDelayRemaining = 0.0f;
    float poisonTimer = 0.0f;
    float poisonTickTimer = 0.0f;
    int poisonDamage = 0;
    float stickyTimer = 0.0f;
    float stickySpeedMultiplier = 1.0f;

    // Damage in half-heart units (player has 6 hp = 3 hearts). Untelegraphed
    // contact is 1 heart; a charge/slam (dodgeable) is 1.5 hearts.
    int contactDamage = 2;
    int chargeContactDamage = 3;
    int maxAdds = 4;
    std::vector<BossHazard> hazards;

    Rect GetRect() const { return { pos.x, pos.y, w, h }; }
};

namespace BossAI {
    // roomAdds: currently-alive adds this boss has summoned (for the cap).
    // spawnedAdds: newly-summoned adds this frame, appended by the caller.
    void Update(Boss& boss, Vec2 playerPos, float dt, std::vector<Projectile>& bossProjectiles,
                const std::vector<Enemy>& roomAdds, std::vector<Enemy>& spawnedAdds);
}

Boss SpawnBoss1();
Boss SpawnBoss2();
Boss SpawnBoss3();
Boss SpawnBoss4();
Boss SpawnBoss5();
Boss SpawnBoss6();
Boss SpawnBoss7();
Boss SpawnBoss8();
Boss SpawnBoss9();
Boss SpawnBoss10();
Boss SpawnBossVariant(int variant);