#pragma once
#include <vector>
#include "../../engine/core/types.h"
#include "../../engine/core/rng.h"
#include "../rooms/room.h"
#include "../player/player.h"

class Dungeon {
public:
    bool Generate(uint32_t seed);

    const std::vector<Room>& Rooms() const;
    Room& CurrentRoom();
    const Room& CurrentRoom() const;
    int CurrentRoomIndex() const;

    void PlacePlayerAtCurrentRoomCenter(Player& player) const;
    bool TryTransition(Player& player);
    void MarkCurrentRoomCleared();

private:
    std::vector<Room> rooms;
    std::vector<int> cellToRoomIndex;
    int gridWidth = 5;
    int gridHeight = 5;
    int currentRoomIndex = -1;
    int startRoomIndex = -1;
    int bossRoomIndex = -1;

    int CellIndex(int x, int y) const;
    bool InBounds(int x, int y) const;
    int RoomIndexAtCell(int x, int y) const;
    int AddRoomAtCell(int x, int y);
    void BuildConnections();
    void ApplyRoomDefaults();
    void MovePlayerIntoRoom(Player& player, int fromRoomIndex, int toRoomIndex) const;
};
