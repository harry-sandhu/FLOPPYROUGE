#pragma once
#include <vector>
#include <cmath>
#include "../../engine/core/types.h"

enum class RoomType {
    START,
    NORMAL,
    BOSS,
    TREASURE,
    CURSE,
    SHOP
};

enum class RoomPickupType {
    ITEM,
    EXIT,
    TROPHY,
    HEART,
    BOMB,
    COIN,
    KEY,
    CHEST
};

enum class ChestType {
    WOODEN,
    IRON,
    STONE,
    GOLDEN,
    DEVIL,
    ANGEL,
    GAMBLE
};

enum class RoomTerrainType {
    ROCK_BOMBABLE_COIN,
    ROCK_BOMBABLE_HEART,
    ROCK_BOMBABLE,
    ROCK_INDESTRUCTIBLE,
    ROCK_EXPLOSIVE,
    CRATE_DESTRUCTIBLE,
    BLOCK_PUSHABLE,
    BRIDGE_TEMPORARY,
    BRIDGE_FRAGILE,
    TELEPORT_PAD,
    PRESSURE_PLATE,
    LILY_PAD,
    TERRAIN_WEB,
    TERRAIN_SLIME,
    TERRAIN_MUD,
    TERRAIN_FIRE,
    TERRAIN_ACID,
    TERRAIN_POISON,
    PIT,
    TRAP_POISON,
    TRAP_TELEPORT,
    TRAP_SUMMON,
    TRAP_SPIKE
};

enum class RoomArchetype {
    OPEN_ARENA,
    PILLAR_FIELD,
    BROKEN_ARENA,
    GAUNTLET,
    HAZARD_ROOM,
    RITUAL_ROOM,
    WHISPERING_STACKS,
    RUNIC_LATTICE,
    FORKING_PATH,
    SPIRE_ASCENT,
    FLOODED_CHAMBER,
    COLLAPSED_VAULT,
    SENTRY_HALL,
    GARDEN_MAZE,
    SHATTERED_BRIDGE,
    ECHO_ROOM,
    THRONE_APPROACH,
    TWIN_ISLANDS,
    NARROW_VEINS,
    AMPHITHEATER
};

enum class RoomEncounterFamily {
    RUSH,
    ARTILLERY,
    SWARM,
    GUARDIAN,
    AMBUSH,
    MIXED,
    ELITE
};

enum class DungeonTheme {
    RUINS,
    FORGE,
    CRYPT,
    FUNGAL,
    DRACONIC
};

struct TerrainTraversalProfile {
    bool canFly = false;
    bool canCrossPits = false;
    bool immuneToHazards = false;
};

struct RoomTerrainFeature {
    Vec2 pos = { 0.0f, 0.0f };
    float w = 10.0f;
    float h = 10.0f;
    RoomTerrainType type = RoomTerrainType::ROCK_BOMBABLE_COIN;
    int rewardAmount = 1;
    int featureId = -1;
    int linkId = -1;
    int hitPoints = 1;
    int usesRemaining = 0;
    float timer = 0.0f;
    bool broken = false;
    bool triggered = false;
    bool active = true;

