#include "boss.h"
#include "../player/projectile_system.h"
#include <cmath>

namespace {
    constexpr float PI = 3.14159265f;
    constexpr int BOSS_PROJECTILE_DAMAGE = 8;
    constexpr float BOSS_PROJECTILE_RANGE = 999999.0f;

    Vec2 Normalize(Vec2 v) {
        float len = std::sqrt(v.x * v.x + v.y * v.y);
        if (len > 0.01f) { v.x /= len; v.y /= len; }
        return v;
    }

    void FireSpreadShot(Boss& boss, Vec2 playerPos, std::vector<Projectile>& out) {
        Vec2 dir = Normalize({ playerPos.x - boss.pos.x, playerPos.y - boss.pos.y });
        float baseAngle = std::atan2(dir.y, dir.x);
        const int count = 5;
        const float spread = 0.5f;
        const float speed = 90.0f;

        for (int i = 0; i < count; ++i) {
            float t = (count == 1) ? 0.0f : (float)i / (count - 1) - 0.5f;
            float angle = baseAngle + t * spread;
            ProjectileSystem::Spawn(
                out,
                boss.pos,
                { std::cos(angle) * speed, std::sin(angle) * speed },
                BOSS_PROJECTILE_DAMAGE,
                BOSS_PROJECTILE_RANGE
            );
        }
    }

    void FireRadialBurst(Boss& boss, std::vector<Projectile>& out) {
        const int count = 12;
        const float speed = 70.0f;
        for (int i = 0; i < count; ++i) {
            float angle = (2.0f * PI) * ((float)i / count);
            ProjectileSystem::Spawn(
                out,
                boss.pos,
                { std::cos(angle) * speed, std::sin(angle) * speed },
                BOSS_PROJECTILE_DAMAGE,
                BOSS_PROJECTILE_RANGE
            );
        }
    }

    void FireCardinalBurst(Boss& boss, std::vector<Projectile>& out) {
        const float speed = 100.0f;
        const Vec2 dirs[] = {
            { 1.0f, 0.0f },
            { -1.0f, 0.0f },
            { 0.0f, 1.0f },
            { 0.0f, -1.0f }
        };

        for (const Vec2& dir : dirs) {
            ProjectileSystem::Spawn(
                out,
                boss.pos,
                { dir.x * speed, dir.y * speed },
                BOSS_PROJECTILE_DAMAGE,
                BOSS_PROJECTILE_RANGE
            );
        }
    }

    void FireSpiralBurst(Boss& boss, std::vector<Projectile>& out) {
        const int count = 10;
        const float speed = 85.0f;
        float offset = boss.attackIndex * 0.35f;
        for (int i = 0; i < count; ++i) {
            float angle = (2.0f * PI) * ((float)i / count) + offset;
            ProjectileSystem::Spawn(
                out,
                boss.pos,
                { std::cos(angle) * speed, std::sin(angle) * speed },
                BOSS_PROJECTILE_DAMAGE,
                BOSS_PROJECTILE_RANGE
            );
        }
    }

    void FireTripleSpread(Boss& boss, Vec2 playerPos, std::vector<Projectile>& out) {
        Vec2 dir = Normalize({ playerPos.x - boss.pos.x, playerPos.y - boss.pos.y });
        float baseAngle = std::atan2(dir.y, dir.x);
        const float speed = 110.0f;
        const float spread = 0.22f;

        for (int i = -1; i <= 1; ++i) {
            float angle = baseAngle + spread * (float)i;
            ProjectileSystem::Spawn(
                out,
                boss.pos,
                { std::cos(angle) * speed, std::sin(angle) * speed },
                BOSS_PROJECTILE_DAMAGE,
                BOSS_PROJECTILE_RANGE
            );
        }
    }

    void StartCharge(Boss& boss, Vec2 playerPos) {
        boss.isCharging = true;
        boss.chargeDir = Normalize({ playerPos.x - boss.pos.x, playerPos.y - boss.pos.y });
        boss.chargeTimeRemaining = boss.chargeDuration;
    }

    void UpdateVariant0(Boss& boss, Vec2 playerPos, float dt, std::vector<Projectile>& bossProjectiles) {
        boss.attackTimer -= dt;
        if (boss.attackTimer <= 0.0f) {
            BossAttackType attack = (BossAttackType)(boss.attackIndex % 3);
            boss.attackIndex++;

            switch (attack) {
                case BossAttackType::SPREAD_SHOT:  FireSpreadShot(boss, playerPos, bossProjectiles); break;
                case BossAttackType::RADIAL_BURST: FireRadialBurst(boss, bossProjectiles); break;
                case BossAttackType::CHARGE:       StartCharge(boss, playerPos); break;
            }

            boss.attackTimer = (boss.phase == 1) ? boss.attackCooldownPhase1 : boss.attackCooldownPhase2;
        }
    }

