#include "boss.h"
#include "../player/projectile_system.h"
#include "../enemies/enemy_database.h"
#include <cmath>

namespace {
    constexpr float PI = 3.14159265f;
    constexpr int BOSS_PROJECTILE_DAMAGE = 2;
    constexpr float BOSS_PROJECTILE_RANGE = 999999.0f;
    constexpr int MAX_BOSS_HAZARDS = 3;

    Vec2 Normalize(Vec2 v) {
        float len = std::sqrt(v.x * v.x + v.y * v.y);
        if (len > 0.01f) { v.x /= len; v.y /= len; }
        return v;
    }

    void FireSpreadShot(Boss& boss, Vec2 playerPos, std::vector<Projectile>& out) {
        Vec2 dir = Normalize({ playerPos.x - boss.pos.x, playerPos.y - boss.pos.y });
        float baseAngle = std::atan2(dir.y, dir.x);
        const int count = 3;
        const float spread = 0.4f;
        const float speed = 86.0f;

        for (int i = 0; i < count; ++i) {
            float t = (count == 1) ? 0.0f : (float)i / (count - 1) - 0.5f;
            float angle = baseAngle + t * spread;
            ProjectileSystem::Spawn(
                out, boss.pos,
                { std::cos(angle) * speed, std::sin(angle) * speed },
                BOSS_PROJECTILE_DAMAGE, BOSS_PROJECTILE_RANGE
            );
        }
    }

    void FireRadialBurst(Boss& boss, std::vector<Projectile>& out) {
        const int count = 8;
        const float speed = 66.0f;
        for (int i = 0; i < count; ++i) {
            float angle = (2.0f * PI) * ((float)i / count);
            ProjectileSystem::Spawn(
                out, boss.pos,
                { std::cos(angle) * speed, std::sin(angle) * speed },
                BOSS_PROJECTILE_DAMAGE, BOSS_PROJECTILE_RANGE
            );
        }
    }

    void FireCardinalBurst(Boss& boss, std::vector<Projectile>& out) {
        const float speed = 100.0f;
        const Vec2 dirs[] = {
            { 1.0f, 0.0f }, { -1.0f, 0.0f }, { 0.0f, 1.0f }, { 0.0f, -1.0f }
        };
        for (const Vec2& dir : dirs) {
            ProjectileSystem::Spawn(
                out, boss.pos,
                { dir.x * speed, dir.y * speed },
                BOSS_PROJECTILE_DAMAGE, BOSS_PROJECTILE_RANGE
            );
        }
    }

    void FireSpiralBurst(Boss& boss, std::vector<Projectile>& out) {
        const int count = 8;
        const float speed = 80.0f;
        float offset = boss.attackIndex * 0.32f;
        for (int i = 0; i < count; ++i) {
            float angle = (2.0f * PI) * ((float)i / count) + offset;
            ProjectileSystem::Spawn(
                out, boss.pos,
                { std::cos(angle) * speed, std::sin(angle) * speed },
                BOSS_PROJECTILE_DAMAGE, BOSS_PROJECTILE_RANGE
            );
        }
    }

    void FireTripleSpread(Boss& boss, Vec2 playerPos, std::vector<Projectile>& out) {
        Vec2 dir = Normalize({ playerPos.x - boss.pos.x, playerPos.y - boss.pos.y });
        float baseAngle = std::atan2(dir.y, dir.x);
        const float speed = 104.0f;
        const float spread = 0.18f;

        for (int i = -1; i <= 1; ++i) {
            float angle = baseAngle + spread * (float)i;
            ProjectileSystem::Spawn(
                out, boss.pos,
                { std::cos(angle) * speed, std::sin(angle) * speed },
                BOSS_PROJECTILE_DAMAGE, BOSS_PROJECTILE_RANGE
            );
        }
    }

    void FireDenseRing(Boss& boss, std::vector<Projectile>& out) {
        const int count = 14;
        const float speed = 60.0f;
        for (int i = 0; i < count; ++i) {
            float angle = (2.0f * PI) * ((float)i / count);
            ProjectileSystem::Spawn(
                out, boss.pos,
                { std::cos(angle) * speed, std::sin(angle) * speed },
                BOSS_PROJECTILE_DAMAGE, BOSS_PROJECTILE_RANGE
            );
        }
    }


