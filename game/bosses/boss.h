#pragma once
#include "../../engine/core/types.h"
#include "../player/projectile.h"
#include <vector>

enum class BossAttackType {
    SPREAD_SHOT,
    RADIAL_BURST,
    CHARGE
};

struct Boss {
    Vec2 pos;
    float w = 28.0f, h = 28.0f;
    int hp = 300;
    int maxHp = 300;
    int phase = 1;
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

    int contactDamage = 15;
    int chargeContactDamage = 25;

    Rect GetRect() const { return { pos.x, pos.y, w, h }; }
};

namespace BossAI {
    void Update(Boss& boss, Vec2 playerPos, float dt, std::vector<Projectile>& bossProjectiles);
}