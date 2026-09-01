#include "terrain_gen.h"

#include <algorithm>
#include <array>
#include <vector>

#include "../../engine/core/rng.h"

namespace TerrainGen {
    namespace {
        constexpr int TERRAIN_GRID_W = 16;
        constexpr int TERRAIN_GRID_H = 9;
        constexpr float TERRAIN_CELL_W = 20.0f;
        constexpr float TERRAIN_CELL_H = 20.0f;

        int TerrainCellIndex(int x, int y) {
            return y * TERRAIN_GRID_W + x;
        }

        bool IsReservedTerrainCell(const Room& room, int x, int y) {
            const int centerX = TERRAIN_GRID_W / 2;
            const int centerY = TERRAIN_GRID_H / 2;

            if (x == centerX || x == centerX - 1 || y == centerY) return true;

            if (room.north >= 0 && y <= 1 && x >= centerX - 1 && x <= centerX + 1) return true;
            if (room.south >= 0 && y >= TERRAIN_GRID_H - 2 && x >= centerX - 1 && x <= centerX + 1) return true;
            if (room.west >= 0 && x <= 1 && y >= centerY - 1 && y <= centerY + 1) return true;
            if (room.east >= 0 && x >= TERRAIN_GRID_W - 2 && y >= centerY - 1 && y <= centerY + 1) return true;

            return false;
        }
    }

    std::vector<RoomTerrainFeature> GenerateCellularTerrain(const Room& room, int floor) {
        std::array<bool, TERRAIN_GRID_W * TERRAIN_GRID_H> solid{};
        std::array<bool, TERRAIN_GRID_W * TERRAIN_GRID_H> next{};

        float seedChance = (room.archetype == RoomArchetype::BROKEN_ARENA) ? 0.36f : 0.48f;
        seedChance += 0.02f * (float)std::min(3, floor / 2);
        if (room.type == RoomType::CURSE) seedChance += 0.04f;
        if (room.theme == DungeonTheme::DRACONIC) seedChance += 0.03f;
        if (room.theme == DungeonTheme::CRYPT || room.theme == DungeonTheme::FUNGAL) seedChance += 0.02f;
        seedChance = std::clamp(seedChance, 0.28f, 0.65f);

        for (int y = 0; y < TERRAIN_GRID_H; ++y) {
            for (int x = 0; x < TERRAIN_GRID_W; ++x) {
                int idx = TerrainCellIndex(x, y);
                solid[idx] = !IsReservedTerrainCell(room, x, y) && RNG::Chance(seedChance);
            }
        }

        int smoothingPasses = (room.archetype == RoomArchetype::BROKEN_ARENA) ? 3 : 4;
        for (int pass = 0; pass < smoothingPasses; ++pass) {
            next = solid;
            for (int y = 0; y < TERRAIN_GRID_H; ++y) {
                for (int x = 0; x < TERRAIN_GRID_W; ++x) {
                    int idx = TerrainCellIndex(x, y);
                    if (IsReservedTerrainCell(room, x, y)) {
                        next[idx] = false;
                        continue;
                    }

                    int solidNeighbors = 0;
                    for (int oy = -1; oy <= 1; ++oy) {
                        for (int ox = -1; ox <= 1; ++ox) {
                            if (ox == 0 && oy == 0) continue;
                            int nx = x + ox;
                            int ny = y + oy;
                            if (nx < 0 || ny < 0 || nx >= TERRAIN_GRID_W || ny >= TERRAIN_GRID_H) continue;
                            if (solid[TerrainCellIndex(nx, ny)]) solidNeighbors++;
                        }
                    }

                    int threshold = (room.archetype == RoomArchetype::BROKEN_ARENA) ? 5 : 4;
                    next[idx] = solidNeighbors >= threshold;
                }
            }
            solid = next;
        }

        std::vector<RoomTerrainFeature> terrain;
        int solidCount = 0;
        for (int y = 0; y < TERRAIN_GRID_H; ++y) {
            for (int x = 0; x < TERRAIN_GRID_W; ++x) {
                int idx = TerrainCellIndex(x, y);
                if (!solid[idx] || IsReservedTerrainCell(room, x, y)) continue;
                solidCount++;

                RoomTerrainFeature feature;
                feature.pos = {
                    room.x + (float)x * TERRAIN_CELL_W,
                    room.y + (float)y * TERRAIN_CELL_H
                };
                feature.w = TERRAIN_CELL_W;
                feature.h = TERRAIN_CELL_H;
                feature.rewardAmount = 0;

                float pitChance = (room.archetype == RoomArchetype::HAZARD_ROOM) ? 0.58f : 0.22f;
                if (room.theme == DungeonTheme::CRYPT) pitChance += 0.14f;
                if (room.theme == DungeonTheme::FUNGAL) pitChance += 0.10f;
                if (room.theme == DungeonTheme::FORGE) pitChance -= 0.12f;
                if (room.theme == DungeonTheme::DRACONIC) pitChance += 0.08f;
                pitChance = std::clamp(pitChance, 0.0f, 0.85f);

                if (RNG::Chance(pitChance)) {
                    feature.type = RoomTerrainType::PIT;
                } else {
                    float indestructibleChance = 0.15f + 0.03f * (float)floor;
                    if (room.theme == DungeonTheme::FORGE) indestructibleChance += 0.10f;
                    if (room.theme == DungeonTheme::DRACONIC) indestructibleChance += 0.08f;
                    if (room.theme == DungeonTheme::CRYPT || room.theme == DungeonTheme::FUNGAL) indestructibleChance -= 0.04f;
                    indestructibleChance = std::clamp(indestructibleChance, 0.10f, 0.50f);

                    if (RNG::Chance(indestructibleChance)) {
                        feature.type = RoomTerrainType::ROCK_INDESTRUCTIBLE;
                    } else if (RNG::Chance(0.02f)) {
                        if (RNG::Chance(0.70f)) {
                            feature.type = RoomTerrainType::ROCK_BOMBABLE_COIN;
                            feature.rewardAmount = RNG::Chance(0.22f) ? 5 : 1;
                        } else {
                            feature.type = RoomTerrainType::ROCK_BOMBABLE_HEART;
                            feature.rewardAmount = 1;
                        }
                    } else {
                        feature.type = RoomTerrainType::ROCK_BOMBABLE;
                    }
                }

                terrain.push_back(feature);
            }
        }

        if (solidCount < 4) terrain.clear();
        return terrain;
    }

