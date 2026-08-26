#include "enemy.h"
#include "enemy_database.h"
#include "../player/projectile_system.h"
#include "../../engine/core/rng.h"
#include <algorithm>
#include <cmath>

namespace {
    // Damage is in half-heart units (player has 6 hp = 3 hearts). Normal
    // enemies poke for half a heart; special-variant enemies (Reinforcer/
    // Creeper/Death Ring) hit for a full heart.
    constexpr int ENEMY_PROJECTILE_DAMAGE_NORMAL = 1;
    constexpr int ENEMY_PROJECTILE_DAMAGE_SPECIAL = 2;
    constexpr int SUMMONER_MAX_ADDITIONAL_ENEMIES = 5;
    constexpr float ENEMY_PROJECTILE_RANGE = 999999.0f;
    constexpr int CREEP_PROJECTILE_DAMAGE = 1;
    constexpr float CREEP_PROJECTILE_LIFE = 0.9f;
    constexpr float PI = 3.14159265f;

    int EnemyProjectileDamage(const Enemy& enemy) {
        return (enemy.specialType == EnemySpecialType::NONE)
            ? ENEMY_PROJECTILE_DAMAGE_NORMAL
            : ENEMY_PROJECTILE_DAMAGE_SPECIAL;
    }

    float FrozenSlowScale(const Enemy& enemy) {
        if (enemy.IsFrozen()) return 0.0f;
        return (enemy.stickyTimer > 0.0f) ? enemy.stickySpeedMultiplier : 1.0f;
    }

    Vec2 DirectionTo(Vec2 from, Vec2 to, float& outDist) {
        Vec2 d = { to.x - from.x, to.y - from.y };
        outDist = std::sqrt(d.x * d.x + d.y * d.y);
        if (outDist > 0.01f) {
            d.x /= outDist;
            d.y /= outDist;
        }
        return d;
    }

    // Fires enemy.attackPattern toward `dir` (aimed patterns) or around the
    // enemy (radial/spiral patterns). Shared by SHOOTER and EXPLODER so any
    // enemy in data can be given any pattern with no new C++.
    void FireByPattern(Enemy& enemy, Vec2 dir, std::vector<Projectile>& out) {
        ProjectileMods mods;
        mods.homing = enemy.hasHomingShots;

        switch (enemy.attackPattern) {
            case AttackPattern::TRIPLE: {
                float baseAngle = std::atan2(dir.y, dir.x);
                const float spread = 0.35f;
                for (int i = -1; i <= 1; ++i) {
                    float angle = baseAngle + spread * (float)i;
                    ProjectileSystem::Spawn(
                        out, enemy.pos,
                        { std::cos(angle) * enemy.shotSpeed, std::sin(angle) * enemy.shotSpeed },
                        EnemyProjectileDamage(enemy),
                        ENEMY_PROJECTILE_RANGE,
                        0.0f,
                        mods
                    );
                }
                break;
            }

            case AttackPattern::RADIAL: {
                const int count = 8;
                for (int i = 0; i < count; ++i) {
                    float angle = (2.0f * PI) * ((float)i / count);
                    ProjectileSystem::Spawn(
                        out, enemy.pos,
                        { std::cos(angle) * enemy.shotSpeed, std::sin(angle) * enemy.shotSpeed },
                        EnemyProjectileDamage(enemy),
                        ENEMY_PROJECTILE_RANGE,
                        0.0f,
                        mods
                    );
                }
                break;
            }

            case AttackPattern::SPIRAL: {
                const int count = 6;
                for (int i = 0; i < count; ++i) {
                    float angle = (2.0f * PI) * ((float)i / count) + enemy.spiralOffset;
                    ProjectileSystem::Spawn(
                        out, enemy.pos,
                        { std::cos(angle) * enemy.shotSpeed, std::sin(angle) * enemy.shotSpeed },
                        EnemyProjectileDamage(enemy),
                        ENEMY_PROJECTILE_RANGE,
                        0.0f,
                        mods
                    );
                }
                enemy.spiralOffset += 0.35f;
                break;
            }

            case AttackPattern::SINGLE:
            default:
                ProjectileSystem::Spawn(
                    out, enemy.pos,
                    { dir.x * enemy.shotSpeed, dir.y * enemy.shotSpeed },
                    EnemyProjectileDamage(enemy),
                    ENEMY_PROJECTILE_RANGE,
                    0.0f,
                    mods
                );
                break;
        }
    }

    void UpdateChaser(Enemy& enemy, Vec2 playerPos, float dt) {
        float dist;
        Vec2 dir = DirectionTo(enemy.pos, playerPos, dist);
        float slowScale = FrozenSlowScale(enemy);
        enemy.pos.x += dir.x * enemy.speed * slowScale * dt;
        enemy.pos.y += dir.y * enemy.speed * slowScale * dt;
    }

