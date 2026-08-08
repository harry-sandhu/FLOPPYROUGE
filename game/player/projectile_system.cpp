#include "projectile_system.h"
#include <algorithm>
#include "../../engine/collision.h"
#include "../../engine/renderer.h"

namespace ProjectileSystem {

void Spawn(std::vector<Projectile>& projectiles, Vec2 pos, Vec2 vel) {
    Projectile p;
    p.pos = pos;
    p.vel = vel;
    projectiles.push_back(p);
}

void UpdateAndCollide(std::vector<Projectile>& projectiles, Enemy& enemy, float projectileSize, float dt) {
    for (auto& p : projectiles) {
        if (!p.alive) continue;
        p.pos.x += p.vel.x * dt;
        p.pos.y += p.vel.y * dt;
        if (p.pos.y < 0) p.alive = false;

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

void Draw(const std::vector<Projectile>& projectiles, int size) {
    for (auto& p : projectiles) {
        Renderer::DrawRect((int)p.pos.x, (int)p.pos.y, size, size, 0xFFFFFF00);
    }
}

} // namespace ProjectileSystem