    namespace {
        constexpr int GRID_CELL_COUNT = TERRAIN_GRID_W * TERRAIN_GRID_H;
        constexpr float GRID_FEATURE_INSET = 2.0f;
        constexpr float GRID_FEATURE_SIZE = TERRAIN_CELL_W - GRID_FEATURE_INSET * 2.0f;

        bool IsValidTerrainCell(const Room& room, int x, int y) {
            return x >= 0 && y >= 0 && x < TERRAIN_GRID_W && y < TERRAIN_GRID_H && !IsReservedTerrainCell(room, x, y);
        }

        void AddGridFeature(const Room& room, std::vector<RoomTerrainFeature>& terrain, std::array<bool, GRID_CELL_COUNT>& occupied, int x, int y, RoomTerrainType type) {
            if (!IsValidTerrainCell(room, x, y)) return;
            int idx = TerrainCellIndex(x, y);
            if (occupied[idx]) return;
            occupied[idx] = true;

            RoomTerrainFeature feature;
            feature.pos = {
                room.x + (float)x * TERRAIN_CELL_W + GRID_FEATURE_INSET,
                room.y + (float)y * TERRAIN_CELL_H + GRID_FEATURE_INSET
            };
            feature.w = GRID_FEATURE_SIZE;
            feature.h = GRID_FEATURE_SIZE;
            feature.type = type;
            feature.rewardAmount = 0;
            terrain.push_back(feature);
        }
    }

