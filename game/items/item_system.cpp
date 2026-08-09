#include "item_system.h"
#include <algorithm>
#include <cmath>

namespace {
    void AddOwnedItem(Player& player, int itemId) {
        if (itemId < 0 || player.ownedItemCount >= Player::MAX_OWNED_ITEMS) return;
        player.ownedItemIds[player.ownedItemCount++] = itemId;
    }

    void ApplyStatMod(Player& player, const ItemTemplate& item) {
        auto applyFloat = [&](float& stat) {
            if (item.mode == ItemMode::ADD) stat += item.value;
            else stat *= item.value;
        };

        auto applyInt = [&](int& stat) {
            float value = (item.mode == ItemMode::ADD) ? (stat + item.value) : (stat * item.value);
            stat = std::max(1, (int)std::lround(value));
        };

        switch (item.stat) {
            case ItemStat::DAMAGE: applyInt(player.damage); break;
            case ItemStat::SHOT_SPEED: applyFloat(player.shotSpeed); break;
            case ItemStat::RANGE: applyFloat(player.range); break;
            case ItemStat::FIRE_RATE: applyFloat(player.fireRate); break;
            case ItemStat::PROJECTILE_COUNT: applyInt(player.projectileCount); break;
            case ItemStat::MOVE_SPEED: applyFloat(player.moveSpeed); break;
            case ItemStat::MAX_HP: {
                applyInt(player.maxHp);
                if (player.maxHp < 1) player.maxHp = 1;
                if (player.hp > player.maxHp) player.hp = player.maxHp;
                break;
            }
            case ItemStat::DASH_SPEED: applyFloat(player.dashSpeed); break;
            case ItemStat::DASH_DURATION: applyFloat(player.dashDuration); break;
            case ItemStat::DASH_COOLDOWN: applyFloat(player.dashCooldown); break;
            case ItemStat::UNKNOWN:
            default:
                break;
        }
    }

    void ApplyUnlock(Player& player, const ItemTemplate& item) {
        switch (item.flag) {
            case ItemFlag::DIAGONAL_FIRE: player.hasDiagonalFire = true; break;
            case ItemFlag::DASH: player.hasDash = true; break;
            case ItemFlag::UNKNOWN:
            default:
                break;
        }
    }
}

namespace ItemSystem {

void ApplyItem(Player& player, const ItemTemplate& item) {
    if (item.type == ItemType::UNLOCK) {
        ApplyUnlock(player, item);
    } else {
        ApplyStatMod(player, item);
    }
}

void GrantItem(Player& player, int itemId) {
    const ItemTemplate* item = ItemDatabase::Get(itemId);
    if (!item) return;

    ApplyItem(player, *item);
    AddOwnedItem(player, itemId);
}

} // namespace ItemSystem