    void UpdateShooter(Enemy& enemy, Vec2 playerPos, float dt, std::vector<Projectile>& enemyProjectiles,
                        bool attackReady) {
        float dist;
        Vec2 dir = DirectionTo(enemy.pos, playerPos, dist);

        const float buffer = 15.0f;
        float slowScale = FrozenSlowScale(enemy);
        if (dist < enemy.preferredDistance - buffer) {
            enemy.pos.x -= dir.x * enemy.speed * slowScale * dt;
            enemy.pos.y -= dir.y * enemy.speed * slowScale * dt;
        } else if (dist > enemy.preferredDistance + buffer) {
            enemy.pos.x += dir.x * enemy.speed * slowScale * dt;
            enemy.pos.y += dir.y * enemy.speed * slowScale * dt;
        }

        enemy.shootTimer -= dt;
        if (attackReady && !enemy.IsFrozen() && enemy.shootTimer <= 0.0f && dist <= enemy.shootRange) {
            FireByPattern(enemy, dir, enemyProjectiles);
            enemy.shootTimer = enemy.shootCooldown;
        }
    }

    void UpdateCharger(Enemy& enemy, Vec2 playerPos, float dt, bool attackReady) {
        if (enemy.IsFrozen()) return;

        if (enemy.bouncesOffWalls && enemy.isCharging) {
            enemy.pos.x += enemy.chargeDir.x * enemy.speed * 4.0f * dt;
            enemy.pos.y += enemy.chargeDir.y * enemy.speed * 4.0f * dt;
            enemy.chargeTimeRemaining -= dt;
            if (enemy.chargeTimeRemaining <= 0.0f) enemy.isCharging = false;
            return;
        }

        float dist;
        Vec2 dir = DirectionTo(enemy.pos, playerPos, dist);

        enemy.shootTimer -= dt;
        float moveSpeed = enemy.speed;
        if (attackReady && enemy.shootTimer <= 0.0f) {
            moveSpeed *= 4.0f;
            enemy.shootTimer = enemy.shootCooldown;
            if (enemy.bouncesOffWalls) {
                enemy.isCharging = true;
                enemy.chargeDir = dir;
                enemy.chargeTimeRemaining = 0.35f;
            }
        }

        enemy.pos.x += dir.x * moveSpeed * dt;
        enemy.pos.y += dir.y * moveSpeed * dt;
    }

    void UpdateSummoner(Enemy& enemy, Vec2 playerPos, float dt,
                        const std::vector<Enemy>& roomEnemies, std::vector<Enemy>& spawnedEnemies,
                        bool attackReady) {
        float dist;
        Vec2 dir = DirectionTo(enemy.pos, playerPos, dist);

        const float buffer = 20.0f;
        float slowScale = FrozenSlowScale(enemy);
        if (dist < enemy.preferredDistance - buffer) {
            enemy.pos.x -= dir.x * enemy.speed * slowScale * dt;
            enemy.pos.y -= dir.y * enemy.speed * slowScale * dt;
        } else if (dist > enemy.preferredDistance + buffer) {
            enemy.pos.x += dir.x * enemy.speed * slowScale * dt;
            enemy.pos.y += dir.y * enemy.speed * slowScale * dt;
        }

        enemy.shootTimer -= dt;
        if (attackReady && !enemy.IsFrozen() && enemy.shootTimer <= 0.0f && (int)roomEnemies.size() < SUMMONER_MAX_ADDITIONAL_ENEMIES) {
            Vec2 spawnPos = {
                enemy.pos.x + dir.x * 14.0f,
                enemy.pos.y + dir.y * 14.0f
            };
            spawnedEnemies.push_back(EnemyDatabase::Spawn("Zombie", spawnPos));
            enemy.shootTimer = enemy.shootCooldown;
        }
    }

    void UpdateSpawner(Enemy& enemy, Vec2 playerPos, float dt,
                       std::vector<Enemy>& spawnedEnemies, bool attackReady) {
        float dist;
        Vec2 dir = DirectionTo(enemy.pos, playerPos, dist);

        const float buffer = 18.0f;
        const float preferred = std::max(56.0f, enemy.preferredDistance);
        float slowScale = FrozenSlowScale(enemy);
        if (dist < preferred - buffer) {
            enemy.pos.x -= dir.x * enemy.speed * 0.65f * slowScale * dt;
            enemy.pos.y -= dir.y * enemy.speed * 0.65f * slowScale * dt;
        } else if (dist > preferred + buffer) {
            enemy.pos.x += dir.x * enemy.speed * 0.45f * slowScale * dt;
            enemy.pos.y += dir.y * enemy.speed * 0.45f * slowScale * dt;
        } else {
            enemy.pos.x += -dir.y * enemy.speed * 0.18f * slowScale * dt;
            enemy.pos.y += dir.x * enemy.speed * 0.18f * slowScale * dt;
        }

        enemy.spawnTimer -= dt;
        if (!attackReady || enemy.IsFrozen() || enemy.spawnTimer > 0.0f) return;
        if (enemy.spawnedChildren >= enemy.spawnLimit) return;

        const char* spawnName = (enemy.spawnEnemy[0] != '\0') ? enemy.spawnEnemy : "Zombie";
        int burst = std::min(enemy.spawnCount, enemy.spawnLimit - enemy.spawnedChildren);
        const float radius = std::max(10.0f, std::min(enemy.w, enemy.h) * 0.65f);
        for (int i = 0; i < burst; ++i) {
            float angle = (2.0f * PI) * ((float)i / std::max(1, burst));
            Vec2 spawnPos = {
                enemy.pos.x + std::cos(angle) * radius,
                enemy.pos.y + std::sin(angle) * radius
            };
            spawnedEnemies.push_back(EnemyDatabase::Spawn(spawnName, spawnPos));
        }

        enemy.spawnedChildren += burst;
        enemy.spawnTimer = enemy.shootCooldown;
    }