    std::vector<RoomTerrainFeature> GenerateGridRuleTerrain(const Room& room, int floor) {
        std::vector<RoomTerrainFeature> terrain;
        std::array<bool, GRID_CELL_COUNT> occupied{};

        auto Add = [&](int x, int y, RoomTerrainType type = RoomTerrainType::ROCK_INDESTRUCTIBLE) {
            AddGridFeature(room, terrain, occupied, x, y, type);
        };

        auto AddColumn = [&](int x, int gapRowA, int gapRowB, int startY, int endY, RoomTerrainType type) {
            for (int y = startY; y <= endY; ++y) {
                if (y == gapRowA || y == gapRowB) continue;
                Add(x, y, type);
            }
        };

        switch (room.archetype) {
            case RoomArchetype::WHISPERING_STACKS: {
                int spacing = 3 + RNG::Range(0, 1);
                int phase = RNG::Range(0, spacing - 1);
                float gapChance = std::clamp(0.12f + 0.02f * (float)floor, 0.12f, 0.30f);
                RoomTerrainType type = (room.theme == DungeonTheme::RUINS) ? RoomTerrainType::CRATE_DESTRUCTIBLE : RoomTerrainType::ROCK_INDESTRUCTIBLE;

                for (int y = 1; y < TERRAIN_GRID_H - 1; ++y) {
                    for (int x = 1; x < TERRAIN_GRID_W - 1; ++x) {
                        if (IsReservedTerrainCell(room, x, y)) continue;
                        bool lattice = ((x + y + phase) % spacing == 0) || ((x + phase) % spacing == 0 && (y % 2 == 0));
                        if (!lattice) continue;
                        if (RNG::Chance(gapChance)) continue;
                        Add(x, y, type);
                    }
                }
                break;
            }

            case RoomArchetype::RUNIC_LATTICE: {
                int phaseX = RNG::Range(0, 1);
                int phaseY = RNG::Range(0, 1);
                float gapChance = std::clamp(0.08f + 0.01f * (float)floor, 0.08f, 0.22f);
                RoomTerrainType type = (room.theme == DungeonTheme::FORGE) ? RoomTerrainType::ROCK_EXPLOSIVE : RoomTerrainType::ROCK_INDESTRUCTIBLE;

                for (int y = 1; y < TERRAIN_GRID_H - 1; ++y) {
                    for (int x = 1; x < TERRAIN_GRID_W - 1; ++x) {
                        if (IsReservedTerrainCell(room, x, y)) continue;
                        bool checker = (((x + phaseX) + (y + phaseY)) % 2) == 0;
                        if (!checker) continue;
                        if (RNG::Chance(gapChance)) continue;
                        Add(x, y, type);
                    }
                }
                break;
            }

            case RoomArchetype::FORKING_PATH: {
                RoomTerrainType wallType = (room.theme == DungeonTheme::CRYPT) ? RoomTerrainType::ROCK_BOMBABLE : RoomTerrainType::ROCK_INDESTRUCTIBLE;
                int wallColumns[] = { 3, 7, 11 };
                int offset = RNG::Range(0, 1);
                int maxRows = TERRAIN_GRID_H - 2;

                for (int i = 0; i < 3; ++i) {
                    int x = wallColumns[i] + offset;
                    if (x >= TERRAIN_GRID_W - 2) x = wallColumns[i];
                    int gapRow = 1 + RNG::Range(1, maxRows - 2);
                    AddColumn(x, gapRow, std::max(1, gapRow - 1), 1, maxRows, wallType);

                    if (x - 1 > 0 && RNG::Chance(0.45f)) Add(x - 1, gapRow, wallType);
                    if (x + 1 < TERRAIN_GRID_W - 1 && RNG::Chance(0.45f)) Add(x + 1, gapRow, wallType);
                    if (gapRow + 1 < maxRows && RNG::Chance(0.35f)) Add(x, gapRow + 1, wallType);
                }
                break;
            }

            default:
                break;
        }

        return terrain;
    }

    namespace {
        using CellMask = std::array<bool, GRID_CELL_COUNT>;

        struct TerrainPick {
            RoomTerrainType type = RoomTerrainType::ROCK_INDESTRUCTIBLE;
            int rewardAmount = 0;
        };

