#include "item_database.h"
#include "../../engine/data_parser.h"
#include <cstring>
#include <vector>

namespace {
    ItemTemplate g_templates[MAX_ITEM_TEMPLATES];
    int g_templateCount = 0;

    ItemType ParseType(const char* s) {
        if (std::strcmp(s, "unlock") == 0) return ItemType::UNLOCK;
        if (std::strcmp(s, "proc_synergy") == 0) return ItemType::PROC_SYNERGY;
        return ItemType::STAT_MOD;
    }

    ItemStat ParseStat(const char* s) {
        if (std::strcmp(s, "damage") == 0) return ItemStat::DAMAGE;
        if (std::strcmp(s, "shotSpeed") == 0) return ItemStat::SHOT_SPEED;
        if (std::strcmp(s, "range") == 0) return ItemStat::RANGE;
        if (std::strcmp(s, "fireRate") == 0) return ItemStat::FIRE_RATE;
        if (std::strcmp(s, "projectileCount") == 0) return ItemStat::PROJECTILE_COUNT;
        if (std::strcmp(s, "moveSpeed") == 0) return ItemStat::MOVE_SPEED;
        if (std::strcmp(s, "luck") == 0) return ItemStat::LUCK;
        if (std::strcmp(s, "maxHp") == 0) return ItemStat::MAX_HP;
        if (std::strcmp(s, "heal") == 0) return ItemStat::HEAL;
        if (std::strcmp(s, "homingChance") == 0) return ItemStat::HOMING_CHANCE;
        if (std::strcmp(s, "poisonChance") == 0) return ItemStat::POISON_CHANCE;
        if (std::strcmp(s, "stickyChance") == 0) return ItemStat::STICKY_CHANCE;
        if (std::strcmp(s, "piercingChance") == 0) return ItemStat::PIERCING_CHANCE;
        if (std::strcmp(s, "explosiveChance") == 0) return ItemStat::EXPLOSIVE_CHANCE;
        if (std::strcmp(s, "dashSpeed") == 0) return ItemStat::DASH_SPEED;
        if (std::strcmp(s, "dashDuration") == 0) return ItemStat::DASH_DURATION;
        if (std::strcmp(s, "dashCooldown") == 0) return ItemStat::DASH_COOLDOWN;
        if (std::strcmp(s, "critChance") == 0) return ItemStat::CRIT_CHANCE;
        if (std::strcmp(s, "lifestealChance") == 0) return ItemStat::LIFESTEAL_CHANCE;
        if (std::strcmp(s, "burnChance") == 0) return ItemStat::BURN_CHANCE;
        if (std::strcmp(s, "freezeChance") == 0) return ItemStat::FREEZE_CHANCE;
        if (std::strcmp(s, "magnetChance") == 0) return ItemStat::MAGNET_CHANCE;
        if (std::strcmp(s, "boomerangChance") == 0) return ItemStat::BOOMERANG_CHANCE;
        if (std::strcmp(s, "growingChance") == 0) return ItemStat::GROWING_CHANCE;
        if (std::strcmp(s, "shrinkingChance") == 0) return ItemStat::SHRINKING_CHANCE;
        if (std::strcmp(s, "chainChance") == 0) return ItemStat::CHAIN_CHANCE;
        if (std::strcmp(s, "gravityChance") == 0) return ItemStat::GRAVITY_CHANCE;
        if (std::strcmp(s, "vortexChance") == 0) return ItemStat::VORTEX_CHANCE;
        if (std::strcmp(s, "markChance") == 0) return ItemStat::MARK_CHANCE;
        if (std::strcmp(s, "wallBounceChance") == 0) return ItemStat::WALL_BOUNCE_CHANCE;
        if (std::strcmp(s, "enemyBounceChance") == 0) return ItemStat::ENEMY_BOUNCE_CHANCE;
        if (std::strcmp(s, "splitChance") == 0) return ItemStat::SPLIT_CHANCE;
        if (std::strcmp(s, "dodgeChance") == 0) return ItemStat::DODGE_CHANCE;
        return ItemStat::UNKNOWN;
    }

    ItemMode ParseMode(const char* s) {
        if (std::strcmp(s, "multiply") == 0) return ItemMode::MULTIPLY;
        return ItemMode::ADD;
    }

