#include "item_system.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace {
    void AddOwnedItem(Player& player, int itemId) {
        if (itemId < 0 || player.ownedItemCount >= Player::MAX_OWNED_ITEMS) return;
        player.ownedItemIds[player.ownedItemCount++] = itemId;
    }

    void ApplyStatModValues(Player& player, ItemStat stat, ItemMode mode, float value) {
        auto applyFloat = [&](float& s) {
            if (mode == ItemMode::ADD) s += value;
            else s *= value;
        };

        auto applyInt = [&](int& s) {
            float v = (mode == ItemMode::ADD) ? (s + value) : (s * value);
            s = std::max(1, (int)std::lround(v));
        };

        auto applyHeal = [&](int amount) {
            player.hp = std::min(player.maxHp, player.hp + std::max(1, amount));
        };

        switch (stat) {
            case ItemStat::DAMAGE: applyInt(player.damage); break;
            case ItemStat::SHOT_SPEED: applyFloat(player.shotSpeed); break;
            case ItemStat::RANGE: applyFloat(player.range); break;
            case ItemStat::FIRE_RATE: applyFloat(player.fireRate); break;
            case ItemStat::PROJECTILE_COUNT: applyInt(player.projectileCount); break;
            case ItemStat::MOVE_SPEED: applyFloat(player.moveSpeed); break;
            case ItemStat::LUCK: {
                float v = (mode == ItemMode::ADD) ? (player.luck + value) : (player.luck * value);
                player.luck = (int)std::lround(v);
                break;
            }
            case ItemStat::MAX_HP: {
                int before = player.maxHp;
                applyInt(player.maxHp);
                if (player.maxHp < 2) player.maxHp = 2;
                player.hp += (player.maxHp - before);
                if (player.hp > player.maxHp) player.hp = player.maxHp;
                break;
            }
            case ItemStat::HEAL: applyHeal((int)std::lround(value)); break;
            case ItemStat::HOMING_CHANCE: applyFloat(player.homingChance); break;
            case ItemStat::POISON_CHANCE: applyFloat(player.poisonChance); break;
            case ItemStat::STICKY_CHANCE: applyFloat(player.stickyChance); break;
            case ItemStat::PIERCING_CHANCE: applyFloat(player.piercingChance); break;
            case ItemStat::EXPLOSIVE_CHANCE: applyFloat(player.explosiveChance); break;
            case ItemStat::DASH_SPEED: applyFloat(player.dashSpeed); break;
            case ItemStat::DASH_DURATION: applyFloat(player.dashDuration); break;
            case ItemStat::DASH_COOLDOWN: applyFloat(player.dashCooldown); break;
            case ItemStat::UNKNOWN:
            default:
                break;
        }
    }

    void ApplyStatMod(Player& player, const ItemTemplate& item) {
        ApplyStatModValues(player, item.stat, item.mode, item.value);
        if (item.stat2 != ItemStat::UNKNOWN) {
            ApplyStatModValues(player, item.stat2, item.mode2, item.value2);
        }
    }

    void ApplyUnlock(Player& player, const ItemTemplate& item) {
        switch (item.flag) {
            case ItemFlag::DIAGONAL_FIRE: player.hasDiagonalFire = true; break;
            case ItemFlag::DASH: player.hasDash = true; break;
            case ItemFlag::HOMING: player.hasHomingShots = true; break;
            case ItemFlag::UNKNOWN:
            default:
                break;
        }
        if (item.stat2 != ItemStat::UNKNOWN) {
            ApplyStatModValues(player, item.stat2, item.mode2, item.value2);
        }
    }

    void ApplyProcSynergy(Player& player, const ItemTemplate& item) {
        int procCount = 0;
        if (player.homingChance > 0.0f) procCount++;
        if (player.poisonChance > 0.0f) procCount++;
        if (player.stickyChance > 0.0f) procCount++;
        if (player.piercingChance > 0.0f) procCount++;
        if (player.explosiveChance > 0.0f) procCount++;

        int bonus = (int)std::lround(item.perProcValue * (float)procCount);
        player.damage = std::max(1, player.damage + bonus);
    }
}

namespace ItemSystem {

void ApplyItem(Player& player, const ItemTemplate& item) {
    if (item.type == ItemType::UNLOCK) {
        ApplyUnlock(player, item);
    } else if (item.type == ItemType::PROC_SYNERGY) {
        ApplyProcSynergy(player, item);
    } else {
        ApplyStatMod(player, item);
    }
}

void GrantItem(Player& player, int itemId) {
    const ItemTemplate* item = ItemDatabase::Get(itemId);
    if (!item) return;

    ApplyItem(player, *item);
    AddOwnedItem(player, itemId);
    std::strncpy(player.pickupName, item->name, sizeof(player.pickupName) - 1);
    player.pickupName[sizeof(player.pickupName) - 1] = '\0';
    player.pickupMessageTimer = 1.5f;
}

} // namespace ItemSystem