#pragma once
#include "boss.h"

constexpr int MAX_BOSS_TEMPLATES = 32;
constexpr int MAX_ATTACK_CYCLE_LENGTH = 16;

struct BossTemplate {
    char name[32] = {};
    int hp = 300;
    float driftSpeed = 15.0f;
    float attackCooldownPhase1 = 2.5f;
    float attackCooldownPhase2 = 1.5f;
    float chargeSpeed = 220.0f;
    // Set to 0 to disable phase 2 entirely.
    float phase2HpRatio = 0.5f;
    int contactDamage = 1;
    int chargeContactDamage = 2;
    int maxAdds = 4;
    
    // Attack cycle (phase 1 and phase 2 can have different cycles)
    BossAttackType attackCyclePhase1[MAX_ATTACK_CYCLE_LENGTH];
    int attackCyclePhase1Length = 0;
    BossAttackType attackCyclePhase2[MAX_ATTACK_CYCLE_LENGTH];
    int attackCyclePhase2Length = 0;
};

namespace BossDatabase {
    bool Load(const char* path);
    const BossTemplate* Get(int index);
    int Count();
}
