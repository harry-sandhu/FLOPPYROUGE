#include "projectile_system.h"
#include <algorithm>
#include "../../engine/collision.h"
#include "../../engine/renderer.h"

namespace {
    // Generous off-room bounds check — internal resolution is 320x180.
    bool OutOfBounds(Vec2 pos) {
        return pos.x < -16.0f || pos.x > 336.0f || pos.y < -16.0f || pos.y > 196.0f;
    }
}

namespace ProjectileSystem {

void Spawn(std::vector<Projectile>& projectiles, Vec2 pos, Vec2 vel) {
    Projectile p;
    p.pos = pos;
    p.vel = vel;
    projectiles.push_back(p);
}

void UpdateAndCollideVsEnemy(std::vector<Projectile>& projectiles, Enemy& enemy, float projectileSize, float dt) {
    for (auto& p : projectiles) {
        if (!p.alive) continue;
        p.pos.x += p.vel.x * dt;
        p.pos.y += p.vel.y * dt;
        if (OutOfBounds(p.pos)) p.alive = false;

        if (enemy.alive && Collision::CheckAABB(p.GetRect(projectileSize), enemy.GetRect())) {
            p.alive = false;
            enemy.hp -= 10;
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
                               int damage, float invincibleDuration, float dt) {
    for (auto& p : projectiles) {
        if (!p.alive) continue;
        p.pos.x += p.vel.x * dt;
        p.pos.y += p.vel.y * dt;
        if (OutOfBounds(p.pos)) p.alive = false;

        Rect projRect = p.GetRect(projectileSize);
        if (Collision::CheckAABB(projRect, player.GetRect())) {
            p.alive = false;
            PlayerLogic::TakeDamage(player, damage, invincibleDuration);
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
        p.pos.x += p.vel.x * dt;
        p.pos.y += p.vel.y * dt;
        if (OutOfBounds(p.pos)) p.alive = false;

        if (boss.alive && Collision::CheckAABB(p.GetRect(projectileSize), boss.GetRect())) {
            p.alive = false;
            boss.hp -= 10;
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