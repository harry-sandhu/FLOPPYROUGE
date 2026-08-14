#include "player.h"
#include "projectile_system.h"
#include "../../engine/input.h"
#include "../../engine/core/rng.h"
#include <algorithm>
#include <cmath>
#include <windows.h>
#include "../items/item_system.h"

namespace {
    Vec2 Normalize(Vec2 v) {
        float lenSq = v.x * v.x + v.y * v.y;
        if (lenSq > 0.0001f) {
            float len = std::sqrt(lenSq);
            v.x /= len;
            v.y /= len;
        }
        return v;
    }

    bool BuildShootDirection(const Player& player, Vec2& outDir) {
        int dx = 0;
        int dy = 0;
        int pressedCount = 0;

        if (Input::IsDown(VK_UP))    { dy -= 1; pressedCount++; }
        if (Input::IsDown(VK_DOWN))  { dy += 1; pressedCount++; }
        if (Input::IsDown(VK_LEFT))  { dx -= 1; pressedCount++; }
        if (Input::IsDown(VK_RIGHT)) { dx += 1; pressedCount++; }

        if (pressedCount == 1) {
            outDir = { (float)dx, (float)dy };
            return true;
        }

        if (pressedCount == 2 && dx != 0 && dy != 0 && player.hasDiagonalFire) {
            outDir = Normalize({ (float)dx, (float)dy });
            return true;
        }

        return false;
    }

    float LuckAdjustedChance(float baseChance, int luck) {
        float chance = baseChance + (float)luck * 0.05f;
        if (chance < 0.0f) chance = 0.0f;
        if (chance > 1.0f) chance = 1.0f;
        return chance;
    }

    void BeginDash(Player& player, Vec2 dashDir) {
        player.isDashing = true;
        player.dashDir = dashDir;
        player.dashTimeRemaining = player.dashDuration;
        player.dashCooldownRemaining = player.dashCooldown;
        player.invincibleTimer = std::max(player.invincibleTimer, player.dashDuration);
        player.actionFlashTimer = std::max(player.actionFlashTimer, 0.10f);
    }
}

