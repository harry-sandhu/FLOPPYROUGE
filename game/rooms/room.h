#pragma once
#include <vector>
#include "../../engine/core/types.h"

enum class RoomType {
    START,
    NORMAL,
    BOSS,
    TREASURE,
    CURSE
};

struct Room {
    RoomType type = RoomType::NORMAL;
    float x = 0.0f;
    float y = 0.0f;
    float width = 320.0f;
    float height = 180.0f;

    Vec2 gridPos = { 0.0f, 0.0f };

    bool cleared = false;
    bool gateOpen = false;

    int north = -1;
    int south = -1;
    int east = -1;
    int west = -1;

    std::vector<int> enemySpawnList;
    std::vector<int> itemSpawnList;

    // Clamps a position (top-left of an entity's bounding box) so the
    // entity of the given size stays fully inside the room.
    Vec2 ClampToRoom(Vec2 pos, float entityWidth, float entityHeight) const {
        float minX = x;
        float minY = y;
        float maxX = x + width - entityWidth;
        float maxY = y + height - entityHeight;

        if (pos.x < minX) pos.x = minX;
        if (pos.y < minY) pos.y = minY;
        if (pos.x > maxX) pos.x = maxX;
        if (pos.y > maxY) pos.y = maxY;

        return pos;
    }
};