    void UpdateExploder(Enemy& enemy, Vec2 playerPos, float dt, std::vector<Projectile>& enemyProjectiles,
                        bool attackReady) {
        float dist;
        Vec2 dir = DirectionTo(enemy.pos, playerPos, dist);

        if (enemy.explodesOnTimer) {
            if (enemy.fuseTimer <= 0.0f && dist > enemy.shootRange) {
                float slowScale = FrozenSlowScale(enemy);
                enemy.pos.x += dir.x * enemy.speed * slowScale * dt;
                enemy.pos.y += dir.y * enemy.speed * slowScale * dt;
            } else if (enemy.fuseTimer <= 0.0f && attackReady && !enemy.IsFrozen()) {
                enemy.fuseTimer = enemy.fuseDuration; // arm the fuse once in range
            }

            if (enemy.fuseTimer > 0.0f && !enemy.IsFrozen()) {
                enemy.fuseTimer -= dt;
                if (enemy.fuseTimer <= 0.0f) {
                    FireByPattern(enemy, dir, enemyProjectiles);
                    enemy.alive = false;
                }
            }
            return;
        }

        float slowScale = FrozenSlowScale(enemy);
        enemy.pos.x += dir.x * enemy.speed * slowScale * dt;
        enemy.pos.y += dir.y * enemy.speed * slowScale * dt;

        enemy.shootTimer -= dt;
        if (attackReady && !enemy.IsFrozen() && enemy.shootTimer <= 0.0f && dist <= enemy.shootRange) {
            FireByPattern(enemy, dir, enemyProjectiles);
            enemy.alive = false;
        }
    }

    void UpdateMimic(Enemy& enemy, Vec2 playerPos, float dt, bool attackReady) {
        float dist;
        Vec2 dir = DirectionTo(enemy.pos, playerPos, dist);
        float slowScale = FrozenSlowScale(enemy);

        const float wakeRadius = 60.0f;
        const float lungeRadius = 22.0f;
        const float chestRevealRadius = 36.0f;

        // Mimics pretend to be scenery until the player gets close, then
        // they snap open and pursue hard.
        if (dist > wakeRadius) {
            return;
        }

        float moveSpeed = enemy.speed;
        if (dist <= chestRevealRadius) {
            moveSpeed *= 2.2f;
        } else {
            moveSpeed *= 1.15f;
        }

        enemy.pos.x += dir.x * moveSpeed * slowScale * dt;
        enemy.pos.y += dir.y * moveSpeed * slowScale * dt;

        if (dist <= lungeRadius && attackReady) {
            enemy.shootTimer = std::max(enemy.shootCooldown, 0.35f);
        }
    }

    void UpdateStrafer(Enemy& enemy, Vec2 playerPos, float dt, std::vector<Projectile>& enemyProjectiles,
                       bool attackReady) {
        float dist;
        Vec2 dir = DirectionTo(enemy.pos, playerPos, dist);
        Vec2 perp = { -dir.y, dir.x };
        float slowScale = FrozenSlowScale(enemy);

        const float preferred = std::max(40.0f, enemy.preferredDistance);
        const float buffer = 18.0f;
        float orbitPhase = std::sin(enemy.spiralOffset);
        float orbitDir = (orbitPhase >= 0.0f) ? 1.0f : -1.0f;
        float radial = 0.0f;
        if (dist > preferred + buffer) radial = 1.0f;
        else if (dist < preferred - buffer) radial = -1.0f;

        enemy.pos.x += (dir.x * radial + perp.x * orbitDir * 0.85f) * enemy.speed * slowScale * dt;
        enemy.pos.y += (dir.y * radial + perp.y * orbitDir * 0.85f) * enemy.speed * slowScale * dt;
        enemy.spiralOffset += dt * 3.0f;

        enemy.shootTimer -= dt;
        if (attackReady && !enemy.IsFrozen() && enemy.shootTimer <= 0.0f && dist <= enemy.shootRange) {
            FireByPattern(enemy, dir, enemyProjectiles);
            enemy.shootTimer = enemy.shootCooldown;
        }
    }

