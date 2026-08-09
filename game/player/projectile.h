#pragma once
#include "../../engine/core/types.h"

struct Projectile {
    Vec2 pos;
    Vec2 vel;
    int damage = 0;
    float remainingRange = 999999.0f;
    bool alive = true;

    Rect GetRect(float size) const { return { pos.x, pos.y, size, size }; }
};
