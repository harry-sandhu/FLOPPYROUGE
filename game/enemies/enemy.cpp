#include "enemy.h"
#include "enemy_database.h"
#include "../player/projectile_system.h"
#include <cmath>

namespace {
    constexpr int ENEMY_PROJECTILE_DAMAGE = 8;
    constexpr int SUMMONER_MAX_ADDITIONAL_ENEMIES = 6;
    constexpr float ENEMY_PROJECTILE_RANGE = 999999.0f;
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
            const int count = 8;
            const float speed = 95.0f;
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
}

namespace EnemyAI {

void Update(Enemy& enemy, Vec2 playerPos, float dt, const std::vector<Enemy>& roomEnemies,
            std::vector<Enemy>& spawnedEnemies, std::vector<Projectile>& enemyProjectiles) {
    if (!enemy.alive) return;

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
}

} // namespace EnemyAI