    void UpdateDasher(Enemy& enemy, Vec2 playerPos, float dt, std::vector<Projectile>& enemyProjectiles,
                      bool attackReady) {
        if (enemy.IsFrozen()) return;

        float dist;
        Vec2 dir = DirectionTo(enemy.pos, playerPos, dist);
        float slowScale = FrozenSlowScale(enemy);

        if (enemy.isCharging) {
            enemy.pos.x += enemy.chargeDir.x * enemy.speed * 6.0f * slowScale * dt;
            enemy.pos.y += enemy.chargeDir.y * enemy.speed * 6.0f * slowScale * dt;
            enemy.chargeTimeRemaining -= dt;
            if (enemy.chargeTimeRemaining <= 0.0f) {
                enemy.isCharging = false;
                enemy.shootTimer = enemy.shootCooldown;
            }
            return;
        }

        Vec2 perp = { -dir.y, dir.x };
        const float preferred = std::max(48.0f, enemy.preferredDistance);
        const float buffer = 16.0f;
        if (dist < preferred - buffer) {
            enemy.pos.x -= dir.x * enemy.speed * 1.10f * slowScale * dt;
            enemy.pos.y -= dir.y * enemy.speed * 1.10f * slowScale * dt;
        } else if (dist > preferred + buffer) {
            enemy.pos.x += dir.x * enemy.speed * 0.55f * slowScale * dt;
            enemy.pos.y += dir.y * enemy.speed * 0.55f * slowScale * dt;
        } else {
            float orbitDir = (std::sin(enemy.spiralOffset) >= 0.0f) ? 1.0f : -1.0f;
            enemy.pos.x += perp.x * orbitDir * enemy.speed * 0.75f * slowScale * dt;
            enemy.pos.y += perp.y * orbitDir * enemy.speed * 0.75f * slowScale * dt;
            enemy.spiralOffset += dt * 4.0f;
        }

        enemy.shootTimer -= dt;
        if (attackReady && enemy.shootTimer <= 0.0f && dist <= enemy.shootRange + 28.0f) {
            enemy.isCharging = true;
            enemy.chargeDir = dir;
            enemy.chargeTimeRemaining = 0.22f;
            FireByPattern(enemy, dir, enemyProjectiles);
            enemy.shootTimer = enemy.shootCooldown;
        }
    }

    void UpdateLurker(Enemy& enemy, Vec2 playerPos, float dt, std::vector<Projectile>& enemyProjectiles,
                      bool attackReady) {
        float dist;
        Vec2 dir = DirectionTo(enemy.pos, playerPos, dist);
        float slowScale = FrozenSlowScale(enemy);

        const float wakeRadius = std::max(48.0f, enemy.preferredDistance);
        if (dist > wakeRadius) {
            enemy.shootTimer -= dt * 0.35f;
            if (enemy.shootTimer < 0.0f) enemy.shootTimer = 0.0f;
            return;
        }

        float surgeSpeed = enemy.speed * ((dist < 24.0f) ? 2.4f : 1.4f);
        enemy.pos.x += dir.x * surgeSpeed * slowScale * dt;
        enemy.pos.y += dir.y * surgeSpeed * slowScale * dt;

        enemy.shootTimer -= dt;
        if (attackReady && !enemy.IsFrozen() && enemy.shootTimer <= 0.0f) {
            FireByPattern(enemy, dir, enemyProjectiles);
            enemy.shootTimer = enemy.shootCooldown * 0.85f;
        }
    }

    void EnsureAnchor(Enemy& enemy) {
        if (!enemy.aiAnchorSet) {
            enemy.aiAnchor = enemy.pos;
            enemy.aiAnchorSet = true;
        }
    }

    void UpdateTeleporter(Enemy& enemy, Vec2 playerPos, float dt, std::vector<Projectile>& enemyProjectiles,
                          bool attackReady) {
        float dist;
        Vec2 dir = DirectionTo(enemy.pos, playerPos, dist);
        float slowScale = FrozenSlowScale(enemy);

        enemy.shootTimer -= dt;
        if (dist < 42.0f || enemy.shootTimer <= 0.0f) {
            float angle = RNG::Range(0.0f, 2.0f * PI);
            float radius = (dist < 42.0f) ? RNG::Range(92.0f, 128.0f) : RNG::Range(72.0f, 112.0f);
            enemy.pos.x = playerPos.x + std::cos(angle) * radius;
            enemy.pos.y = playerPos.y + std::sin(angle) * radius;
            if (attackReady) {
                float fireDist;
                Vec2 fireDir = DirectionTo(enemy.pos, playerPos, fireDist);
                FireByPattern(enemy, fireDir, enemyProjectiles);
            }
            enemy.shootTimer = enemy.shootCooldown;
            return;
        }

        if (dist > enemy.preferredDistance) {
            enemy.pos.x += dir.x * enemy.speed * 0.35f * slowScale * dt;
            enemy.pos.y += dir.y * enemy.speed * 0.35f * slowScale * dt;
        } else {
            Vec2 perp = { -dir.y, dir.x };
            float orbitDir = (std::sin(enemy.spiralOffset) >= 0.0f) ? 1.0f : -1.0f;
            enemy.pos.x += perp.x * orbitDir * enemy.speed * 0.55f * slowScale * dt;
            enemy.pos.y += perp.y * orbitDir * enemy.speed * 0.55f * slowScale * dt;
            enemy.spiralOffset += dt * 3.0f;
        }
    }

