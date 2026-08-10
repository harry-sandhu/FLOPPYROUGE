#include "enemy.h"
#include "enemy_database.h"
#include "../player/projectile_system.h"
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
                        enemy.hasHomingShots
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
                        enemy.hasHomingShots
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
                        enemy.hasHomingShots
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
                    enemy.hasHomingShots
                );
                break;
        }
    }

    void UpdateChaser(Enemy& enemy, Vec2 playerPos, float dt) {
        float dist;
        Vec2 dir = DirectionTo(enemy.pos, playerPos, dist);
        float slowScale = (enemy.stickyTimer > 0.0f) ? enemy.stickySpeedMultiplier : 1.0f;
        enemy.pos.x += dir.x * enemy.speed * slowScale * dt;
        enemy.pos.y += dir.y * enemy.speed * slowScale * dt;
    }

    void UpdateShooter(Enemy& enemy, Vec2 playerPos, float dt, std::vector<Projectile>& enemyProjectiles) {
        float dist;
        Vec2 dir = DirectionTo(enemy.pos, playerPos, dist);

        const float buffer = 15.0f;
        float slowScale = (enemy.stickyTimer > 0.0f) ? enemy.stickySpeedMultiplier : 1.0f;
        if (dist < enemy.preferredDistance - buffer) {
            enemy.pos.x -= dir.x * enemy.speed * slowScale * dt;
            enemy.pos.y -= dir.y * enemy.speed * slowScale * dt;
        } else if (dist > enemy.preferredDistance + buffer) {
            enemy.pos.x += dir.x * enemy.speed * slowScale * dt;
            enemy.pos.y += dir.y * enemy.speed * slowScale * dt;
        }

        enemy.shootTimer -= dt;
        if (enemy.shootTimer <= 0.0f && dist <= enemy.shootRange) {
            FireByPattern(enemy, dir, enemyProjectiles);
            enemy.shootTimer = enemy.shootCooldown;
        }
    }

    void UpdateCharger(Enemy& enemy, Vec2 playerPos, float dt) {
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
        if (enemy.shootTimer <= 0.0f) {
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
                        const std::vector<Enemy>& roomEnemies, std::vector<Enemy>& spawnedEnemies) {
        float dist;
        Vec2 dir = DirectionTo(enemy.pos, playerPos, dist);

        const float buffer = 20.0f;
        float slowScale = (enemy.stickyTimer > 0.0f) ? enemy.stickySpeedMultiplier : 1.0f;
        if (dist < enemy.preferredDistance - buffer) {
            enemy.pos.x -= dir.x * enemy.speed * slowScale * dt;
            enemy.pos.y -= dir.y * enemy.speed * slowScale * dt;
        } else if (dist > enemy.preferredDistance + buffer) {
            enemy.pos.x += dir.x * enemy.speed * slowScale * dt;
            enemy.pos.y += dir.y * enemy.speed * slowScale * dt;
        }

        enemy.shootTimer -= dt;
        if (enemy.shootTimer <= 0.0f && (int)roomEnemies.size() < SUMMONER_MAX_ADDITIONAL_ENEMIES) {
            Vec2 spawnPos = {
                enemy.pos.x + dir.x * 14.0f,
                enemy.pos.y + dir.y * 14.0f
            };
            spawnedEnemies.push_back(EnemyDatabase::Spawn("Zombie", spawnPos));
            enemy.shootTimer = enemy.shootCooldown;
        }
    }

    void UpdateExploder(Enemy& enemy, Vec2 playerPos, float dt, std::vector<Projectile>& enemyProjectiles) {
        float dist;
        Vec2 dir = DirectionTo(enemy.pos, playerPos, dist);

        if (enemy.explodesOnTimer) {
            if (enemy.fuseTimer <= 0.0f && dist > enemy.shootRange) {
                float slowScale = (enemy.stickyTimer > 0.0f) ? enemy.stickySpeedMultiplier : 1.0f;
                enemy.pos.x += dir.x * enemy.speed * slowScale * dt;
                enemy.pos.y += dir.y * enemy.speed * slowScale * dt;
            } else if (enemy.fuseTimer <= 0.0f) {
                enemy.fuseTimer = enemy.fuseDuration; // arm the fuse once in range
            }

            if (enemy.fuseTimer > 0.0f) {
                enemy.fuseTimer -= dt;
                if (enemy.fuseTimer <= 0.0f) {
                    FireByPattern(enemy, dir, enemyProjectiles);
                    enemy.alive = false;
                }
            }
            return;
        }

        float slowScale = (enemy.stickyTimer > 0.0f) ? enemy.stickySpeedMultiplier : 1.0f;
        enemy.pos.x += dir.x * enemy.speed * slowScale * dt;
        enemy.pos.y += dir.y * enemy.speed * slowScale * dt;

        enemy.shootTimer -= dt;
        if (enemy.shootTimer <= 0.0f && dist <= enemy.shootRange) {
            FireByPattern(enemy, dir, enemyProjectiles);
            enemy.alive = false;
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
    if (enemy.spawnDelayRemaining > 0.0f) {
        enemy.spawnDelayRemaining -= dt;
        if (enemy.spawnDelayRemaining > 0.0f) return;
    }

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

    Vec2 beforePos = enemy.pos;

    switch (enemy.aiType) {
        case AIType::CHASER:
            UpdateChaser(enemy, playerPos, dt);
            break;
        case AIType::SHOOTER:
            UpdateShooter(enemy, playerPos, dt, enemyProjectiles);
            break;
        case AIType::CHARGER:
            UpdateCharger(enemy, playerPos, dt);
            break;
        case AIType::SUMMONER:
            UpdateSummoner(enemy, playerPos, dt, roomEnemies, spawnedEnemies);
            break;
        case AIType::EXPLODER:
            UpdateExploder(enemy, playerPos, dt, enemyProjectiles);
            break;
    }

    MaybeDropCreep(enemy, beforePos, dt, enemyProjectiles);
}

} // namespace EnemyAI