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
    constexpr int BOSS_VARIANT_TIER[32] = {
        1, 1, 1, 1, 1, 1,
        2, 2, 2, 2, 2, 2,
        3, 3, 3, 3, 3, 3, 3, 3,
        4, 4, 4, 4, 4, 4,
        5, 5, 5, 5, 5, 5
    };

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

    int PickItemForRoom(const char* pools, int minTier, int maxTier) {
        int cappedMin = std::clamp(minTier, 1, 5);
        int cappedMax = std::clamp(maxTier, cappedMin, 5);
        int itemId = ItemDatabase::Pick(pools, cappedMin, cappedMax);
        if (itemId >= 0) return itemId;
        return ItemDatabase::Pick(nullptr, cappedMin, cappedMax);
    }

    int PickChestItem(ChestType chestType, int floor) {
        switch (chestType) {
            case ChestType::WOODEN:
                return PickItemForRoom("TREASURE,SHOP,CHEST", 1, std::min(3, 1 + floor));
            case ChestType::IRON:
                return PickItemForRoom("TREASURE,SHOP,CHEST", 1, std::min(4, 2 + floor));
            case ChestType::STONE:
                return PickItemForRoom("TREASURE,BOSS,CHEST", 2, std::min(4, 2 + floor));
            case ChestType::GOLDEN:
                return PickItemForRoom("BOSS,SHOP,CHEST", 3, std::min(5, 3 + floor));
            case ChestType::DEVIL:
                return PickItemForRoom("CURSE,BOSS,CHEST", 3, 5);
            case ChestType::ANGEL:
                return PickItemForRoom("BOSS,CHEST", 4, 5);
            case ChestType::GAMBLE:
                return PickItemForRoom("TREASURE,BOSS,SHOP,CHEST", 1, 5);
        }
        return ItemDatabase::Pick(nullptr, 1, 5);
    }

    bool RectsOverlap(const Rect& a, const Rect& b) {
        return !(a.x + a.w <= b.x || b.x + b.w <= a.x || a.y + a.h <= b.y || b.y + b.h <= a.y);
    }

    void PopulateRoomHazards(Room& room, int floor) {
        room.rocks.clear();
        room.traps.clear();

        if (room.type == RoomType::START || room.type == RoomType::TREASURE || room.type == RoomType::SHOP) {
            return;
        }

        static const Vec2 ROCK_CLUSTERS[][4] = {
            { { 34.0f, 30.0f }, { 46.0f, 30.0f }, { 34.0f, 42.0f }, { 46.0f, 42.0f } },
            { { 238.0f, 30.0f }, { 250.0f, 30.0f }, { 238.0f, 42.0f }, { 250.0f, 42.0f } },
            { { 34.0f, 126.0f }, { 46.0f, 126.0f }, { 34.0f, 138.0f }, { 46.0f, 138.0f } },
            { { 238.0f, 126.0f }, { 250.0f, 126.0f }, { 238.0f, 138.0f }, { 250.0f, 138.0f } },
            { { 84.0f, 66.0f }, { 96.0f, 66.0f }, { 84.0f, 78.0f }, { 96.0f, 78.0f } },
            { { 220.0f, 66.0f }, { 232.0f, 66.0f }, { 220.0f, 78.0f }, { 232.0f, 78.0f } },
            { { 84.0f, 100.0f }, { 96.0f, 100.0f }, { 84.0f, 112.0f }, { 96.0f, 112.0f } },
            { { 220.0f, 100.0f }, { 232.0f, 100.0f }, { 220.0f, 112.0f }, { 232.0f, 112.0f } }
        };

        static const Vec2 TRAP_SPOTS[] = {
            { 70.0f, 48.0f }, { 158.0f, 48.0f }, { 246.0f, 48.0f },
            { 70.0f, 116.0f }, { 158.0f, 116.0f }, { 246.0f, 116.0f },
            { 122.0f, 82.0f }, { 196.0f, 82.0f }
        };

        const Rect playerCore = { room.width * 0.5f - 18.0f, room.height * 0.5f - 18.0f, 36.0f, 36.0f };

        int clusterCount = (room.type == RoomType::BOSS) ? 4 : 3;
        clusterCount += std::min(2, floor / 2);
        if (room.type == RoomType::CURSE) clusterCount++;
        clusterCount = std::clamp(clusterCount, 2, 7);

        std::vector<int> clusterIndices;
        for (int i = 0; i < (int)(sizeof(ROCK_CLUSTERS) / sizeof(ROCK_CLUSTERS[0])); ++i) {
            bool overlapsPlayer = false;
            for (int j = 0; j < 4; ++j) {
                Rect rockRect = { ROCK_CLUSTERS[i][j].x, ROCK_CLUSTERS[i][j].y, 12.0f, 12.0f };
                if (RectsOverlap(rockRect, playerCore)) {
                    overlapsPlayer = true;
                    break;
                }
            }
            if (!overlapsPlayer) clusterIndices.push_back(i);
        }

        for (int i = 0; i < clusterCount && !clusterIndices.empty(); ++i) {
            int choiceIndex = RNG::Range(0, (int)clusterIndices.size() - 1);
            int cluster = clusterIndices[choiceIndex];
            clusterIndices.erase(clusterIndices.begin() + choiceIndex);

            int rocksInCluster = RNG::Chance(0.35f) ? 4 : 3;
            if (room.type == RoomType::BOSS) rocksInCluster = 4;
            if (room.type == RoomType::CURSE && RNG::Chance(0.35f)) rocksInCluster = 2;

            for (int j = 0; j < rocksInCluster; ++j) {
                RoomRock rock;
                rock.pos = ROCK_CLUSTERS[cluster][j];
                rock.w = 12.0f;
                rock.h = 12.0f;
                float indestructibleChance = 0.18f + 0.04f * (float)floor;
                if (room.type == RoomType::BOSS) indestructibleChance += 0.10f;
                if (room.type == RoomType::CURSE) indestructibleChance += 0.05f;
                if (RNG::Chance(std::clamp(indestructibleChance, 0.18f, 0.45f))) {
                    rock.type = RoomRockType::INDESTRUCTIBLE;
                } else if (RNG::Chance(0.62f)) {
                    rock.type = RoomRockType::BOMBABLE_COIN;
                    rock.rewardAmount = RNG::Chance(0.22f) ? 5 : 1;
                } else {
                    rock.type = RoomRockType::BOMBABLE_HEART;
                    rock.rewardAmount = 1;
                }
                room.rocks.push_back(rock);
            }
        }

        int trapTarget = 0;
        if (room.type == RoomType::BOSS) {
            trapTarget = (floor >= 2 && RNG::Chance(0.70f)) ? 2 : 1;
        } else if (room.type == RoomType::NORMAL || room.IsEnemyCurseRoom()) {
            if (RNG::Chance(std::clamp(0.20f + 0.08f * (float)floor, 0.20f, 0.55f))) {
                trapTarget = 1;
                if (floor >= 3 && RNG::Chance(0.30f)) trapTarget = 2;
            }
        }
        trapTarget = std::min(trapTarget, 3);

        std::vector<int> trapIndices;
        for (int i = 0; i < (int)(sizeof(TRAP_SPOTS) / sizeof(TRAP_SPOTS[0])); ++i) {
            Rect trapRect = { TRAP_SPOTS[i].x, TRAP_SPOTS[i].y, 12.0f, 12.0f };
            if (RectsOverlap(trapRect, playerCore)) continue;
            bool blocked = false;
            for (const RoomRock& rock : room.rocks) {
                if (RectsOverlap(trapRect, rock.GetRect())) {
                    blocked = true;
                    break;
                }
            }
            if (!blocked) trapIndices.push_back(i);
        }

        for (int i = 0; i < trapTarget && !trapIndices.empty(); ++i) {
            int choiceIndex = RNG::Range(0, (int)trapIndices.size() - 1);
            int spotIndex = trapIndices[choiceIndex];
            trapIndices.erase(trapIndices.begin() + choiceIndex);

            RoomTrap trap;
            trap.pos = TRAP_SPOTS[spotIndex];
            trap.w = 12.0f;
            trap.h = 12.0f;
            int roll = RNG::Range(0, 99);
            if (roll < 35) trap.type = RoomTrapType::POISON;
            else if (roll < 62) trap.type = RoomTrapType::TELEPORT;
            else if (roll < 84) trap.type = RoomTrapType::SUMMON;
            else trap.type = RoomTrapType::SPIKE;
            room.traps.push_back(trap);
        }
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
        for (int v = 0; v < 32; ++v) {
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
                int maxTier = std::clamp(1 + currentFloor, 1, 5);
                for (int j = 0; j < itemDrops; ++j) {
                    int itemId = PickItemForRoom("TREASURE,SHOP,CHEST", 1, maxTier);
                    if (itemId < 0) itemId = RNG::Range(0, itemCount - 1);
                    rooms[i].itemSpawnList.push_back(itemId);
                }
            } else if (rooms[i].type == RoomType::BOSS && itemCount > 0 && currentFloor < totalFloors) {
                int itemId = PickItemForRoom("BOSS,TREASURE,CHEST", 2, std::clamp(2 + currentFloor, 2, 5));
                if (itemId < 0) itemId = RNG::Range(0, itemCount - 1);
                rooms[i].itemSpawnList.push_back(itemId);
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
                    int maxTier = std::clamp(3 + currentFloor / 2, 3, 5);
                    for (int j = 0; j < itemDrops; ++j) {
                        int itemId = PickItemForRoom("CURSE,BOSS,CHEST", 3, maxTier);
                        if (itemId < 0) itemId = RNG::Range(0, itemCount - 1);
                        rooms[i].itemSpawnList.push_back(itemId);
                    }
                }
            } else if (rooms[i].type == RoomType::SHOP) {
                rooms[i].lootGranted = false;
            }
        }

        ApplyRoomDefaults();
        for (auto& room : rooms) {
            PopulateRoomHazards(room, currentFloor);
        }
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
        pickup.itemId = PickChestItem(pickup.chestType, currentFloor);
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
