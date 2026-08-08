#pragma once
#include "../../engine/core/types.h"

struct Player {
    Vec2 pos;
    int size = 10;
    int hp = 100;
    int maxHp = 100;
    float speed = 60.0f;
    float invincibleTimer = 0.0f;

    Rect GetRect() const { return { pos.x, pos.y, (float)size, (float)size }; }
    bool IsInvincible() const { return invincibleTimer > 0.0f; }
};

namespace PlayerLogic {
    void HandleMovement(Player& player, float dt);
    void TakeDamage(Player& player, int amount, float invincibleDuration);
    void UpdateTimers(Player& player, float dt);
}