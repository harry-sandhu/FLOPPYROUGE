#pragma once
#include <vector>
#include <cstdint>
#include "projectile.h"
#include "player.h"
#include "../bosses/boss.h"
#include "../enemies/enemy.h"

namespace ProjectileSystem {
    void Spawn(std::vector<Projectile>& projectiles, Vec2 pos, Vec2 vel);

    void UpdateAndCollideVsEnemy(std::vector<Projectile>& projectiles, Enemy& enemy, float projectileSize, float dt);

    void UpdateAndCollideVsBoss(std::vector<Projectile>& projectiles, Boss& boss, float projectileSize, float dt);

    void UpdateAndCollideVsPlayer(std::vector<Projectile>& projectiles, Player& player, float projectileSize,
                                   int damage, float invincibleDuration, float dt);

    void Draw(const std::vector<Projectile>& projectiles, int size, uint32_t color);
}