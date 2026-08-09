#include "dungeon.h"
#include <algorithm>
#include <climits>
#include <queue>
#include "../enemies/enemy_database.h"
#include "../items/item_database.h"

namespace {
    constexpr int CURSE_DAMAGE = 10;
    constexpr int MAX_GENERATION_ATTEMPTS = 64;
    constexpr int MAIN_PATH_ROOMS = 6;
    constexpr int EXTRA_NORMAL_ROOMS = 2;

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
            case RoomType::CURSE:
                room.cleared = true;
                room.gateOpen = true;
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
    RNG::Seed(seed);

    for (int attempt = 0; attempt < MAX_GENERATION_ATTEMPTS; ++attempt) {
        rooms.clear();
        rooms.reserve(16);
        cellToRoomIndex.assign(gridWidth * gridHeight, -1);
        currentRoomIndex = -1;
        startRoomIndex = -1;
        bossRoomIndex = -1;

        int bossX = RNG::Range(0, gridWidth - 1);
        int bossY = RNG::Range(0, gridHeight - 1);
        bossRoomIndex = AddRoomAtCell(bossX, bossY);
        rooms[bossRoomIndex].type = RoomType::BOSS;
        rooms[bossRoomIndex].bossVariant = RNG::Range(0, 2);

        std::vector<int> pathRoomIndices;
        pathRoomIndices.push_back(bossRoomIndex);

        int currentX = bossX;
        int currentY = bossY;
        bool failed = false;

        for (int i = 1; i < MAIN_PATH_ROOMS; ++i) {
            std::vector<Vec2> candidates;
            if (InBounds(currentX + 1, currentY) && RoomIndexAtCell(currentX + 1, currentY) < 0) candidates.push_back({ (float)(currentX + 1), (float)currentY });
            if (InBounds(currentX - 1, currentY) && RoomIndexAtCell(currentX - 1, currentY) < 0) candidates.push_back({ (float)(currentX - 1), (float)currentY });
            if (InBounds(currentX, currentY + 1) && RoomIndexAtCell(currentX, currentY + 1) < 0) candidates.push_back({ (float)currentX, (float)(currentY + 1) });
            if (InBounds(currentX, currentY - 1) && RoomIndexAtCell(currentX, currentY - 1) < 0) candidates.push_back({ (float)currentX, (float)(currentY - 1) });

            if (candidates.empty()) {
                failed = true;
                break;
            }

            const Vec2& nextCell = candidates[RNG::Range(0, (int)candidates.size() - 1)];
            currentX = (int)nextCell.x;
            currentY = (int)nextCell.y;

            int roomIndex = AddRoomAtCell(currentX, currentY);
            rooms[roomIndex].type = RoomType::NORMAL;
            pathRoomIndices.push_back(roomIndex);
        }

        if (failed) continue;

        int addedNormals = 0;
        int branchAttempts = 0;
        while (addedNormals < EXTRA_NORMAL_ROOMS && branchAttempts < 24) {
            branchAttempts++;
            int parentIndex = pathRoomIndices[RNG::Range(0, (int)pathRoomIndices.size() - 1)];
            const Room& parent = rooms[parentIndex];

            std::vector<Vec2> candidates;
            int px = (int)parent.gridPos.x;
            int py = (int)parent.gridPos.y;
            if (InBounds(px + 1, py) && RoomIndexAtCell(px + 1, py) < 0) candidates.push_back({ (float)(px + 1), (float)py });
            if (InBounds(px - 1, py) && RoomIndexAtCell(px - 1, py) < 0) candidates.push_back({ (float)(px - 1), (float)py });
            if (InBounds(px, py + 1) && RoomIndexAtCell(px, py + 1) < 0) candidates.push_back({ (float)px, (float)(py + 1) });
            if (InBounds(px, py - 1) && RoomIndexAtCell(px, py - 1) < 0) candidates.push_back({ (float)px, (float)(py - 1) });

            if (candidates.empty()) continue;

            const Vec2& nextCell = candidates[RNG::Range(0, (int)candidates.size() - 1)];
            int roomIndex = AddRoomAtCell((int)nextCell.x, (int)nextCell.y);
            rooms[roomIndex].type = RoomType::NORMAL;
            addedNormals++;
        }

        BuildConnections();

        std::vector<int> distFromBoss = ComputeDistances(rooms, bossRoomIndex);
        int bestStart = bossRoomIndex;
        int bestStartDist = -1;
        for (int i = 0; i < (int)rooms.size(); ++i) {
            if (distFromBoss[i] > bestStartDist) {
                bestStartDist = distFromBoss[i];
                bestStart = i;
            }
        }
        startRoomIndex = bestStart;
        rooms[startRoomIndex].type = RoomType::START;

        std::vector<int> distFromStart = ComputeDistances(rooms, startRoomIndex);

        int treasureIndex = -1;
        int treasureScore = INT_MAX;
        for (int i = 0; i < (int)rooms.size(); ++i) {
            if (i == bossRoomIndex || i == startRoomIndex) continue;
            if (distFromBoss[i] <= 1) continue;

            int score = distFromStart[i] * 10 - distFromBoss[i];
            if (score < treasureScore) {
                treasureScore = score;
                treasureIndex = i;
            }
        }

        int curseIndex = -1;
        for (int i = 0; i < (int)rooms.size(); ++i) {
            if (i == bossRoomIndex || i == startRoomIndex || i == treasureIndex) continue;
            if (distFromBoss[i] <= 1) continue;
            curseIndex = i;
            break;
        }

        if (treasureIndex < 0 || curseIndex < 0) continue;

        rooms[treasureIndex].type = RoomType::TREASURE;
        rooms[curseIndex].type = RoomType::CURSE;

        const int enemyCount = EnemyDatabase::Count();
        const int itemCount = ItemDatabase::Count();
        for (int i = 0; i < (int)rooms.size(); ++i) {
            rooms[i].enemySpawnList.clear();
            rooms[i].itemSpawnList.clear();

            if (rooms[i].type == RoomType::NORMAL && enemyCount > 0) {
                int spawnCount = 2 + (distFromBoss[i] > 2 ? 1 : 0);
                for (int j = 0; j < spawnCount; ++j) {
                    rooms[i].enemySpawnList.push_back(RNG::Range(0, enemyCount - 1));
                }
            } else if (rooms[i].type == RoomType::TREASURE && itemCount > 0) {
                int itemDrops = 1 + RNG::Range(0, 1);
                for (int j = 0; j < itemDrops; ++j) {
                    rooms[i].itemSpawnList.push_back(RNG::Range(0, itemCount - 1));
                }
            } else if (rooms[i].type == RoomType::CURSE && itemCount > 0) {
                rooms[i].itemSpawnList.push_back(RNG::Range(0, itemCount - 1));
            }
        }

        ApplyRoomDefaults();
        currentRoomIndex = startRoomIndex;
        return true;
    }

    return false;
}

Room& Dungeon::CurrentRoom() {
    return rooms[currentRoomIndex];
}

const Room& Dungeon::CurrentRoom() const {
    return rooms[currentRoomIndex];
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
        PlayerLogic::TakeDamage(player, CURSE_DAMAGE, 0.0f);
    }

    currentRoomIndex = toIndex;
    MovePlayerIntoRoom(player, fromIndex, toIndex);

    if (rooms[currentRoomIndex].type == RoomType::CURSE) {
        PlayerLogic::TakeDamage(player, CURSE_DAMAGE, 0.0f);
    }

    return true;
}

void Dungeon::MarkCurrentRoomCleared() {
    if (currentRoomIndex < 0 || currentRoomIndex >= (int)rooms.size()) return;
    rooms[currentRoomIndex].cleared = true;
    rooms[currentRoomIndex].gateOpen = true;
}
