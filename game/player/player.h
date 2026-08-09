#pragma once
#include <vector>
#include "../../engine/core/types.h"
#include "projectile.h"

struct Player {
    Vec2 pos;
    int size = 10;

    int damage = 10;
    float shotSpeed = 140.0f;
    float range = 150.0f;
    float fireRate = 3.0f;
    int projectileCount = 1;
    float moveSpeed = 60.0f;
    int hp = 100;
    int maxHp = 100;

    float dashSpeed = 180.0f;
    float dashDuration = 0.12f;
    float dashCooldown = 0.8f;

    float invincibleTimer = 0.0f;
    float fireCooldownRemaining = 0.0f;
    float dashCooldownRemaining = 0.0f;
    float dashTimeRemaining = 0.0f;

    bool hasDiagonalFire = false;
    bool hasDash = false;
    bool isDashing = false;

    Vec2 facingDir = { 0.0f, -1.0f };
    Vec2 dashDir = { 0.0f, 0.0f };

    Rect GetRect() const { return { pos.x, pos.y, (float)size, (float)size }; }
    bool IsInvincible() const { return invincibleTimer > 0.0f; }
};

namespace PlayerLogic {
    void HandleMovement(Player& player, float dt);
    void HandleShooting(Player& player, float dt, std::vector<Projectile>& playerProjectiles);
    void TakeDamage(Player& player, int amount, float invincibleDuration);
    void UpdateTimers(Player& player, float dt);
}