    void UpdateVariant1(Boss& boss, Vec2 playerPos, float dt, std::vector<Projectile>& bossProjectiles) {
        boss.attackTimer -= dt;
        if (boss.attackTimer <= 0.0f) {
            int step = boss.attackIndex % 3;
            boss.attackIndex++;

            switch (step) {
                case 0: FireCardinalBurst(boss, bossProjectiles); break;
                case 1: FireTripleSpread(boss, playerPos, bossProjectiles); break;
                case 2: StartCharge(boss, playerPos); break;
            }

            boss.attackTimer = (boss.phase == 1) ? boss.attackCooldownPhase1 : boss.attackCooldownPhase2;
        }
    }

    void UpdateVariant2(Boss& boss, Vec2 playerPos, float dt, std::vector<Projectile>& bossProjectiles) {
        boss.attackTimer -= dt;
        if (boss.attackTimer <= 0.0f) {
            int step = boss.attackIndex % 3;
            boss.attackIndex++;

            switch (step) {
                case 0: FireSpiralBurst(boss, bossProjectiles); break;
                case 1: FireRadialBurst(boss, bossProjectiles); break;
                case 2: FireTripleSpread(boss, playerPos, bossProjectiles); break;
            }

            boss.attackTimer = (boss.phase == 1) ? boss.attackCooldownPhase1 : boss.attackCooldownPhase2;
        }
    }

    Boss MakeBoss(int variant) {
        Boss boss;
        boss.variant = variant;
        boss.pos = { 146.0f, 20.0f };

        switch (variant) {
            case 1:
                boss.hp = boss.maxHp = 260;
                boss.driftSpeed = 18.0f;
                boss.attackCooldownPhase1 = 2.0f;
                boss.attackCooldownPhase2 = 1.2f;
                boss.chargeSpeed = 240.0f;
                break;
            case 2:
                boss.hp = boss.maxHp = 360;
                boss.driftSpeed = 12.0f;
                boss.attackCooldownPhase1 = 1.8f;
                boss.attackCooldownPhase2 = 1.0f;
                boss.chargeSpeed = 260.0f;
                break;
            case 0:
            default:
                boss.hp = boss.maxHp = 300;
                boss.driftSpeed = 15.0f;
                boss.attackCooldownPhase1 = 2.5f;
                boss.attackCooldownPhase2 = 1.5f;
                boss.chargeSpeed = 220.0f;
                break;
        }

        boss.attackTimer = boss.attackCooldownPhase1;
        return boss;
    }
}

namespace BossAI {

void Update(Boss& boss, Vec2 playerPos, float dt, std::vector<Projectile>& bossProjectiles) {
    if (!boss.alive) return;

    if (boss.phase == 1 && boss.hp <= boss.maxHp / 2) {
        boss.phase = 2;
    }

    if (boss.isCharging) {
        boss.pos.x += boss.chargeDir.x * boss.chargeSpeed * dt;
        boss.pos.y += boss.chargeDir.y * boss.chargeSpeed * dt;
        boss.chargeTimeRemaining -= dt;
        if (boss.chargeTimeRemaining <= 0.0f) boss.isCharging = false;
        return;
    }

    Vec2 dir = Normalize({ playerPos.x - boss.pos.x, playerPos.y - boss.pos.y });
    boss.pos.x += dir.x * boss.driftSpeed * dt;
    boss.pos.y += dir.y * boss.driftSpeed * dt;

    switch (boss.variant) {
        case 1: UpdateVariant1(boss, playerPos, dt, bossProjectiles); break;
        case 2: UpdateVariant2(boss, playerPos, dt, bossProjectiles); break;
        case 0:
        default:
            UpdateVariant0(boss, playerPos, dt, bossProjectiles);
            break;
    }
}

} // namespace BossAI

Boss SpawnBoss1() {
    return MakeBoss(0);
}

Boss SpawnBoss2() {
    return MakeBoss(1);
}

Boss SpawnBoss3() {
    return MakeBoss(2);
}

Boss SpawnBossVariant(int variant) {
    switch (variant % 3) {
        case 1: return SpawnBoss2();
        case 2: return SpawnBoss3();
        case 0:
        default:
            return SpawnBoss1();
    }
}
