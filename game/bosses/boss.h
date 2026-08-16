#pragma once
#include "../../engine/core/types.h"
#include "../player/projectile.h"
#include "../enemies/enemy.h"
#include <vector>

enum class BossAttackType {
    SPREAD_SHOT,       // 0
    RADIAL_BURST,      // 1
    CHARGE,            // 2
    LASER_SWEEP,       // 3
    SUMMON_WAVE,       // 4
    FLOOR_HAZARD,      // 5
    MIRROR_SHOT,       // 6
    CARDINAL_BURST,    // 7
    SPIRAL_BURST,      // 8
    TRIPLE_SPREAD,     // 9
    DENSE_RING,        // 10
    GAPPED_RING        // 11
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
    // See Enemy::attackDelayRemaining - same two-stage spawn model applies
    // to bosses: inert during spawnDelayRemaining, movable-but-harmless
    // until attackDelayRemaining also expires.
    float attackDelayRemaining = 0.0f;
    float poisonTimer = 0.0f;
    float poisonTickTimer = 0.0f;
    int poisonDamage = 0;
    float stickyTimer = 0.0f;
    float stickySpeedMultiplier = 1.0f;
    float burnTimer = 0.0f;
    float burnTickTimer = 0.0f;
    int burnDamage = 0;
    float freezeTimer = 0.0f;
    int markStacks = 0;

    // Damage in half-heart units (player has 6 hp = 3 hearts). Hard cap
    // across the whole game: every damage source is either 1 (half heart)
    // or 2 (a full heart), never more - untelegraphed contact defaults to
    // half a heart, a charge/slam (dodgeable, telegraphed) to a full heart.
    int contactDamage = 1;
    int chargeContactDamage = 2;
    int maxAdds = 4;
    std::vector<BossHazard> hazards;

    Rect GetRect() const { return { pos.x, pos.y, w, h }; }
    bool IsFrozen() const { return freezeTimer > 0.0f; }
    static constexpr float FREEZE_RESIST = 0.2f; // bosses only take 20% of applied freeze duration
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