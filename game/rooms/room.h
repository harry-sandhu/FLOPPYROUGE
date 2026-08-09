#pragma once
#include "../../engine/core/types.h"

// Minimal room bounds for now — walls/doors/spawn points get added
// once dungeon generation exists. This just defines the playable
// rectangle so entities can't leave it.
struct Room {
    float x = 0.0f;
    float y = 0.0f;
    float width = 320.0f;
    float height = 180.0f;

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