namespace PlayerLogic {

void HandleMovement(Player& player, float dt) {
    if (player.hasDash && Input::IsPressed(VK_SPACE) && !player.isDashing && player.dashCooldownRemaining <= 0.0f) {
        Vec2 dashDir = player.facingDir;
        if (dashDir.x == 0.0f && dashDir.y == 0.0f) {
            dashDir = { 0.0f, -1.0f };
        }
        BeginDash(player, Normalize(dashDir));
    }

    if (player.isDashing) {
        player.pos.x += player.dashDir.x * player.dashSpeed * dt;
        player.pos.y += player.dashDir.y * player.dashSpeed * dt;
        return;
    }

    float moveX = 0.0f;
    float moveY = 0.0f;

    if (Input::IsDown('W')) moveY -= 1.0f;
    if (Input::IsDown('S')) moveY += 1.0f;
    if (Input::IsDown('A')) moveX -= 1.0f;
    if (Input::IsDown('D')) moveX += 1.0f;

    if (moveX != 0.0f || moveY != 0.0f) {
        player.facingDir = Normalize({ moveX, moveY });
    }

    player.pos.x += moveX * player.moveSpeed * dt;
    player.pos.y += moveY * player.moveSpeed * dt;
}

void HandleShooting(Player& player, float dt, std::vector<Projectile>& playerProjectiles) {
    Vec2 shootDir;
    if (!BuildShootDirection(player, shootDir)) {
        player.chargeHeldTime = 0.0f;
        player.lastShootDir = { 0.0f, 0.0f };
        return;
    }

    // Charge tracking: time spent holding the *same* direction uninterrupted.
    // (Capacitor) - approximated on top of the existing auto-fire-while-held
    // model rather than a separate hold-to-charge input mode.
    bool sameDir = (player.lastShootDir.x == shootDir.x && player.lastShootDir.y == shootDir.y);
    if (player.hasChargedShots) {
        if (sameDir) player.chargeHeldTime += dt;
        else player.chargeHeldTime = 0.0f;
    }
    player.lastShootDir = shootDir;

    if (player.fireCooldownRemaining > 0.0f) return;

    int projectileCount = std::max(1, player.projectileCount);
    Vec2 spawnPos = {
        player.pos.x + player.size / 2.0f,
        player.pos.y + player.size / 2.0f
    };

    ProjectileMods mods;
    mods.homing = player.hasHomingShots;
    mods.poison = RNG::Chance(LuckAdjustedChance(player.poisonChance, player.luck));
    mods.sticky = RNG::Chance(LuckAdjustedChance(player.stickyChance, player.luck));
    mods.piercing = RNG::Chance(LuckAdjustedChance(player.piercingChance, player.luck));
    mods.explosive = RNG::Chance(LuckAdjustedChance(player.explosiveChance, player.luck));
    mods.burn = RNG::Chance(LuckAdjustedChance(player.burnChance, player.luck));
    mods.freeze = RNG::Chance(LuckAdjustedChance(player.freezeChance, player.luck));
    mods.magnet = RNG::Chance(LuckAdjustedChance(player.magnetChance, player.luck));
    mods.boomerang = RNG::Chance(LuckAdjustedChance(player.boomerangChance, player.luck));
    mods.growing = RNG::Chance(LuckAdjustedChance(player.growingChance, player.luck));
    mods.shrinking = RNG::Chance(LuckAdjustedChance(player.shrinkingChance, player.luck));
    mods.chain = RNG::Chance(LuckAdjustedChance(player.chainChance, player.luck));
    mods.gravity = RNG::Chance(LuckAdjustedChance(player.gravityChance, player.luck));
    mods.vortex = RNG::Chance(LuckAdjustedChance(player.vortexChance, player.luck));
    mods.crit = RNG::Chance(LuckAdjustedChance(player.critChance, player.luck));
    mods.lifesteal = RNG::Chance(LuckAdjustedChance(player.lifestealChance, player.luck));
    mods.marking = RNG::Chance(LuckAdjustedChance(player.markChance, player.luck));
    mods.wallBounce = RNG::Chance(LuckAdjustedChance(player.wallBounceChance, player.luck));
    mods.enemyBounce = RNG::Chance(LuckAdjustedChance(player.enemyBounceChance, player.luck));
    mods.splitOnImpact = RNG::Chance(LuckAdjustedChance(player.splitChance, player.luck));
    mods.ignoreBounds = player.hasVoidHeart;

    if (player.hasChaosEngine) {
        int pick = RNG::Range(0, 6);
        switch (pick) {
            case 0: mods.piercing = true; break;
            case 1: mods.explosive = true; break;
            case 2: mods.poison = true; break;
            case 3: mods.sticky = true; break;
            case 4: mods.homing = true; break;
            case 5: mods.splitOnImpact = true; break;
            case 6: mods.wallBounce = true; break;
        }
    }

    // Base damage: flat streak bonus from Last Shot, then a multiplier from
    // charge/devastator/infinite-loop stacked on via sizeScale at spawn time.
    int baseDamage = player.damage + ItemSystem::ComputeSynergyBonus(player);
    if (player.hasLastShot) {
        baseDamage += std::min(player.lastShotStreak, 10) * 2;
    }
    if (mods.crit) {
        baseDamage = (int)std::lround(baseDamage * Player::CRIT_MULTIPLIER);
    }

    float shotSizeScale = 1.0f;

    if (player.hasChargedShots && player.chargeHeldTime > 0.0f) {
        float t = std::min(player.chargeHeldTime / 1.5f, 1.0f);
        shotSizeScale *= (1.0f + t); // up to +100% at full charge
    }

    if (player.hasDevastator) {
        player.devastatorShotCounter++;
        if (player.devastatorShotCounter >= 20) {
            player.devastatorShotCounter = 0;
            shotSizeScale *= 2.5f;
        }
    }

    if (player.hasInfiniteLoop) {
        player.infiniteLoopCounter++;
        if (player.infiniteLoopCounter >= 10) {
            player.infiniteLoopCounter = 0;
            shotSizeScale *= 1.5f;
        }
    }

    for (int i = 0; i < projectileCount; ++i) {
        Vec2 vel = { shootDir.x * player.shotSpeed, shootDir.y * player.shotSpeed };
        
        

        ProjectileSystem::Spawn(
            playerProjectiles,
            spawnPos,
            vel,
            baseDamage,
            player.range,
            0.0f,
            mods
        );
        // sizeScale is applied post-spawn since Spawn() doesn't take it directly.
        if (shotSizeScale != 1.0f && !playerProjectiles.empty()) {
            playerProjectiles.back().sizeScale = shotSizeScale;
        }
    }

    // Twin Soul: mirrored shot from the opposite side of the player.
    if (player.hasTwinSoul) {
        Vec2 playerCenter = { player.pos.x + player.size / 2.0f, player.pos.y + player.size / 2.0f };
        Vec2 mirroredPos = { playerCenter.x - (spawnPos.x - playerCenter.x), playerCenter.y - (spawnPos.y - playerCenter.y) };
        Vec2 vel = { shootDir.x * player.shotSpeed, shootDir.y * player.shotSpeed };
        ProjectileSystem::Spawn(playerProjectiles, mirroredPos, vel, baseDamage, player.range, 0.0f, mods);
        if (shotSizeScale != 1.0f && !playerProjectiles.empty()) {
            playerProjectiles.back().sizeScale = shotSizeScale;
        }
    }

    // Satellites: every 5th shot spawns an orbiter (handled/updated by
    // ProjectileSystem::UpdateOrbiters, called from main.cpp).
    if (player.hasSatellites) {
        player.satelliteShotCounter++;
        if (player.satelliteShotCounter >= 5 && player.orbiterCount < Player::MAX_ORBITERS) {
            player.satelliteShotCounter = 0;
            Orbiter& o = player.orbiters[player.orbiterCount++];
            o.angle = 0.0f;
            o.timeRemaining = 3.0f;
            o.damage = std::max(1, player.damage / 2);
        }
    }

    float fireInterval = (player.fireRate > 0.0f) ? (1.0f / player.fireRate) : 0.0f;
    if (player.hasChargedShots) {
        player.chargeHeldTime = 0.0f; // consumed on fire
    }
    player.fireCooldownRemaining = fireInterval;
    player.actionFlashTimer = std::max(player.actionFlashTimer, 0.06f);
}

bool TakeDamage(Player& player, int amount, float invincibleDuration) {
    if (player.IsInvincible()) return false;

    if (player.dodgeChance > 0.0f && RNG::Chance(player.dodgeChance)) {
        return false; // Lucky Foot: dodged entirely, no shield spent, no i-frames used
    }

    int finalAmount = amount;
    if (player.hasMartyrdom) {
        finalAmount *= 2; // Martyrdom: all incoming damage doubled
    }

    if (player.hasShieldCharm && player.shieldCharges > 0) {
        player.shieldCharges--;
        player.invincibleTimer = invincibleDuration;
        return false; // absorbed, no HP lost
    }

    player.hp -= finalAmount;

    if (player.hp <= 0 && player.hasGuardianAngel && !player.guardianAngelUsed) {
        player.guardianAngelUsed = true;
        player.hp = 1;
    }

    if (player.hp < 0) player.hp = 0;

    float duration = invincibleDuration;
    if (player.hasIronWill) duration *= 1.5f;
    player.invincibleTimer = duration;

    if (player.hasSecondWind && !player.secondWindUsed && player.hp == 1) {
        player.secondWindUsed = true;
        player.shieldCharges = std::max(player.shieldCharges, 1);
        player.hasShieldCharm = true; // grant the mechanic even if the item itself isn't owned
    }

    return true;
}

void RegisterHit(Player& player) {
    if (player.hasLastShot) {
        player.lastShotStreak = std::min(player.lastShotStreak + 1, 10);
        player.lastShotStreakTimer = 1.5f;
    }
}

void UpdateTimers(Player& player, float dt) {
    if (player.invincibleTimer > 0.0f) {
        player.invincibleTimer -= dt;
        if (player.invincibleTimer < 0.0f) player.invincibleTimer = 0.0f;
    }

    if (player.fireCooldownRemaining > 0.0f) {
        player.fireCooldownRemaining -= dt;
        if (player.fireCooldownRemaining < 0.0f) player.fireCooldownRemaining = 0.0f;
    }

    if (player.dashCooldownRemaining > 0.0f) {
        player.dashCooldownRemaining -= dt;
        if (player.dashCooldownRemaining < 0.0f) player.dashCooldownRemaining = 0.0f;
    }

    if (player.actionFlashTimer > 0.0f) {
        player.actionFlashTimer -= dt;
        if (player.actionFlashTimer < 0.0f) player.actionFlashTimer = 0.0f;
    }

    if (player.pickupMessageTimer > 0.0f) {
        player.pickupMessageTimer -= dt;
        if (player.pickupMessageTimer < 0.0f) player.pickupMessageTimer = 0.0f;
    }

    if (player.lastShotStreakTimer > 0.0f) {
        player.lastShotStreakTimer -= dt;
        if (player.lastShotStreakTimer <= 0.0f) {
            player.lastShotStreakTimer = 0.0f;
            player.lastShotStreak = 0;
        }
    }

    if (player.isDashing && player.dashTimeRemaining > 0.0f) {
        player.dashTimeRemaining -= dt;
        if (player.dashTimeRemaining <= 0.0f) {
            player.dashTimeRemaining = 0.0f;
            player.isDashing = false;
            player.dashDir = { 0.0f, 0.0f };
        }
    }

    if (player.hasOverclock) {
        player.overclockTickTimer -= dt;
        if (player.overclockTickTimer <= 0.0f) {
            player.overclockTickTimer = 3.0f;
            TakeDamage(player, 1, 0.0f); // passive tick, no i-frames granted
        }
    }

    if (player.hasSecondSun) {
        player.secondSunAngle += 2.0f * dt;
    }

    if (player.spikedArmorTickTimer > 0.0f) {
        player.spikedArmorTickTimer -= dt;
        if (player.spikedArmorTickTimer < 0.0f) player.spikedArmorTickTimer = 0.0f;
    }
}

} // namespace PlayerLogic