    // LASER_SWEEP: a narrow, fast fan aimed at the player. The fan's
    // center angle shifts with attackIndex, so consecutive picks in a
    // boss's rotation visually sweep across the room. There's no
    // continuous line-collision system in this engine, so this
    // approximates a beam as a tight burst rather than a true
    // persistent line hazard - flagging that honestly rather than
    // overselling it.
    void FireLaserSweep(Boss& boss, Vec2 playerPos, std::vector<Projectile>& out) {
        Vec2 dir = Normalize({ playerPos.x - boss.pos.x, playerPos.y - boss.pos.y });
        float baseAngle = std::atan2(dir.y, dir.x) + (float)(boss.attackIndex % 5) * 0.22f - 0.44f;
        const int count = 5;
        const float beamWidth = 0.05f;
        const float speed = 150.0f;
        for (int i = 0; i < count; ++i) {
            float angle = baseAngle + beamWidth * (float)i;
            ProjectileSystem::Spawn(
                out, boss.pos,
                { std::cos(angle) * speed, std::sin(angle) * speed },
                BOSS_PROJECTILE_DAMAGE, BOSS_PROJECTILE_RANGE
            );
        }
    }

    // MIRROR_SHOT: a full ring with a deliberate gap placed opposite the
    // player's current angle, so dodging means moving to a specific
    // spot rather than just "away from the nearest bullet."
    void FireGappedRing(Boss& boss, Vec2 playerPos, std::vector<Projectile>& out) {
        Vec2 dir = Normalize({ playerPos.x - boss.pos.x, playerPos.y - boss.pos.y });
        float playerAngle = std::atan2(dir.y, dir.x);
        const int count = 12;
        const float speed = 70.0f;
        const int gapStart = 6;
        const int gapSize = 2;
        for (int i = 0; i < count; ++i) {
            int rel = (i - gapStart + count) % count;
            if (rel < gapSize) continue; // safe lane
            float angle = playerAngle + (2.0f * PI) * ((float)i / count);
            ProjectileSystem::Spawn(
                out, boss.pos,
                { std::cos(angle) * speed, std::sin(angle) * speed },
                BOSS_PROJECTILE_DAMAGE, BOSS_PROJECTILE_RANGE
            );
        }
    }

    // FLOOR_HAZARD: drops a persistent damage zone at the player's
    // current position (telegraphed - get off the spot you're
    // standing on). Capped so a boss can't blanket the room.
    void DropFloorHazard(Boss& boss, Vec2 playerPos) {
        if ((int)boss.hazards.size() >= MAX_BOSS_HAZARDS) return;
        BossHazard hazard;
        hazard.pos = playerPos;
        hazard.radius = 16.0f;
        hazard.timeRemaining = 3.0f;
        hazard.tickTimer = 0.0f;
        hazard.tickDamage = 1;
        boss.hazards.push_back(hazard);
    }

