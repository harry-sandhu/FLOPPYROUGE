#pragma once
#include <vector>
#include "../../engine/core/types.h"
#include "projectile.h"

struct Orbiter {
    float angle = 0.0f;
    float timeRemaining = 0.0f;
    int damage = 0;
    float hitCooldown = 0.0f;
};

struct Player {
    static constexpr int MAX_ORBITERS = 6;

    Vec2 pos;
    int size = 10;

    int damage = 10;
    float shotSpeed = 140.0f;
    float range = 150.0f;
    float fireRate = 3.0f;
    int projectileCount = 1;
    float moveSpeed = 60.0f;
    int luck = 0;
    int hp = 6;
    int maxHp = 6;

    float dashSpeed = 180.0f;
    float dashDuration = 0.12f;
    float dashCooldown = 0.8f;

    float homingChance = 0.0f;
    float poisonChance = 0.0f;
    float stickyChance = 0.0f;
    float piercingChance = 0.0f;
    float explosiveChance = 0.0f;
    float critChance = 0.0f;
    float lifestealChance = 0.0f;
    float burnChance = 0.0f;
    float freezeChance = 0.0f;
    float magnetChance = 0.0f;
    float boomerangChance = 0.0f;
    float growingChance = 0.0f;
    float shrinkingChance = 0.0f;
    float chainChance = 0.0f;
    float gravityChance = 0.0f;
    float vortexChance = 0.0f;
    float markChance = 0.0f;
    float wallBounceChance = 0.0f;
    float enemyBounceChance = 0.0f;
    float splitChance = 0.0f;

    static constexpr float CRIT_MULTIPLIER = 2.0f;

    float invincibleTimer = 0.0f;
    float fireCooldownRemaining = 0.0f;
    float dashCooldownRemaining = 0.0f;
    float dashTimeRemaining = 0.0f;
    float actionFlashTimer = 0.0f;
    float pickupMessageTimer = 0.0f;
    char pickupName[32] = {};

    bool hasDiagonalFire = false;
    bool hasDash = false;
    bool hasHomingShots = false;
    bool isDashing = false;

    bool hasVoidHeart = false;
    bool hasTwinSoul = false;
    bool hasParasiteCore = false;
    bool hasLastShot = false;
    bool hasDevastator = false;
    bool hasInfiniteLoop = false;
    bool hasChaosEngine = false;
    bool hasSatellites = false;
    bool hasChargedShots = false;

    bool hasShieldCharm = false;
    int shieldCharges = 0;
    bool hasGuardianAngel = false;
    bool guardianAngelUsed = false;
    bool hasSpikedArmor = false;
    float spikedArmorTickTimer = 0.0f;
    bool hasSecondWind = false;
    bool secondWindUsed = false;
    bool hasIronWill = false;
    bool hasCompass = false;
    bool hasTreasureSense = false;
    float dodgeChance = 0.0f;

    bool hasMartyrdom = false;
    bool hasOverclock = false;
    float overclockTickTimer = 0.0f;
    bool hasHollowCore = false;
    bool hollowCoreApplied = false;
    bool hasSecondSun = false;
    float secondSunAngle = 0.0f;

    int lastShotStreak = 0;
    float lastShotStreakTimer = 0.0f;
    int devastatorShotCounter = 0;
    int infiniteLoopCounter = 0;
    int satelliteShotCounter = 0;
    float chargeHeldTime = 0.0f;
    Vec2 lastShootDir = { 0.0f, 0.0f };

    Orbiter orbiters[MAX_ORBITERS];
    int orbiterCount = 0;

    std::vector<int> ownedItemIds;
    int bombCount = 0;

    Vec2 facingDir = { 0.0f, -1.0f };
    Vec2 dashDir = { 0.0f, 0.0f };

    Rect GetRect() const { return { pos.x, pos.y, (float)size, (float)size }; }
    bool IsInvincible() const { return invincibleTimer > 0.0f; }
};

namespace PlayerLogic {
    void HandleMovement(Player& player, float dt);
    void HandleShooting(Player& player, float dt, std::vector<Projectile>& playerProjectiles);
    bool TakeDamage(Player& player, int amount, float invincibleDuration);
    void UpdateTimers(Player& player, float dt);
    void RegisterHit(Player& player);
}