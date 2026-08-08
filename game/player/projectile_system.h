#pragma once
#include <vector>
#include "projectile.h"
#include "../enemies/enemy.h"

namespace ProjectileSystem {
    void Spawn(std::vector<Projectile>& projectiles, Vec2 pos, Vec2 vel);
    void UpdateAndCollide(std::vector<Projectile>& projectiles, Enemy& enemy, float projectileSize, float dt);
    void Draw(const std::vector<Projectile>& projectiles, int size);
}