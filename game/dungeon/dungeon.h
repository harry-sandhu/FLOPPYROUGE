#pragma once
#include <vector>
#include "../../engine/core/types.h"
#include "../../engine/core/rng.h"
#include "../rooms/room.h"
#include "../player/player.h"

struct DungeonSettings {
    int gridSizePerFloor = 10;
    int gridSizeMax = 30;
    int totalFloors = 5;
    int normalRoomBase = 8;
    int normalRoomPerFloor = 6;
    int normalEnemyBase = 2;
    int normalEnemyPerFloor = 1;
    int deepRoomBonus = 1;
    int treasureMinItems = 1;
    int treasureMaxItems = 2;
    float specialEnemyChance = 0.05f;
    float curseEnemyChance = 0.5f;
    float bombDropChance = 0.15f;
    float heartDropChance = 0.20f;
    float coinDropChance = 0.20f;
    float keyDropChance = 0.05f;
};

class Dungeon {
public:
    bool LoadSettings(const char* path);
    bool Generate(uint32_t seed);
    bool Generate(uint32_t seed, int floorNumber);
    bool AdvanceFloor(uint32_t seed);

    const std::vector<Room>& Rooms() const;
    Room& CurrentRoom();
    const Room& CurrentRoom() const;
    int CurrentRoomIndex() const;
    int CurrentFloor() const;
    int MaxFloors() const;
    bool HasBossRoom() const;
    bool IsFinalFloor() const;
    bool AllCombatRoomsCleared() const;
    float SpecialEnemyChance() const;
    int CurseDamage() const;             // 1 HP (half heart) on floors 1-2, 2 HP (full heart) on the final floor

    void PlacePlayerAtCurrentRoomCenter(Player& player) const;
    bool TryTransition(Player& player);
    void MarkCurrentRoomCleared(bool rollCurseReward = false);

private:
    DungeonSettings settings;
    std::vector<Room> rooms;
    std::vector<int> cellToRoomIndex;
    int gridWidth = 10;
    int gridHeight = 10;
    int currentRoomIndex = -1;
    int startRoomIndex = -1;
    int bossRoomIndex = -1;
    int currentFloor = 1;
    int totalFloors = 5;

    int CellIndex(int x, int y) const;
    bool InBounds(int x, int y) const;
    int RoomIndexAtCell(int x, int y) const;
    int AddRoomAtCell(int x, int y);
    void BuildConnections();
    void ApplyRoomDefaults();
    void MovePlayerIntoRoom(Player& player, int fromRoomIndex, int toRoomIndex) const;
};
