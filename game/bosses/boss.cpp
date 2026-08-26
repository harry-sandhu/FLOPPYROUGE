#include "boss.h"
#include "boss_database.h"
#include "../player/projectile_system.h"
#include "../enemies/enemy_database.h"
#include <algorithm>
#include <cmath>
#include <cstring>

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

    // MIRROR_SHOT: fires from the point on the opposite side of the room
    // from the boss (mirrored through room center), aimed at the player.
    // Reads as a genuine "second source" instead of a reskinned ring.
    void FireMirrorShot(Boss& boss, Vec2 playerPos, std::vector<Projectile>& out) {
        const Vec2 roomCenter = { 160.0f, 90.0f }; // 320x180 play area center
        Vec2 mirrorPos = {
            roomCenter.x + (roomCenter.x - boss.pos.x),
            roomCenter.y + (roomCenter.y - boss.pos.y)
        };

        Vec2 dirFromBoss = Normalize({ playerPos.x - boss.pos.x, playerPos.y - boss.pos.y });
        Vec2 dirFromMirror = Normalize({ playerPos.x - mirrorPos.x, playerPos.y - mirrorPos.y });
        const float speed = 90.0f;

        ProjectileSystem::Spawn(
            out, boss.pos,
            { dirFromBoss.x * speed, dirFromBoss.y * speed },
            BOSS_PROJECTILE_DAMAGE, BOSS_PROJECTILE_RANGE
        );
        ProjectileSystem::Spawn(
            out, mirrorPos,
            { dirFromMirror.x * speed, dirFromMirror.y * speed },
            BOSS_PROJECTILE_DAMAGE, BOSS_PROJECTILE_RANGE
        );
    }

    void FireTeleportBurst(Boss& boss, Vec2 playerPos, std::vector<Projectile>& out) {
        const Vec2 pads[] = {
            { 44.0f, 28.0f }, { 224.0f, 28.0f }, { 44.0f, 104.0f }, { 224.0f, 104.0f }
        };
        int bestIndex = 0;
        float bestDist = -1.0f;
        for (int i = 0; i < 4; ++i) {
            float dx = pads[i].x - playerPos.x;
            float dy = pads[i].y - playerPos.y;
            float dist = dx * dx + dy * dy;
            if (dist > bestDist) {
                bestDist = dist;
                bestIndex = i;
            }
        }

        boss.pos = pads[bestIndex];
        boss.isCharging = false;
        boss.chargeTimeRemaining = 0.0f;
        FireRadialBurst(boss, out);
    }

    // VORTEX_PULL: the boss's signature arena-control move. Player gets
    // yanked toward the boss over ~0.6s, then a radial burst fires the
    // instant the pull ends — punishes standing still, rewards dashing
    // out of the pull before the burst lands.
    void StartVortexPull(Boss& boss) {
        boss.isVortexPulling = true;
        boss.vortexPullTimeRemaining = 0.6f;
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

    // Generic attack executor - dispatches to the correct attack function based on BossAttackType
    void ExecuteAttack(Boss& boss, BossAttackType attackType, Vec2 playerPos, std::vector<Projectile>& bossProjectiles,
                       const std::vector<Enemy>& roomAdds, std::vector<Enemy>& spawnedAdds) {
        switch (attackType) {
            case BossAttackType::SPREAD_SHOT:     FireSpreadShot(boss, playerPos, bossProjectiles); break;
            case BossAttackType::RADIAL_BURST:    FireRadialBurst(boss, bossProjectiles); break;
            case BossAttackType::CHARGE:          StartCharge(boss, playerPos); break;
            case BossAttackType::LASER_SWEEP:     FireLaserSweep(boss, playerPos, bossProjectiles); break;
            case BossAttackType::SUMMON_WAVE:     SummonAdds(boss, roomAdds, spawnedAdds); break;
            case BossAttackType::FLOOR_HAZARD:    DropFloorHazard(boss, playerPos); break;
            case BossAttackType::MIRROR_SHOT:     FireMirrorShot(boss, playerPos, bossProjectiles); break;
            case BossAttackType::CARDINAL_BURST:  FireCardinalBurst(boss, bossProjectiles); break;
            case BossAttackType::SPIRAL_BURST:    FireSpiralBurst(boss, bossProjectiles); break;
            case BossAttackType::TRIPLE_SPREAD:   FireTripleSpread(boss, playerPos, bossProjectiles); break;
            case BossAttackType::DENSE_RING:      FireDenseRing(boss, bossProjectiles); break;
            case BossAttackType::GAPPED_RING:     FireGappedRing(boss, playerPos, bossProjectiles); break;
            case BossAttackType::TELEPORT_BURST:  FireTeleportBurst(boss, playerPos, bossProjectiles); break;
            case BossAttackType::VORTEX_PULL:     StartVortexPull(boss); break;
            default: break;
        }
    }

    // Data-driven update function that uses attack cycles from BossTemplate
    void UpdateBossWithCycle(Boss& boss, Vec2 playerPos, float dt, std::vector<Projectile>& bossProjectiles,
                             const std::vector<Enemy>& roomAdds, std::vector<Enemy>& spawnedAdds,
                             const BossTemplate* templateData) {
        static const BossAttackType GENERIC_CYCLE[] = {
            BossAttackType::SPREAD_SHOT,
            BossAttackType::RADIAL_BURST,
            BossAttackType::CHARGE
        };

        const BossAttackType* cycle = GENERIC_CYCLE;
        int cycleLength = (int)(sizeof(GENERIC_CYCLE) / sizeof(GENERIC_CYCLE[0]));
        if (templateData) {
            const BossAttackType* templateCycle =
                (boss.phase == 1) ? templateData->attackCyclePhase1 : templateData->attackCyclePhase2;
            int templateLength =
                (boss.phase == 1) ? templateData->attackCyclePhase1Length : templateData->attackCyclePhase2Length;
            if (templateLength > 0) {
                cycle = templateCycle;
                cycleLength = templateLength;
            }
        }

        boss.attackTimer -= dt;
        if (boss.attackTimer <= 0.0f) {
            BossAttackType attack = cycle[boss.attackIndex % cycleLength];
            boss.attackIndex++;
            ExecuteAttack(boss, attack, playerPos, bossProjectiles, roomAdds, spawnedAdds);
            boss.attackTimer = (boss.phase == 1) ? boss.attackCooldownPhase1 : boss.attackCooldownPhase2;
        }
    }

    Boss MakeBoss(int variant) {
        Boss boss;
        boss.variant = variant;
        boss.pos = { 146.0f, 20.0f };

        if (const BossTemplate* templateData = BossDatabase::Get(variant % std::max(1, BossDatabase::Count()))) {
            boss.hp = boss.maxHp = templateData->hp;
            boss.driftSpeed = templateData->driftSpeed;
            boss.attackCooldownPhase1 = templateData->attackCooldownPhase1;
            boss.attackCooldownPhase2 = templateData->attackCooldownPhase2;
            boss.chargeSpeed = templateData->chargeSpeed;
            boss.contactDamage = templateData->contactDamage;
            boss.chargeContactDamage = templateData->chargeContactDamage;
            boss.maxAdds = templateData->maxAdds;
            boss.isDragonFinale = std::strcmp(templateData->name, "DragonSovereign") == 0;
            if (boss.isDragonFinale) {
                boss.w = 46.0f;
                boss.h = 24.0f;
                boss.driftSpeed *= 1.05f;
                boss.maxAdds = std::max(boss.maxAdds, 6);
            }
        }

        boss.attackTimer = boss.attackCooldownPhase1;
        return boss;
    }
}

