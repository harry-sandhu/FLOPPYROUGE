#include "../../engine/core/rng.h"
#include "item_system.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace {
    void AddOwnedItem(Player& player, int itemId) {
        if (itemId < 0) return;
        player.ownedItemIds.push_back(itemId);
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
            case ItemStat::CRIT_CHANCE: applyFloat(player.critChance); break;
            case ItemStat::LIFESTEAL_CHANCE: applyFloat(player.lifestealChance); break;
            case ItemStat::BURN_CHANCE: applyFloat(player.burnChance); break;
            case ItemStat::FREEZE_CHANCE: applyFloat(player.freezeChance); break;
            case ItemStat::MAGNET_CHANCE: applyFloat(player.magnetChance); break;
            case ItemStat::BOOMERANG_CHANCE: applyFloat(player.boomerangChance); break;
            case ItemStat::GROWING_CHANCE: applyFloat(player.growingChance); break;
            case ItemStat::SHRINKING_CHANCE: applyFloat(player.shrinkingChance); break;
            case ItemStat::CHAIN_CHANCE: applyFloat(player.chainChance); break;
            case ItemStat::GRAVITY_CHANCE: applyFloat(player.gravityChance); break;
            case ItemStat::VORTEX_CHANCE: applyFloat(player.vortexChance); break;
            case ItemStat::MARK_CHANCE: applyFloat(player.markChance); break;
            case ItemStat::WALL_BOUNCE_CHANCE: applyFloat(player.wallBounceChance); break;
            case ItemStat::ENEMY_BOUNCE_CHANCE: applyFloat(player.enemyBounceChance); break;
            case ItemStat::SPLIT_CHANCE: applyFloat(player.splitChance); break;
            case ItemStat::DODGE_CHANCE: applyFloat(player.dodgeChance); break;
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

    // Hollow Core: doesn't retroactively strip existing stat items (their
    // effects are baked additively/multiplicatively into player fields with
    // no safe way to invert that history), but grants an immediate burst of
    // 3 random proc-chance modifiers as a build-defining payoff instead.
    void ApplyHollowCoreBurst(Player& player) {
        float* procs[] = {
            &player.poisonChance, &player.stickyChance, &player.piercingChance,
            &player.explosiveChance, &player.burnChance, &player.freezeChance,
            &player.magnetChance, &player.boomerangChance, &player.chainChance,
            &player.gravityChance, &player.critChance, &player.lifestealChance,
            &player.markChance, &player.wallBounceChance, &player.enemyBounceChance,
            &player.splitChance
        };
        const int count = (int)(sizeof(procs) / sizeof(procs[0]));

        int chosen = 0;
        int attempts = 0;
        bool used[64] = {}; // count is well under 64
        while (chosen < 3 && attempts < 50) {
            attempts++;
            int idx = RNG::Range(0, count - 1);
            if (used[idx]) continue;
            used[idx] = true;
            *procs[idx] += 0.25f;
            chosen++;
        }
    }

    void ApplyUnlock(Player& player, const ItemTemplate& item) {
        switch (item.flag) {
            case ItemFlag::DIAGONAL_FIRE: player.hasDiagonalFire = true; break;
            case ItemFlag::DASH: player.hasDash = true; break;
            case ItemFlag::HOMING: player.hasHomingShots = true; break;
            case ItemFlag::VOID_HEART: player.hasVoidHeart = true; break;
            case ItemFlag::TWIN_SOUL: player.hasTwinSoul = true; break;
            case ItemFlag::PARASITE_CORE: player.hasParasiteCore = true; break;
            case ItemFlag::LAST_SHOT: player.hasLastShot = true; break;
            case ItemFlag::DEVASTATOR: player.hasDevastator = true; break;
            case ItemFlag::INFINITE_LOOP: player.hasInfiniteLoop = true; break;
            case ItemFlag::CHAOS_ENGINE: player.hasChaosEngine = true; break;
            case ItemFlag::SATELLITES: player.hasSatellites = true; break;
            case ItemFlag::CHARGED_SHOTS: player.hasChargedShots = true; break;
            case ItemFlag::SHIELD_CHARM:
                player.hasShieldCharm = true;
                player.shieldCharges = std::max(player.shieldCharges, 1);
                break;
            case ItemFlag::GUARDIAN_ANGEL: player.hasGuardianAngel = true; break;
            case ItemFlag::SPIKED_ARMOR: player.hasSpikedArmor = true; break;
            case ItemFlag::SECOND_WIND: player.hasSecondWind = true; break;
            case ItemFlag::IRON_WILL: player.hasIronWill = true; break;
            case ItemFlag::COMPASS: player.hasCompass = true; break;
            case ItemFlag::TREASURE_SENSE: player.hasTreasureSense = true; break;
            case ItemFlag::MARTYRDOM: player.hasMartyrdom = true; break;
            case ItemFlag::OVERCLOCK:
                player.hasOverclock = true;
                player.overclockTickTimer = 3.0f;
                player.fireRate *= 1.8f;
                player.moveSpeed *= 1.8f;
                break;
            case ItemFlag::HOLLOW_CORE:
                player.hasHollowCore = true;
                ApplyHollowCoreBurst(player);
                break;
            case ItemFlag::SECOND_SUN: player.hasSecondSun = true; break;
            case ItemFlag::UNKNOWN:
            default:
                break;
        }
        if (item.stat2 != ItemStat::UNKNOWN) {
            ApplyStatModValues(player, item.stat2, item.mode2, item.value2);
        }
    }
}

namespace ItemSystem {

int ComputeSynergyBonus(const Player& player) {
    int activeProcs = 0;
    if (player.hasHomingShots) activeProcs++;
    if (player.poisonChance > 0.0f) activeProcs++;
    if (player.stickyChance > 0.0f) activeProcs++;
    if (player.piercingChance > 0.0f) activeProcs++;
    if (player.explosiveChance > 0.0f) activeProcs++;
    if (player.burnChance > 0.0f) activeProcs++;
    if (player.freezeChance > 0.0f) activeProcs++;

    int bonus = 0;
    for (int itemId : player.ownedItemIds) {
        const ItemTemplate* item = ItemDatabase::Get(itemId);
        if (!item || item->type != ItemType::PROC_SYNERGY) continue;
        bonus += (int)std::lround(item->perProcValue * (float)activeProcs);
    }
    return bonus;
}

void ApplyItem(Player& player, const ItemTemplate& item) {
    if (item.type == ItemType::UNLOCK) {
        ApplyUnlock(player, item);
    } else if (item.type == ItemType::PROC_SYNERGY) {
        // No longer applied as a one-time flat bonus at pickup — SynergyCore
        // now recomputes dynamically from currently-owned procs at fire
        // time (see ItemSystem::ComputeSynergyBonus, used in
        // PlayerLogic::HandleShooting). Nothing to do here except be owned.
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
