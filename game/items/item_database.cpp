#include "item_database.h"
#include "../../engine/data_parser.h"
#include <cstring>
#include <vector>

namespace {
    ItemTemplate g_templates[MAX_ITEM_TEMPLATES];
    int g_templateCount = 0;

    ItemType ParseType(const char* s) {
        if (std::strcmp(s, "unlock") == 0) return ItemType::UNLOCK;
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
        item.type = ParseType(block.GetString("type", "stat_mod"));
        item.stat = ParseStat(block.GetString("stat", "damage"));
        item.mode = ParseMode(block.GetString("mode", "add"));
        item.flag = ParseFlag(block.GetString("flag", ""));
        item.value = block.GetFloat("value", 0.0f);

        // Optional second effect for trade-off items.
        item.stat2 = ParseStat(block.GetString("stat2", "unknown"));
        item.mode2 = ParseMode(block.GetString("mode2", "add"));
        item.value2 = block.GetFloat("value2", 0.0f);
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