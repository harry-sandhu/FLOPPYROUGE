#include "projectile_system.h"
#include <algorithm>
#include <cmath>
#include "../../engine/collision.h"
#include "../../engine/renderer.h"

namespace {
    // Generous off-room bounds check — internal resolution is 320x180.
    bool OutOfBounds(Vec2 pos) {
        return pos.x < -16.0f || pos.x > 336.0f || pos.y < -16.0f || pos.y > 196.0f;
    }

    void AdvanceProjectile(Projectile& p, float dt) {
        float stepX = p.vel.x * dt;
        float stepY = p.vel.y * dt;
        p.pos.x += stepX;
        p.pos.y += stepY;

        float travel = std::sqrt(stepX * stepX + stepY * stepY);
        p.remainingRange -= travel;

        if (p.remainingRange <= 0.0f || OutOfBounds(p.pos)) {
            p.alive = false;
        }
    }
}

namespace ProjectileSystem {

void Spawn(std::vector<Projectile>& projectiles, Vec2 pos, Vec2 vel, int damage, float remainingRange) {
    Projectile p;
    p.pos = pos;
    p.vel = vel;
    p.damage = damage;
    p.remainingRange = remainingRange;
    projectiles.push_back(p);
}

void UpdateAndCollideVsEnemy(std::vector<Projectile>& projectiles, Enemy& enemy, float projectileSize, float dt) {
    for (auto& p : projectiles) {
        if (!p.alive) continue;
        AdvanceProjectile(p, dt);

        if (enemy.alive && p.alive && Collision::CheckAABB(p.GetRect(projectileSize), enemy.GetRect())) {
            p.alive = false;
            enemy.hp -= p.damage;
            if (enemy.hp <= 0) enemy.alive = false;
        }
    }

    projectiles.erase(
        std::remove_if(projectiles.begin(), projectiles.end(),
            [](const Projectile& p) { return !p.alive; }),
        projectiles.end()
    );
}

void UpdateAndCollideVsPlayer(std::vector<Projectile>& projectiles, Player& player, float projectileSize,
                               float invincibleDuration, float dt) {
    for (auto& p : projectiles) {
        if (!p.alive) continue;
        AdvanceProjectile(p, dt);

        Rect projRect = p.GetRect(projectileSize);
        if (p.alive && Collision::CheckAABB(projRect, player.GetRect())) {
            p.alive = false;
            PlayerLogic::TakeDamage(player, p.damage, invincibleDuration);
        }
    }

    projectiles.erase(
        std::remove_if(projectiles.begin(), projectiles.end(),
            [](const Projectile& p) { return !p.alive; }),
        projectiles.end()
    );
}

void UpdateAndCollideVsBoss(std::vector<Projectile>& projectiles, Boss& boss, float projectileSize, float dt) {
    for (auto& p : projectiles) {
        if (!p.alive) continue;
        AdvanceProjectile(p, dt);

        if (boss.alive && p.alive && Collision::CheckAABB(p.GetRect(projectileSize), boss.GetRect())) {
            p.alive = false;
            boss.hp -= p.damage;
            if (boss.hp <= 0) { boss.hp = 0; boss.alive = false; }
        }
    }

    projectiles.erase(
        std::remove_if(projectiles.begin(), projectiles.end(),
            [](const Projectile& p) { return !p.alive; }),
        projectiles.end()
    );
}

void Draw(const std::vector<Projectile>& projectiles, int size, uint32_t color) {
    for (auto& p : projectiles) {
        Renderer::DrawRect((int)p.pos.x, (int)p.pos.y, size, size, color);
    }
}

} // namespace ProjectileSystem