        template <typename MaskFn>
        CellMask BuildCellMask(const Room& room, int floor, float seedChance, int passes, int threshold, MaskFn maskFn) {
            CellMask solid{};
            CellMask next{};

            for (int y = 0; y < TERRAIN_GRID_H; ++y) {
                for (int x = 0; x < TERRAIN_GRID_W; ++x) {
                    int idx = TerrainCellIndex(x, y);
                    if (IsReservedTerrainCell(room, x, y)) {
                        solid[idx] = false;
                        continue;
                    }
                    float chance = std::clamp(seedChance * maskFn(x, y), 0.0f, 0.95f);
                    solid[idx] = RNG::Chance(chance);
                }
            }

            for (int pass = 0; pass < passes; ++pass) {
                next = solid;
                for (int y = 0; y < TERRAIN_GRID_H; ++y) {
                    for (int x = 0; x < TERRAIN_GRID_W; ++x) {
                        int idx = TerrainCellIndex(x, y);
                        if (IsReservedTerrainCell(room, x, y)) {
                            next[idx] = false;
                            continue;
                        }

                        int solidNeighbors = 0;
                        for (int oy = -1; oy <= 1; ++oy) {
                            for (int ox = -1; ox <= 1; ++ox) {
                                if (ox == 0 && oy == 0) continue;
                                int nx = x + ox;
                                int ny = y + oy;
                                if (nx < 0 || ny < 0 || nx >= TERRAIN_GRID_W || ny >= TERRAIN_GRID_H) continue;
                                if (solid[TerrainCellIndex(nx, ny)]) solidNeighbors++;
                            }
                        }

                        next[idx] = solidNeighbors >= threshold;
                    }
                }
                solid = next;
            }

            (void)floor;
            return solid;
        }

        template <typename PickFn>
        std::vector<RoomTerrainFeature> EmitCellMask(const Room& room, const CellMask& mask, PickFn pickFn, float inset = 2.0f, float size = 16.0f) {
            std::vector<RoomTerrainFeature> terrain;
            for (int y = 0; y < TERRAIN_GRID_H; ++y) {
                for (int x = 0; x < TERRAIN_GRID_W; ++x) {
                    int idx = TerrainCellIndex(x, y);
                    if (!mask[idx] || IsReservedTerrainCell(room, x, y)) continue;

                    RoomTerrainFeature feature;
                    feature.pos = {
                        room.x + (float)x * TERRAIN_CELL_W + inset,
                        room.y + (float)y * TERRAIN_CELL_H + inset
                    };
                    feature.w = size;
                    feature.h = size;
                    feature.rewardAmount = 0;
                    TerrainPick pick = pickFn(x, y);
                    feature.type = pick.type;
                    feature.rewardAmount = pick.rewardAmount;
                    terrain.push_back(feature);
                }
            }
            return terrain;
        }

        TerrainPick PickBasicBlock(const Room& room, int floor, int x, int y, bool preferBombable, bool preferCrate) {
            TerrainPick pick;
            float indestructibleChance = 0.18f + 0.03f * (float)floor;
            if (room.type == RoomType::BOSS) indestructibleChance += 0.08f;
            if (room.type == RoomType::CURSE) indestructibleChance += 0.05f;
            if (room.theme == DungeonTheme::FORGE) indestructibleChance += 0.08f;
            if (room.theme == DungeonTheme::DRACONIC) indestructibleChance += 0.06f;
            if (room.theme == DungeonTheme::CRYPT || room.theme == DungeonTheme::FUNGAL) indestructibleChance -= 0.03f;

            if (RNG::Chance(std::clamp(indestructibleChance, 0.10f, 0.55f))) {
                pick.type = RoomTerrainType::ROCK_INDESTRUCTIBLE;
                return pick;
            }

            if (preferCrate && room.theme == DungeonTheme::RUINS && RNG::Chance(0.40f)) {
                pick.type = RoomTerrainType::CRATE_DESTRUCTIBLE;
                return pick;
            }

            if (preferBombable && RNG::Chance(0.25f)) {
                pick.type = RoomTerrainType::ROCK_BOMBABLE;
                return pick;
            }

            if (room.theme == DungeonTheme::FORGE && RNG::Chance(0.28f)) {
                pick.type = RoomTerrainType::ROCK_EXPLOSIVE;
                return pick;
            }

            if (RNG::Chance(0.08f)) {
                pick.type = RoomTerrainType::ROCK_BOMBABLE_HEART;
                pick.rewardAmount = 1;
            } else if (RNG::Chance(0.12f)) {
                pick.type = RoomTerrainType::ROCK_BOMBABLE_COIN;
                pick.rewardAmount = RNG::Chance(0.20f) ? 5 : 1;
            } else {
                pick.type = RoomTerrainType::ROCK_BOMBABLE;
            }
            return pick;
        }

