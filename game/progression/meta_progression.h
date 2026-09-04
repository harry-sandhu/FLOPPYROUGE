#pragma once

namespace MetaProgression {
    struct State {
        int floorClears[9] = {};
        int bossDefeats[36] = {};
    };

    void Load();
    void Save();
    void Reset();
    const State& Get();

    int MaxFloorCap();
    int MaxUnlockedItemTier();
    bool IsItemUnlocked(const char* name, int tier, const char* pools);
    bool IsGambleChestUnlocked();
    bool IsTrueDragonUnlocked();
    bool IsCuratedDraconicUnlocked();
    bool IsBossUnlocked(const char* name);

    const char* EnemyPoolForTheme(const char* themeName, const char* originalPool);
    const char* BossPoolForTheme(const char* themeName, const char* originalPool);

    void RecordBossClear(int floor, int bossIndex);
}
