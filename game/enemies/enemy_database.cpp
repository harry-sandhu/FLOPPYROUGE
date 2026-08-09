#include "enemy_database.h"
#include "../../engine/data_parser.h"
#include <cstring>

namespace {
    EnemyTemplate g_templates[MAX_ENEMY_TEMPLATES];
    int g_templateCount = 0;

    AIType ParseAIType(const char* s) {
        if (std::strcmp(s, "SHOOTER") == 0) return AIType::SHOOTER;
        if (std::strcmp(s, "CHARGER") == 0) return AIType::CHARGER;
        if (std::strcmp(s, "SUMMONER") == 0) return AIType::SUMMONER;
        if (std::strcmp(s, "EXPLODER") == 0) return AIType::EXPLODER;
        return AIType::CHASER;
    }

    const EnemyTemplate* Find(const char* name) {
        for (int i = 0; i < g_templateCount; ++i) {
            if (std::strcmp(g_templates[i].name, name) == 0) return &g_templates[i];
        }
        return nullptr;
    }
}

namespace EnemyDatabase {

bool Load(const char* path) {
    std::vector<DataBlock> blocks = DataParser::ParseFile(path);
    if (blocks.empty()) return false;

    g_templateCount = 0;
    for (auto& block : blocks) {
        if (g_templateCount >= MAX_ENEMY_TEMPLATES) break;

        EnemyTemplate& t = g_templates[g_templateCount++];
        std::strncpy(t.name, block.name, sizeof(t.name) - 1);
        t.aiType = ParseAIType(block.GetString("ai", "CHASER"));
        t.hp = block.GetInt("hp", 30);
        t.speed = block.GetFloat("speed", 40.0f);
        t.w = block.GetFloat("w", 12.0f);
        t.h = block.GetFloat("h", 12.0f);
        t.shootCooldown = block.GetFloat("shoot_cooldown", 1.5f);
        t.shootRange = block.GetFloat("shoot_range", 110.0f);
        t.preferredDistance = block.GetFloat("preferred_distance", 70.0f);
        t.shotSpeed = block.GetFloat("shot_speed", 80.0f);
    }
    return true;
}

bool Exists(const char* name) {
    return Find(name) != nullptr;
}

Enemy Spawn(const char* name, Vec2 pos) {
    Enemy e;
    e.pos = pos;

    const EnemyTemplate* t = Find(name);
    if (!t) return e; // unknown name -> default CHASER, hp 30

    e.aiType = t->aiType;
    e.hp = t->hp;
    e.maxHp = t->hp;
    e.speed = t->speed;
    e.w = t->w;
    e.h = t->h;
    e.shootCooldown = t->shootCooldown;
    e.shootRange = t->shootRange;
    e.preferredDistance = t->preferredDistance;
    e.shotSpeed = t->shotSpeed;
    return e;
}

} // namespace EnemyDatabase