        std::vector<RoomTerrainFeature> GenerateRepulsionTerrain(const Room& room, int floor) {
            auto mask = BuildCellMask(room, floor, 0.30f, 3, 4, [&](int x, int y) {
                float centerX = (TERRAIN_GRID_W - 1) * 0.5f;
                float centerY = (TERRAIN_GRID_H - 1) * 0.5f;
                float dx = (float)x - centerX;
                float dy = (float)y - centerY;
                float dist = std::sqrt(dx * dx + dy * dy);
                float bias = 1.0f - std::clamp(dist / 7.0f, 0.0f, 0.7f);
                return std::clamp(0.5f + bias, 0.15f, 1.0f);
            });

            CellMask kept{};
            for (int y = 0; y < TERRAIN_GRID_H; ++y) {
                for (int x = 0; x < TERRAIN_GRID_W; ++x) {
                    int idx = TerrainCellIndex(x, y);
                    if (!mask[idx]) continue;
                    bool tooClose = false;
                    for (int oy = -1; oy <= 1 && !tooClose; ++oy) {
                        for (int ox = -1; ox <= 1; ++ox) {
                            if (ox == 0 && oy == 0) continue;
                            int nx = x + ox;
                            int ny = y + oy;
                            if (nx < 0 || ny < 0 || nx >= TERRAIN_GRID_W || ny >= TERRAIN_GRID_H) continue;
                            if (kept[TerrainCellIndex(nx, ny)]) {
                                tooClose = true;
                                break;
                            }
                        }
                    }
                    if (!tooClose) kept[idx] = true;
                }
            }

            return EmitCellMask(room, kept, [&](int x, int y) {
                return PickBasicBlock(room, floor, x, y, false, room.theme == DungeonTheme::RUINS);
            });
        }

        std::vector<RoomTerrainFeature> GenerateMirrorTerrain(const Room& room, int floor, bool vertical, bool twinIslands) {
            auto mask = BuildCellMask(room, floor, twinIslands ? 0.42f : 0.34f, twinIslands ? 3 : 4, twinIslands ? 4 : 3, [&](int x, int y) {
                float centerX = (TERRAIN_GRID_W - 1) * 0.5f;
                float centerY = (TERRAIN_GRID_H - 1) * 0.5f;
                float dx = std::fabs((float)x - centerX);
                float dy = std::fabs((float)y - centerY);
                float side = vertical ? dx : dy;
                float bias = 1.0f - std::clamp(side / 4.0f, 0.0f, 0.8f);
                if (twinIslands) {
                    float centralGap = 1.0f - std::clamp(dx / 2.0f, 0.0f, 1.0f);
                    bias = std::max(bias, 0.25f + centralGap * 0.6f);
                }
                return std::clamp(bias, 0.10f, 1.0f);
            });

            CellMask mirrored = mask;
            for (int y = 0; y < TERRAIN_GRID_H; ++y) {
                for (int x = 0; x < TERRAIN_GRID_W; ++x) {
                    int mx = vertical ? (TERRAIN_GRID_W - 1 - x) : x;
                    int my = vertical ? y : (TERRAIN_GRID_H - 1 - y);
                    if (mx < 0 || my < 0 || mx >= TERRAIN_GRID_W || my >= TERRAIN_GRID_H) continue;
                    bool solid = mask[TerrainCellIndex(x, y)] || mask[TerrainCellIndex(mx, my)];
                    mirrored[TerrainCellIndex(x, y)] = solid;
                    mirrored[TerrainCellIndex(mx, my)] = solid;
                }
            }

            if (twinIslands) {
                int bridgeY = TERRAIN_GRID_H / 2;
                for (int x = TERRAIN_GRID_W / 2 - 1; x <= TERRAIN_GRID_W / 2 + 1; ++x) {
                    mirrored[TerrainCellIndex(x, bridgeY)] = false;
                }
            }

            return EmitCellMask(room, mirrored, [&](int x, int y) {
                if (twinIslands && std::abs(x - TERRAIN_GRID_W / 2) <= 1 && y == TERRAIN_GRID_H / 2) {
                    TerrainPick pick;
                    pick.type = (RNG::Chance(0.55f)) ? RoomTerrainType::BRIDGE_TEMPORARY : RoomTerrainType::BRIDGE_FRAGILE;
                    return pick;
                }
                return PickBasicBlock(room, floor, x, y, room.theme != DungeonTheme::FORGE, false);
            });
        }

