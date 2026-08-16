#include "dungeon.h"
#include <algorithm>
#include <climits>
#include <queue>
#include <cstring>
#include "../../engine/data_parser.h"
#include "../enemies/enemy_database.h"
#include "../items/item_database.h"

namespace {
    constexpr int MAX_GENERATION_ATTEMPTS = 64;
    constexpr int BOSS_VARIANT_TIER[10] = { 1, 1, 2, 2, 3, 2, 2, 3, 2, 3 };

    ChestType RollChestTypeForFloor(int floor) {
        int roll = RNG::Range(0, 99);
        if (floor <= 1) {
            if (roll < 70) return ChestType::WOODEN;
            if (roll < 90) return ChestType::IRON;
            return ChestType::GOLDEN;
        }
        if (floor == 2) {
            if (roll < 35) return ChestType::WOODEN;
            if (roll < 60) return ChestType::IRON;
            if (roll < 80) return ChestType::STONE;
            if (roll < 92) return ChestType::GOLDEN;
            return ChestType::DEVIL;
        }
        if (roll < 20) return ChestType::STONE;
        if (roll < 42) return ChestType::GOLDEN;
        if (roll < 66) return ChestType::DEVIL;
        if (roll < 88) return ChestType::ANGEL;
        return ChestType::IRON;
    }

    // Normal-room connection-degree distribution: 10% / 50% / 30% / 10%
    // for 1 / 2 / 3 / 4 connections. START/BOSS/TREASURE/CURSE are excluded.
    int RollNormalRoomTargetDegree() {
        int roll = RNG::Range(0, 99);
        if (roll < 10) return 1;   // 10%
        if (roll < 60) return 2;   // +50% -> 60
        if (roll < 90) return 3;   // +30% -> 90
        return 4;                  // remaining 10%
    }

    // ... existing DoorDir, TransitionCandidate, ComputeDistances stay as-is

    enum class DoorDir {
        NORTH,
        SOUTH,
        EAST,
        WEST
    };

    struct TransitionCandidate {
        DoorDir dir;
        int nextIndex = -1;
        float overflow = 0.0f;
    };

    std::vector<int> ComputeDistances(const std::vector<Room>& rooms, int startIndex) {
        std::vector<int> dist(rooms.size(), -1);
        if (startIndex < 0 || startIndex >= (int)rooms.size()) return dist;

        std::queue<int> q;
        dist[startIndex] = 0;
        q.push(startIndex);

        while (!q.empty()) {
            int index = q.front();
            q.pop();
            const Room& room = rooms[index];

            const int neighbors[4] = { room.north, room.south, room.east, room.west };
            for (int next : neighbors) {
                if (next < 0 || next >= (int)rooms.size()) continue;
                if (dist[next] != -1) continue;
                dist[next] = dist[index] + 1;
                q.push(next);
            }
        }

        return dist;
    }
}

bool Dungeon::LoadSettings(const char* path) {
    std::vector<DataBlock> blocks = DataParser::ParseFile(path);
    if (blocks.empty()) return false;

    bool loaded = false;
    for (const DataBlock& block : blocks) {
        if (std::strcmp(block.name, "Dungeon") != 0) continue;

        settings.gridSizePerFloor = std::max(1, block.GetInt("grid_size_per_floor", settings.gridSizePerFloor));
        settings.gridSizeMax = std::max(1, block.GetInt("grid_size_max", settings.gridSizeMax));
        settings.totalFloors = std::max(1, block.GetInt("total_floors", settings.totalFloors));
        settings.normalRoomBase = std::max(3, block.GetInt("normal_room_base", settings.normalRoomBase));
        settings.normalRoomPerFloor = std::max(0, block.GetInt("normal_room_per_floor", settings.normalRoomPerFloor));
        settings.normalEnemyBase = std::max(0, block.GetInt("normal_enemy_base", settings.normalEnemyBase));
        settings.normalEnemyPerFloor = std::max(0, block.GetInt("normal_enemy_per_floor", settings.normalEnemyPerFloor));
        settings.deepRoomBonus = std::max(0, block.GetInt("deep_room_bonus", settings.deepRoomBonus));
        settings.treasureMinItems = std::max(0, block.GetInt("treasure_min_items", settings.treasureMinItems));
        settings.treasureMaxItems = std::max(settings.treasureMinItems, block.GetInt("treasure_max_items", settings.treasureMaxItems));
        settings.specialEnemyChance = std::clamp(block.GetFloat("special_enemy_chance", settings.specialEnemyChance), 0.0f, 1.0f);
        settings.curseEnemyChance = std::clamp(block.GetFloat("curse_enemy_chance", settings.curseEnemyChance), 0.0f, 1.0f);
        settings.bombDropChance = std::clamp(block.GetFloat("bomb_drop_chance", settings.bombDropChance), 0.0f, 1.0f);
        settings.heartDropChance = std::clamp(block.GetFloat("heart_drop_chance", settings.heartDropChance), 0.0f, 1.0f);
        settings.coinDropChance = std::clamp(block.GetFloat("coin_drop_chance", settings.coinDropChance), 0.0f, 1.0f);
        settings.keyDropChance = std::clamp(block.GetFloat("key_drop_chance", settings.keyDropChance), 0.0f, 1.0f);
        loaded = true;
        break;
    }

    if (loaded) {
        totalFloors = settings.totalFloors;
    }

    return loaded;
}