    ItemFlag ParseFlag(const char* s) {
        if (std::strcmp(s, "diagonal_fire") == 0) return ItemFlag::DIAGONAL_FIRE;
        if (std::strcmp(s, "dash") == 0) return ItemFlag::DASH;
        if (std::strcmp(s, "homing") == 0) return ItemFlag::HOMING;
        if (std::strcmp(s, "void_heart") == 0) return ItemFlag::VOID_HEART;
        if (std::strcmp(s, "twin_soul") == 0) return ItemFlag::TWIN_SOUL;
        if (std::strcmp(s, "parasite_core") == 0) return ItemFlag::PARASITE_CORE;
        if (std::strcmp(s, "last_shot") == 0) return ItemFlag::LAST_SHOT;
        if (std::strcmp(s, "devastator") == 0) return ItemFlag::DEVASTATOR;
        if (std::strcmp(s, "infinite_loop") == 0) return ItemFlag::INFINITE_LOOP;
        if (std::strcmp(s, "chaos_engine") == 0) return ItemFlag::CHAOS_ENGINE;
        if (std::strcmp(s, "satellites") == 0) return ItemFlag::SATELLITES;
        if (std::strcmp(s, "charged_shots") == 0) return ItemFlag::CHARGED_SHOTS;
        if (std::strcmp(s, "shield_charm") == 0) return ItemFlag::SHIELD_CHARM;
        if (std::strcmp(s, "guardian_angel") == 0) return ItemFlag::GUARDIAN_ANGEL;
        if (std::strcmp(s, "spiked_armor") == 0) return ItemFlag::SPIKED_ARMOR;
        if (std::strcmp(s, "second_wind") == 0) return ItemFlag::SECOND_WIND;
        if (std::strcmp(s, "iron_will") == 0) return ItemFlag::IRON_WILL;
        if (std::strcmp(s, "compass") == 0) return ItemFlag::COMPASS;
        if (std::strcmp(s, "treasure_sense") == 0) return ItemFlag::TREASURE_SENSE;
        if (std::strcmp(s, "martyrdom") == 0) return ItemFlag::MARTYRDOM;
        if (std::strcmp(s, "overclock") == 0) return ItemFlag::OVERCLOCK;
        if (std::strcmp(s, "hollow_core") == 0) return ItemFlag::HOLLOW_CORE;
        if (std::strcmp(s, "second_sun") == 0) return ItemFlag::SECOND_SUN;
        return ItemFlag::UNKNOWN;
    }

    const ItemTemplate* FindInternal(const char* name) {
        for (int i = 0; i < g_templateCount; ++i) {
            if (std::strcmp(g_templates[i].name, name) == 0) return &g_templates[i];
        }
        return nullptr;
    }
}

namespace ItemDatabase {

bool Load(const char* path) {
    std::vector<DataBlock> blocks = DataParser::ParseFile(path);
    if (blocks.empty()) return false;

    g_templateCount = 0;
    for (auto& block : blocks) {
        if (g_templateCount >= MAX_ITEM_TEMPLATES) break;

        
        ItemTemplate& item = g_templates[g_templateCount++];
        std::strncpy(item.name, block.name, sizeof(item.name) - 1);
        std::strncpy(item.desc, block.GetString("desc", "???"), sizeof(item.desc) - 1);
        item.desc[sizeof(item.desc) - 1] = '\0';
        item.type = ParseType(block.GetString("type", "stat_mod"));
        item.stat = ParseStat(block.GetString("stat", "damage"));
        item.mode = ParseMode(block.GetString("mode", "add"));
        item.flag = ParseFlag(block.GetString("flag", ""));
        item.value = block.GetFloat("value", 0.0f);

        // Optional second effect for trade-off items.
        item.stat2 = ParseStat(block.GetString("stat2", "unknown"));
        item.mode2 = ParseMode(block.GetString("mode2", "add"));
        item.value2 = block.GetFloat("value2", 0.0f);
        item.perProcValue = block.GetFloat("per_proc_value", 0.0f);
    }

    return true;
}

int Count() {
    return g_templateCount;
}

const ItemTemplate* Get(int index) {
    if (index < 0 || index >= g_templateCount) return nullptr;
    return &g_templates[index];
}

const ItemTemplate* Find(const char* name) {
    return FindInternal(name);
}

int IndexOf(const char* name) {
    for (int i = 0; i < g_templateCount; ++i) {
        if (std::strcmp(g_templates[i].name, name) == 0) return i;
    }
    return -1;
}

} // namespace ItemDatabase