        std::vector<RoomTerrainFeature> GenerateBranchingTerrain(const Room& room, int floor, bool manyForks) {
            CellMask solid{};
            struct Node { int x; int y; int dx; int dy; int depth; };
            std::vector<Node> frontier;
            frontier.push_back({ TERRAIN_GRID_W / 2, TERRAIN_GRID_H / 2, 0, -1, 0 });
            frontier.push_back({ TERRAIN_GRID_W / 2 - 1, TERRAIN_GRID_H / 2, 1, 0, 0 });

            int maxDepth = manyForks ? 28 : 18;
            int branchChance = manyForks ? 50 : 25;

            while (!frontier.empty()) {
                Node node = frontier.back();
                frontier.pop_back();
                for (int step = 0; step < 3; ++step) {
                    if (node.x < 0 || node.y < 0 || node.x >= TERRAIN_GRID_W || node.y >= TERRAIN_GRID_H) break;
                    if (!IsReservedTerrainCell(room, node.x, node.y)) {
                        solid[TerrainCellIndex(node.x, node.y)] = true;
                    }
                    if (node.depth >= maxDepth) break;
                    if (RNG::Chance(0.45f)) {
                        int turn = RNG::Range(-1, 1);
                        if (node.dx == 0 && node.dy == 0) node.dy = 1;
                        if (node.dx != 0) {
                            node.dy = turn;
                            node.dx = (node.dx > 0) ? 1 : -1;
                        } else {
                            node.dx = turn;
                            node.dy = (node.dy > 0) ? 1 : -1;
                        }
                    }
                    node.x += node.dx;
                    node.y += node.dy;
                    node.depth++;
                    if (manyForks && RNG::Chance(0.30f) && frontier.size() < 12) {
                        frontier.push_back({ node.x, node.y, node.dy, node.dx, node.depth + 1 });
                    }
                    if (RNG::Chance(branchChance / 100.0f) && frontier.size() < 12) {
                        frontier.push_back({ node.x, node.y, -node.dy, node.dx, node.depth + 1 });
                    }
                }
            }

            return EmitCellMask(room, solid, [&](int x, int y) {
                return PickBasicBlock(room, floor, x, y, true, room.theme == DungeonTheme::RUINS);
            });
        }