int Dungeon::CellIndex(int x, int y) const {
    return y * gridWidth + x;
}

bool Dungeon::InBounds(int x, int y) const {
    return x >= 0 && y >= 0 && x < gridWidth && y < gridHeight;
}

int Dungeon::RoomIndexAtCell(int x, int y) const {
    if (!InBounds(x, y)) return -1;
    return cellToRoomIndex[CellIndex(x, y)];
}

int Dungeon::AddRoomAtCell(int x, int y) {
    Room room;
    room.x = 0.0f;
    room.y = 0.0f;
    room.width = 320.0f;
    room.height = 180.0f;
    room.gridPos = { (float)x, (float)y };

    rooms.push_back(room);
    int index = (int)rooms.size() - 1;
    cellToRoomIndex[CellIndex(x, y)] = index;
    return index;
}

void Dungeon::BuildConnections() {
    for (auto& room : rooms) {
        room.north = room.south = room.east = room.west = -1;
    }

    for (int y = 0; y < gridHeight; ++y) {
        for (int x = 0; x < gridWidth; ++x) {
            int index = RoomIndexAtCell(x, y);
            if (index < 0) continue;

            Room& room = rooms[index];
            if (RoomIndexAtCell(x, y - 1) >= 0) room.north = RoomIndexAtCell(x, y - 1);
            if (RoomIndexAtCell(x, y + 1) >= 0) room.south = RoomIndexAtCell(x, y + 1);
            if (RoomIndexAtCell(x + 1, y) >= 0) room.east = RoomIndexAtCell(x + 1, y);
            if (RoomIndexAtCell(x - 1, y) >= 0) room.west = RoomIndexAtCell(x - 1, y);
        }
    }
}

void Dungeon::ApplyRoomDefaults() {
    for (auto& room : rooms) {
        switch (room.type) {
            case RoomType::START:
            case RoomType::TREASURE:
            case RoomType::SHOP:
                room.cleared = true;
                room.gateOpen = true;
                room.lootGranted = false;
                break;
            case RoomType::CURSE:
                if (room.IsEnemyCurseRoom()) {
                    room.cleared = false;
                    room.gateOpen = false;
                } else {
                    room.cleared = true;
                    room.gateOpen = true;
                }
                room.lootGranted = false;
                break;
            case RoomType::NORMAL:
            case RoomType::BOSS:
            default:
                room.cleared = false;
                room.gateOpen = false;
                room.lootGranted = false;
                break;
        }
    }
}

