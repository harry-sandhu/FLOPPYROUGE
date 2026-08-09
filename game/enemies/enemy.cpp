#include "enemy.h"
#include "enemy_database.h"
#include "../player/projectile_system.h"
#include <cmath>

namespace {
    constexpr int ENEMY_PROJECTILE_DAMAGE = 7;
    constexpr int SUMMONER_MAX_ADDITIONAL_ENEMIES = 5;
    constexpr float ENEMY_PROJECTILE_RANGE = 999999.0f;
    constexpr int CREEP_PROJECTILE_DAMAGE = 4;
    constexpr float CREEP_PROJECTILE_LIFE = 0.9f;
    constexpr float PI = 3.14159265f;

    Vec2 DirectionTo(Vec2 from, Vec2 to, float& outDist) {
        Vec2 d = { to.x - from.x, to.y - from.y };
        outDist = std::sqrt(d.x * d.x + d.y * d.y);
        if (outDist > 0.01f) {
            d.x /= outDist;
            d.y /= outDist;
        }
        return d;
    }

    void UpdateChaser(Enemy& enemy, Vec2 playerPos, float dt) {
        float dist;
        Vec2 dir = DirectionTo(enemy.pos, playerPos, dist);
        enemy.pos.x += dir.x * enemy.speed * dt;
        enemy.pos.y += dir.y * enemy.speed * dt;
    }

    void UpdateShooter(Enemy& enemy, Vec2 playerPos, float dt, std::vector<Projectile>& enemyProjectiles) {
        float dist;
        Vec2 dir = DirectionTo(enemy.pos, playerPos, dist);

        const float buffer = 15.0f;
        if (dist < enemy.preferredDistance - buffer) {
            enemy.pos.x -= dir.x * enemy.speed * dt;
            enemy.pos.y -= dir.y * enemy.speed * dt;
        } else if (dist > enemy.preferredDistance + buffer) {
            enemy.pos.x += dir.x * enemy.speed * dt;
            enemy.pos.y += dir.y * enemy.speed * dt;
        }

        enemy.shootTimer -= dt;
        if (enemy.shootTimer <= 0.0f && dist <= enemy.shootRange) {
            ProjectileSystem::Spawn(
                enemyProjectiles,
                enemy.pos,
                { dir.x * enemy.shotSpeed, dir.y * enemy.shotSpeed },
                ENEMY_PROJECTILE_DAMAGE,
                ENEMY_PROJECTILE_RANGE
            );
            enemy.shootTimer = enemy.shootCooldown;
        }
    }

    void UpdateCharger(Enemy& enemy, Vec2 playerPos, float dt) {
        float dist;
        Vec2 dir = DirectionTo(enemy.pos, playerPos, dist);

        enemy.shootTimer -= dt;
        float moveSpeed = enemy.speed;
        if (enemy.shootTimer <= 0.0f) {
            moveSpeed *= 4.0f;
            enemy.shootTimer = enemy.shootCooldown;
        }

        enemy.pos.x += dir.x * moveSpeed * dt;
        enemy.pos.y += dir.y * moveSpeed * dt;
    }

    void UpdateSummoner(Enemy& enemy, Vec2 playerPos, float dt,
                        const std::vector<Enemy>& roomEnemies, std::vector<Enemy>& spawnedEnemies) {
        float dist;
        Vec2 dir = DirectionTo(enemy.pos, playerPos, dist);

        const float buffer = 20.0f;
        if (dist < enemy.preferredDistance - buffer) {
            enemy.pos.x -= dir.x * enemy.speed * dt;
            enemy.pos.y -= dir.y * enemy.speed * dt;
        } else if (dist > enemy.preferredDistance + buffer) {
            enemy.pos.x += dir.x * enemy.speed * dt;
            enemy.pos.y += dir.y * enemy.speed * dt;
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

        enemy.pos.x += dir.x * enemy.speed * dt;
        enemy.pos.y += dir.y * enemy.speed * dt;

        enemy.shootTimer -= dt;
        if (enemy.shootTimer <= 0.0f && dist <= enemy.shootRange) {
            const int count = 6;
            const float speed = 88.0f;
            for (int i = 0; i < count; ++i) {
                float angle = (2.0f * PI) * ((float)i / count);
                ProjectileSystem::Spawn(
                    enemyProjectiles,
                    enemy.pos,
                    { std::cos(angle) * speed, std::sin(angle) * speed },
                    ENEMY_PROJECTILE_DAMAGE,
                    ENEMY_PROJECTILE_RANGE
                );
            }
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
