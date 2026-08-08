#pragma once
#include "../../engine/core/types.h"

enum class AIType {
    CHASER,
    SHOOTER,
    CHARGER,
    SUMMONER,
    EXPLODER
};

struct Enemy {
    Vec2 pos;
    float w = 12.0f, h = 12.0f;
    float speed = 40.0f;
    int hp = 30;
    int maxHp = 30;
    AIType aiType = AIType::CHASER;
    bool alive = true;

    Rect GetRect() const { return { pos.x, pos.y, w, h }; }
};

namespace EnemyAI {
    // Advances the enemy one frame according to its aiType.
    void Update(Enemy& enemy, Vec2 playerPos, float dt);
}