#include "dungeon.h"
#include <algorithm>
#include <array>
#include <climits>
#include <cmath>
#include <queue>
#include <cstring>
#include "../../engine/data_parser.h"
#include "../enemies/enemy_database.h"
#include "../bosses/boss_database.h"
#include "../items/item_database.h"
#include "../progression/meta_progression.h"
#include "terrain_gen.h"

namespace {
    constexpr int MAX_GENERATION_ATTEMPTS = 64;
    constexpr int BOSS_VARIANT_TIER[MAX_BOSS_TEMPLATES] = {
        1, 1, 1, 1, 1, 1,
        2, 2, 2, 2, 2, 2,
        3, 3, 3, 3, 3, 3, 3, 3,
        4, 4, 4, 4, 4, 4,
        5, 5, 5, 5, 5, 5,
        6, 6, 6, 6
    };

    ChestType RollChestTypeForFloor(int floor) {
        int roll = RNG::Range(0, 99);
        if (MetaProgression::IsGambleChestUnlocked()) {
            if (floor >= 8 && roll < 18) return ChestType::GAMBLE;
            if (floor >= 5 && roll < 8) return ChestType::GAMBLE;
        }
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

    struct DungeonThemeProfile {
        DungeonTheme theme = DungeonTheme::RUINS;
        char name[16] = {};
        char enemyPools[512] = {};
        char bossPools[512] = {};
        float specialEnemyBonus = 0.0f;
        float trapChanceBonus = 0.0f;
        int rockBonus = 0;
        int pitBonus = 0;
        int rewardBonus = 0;
    };

    DungeonThemeProfile g_themeProfiles[8];
    int g_themeProfileCount = 0;

    DungeonTheme ParseThemeName(const char* s) {
        if (std::strcmp(s, "FORGE") == 0) return DungeonTheme::FORGE;
        if (std::strcmp(s, "CRYPT") == 0) return DungeonTheme::CRYPT;
        if (std::strcmp(s, "FUNGAL") == 0) return DungeonTheme::FUNGAL;
        if (std::strcmp(s, "DRACONIC") == 0) return DungeonTheme::DRACONIC;
        return DungeonTheme::RUINS;
    }

    const char* ThemeName(DungeonTheme theme) {
        switch (theme) {
            case DungeonTheme::FORGE: return "FORGE";
            case DungeonTheme::CRYPT: return "CRYPT";
            case DungeonTheme::FUNGAL: return "FUNGAL";
            case DungeonTheme::DRACONIC: return "DRACONIC";
            case DungeonTheme::RUINS:
            default:
                return "RUINS";
        }
    }

    const DungeonThemeProfile& DefaultThemeProfile(DungeonTheme theme) {
        static DungeonThemeProfile defaults[5];
        static bool initialized = false;
        if (!initialized) {
            initialized = true;

            defaults[0].theme = DungeonTheme::RUINS;
            std::strncpy(defaults[0].name, "RUINS", sizeof(defaults[0].name) - 1);
            std::strncpy(defaults[0].enemyPools, "Zombie,Gunner,Fly,Spider", sizeof(defaults[0].enemyPools) - 1);
            std::strncpy(defaults[0].bossPools, "Runt,Pinwheel,Spinner,GlassKing,Patroller", sizeof(defaults[0].bossPools) - 1);

            defaults[1].theme = DungeonTheme::FORGE;
            std::strncpy(defaults[1].name, "FORGE", sizeof(defaults[1].name) - 1);
            std::strncpy(defaults[1].enemyPools, "BombKnight,Warlord,JuggernautPrime,HexMatron", sizeof(defaults[1].enemyPools) - 1);
            std::strncpy(defaults[1].bossPools, "IronSaint,Graveforge,Emberlord,Coward", sizeof(defaults[1].bossPools) - 1);
            defaults[1].rockBonus = 1;
            defaults[1].trapChanceBonus = 0.08f;
            defaults[1].specialEnemyBonus = 0.02f;

            defaults[2].theme = DungeonTheme::CRYPT;
            std::strncpy(defaults[2].name, "CRYPT", sizeof(defaults[2].name) - 1);
            std::strncpy(defaults[2].enemyPools, "Mimic,BlightGrub,HexMatron,Reaper", sizeof(defaults[2].enemyPools) - 1);
            std::strncpy(defaults[2].bossPools, "Grief,Hollowqueen,Voidcrown,Linker", sizeof(defaults[2].bossPools) - 1);
            defaults[2].trapChanceBonus = 0.12f;
            defaults[2].pitBonus = 1;

            defaults[3].theme = DungeonTheme::FUNGAL;
            std::strncpy(defaults[3].name, "FUNGAL", sizeof(defaults[3].name) - 1);
            std::strncpy(defaults[3].enemyPools, "BlightGrub,VenomEye,SplitterLord,SnareTurret", sizeof(defaults[3].enemyPools) - 1);
            std::strncpy(defaults[3].bossPools, "Mire,Spite,Harvester,Nullsire,SwarmLeader", sizeof(defaults[3].bossPools) - 1);
            defaults[3].trapChanceBonus = 0.10f;
            defaults[3].specialEnemyBonus = 0.03f;
            defaults[3].pitBonus = 1;

            defaults[4].theme = DungeonTheme::DRACONIC;
            std::strncpy(defaults[4].name, "DRACONIC", sizeof(defaults[4].name) - 1);
            std::strncpy(defaults[4].enemyPools, "Ravager,Stormeye,Railwing,Titan,Eclipse", sizeof(defaults[4].enemyPools) - 1);
            std::strncpy(defaults[4].bossPools, "DragonSovereign,Eclipse,Emberlord,Linker,SwarmLeader,Coward", sizeof(defaults[4].bossPools) - 1);
            defaults[4].rockBonus = 2;
            defaults[4].trapChanceBonus = 0.15f;
            defaults[4].specialEnemyBonus = 0.05f;
            defaults[4].pitBonus = 2;
            defaults[4].rewardBonus = 1;
        }

        for (const auto& profile : defaults) {
            if (profile.theme == theme) return profile;
        }
        return defaults[0];
    }

    const DungeonThemeProfile& ThemeProfileFor(DungeonTheme theme) {
        for (int i = 0; i < g_themeProfileCount; ++i) {
            if (g_themeProfiles[i].theme == theme) return g_themeProfiles[i];
        }
        return DefaultThemeProfile(theme);
    }

    void LoadDefaultThemeProfiles() {
        g_themeProfileCount = 0;
        for (DungeonTheme theme : { DungeonTheme::RUINS, DungeonTheme::FORGE, DungeonTheme::CRYPT,
                                     DungeonTheme::FUNGAL, DungeonTheme::DRACONIC }) {
            if (g_themeProfileCount >= (int)(sizeof(g_themeProfiles) / sizeof(g_themeProfiles[0]))) break;
            g_themeProfiles[g_themeProfileCount++] = DefaultThemeProfile(theme);
        }
    }

    bool LoadThemeProfiles(const char* path) {
        std::vector<DataBlock> blocks = DataParser::ParseFile(path);
        if (blocks.empty()) return false;

        LoadDefaultThemeProfiles();
        for (const DataBlock& block : blocks) {
            if (g_themeProfileCount >= (int)(sizeof(g_themeProfiles) / sizeof(g_themeProfiles[0]))) break;
            DungeonThemeProfile profile = DefaultThemeProfile(ParseThemeName(block.name));
            profile.theme = ParseThemeName(block.GetString("theme", block.name));
            std::strncpy(profile.name, block.name, sizeof(profile.name) - 1);
            std::strncpy(profile.enemyPools, block.GetString("enemy_pools", profile.enemyPools), sizeof(profile.enemyPools) - 1);
            profile.enemyPools[sizeof(profile.enemyPools) - 1] = '\0';
            std::strncpy(profile.bossPools, block.GetString("boss_pools", profile.bossPools), sizeof(profile.bossPools) - 1);
            profile.bossPools[sizeof(profile.bossPools) - 1] = '\0';
            profile.specialEnemyBonus = std::clamp(block.GetFloat("special_enemy_bonus", profile.specialEnemyBonus), 0.0f, 0.25f);
            profile.trapChanceBonus = std::clamp(block.GetFloat("trap_chance_bonus", profile.trapChanceBonus), -0.10f, 0.30f);
            profile.rockBonus = block.GetInt("rock_bonus", profile.rockBonus);
            profile.pitBonus = block.GetInt("pit_bonus", profile.pitBonus);
            profile.rewardBonus = block.GetInt("reward_bonus", profile.rewardBonus);

            bool replaced = false;
            for (int i = 0; i < g_themeProfileCount; ++i) {
                if (g_themeProfiles[i].theme == profile.theme) {
                    g_themeProfiles[i] = profile;
                    replaced = true;
                    break;
                }
            }
            if (!replaced) {
                g_themeProfiles[g_themeProfileCount++] = profile;
            }
        }

        return true;
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
        int unlockedMax = MetaProgression::MaxUnlockedItemTier();
        if (cappedMin > unlockedMax) cappedMin = 1;
        int cappedMax = std::clamp(maxTier, cappedMin, unlockedMax);
        int itemId = ItemDatabase::Pick(pools, cappedMin, cappedMax);
        if (itemId >= 0) return itemId;
        return ItemDatabase::Pick(nullptr, cappedMin, cappedMax);
    }

    DungeonTheme RollDungeonTheme(int floor) {
        if (floor <= 1) return DungeonTheme::RUINS;
        if (floor == 2) return DungeonTheme::FORGE;
        if (floor == 3) return DungeonTheme::CRYPT;
        if (floor == 4) return DungeonTheme::FUNGAL;
        return DungeonTheme::DRACONIC;
    }

    RoomArchetype RollArchetype(RoomType type, int floor) {
        switch (type) {
            case RoomType::START:
            case RoomType::TREASURE:
            case RoomType::SHOP:
                return RoomArchetype::OPEN_ARENA;
            case RoomType::BOSS:
                return RoomArchetype::RITUAL_ROOM;
            case RoomType::CURSE:
                return (RNG::Chance(0.55f)) ? RoomArchetype::HAZARD_ROOM : RoomArchetype::BROKEN_ARENA;
            case RoomType::NORMAL:
            default:
                break;
        }

        float roll = RNG::Range(0.0f, 1.0f);
        if (floor >= 4) {
            if (roll < 0.06f) return RoomArchetype::WHISPERING_STACKS;
            if (roll < 0.12f) return RoomArchetype::RUNIC_LATTICE;
            if (roll < 0.18f) return RoomArchetype::FORKING_PATH;
            if (roll < 0.26f) return RoomArchetype::SPIRE_ASCENT;
            if (roll < 0.34f) return RoomArchetype::AMPHITHEATER;
            if (roll < 0.42f) return RoomArchetype::FLOODED_CHAMBER;
            if (roll < 0.50f) return RoomArchetype::SHATTERED_BRIDGE;
            if (roll < 0.58f) return RoomArchetype::ECHO_ROOM;
            if (roll < 0.66f) return RoomArchetype::THRONE_APPROACH;
            if (roll < 0.74f) return RoomArchetype::COLLAPSED_VAULT;
            if (roll < 0.82f) return RoomArchetype::SENTRY_HALL;
            if (roll < 0.90f) return RoomArchetype::TWIN_ISLANDS;
            if (roll < 0.96f) return RoomArchetype::NARROW_VEINS;
            return RoomArchetype::GARDEN_MAZE;
        } else if (floor == 3) {
            if (roll < 0.04f) return RoomArchetype::WHISPERING_STACKS;
            if (roll < 0.08f) return RoomArchetype::RUNIC_LATTICE;
            if (roll < 0.12f) return RoomArchetype::FORKING_PATH;
            if (roll < 0.20f) return RoomArchetype::SPIRE_ASCENT;
            if (roll < 0.28f) return RoomArchetype::FLOODED_CHAMBER;
            if (roll < 0.36f) return RoomArchetype::SENTRY_HALL;
            if (roll < 0.44f) return RoomArchetype::TWIN_ISLANDS;
            if (roll < 0.54f) return RoomArchetype::SHATTERED_BRIDGE;
            if (roll < 0.64f) return RoomArchetype::ECHO_ROOM;
            if (roll < 0.74f) return RoomArchetype::THRONE_APPROACH;
            if (roll < 0.82f) return RoomArchetype::COLLAPSED_VAULT;
            if (roll < 0.90f) return RoomArchetype::NARROW_VEINS;
            if (roll < 0.96f) return RoomArchetype::GARDEN_MAZE;
            return RoomArchetype::AMPHITHEATER;
        }

        if (floor <= 1) {
            if (roll < 0.35f) return RoomArchetype::OPEN_ARENA;
            if (roll < 0.65f) return RoomArchetype::PILLAR_FIELD;
            return RoomArchetype::BROKEN_ARENA;
        }
        if (floor == 2) {
            if (roll < 0.25f) return RoomArchetype::OPEN_ARENA;
            if (roll < 0.50f) return RoomArchetype::PILLAR_FIELD;
            if (roll < 0.75f) return RoomArchetype::GAUNTLET;
            return RoomArchetype::BROKEN_ARENA;
        }
        if (floor == 3) {
            if (roll < 0.20f) return RoomArchetype::GAUNTLET;
            if (roll < 0.45f) return RoomArchetype::HAZARD_ROOM;
            if (roll < 0.70f) return RoomArchetype::PILLAR_FIELD;
            return RoomArchetype::BROKEN_ARENA;
        }
        if (roll < 0.20f) return RoomArchetype::HAZARD_ROOM;
        if (roll < 0.45f) return RoomArchetype::GAUNTLET;
        if (roll < 0.70f) return RoomArchetype::PILLAR_FIELD;
        return RoomArchetype::BROKEN_ARENA;
    }

    RoomEncounterFamily RollEncounterFamily(RoomArchetype archetype, int floor) {
        switch (archetype) {
            case RoomArchetype::OPEN_ARENA:
                return RoomEncounterFamily::RUSH;
            case RoomArchetype::PILLAR_FIELD:
                return RoomEncounterFamily::GUARDIAN;
            case RoomArchetype::BROKEN_ARENA:
                return RoomEncounterFamily::AMBUSH;
            case RoomArchetype::GAUNTLET:
                return RoomEncounterFamily::SWARM;
            case RoomArchetype::HAZARD_ROOM:
                return RoomEncounterFamily::ARTILLERY;
            case RoomArchetype::RITUAL_ROOM:
                return RoomEncounterFamily::ELITE;
            case RoomArchetype::WHISPERING_STACKS:
                return RoomEncounterFamily::AMBUSH;
            case RoomArchetype::RUNIC_LATTICE:
                return RoomEncounterFamily::GUARDIAN;
            case RoomArchetype::FORKING_PATH:
                return RoomEncounterFamily::SWARM;
            case RoomArchetype::SPIRE_ASCENT:
            case RoomArchetype::ECHO_ROOM:
            case RoomArchetype::THRONE_APPROACH:
            case RoomArchetype::AMPHITHEATER:
                return RoomEncounterFamily::ARTILLERY;
            case RoomArchetype::FLOODED_CHAMBER:
            case RoomArchetype::SHATTERED_BRIDGE:
            case RoomArchetype::TWIN_ISLANDS:
            case RoomArchetype::SENTRY_HALL:
                return RoomEncounterFamily::GUARDIAN;
            case RoomArchetype::COLLAPSED_VAULT:
            case RoomArchetype::NARROW_VEINS:
            case RoomArchetype::GARDEN_MAZE:
                return RoomEncounterFamily::AMBUSH;
        }
        return (floor >= 3) ? RoomEncounterFamily::MIXED : RoomEncounterFamily::RUSH;
    }

    int EncounterFamilySpawnBias(RoomEncounterFamily family) {
        switch (family) {
            case RoomEncounterFamily::RUSH: return -1;
            case RoomEncounterFamily::ARTILLERY: return 0;
            case RoomEncounterFamily::SWARM: return 2;
            case RoomEncounterFamily::GUARDIAN: return 0;
            case RoomEncounterFamily::AMBUSH: return 1;
            case RoomEncounterFamily::MIXED: return 1;
            case RoomEncounterFamily::ELITE: return 0;
        }
        return 0;
    }

    float EncounterFamilySpecialChance(RoomEncounterFamily family) {
        switch (family) {
            case RoomEncounterFamily::RUSH: return 0.04f;
            case RoomEncounterFamily::ARTILLERY: return 0.16f;
            case RoomEncounterFamily::SWARM: return 0.08f;
            case RoomEncounterFamily::GUARDIAN: return 0.18f;
            case RoomEncounterFamily::AMBUSH: return 0.12f;
            case RoomEncounterFamily::MIXED: return 0.10f;
            case RoomEncounterFamily::ELITE: return 0.22f;
        }
        return 0.10f;
    }

    bool PoolsContainToken(const char* pools, const char* token) {
        if (!pools || !token || !token[0]) return false;
        char buffer[96];
        std::strncpy(buffer, pools, sizeof(buffer) - 1);
        buffer[sizeof(buffer) - 1] = '\0';
        char* part = std::strtok(buffer, ",");
        while (part) {
            while (*part == ' ' || *part == '\t') ++part;
            char* end = part + std::strlen(part);
            while (end > part && (end[-1] == ' ' || end[-1] == '\t')) --end;
            *end = '\0';
            if (std::strcmp(part, token) == 0) return true;
            part = std::strtok(nullptr, ",");
        }
        return false;
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

    constexpr int TERRAIN_GRID_W = 16;
    constexpr int TERRAIN_GRID_H = 9;
    constexpr float TERRAIN_CELL_W = 20.0f;
    constexpr float TERRAIN_CELL_H = 20.0f;

    int TerrainCellIndex(int x, int y) {
        return y * TERRAIN_GRID_W + x;
    }

    Rect TerrainCellRect(const Room& room, int x, int y) {
        return {
            room.x + (float)x * TERRAIN_CELL_W,
            room.y + (float)y * TERRAIN_CELL_H,
            TERRAIN_CELL_W,
            TERRAIN_CELL_H
        };
    }

    bool TerrainBlocksBaseline(const RoomTerrainFeature& feature) {
        return feature.type == RoomTerrainType::PIT || feature.BlocksMovement();
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

    int RoomThreatBudget(const Room& room, int floor) {
        int budget = 8 + floor * 4;
        switch (room.archetype) {
            case RoomArchetype::OPEN_ARENA: budget += 4; break;
            case RoomArchetype::PILLAR_FIELD: budget += 6; break;
            case RoomArchetype::BROKEN_ARENA: budget += 8; break;
            case RoomArchetype::GAUNTLET: budget += 7; break;
            case RoomArchetype::HAZARD_ROOM: budget += 9; break;
            case RoomArchetype::RITUAL_ROOM: budget += 12; break;
            case RoomArchetype::WHISPERING_STACKS: budget += 5; break;
            case RoomArchetype::RUNIC_LATTICE: budget += 6; break;
            case RoomArchetype::FORKING_PATH: budget += 7; break;
            case RoomArchetype::SPIRE_ASCENT: budget += 8; break;
            case RoomArchetype::FLOODED_CHAMBER: budget += 10; break;
            case RoomArchetype::COLLAPSED_VAULT: budget += 9; break;
            case RoomArchetype::SENTRY_HALL: budget += 8; break;
            case RoomArchetype::GARDEN_MAZE: budget += 9; break;
            case RoomArchetype::SHATTERED_BRIDGE: budget += 8; break;
            case RoomArchetype::ECHO_ROOM: budget += 7; break;
            case RoomArchetype::THRONE_APPROACH: budget += 10; break;
            case RoomArchetype::TWIN_ISLANDS: budget += 8; break;
            case RoomArchetype::NARROW_VEINS: budget += 8; break;
            case RoomArchetype::AMPHITHEATER: budget += 9; break;
        }

        switch (room.type) {
            case RoomType::START: budget = 0; break;
            case RoomType::TREASURE: budget = 2; break;
            case RoomType::SHOP: budget = 1; break;
            case RoomType::CURSE: budget += 6; break;
            case RoomType::BOSS: budget += 8; break;
            case RoomType::NORMAL:
            default:
                break;
        }

        switch (room.theme) {
            case DungeonTheme::FORGE: budget += 2; break;
            case DungeonTheme::CRYPT: budget += 2; break;
            case DungeonTheme::FUNGAL: budget += 1; break;
            case DungeonTheme::DRACONIC: budget += 4; break;
            case DungeonTheme::RUINS:
            default:
                break;
        }

        return std::max(0, budget);
    }

    int TerrainThreatCost(const RoomTerrainFeature& feature) {
        switch (feature.type) {
            case RoomTerrainType::ROCK_INDESTRUCTIBLE: return 3;
            case RoomTerrainType::ROCK_BOMBABLE_COIN:
            case RoomTerrainType::ROCK_BOMBABLE_HEART:
            case RoomTerrainType::ROCK_BOMBABLE:
            case RoomTerrainType::CRATE_DESTRUCTIBLE:
                return 2;
            case RoomTerrainType::ROCK_EXPLOSIVE:
            case RoomTerrainType::BLOCK_PUSHABLE:
                return 3;
            case RoomTerrainType::BRIDGE_TEMPORARY:
            case RoomTerrainType::BRIDGE_FRAGILE:
            case RoomTerrainType::TELEPORT_PAD:
            case RoomTerrainType::PRESSURE_PLATE:
            case RoomTerrainType::LILY_PAD:
                return 1;
            case RoomTerrainType::TERRAIN_WEB:
            case RoomTerrainType::TERRAIN_SLIME:
            case RoomTerrainType::TERRAIN_MUD:
            case RoomTerrainType::TERRAIN_FIRE:
            case RoomTerrainType::TERRAIN_ACID:
            case RoomTerrainType::TERRAIN_POISON:
                return 1;
            case RoomTerrainType::PIT:
                return 3;
            case RoomTerrainType::TRAP_POISON:
            case RoomTerrainType::TRAP_SPIKE:
                return 1;
            case RoomTerrainType::TRAP_TELEPORT:
            case RoomTerrainType::TRAP_SUMMON:
                return 2;
        }
        return 1;
    }

    int EnemyThreatCost(int enemyIndex) {
        int tier = EnemyDatabase::Tier(enemyIndex);
        return std::clamp(tier + 1, 1, 6);
    }

    float ArchetypeOpenTarget(RoomArchetype archetype) {
        switch (archetype) {
            case RoomArchetype::OPEN_ARENA: return 0.72f;
            case RoomArchetype::PILLAR_FIELD: return 0.62f;
            case RoomArchetype::BROKEN_ARENA: return 0.56f;
            case RoomArchetype::GAUNTLET: return 0.49f;
            case RoomArchetype::HAZARD_ROOM: return 0.55f;
            case RoomArchetype::RITUAL_ROOM: return 0.58f;
            case RoomArchetype::WHISPERING_STACKS: return 0.60f;
            case RoomArchetype::RUNIC_LATTICE: return 0.58f;
            case RoomArchetype::FORKING_PATH: return 0.52f;
            case RoomArchetype::SPIRE_ASCENT: return 0.56f;
            case RoomArchetype::FLOODED_CHAMBER: return 0.50f;
            case RoomArchetype::COLLAPSED_VAULT: return 0.46f;
            case RoomArchetype::SENTRY_HALL: return 0.54f;
            case RoomArchetype::GARDEN_MAZE: return 0.48f;
            case RoomArchetype::SHATTERED_BRIDGE: return 0.50f;
            case RoomArchetype::ECHO_ROOM: return 0.68f;
            case RoomArchetype::THRONE_APPROACH: return 0.52f;
            case RoomArchetype::TWIN_ISLANDS: return 0.58f;
            case RoomArchetype::NARROW_VEINS: return 0.44f;
            case RoomArchetype::AMPHITHEATER: return 0.64f;
        }
        return 0.60f;
    }

    float ArchetypeLargestRegionTarget(RoomArchetype archetype) {
        switch (archetype) {
            case RoomArchetype::OPEN_ARENA: return 0.85f;
            case RoomArchetype::PILLAR_FIELD: return 0.74f;
            case RoomArchetype::BROKEN_ARENA: return 0.70f;
            case RoomArchetype::GAUNTLET: return 0.66f;
            case RoomArchetype::HAZARD_ROOM: return 0.68f;
            case RoomArchetype::RITUAL_ROOM: return 0.72f;
            case RoomArchetype::WHISPERING_STACKS: return 0.74f;
            case RoomArchetype::RUNIC_LATTICE: return 0.70f;
            case RoomArchetype::FORKING_PATH: return 0.66f;
            case RoomArchetype::SPIRE_ASCENT: return 0.72f;
            case RoomArchetype::FLOODED_CHAMBER: return 0.68f;
            case RoomArchetype::COLLAPSED_VAULT: return 0.64f;
            case RoomArchetype::SENTRY_HALL: return 0.70f;
            case RoomArchetype::GARDEN_MAZE: return 0.62f;
            case RoomArchetype::SHATTERED_BRIDGE: return 0.66f;
            case RoomArchetype::ECHO_ROOM: return 0.78f;
            case RoomArchetype::THRONE_APPROACH: return 0.68f;
            case RoomArchetype::TWIN_ISLANDS: return 0.72f;
            case RoomArchetype::NARROW_VEINS: return 0.60f;
            case RoomArchetype::AMPHITHEATER: return 0.76f;
        }
        return 0.70f;
    }

    void BuildTerrainOccupancy(const Room& room, std::array<bool, TERRAIN_GRID_W * TERRAIN_GRID_H>& blocked) {
        blocked.fill(false);
        for (const auto& feature : room.terrain) {
            if (feature.broken || !TerrainBlocksBaseline(feature)) continue;
            Rect featureRect = feature.GetRect();
            for (int y = 0; y < TERRAIN_GRID_H; ++y) {
                for (int x = 0; x < TERRAIN_GRID_W; ++x) {
                    if (IsReservedTerrainCell(room, x, y)) continue;
                    Rect cellRect = TerrainCellRect(room, x, y);
                    if (featureRect.x + featureRect.w <= cellRect.x ||
                        cellRect.x + cellRect.w <= featureRect.x ||
                        featureRect.y + featureRect.h <= cellRect.y ||
                        cellRect.y + cellRect.h <= featureRect.y) {
                        continue;
                    }
                    blocked[TerrainCellIndex(x, y)] = true;
                }
            }
        }
    }

    bool ValidateCombatSpace(const Room& room) {
        std::array<bool, TERRAIN_GRID_W * TERRAIN_GRID_H> blocked{};
        BuildTerrainOccupancy(room, blocked);

        int totalOpen = 0;
        int largestRegion = 0;
        std::array<bool, TERRAIN_GRID_W * TERRAIN_GRID_H> visited{};

        auto FloodFrom = [&](int startX, int startY) {
            std::queue<int> q;
            int region = 0;
            int startIdx = TerrainCellIndex(startX, startY);
            visited[startIdx] = true;
            q.push(startIdx);
            while (!q.empty()) {
                int idx = q.front();
                q.pop();
                region++;
                int x = idx % TERRAIN_GRID_W;
                int y = idx / TERRAIN_GRID_W;
                const int nx[4] = { x + 1, x - 1, x, x };
                const int ny[4] = { y, y, y + 1, y - 1 };
                for (int i = 0; i < 4; ++i) {
                    if (nx[i] < 0 || ny[i] < 0 || nx[i] >= TERRAIN_GRID_W || ny[i] >= TERRAIN_GRID_H) continue;
                    int nextIdx = TerrainCellIndex(nx[i], ny[i]);
                    if (blocked[nextIdx] || visited[nextIdx]) continue;
                    visited[nextIdx] = true;
                    q.push(nextIdx);
                }
            }
            return region;
        };

        for (int y = 0; y < TERRAIN_GRID_H; ++y) {
            for (int x = 0; x < TERRAIN_GRID_W; ++x) {
                int idx = TerrainCellIndex(x, y);
                if (blocked[idx] || IsReservedTerrainCell(room, x, y)) continue;
                totalOpen++;
                if (visited[idx]) continue;
                largestRegion = std::max(largestRegion, FloodFrom(x, y));
            }
        }

        int usableCells = 0;
        for (int y = 0; y < TERRAIN_GRID_H; ++y) {
            for (int x = 0; x < TERRAIN_GRID_W; ++x) {
                if (!IsReservedTerrainCell(room, x, y)) usableCells++;
            }
        }
        float openPct = (usableCells > 0) ? ((float)totalOpen / (float)usableCells) : 1.0f;
        float largestPct = (totalOpen > 0) ? ((float)largestRegion / (float)totalOpen) : 1.0f;

        float openTarget = ArchetypeOpenTarget(room.archetype);
        if (room.type == RoomType::START || room.type == RoomType::TREASURE || room.type == RoomType::SHOP) openTarget = 0.72f;
        if (room.type == RoomType::CURSE) openTarget -= 0.03f;
        if (room.type == RoomType::BOSS) openTarget += 0.02f;
        openTarget = std::clamp(openTarget, 0.40f, 0.85f);

        float largestTarget = ArchetypeLargestRegionTarget(room.archetype);
        if (room.type == RoomType::BOSS) largestTarget += 0.05f;
        if (room.type == RoomType::START || room.type == RoomType::SHOP) largestTarget = 0.90f;
        largestTarget = std::clamp(largestTarget, 0.55f, 0.95f);

        return openPct >= openTarget && largestPct >= largestTarget;
    }

    bool ValidateTerrainLayout(const Room& room) {
        std::array<bool, TERRAIN_GRID_W * TERRAIN_GRID_H> blocked{};
        BuildTerrainOccupancy(room, blocked);

        std::vector<int> doorCells;
        auto AddDoorCells = [&](int minX, int maxX, int minY, int maxY) {
            for (int y = minY; y <= maxY; ++y) {
                for (int x = minX; x <= maxX; ++x) {
                    if (x < 0 || y < 0 || x >= TERRAIN_GRID_W || y >= TERRAIN_GRID_H) continue;
                    if (blocked[TerrainCellIndex(x, y)]) continue;
                    doorCells.push_back(TerrainCellIndex(x, y));
                }
            }
        };

        if (room.north >= 0) AddDoorCells(TERRAIN_GRID_W / 2 - 1, TERRAIN_GRID_W / 2 + 1, 0, 1);
        if (room.south >= 0) AddDoorCells(TERRAIN_GRID_W / 2 - 1, TERRAIN_GRID_W / 2 + 1, TERRAIN_GRID_H - 2, TERRAIN_GRID_H - 1);
        if (room.west >= 0) AddDoorCells(0, 1, TERRAIN_GRID_H / 2 - 1, TERRAIN_GRID_H / 2 + 1);
        if (room.east >= 0) AddDoorCells(TERRAIN_GRID_W - 2, TERRAIN_GRID_W - 1, TERRAIN_GRID_H / 2 - 1, TERRAIN_GRID_H / 2 + 1);

        if (doorCells.empty()) return true;

        std::array<bool, TERRAIN_GRID_W * TERRAIN_GRID_H> visited{};
        std::queue<int> q;
        visited[doorCells[0]] = true;
        q.push(doorCells[0]);

        auto EnqueueNeighbor = [&](int x, int y) {
            if (x < 0 || y < 0 || x >= TERRAIN_GRID_W || y >= TERRAIN_GRID_H) return;
            int idx = TerrainCellIndex(x, y);
            if (blocked[idx] || visited[idx]) return;
            visited[idx] = true;
            q.push(idx);
        };

        while (!q.empty()) {
            int idx = q.front();
            q.pop();
            int x = idx % TERRAIN_GRID_W;
            int y = idx / TERRAIN_GRID_W;
            EnqueueNeighbor(x + 1, y);
            EnqueueNeighbor(x - 1, y);
            EnqueueNeighbor(x, y + 1);
            EnqueueNeighbor(x, y - 1);
        }

        for (int doorCell : doorCells) {
            if (!visited[doorCell]) return false;
        }

        return true;
    }

    bool RectsOverlap(const Rect& a, const Rect& b) {
        return !(a.x + a.w <= b.x || b.x + b.w <= a.x || a.y + a.h <= b.y || b.y + b.h <= a.y);
    }

    bool PopulateRoomHazards(Room& room, int floor) {
        room.terrain.clear();
        room.threatBudget = RoomThreatBudget(room, floor);
        room.threatSpent = 0;

        if (room.type == RoomType::START || room.type == RoomType::TREASURE || room.type == RoomType::SHOP) {
            return true;
        }

        const DungeonThemeProfile& themeProfile = ThemeProfileFor(room.theme);
        int nextFeatureId = 0;

        auto AddFeature = [&](RoomTerrainFeature feature) {
            feature.featureId = nextFeatureId++;
            room.terrain.push_back(feature);
            room.threatSpent += TerrainThreatCost(feature);
        };

        auto OverlapsBlocking = [&](const Rect& rect) {
            for (const auto& feature : room.terrain) {
                if (feature.broken) continue;
                if (!TerrainBlocksBaseline(feature)) continue;
                if (RectsOverlap(rect, feature.GetRect())) return true;
            }
            return false;
        };

        auto GenerateTraps = [&]() {
            int trapTarget = 0;
            if (room.type == RoomType::BOSS) {
                trapTarget = (floor >= 2 && RNG::Chance(0.70f)) ? 2 : 1;
            } else if (room.type == RoomType::NORMAL || room.IsEnemyCurseRoom()) {
                float trapChance = std::clamp(0.20f + 0.08f * (float)floor, 0.20f, 0.55f);
                if (room.archetype == RoomArchetype::HAZARD_ROOM) trapChance += 0.15f;
                if (room.archetype == RoomArchetype::GAUNTLET) trapChance += 0.08f;
                trapChance += themeProfile.trapChanceBonus;
                if (RNG::Chance(std::clamp(trapChance, 0.0f, 0.75f))) {
                    trapTarget = 1;
                    if (floor >= 3 && RNG::Chance(0.30f)) trapTarget = 2;
                }
            }
            trapTarget = std::min(trapTarget, 3);

            static const Vec2 TRAP_SPOTS[] = {
                { 70.0f, 48.0f }, { 158.0f, 48.0f }, { 246.0f, 48.0f },
                { 70.0f, 116.0f }, { 158.0f, 116.0f }, { 246.0f, 116.0f },
                { 122.0f, 82.0f }, { 196.0f, 82.0f }
            };

            const Rect playerCore = { room.width * 0.5f - 18.0f, room.height * 0.5f - 18.0f, 36.0f, 36.0f };
            std::vector<int> trapIndices;
            for (int i = 0; i < (int)(sizeof(TRAP_SPOTS) / sizeof(TRAP_SPOTS[0])); ++i) {
                Rect trapRect = { TRAP_SPOTS[i].x, TRAP_SPOTS[i].y, 12.0f, 12.0f };
                if (RectsOverlap(trapRect, playerCore)) continue;
                if (OverlapsBlocking(trapRect)) continue;
                trapIndices.push_back(i);
            }

            for (int i = 0; i < trapTarget && !trapIndices.empty(); ++i) {
                int choiceIndex = RNG::Range(0, (int)trapIndices.size() - 1);
                int spotIndex = trapIndices[choiceIndex];
                trapIndices.erase(trapIndices.begin() + choiceIndex);

                RoomTerrainFeature feature;
                feature.pos = TRAP_SPOTS[spotIndex];
                feature.w = 12.0f;
                feature.h = 12.0f;
                feature.rewardAmount = 0;
                int roll = RNG::Range(0, 99);
                if (roll < 35) feature.type = RoomTerrainType::TRAP_POISON;
                else if (roll < 62) feature.type = RoomTerrainType::TRAP_TELEPORT;
                else if (roll < 84) feature.type = RoomTerrainType::TRAP_SUMMON;
                else feature.type = RoomTerrainType::TRAP_SPIKE;
                AddFeature(feature);
            }
        };

        auto GenerateRuleBasedTerrain = [&]() {
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

            static const Vec2 PIT_SPOTS[] = {
                { 108.0f, 54.0f }, { 176.0f, 54.0f },
                { 108.0f, 98.0f }, { 176.0f, 98.0f },
                { 142.0f, 72.0f }
            };

            const Rect playerCore = { room.width * 0.5f - 18.0f, room.height * 0.5f - 18.0f, 36.0f, 36.0f };
            auto OverlapsAny = [&](const Rect& rect) {
                for (const auto& feature : room.terrain) {
                    if (feature.broken) continue;
                    if (RectsOverlap(rect, feature.GetRect())) return true;
                }
                return false;
            };

            int clusterCount = 2;
            switch (room.archetype) {
                case RoomArchetype::OPEN_ARENA: clusterCount = 1; break;
                case RoomArchetype::PILLAR_FIELD: clusterCount = 3; break;
                case RoomArchetype::BROKEN_ARENA: clusterCount = 4; break;
                case RoomArchetype::GAUNTLET: clusterCount = 2; break;
                case RoomArchetype::HAZARD_ROOM: clusterCount = 2; break;
                case RoomArchetype::RITUAL_ROOM: clusterCount = 4; break;
            }
            clusterCount += std::min(2, floor / 2);
            if (room.type == RoomType::CURSE) clusterCount++;
            clusterCount += themeProfile.rockBonus;
            clusterCount = std::clamp(clusterCount, 1, 7);

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
                if (room.archetype == RoomArchetype::OPEN_ARENA) rocksInCluster = 2;
                if (room.archetype == RoomArchetype::GAUNTLET) rocksInCluster = 3;
                if (room.archetype == RoomArchetype::RITUAL_ROOM) rocksInCluster = 4;
                if (room.type == RoomType::CURSE && RNG::Chance(0.35f)) rocksInCluster = 2;
                if (room.theme == DungeonTheme::DRACONIC) rocksInCluster++;

                for (int j = 0; j < rocksInCluster; ++j) {
                    RoomTerrainFeature feature;
                    feature.pos = ROCK_CLUSTERS[cluster][j];
                    feature.w = 12.0f;
                    feature.h = 12.0f;
                    float indestructibleChance = 0.18f + 0.04f * (float)floor;
                    if (room.type == RoomType::BOSS) indestructibleChance += 0.10f;
                    if (room.type == RoomType::CURSE) indestructibleChance += 0.05f;
                    if (RNG::Chance(std::clamp(indestructibleChance, 0.18f, 0.45f))) {
                        feature.type = RoomTerrainType::ROCK_INDESTRUCTIBLE;
                    } else if (room.theme == DungeonTheme::FORGE && RNG::Chance(0.30f)) {
                        feature.type = RoomTerrainType::ROCK_EXPLOSIVE;
                    } else if (room.theme == DungeonTheme::RUINS && RNG::Chance(0.22f)) {
                        feature.type = RoomTerrainType::CRATE_DESTRUCTIBLE;
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
                    AddFeature(feature);
                }
            }

            int pitTarget = 0;
            if (room.archetype == RoomArchetype::BROKEN_ARENA) pitTarget = 1;
            if (room.archetype == RoomArchetype::HAZARD_ROOM) pitTarget = 2;
            if (room.archetype == RoomArchetype::RITUAL_ROOM) pitTarget = 2;
            if (room.theme == DungeonTheme::CRYPT) pitTarget++;
            if (room.theme == DungeonTheme::DRACONIC) pitTarget += 2;
            pitTarget += themeProfile.pitBonus;
            pitTarget = std::clamp(pitTarget, 0, 4);

            std::vector<int> pitIndices;
            for (int i = 0; i < (int)(sizeof(PIT_SPOTS) / sizeof(PIT_SPOTS[0])); ++i) {
                Rect pitRect = { PIT_SPOTS[i].x, PIT_SPOTS[i].y, 14.0f, 14.0f };
                if (RectsOverlap(pitRect, playerCore)) continue;
                if (OverlapsBlocking(pitRect)) continue;
                pitIndices.push_back(i);
            }

            for (int i = 0; i < pitTarget && !pitIndices.empty(); ++i) {
                int choiceIndex = RNG::Range(0, (int)pitIndices.size() - 1);
                int spotIndex = pitIndices[choiceIndex];
                pitIndices.erase(pitIndices.begin() + choiceIndex);

                RoomTerrainFeature feature;
                feature.pos = PIT_SPOTS[spotIndex];
                feature.w = 14.0f;
                feature.h = 14.0f;
                feature.type = RoomTerrainType::PIT;
                AddFeature(feature);
            }

            if ((room.type == RoomType::BOSS || room.archetype == RoomArchetype::RITUAL_ROOM) && RNG::Chance(0.60f)) {
                RoomTerrainFeature leftPad;
                leftPad.type = RoomTerrainType::TELEPORT_PAD;
                leftPad.linkId = 1;
                leftPad.pos = { 54.0f, 70.0f };
                leftPad.w = 12.0f;
                leftPad.h = 12.0f;
                if (!OverlapsAny(leftPad.GetRect())) AddFeature(leftPad);

                RoomTerrainFeature rightPad = leftPad;
                rightPad.pos = { 254.0f, 70.0f };
                if (!OverlapsAny(rightPad.GetRect())) AddFeature(rightPad);
            }

            if (room.theme == DungeonTheme::FUNGAL && RNG::Chance(0.60f)) {
                RoomTerrainFeature web;
                web.type = RNG::Chance(0.50f) ? RoomTerrainType::TERRAIN_WEB : RoomTerrainType::TERRAIN_SLIME;
                web.pos = { 140.0f, 54.0f };
                web.w = 20.0f;
                web.h = 20.0f;
                if (!OverlapsAny(web.GetRect())) AddFeature(web);
            }

            if (room.theme == DungeonTheme::CRYPT && RNG::Chance(0.50f)) {
                RoomTerrainFeature acid;
                acid.type = RNG::Chance(0.50f) ? RoomTerrainType::TERRAIN_ACID : RoomTerrainType::TERRAIN_POISON;
                acid.pos = { 140.0f, 104.0f };
                acid.w = 20.0f;
                acid.h = 20.0f;
                if (!OverlapsAny(acid.GetRect())) AddFeature(acid);
            }

            if (room.type == RoomType::CURSE && RNG::Chance(0.50f)) {
                RoomTerrainFeature plate;
                plate.type = RoomTerrainType::PRESSURE_PLATE;
                plate.linkId = 2;
                plate.pos = { 150.0f, 82.0f };
                plate.w = 12.0f;
                plate.h = 12.0f;
                if (!OverlapsAny(plate.GetRect())) AddFeature(plate);
            }
        };

        bool terrainOk = true;
        if (room.archetype == RoomArchetype::BROKEN_ARENA || room.archetype == RoomArchetype::HAZARD_ROOM) {
            auto terrain = TerrainGen::GenerateCellularTerrain(room, floor);
            terrainOk = !terrain.empty();
            for (auto feature : terrain) {
                AddFeature(feature);
            }
        } else if (room.archetype == RoomArchetype::PILLAR_FIELD ||
                   room.archetype == RoomArchetype::RITUAL_ROOM ||
                   room.archetype == RoomArchetype::WHISPERING_STACKS ||
                   room.archetype == RoomArchetype::RUNIC_LATTICE ||
                   room.archetype == RoomArchetype::FORKING_PATH ||
                   room.archetype == RoomArchetype::SPIRE_ASCENT ||
                   room.archetype == RoomArchetype::FLOODED_CHAMBER ||
                   room.archetype == RoomArchetype::COLLAPSED_VAULT ||
                   room.archetype == RoomArchetype::SENTRY_HALL ||
                   room.archetype == RoomArchetype::GARDEN_MAZE ||
                   room.archetype == RoomArchetype::SHATTERED_BRIDGE ||
                   room.archetype == RoomArchetype::ECHO_ROOM ||
                   room.archetype == RoomArchetype::THRONE_APPROACH ||
                   room.archetype == RoomArchetype::TWIN_ISLANDS ||
                   room.archetype == RoomArchetype::NARROW_VEINS ||
                   room.archetype == RoomArchetype::AMPHITHEATER) {
            auto terrain = TerrainGen::GenerateArchetypeTerrain(room, floor);
            terrainOk = !terrain.empty();
            for (auto feature : terrain) {
                AddFeature(feature);
            }
        } else {
            GenerateRuleBasedTerrain();
        }

        if (!terrainOk) return false;
        GenerateTraps();
        return ValidateTerrainLayout(room);
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

    LoadDefaultThemeProfiles();
    LoadThemeProfiles("data/themes.txt");

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
        totalFloors = std::min(settings.totalFloors, MetaProgression::MaxFloorCap());
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
    totalFloors = std::min(settings.totalFloors, MetaProgression::MaxFloorCap());
    currentFloor = std::max(1, std::min(floorNumber, totalFloors));

    int gridSize = std::min(currentFloor * settings.gridSizePerFloor, settings.gridSizeMax);
    gridWidth = gridSize;
    gridHeight = gridSize;

    const int normalRoomTarget = settings.normalRoomBase + settings.normalRoomPerFloor * std::max(0, currentFloor - 1);

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
        for (int v = 0; v < MAX_BOSS_TEMPLATES; ++v) {
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

        DungeonTheme floorTheme = RollDungeonTheme(currentFloor);
        const DungeonThemeProfile& themeProfile = ThemeProfileFor(floorTheme);
        for (auto& room : rooms) {
            room.theme = floorTheme;
            room.archetype = RollArchetype(room.type, currentFloor);
            room.encounterFamily = RollEncounterFamily(room.archetype, currentFloor);
            if (room.type == RoomType::BOSS) {
                room.archetype = RoomArchetype::RITUAL_ROOM;
                room.encounterFamily = RoomEncounterFamily::ELITE;
            } else if (room.type == RoomType::START || room.type == RoomType::TREASURE || room.type == RoomType::SHOP) {
                room.archetype = RoomArchetype::OPEN_ARENA;
                room.encounterFamily = RoomEncounterFamily::MIXED;
            }
        }

        auto PickBossVariantForTheme = [&](const DungeonThemeProfile& profile) {
            std::vector<int> themedVariants;
            if (profile.bossPools[0] != '\0') {
                char buffer[512];
                std::strncpy(buffer, profile.bossPools, sizeof(buffer) - 1);
                buffer[sizeof(buffer) - 1] = '\0';
                char* token = std::strtok(buffer, ",");
                while (token) {
                    while (*token == ' ' || *token == '\t') ++token;
                    char* end = token + std::strlen(token);
                    while (end > token && (end[-1] == ' ' || end[-1] == '\t')) --end;
                    *end = '\0';
                    int bossIndex = BossDatabase::IndexOf(token);
                    if (bossIndex >= 0 && !MetaProgression::IsBossUnlocked(token)) bossIndex = -1;
                    if (bossIndex >= 0) themedVariants.push_back(bossIndex);
                    token = std::strtok(nullptr, ",");
                }
            }

            if (themedVariants.empty()) {
                for (int v = 0; v < MAX_BOSS_TEMPLATES; ++v) {
                    if (BOSS_VARIANT_TIER[v] <= currentFloor) themedVariants.push_back(v);
                }
            }
            if (themedVariants.empty()) return 0;
            return themedVariants[RNG::Range(0, (int)themedVariants.size() - 1)];
        };

        DungeonThemeProfile effectiveThemeProfile = themeProfile;
        const char* effectiveBossPools = MetaProgression::BossPoolForTheme(themeProfile.name, themeProfile.bossPools);
        if (effectiveBossPools) {
            std::strncpy(effectiveThemeProfile.bossPools, effectiveBossPools, sizeof(effectiveThemeProfile.bossPools) - 1);
            effectiveThemeProfile.bossPools[sizeof(effectiveThemeProfile.bossPools) - 1] = '\0';
        }
        rooms[bossRoomIndex].bossVariant = PickBossVariantForTheme(effectiveThemeProfile);

        const int enemyCount = EnemyDatabase::Count();
        const int maxEnemyTier = std::max(1, EnemyDatabase::MaxTier());
        const float floorProgress = (totalFloors > 1)
            ? (float)(currentFloor - 1) / (float)(totalFloors - 1)
            : 0.0f;
        const int allowedEnemyTier = std::clamp(
            1 + (int)std::lround(floorProgress * (float)(maxEnemyTier - 1)),
            1,
            maxEnemyTier
        );
        const int itemCount = ItemDatabase::Count();

        std::vector<int> eligibleEnemyIndices;
        for (int e = 0; e < enemyCount; ++e) {
            if (EnemyDatabase::Tier(e) <= allowedEnemyTier) eligibleEnemyIndices.push_back(e);
        }
        if (eligibleEnemyIndices.empty()) {
            for (int e = 0; e < enemyCount; ++e) eligibleEnemyIndices.push_back(e); // safety fallback
        }

        std::vector<int> themedEnemyIndices;
        const char* effectiveEnemyPools = MetaProgression::EnemyPoolForTheme(themeProfile.name, themeProfile.enemyPools);
        if (effectiveEnemyPools && effectiveEnemyPools[0] != '\0') {
            char buffer[512];
            std::strncpy(buffer, effectiveEnemyPools, sizeof(buffer) - 1);
            buffer[sizeof(buffer) - 1] = '\0';
            char* token = std::strtok(buffer, ",");
            while (token) {
                while (*token == ' ' || *token == '\t') ++token;
                char* end = token + std::strlen(token);
                while (end > token && (end[-1] == ' ' || end[-1] == '\t')) --end;
                *end = '\0';
                int enemyIndex = EnemyDatabase::IndexOf(token);
                if (enemyIndex >= 0 && EnemyDatabase::Tier(enemyIndex) <= currentFloor) {
                    themedEnemyIndices.push_back(enemyIndex);
                }
                token = std::strtok(nullptr, ",");
            }
        }

        auto PickEnemyIndex = [&](RoomEncounterFamily family) {
            const std::vector<int>& source = themedEnemyIndices.empty() ? eligibleEnemyIndices : themedEnemyIndices;
            if (source.empty()) return -1;

            int start = 0;
            int end = (int)source.size() - 1;
            if (family == RoomEncounterFamily::RUSH) {
                end = std::max(0, (int)source.size() / 2);
            } else if (family == RoomEncounterFamily::ARTILLERY || family == RoomEncounterFamily::ELITE) {
                start = std::max(0, (int)source.size() / 3);
            } else if (family == RoomEncounterFamily::GUARDIAN) {
                start = std::max(0, (int)source.size() / 4);
            }

            if (start > end) std::swap(start, end);
            int choice = RNG::Range(start, end);
            choice = std::clamp(choice, 0, (int)source.size() - 1);
            return source[choice];
        };

        auto PickEnemyIndexForBudget = [&](RoomEncounterFamily family, int budget) {
            const std::vector<int>& source = themedEnemyIndices.empty() ? eligibleEnemyIndices : themedEnemyIndices;
            if (source.empty() || budget <= 0) return -1;

            const int preferredTier = allowedEnemyTier;

            std::vector<std::pair<int, int>> candidates;
            candidates.reserve(source.size());
            for (int enemyIndex : source) {
                int cost = EnemyThreatCost(enemyIndex);
                if (cost <= budget) {
                    int tier = EnemyDatabase::Tier(enemyIndex);
                    int weight = std::max(1, 8 - std::abs(tier - preferredTier) * 2);
                    if (family == RoomEncounterFamily::RUSH) {
                        weight += (tier <= preferredTier ? 2 : 0);
                    } else if (family == RoomEncounterFamily::ARTILLERY || family == RoomEncounterFamily::ELITE) {
                        weight += (tier >= preferredTier ? 2 : 0);
                    } else if (family == RoomEncounterFamily::GUARDIAN) {
                        weight += 1;
                    }
                    candidates.push_back({ enemyIndex, weight });
                }
            }

            if (candidates.empty()) return -1;

            int totalWeight = 0;
            for (const auto& c : candidates) totalWeight += c.second;
            int roll = RNG::Range(1, totalWeight);
            for (const auto& c : candidates) {
                roll -= c.second;
                if (roll <= 0) return c.first;
            }

            return candidates.back().first;
        };

        for (int i = 0; i < (int)rooms.size(); ++i) {
            rooms[i].enemySpawnList.clear();
            rooms[i].itemSpawnList.clear();
            int roomThreatRemaining = std::max(0, rooms[i].threatBudget - rooms[i].threatSpent);

            if (rooms[i].type == RoomType::NORMAL && enemyCount > 0) {
                if (RNG::Chance(0.10f)) {
                    continue;
                }

                int distBonus = distFromStart[i];
                int enemyBudget = std::max(roomThreatRemaining + 2, 6 + currentFloor * 2);
                enemyBudget += (distBonus > 2 ? settings.deepRoomBonus : 0);
                enemyBudget += std::max(0, EncounterFamilySpawnBias(rooms[i].encounterFamily));
                if (rooms[i].theme == DungeonTheme::DRACONIC) enemyBudget += 1;

                int spent = 0;
                int enemySlots = 0;
                int minEnemySlots = (currentFloor == 1) ? 2 : 3;
                while ((spent < enemyBudget || enemySlots < minEnemySlots) && enemySlots < 12) {
                    int budgetLeft = enemyBudget - spent;
                    int enemyIndex = PickEnemyIndexForBudget(rooms[i].encounterFamily, budgetLeft);
                    if (enemyIndex < 0) break;
                    int cost = EnemyThreatCost(enemyIndex);
                    if (cost > budgetLeft) break;
                    rooms[i].enemySpawnList.push_back(enemyIndex);
                    spent += cost;
                    enemySlots++;
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
                    int enemyBudget = std::max(roomThreatRemaining + 2, 7 + currentFloor * 2);
                    enemyBudget += (distBonus > 2 ? settings.deepRoomBonus : 0);
                    enemyBudget += std::max(0, EncounterFamilySpawnBias(rooms[i].encounterFamily));
                    int spent = 0;
                    int enemySlots = 0;
                    int minEnemySlots = (currentFloor == 1) ? 2 : 3;
                    while ((spent < enemyBudget || enemySlots < minEnemySlots) && enemySlots < 12) {
                        int budgetLeft = enemyBudget - spent;
                        int enemyIndex = PickEnemyIndexForBudget(rooms[i].encounterFamily, budgetLeft);
                        if (enemyIndex < 0) break;
                        int cost = EnemyThreatCost(enemyIndex);
                        if (cost > budgetLeft) break;
                        rooms[i].enemySpawnList.push_back(enemyIndex);
                        spent += cost;
                        enemySlots++;
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
        bool terrainOk = true;
        for (auto& room : rooms) {
            if (!PopulateRoomHazards(room, currentFloor)) {
                terrainOk = false;
                break;
            }
        }
        if (!terrainOk) continue;
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

bool Dungeon::ValidateRoomTerrain(const Room& room) const {
    return ValidateTerrainLayout(room);
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

    if (!rollCurseReward || room.lootGranted) return;
    bool rewardRoom = room.type == RoomType::NORMAL || room.type == RoomType::BOSS || room.IsEnemyCurseRoom();
    if (!rewardRoom) return;

    const DungeonThemeProfile& themeProfile = ThemeProfileFor(room.theme);

    auto MakePickup = [&](RoomPickupType type, int amount = 0, float offsetX = 0.0f, float offsetY = 0.0f) {
        RoomPickup pickup;
        pickup.type = type;
        pickup.amount = amount;
        pickup.pos = {
            room.x + room.width * 0.5f - 4.0f + offsetX,
            room.y + room.height * 0.5f - 4.0f + offsetY
        };
        room.pickups.push_back(pickup);
    };

    float roll = std::clamp(RNG::Range(0.0f, 1.0f) - 0.03f * (float)themeProfile.rewardBonus, 0.0f, 1.0f);
    if (roll < 0.10f) {
        RoomPickup pickup;
        pickup.type = RoomPickupType::CHEST;
        pickup.chestType = RollChestTypeForFloor(currentFloor);
        pickup.itemId = PickChestItem(pickup.chestType, currentFloor);
        pickup.pos = { room.x + room.width * 0.5f - 4.0f, room.y + room.height * 0.5f - 4.0f };
        room.pickups.push_back(pickup);
    } else if (roll < 0.10f + settings.heartDropChance) {
        MakePickup(RoomPickupType::HEART, 0, 0.0f, -10.0f);
    } else if (roll < 0.10f + settings.heartDropChance + settings.bombDropChance) {
        MakePickup(RoomPickupType::BOMB, 0, 0.0f, -10.0f);
    } else if (roll < 0.10f + settings.heartDropChance + settings.bombDropChance + settings.coinDropChance) {
        int coinValue = (RNG::Chance(0.08f)) ? 10 : (RNG::Chance(0.25f) ? 5 : 1);
        MakePickup(RoomPickupType::COIN, coinValue, 0.0f, -10.0f);
    } else if (RNG::Chance(settings.keyDropChance)) {
        MakePickup(RoomPickupType::KEY, 1, 0.0f, -10.0f);
    }

    room.lootGranted = true;
}
