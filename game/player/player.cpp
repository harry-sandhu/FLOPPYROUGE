#include "player.h"
#include "projectile_system.h"
#include "../../engine/input.h"
#include "../../engine/core/rng.h"
#include <algorithm>
#include <cmath>
#include <windows.h>
#include "../items/item_system.h"

namespace {
    enum class ShotStyle {
        BULLET,
        ROCKET,
        BURST,
        LASER,
        CRIMSON_RAY,
        SLASH
    };

    Vec2 Normalize(Vec2 v) {
        float lenSq = v.x * v.x + v.y * v.y;
        if (lenSq > 0.0001f) {
            float len = std::sqrt(lenSq);
            v.x /= len;
            v.y /= len;
        }
        return v;
    }

    int CollectShotStyles(const Player& player, ShotStyle* outStyles, int maxStyles) {
        int count = 0;
        auto add = [&](ShotStyle style) {
            if (count < maxStyles) outStyles[count++] = style;
        };

        if (player.hasRocketShots) add(ShotStyle::ROCKET);
        if (player.hasBurstShots) add(ShotStyle::BURST);
        if (player.hasLaserShots) add(ShotStyle::LASER);
        if (player.hasCrimsonRay) add(ShotStyle::CRIMSON_RAY);
        if (player.hasBladeArc) add(ShotStyle::SLASH);

        if (count == 0) add(ShotStyle::BULLET);
        return count;
    }

