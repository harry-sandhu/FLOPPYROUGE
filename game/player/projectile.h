#pragma once
#include "../../engine/core/types.h"

struct ProjectileMods {
    bool homing = false;
    bool poison = false;
    bool sticky = false;
    bool piercing = false;
    bool explosive = false;
    bool burn = false;
    bool freeze = false;
    bool magnet = false;
    bool boomerang = false;
    bool growing = false;
    bool shrinking = false;
    bool chain = false;
    bool gravity = false;
    bool vortex = false;
    bool crit = false;
    bool lifesteal = false;
    bool marking = false;
    bool wallBounce = false;
    bool enemyBounce = false;
    bool splitOnImpact = false;
    bool ignoreBounds = false;
};

struct Projectile {
    Vec2 pos;
    Vec2 vel;
    int damage = 0;
    float remainingRange = 999999.0f;
    float maxRange = 999999.0f;
    float lifeRemaining = 0.0f;

    bool homing = false;
    bool poison = false;
    bool sticky = false;
    bool piercing = false;
    bool explosive = false;
    bool burn = false;
    bool freeze = false;
    bool magnet = false;
    bool boomerang = false;
    bool growing = false;
    bool shrinking = false;
    bool chain = false;
    bool gravity = false;
    bool vortex = false;
    bool crit = false;
    bool lifesteal = false;
    bool marking = false;
    bool wallBounce = false;
    bool enemyBounce = false;
    bool splitOnImpact = false;
    bool ignoreBounds = false;

    int pierceCount = 0;
    int wallBounceCount = 0;
    int enemyBounceCount = 0;
    int splitGeneration = 0;
    bool returning = false;
    float traveledDistance = 0.0f;
    float sizeScale = 1.0f;

    bool alive = true;

    Rect GetRect(float size) const { return { pos.x, pos.y, size, size }; }
};