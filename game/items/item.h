#pragma once

enum class ItemType {
    STAT_MOD,
    UNLOCK
};

enum class ItemStat {
    DAMAGE,
    SHOT_SPEED,
    RANGE,
    FIRE_RATE,
    PROJECTILE_COUNT,
    MOVE_SPEED,
    MAX_HP,
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
    UNKNOWN
};

struct ItemTemplate {
    char name[32] = {};
    ItemType type = ItemType::STAT_MOD;
    ItemStat stat = ItemStat::DAMAGE;
    ItemMode mode = ItemMode::ADD;
    ItemFlag flag = ItemFlag::UNKNOWN;
    float value = 0.0f;
};