        std::vector<RoomTerrainFeature> GenerateMazeTerrain(const Room& room, int floor) {
            CellMask solid{};
            solid.fill(true);
            for (int y = 0; y < TERRAIN_GRID_H; ++y) {
                for (int x = 0; x < TERRAIN_GRID_W; ++x) {
                    if (IsReservedTerrainCell(room, x, y)) solid[TerrainCellIndex(x, y)] = false;
                }
            }

            int x = TERRAIN_GRID_W / 2;
            int y = TERRAIN_GRID_H / 2;
            for (int i = 0; i < 26; ++i) {
                solid[TerrainCellIndex(x, y)] = false;
                int dir = RNG::Range(0, 3);
                if (dir == 0 && x > 1) x--;
                if (dir == 1 && x < TERRAIN_GRID_W - 2) x++;
                if (dir == 2 && y > 1) y--;
                if (dir == 3 && y < TERRAIN_GRID_H - 2) y++;
                if (RNG::Chance(0.25f)) {
                    int sideX = std::clamp(x + RNG::Range(-1, 1), 1, TERRAIN_GRID_W - 2);
                    int sideY = std::clamp(y + RNG::Range(-1, 1), 1, TERRAIN_GRID_H - 2);
                    solid[TerrainCellIndex(sideX, sideY)] = false;
                }
            }

            return EmitCellMask(room, solid, [&](int gx, int gy) {
                float hazardChance = std::clamp(0.12f + 0.02f * (float)floor, 0.12f, 0.35f);
                if (RNG::Chance(hazardChance)) {
                    TerrainPick pick;
                    pick.type = (RNG::Chance(0.50f)) ? RoomTerrainType::TERRAIN_WEB : RoomTerrainType::TERRAIN_SLIME;
                    return pick;
                }
                return PickBasicBlock(room, floor, gx, gy, true, true);
            });
        }
    }

