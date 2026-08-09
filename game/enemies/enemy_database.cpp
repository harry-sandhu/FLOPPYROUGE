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

    AttackPattern ParseAttackPattern(const char* s) {
        if (std::strcmp(s, "TRIPLE") == 0) return AttackPattern::TRIPLE;
        if (std::strcmp(s, "RADIAL") == 0) return AttackPattern::RADIAL;
        if (std::strcmp(s, "SPIRAL") == 0) return AttackPattern::SPIRAL;
        return AttackPattern::SINGLE;
    }

    const EnemyTemplate* Find(const char* name) {
        for (int i = 0; i < g_templateCount; ++i) {
            if (std::strcmp(g_templates[i].name, name) == 0) return &g_templates[i];
        }
        return nullptr;
    }

    const EnemyTemplate* FindByIndex(int index) {
        if (index < 0 || index >= g_templateCount) return nullptr;
        return &g_templates[index];
    }

    void ApplyTemplate(Enemy& e, const EnemyTemplate& t) {
        e.aiType = t.aiType;
        e.attackPattern = t.attackPattern;
        e.shielded = t.shielded;
        e.hp = t.hp;
        e.maxHp = t.hp;
        e.speed = t.speed;
        e.w = t.w;
        e.h = t.h;
        e.shootCooldown = t.shootCooldown;
        e.shootRange = t.shootRange;
        e.preferredDistance = t.preferredDistance;
        e.shotSpeed = t.shotSpeed;
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
        t.attackPattern = ParseAttackPattern(block.GetString("attack_pattern", "SINGLE"));
        t.shielded = block.GetBool("shielded", false);
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

int Count() {
    return g_templateCount;
}

bool Exists(const char* name) {
    return Find(name) != nullptr;
}

Enemy Spawn(const char* name, Vec2 pos) {
    Enemy e;
    e.pos = pos;
    std::strncpy(e.templateName, name, sizeof(e.templateName) - 1);

    const EnemyTemplate* t = Find(name);
    if (!t) return e; // unknown name -> default CHASER, hp 30

    ApplyTemplate(e, *t);
    return e;
}

Enemy Spawn(int index, Vec2 pos) {
    Enemy e;
    e.pos = pos;

    const EnemyTemplate* t = FindByIndex(index);
    if (!t) return e;

    std::strncpy(e.templateName, t->name, sizeof(e.templateName) - 1);
    ApplyTemplate(e, *t);
    return e;
}

} // namespace EnemyDatabase