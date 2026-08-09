#include "boss.h"
#include <cmath>

namespace {
    constexpr float PI = 3.14159265f;

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
            Projectile p;
            p.pos = boss.pos;
            p.vel = { std::cos(angle) * speed, std::sin(angle) * speed };
            out.push_back(p);
        }
    }

    void FireRadialBurst(Boss& boss, std::vector<Projectile>& out) {
        const int count = 12;
        const float speed = 70.0f;
        for (int i = 0; i < count; ++i) {
            float angle = (2.0f * PI) * ((float)i / count);
            Projectile p;
            p.pos = boss.pos;
            p.vel = { std::cos(angle) * speed, std::sin(angle) * speed };
            out.push_back(p);
        }
    }

    void StartCharge(Boss& boss, Vec2 playerPos) {
        boss.isCharging = true;
        boss.chargeDir = Normalize({ playerPos.x - boss.pos.x, playerPos.y - boss.pos.y });
        boss.chargeTimeRemaining = boss.chargeDuration;
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

} // namespace BossAI