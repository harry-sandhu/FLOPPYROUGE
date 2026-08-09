#pragma once
#include <vector>
#include <cstdint>
#include "projectile.h"
#include "player.h"
#include "../bosses/boss.h"
#include "../enemies/enemy.h"

namespace ProjectileSystem {
    void Spawn(std::vector<Projectile>& projectiles, Vec2 pos, Vec2 vel, int damage = 0,
               float remainingRange = 999999.0f, float lifeRemaining = 0.0f);
    void Advance(std::vector<Projectile>& projectiles, float dt);

    void UpdateAndCollideVsEnemy(std::vector<Projectile>& projectiles, Enemy& enemy, float projectileSize,
                                 std::vector<Enemy>& spawnedEnemies, std::vector<Projectile>& enemyProjectiles,
                                 float dt);

    void UpdateAndCollideVsBoss(std::vector<Projectile>& projectiles, Boss& boss, float projectileSize, float dt);

    void UpdateAndCollideVsPlayer(std::vector<Projectile>& projectiles, Player& player, float projectileSize,
                                   float invincibleDuration, float dt);

    void Draw(const std::vector<Projectile>& projectiles, int size, uint32_t color);
}