    void UpdateGuardian(Enemy& enemy, Vec2 playerPos, float dt, std::vector<Projectile>& enemyProjectiles,
                        bool attackReady) {
        EnsureAnchor(enemy);

        float distToPlayer;
        Vec2 toPlayer = DirectionTo(enemy.pos, playerPos, distToPlayer);

        float distToAnchor;
        Vec2 toAnchor = DirectionTo(enemy.pos, enemy.aiAnchor, distToAnchor);
        float slowScale = FrozenSlowScale(enemy);

        const float guardRadius = 72.0f;
        if (distToAnchor > guardRadius) {
            enemy.pos.x += toAnchor.x * enemy.speed * 1.15f * slowScale * dt;
            enemy.pos.y += toAnchor.y * enemy.speed * 1.15f * slowScale * dt;
        } else if (distToPlayer < 36.0f) {
            enemy.pos.x -= toPlayer.x * enemy.speed * 0.70f * slowScale * dt;
            enemy.pos.y -= toPlayer.y * enemy.speed * 0.70f * slowScale * dt;
        } else {
            Vec2 orbit = { -toAnchor.y, toAnchor.x };
            float orbitDir = (std::sin(enemy.aiStateTimer) >= 0.0f) ? 1.0f : -1.0f;
            enemy.pos.x += orbit.x * orbitDir * enemy.speed * 0.42f * slowScale * dt;
            enemy.pos.y += orbit.y * orbitDir * enemy.speed * 0.42f * slowScale * dt;
            enemy.aiStateTimer += dt * 2.0f;
        }

        enemy.shootTimer -= dt;
        if (attackReady && !enemy.IsFrozen() && enemy.shootTimer <= 0.0f && distToPlayer <= enemy.shootRange) {
            FireByPattern(enemy, toPlayer, enemyProjectiles);
            enemy.shootTimer = enemy.shootCooldown;
        }
    }

    void UpdateBurrower(Enemy& enemy, Vec2 playerPos, float dt, std::vector<Projectile>& enemyProjectiles,
                        bool attackReady) {
        float dist;
        Vec2 dir = DirectionTo(enemy.pos, playerPos, dist);
        float slowScale = FrozenSlowScale(enemy);

        if (enemy.isCharging) {
            enemy.chargeTimeRemaining -= dt;
            if (enemy.chargeTimeRemaining > 0.0f) return;

            float angle = RNG::Range(0.0f, 2.0f * PI);
            float radius = RNG::Range(24.0f, 48.0f);
            enemy.pos.x = playerPos.x + std::cos(angle) * radius;
            enemy.pos.y = playerPos.y + std::sin(angle) * radius;
            float fireDist;
            Vec2 fireDir = DirectionTo(enemy.pos, playerPos, fireDist);
            FireByPattern(enemy, fireDir, enemyProjectiles);
            enemy.isCharging = false;
            enemy.shootTimer = enemy.shootCooldown * 0.75f;
            return;
        }

        if (dist > enemy.preferredDistance + 20.0f) {
            enemy.pos.x += dir.x * enemy.speed * 0.55f * slowScale * dt;
            enemy.pos.y += dir.y * enemy.speed * 0.55f * slowScale * dt;
        } else if (dist < 28.0f) {
            enemy.pos.x -= dir.x * enemy.speed * 0.45f * slowScale * dt;
            enemy.pos.y -= dir.y * enemy.speed * 0.45f * slowScale * dt;
        } else {
            Vec2 perp = { -dir.y, dir.x };
            float orbitDir = (std::sin(enemy.spiralOffset) >= 0.0f) ? 1.0f : -1.0f;
            enemy.pos.x += perp.x * orbitDir * enemy.speed * 0.32f * slowScale * dt;
            enemy.pos.y += perp.y * orbitDir * enemy.speed * 0.32f * slowScale * dt;
            enemy.spiralOffset += dt * 2.5f;
        }

        enemy.shootTimer -= dt;
        if (attackReady && !enemy.IsFrozen() && enemy.shootTimer <= 0.0f && dist <= enemy.shootRange) {
            enemy.isCharging = true;
            enemy.chargeTimeRemaining = 0.42f;
            enemy.shootTimer = enemy.shootCooldown;
        }
    }

    void UpdateArtillery(Enemy& enemy, Vec2 playerPos, float dt, std::vector<Projectile>& enemyProjectiles,
                         bool attackReady) {
        float dist;
        Vec2 dir = DirectionTo(enemy.pos, playerPos, dist);
        float slowScale = FrozenSlowScale(enemy);
        const float preferred = std::max(140.0f, enemy.preferredDistance);

        if (dist < preferred - 18.0f) {
            enemy.pos.x -= dir.x * enemy.speed * 0.55f * slowScale * dt;
            enemy.pos.y -= dir.y * enemy.speed * 0.55f * slowScale * dt;
        } else if (dist > preferred + 24.0f) {
            enemy.pos.x += dir.x * enemy.speed * 0.20f * slowScale * dt;
            enemy.pos.y += dir.y * enemy.speed * 0.20f * slowScale * dt;
        } else {
            Vec2 perp = { -dir.y, dir.x };
            enemy.pos.x += perp.x * enemy.speed * 0.12f * slowScale * dt;
            enemy.pos.y += perp.y * enemy.speed * 0.12f * slowScale * dt;
        }

        enemy.shootTimer -= dt;
        if (attackReady && !enemy.IsFrozen() && enemy.shootTimer <= 0.0f && dist <= enemy.shootRange) {
            FireByPattern(enemy, dir, enemyProjectiles);
            enemy.shootTimer = enemy.shootCooldown;
        }
    }

