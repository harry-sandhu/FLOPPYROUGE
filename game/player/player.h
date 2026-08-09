#pragma once
#include <vector>
#include "../../engine/core/types.h"
#include "projectile.h"

struct Player {
    static constexpr int MAX_OWNED_ITEMS = 32;

    Vec2 pos;
    int size = 10;

    int damage = 10;
    float shotSpeed = 140.0f;
    float range = 150.0f;
    float fireRate = 3.0f;
    int projectileCount = 1;
    float moveSpeed = 60.0f;
    int luck = 0;
    int hp = 6;
    int maxHp = 6;

    float dashSpeed = 180.0f;
    float dashDuration = 0.12f;
    float dashCooldown = 0.8f;

    float homingChance = 0.0f;
    float poisonChance = 0.0f;
    float stickyChance = 0.0f;
    float piercingChance = 0.0f;
    float explosiveChance = 0.0f;

    float invincibleTimer = 0.0f;
    float fireCooldownRemaining = 0.0f;
    float dashCooldownRemaining = 0.0f;
    float dashTimeRemaining = 0.0f;
    float actionFlashTimer = 0.0f;
    float pickupMessageTimer = 0.0f;
    char pickupName[32] = {};

    bool hasDiagonalFire = false;
    bool hasDash = false;
    bool hasHomingShots = false;
    bool isDashing = false;

    int ownedItemIds[MAX_OWNED_ITEMS] = {};
    int ownedItemCount = 0;

    Vec2 facingDir = { 0.0f, -1.0f };
    Vec2 dashDir = { 0.0f, 0.0f };

    Rect GetRect() const { return { pos.x, pos.y, (float)size, (float)size }; }
    bool IsInvincible() const { return invincibleTimer > 0.0f; }
};

namespace PlayerLogic {
    void HandleMovement(Player& player, float dt);
    void HandleShooting(Player& player, float dt, std::vector<Projectile>& playerProjectiles);
    bool TakeDamage(Player& player, int amount, float invincibleDuration);
    void UpdateTimers(Player& player, float dt);
}
