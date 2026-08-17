#pragma once

enum class ItemType {
    STAT_MOD,
    UNLOCK,
    PROC_SYNERGY
};

enum class ItemStat {
    DAMAGE,
    DAMAGE_MULTIPLIER,
    SHOT_SPEED,
    RANGE,
    FIRE_RATE,
    FIRE_RATE_MULTIPLIER,
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
    CRIT_CHANCE,
    LIFESTEAL_CHANCE,
    BURN_CHANCE,
    FREEZE_CHANCE,
    MAGNET_CHANCE,
    BOOMERANG_CHANCE,
    GROWING_CHANCE,
    SHRINKING_CHANCE,
    CHAIN_CHANCE,
    GRAVITY_CHANCE,
    VORTEX_CHANCE,
    MARK_CHANCE,
    WALL_BOUNCE_CHANCE,
    ENEMY_BOUNCE_CHANCE,
    SPLIT_CHANCE,
    DODGE_CHANCE,
    DAMAGE_REDUCTION,
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
    VOID_HEART,
    TWIN_SOUL,
    PARASITE_CORE,
    LAST_SHOT,
    DEVASTATOR,
    INFINITE_LOOP,
    CHAOS_ENGINE,
    SATELLITES,
    CHARGED_SHOTS,
    SHIELD_CHARM,
    GUARDIAN_ANGEL,
    SPIKED_ARMOR,
    SECOND_WIND,
    IRON_WILL,
    COMPASS,
    TREASURE_SENSE,
    MARTYRDOM,
    OVERCLOCK,
    HOLLOW_CORE,
    SECOND_SUN,
    BURST_SHOTS,
    ROCKET_ROUNDS,
    LASER_LENS,
    CRIMSON_RAY,
    BLADE_ARC,
    BULWARK_CORE,
    THORN_MANTLE,
    REGEN_CHARM,
    MIRROR_WARD,
    PHOENIX_FEATHER,
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
    
    int tier = 1;  // 1-5, with 5 being strongest
    char pools[64] = {};  // comma-separated: TREASURE,BOSS,CURSE,SHOP,CHEST
};