    void UpdateLinker(Enemy& enemy, Vec2 playerPos, float dt, const std::vector<Enemy>& roomEnemies,
                      std::vector<Projectile>& enemyProjectiles, bool attackReady) {
        float distToPlayer;
        Vec2 toPlayer = DirectionTo(enemy.pos, playerPos, distToPlayer);
        float slowScale = FrozenSlowScale(enemy);

        const Enemy* closestAlly = nullptr;
        float closestAllyDistSq = 1.0e30f;
        for (const auto& ally : roomEnemies) {
            if (!ally.alive || &ally == &enemy) continue;
            float dx = ally.pos.x - enemy.pos.x;
            float dy = ally.pos.y - enemy.pos.y;
            float distSq = dx * dx + dy * dy;
            if (distSq < closestAllyDistSq) {
                closestAllyDistSq = distSq;
                closestAlly = &ally;
            }
        }

        if (closestAlly) {
            float linkDist = std::sqrt(closestAllyDistSq);
            Vec2 toAlly = DirectionTo(enemy.pos, closestAlly->pos, linkDist);
            if (linkDist > 72.0f) {
                enemy.pos.x += toAlly.x * enemy.speed * 0.90f * slowScale * dt;
                enemy.pos.y += toAlly.y * enemy.speed * 0.90f * slowScale * dt;
            } else if (linkDist < 28.0f) {
                enemy.pos.x -= toAlly.x * enemy.speed * 0.55f * slowScale * dt;
                enemy.pos.y -= toAlly.y * enemy.speed * 0.55f * slowScale * dt;
            } else {
                Vec2 perp = { -toAlly.y, toAlly.x };
                float orbitDir = (std::sin(enemy.aiStateTimer) >= 0.0f) ? 1.0f : -1.0f;
                enemy.pos.x += perp.x * orbitDir * enemy.speed * 0.42f * slowScale * dt;
                enemy.pos.y += perp.y * orbitDir * enemy.speed * 0.42f * slowScale * dt;
                enemy.aiStateTimer += dt * 2.0f;
            }
        } else {
            enemy.pos.x += toPlayer.x * enemy.speed * 0.35f * slowScale * dt;
            enemy.pos.y += toPlayer.y * enemy.speed * 0.35f * slowScale * dt;
        }

        enemy.shootTimer -= dt;
        if (attackReady && !enemy.IsFrozen() && enemy.shootTimer <= 0.0f && distToPlayer <= enemy.shootRange) {
            ProjectileMods mods;
            mods.homing = enemy.hasHomingShots;
            ProjectileSystem::Spawn(
                enemyProjectiles,
                enemy.pos,
                { toPlayer.x * enemy.shotSpeed, toPlayer.y * enemy.shotSpeed },
                EnemyProjectileDamage(enemy),
                ENEMY_PROJECTILE_RANGE,
                0.0f,
                mods
            );
            enemy.shootTimer = enemy.shootCooldown * 0.9f;
        }
    }

    void UpdateSwarmLeader(Enemy& enemy, Vec2 playerPos, float dt, const std::vector<Enemy>& roomEnemies,
                           std::vector<Projectile>& enemyProjectiles, bool attackReady) {
        float distToPlayer;
        Vec2 toPlayer = DirectionTo(enemy.pos, playerPos, distToPlayer);
        float slowScale = FrozenSlowScale(enemy);

        int nearbyAllies = 0;
        for (const auto& ally : roomEnemies) {
            if (!ally.alive || &ally == &enemy) continue;
            float dx = ally.pos.x - enemy.pos.x;
            float dy = ally.pos.y - enemy.pos.y;
            if ((dx * dx + dy * dy) <= 6400.0f) nearbyAllies++;
        }

        float speedBoost = 1.0f + 0.12f * (float)nearbyAllies;
        if (distToPlayer > enemy.preferredDistance + 18.0f) {
            enemy.pos.x += toPlayer.x * enemy.speed * speedBoost * slowScale * dt;
            enemy.pos.y += toPlayer.y * enemy.speed * speedBoost * slowScale * dt;
        } else if (distToPlayer < enemy.preferredDistance - 18.0f) {
            enemy.pos.x -= toPlayer.x * enemy.speed * 0.65f * slowScale * dt;
            enemy.pos.y -= toPlayer.y * enemy.speed * 0.65f * slowScale * dt;
        } else {
            Vec2 perp = { -toPlayer.y, toPlayer.x };
            float orbitDir = (nearbyAllies % 2 == 0) ? 1.0f : -1.0f;
            enemy.pos.x += perp.x * orbitDir * enemy.speed * 0.50f * slowScale * dt;
            enemy.pos.y += perp.y * orbitDir * enemy.speed * 0.50f * slowScale * dt;
        }

        enemy.shootTimer -= dt * std::max(0.55f, 1.0f - 0.08f * (float)nearbyAllies);
        if (attackReady && !enemy.IsFrozen() && enemy.shootTimer <= 0.0f && distToPlayer <= enemy.shootRange) {
            if (nearbyAllies >= 2) {
                FireByPattern(enemy, toPlayer, enemyProjectiles);
            } else {
                ProjectileSystem::Spawn(
                    enemyProjectiles,
                    enemy.pos,
                    { toPlayer.x * enemy.shotSpeed, toPlayer.y * enemy.shotSpeed },
                    EnemyProjectileDamage(enemy),
                    ENEMY_PROJECTILE_RANGE,
                    0.0f,
                    {}
                );
            }
            enemy.shootTimer = enemy.shootCooldown;
        }
    }

