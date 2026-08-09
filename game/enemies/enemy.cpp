#include "enemy.h"
#include <cmath>

namespace {
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
            Projectile p;
            p.pos = enemy.pos;
            p.vel = { dir.x * enemy.shotSpeed, dir.y * enemy.shotSpeed };
            enemyProjectiles.push_back(p);
            enemy.shootTimer = enemy.shootCooldown;
        }
    }
}

namespace EnemyAI {

void Update(Enemy& enemy, Vec2 playerPos, float dt, std::vector<Projectile>& enemyProjectiles) {
    if (!enemy.alive) return;

    switch (enemy.aiType) {
        case AIType::CHASER:
            UpdateChaser(enemy, playerPos, dt);
            break;
        case AIType::SHOOTER:
            UpdateShooter(enemy, playerPos, dt, enemyProjectiles);
            break;
        default:
            break;
    }
}

} // namespace EnemyAI