    ProjectileKind ShotKind(ShotStyle style) {
        switch (style) {
            case ShotStyle::ROCKET: return ProjectileKind::ROCKET;
            case ShotStyle::BURST: return ProjectileKind::BULLET;
            case ShotStyle::LASER: return ProjectileKind::LASER;
            case ShotStyle::CRIMSON_RAY: return ProjectileKind::CRIMSON_RAY;
            case ShotStyle::SLASH: return ProjectileKind::SLASH;
            case ShotStyle::BULLET:
            default:
                return ProjectileKind::BULLET;
        }
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

    void ApplyPoisonTick(Player& player) {
        int damage = std::max(1, player.poisonDamage);
        if (player.hasMartyrdom) damage *= 2;
        player.hp -= damage;
        if (player.hp < 0) player.hp = 0;
        if (player.hp <= 0 && player.hasGuardianAngel && !player.guardianAngelUsed) {
            player.guardianAngelUsed = true;
            player.hp = 1;
        }
    }

    void BeginDash(Player& player, Vec2 dashDir) {
        player.isDashing = true;
        player.dashDir = dashDir;
        player.dashTimeRemaining = player.dashDuration;
        player.dashCooldownRemaining = player.dashCooldown;
        player.invincibleTimer = std::max(player.invincibleTimer, player.dashDuration);
        player.actionFlashTimer = std::max(player.actionFlashTimer, 0.10f);
    }

    void SpawnBeamLine(std::vector<Projectile>& playerProjectiles, const Player& player, Vec2 center, Vec2 dir,
                       int baseDamage, float range, ShotStyle style, const ProjectileMods& baseMods) {
        const int segmentCount = (style == ShotStyle::CRIMSON_RAY) ? 7 : 6;
        const float segmentSpacing = 11.0f;
        const float startOffset = 10.0f;
        const float segmentSize = (style == ShotStyle::CRIMSON_RAY) ? 10.0f : 8.0f;
        const float life = (style == ShotStyle::CRIMSON_RAY) ? 0.10f : 0.08f;
        const int beamDamage = (style == ShotStyle::CRIMSON_RAY)
            ? std::max(1, (int)std::lround((float)baseDamage * 0.35f))
            : std::max(1, (int)std::lround((float)baseDamage * 0.22f));

        ProjectileMods mods = baseMods;
        mods.kind = ShotKind(style);
        mods.piercing = true;
        mods.ignoreBounds = true;

        for (int i = 0; i < segmentCount; ++i) {
            float dist = startOffset + (float)i * segmentSpacing;
            Vec2 segCenter = { center.x + dir.x * dist, center.y + dir.y * dist };
            Vec2 segPos = { segCenter.x - segmentSize * 0.5f, segCenter.y - segmentSize * 0.5f };
            ProjectileSystem::Spawn(playerProjectiles, segPos, { 0.0f, 0.0f }, beamDamage, range, life, mods);
            if (!playerProjectiles.empty()) {
                playerProjectiles.back().sizeScale = (style == ShotStyle::CRIMSON_RAY) ? 1.35f : 1.15f;
            }
        }
    }

    void SpawnSlashWave(std::vector<Projectile>& playerProjectiles, const Player& player, Vec2 center, Vec2 dir,
                        int baseDamage, float range, const ProjectileMods& baseMods) {
        Vec2 perp = { -dir.y, dir.x };
        const float forward = 14.0f;
        const float side = 8.0f;
        const float slashLife = 0.10f;
        const int slashDamage = std::max(1, baseDamage + 3);

        ProjectileMods mods = baseMods;
        mods.kind = ProjectileKind::SLASH;
        mods.ignoreBounds = true;

        Vec2 offsets[] = {
            { dir.x * forward, dir.y * forward },
            { dir.x * forward + perp.x * side, dir.y * forward + perp.y * side },
            { dir.x * forward - perp.x * side, dir.y * forward - perp.y * side }
        };

        for (const Vec2& offset : offsets) {
            Vec2 pos = { center.x + offset.x - 5.0f, center.y + offset.y - 5.0f };
            ProjectileSystem::Spawn(playerProjectiles, pos, { 0.0f, 0.0f }, slashDamage, range, slashLife, mods);
            if (!playerProjectiles.empty()) {
                playerProjectiles.back().sizeScale = 1.4f;
            }
        }
    }

    void SpawnShotStyle(ShotStyle style, std::vector<Projectile>& playerProjectiles, const Player& player,
                        Vec2 playerCenter, Vec2 shootDir, int baseDamage, float range,
                        const ProjectileMods& baseMods, int projectileCount, float shotSizeScale) {
        if (style == ShotStyle::ROCKET) {
            for (int i = 0; i < projectileCount; ++i) {
                ProjectileMods rocketMods = baseMods;
                rocketMods.kind = ProjectileKind::ROCKET;
                rocketMods.explosive = true;
                Vec2 vel = { shootDir.x * (player.shotSpeed * 0.80f), shootDir.y * (player.shotSpeed * 0.80f) };
                ProjectileSystem::Spawn(
                    playerProjectiles,
                    playerCenter,
                    vel,
                    std::max(1, (int)std::lround(baseDamage * 1.15f)),
                    range * 1.15f,
                    0.0f,
                    rocketMods
                );
                if (!playerProjectiles.empty()) {
                    playerProjectiles.back().sizeScale = std::max(1.25f, shotSizeScale * 1.15f);
                }
            }
            return;
        }

        if (style == ShotStyle::BURST) {
            const int pelletCount = 5;
            const float spread = 0.56f;
            const float speed = 0.92f;
            ProjectileMods mods = baseMods;
            mods.kind = ProjectileKind::BULLET;
            float baseAngle = std::atan2(shootDir.y, shootDir.x);
            for (int i = 0; i < pelletCount; ++i) {
                float t = (pelletCount == 1) ? 0.0f : ((float)i / (pelletCount - 1)) - 0.5f;
                float angle = baseAngle + t * spread;
                Vec2 vel = { std::cos(angle) * speed * 110.0f, std::sin(angle) * speed * 110.0f };
                ProjectileSystem::Spawn(playerProjectiles, playerCenter, vel, baseDamage, range * 0.9f, 0.0f, mods);
                if (!playerProjectiles.empty()) {
                    playerProjectiles.back().sizeScale = 0.92f;
                }
            }
            return;
        }

        if (style == ShotStyle::LASER || style == ShotStyle::CRIMSON_RAY) {
            SpawnBeamLine(playerProjectiles, player, playerCenter, shootDir, baseDamage, range, style, baseMods);
            return;
        }

        if (style == ShotStyle::SLASH) {
            SpawnSlashWave(playerProjectiles, player, playerCenter, shootDir, baseDamage, range, baseMods);
            return;
        }

        ProjectileMods mods = baseMods;
        mods.kind = ProjectileKind::BULLET;
        for (int i = 0; i < projectileCount; ++i) {
            Vec2 vel = { shootDir.x * player.shotSpeed, shootDir.y * player.shotSpeed };
            ProjectileSystem::Spawn(playerProjectiles, playerCenter, vel, baseDamage, range, 0.0f, mods);
            if (shotSizeScale != 1.0f && !playerProjectiles.empty()) {
                playerProjectiles.back().sizeScale = shotSizeScale;
            }
        }
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
    Vec2 playerCenter = {
        player.pos.x + player.size / 2.0f,
        player.pos.y + player.size / 2.0f
    };
    ShotStyle shotStyles[5];
    int shotStyleCount = CollectShotStyles(player, shotStyles, 5);

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
    float effectiveDamage = ((float)player.damage + (float)ItemSystem::ComputeSynergyBonus(player)) * player.damageMultiplier;
    if (effectiveDamage < 1.0f) effectiveDamage = 1.0f;
    int baseDamage = (int)std::lround(effectiveDamage);
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

    if (shotStyleCount > 0) {
        for (int i = 0; i < shotStyleCount; ++i) {
            ShotStyle style = shotStyles[i];
            ProjectileMods styleMods = mods;
            styleMods.kind = ShotKind(style);
            if (style == ShotStyle::LASER || style == ShotStyle::CRIMSON_RAY || style == ShotStyle::SLASH) {
                styleMods.ignoreBounds = true;
                styleMods.piercing = true;
            }
            SpawnShotStyle(style, playerProjectiles, player, playerCenter, shootDir, baseDamage, player.range,
                           styleMods, projectileCount, shotSizeScale);
        }
    }

    // Twin Soul: mirrored shot from the opposite side of the player.
    if (player.hasTwinSoul) {
        Vec2 mirroredPos = {
            playerCenter.x - shootDir.x * 8.0f - 4.0f,
            playerCenter.y - shootDir.y * 8.0f - 4.0f
        };
        for (int i = 0; i < shotStyleCount; ++i) {
            ShotStyle style = shotStyles[i];
            ProjectileMods twinMods = mods;
            twinMods.kind = ShotKind(style);
            if (style == ShotStyle::LASER || style == ShotStyle::CRIMSON_RAY || style == ShotStyle::SLASH) {
                twinMods.ignoreBounds = true;
                twinMods.piercing = true;
            }
            SpawnShotStyle(style, playerProjectiles, player, mirroredPos, shootDir, baseDamage, player.range,
                           twinMods, projectileCount, shotSizeScale);
        }
    }

    // Satellites: every 5th shot spawns an orbiter (handled/updated by
    // ProjectileSystem::UpdateOrbiters, called from main.cpp).
    if (player.hasSatellites) {
        player.satelliteShotCounter++;
        if (player.satelliteShotCounter >= 5 && player.orbiterCount < Player::MAX_ORBITERS) {
            player.satelliteShotCounter = 0;
            Orbiter& o = player.orbiters[player.orbiterCount++];
            o.angle = 6.2831853f * ((float)(player.orbiterCount - 1) / (float)Player::MAX_ORBITERS);
            o.timeRemaining = 15.0f;
            o.damage = std::max(1, player.damage / 2);
        }
    }

    float effectiveFireRate = player.fireRate * player.fireRateMultiplier;
    if (effectiveFireRate < 0.1f) effectiveFireRate = 0.1f;
    float fireInterval = 1.0f / effectiveFireRate;
    bool hasLaserStyle = false;
    bool hasBurstStyle = false;
    bool hasRocketStyle = false;
    bool hasSlashStyle = false;
    bool hasRayStyle = false;
    for (int i = 0; i < shotStyleCount; ++i) {
        switch (shotStyles[i]) {
            case ShotStyle::ROCKET: hasRocketStyle = true; break;
            case ShotStyle::BURST: hasBurstStyle = true; break;
            case ShotStyle::LASER: hasLaserStyle = true; break;
            case ShotStyle::CRIMSON_RAY: hasRayStyle = true; break;
            case ShotStyle::SLASH: hasSlashStyle = true; break;
            case ShotStyle::BULLET:
            default:
                break;
        }
    }
    if (hasBurstStyle) fireInterval = std::min(fireInterval, 0.22f);
    if (hasLaserStyle) fireInterval = std::min(fireInterval, 0.05f);
    if (hasRayStyle) fireInterval = std::min(fireInterval, 0.08f);
    if (hasSlashStyle) fireInterval = std::min(fireInterval, 0.12f);
    if (hasRocketStyle) fireInterval = std::min(fireInterval, 0.24f);
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

    if (player.damageReduction > 0.0f) {
        float reduction = std::clamp(player.damageReduction, 0.0f, 0.75f);
        finalAmount = std::max(1, (int)std::lround((float)finalAmount * (1.0f - reduction)));
    }

    if (player.hasPhoenixFeather && !player.phoenixFeatherUsed && finalAmount >= player.hp) {
        player.phoenixFeatherUsed = true;
        player.hp = player.maxHp;
        float duration = invincibleDuration;
        if (duration < 1.0f) duration = 1.0f;
        player.invincibleTimer = duration;
        return true;
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

    if (player.poisonTimer > 0.0f) {
        player.poisonTimer -= dt;
        player.poisonTickTimer -= dt;
        if (player.poisonTickTimer <= 0.0f) {
            player.poisonTickTimer = 0.5f;
            ApplyPoisonTick(player);
        }
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