void Dungeon::MovePlayerIntoRoom(Player& player, int fromRoomIndex, int toRoomIndex) const {
    if (fromRoomIndex < 0 || toRoomIndex < 0 || fromRoomIndex >= (int)rooms.size() || toRoomIndex >= (int)rooms.size()) {
        return;
    }

    const Room& from = rooms[fromRoomIndex];
    const Room& to = rooms[toRoomIndex];
    const float inset = 1.0f;

    if (to.gridPos.x > from.gridPos.x) {
        player.pos.x = to.x + inset;
        player.pos.y = to.y + (to.height - player.size) * 0.5f;
    } else if (to.gridPos.x < from.gridPos.x) {
        player.pos.x = to.x + to.width - player.size - inset;
        player.pos.y = to.y + (to.height - player.size) * 0.5f;
    } else if (to.gridPos.y > from.gridPos.y) {
        player.pos.x = to.x + (to.width - player.size) * 0.5f;
        player.pos.y = to.y + inset;
    } else if (to.gridPos.y < from.gridPos.y) {
        player.pos.x = to.x + (to.width - player.size) * 0.5f;
        player.pos.y = to.y + to.height - player.size - inset;
    }
}

bool Dungeon::Generate(uint32_t seed) {
    return Generate(seed, 1);
}