    void UpdatePatroller(Enemy& enemy, Vec2 playerPos, float dt, std::vector<Projectile>& enemyProjectiles,
                         bool attackReady) {
        EnsureAnchor(enemy);

        const Vec2 waypoints[4] = {
            { enemy.aiAnchor.x + 42.0f, enemy.aiAnchor.y },
            { enemy.aiAnchor.x, enemy.aiAnchor.y + 42.0f },
            { enemy.aiAnchor.x - 42.0f, enemy.aiAnchor.y },
            { enemy.aiAnchor.x, enemy.aiAnchor.y - 42.0f }
        };

        int waypoint = ((int)enemy.aiStateTimer) & 3;
        float distToWaypoint;
        Vec2 toWaypoint = DirectionTo(enemy.pos, waypoints[waypoint], distToWaypoint);
        float slowScale = FrozenSlowScale(enemy);
        enemy.pos.x += toWaypoint.x * enemy.speed * 0.85f * slowScale * dt;
        enemy.pos.y += toWaypoint.y * enemy.speed * 0.85f * slowScale * dt;
        if (distToWaypoint < 8.0f) {
            enemy.aiStateTimer = (float)(((int)enemy.aiStateTimer + 1) & 3);
        }

        float distToPlayer;
        Vec2 toPlayer = DirectionTo(enemy.pos, playerPos, distToPlayer);
        enemy.shootTimer -= dt;
        if (attackReady && !enemy.IsFrozen() && enemy.shootTimer <= 0.0f && distToPlayer <= enemy.shootRange) {
            FireByPattern(enemy, toPlayer, enemyProjectiles);
            enemy.shootTimer = enemy.shootCooldown;
        }
    }

    void UpdateCoward(Enemy& enemy, Vec2 playerPos, float dt, std::vector<Projectile>& enemyProjectiles,
                      bool attackReady) {
        EnsureAnchor(enemy);

        float distToPlayer;
        Vec2 toPlayer = DirectionTo(enemy.pos, playerPos, distToPlayer);
        float slowScale = FrozenSlowScale(enemy);
        float hpRatio = (enemy.maxHp > 0) ? (float)enemy.hp / (float)enemy.maxHp : 1.0f;
        bool scared = (hpRatio < 0.45f) || (distToPlayer < 44.0f);

        if (scared) {
            float distToHome;
            Vec2 toHome = DirectionTo(enemy.pos, enemy.aiAnchor, distToHome);
            enemy.pos.x -= toPlayer.x * enemy.speed * 0.95f * slowScale * dt;
            enemy.pos.y -= toPlayer.y * enemy.speed * 0.95f * slowScale * dt;
            enemy.pos.x += toHome.x * enemy.speed * 0.55f * slowScale * dt;
            enemy.pos.y += toHome.y * enemy.speed * 0.55f * slowScale * dt;
        } else {
            if (distToPlayer > enemy.preferredDistance + 15.0f) {
                enemy.pos.x += toPlayer.x * enemy.speed * 0.55f * slowScale * dt;
                enemy.pos.y += toPlayer.y * enemy.speed * 0.55f * slowScale * dt;
            } else if (distToPlayer < enemy.preferredDistance - 15.0f) {
                enemy.pos.x -= toPlayer.x * enemy.speed * 0.45f * slowScale * dt;
                enemy.pos.y -= toPlayer.y * enemy.speed * 0.45f * slowScale * dt;
            }
        }

        enemy.shootTimer -= dt;
        if (attackReady && !enemy.IsFrozen() && enemy.shootTimer <= 0.0f && distToPlayer <= enemy.shootRange) {
            FireByPattern(enemy, toPlayer, enemyProjectiles);
            enemy.shootTimer = scared ? enemy.shootCooldown * 1.4f : enemy.shootCooldown;
        }
    }

    void MaybeDropCreep(Enemy& enemy, Vec2 beforePos, float dt, std::vector<Projectile>& enemyProjectiles) {
        if (enemy.specialType != EnemySpecialType::CREEPER) return;

        float dx = enemy.pos.x - beforePos.x;
        float dy = enemy.pos.y - beforePos.y;
        float movedSq = dx * dx + dy * dy;
        if (movedSq < 0.01f) return;

        enemy.creepDropTimer -= dt;
        if (enemy.creepDropTimer > 0.0f) return;

        ProjectileSystem::Spawn(
            enemyProjectiles,
            { enemy.pos.x + enemy.w * 0.25f, enemy.pos.y + enemy.h * 0.25f },
            { 0.0f, 0.0f },
            CREEP_PROJECTILE_DAMAGE,
            ENEMY_PROJECTILE_RANGE,
            CREEP_PROJECTILE_LIFE
        );
        enemy.creepDropTimer = enemy.creepDropInterval;
    }
}

