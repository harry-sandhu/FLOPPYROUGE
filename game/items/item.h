#pragma once

enum class ItemType {
    STAT_MOD,
    UNLOCK,
    PROC_SYNERGY
};

enum class ItemStat {
    DAMAGE,
    SHOT_SPEED,
    RANGE,
    FIRE_RATE,
    PROJECTILE_COUNT,
    MOVE_SPEED,
    LUCK,
    MAX_HP,
    HEAL,
    HOMING_CHANCE,
    POISON_CHANCE,
    STICKY_CHANCE,
    PIERCING_CHANCE,
    EXPLOSIVE_CHANCE,
    DASH_SPEED,
    DASH_DURATION,
    DASH_COOLDOWN,
    UNKNOWN
};

enum class ItemMode {
    ADD,
    MULTIPLY
};

enum class ItemFlag {
    DIAGONAL_FIRE,
    DASH,
    HOMING,
    UNKNOWN
};

struct ItemTemplate {
    char name[32] = {};
    char desc[48] = {};
    ItemType type = ItemType::STAT_MOD;
    ItemStat stat = ItemStat::DAMAGE;
    ItemMode mode = ItemMode::ADD;
    ItemFlag flag = ItemFlag::UNKNOWN;
    float value = 0.0f;

    ItemStat stat2 = ItemStat::UNKNOWN;
    ItemMode mode2 = ItemMode::ADD;
    float value2 = 0.0f;
    float perProcValue = 0.0f;
};