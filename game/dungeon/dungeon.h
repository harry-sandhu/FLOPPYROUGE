#pragma once
#include <vector>
#include "../../engine/core/types.h"
#include "../../engine/core/rng.h"
#include "../rooms/room.h"
#include "../player/player.h"

struct DungeonSettings {
    int gridWidth = 5;
    int gridHeight = 5;
    int totalFloors = 3;
    int mainPathBase = 5;
    int mainPathPerFloor = 1;
    int extraNormalBase = 1;
    int extraNormalPerFloor = 1;
    int normalEnemyBase = 2;
    int normalEnemyPerFloor = 1;
    int deepRoomBonus = 1;
    int treasureMinItems = 1;
    int treasureMaxItems = 2;
    int curseDamage = 10;
    float specialEnemyChance = 0.05f;
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

    void PlacePlayerAtCurrentRoomCenter(Player& player) const;
    bool TryTransition(Player& player);
    void MarkCurrentRoomCleared();

private:
    DungeonSettings settings;
    std::vector<Room> rooms;
    std::vector<int> cellToRoomIndex;
    int gridWidth = 5;
    int gridHeight = 5;
    int currentRoomIndex = -1;
    int startRoomIndex = -1;
    int bossRoomIndex = -1;
    int currentFloor = 1;
    int totalFloors = 3;

    int CellIndex(int x, int y) const;
    bool InBounds(int x, int y) const;
    int RoomIndexAtCell(int x, int y) const;
    int AddRoomAtCell(int x, int y);
    void BuildConnections();
    void ApplyRoomDefaults();
    void MovePlayerIntoRoom(Player& player, int fromRoomIndex, int toRoomIndex) const;
};
