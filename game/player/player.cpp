#include "player.h"
#include "projectile_system.h"
#include "../../engine/input.h"
#include <algorithm>
#include <cmath>
#include <windows.h>

namespace {
    Vec2 Normalize(Vec2 v) {
        float lenSq = v.x * v.x + v.y * v.y;
        if (lenSq > 0.0001f) {
            float len = std::sqrt(lenSq);
            v.x /= len;
            v.y /= len;
        }
        return v;
    }

    bool BuildShootDirection(const Player& player, Vec2& outDir) {
        int dx = 0;
        int dy = 0;
        int pressedCount = 0;

        if (Input::IsDown(VK_UP))    { dy -= 1; pressedCount++; }
        if (Input::IsDown(VK_DOWN))  { dy += 1; pressedCount++; }
        if (Input::IsDown(VK_LEFT))  { dx -= 1; pressedCount++; }
        if (Input::IsDown(VK_RIGHT)) { dx += 1; pressedCount++; }

        if (pressedCount == 1) {
            outDir = { (float)dx, (float)dy };
            return true;
        }

        if (pressedCount == 2 && dx != 0 && dy != 0 && player.hasDiagonalFire) {
            outDir = Normalize({ (float)dx, (float)dy });
            return true;
        }

        return false;
    }

    void BeginDash(Player& player, Vec2 dashDir) {
        player.isDashing = true;
        player.dashDir = dashDir;
        player.dashTimeRemaining = player.dashDuration;
        player.dashCooldownRemaining = player.dashCooldown;
        player.invincibleTimer = std::max(player.invincibleTimer, player.dashDuration);
        player.actionFlashTimer = std::max(player.actionFlashTimer, 0.10f);
    }
}

namespace PlayerLogic {

void HandleMovement(Player& player, float dt) {
    if (player.hasDash && Input::IsPressed(VK_SPACE) && !player.isDashing && player.dashCooldownRemaining <= 0.0f) {
        Vec2 dashDir = player.facingDir;
        if (dashDir.x == 0.0f && dashDir.y == 0.0f) {
            dashDir = { 0.0f, -1.0f };
        }
        BeginDash(player, Normalize(dashDir));
    }

    if (player.isDashing) {
        player.pos.x += player.dashDir.x * player.dashSpeed * dt;
        player.pos.y += player.dashDir.y * player.dashSpeed * dt;
        return;
    }

    float moveX = 0.0f;
    float moveY = 0.0f;

    if (Input::IsDown('W')) moveY -= 1.0f;
    if (Input::IsDown('S')) moveY += 1.0f;
    if (Input::IsDown('A')) moveX -= 1.0f;
    if (Input::IsDown('D')) moveX += 1.0f;

    if (moveX != 0.0f || moveY != 0.0f) {
        player.facingDir = Normalize({ moveX, moveY });
    }

    player.pos.x += moveX * player.moveSpeed * dt;
    player.pos.y += moveY * player.moveSpeed * dt;
}

void HandleShooting(Player& player, float /*dt*/, std::vector<Projectile>& playerProjectiles) {
    Vec2 shootDir;
    if (!BuildShootDirection(player, shootDir)) return;

    if (player.fireCooldownRemaining > 0.0f) return;

    int projectileCount = std::max(1, player.projectileCount);
    Vec2 spawnPos = {
        player.pos.x + player.size / 2.0f,
        player.pos.y + player.size / 2.0f
    };

    for (int i = 0; i < projectileCount; ++i) {
        ProjectileSystem::Spawn(
            playerProjectiles,
            spawnPos,
            { shootDir.x * player.shotSpeed, shootDir.y * player.shotSpeed },
            player.damage,
            player.range
        );
    }

    float fireInterval = (player.fireRate > 0.0f) ? (1.0f / player.fireRate) : 0.0f;
    player.fireCooldownRemaining = fireInterval;
    player.actionFlashTimer = std::max(player.actionFlashTimer, 0.06f);
}

bool TakeDamage(Player& player, int amount, float invincibleDuration) {
    if (player.IsInvincible()) return false;
    player.hp -= amount;
    if (player.hp < 0) player.hp = 0;
    player.invincibleTimer = invincibleDuration;
    return true;
}

void UpdateTimers(Player& player, float dt) {
    if (player.invincibleTimer > 0.0f) {
        player.invincibleTimer -= dt;
        if (player.invincibleTimer < 0.0f) player.invincibleTimer = 0.0f;
    }

    if (player.fireCooldownRemaining > 0.0f) {
        player.fireCooldownRemaining -= dt;
        if (player.fireCooldownRemaining < 0.0f) player.fireCooldownRemaining = 0.0f;
    }

    if (player.dashCooldownRemaining > 0.0f) {
        player.dashCooldownRemaining -= dt;
        if (player.dashCooldownRemaining < 0.0f) player.dashCooldownRemaining = 0.0f;
    }

    if (player.actionFlashTimer > 0.0f) {
        player.actionFlashTimer -= dt;
        if (player.actionFlashTimer < 0.0f) player.actionFlashTimer = 0.0f;
    }

    if (player.isDashing && player.dashTimeRemaining > 0.0f) {
        player.dashTimeRemaining -= dt;
        if (player.dashTimeRemaining <= 0.0f) {
            player.dashTimeRemaining = 0.0f;
            player.isDashing = false;
            player.dashDir = { 0.0f, 0.0f };
        }
    }
}

} // namespace PlayerLogic
