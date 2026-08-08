#pragma once
#include "../../engine/core/types.h"

struct Projectile {
    Vec2 pos;
    Vec2 vel;
    bool alive = true;

    Rect GetRect(float size) const { return { pos.x, pos.y, size, size }; }
};