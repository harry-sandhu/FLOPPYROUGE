#include "meta_progression.h"
#include "../bosses/boss_database.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace {
    constexpr const char* SAVE_PATH = "floppyrogue_meta.sav";
    MetaProgression::State g_state;

    bool HasToken(const char* list, const char* token) {
        if (!list || !token || token[0] == '\0') return false;

        char buffer[256];
        std::strncpy(buffer, list, sizeof(buffer) - 1);
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

    bool IsSecretTierOne(const char* name) {
        return std::strcmp(name, "Martyrdom") == 0 ||
               std::strcmp(name, "ChaosEngine") == 0 ||
               std::strcmp(name, "HollowCore") == 0;
    }

    bool IsOrphanBossWaveOne(const char* name) {
        return std::strcmp(name, "Mirrorfiend") == 0 ||
               std::strcmp(name, "Siren") == 0 ||
               std::strcmp(name, "Burrower") == 0 ||
               std::strcmp(name, "Railwing") == 0 ||
               std::strcmp(name, "Riftmother") == 0 ||
               std::strcmp(name, "WebMother") == 0;
    }

    bool IsOrphanBossWaveTwo(const char* name) {
        return std::strcmp(name, "Hexcaller") == 0 ||
               std::strcmp(name, "Bulwark") == 0 ||
               std::strcmp(name, "Ravager") == 0 ||
               std::strcmp(name, "Stormeye") == 0 ||
               std::strcmp(name, "Titan") == 0 ||
               std::strcmp(name, "Chorus") == 0 ||
               std::strcmp(name, "RedWarden") == 0 ||
               std::strcmp(name, "Ashcaller") == 0 ||
               std::strcmp(name, "Frostgrip") == 0 ||
               std::strcmp(name, "Duskfang") == 0;
    }
}

namespace MetaProgression {

void Load() {
    g_state = State{};

    FILE* file = std::fopen(SAVE_PATH, "rb");
    if (!file) return;

    char magic[8] = {};
    std::fread(magic, 1, sizeof(magic), file);
    if (std::memcmp(magic, "FRMETA1", 7) == 0) {
        std::fread(&g_state, sizeof(g_state), 1, file);
    }
    std::fclose(file);
}

void Save() {
    FILE* file = std::fopen(SAVE_PATH, "wb");
    if (!file) return;

    char magic[8] = "FRMETA1";
    std::fwrite(magic, 1, sizeof(magic), file);
    std::fwrite(&g_state, sizeof(g_state), 1, file);
    std::fclose(file);
}

void Reset() {
    g_state = State{};
    Save();
}

const State& Get() {
    return g_state;
}

int MaxFloorCap() {
    if (g_state.floorClears[5] >= 2) return 8;
    if (g_state.floorClears[4] >= 1) return 5;
    if (g_state.floorClears[3] >= 2) return 4;
    return 3;
}

int MaxUnlockedItemTier() {
    if (g_state.floorClears[8] >= 1) return 5;
    if (g_state.floorClears[5] >= 1) return 4;
    if (g_state.floorClears[3] >= 1) return 3;
    return 2;
}

bool IsItemUnlocked(const char* name, int tier, const char* pools) {
    if (!name) return false;
    if (IsSecretTierOne(name)) return g_state.floorClears[3] >= 3;
    if (tier > MaxUnlockedItemTier()) return false;

    // Curse-pool items are hidden from fresh saves, then join their tier
    // milestone normally once the player has proven the early game.
    if (HasToken(pools, "CURSE") && g_state.floorClears[3] < 1) return false;
    return true;
}

bool IsGambleChestUnlocked() {
    return g_state.floorClears[8] >= 1;
}

bool IsTrueDragonUnlocked() {
    return g_state.floorClears[8] >= 3;
}

bool IsCuratedDraconicUnlocked() {
    return g_state.floorClears[5] >= 3;
}

bool IsBossUnlocked(const char* name) {
    if (!name || name[0] == '\0') return false;
    if (IsOrphanBossWaveOne(name)) return g_state.floorClears[5] >= 2;
    if (IsOrphanBossWaveTwo(name)) return g_state.floorClears[8] >= 2;
    return true;
}

const char* EnemyPoolForTheme(const char* themeName, const char* originalPool) {
    if (!themeName) return originalPool;
    if (std::strcmp(themeName, "DRACONIC") == 0 && IsCuratedDraconicUnlocked()) {
        return "ApexWarden,NullTitan,VoidGuardian,RuinSovereign,BulwarkOracle,DreadHarrier,RiftHarbinger,OblivionEye,CatacombReaper,DeepBurrower,DoomLattice,HexApex,DreadPillar,SwarmApex,SwarmIcon,BroodMonarch,SwarmMarshal,RiftGrub";
    }
    return originalPool;
}

const char* BossPoolForTheme(const char* themeName, const char* originalPool) {
    if (!themeName) return originalPool;

    if (std::strcmp(themeName, "DRACONIC") == 0) {
        if (IsTrueDragonUnlocked()) return "DragonSovereign";
        if (g_state.floorClears[8] >= 2) {
            return "DragonSovereign,Eclipse,Emberlord,Linker,SwarmLeader,Coward,Patroller,Mirrorfiend,Siren,Burrower,Railwing,Riftmother,WebMother,Hexcaller,Bulwark,Ravager,Stormeye,Titan,Chorus,RedWarden,Ashcaller,Frostgrip,Duskfang";
        }
        if (g_state.floorClears[5] >= 2) {
            return "DragonSovereign,Eclipse,Emberlord,Linker,SwarmLeader,Coward,Patroller,Mirrorfiend,Siren,Burrower,Railwing,Riftmother,WebMother";
        }
        if (g_state.floorClears[5] >= 1) {
            return "DragonSovereign,Eclipse,Emberlord,Linker,SwarmLeader,Coward,Patroller";
        }
    }

    return originalPool;
}

void RecordBossClear(int floor, int bossIndex) {
    floor = std::clamp(floor, 1, 8);
    g_state.floorClears[floor]++;

    if (bossIndex >= 0 && bossIndex < 36) {
        g_state.bossDefeats[bossIndex]++;
    }

    Save();
}

} // namespace MetaProgression