    // Duke-of-Flies-style summon: spawns small adds around the boss, capped
    // by boss.maxAdds so the room can't get flooded.
    void SummonAdds(Boss& boss, const std::vector<Enemy>& roomAdds, std::vector<Enemy>& spawnedAdds) {
        if ((int)roomAdds.size() + (int)spawnedAdds.size() >= boss.maxAdds) return;

        const int count = 2;
        for (int i = 0; i < count; ++i) {
            float angle = (2.0f * PI) * ((float)i / count) + (float)boss.attackIndex * 0.6f;
            Vec2 spawnPos = {
                boss.pos.x + std::cos(angle) * 24.0f,
                boss.pos.y + std::sin(angle) * 24.0f
            };
            spawnedAdds.push_back(EnemyDatabase::Spawn("Fly", spawnPos));
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

    void UpdateVariant3(Boss& boss, Vec2 playerPos, float dt, std::vector<Projectile>& bossProjectiles) {
        boss.attackTimer -= dt;
        if (boss.attackTimer <= 0.0f) {
            int step = boss.attackIndex % 4;
            boss.attackIndex++;
            switch (step) {
                case 0: FireSpiralBurst(boss, bossProjectiles); break;
                case 1: FireCardinalBurst(boss, bossProjectiles); break;
                case 2: FireSpiralBurst(boss, bossProjectiles); break;
                case 3: StartCharge(boss, playerPos); break;
            }
            boss.attackTimer = (boss.phase == 1) ? boss.attackCooldownPhase1 : boss.attackCooldownPhase2;
        }
    }

    void UpdateVariant4(Boss& boss, Vec2 playerPos, float dt, std::vector<Projectile>& bossProjectiles) {
        boss.attackTimer -= dt;
        if (boss.attackTimer <= 0.0f) {
            int step = boss.attackIndex % 5;
            boss.attackIndex++;
            switch (step) {
                case 0: FireTripleSpread(boss, playerPos, bossProjectiles); break;
                case 1: FireDenseRing(boss, bossProjectiles); break;
                case 2: StartCharge(boss, playerPos); break;
                case 3: FireSpiralBurst(boss, bossProjectiles); break;
                case 4: FireCardinalBurst(boss, bossProjectiles); break;
            }
            boss.attackTimer = (boss.phase == 1) ? boss.attackCooldownPhase1 : boss.attackCooldownPhase2;
        }
    }

    // Duke-of-Flies style: mostly passive/drifting, leans on adds instead of
    // dense bullet patterns. Phase 2 adds a spread shot into the rotation.
    void UpdateVariant5(Boss& boss, Vec2 playerPos, float dt, std::vector<Projectile>& bossProjectiles,
                        const std::vector<Enemy>& roomAdds, std::vector<Enemy>& spawnedAdds) {
        boss.attackTimer -= dt;
        if (boss.attackTimer <= 0.0f) {
            int stepCount = (boss.phase == 1) ? 2 : 3;
            int step = boss.attackIndex % stepCount;
            boss.attackIndex++;
            switch (step) {
                case 0: SummonAdds(boss, roomAdds, spawnedAdds); break;
                case 1: FireRadialBurst(boss, bossProjectiles); break;
                case 2: FireSpreadShot(boss, playerPos, bossProjectiles); break;
            }
            boss.attackTimer = (boss.phase == 1) ? boss.attackCooldownPhase1 : boss.attackCooldownPhase2;
        }
    }

    void UpdateVariant6(Boss& boss, Vec2 playerPos, float dt, std::vector<Projectile>& bossProjectiles) {
        boss.attackTimer -= dt;
        if (boss.attackTimer <= 0.0f) {
            int step = boss.attackIndex % 3;
            boss.attackIndex++;
            switch (step) {
                case 0: DropFloorHazard(boss, playerPos); break;
                case 1: StartCharge(boss, playerPos); break;
                case 2: FireRadialBurst(boss, bossProjectiles); break;
            }
            boss.attackTimer = (boss.phase == 1) ? boss.attackCooldownPhase1 : boss.attackCooldownPhase2;
        }
    }

    void UpdateVariant7(Boss& boss, Vec2 playerPos, float dt, std::vector<Projectile>& bossProjectiles,
                        const std::vector<Enemy>& roomAdds, std::vector<Enemy>& spawnedAdds) {
        boss.attackTimer -= dt;
        if (boss.attackTimer <= 0.0f) {
            int step = boss.attackIndex % 3;
            boss.attackIndex++;
            switch (step) {
                case 0: FireLaserSweep(boss, playerPos, bossProjectiles); break;
                case 1: SummonAdds(boss, roomAdds, spawnedAdds); break;
                case 2: FireLaserSweep(boss, playerPos, bossProjectiles); break;
            }
            boss.attackTimer = (boss.phase == 1) ? boss.attackCooldownPhase1 : boss.attackCooldownPhase2;
        }
    }

    void UpdateVariant8(Boss& boss, Vec2 playerPos, float dt, std::vector<Projectile>& bossProjectiles) {
        boss.attackTimer -= dt;
        if (boss.attackTimer <= 0.0f) {
            int step = boss.attackIndex % 2;
            boss.attackIndex++;
            switch (step) {
                case 0: FireGappedRing(boss, playerPos, bossProjectiles); break;
                case 1: FireSpreadShot(boss, playerPos, bossProjectiles); break;
            }
            boss.attackTimer = (boss.phase == 1) ? boss.attackCooldownPhase1 : boss.attackCooldownPhase2;
        }
    }

    void UpdateVariant9(Boss& boss, Vec2 playerPos, float dt, std::vector<Projectile>& bossProjectiles,
                        const std::vector<Enemy>& roomAdds, std::vector<Enemy>& spawnedAdds) {
        boss.attackTimer -= dt;
        if (boss.attackTimer <= 0.0f) {
            int step = boss.attackIndex % 6;
            boss.attackIndex++;
            switch (step) {
                case 0: FireGappedRing(boss, playerPos, bossProjectiles); break;
                case 1: DropFloorHazard(boss, playerPos); break;
                case 2: FireLaserSweep(boss, playerPos, bossProjectiles); break;
                case 3: StartCharge(boss, playerPos); break;
                case 4: SummonAdds(boss, roomAdds, spawnedAdds); break;
                case 5: FireSpiralBurst(boss, bossProjectiles); break;
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
                boss.hp = boss.maxHp = 250;
                boss.driftSpeed = 17.0f;
                boss.attackCooldownPhase1 = 2.1f;
                boss.attackCooldownPhase2 = 1.25f;
                boss.chargeSpeed = 230.0f;
                break;
            case 2:
                boss.hp = boss.maxHp = 340;
                boss.driftSpeed = 12.0f;
                boss.attackCooldownPhase1 = 1.9f;
                boss.attackCooldownPhase2 = 1.05f;
                boss.chargeSpeed = 250.0f;
                break;
            case 3:
                boss.hp = boss.maxHp = 380;
                boss.driftSpeed = 14.0f;
                boss.attackCooldownPhase1 = 1.8f;
                boss.attackCooldownPhase2 = 1.0f;
                boss.chargeSpeed = 240.0f;
                boss.contactDamage = 2;
                boss.chargeContactDamage = 3;
                break;
            case 4:
                boss.hp = boss.maxHp = 460;
                boss.driftSpeed = 16.0f;
                boss.attackCooldownPhase1 = 1.5f;
                boss.attackCooldownPhase2 = 0.85f;
                boss.chargeSpeed = 260.0f;
                boss.contactDamage = 3;
                boss.chargeContactDamage = 4;
                break;
            case 5:
                boss.hp = boss.maxHp = 300;
                boss.driftSpeed = 10.0f;
                boss.attackCooldownPhase1 = 2.6f;
                boss.attackCooldownPhase2 = 1.6f;
                boss.chargeSpeed = 180.0f;
                boss.contactDamage = 2;
                boss.chargeContactDamage = 3;
                boss.maxAdds = 5;
                break;

            case 6: // arena-control: floor hazards + charge
                boss.hp = boss.maxHp = 400;
                boss.driftSpeed = 13.0f;
                boss.attackCooldownPhase1 = 2.0f;
                boss.attackCooldownPhase2 = 1.2f;
                boss.chargeSpeed = 235.0f;
                boss.contactDamage = 2;
                boss.chargeContactDamage = 3;
                break;
            case 7: // attrition: laser sweep + summon wave
                boss.hp = boss.maxHp = 430;
                boss.driftSpeed = 11.0f;
                boss.attackCooldownPhase1 = 2.2f;
                boss.attackCooldownPhase2 = 1.3f;
                boss.chargeSpeed = 200.0f;
                boss.maxAdds = 5;
                break;
            case 8: // precision: mirror shot + spread shot
                boss.hp = boss.maxHp = 350;
                boss.driftSpeed = 15.0f;
                boss.attackCooldownPhase1 = 1.9f;
                boss.attackCooldownPhase2 = 1.1f;
                boss.chargeSpeed = 220.0f;
                break;
            case 9: // all-rounder late boss
                boss.hp = boss.maxHp = 500;
                boss.driftSpeed = 14.0f;
                boss.attackCooldownPhase1 = 1.6f;
                boss.attackCooldownPhase2 = 0.9f;
                boss.chargeSpeed = 245.0f;
                boss.contactDamage = 3;
                boss.chargeContactDamage = 4;
                boss.maxAdds = 4;
                break;    
            case 0:
            default:
                boss.hp = boss.maxHp = 290;
                boss.driftSpeed = 15.0f;
                boss.attackCooldownPhase1 = 2.5f;
                boss.attackCooldownPhase2 = 1.55f;
                boss.chargeSpeed = 215.0f;
                break;
        }

        boss.attackTimer = boss.attackCooldownPhase1;
        return boss;
    }
}

namespace BossAI {

void Update(Boss& boss, Vec2 playerPos, float dt, std::vector<Projectile>& bossProjectiles,
            const std::vector<Enemy>& roomAdds, std::vector<Enemy>& spawnedAdds) {
    if (!boss.alive) return;
    if (boss.spawnDelayRemaining > 0.0f) {
        boss.spawnDelayRemaining -= dt;
        if (boss.spawnDelayRemaining > 0.0f) return;
    }

    if (boss.poisonTimer > 0.0f) {
        boss.poisonTimer -= dt;
        boss.poisonTickTimer -= dt;
        if (boss.poisonTickTimer <= 0.0f) {
            boss.poisonTickTimer = 0.45f;
            boss.hp -= std::max(1, boss.poisonDamage);
            if (boss.hp <= 0) boss.alive = false;
        }
    }

    if (boss.stickyTimer > 0.0f) {
        boss.stickyTimer -= dt;
        if (boss.stickyTimer <= 0.0f) {
            boss.stickyTimer = 0.0f;
            boss.stickySpeedMultiplier = 1.0f;
        }
    }

    // Tick down and expire any active floor hazards, regardless of variant.
    for (auto it = boss.hazards.begin(); it != boss.hazards.end(); ) {
        it->timeRemaining -= dt;
        if (it->timeRemaining <= 0.0f) it = boss.hazards.erase(it);
        else ++it;
    }

    if (boss.phase == 1 && boss.hp <= boss.maxHp / 2) {
        boss.phase = 2;
    }

    if (boss.isCharging) {
        float slowScale = (boss.stickyTimer > 0.0f) ? boss.stickySpeedMultiplier : 1.0f;
        boss.pos.x += boss.chargeDir.x * boss.chargeSpeed * slowScale * dt;
        boss.pos.y += boss.chargeDir.y * boss.chargeSpeed * slowScale * dt;
        boss.chargeTimeRemaining -= dt;
        if (boss.chargeTimeRemaining <= 0.0f) boss.isCharging = false;
        return;
    }

    Vec2 dir = Normalize({ playerPos.x - boss.pos.x, playerPos.y - boss.pos.y });
    float slowScale = (boss.stickyTimer > 0.0f) ? boss.stickySpeedMultiplier : 1.0f;
    boss.pos.x += dir.x * boss.driftSpeed * slowScale * dt;
    boss.pos.y += dir.y * boss.driftSpeed * slowScale * dt;

    switch (boss.variant) {
        case 1: UpdateVariant1(boss, playerPos, dt, bossProjectiles); break;
        case 2: UpdateVariant2(boss, playerPos, dt, bossProjectiles); break;
        case 3: UpdateVariant3(boss, playerPos, dt, bossProjectiles); break;
        case 4: UpdateVariant4(boss, playerPos, dt, bossProjectiles); break;
        case 5: UpdateVariant5(boss, playerPos, dt, bossProjectiles, roomAdds, spawnedAdds); break;
        case 6: UpdateVariant6(boss, playerPos, dt, bossProjectiles); break;
        case 7: UpdateVariant7(boss, playerPos, dt, bossProjectiles, roomAdds, spawnedAdds); break;
        case 8: UpdateVariant8(boss, playerPos, dt, bossProjectiles); break;
        case 9: UpdateVariant9(boss, playerPos, dt, bossProjectiles, roomAdds, spawnedAdds); break;
        case 0:
        default:
            UpdateVariant0(boss, playerPos, dt, bossProjectiles);
            break;
    }
}

} // namespace BossAI

Boss SpawnBoss1() { return MakeBoss(0); }
Boss SpawnBoss2() { return MakeBoss(1); }
Boss SpawnBoss3() { return MakeBoss(2); }
Boss SpawnBoss4() { return MakeBoss(3); }
Boss SpawnBoss5() { return MakeBoss(4); }
Boss SpawnBoss6() { return MakeBoss(5); }
Boss SpawnBoss7() { return MakeBoss(6); }
Boss SpawnBoss8() { return MakeBoss(7); }
Boss SpawnBoss9() { return MakeBoss(8); }
Boss SpawnBoss10() { return MakeBoss(9); }

Boss SpawnBossVariant(int variant) {
    switch (variant % 10) {
        case 1: return SpawnBoss2();
        case 2: return SpawnBoss3();
        case 3: return SpawnBoss4();
        case 4: return SpawnBoss5();
        case 5: return SpawnBoss6();
        case 6: return SpawnBoss7();
        case 7: return SpawnBoss8();
        case 8: return SpawnBoss9();
        case 9: return SpawnBoss10();
        case 0:
        default:
            return SpawnBoss1();
    }
}