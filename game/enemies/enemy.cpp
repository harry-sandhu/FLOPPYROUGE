#include "enemy.h"
#include <cmath>

namespace {
    void UpdateChaser(Enemy& enemy, Vec2 playerPos, float dt) {
        Vec2 toPlayer = { playerPos.x - enemy.pos.x, playerPos.y - enemy.pos.y };
        float dist = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.y * toPlayer.y);
        if (dist > 0.01f) {
            toPlayer.x /= dist;
            toPlayer.y /= dist;
            enemy.pos.x += toPlayer.x * enemy.speed * dt;
            enemy.pos.y += toPlayer.y * enemy.speed * dt;
        }
    }
}

namespace EnemyAI {

void Update(Enemy& enemy, Vec2 playerPos, float dt) {
    if (!enemy.alive) return;

    switch (enemy.aiType) {
        case AIType::CHASER:
            UpdateChaser(enemy, playerPos, dt);
            break;
        // SHOOTER, CHARGER, SUMMONER, EXPLODER added as they're built —
        // each just gets its own Update* function and a case here.
        default:
            break;
    }
}

} // namespace EnemyAI