namespace EnemyAI {

void Update(Enemy& enemy, Vec2 playerPos, float dt, const std::vector<Enemy>& roomEnemies,
            std::vector<Enemy>& spawnedEnemies, std::vector<Projectile>& enemyProjectiles) {
    if (!enemy.alive) return;
    if (enemy.attackDelayRemaining > 0.0f) {
        enemy.attackDelayRemaining -= dt;
    }
    if (enemy.spawnDelayRemaining > 0.0f) {
        enemy.spawnDelayRemaining -= dt;
        if (enemy.spawnDelayRemaining > 0.0f) return;
    }
    bool attackReady = enemy.attackDelayRemaining <= 0.0f;

    if (enemy.poisonTimer > 0.0f) {
        enemy.poisonTimer -= dt;
        enemy.poisonTickTimer -= dt;
        if (enemy.poisonTickTimer <= 0.0f) {
            enemy.poisonTickTimer = 0.45f;
            enemy.hp -= std::max(1, enemy.poisonDamage);
            if (enemy.hp <= 0) enemy.alive = false;
        }
    }

    if (enemy.stickyTimer > 0.0f) {
        enemy.stickyTimer -= dt;
        if (enemy.stickyTimer <= 0.0f) {
            enemy.stickyTimer = 0.0f;
            enemy.stickySpeedMultiplier = 1.0f;
        }
    }

    if (enemy.burnTimer > 0.0f) {
        enemy.burnTimer -= dt;
        enemy.burnTickTimer -= dt;
        if (enemy.burnTickTimer <= 0.0f) {
            enemy.burnTickTimer = 0.5f;
            enemy.hp -= std::max(1, enemy.burnDamage);
            if (enemy.hp <= 0) enemy.alive = false;
        }
    }

    if (enemy.freezeTimer > 0.0f) {
        enemy.freezeTimer -= dt;
        if (enemy.freezeTimer < 0.0f) enemy.freezeTimer = 0.0f;
    }

    if (!enemy.alive) return;

    Vec2 beforePos = enemy.pos;

    switch (enemy.aiType) {
        case AIType::CHASER:
            UpdateChaser(enemy, playerPos, dt);
            break;
        case AIType::SHOOTER:
            UpdateShooter(enemy, playerPos, dt, enemyProjectiles, attackReady);
            break;
        case AIType::CHARGER:
            UpdateCharger(enemy, playerPos, dt, attackReady);
            break;
        case AIType::SUMMONER:
            UpdateSummoner(enemy, playerPos, dt, roomEnemies, spawnedEnemies, attackReady);
            break;
        case AIType::SPAWNER:
            UpdateSpawner(enemy, playerPos, dt, spawnedEnemies, attackReady);
            break;
        case AIType::EXPLODER:
            UpdateExploder(enemy, playerPos, dt, enemyProjectiles, attackReady);
            break;
        case AIType::MIMIC:
            UpdateMimic(enemy, playerPos, dt, attackReady);
            break;
        case AIType::STRAFER:
            UpdateStrafer(enemy, playerPos, dt, enemyProjectiles, attackReady);
            break;
        case AIType::DASHER:
            UpdateDasher(enemy, playerPos, dt, enemyProjectiles, attackReady);
            break;
        case AIType::LURKER:
            UpdateLurker(enemy, playerPos, dt, enemyProjectiles, attackReady);
            break;
        case AIType::TELEPORTER:
            UpdateTeleporter(enemy, playerPos, dt, enemyProjectiles, attackReady);
            break;
        case AIType::GUARDIAN:
            UpdateGuardian(enemy, playerPos, dt, enemyProjectiles, attackReady);
            break;
        case AIType::BURROWER:
            UpdateBurrower(enemy, playerPos, dt, enemyProjectiles, attackReady);
            break;
        case AIType::ARTILLERY:
            UpdateArtillery(enemy, playerPos, dt, enemyProjectiles, attackReady);
            break;
        case AIType::LINKER:
            UpdateLinker(enemy, playerPos, dt, roomEnemies, enemyProjectiles, attackReady);
            break;
        case AIType::SWARM_LEADER:
            UpdateSwarmLeader(enemy, playerPos, dt, roomEnemies, enemyProjectiles, attackReady);
            break;
        case AIType::PATROLLER:
            UpdatePatroller(enemy, playerPos, dt, enemyProjectiles, attackReady);
            break;
        case AIType::COWARD:
            UpdateCoward(enemy, playerPos, dt, enemyProjectiles, attackReady);
            break;
    }

    MaybeDropCreep(enemy, beforePos, dt, enemyProjectiles);
}

} // namespace EnemyAI
