#include "boss_database.h"
#include "../../engine/data_parser.h"
#include <algorithm>
#include <cstring>
#include <vector>

namespace {
    BossTemplate g_templates[MAX_BOSS_TEMPLATES];
    int g_templateCount = 0;

    BossAttackType ParseAttackType(const char* name) {
        if (std::strcmp(name, "SPREAD_SHOT") == 0) return BossAttackType::SPREAD_SHOT;
        if (std::strcmp(name, "RADIAL_BURST") == 0) return BossAttackType::RADIAL_BURST;
        if (std::strcmp(name, "CHARGE") == 0) return BossAttackType::CHARGE;
        if (std::strcmp(name, "LASER_SWEEP") == 0) return BossAttackType::LASER_SWEEP;
        if (std::strcmp(name, "SUMMON_WAVE") == 0) return BossAttackType::SUMMON_WAVE;
        if (std::strcmp(name, "FLOOR_HAZARD") == 0) return BossAttackType::FLOOR_HAZARD;
        if (std::strcmp(name, "MIRROR_SHOT") == 0) return BossAttackType::MIRROR_SHOT;
        if (std::strcmp(name, "CARDINAL_BURST") == 0) return BossAttackType::CARDINAL_BURST;
        if (std::strcmp(name, "SPIRAL_BURST") == 0) return BossAttackType::SPIRAL_BURST;
        if (std::strcmp(name, "TRIPLE_SPREAD") == 0) return BossAttackType::TRIPLE_SPREAD;
        if (std::strcmp(name, "DENSE_RING") == 0) return BossAttackType::DENSE_RING;
        if (std::strcmp(name, "GAPPED_RING") == 0) return BossAttackType::GAPPED_RING;
        if (std::strcmp(name, "TELEPORT_BURST") == 0) return BossAttackType::TELEPORT_BURST;
        if (std::strcmp(name, "VORTEX_PULL") == 0) return BossAttackType::VORTEX_PULL;
        return BossAttackType::SPREAD_SHOT; // default
    }

    void ParseAttackCycle(const char* cycleStr, BossAttackType* outCycle, int& outLength) {
        outLength = 0;
        if (!cycleStr || cycleStr[0] == '\0') return;

        char buffer[256];
        std::strncpy(buffer, cycleStr, sizeof(buffer) - 1);
        
        char* token = strtok(buffer, " ,");
        while (token && outLength < MAX_ATTACK_CYCLE_LENGTH) {
            outCycle[outLength++] = ParseAttackType(token);
            token = strtok(nullptr, " ,");
        }
    }
}

namespace BossDatabase {

bool Load(const char* path) {
    std::vector<DataBlock> blocks = DataParser::ParseFile(path);
    if (blocks.empty()) return false;

    g_templateCount = 0;
    for (const DataBlock& block : blocks) {
        if (g_templateCount >= MAX_BOSS_TEMPLATES) break;

        BossTemplate& boss = g_templates[g_templateCount++];
        std::strncpy(boss.name, block.name, sizeof(boss.name) - 1);
        boss.hp = std::max(1, block.GetInt("hp", boss.hp));
        boss.driftSpeed = block.GetFloat("drift_speed", boss.driftSpeed);
        boss.attackCooldownPhase1 = std::max(0.1f, block.GetFloat("attack_cd_phase1", boss.attackCooldownPhase1));
        boss.attackCooldownPhase2 = std::max(0.1f, block.GetFloat("attack_cd_phase2", boss.attackCooldownPhase2));
        boss.chargeSpeed = block.GetFloat("charge_speed", boss.chargeSpeed);
        boss.phase2HpRatio = std::clamp(block.GetFloat("phase2_hp_ratio", boss.phase2HpRatio), 0.0f, 0.90f);
        boss.contactDamage = std::max(1, block.GetInt("contact_damage", boss.contactDamage));
        boss.chargeContactDamage = std::max(1, block.GetInt("charge_contact_damage", boss.chargeContactDamage));
        boss.maxAdds = std::max(0, block.GetInt("max_adds", boss.maxAdds));
        
        // Parse attack cycles
        const char* attackCyclePhase1 = block.GetString("attack_cycle_phase1");
        if (attackCyclePhase1) {
            ParseAttackCycle(attackCyclePhase1, boss.attackCyclePhase1, boss.attackCyclePhase1Length);
        }
        
        const char* attackCyclePhase2 = block.GetString("attack_cycle_phase2");
        if (attackCyclePhase2) {
            ParseAttackCycle(attackCyclePhase2, boss.attackCyclePhase2, boss.attackCyclePhase2Length);
        }
        
        // If phase 2 not specified, use phase 1
        if (boss.attackCyclePhase2Length == 0) {
            std::memcpy(boss.attackCyclePhase2, boss.attackCyclePhase1, 
                       sizeof(BossAttackType) * boss.attackCyclePhase1Length);
            boss.attackCyclePhase2Length = boss.attackCyclePhase1Length;
        }
    }

    return g_templateCount > 0;
}

const BossTemplate* Get(int index) {
    if (index < 0 || index >= g_templateCount) return nullptr;
    return &g_templates[index];
}

int IndexOf(const char* name) {
    for (int i = 0; i < g_templateCount; ++i) {
        if (std::strcmp(g_templates[i].name, name) == 0) return i;
    }
    return -1;
}

int Count() {
    return g_templateCount;
}

} // namespace BossDatabase