namespace BossAI {

void Update(Boss& boss, Vec2 playerPos, float dt, std::vector<Projectile>& bossProjectiles,
            const std::vector<Enemy>& roomAdds, std::vector<Enemy>& spawnedAdds) {
    if (!boss.alive) return;
    if (boss.attackDelayRemaining > 0.0f) {
        boss.attackDelayRemaining -= dt;
    }
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

    if (boss.burnTimer > 0.0f) {
        boss.burnTimer -= dt;
        boss.burnTickTimer -= dt;
        if (boss.burnTickTimer <= 0.0f) {
            boss.burnTickTimer = 0.5f;
            boss.hp -= std::max(1, boss.burnDamage);
            if (boss.hp <= 0) boss.alive = false;
        }
    }

    if (boss.freezeTimer > 0.0f) {
        boss.freezeTimer -= dt;
        if (boss.freezeTimer < 0.0f) boss.freezeTimer = 0.0f;
    }

    if (!boss.alive) return;

    // Tick down and expire any active floor hazards, regardless of variant.
    for (auto it = boss.hazards.begin(); it != boss.hazards.end(); ) {
        it->timeRemaining -= dt;
        if (it->timeRemaining <= 0.0f) it = boss.hazards.erase(it);
        else ++it;
    }

    const BossTemplate* templateData = BossDatabase::Get(boss.variant % std::max(1, BossDatabase::Count()));
    float phase2Ratio = templateData ? templateData->phase2HpRatio : 0.5f;

    if (boss.phase == 1 && boss.hp <= (int)std::lround((float)boss.maxHp * phase2Ratio)) {
        boss.phase = 2;
    }

        if (boss.IsFrozen()) return; // frozen: no movement, no attacks, hazards/status still tick above

    if (boss.isVortexPulling) {
        boss.vortexPullTimeRemaining -= dt;

        if (boss.vortexPullTimeRemaining <= 0.0f) {
            boss.vortexPullTimeRemaining = 0.0f;
            boss.isVortexPulling = false;
            FireRadialBurst(boss, bossProjectiles);
        }

        return;
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
    if (boss.isDragonFinale) {
        Vec2 anchor = { 160.0f, 54.0f };
        float sweep = std::sin((float)boss.attackIndex * 0.35f + boss.hp * 0.01f);
        Vec2 hover = {
            anchor.x + sweep * 78.0f,
            anchor.y + std::cos((float)boss.attackIndex * 0.22f) * 16.0f
        };
        Vec2 hoverDir = Normalize({ hover.x - boss.pos.x, hover.y - boss.pos.y });
        dir = Normalize({ dir.x * 0.45f + hoverDir.x * 0.55f, dir.y * 0.45f + hoverDir.y * 0.55f });
    }
    if (templateData) {
        if (std::strcmp(templateData->name, "Coward") == 0) {
            Vec2 away = Normalize({ boss.pos.x - playerPos.x, boss.pos.y - playerPos.y });
            dir = Normalize({ dir.x * 0.20f + away.x * 0.80f, dir.y * 0.20f + away.y * 0.80f });
        } else if (std::strcmp(templateData->name, "Patroller") == 0) {
            Vec2 patrol = (boss.attackIndex % 2 == 0)
                ? Vec2{ (boss.pos.y < 90.0f) ? 1.0f : -1.0f, 0.0f }
                : Vec2{ 0.0f, (boss.pos.x < 160.0f) ? 1.0f : -1.0f };
            dir = Normalize({ dir.x * 0.35f + patrol.x * 0.65f, dir.y * 0.35f + patrol.y * 0.65f });
        }
    }
    float slowScale = (boss.stickyTimer > 0.0f) ? boss.stickySpeedMultiplier : 1.0f;
    boss.pos.x += dir.x * boss.driftSpeed * slowScale * dt;
    boss.pos.y += dir.y * boss.driftSpeed * slowScale * dt;
    if (boss.isDragonFinale) {
        if (boss.pos.y > 82.0f) boss.pos.y = 82.0f;
    }

    if (boss.attackDelayRemaining <= 0.0f) {
        UpdateBossWithCycle(boss, playerPos, dt, bossProjectiles, roomAdds, spawnedAdds, templateData);
    }
}

} // namespace BossAI

Boss SpawnBossVariant(int variant) {
    return MakeBoss(variant % std::max(1, BossDatabase::Count()));
}
