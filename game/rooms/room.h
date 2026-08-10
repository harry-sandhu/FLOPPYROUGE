#pragma once
#include <vector>
#include <cmath>
#include "../../engine/core/types.h"

enum class RoomType {
    START,
    NORMAL,
    BOSS,
    TREASURE,
    CURSE
};

enum class RoomPickupType {
    ITEM,
    EXIT,
    TROPHY,
    HEART,
    BOMB
};

struct RoomPickup {
    RoomPickupType type = RoomPickupType::ITEM;
    int itemId = -1;
    Vec2 pos = { 0.0f, 0.0f };
    bool collected = false;
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
    bool lootGranted = false;

    int north = -1;
    int south = -1;
    int east = -1;
    int west = -1;

    int bossVariant = 0;
    int targetDegree = 0;

    std::vector<int> enemySpawnList;
    std::vector<int> itemSpawnList;
    std::vector<RoomPickup> pickups;

    bool IsEnemyCurseRoom() const {
        return type == RoomType::CURSE && !enemySpawnList.empty();
    }

    // Wall thickness and door-gap width, in the same 320x180 internal-pixel
    // space everything else is drawn in. Kept here (not in main.cpp) so the
    // renderer and the collision/clamp logic can never disagree about where
    // the door actually is.
    static constexpr float WALL_THICKNESS = 6.0f;
    static constexpr float DOOR_HALF_WIDTH = 14.0f;

    // Strict inner-room clamp - always solid on all four sides, regardless
    // of doors/gates. Used for enemies/bosses/summoned adds, which never
    // need to leave the room they spawned in.
    Vec2 ClampToRoom(Vec2 pos, float entityWidth, float entityHeight) const {
        float minX = x + WALL_THICKNESS;
        float minY = y + WALL_THICKNESS;
        float maxX = x + width - WALL_THICKNESS - entityWidth;
        float maxY = y + height - WALL_THICKNESS - entityHeight;

        if (pos.x < minX) pos.x = minX;
        if (pos.y < minY) pos.y = minY;
        if (pos.x > maxX) pos.x = maxX;
        if (pos.y > maxY) pos.y = maxY;

        return pos;
    }

    bool InDoorLaneHorizontal(float centerY) const {
        return std::fabs(centerY - (y + height * 0.5f)) <= DOOR_HALF_WIDTH;
    }

    bool InDoorLaneVertical(float centerX) const {
        return std::fabs(centerX - (x + width * 0.5f)) <= DOOR_HALF_WIDTH;
    }

    // Player-only clamp: solid walls everywhere except through an open,
    // connected door's gap, where the player is allowed to walk out to the
    // true room edge (and slightly past it, which is what lets
    // Dungeon::TryTransition detect the crossing).
    Vec2 ClampPlayerToRoom(Vec2 pos, float entityWidth, float entityHeight) const {
        float centerX = pos.x + entityWidth * 0.5f;
        float centerY = pos.y + entityHeight * 0.5f;

        bool westOpen  = (west  >= 0) && gateOpen && InDoorLaneHorizontal(centerY);
        bool eastOpen  = (east  >= 0) && gateOpen && InDoorLaneHorizontal(centerY);
        bool northOpen = (north >= 0) && gateOpen && InDoorLaneVertical(centerX);
        bool southOpen = (south >= 0) && gateOpen && InDoorLaneVertical(centerX);

        float minX = westOpen  ? x                            : x + WALL_THICKNESS;
        float maxX = (eastOpen ? x + width                    : x + width - WALL_THICKNESS) - entityWidth;
        float minY = northOpen ? y                             : y + WALL_THICKNESS;
        float maxY = (southOpen ? y + height                  : y + height - WALL_THICKNESS) - entityHeight;

        if (pos.x < minX) pos.x = minX;
        if (pos.y < minY) pos.y = minY;
        if (pos.x > maxX) pos.x = maxX;
        if (pos.y > maxY) pos.y = maxY;

        return pos;
    }
};