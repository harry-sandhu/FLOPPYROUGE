#pragma once

#include <vector>

#include "../rooms/room.h"

namespace TerrainGen {
    std::vector<RoomTerrainFeature> GenerateCellularTerrain(const Room& room, int floor);
    std::vector<RoomTerrainFeature> GenerateGridRuleTerrain(const Room& room, int floor);
    std::vector<RoomTerrainFeature> GenerateArchetypeTerrain(const Room& room, int floor);
}