bool Dungeon::Generate(uint32_t seed, int floorNumber) {
    RNG::Seed(seed);
    currentFloor = std::max(1, std::min(floorNumber, totalFloors));

    int gridSize = std::min(currentFloor * settings.gridSizePerFloor, settings.gridSizeMax);
    gridWidth = gridSize;
    gridHeight = gridSize;

    const int normalRoomTarget = settings.normalRoomBase + settings.normalRoomPerFloor * currentFloor;

    for (int attempt = 0; attempt < MAX_GENERATION_ATTEMPTS; ++attempt) {
        rooms.clear();
        rooms.reserve(gridWidth * gridHeight);
        cellToRoomIndex.assign(gridWidth * gridHeight, -1);
        currentRoomIndex = -1;
        startRoomIndex = -1;
        bossRoomIndex = -1;

        int centerX = gridWidth / 2;
        int centerY = gridHeight / 2;
        startRoomIndex = AddRoomAtCell(centerX, centerY);
        rooms[startRoomIndex].type = RoomType::START;

        std::vector<int> normalRoomIndices;
        int addedNormals = 0;
        int growthAttempts = 0;
        const int maxGrowthAttempts = normalRoomTarget * 20;

        // Degree bookkeeping. Index-aligned with `rooms`. START room (index
        // startRoomIndex) is intentionally left unconstrained/unused here -
        // it's always a valid growth parent regardless of how many rooms
        // end up touching it.
        std::vector<int> roomDegree(rooms.size(), 0);
        std::vector<int> roomTargetDegree(rooms.size(), 0);

        while (addedNormals < normalRoomTarget && growthAttempts < maxGrowthAttempts) {
            growthAttempts++;

            // Eligible parents: START (always) + any normal room that hasn't
            // reached its rolled target degree yet. Once a room hits its
            // target it drops out of the pool, though it can still passively
            // gain +1 degree later if another room grows toward it from the
            // other side (soft floor, not a hard ceiling).
            std::vector<int> eligibleParents;
            eligibleParents.push_back(startRoomIndex);
            for (int idx : normalRoomIndices) {
                if (roomDegree[idx] < roomTargetDegree[idx]) {
                    eligibleParents.push_back(idx);
                }
            }

            int parentIndex = eligibleParents[RNG::Range(0, (int)eligibleParents.size() - 1)];

            const Room& parent = rooms[parentIndex];
            int px = (int)parent.gridPos.x;
            int py = (int)parent.gridPos.y;

            std::vector<Vec2> candidates;
            if (InBounds(px + 1, py) && RoomIndexAtCell(px + 1, py) < 0) candidates.push_back({ (float)(px + 1), (float)py });
            if (InBounds(px - 1, py) && RoomIndexAtCell(px - 1, py) < 0) candidates.push_back({ (float)(px - 1), (float)py });
            if (InBounds(px, py + 1) && RoomIndexAtCell(px, py + 1) < 0) candidates.push_back({ (float)px, (float)(py + 1) });
            if (InBounds(px, py - 1) && RoomIndexAtCell(px, py - 1) < 0) candidates.push_back({ (float)px, (float)(py - 1) });

            if (candidates.empty()) continue;

            const Vec2& cell = candidates[RNG::Range(0, (int)candidates.size() - 1)];
            int roomIndex = AddRoomAtCell((int)cell.x, (int)cell.y);
            rooms[roomIndex].type = RoomType::NORMAL;
            normalRoomIndices.push_back(roomIndex);
            addedNormals++;

            // Grow the bookkeeping vectors and roll this room's target degree.
            roomDegree.resize(rooms.size(), 0);
            roomTargetDegree.resize(rooms.size(), 0);
            roomTargetDegree[roomIndex] = RollNormalRoomTargetDegree();
            rooms[roomIndex].targetDegree = roomTargetDegree[roomIndex];

            // BuildConnections() later links ANY grid-adjacent rooms, not
            // just parent/child pairs, so credit degree to every neighbor
            // that already exists at this cell - not just the chosen parent.
            int nx = (int)cell.x, ny = (int)cell.y;
            const int neighborCells[4][2] = {
                { nx + 1, ny }, { nx - 1, ny }, { nx, ny + 1 }, { nx, ny - 1 }
            };
            for (auto& nc : neighborCells) {
                int neighborRoom = RoomIndexAtCell(nc[0], nc[1]);
                if (neighborRoom >= 0 && neighborRoom != roomIndex) {
                    roomDegree[roomIndex]++;
                    roomDegree[neighborRoom]++;
                }
            }
        }

        if (addedNormals < normalRoomTarget) continue;

        BuildConnections();

        std::vector<int> degree(rooms.size(), 0);
        for (int i = 0; i < (int)rooms.size(); ++i) {
            const Room& r = rooms[i];
            if (r.north >= 0) degree[i]++;
            if (r.south >= 0) degree[i]++;
            if (r.east >= 0) degree[i]++;
            if (r.west >= 0) degree[i]++;
        }

        std::vector<int> distFromStart = ComputeDistances(rooms, startRoomIndex);

        std::vector<int> deadEnds;
        for (int i = 0; i < (int)rooms.size(); ++i) {
            if (i == startRoomIndex) continue;
            if (degree[i] == 1) deadEnds.push_back(i);
        }

        if ((int)deadEnds.size() < 4) continue;

        bossRoomIndex = deadEnds[0];
        int bestDist = distFromStart[bossRoomIndex];
        for (int idx : deadEnds) {
            if (distFromStart[idx] > bestDist) {
                bestDist = distFromStart[idx];
                bossRoomIndex = idx;
            }
        }
        rooms[bossRoomIndex].type = RoomType::BOSS;

        std::vector<int> eligibleBossVariants;
        for (int v = 0; v < 10; ++v) {
            if (BOSS_VARIANT_TIER[v] <= currentFloor) {
                eligibleBossVariants.push_back(v);
            }
        }
        
        rooms[bossRoomIndex].bossVariant =
            eligibleBossVariants[RNG::Range(0, (int)eligibleBossVariants.size() - 1)];

        std::vector<int> remaining;
        for (int idx : deadEnds) {
            if (idx != bossRoomIndex) remaining.push_back(idx);
        }

        if ((int)remaining.size() < 3) continue;

        int treasureIndex = remaining[RNG::Range(0, (int)remaining.size() - 1)];
        int curseIndex = -1;
        int shopIndex = -1;
        for (int tries = 0; tries < 32 && (curseIndex < 0 || shopIndex < 0); ++tries) {
            int candidate = remaining[RNG::Range(0, (int)remaining.size() - 1)];
            if (candidate == treasureIndex) continue;
            if (curseIndex < 0) {
                curseIndex = candidate;
                continue;
            }
            if (candidate != curseIndex) {
                shopIndex = candidate;
            }
        }
        if (curseIndex < 0 || shopIndex < 0 || shopIndex == treasureIndex || shopIndex == curseIndex) continue;

        rooms[treasureIndex].type = RoomType::TREASURE;
        rooms[curseIndex].type = RoomType::CURSE;
        rooms[shopIndex].type = RoomType::SHOP;

        const int enemyCount = EnemyDatabase::Count();
        const int itemCount = ItemDatabase::Count();

        std::vector<int> eligibleEnemyIndices;
        for (int e = 0; e < enemyCount; ++e) {
            if (EnemyDatabase::Tier(e) <= currentFloor) eligibleEnemyIndices.push_back(e);
        }
        if (eligibleEnemyIndices.empty()) {
            for (int e = 0; e < enemyCount; ++e) eligibleEnemyIndices.push_back(e); // safety fallback
        }

        auto PickEnemyIndex = [&]() {
            return eligibleEnemyIndices[RNG::Range(0, (int)eligibleEnemyIndices.size() - 1)];
        };

        for (int i = 0; i < (int)rooms.size(); ++i) {
            rooms[i].enemySpawnList.clear();
            rooms[i].itemSpawnList.clear();

            if (rooms[i].type == RoomType::NORMAL && enemyCount > 0) {
                if (RNG::Chance(0.10f)) {
                    continue;
                }

                int distBonus = distFromStart[i];
                int spawnCount = settings.normalEnemyBase
                    + settings.normalEnemyPerFloor * currentFloor
                    + (distBonus > 2 ? settings.deepRoomBonus : 0);
                for (int j = 0; j < spawnCount; ++j) {
                    rooms[i].enemySpawnList.push_back(PickEnemyIndex());
                }
            } else if (rooms[i].type == RoomType::TREASURE && itemCount > 0) {
                int itemDrops = RNG::Range(settings.treasureMinItems, settings.treasureMaxItems);
                for (int j = 0; j < itemDrops; ++j) {
                    rooms[i].itemSpawnList.push_back(RNG::Range(0, itemCount - 1));
                }
            } else if (rooms[i].type == RoomType::BOSS && itemCount > 0 && currentFloor < totalFloors) {
                rooms[i].itemSpawnList.push_back(RNG::Range(0, itemCount - 1));
            } else if (rooms[i].type == RoomType::CURSE) {
                bool enemyVariant = enemyCount > 0 && RNG::Chance(settings.curseEnemyChance);
                if (enemyVariant) {
                    int distBonus = distFromStart[i];
                    int spawnCount = settings.normalEnemyBase
                        + settings.normalEnemyPerFloor * currentFloor
                        + (distBonus > 2 ? settings.deepRoomBonus : 0);
                    for (int j = 0; j < spawnCount; ++j) {
                        rooms[i].enemySpawnList.push_back(PickEnemyIndex());
                    }
                } else if (itemCount > 0) {
                    int itemDrops = RNG::Range(settings.treasureMinItems, settings.treasureMaxItems);
                    for (int j = 0; j < itemDrops; ++j) {
                        rooms[i].itemSpawnList.push_back(RNG::Range(0, itemCount - 1));
                    }
                }
            } else if (rooms[i].type == RoomType::SHOP) {
                rooms[i].lootGranted = false;
            }
        }

        ApplyRoomDefaults();
        currentRoomIndex = startRoomIndex;
        return true;
    }

    return false;
}

