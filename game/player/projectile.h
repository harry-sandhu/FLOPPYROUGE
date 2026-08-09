#pragma once
#include "../../engine/core/types.h"

struct Projectile {
    Vec2 pos;
    Vec2 vel;
    int damage = 0;
    float remainingRange = 999999.0f;
    float lifeRemaining = 0.0f;
    bool homing = false;
    bool poison = false;
    bool sticky = false;
    bool piercing = false;
    bool explosive = false;
    int pierceCount = 0;
    bool alive = true;

    Rect GetRect(float size) const { return { pos.x, pos.y, size, size }; }
};