    Rect GetRect() const { return { pos.x, pos.y, w, h }; }
    bool BlocksMovement() const {
        return type == RoomTerrainType::ROCK_BOMBABLE_COIN ||
               type == RoomTerrainType::ROCK_BOMBABLE_HEART ||
               type == RoomTerrainType::ROCK_BOMBABLE ||
               type == RoomTerrainType::ROCK_INDESTRUCTIBLE ||
               type == RoomTerrainType::ROCK_EXPLOSIVE ||
               type == RoomTerrainType::CRATE_DESTRUCTIBLE ||
               type == RoomTerrainType::BLOCK_PUSHABLE;
    }
    bool IsBombable() const {
        return type == RoomTerrainType::ROCK_BOMBABLE_COIN ||
               type == RoomTerrainType::ROCK_BOMBABLE_HEART ||
               type == RoomTerrainType::ROCK_BOMBABLE ||
               type == RoomTerrainType::CRATE_DESTRUCTIBLE ||
               type == RoomTerrainType::ROCK_EXPLOSIVE;
    }
    bool IsTrap() const {
        return type == RoomTerrainType::TRAP_POISON ||
               type == RoomTerrainType::TRAP_TELEPORT ||
               type == RoomTerrainType::TRAP_SUMMON ||
               type == RoomTerrainType::TRAP_SPIKE;
    }
    bool IsHazard() const {
        return type == RoomTerrainType::PIT ||
               IsTrap() ||
               type == RoomTerrainType::TERRAIN_WEB ||
               type == RoomTerrainType::TERRAIN_SLIME ||
               type == RoomTerrainType::TERRAIN_MUD ||
               type == RoomTerrainType::TERRAIN_FIRE ||
               type == RoomTerrainType::TERRAIN_ACID ||
               type == RoomTerrainType::TERRAIN_POISON;
    }

    bool IsPushable() const {
        return type == RoomTerrainType::BLOCK_PUSHABLE;
    }

    bool IsExplosive() const {
        return type == RoomTerrainType::ROCK_EXPLOSIVE;
    }

    bool IsTraversalAid() const {
        return type == RoomTerrainType::BRIDGE_TEMPORARY ||
               type == RoomTerrainType::BRIDGE_FRAGILE ||
               type == RoomTerrainType::TELEPORT_PAD ||
               type == RoomTerrainType::PRESSURE_PLATE ||
               type == RoomTerrainType::LILY_PAD;
    }

    bool IsSlowTerrain() const {
        return type == RoomTerrainType::TERRAIN_WEB ||
               type == RoomTerrainType::TERRAIN_SLIME ||
               type == RoomTerrainType::TERRAIN_MUD;
    }

    bool IsDamageTerrain() const {
        return type == RoomTerrainType::TERRAIN_FIRE ||
               type == RoomTerrainType::TERRAIN_ACID ||
               type == RoomTerrainType::TERRAIN_POISON;
    }

    bool IsLinked() const {
        return linkId >= 0;
    }
};

struct RoomPickup {
    RoomPickupType type = RoomPickupType::ITEM;
    int itemId = -1;
    int amount = 0;
    int cost = 0;
    ChestType chestType = ChestType::WOODEN;
    int chestAttempts = 0;
    Vec2 pos = { 0.0f, 0.0f };
    bool collected = false;
    bool isMimic = false;
};

struct Room {
    RoomType type = RoomType::NORMAL;
    float x = 0.0f;
    float y = 0.0f;
    float width = 320.0f;
    float height = 180.0f;
    float timeInRoom = 0.0f;
    bool pacingReinforcementSent = false;

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
    RoomArchetype archetype = RoomArchetype::OPEN_ARENA;
    RoomEncounterFamily encounterFamily = RoomEncounterFamily::MIXED;
    DungeonTheme theme = DungeonTheme::RUINS;
    int threatBudget = 0;
    int threatSpent = 0;

    std::vector<int> enemySpawnList;
    std::vector<int> itemSpawnList;
    std::vector<RoomPickup> pickups;
    std::vector<RoomTerrainFeature> terrain;

    bool IsEnemyCurseRoom() const {
        return type == RoomType::CURSE && !enemySpawnList.empty();
    }

    bool CanTraverseTerrain(const RoomTerrainFeature& feature, const TerrainTraversalProfile& profile) const {
        if (feature.type == RoomTerrainType::PIT) {
            return profile.canFly || profile.canCrossPits;
        }
        if (feature.IsTrap()) {
            return true;
        }
        if (feature.IsTraversalAid()) {
            return feature.active;
        }
        if (feature.IsDamageTerrain() || feature.IsSlowTerrain()) {
            return true;
        }
        if (feature.IsBombable() || feature.type == RoomTerrainType::ROCK_INDESTRUCTIBLE || feature.IsPushable()) {
            return false;
        }
        return true;
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