bool Dungeon::AdvanceFloor(uint32_t seed) {
    if (currentFloor >= totalFloors) return false;
    return Generate(seed, currentFloor + 1);
}

int Dungeon::CurrentFloor() const {
    return currentFloor;
}

int Dungeon::MaxFloors() const {
    return totalFloors;
}

bool Dungeon::HasBossRoom() const {
    return true;
}

bool Dungeon::IsFinalFloor() const {
    return currentFloor >= totalFloors;
}

float Dungeon::SpecialEnemyChance() const {
    return settings.specialEnemyChance;
}

int Dungeon::CurseDamage() const {
    return (currentFloor >= totalFloors) ? 2 : 1; // full heart on the final floor, half heart otherwise
}

bool Dungeon::AllCombatRoomsCleared() const {
    for (const Room& room : rooms) {
        bool isCombatRoom = room.type == RoomType::NORMAL || room.type == RoomType::BOSS || room.IsEnemyCurseRoom();
        if (isCombatRoom && !room.cleared) return false;
    }
    return true;
}

Room& Dungeon::CurrentRoom() {
    return rooms[currentRoomIndex];
}

const Room& Dungeon::CurrentRoom() const {
    return rooms[currentRoomIndex];
}

const std::vector<Room>& Dungeon::Rooms() const {
    return rooms;
}

int Dungeon::CurrentRoomIndex() const {
    return currentRoomIndex;
}

void Dungeon::PlacePlayerAtCurrentRoomCenter(Player& player) const {
    if (currentRoomIndex < 0 || currentRoomIndex >= (int)rooms.size()) return;
    const Room& room = rooms[currentRoomIndex];
    player.pos.x = room.x + (room.width - player.size) * 0.5f;
    player.pos.y = room.y + (room.height - player.size) * 0.5f;
}