    std::vector<RoomTerrainFeature> GenerateArchetypeTerrain(const Room& room, int floor) {
        switch (room.archetype) {
            case RoomArchetype::WHISPERING_STACKS:
            case RoomArchetype::RUNIC_LATTICE:
            case RoomArchetype::FORKING_PATH:
                return GenerateGridRuleTerrain(room, floor);
            case RoomArchetype::PILLAR_FIELD:
                return GenerateRepulsionTerrain(room, floor);
            case RoomArchetype::RITUAL_ROOM:
                return GenerateMirrorTerrain(room, floor, true, false);
            case RoomArchetype::SENTRY_HALL:
                return GenerateMirrorTerrain(room, floor, true, false);
            case RoomArchetype::TWIN_ISLANDS:
                return GenerateMirrorTerrain(room, floor, true, true);
            case RoomArchetype::COLLAPSED_VAULT:
                return GenerateBranchingTerrain(room, floor, false);
            case RoomArchetype::NARROW_VEINS:
                return GenerateBranchingTerrain(room, floor, true);
            case RoomArchetype::GARDEN_MAZE:
                return GenerateMazeTerrain(room, floor);
            case RoomArchetype::SPIRE_ASCENT:
            case RoomArchetype::AMPHITHEATER:
            case RoomArchetype::FLOODED_CHAMBER:
            case RoomArchetype::SHATTERED_BRIDGE:
            case RoomArchetype::ECHO_ROOM:
            case RoomArchetype::THRONE_APPROACH:
            default:
                break;
        }

        // Fall back to a biased cellular layout for the remaining archetypes.
        float seedChance = 0.38f;
        int passes = 4;
        int threshold = 4;
        bool invertRadial = false;
        bool strongEdgeBias = false;
        bool useFunnel = false;
        bool useBridgeBands = false;
        bool useSparseCenterClear = false;

        switch (room.archetype) {
            case RoomArchetype::SPIRE_ASCENT:
                seedChance = 0.36f;
                strongEdgeBias = true;
                break;
            case RoomArchetype::AMPHITHEATER:
                seedChance = 0.34f;
                invertRadial = true;
                break;
            case RoomArchetype::FLOODED_CHAMBER:
                seedChance = 0.50f;
                passes = 5;
                threshold = 5;
                invertRadial = true;
                break;
            case RoomArchetype::SHATTERED_BRIDGE:
                seedChance = 0.38f;
                useBridgeBands = true;
                break;
            case RoomArchetype::ECHO_ROOM:
                seedChance = 0.18f;
                passes = 3;
                threshold = 5;
                useSparseCenterClear = true;
                break;
            case RoomArchetype::THRONE_APPROACH:
                seedChance = 0.30f;
                useFunnel = true;
                break;
            default:
                break;
        }

        auto maskFn = [&](int x, int y) {
            float centerX = (TERRAIN_GRID_W - 1) * 0.5f;
            float centerY = (TERRAIN_GRID_H - 1) * 0.5f;
            float dx = ((float)x - centerX) / centerX;
            float dy = ((float)y - centerY) / centerY;
            float radial = std::sqrt(dx * dx + dy * dy);
            float bias = 1.0f;
            if (strongEdgeBias) {
                bias = 0.35f + (1.0f - std::clamp((float)y / (TERRAIN_GRID_H - 1), 0.0f, 1.0f));
            } else if (invertRadial) {
                bias = 0.35f + std::clamp(radial, 0.0f, 1.0f);
            } else if (useBridgeBands) {
                bias = ((x / 3) % 2 == 0) ? 1.0f : 0.55f;
            } else if (useSparseCenterClear) {
                bias = (radial > 0.45f) ? 1.0f : 0.25f;
            } else if (useFunnel) {
                bias = 0.20f + std::clamp((float)y / (TERRAIN_GRID_H - 1), 0.0f, 1.0f) * 0.95f;
            }
            return std::clamp(bias, 0.10f, 1.20f);
        };

        auto solid = BuildCellMask(room, floor, seedChance, passes, threshold, maskFn);
        if (room.archetype == RoomArchetype::SHATTERED_BRIDGE) {
            for (int y = 0; y < TERRAIN_GRID_H; ++y) {
                for (int x = 0; x < TERRAIN_GRID_W; ++x) {
                    int idx = TerrainCellIndex(x, y);
                    if (IsReservedTerrainCell(room, x, y)) continue;
                    if ((x % 3) == 1 && y > 1 && y < TERRAIN_GRID_H - 2 && RNG::Chance(0.50f)) {
                        solid[idx] = false;
                    }
                }
            }
        }

        if (room.archetype == RoomArchetype::ECHO_ROOM) {
            for (int y = 0; y < TERRAIN_GRID_H; ++y) {
                for (int x = 0; x < TERRAIN_GRID_W; ++x) {
                    int idx = TerrainCellIndex(x, y);
                    if (IsReservedTerrainCell(room, x, y)) continue;
                    if (x >= 5 && x <= 10 && y >= 3 && y <= 5) solid[idx] = false;
                }
            }
        }

        return EmitCellMask(room, solid, [&](int x, int y) {
            if (room.archetype == RoomArchetype::FLOODED_CHAMBER) {
                if (RNG::Chance(0.40f)) return TerrainPick{ RoomTerrainType::PIT, 0 };
                if (RNG::Chance(0.25f)) return TerrainPick{ RoomTerrainType::TERRAIN_SLIME, 0 };
            }
            if (room.archetype == RoomArchetype::AMPHITHEATER) {
                if (RNG::Chance(0.25f)) return TerrainPick{ RoomTerrainType::ROCK_EXPLOSIVE, 0 };
            }
            if (room.archetype == RoomArchetype::THRONE_APPROACH && (x == TERRAIN_GRID_W / 2 || y == TERRAIN_GRID_H / 2)) {
                return TerrainPick{ RoomTerrainType::PRESSURE_PLATE, 0 };
            }
            if (room.archetype == RoomArchetype::SHATTERED_BRIDGE && (x == TERRAIN_GRID_W / 2 - 1 || x == TERRAIN_GRID_W / 2)) {
                TerrainPick bridge;
                bridge.type = (RNG::Chance(0.60f)) ? RoomTerrainType::BRIDGE_FRAGILE : RoomTerrainType::BRIDGE_TEMPORARY;
                return bridge;
            }
            if (room.archetype == RoomArchetype::SPIRE_ASCENT && y < 3) {
                return TerrainPick{ RoomTerrainType::ROCK_INDESTRUCTIBLE, 0 };
            }
            if (room.archetype == RoomArchetype::ECHO_ROOM && RNG::Chance(0.18f)) {
                return TerrainPick{ RoomTerrainType::TRAP_TELEPORT, 0 };
            }
            return PickBasicBlock(room, floor, x, y, room.archetype == RoomArchetype::ECHO_ROOM, room.theme == DungeonTheme::RUINS);
        });
    }
}