bool Dungeon::TryTransition(Player& player) {
    if (currentRoomIndex < 0 || currentRoomIndex >= (int)rooms.size()) return false;

    Room& room = rooms[currentRoomIndex];
    if (!room.gateOpen) return false;

    std::vector<TransitionCandidate> candidates;
    float leftOverflow = room.x - player.pos.x;
    float rightOverflow = (player.pos.x + player.size) - (room.x + room.width);
    float topOverflow = room.y - player.pos.y;
    float bottomOverflow = (player.pos.y + player.size) - (room.y + room.height);

    if (leftOverflow > 0.0f && room.west >= 0) candidates.push_back({ DoorDir::WEST, room.west, leftOverflow });
    if (rightOverflow > 0.0f && room.east >= 0) candidates.push_back({ DoorDir::EAST, room.east, rightOverflow });
    if (topOverflow > 0.0f && room.north >= 0) candidates.push_back({ DoorDir::NORTH, room.north, topOverflow });
    if (bottomOverflow > 0.0f && room.south >= 0) candidates.push_back({ DoorDir::SOUTH, room.south, bottomOverflow });

    if (candidates.empty()) return false;

    auto bestIt = std::max_element(candidates.begin(), candidates.end(),
        [](const TransitionCandidate& a, const TransitionCandidate& b) {
            return a.overflow < b.overflow;
        });

    int fromIndex = currentRoomIndex;
    int toIndex = bestIt->nextIndex;
    if (toIndex < 0 || toIndex >= (int)rooms.size()) return false;

    if (rooms[fromIndex].type == RoomType::CURSE) {
        PlayerLogic::TakeDamage(player, CurseDamage(), 0.5f);
    }

    currentRoomIndex = toIndex;
    MovePlayerIntoRoom(player, fromIndex, toIndex);

    if (rooms[currentRoomIndex].type == RoomType::CURSE) {
        PlayerLogic::TakeDamage(player, CurseDamage(), 0.5f);
    }

    return true;
}

void Dungeon::MarkCurrentRoomCleared(bool rollCurseReward) {
    if (currentRoomIndex < 0 || currentRoomIndex >= (int)rooms.size()) return;
    Room& room = rooms[currentRoomIndex];
    room.cleared = true;
    room.gateOpen = true;

    if (!rollCurseReward) return;
    bool rewardRoom = room.type == RoomType::NORMAL || room.type == RoomType::BOSS || room.IsEnemyCurseRoom();
    if (!rewardRoom) return;

    auto MakePickup = [&](RoomPickupType type, int amount = 0) {
        RoomPickup pickup;
        pickup.type = type;
        pickup.amount = amount;
        pickup.pos = { room.x + room.width * 0.5f - 4.0f, room.y + room.height * 0.5f - 4.0f };
        room.pickups.push_back(pickup);
    };

    float roll = RNG::Range(0.0f, 1.0f);
    if (roll < 0.10f) {
        RoomPickup pickup;
        pickup.type = RoomPickupType::CHEST;
        pickup.chestType = RollChestTypeForFloor(currentFloor);
        pickup.itemId = (ItemDatabase::Count() > 0) ? RNG::Range(0, ItemDatabase::Count() - 1) : -1;
        pickup.pos = { room.x + room.width * 0.5f - 4.0f, room.y + room.height * 0.5f - 4.0f };
        room.pickups.push_back(pickup);
    } else if (roll < 0.10f + settings.heartDropChance) {
        MakePickup(RoomPickupType::HEART);
    } else if (roll < 0.10f + settings.heartDropChance + settings.bombDropChance) {
        MakePickup(RoomPickupType::BOMB);
    } else if (roll < 0.10f + settings.heartDropChance + settings.bombDropChance + settings.coinDropChance) {
        int coinValue = (RNG::Chance(0.08f)) ? 10 : (RNG::Chance(0.25f) ? 5 : 1);
        MakePickup(RoomPickupType::COIN, coinValue);
    } else if (RNG::Chance(settings.keyDropChance)) {
        MakePickup(RoomPickupType::KEY, 1);
    }
}
