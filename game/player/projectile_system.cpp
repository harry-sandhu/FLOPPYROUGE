#include "projectile_system.h"
#include <algorithm>
#include <cmath>
#include "../../engine/collision.h"
#include "../../engine/renderer.h"
#include "../enemies/enemy_database.h"
#include "../rooms/room.h"

namespace {
    // Generous off-room bounds check — internal resolution is 320x180.
    bool OutOfBounds(Vec2 pos) {
        return pos.x < -16.0f || pos.x > 336.0f || pos.y < -16.0f || pos.y > 196.0f;
    }

    // Tighter play-area bounds used only for wall-bounce (Ricochet Core).
    // There's no tile-collision system for projectiles in this engine, so
    // this approximates "walls" as the rectangular room boundary.
    constexpr float WALL_MIN_X = 0.0f;
    constexpr float WALL_MAX_X = 320.0f;
    constexpr float WALL_MIN_Y = 0.0f;
    constexpr float WALL_MAX_Y = 180.0f;

    Vec2 Normalize(Vec2 v) {
        float lenSq = v.x * v.x + v.y * v.y;
        if (lenSq > 0.0001f) {
            float len = std::sqrt(lenSq);
            v.x /= len;
            v.y /= len;
        }
        return v;
    }

    uint32_t ProjectileColor(const Projectile& p, uint32_t baseColor) {
        switch (p.kind) {
            case ProjectileKind::ROCKET: return 0xFFFFA14A;
            case ProjectileKind::LASER: return 0xFF7DEBFF;
            case ProjectileKind::CRIMSON_RAY: return 0xFFFF5F78;
            case ProjectileKind::SLASH: return 0xFFFFF2B0;
            case ProjectileKind::BULLET:
            default:
                return baseColor;
        }
    }

    Vec2 FindHomingTarget(const std::vector<Enemy>* roomEnemies, const Boss* boss, const Vec2* playerPos,
                          Vec2 from, bool& found) {
        found = false;
        Vec2 target = from;
        float bestDistSq = 0.0f;

        if (roomEnemies) {
            for (const Enemy& enemy : *roomEnemies) {
                if (!enemy.alive) continue;
                Vec2 center = { enemy.pos.x + enemy.w * 0.5f, enemy.pos.y + enemy.h * 0.5f };
                float dx = center.x - from.x;
                float dy = center.y - from.y;
                float distSq = dx * dx + dy * dy;
                if (!found || distSq < bestDistSq) {
                    found = true;
                    bestDistSq = distSq;
                    target = center;
                }
            }
        }

        if (boss && boss->alive) {
            Vec2 center = { boss->pos.x + boss->w * 0.5f, boss->pos.y + boss->h * 0.5f };
            float dx = center.x - from.x;
            float dy = center.y - from.y;
            float distSq = dx * dx + dy * dy;
            if (!found || distSq < bestDistSq) {
                found = true;
                target = center;
            }
        }

        if (playerPos) {
            float dx = playerPos->x - from.x;
            float dy = playerPos->y - from.y;
            float distSq = dx * dx + dy * dy;
            if (!found || distSq < bestDistSq) {
                found = true;
                target = *playerPos;
            }
        }

        return target;
    }

    // Magnet: like homing, but only within a limited radius and with a much
    // gentler turn rate — a "bend toward" rather than "track."
    Vec2 FindMagnetTarget(const std::vector<Enemy>* roomEnemies, const Boss* boss, Vec2 from, bool& found) {
        found = false;
        Vec2 target = from;
        float bestDistSq = 0.0f;
        const float radiusSq = 40.0f * 40.0f;

        if (roomEnemies) {
            for (const Enemy& enemy : *roomEnemies) {
                if (!enemy.alive) continue;
                Vec2 center = { enemy.pos.x + enemy.w * 0.5f, enemy.pos.y + enemy.h * 0.5f };
                float dx = center.x - from.x;
                float dy = center.y - from.y;
                float distSq = dx * dx + dy * dy;
                if (distSq > radiusSq) continue;
                if (!found || distSq < bestDistSq) {
                    found = true;
                    bestDistSq = distSq;
                    target = center;
                }
            }
        }

        if (boss && boss->alive) {
            Vec2 center = { boss->pos.x + boss->w * 0.5f, boss->pos.y + boss->h * 0.5f };
            float dx = center.x - from.x;
            float dy = center.y - from.y;
            float distSq = dx * dx + dy * dy;
            if (distSq <= radiusSq && (!found || distSq < bestDistSq)) {
                found = true;
                target = center;
            }
        }

        return target;
    }

    void AdvanceProjectile(Projectile& p, float dt, const std::vector<Enemy>* roomEnemies, const Boss* boss,
                           const Vec2* playerPos) {
        if (p.kind == ProjectileKind::ROCKET) {
            float speed = std::sqrt(p.vel.x * p.vel.x + p.vel.y * p.vel.y);
            if (speed > 0.01f) {
                Vec2 dir = Normalize(p.vel);
                speed = std::min(speed + 40.0f * dt, 165.0f);
                p.vel = { dir.x * speed, dir.y * speed };
            }
        }

        if (p.homing) {
            bool found = false;
            Vec2 target = FindHomingTarget(roomEnemies, boss, playerPos, p.pos, found);
            if (found) {
                Vec2 desired = Normalize({ target.x - p.pos.x, target.y - p.pos.y });
                float speed = std::sqrt(p.vel.x * p.vel.x + p.vel.y * p.vel.y);
                Vec2 newVel = { desired.x * speed, desired.y * speed };
                const float turn = std::min(1.0f, 10.0f * dt);
                p.vel.x += (newVel.x - p.vel.x) * turn;
                p.vel.y += (newVel.y - p.vel.y) * turn;
            }
        } else if (p.magnet) {
            bool found = false;
            Vec2 target = FindMagnetTarget(roomEnemies, boss, p.pos, found);
            if (found) {
                Vec2 desired = Normalize({ target.x - p.pos.x, target.y - p.pos.y });
                float speed = std::sqrt(p.vel.x * p.vel.x + p.vel.y * p.vel.y);
                Vec2 newVel = { desired.x * speed, desired.y * speed };
                const float turn = std::min(1.0f, 3.0f * dt);
                p.vel.x += (newVel.x - p.vel.x) * turn;
                p.vel.y += (newVel.y - p.vel.y) * turn;
            }
        }

        // Wall bounce (Ricochet Core): reflect off the room boundary.
        if (p.wallBounce && p.wallBounceCount > 0) {
            if ((p.pos.x <= WALL_MIN_X && p.vel.x < 0.0f) || (p.pos.x >= WALL_MAX_X && p.vel.x > 0.0f)) {
                p.vel.x = -p.vel.x;
                p.wallBounceCount--;
            }
            if ((p.pos.y <= WALL_MIN_Y && p.vel.y < 0.0f) || (p.pos.y >= WALL_MAX_Y && p.vel.y > 0.0f)) {
                p.vel.y = -p.vel.y;
                p.wallBounceCount--;
            }
        }

        float stepX = p.vel.x * dt;
        float stepY = p.vel.y * dt;
        p.pos.x += stepX;
        p.pos.y += stepY;

        float travel = std::sqrt(stepX * stepX + stepY * stepY);
        p.remainingRange -= travel;
        p.traveledDistance += travel;

        // Growing / shrinking: size+damage scale ramps with distance traveled.
        if (p.growing) {
            float t = std::min(p.traveledDistance / 150.0f, 1.0f);
            p.sizeScale = std::max(p.sizeScale, 1.0f + t); // up to +100% at 150 units
        } else if (p.shrinking) {
            float t = std::min(p.traveledDistance / 100.0f, 1.0f);
            p.sizeScale = std::min(p.sizeScale, 1.0f - t * 0.6f); // down to 40% at 100 units
            if (p.sizeScale < 0.4f) p.sizeScale = 0.4f;
        }

        if (p.remainingRange <= 0.0f) {
            if (p.boomerang && !p.returning) {
                p.returning = true;
                p.vel.x = -p.vel.x;
                p.vel.y = -p.vel.y;
                p.remainingRange = p.maxRange;
            } else {
                p.alive = false;
            }
        }

        if (!p.ignoreBounds && OutOfBounds(p.pos)) {
            p.alive = false;
        }

        if (p.lifeRemaining > 0.0f) {
            p.lifeRemaining -= dt;
            if (p.lifeRemaining <= 0.0f) {
                p.alive = false;
            }
        }
    }

    void ApplyPoison(Enemy& enemy, int damage) {
        enemy.poisonTimer = std::max(enemy.poisonTimer, 2.5f);
        enemy.poisonTickTimer = 0.45f;
        enemy.poisonDamage = std::max(enemy.poisonDamage, std::max(1, damage));
    }

    void ApplySticky(Enemy& enemy) {
        enemy.stickyTimer = std::max(enemy.stickyTimer, 1.6f);
        enemy.stickySpeedMultiplier = 0.55f;
    }

    void ApplyBurn(Enemy& enemy, int damage) {
        enemy.burnTimer = std::max(enemy.burnTimer, 1.5f);
        enemy.burnTickTimer = 0.5f;
        enemy.burnDamage = std::max(enemy.burnDamage, std::max(1, damage + 1));
    }

    void ApplyFreeze(Enemy& enemy) {
        enemy.freezeTimer = std::max(enemy.freezeTimer, 1.2f);
    }

    void ApplyPoison(Boss& boss, int damage) {
        boss.poisonTimer = std::max(boss.poisonTimer, 2.5f);
        boss.poisonTickTimer = 0.45f;
        boss.poisonDamage = std::max(boss.poisonDamage, std::max(1, damage));
    }

    void ApplySticky(Boss& boss) {
        boss.stickyTimer = std::max(boss.stickyTimer, 1.4f);
        boss.stickySpeedMultiplier = 0.60f;
    }

    void ApplyBurn(Boss& boss, int damage) {
        boss.burnTimer = std::max(boss.burnTimer, 1.5f);
        boss.burnTickTimer = 0.5f;
        boss.burnDamage = std::max(boss.burnDamage, std::max(1, damage + 1));
    }

    void ApplyFreeze(Boss& boss) {
        // Bosses are heavily resistant to freeze.
        boss.freezeTimer = std::max(boss.freezeTimer, 1.2f * Boss::FREEZE_RESIST);
    }

    void ApplyGravityPulse(std::vector<Enemy>& roomEnemies, Vec2 center, float radius, float pullStrength) {
        float radiusSq = radius * radius;
        for (Enemy& other : roomEnemies) {
            if (!other.alive) continue;
            Vec2 otherCenter = { other.pos.x + other.w * 0.5f, other.pos.y + other.h * 0.5f };
            float dx = center.x - otherCenter.x;
            float dy = center.y - otherCenter.y;
            float distSq = dx * dx + dy * dy;
            if (distSq <= radiusSq && distSq > 1.0f) {
                float dist = std::sqrt(distSq);
                other.pos.x += (dx / dist) * pullStrength;
                other.pos.y += (dy / dist) * pullStrength;
            }
        }
    }

    int EffectiveDamage(const Projectile& p) {
        return std::max(1, (int)std::lround(p.damage * p.sizeScale));
    }

    // Fracture Cell: spawns 2 children at reduced damage, once per shot
    // (children have splitOnImpact cleared, so no infinite recursion).
    void SpawnSplitChildren(std::vector<Projectile>& projectiles, const Projectile& parent, Vec2 impactPos) {
        if (parent.splitGeneration > 0) return;
        int childDamage = std::max(1, (int)(parent.damage * 0.55f));
        float baseAngle = std::atan2(parent.vel.y, parent.vel.x);
        const float spread = 0.55f;
        for (int i = -1; i <= 1; i += 2) {
            float angle = baseAngle + spread * (float)i;
            float speed = std::sqrt(parent.vel.x * parent.vel.x + parent.vel.y * parent.vel.y);
            Projectile child;
            child.pos = impactPos;
            child.vel = { std::cos(angle) * speed, std::sin(angle) * speed };
            child.damage = childDamage;
            child.remainingRange = parent.remainingRange * 0.6f;
            child.maxRange = child.remainingRange;
            child.kind = parent.kind;
            child.piercing = parent.piercing;
            child.splitGeneration = 1;
            projectiles.push_back(child);
        }
    }
}

namespace ProjectileSystem {

void Spawn(std::vector<Projectile>& projectiles, Vec2 pos, Vec2 vel, int damage,
           float remainingRange, float lifeRemaining, const ProjectileMods& mods) {
    Projectile p;
    p.pos = pos;
    p.vel = vel;
    p.damage = damage;
    p.remainingRange = remainingRange;
    p.maxRange = remainingRange;
    p.lifeRemaining = lifeRemaining;

    p.homing = mods.homing;
    p.kind = mods.kind;
    p.poison = mods.poison;
    p.sticky = mods.sticky;
    p.piercing = mods.piercing;
    p.explosive = mods.explosive;
    p.burn = mods.burn;
    p.freeze = mods.freeze;
    p.magnet = mods.magnet;
    p.boomerang = mods.boomerang;
    p.growing = mods.growing;
    p.shrinking = mods.shrinking;
    p.chain = mods.chain;
    p.gravity = mods.gravity;
    p.vortex = mods.vortex;
    p.crit = mods.crit;
    p.lifesteal = mods.lifesteal;
    p.marking = mods.marking;
    p.wallBounce = mods.wallBounce;
    p.enemyBounce = mods.enemyBounce;
    p.splitOnImpact = mods.splitOnImpact;
    p.ignoreBounds = mods.ignoreBounds;

    p.pierceCount = mods.piercing ? 1 : 0;
    p.wallBounceCount = mods.wallBounce ? 2 : 0;
    p.enemyBounceCount = mods.enemyBounce ? 2 : 0;

    if (p.kind == ProjectileKind::LASER || p.kind == ProjectileKind::CRIMSON_RAY || p.kind == ProjectileKind::SLASH) {
        p.ignoreBounds = true;
    }
    if (p.kind == ProjectileKind::ROCKET) {
        p.sizeScale = 1.25f;
    }

    projectiles.push_back(p);
}

void Advance(std::vector<Projectile>& projectiles, float dt, const std::vector<Enemy>* roomEnemies,
             const Boss* boss, const Vec2* playerPos) {
    for (auto& p : projectiles) {
        if (!p.alive) continue;
        AdvanceProjectile(p, dt, roomEnemies, boss, playerPos);
    }

    projectiles.erase(
        std::remove_if(projectiles.begin(), projectiles.end(),
            [](const Projectile& p) { return !p.alive; }),
        projectiles.end()
    );
}

void UpdateAndCollideVsEnemy(std::vector<Projectile>& projectiles, std::vector<Enemy>& roomEnemies, Enemy& enemy,
                             float projectileSize, std::vector<Enemy>& spawnedEnemies,
                             std::vector<RoomPickup>& spawnedPickups,
                             std::vector<Projectile>& enemyProjectiles, float dt, Player* player) {
    bool wasAlive = enemy.alive;
    std::vector<Projectile> splitSpawns;

    for (auto& p : projectiles) {
        if (!p.alive) continue;
        if (enemy.alive && p.alive && Collision::CheckAABB(p.GetRect(projectileSize), enemy.GetRect())) {
            if (enemy.shielded) {
                enemy.shielded = false;
                p.alive = false;
                continue;
            }

            int dmg = EffectiveDamage(p);
            enemy.hp -= dmg;
            if (player) PlayerLogic::RegisterHit(*player);

            if (p.poison) ApplyPoison(enemy, dmg);
            if (p.sticky) ApplySticky(enemy);
            if (p.burn) ApplyBurn(enemy, dmg);
            if (p.freeze) ApplyFreeze(enemy);

            if (p.marking) {
                enemy.markStacks++;
                if (enemy.markStacks >= 5) {
                    enemy.markStacks = 0;
                    enemy.hp -= std::max(4, dmg);
                }
            }

            if (p.explosive) {
                float splashRadius = 18.0f;
                if (p.kind == ProjectileKind::ROCKET) splashRadius = 28.0f;
                const float splashRadiusSq = splashRadius * splashRadius;
                const Vec2 center = { enemy.pos.x + enemy.w * 0.5f, enemy.pos.y + enemy.h * 0.5f };
                for (Enemy& other : roomEnemies) {
                    if (!other.alive) continue;
                    Vec2 otherCenter = { other.pos.x + other.w * 0.5f, other.pos.y + other.h * 0.5f };
                    float dx = otherCenter.x - center.x;
                    float dy = otherCenter.y - center.y;
                    if (dx * dx + dy * dy <= splashRadiusSq) {
                        other.hp -= std::max(1, dmg / 2);
                        if (other.hp <= 0) other.alive = false;
                    }
                }
            }

            if (p.gravity || p.vortex) {
                const Vec2 center = { enemy.pos.x + enemy.w * 0.5f, enemy.pos.y + enemy.h * 0.5f };
                float radius = p.vortex ? 40.0f : 22.0f;
                float pull = p.vortex ? 14.0f : 6.0f;
                ApplyGravityPulse(roomEnemies, center, radius, pull);
            }

            if (p.chain) {
                const int maxJumps = 3;
                float jumpDamage = (float)dmg;
                Vec2 fromCenter = { enemy.pos.x + enemy.w * 0.5f, enemy.pos.y + enemy.h * 0.5f };
                const float chainRadiusSq = 45.0f * 45.0f;
                for (int jump = 0; jump < maxJumps; ++jump) {
                    jumpDamage *= 0.6f;
                    Enemy* best = nullptr;
                    float bestDistSq = 0.0f;
                    for (Enemy& other : roomEnemies) {
                        if (!other.alive || &other == &enemy) continue;
                        Vec2 otherCenter = { other.pos.x + other.w * 0.5f, other.pos.y + other.h * 0.5f };
                        float dx = otherCenter.x - fromCenter.x;
                        float dy = otherCenter.y - fromCenter.y;
                        float distSq = dx * dx + dy * dy;
                        if (distSq > chainRadiusSq) continue;
                        if (!best || distSq < bestDistSq) {
                            best = &other;
                            bestDistSq = distSq;
                        }
                    }
                    if (!best) break;
                    best->hp -= std::max(1, (int)std::lround(jumpDamage));
                    if (best->hp <= 0) best->alive = false;
                    fromCenter = { best->pos.x + best->w * 0.5f, best->pos.y + best->h * 0.5f };
                }
            }

            if (p.splitOnImpact) {
                SpawnSplitChildren(splitSpawns, p, p.pos);
            }

            if (p.lifesteal && enemy.hp <= 0 && player) {
                player->hp = std::min(player->maxHp, player->hp + 1);
            }

            bool consumedByPierce = false;
            if (p.piercing && p.pierceCount > 0) {
                p.pierceCount--;
                p.pos.x += p.vel.x * dt * 0.5f;
                p.pos.y += p.vel.y * dt * 0.5f;
                consumedByPierce = true;
                if (p.pierceCount <= 0) {
                    // still allowed to also bounce below if it has bounce charges;
                    // otherwise this was its last pierce so let it die naturally later.
                }
            }

            if (!consumedByPierce) {
                if (p.enemyBounce && p.enemyBounceCount > 0) {
                    p.enemyBounceCount--;
                    Vec2 center = { enemy.pos.x + enemy.w * 0.5f, enemy.pos.y + enemy.h * 0.5f };
                    Vec2 away = Normalize({ p.pos.x - center.x, p.pos.y - center.y });
                    if (away.x == 0.0f && away.y == 0.0f) away = { -p.vel.x, -p.vel.y };
                    float speed = std::sqrt(p.vel.x * p.vel.x + p.vel.y * p.vel.y);
                    p.vel = { away.x * speed, away.y * speed };
                } else {
                    p.alive = false;
                }
            }

            if (enemy.hp <= 0) enemy.alive = false;
        }
    }

    if (!splitSpawns.empty()) {
        projectiles.insert(projectiles.end(), splitSpawns.begin(), splitSpawns.end());
    }

    if (wasAlive && !enemy.alive) {
        if (enemy.specialType == EnemySpecialType::REINFORCER) {
            for (int i = 0; i < 2; ++i) {
                Vec2 offset = { (float)(i * 10 - 5), (float)(i * 6 - 3) };
                Enemy child = EnemyDatabase::Spawn(enemy.templateName, { enemy.pos.x + offset.x, enemy.pos.y + offset.y });
                spawnedEnemies.push_back(child);
            }
      } else if (enemy.specialType == EnemySpecialType::DEATH_RING) {
            const int count = 12;
            const float speed = 95.0f;
            for (int i = 0; i < count; ++i) {
                float angle = (6.2831853f) * ((float)i / count);
                ProjectileSystem::Spawn(
                    enemyProjectiles,
                    enemy.pos,
                    { std::cos(angle) * speed, std::sin(angle) * speed },
                    2,
                    999999.0f
                );
            }
        } else if (enemy.mimicsItemPickup && enemy.mimicItemId >= 0) {
            RoomPickup pickup;
            pickup.type = RoomPickupType::ITEM;
            pickup.itemId = enemy.mimicItemId;
            pickup.pos = { enemy.pos.x, enemy.pos.y };
            spawnedPickups.push_back(pickup);
        }

        if (enemy.splitsOnDeath && !enemy.isSplitChild) {
            for (int i = 0; i < 2; ++i) {
                Vec2 offset = { (float)(i * 8 - 4), (float)(i * 5 - 2) };
                Enemy child = EnemyDatabase::Spawn(enemy.templateName, { enemy.pos.x + offset.x, enemy.pos.y + offset.y });
                child.hp = std::max(1, enemy.maxHp / 3);
                child.maxHp = child.hp;
                child.isSplitChild = true;
                spawnedEnemies.push_back(child);
            }
        }

        // Parasite Core: on any enemy death, fire 2 small player shots
        // outward from the death position.
        if (player && player->hasParasiteCore) {
            const float speed = 100.0f;
            ProjectileMods childMods;
            for (int i = 0; i < 2; ++i) {
                float angle = (3.14159265f) * (float)i;
                ProjectileSystem::Spawn(
                    projectiles, enemy.pos,
                    { std::cos(angle) * speed, std::sin(angle) * speed },
                    std::max(1, player->damage / 3),
                    80.0f, 0.0f, childMods
                );
            }
        }
    }

    projectiles.erase(
        std::remove_if(projectiles.begin(), projectiles.end(),
            [](const Projectile& p) { return !p.alive; }),
        projectiles.end()
    );
}

bool UpdateAndCollideVsPlayer(std::vector<Projectile>& projectiles, Player& player, float projectileSize,
                               float invincibleDuration, int currentFloor, float dt) {
    bool tookDamage = false;
    for (auto& p : projectiles) {
        if (!p.alive) continue;
        Rect projRect = p.GetRect(projectileSize);
        if (p.alive && Collision::CheckAABB(projRect, player.GetRect())) {
            p.alive = false;

            if (player.hasMirrorWard && player.mirrorWardCharges > 0) {
                player.mirrorWardCharges--;
                continue;
            }

            int damage = p.damage;
            if (currentFloor >= 3) damage = std::max(damage, 2);
            tookDamage = PlayerLogic::TakeDamage(player, damage, invincibleDuration) || tookDamage;
        }
    }

    projectiles.erase(
        std::remove_if(projectiles.begin(), projectiles.end(),
            [](const Projectile& p) { return !p.alive; }),
        projectiles.end()
    );

    return tookDamage;
}

void UpdateAndCollideVsBoss(std::vector<Projectile>& projectiles, Boss& boss, float projectileSize, float dt,
                             Player* player) {
    for (auto& p : projectiles) {
        if (!p.alive) continue;
        if (boss.alive && p.alive && Collision::CheckAABB(p.GetRect(projectileSize), boss.GetRect())) {
            int dmg = EffectiveDamage(p);
            boss.hp -= dmg;
            if (player) PlayerLogic::RegisterHit(*player);

            if (p.poison) ApplyPoison(boss, dmg);
            if (p.sticky) ApplySticky(boss);
            if (p.burn) ApplyBurn(boss, dmg);
            if (p.freeze) ApplyFreeze(boss);

            if (p.marking) {
                boss.markStacks++;
                if (boss.markStacks >= 5) {
                    boss.markStacks = 0;
                    boss.hp -= std::max(4, dmg);
                }
            }

            if (p.lifesteal && boss.hp <= 0 && player) {
                player->hp = std::min(player->maxHp, player->hp + 1);
            }

            bool consumedByPierce = false;
            if (p.piercing && p.pierceCount > 0) {
                p.pierceCount--;
                p.pos.x += p.vel.x * dt * 0.5f;
                p.pos.y += p.vel.y * dt * 0.5f;
                consumedByPierce = true;
            }

            if (!consumedByPierce) {
                if (p.enemyBounce && p.enemyBounceCount > 0) {
                    p.enemyBounceCount--;
                    Vec2 center = { boss.pos.x + boss.w * 0.5f, boss.pos.y + boss.h * 0.5f };
                    Vec2 away = Normalize({ p.pos.x - center.x, p.pos.y - center.y });
                    if (away.x == 0.0f && away.y == 0.0f) away = { -p.vel.x, -p.vel.y };
                    float speed = std::sqrt(p.vel.x * p.vel.x + p.vel.y * p.vel.y);
                    p.vel = { away.x * speed, away.y * speed };
                } else {
                    p.alive = false;
                }
            }

            if (boss.hp <= 0) { boss.hp = 0; boss.alive = false; }
        }
    }

    projectiles.erase(
        std::remove_if(projectiles.begin(), projectiles.end(),
            [](const Projectile& p) { return !p.alive; }),
        projectiles.end()
    );
}

void Draw(const std::vector<Projectile>& projectiles, int size, uint32_t color, Vec2 offset) {
    for (auto& p : projectiles) {
        int drawSize = (int)std::lround(size * p.sizeScale);
        if (drawSize < 1) drawSize = 1;
        Renderer::DrawRect((int)(p.pos.x + offset.x), (int)(p.pos.y + offset.y), drawSize, drawSize, ProjectileColor(p, color));
    }
}

void UpdateOrbiters(Player& player, float dt, std::vector<Enemy>* roomEnemies, Boss* boss) {
    const float orbitRadius = 20.0f;
    const float angularSpeed = 3.0f;
    const float hitRadius = 6.0f;
    const float hitRadiusSq = hitRadius * hitRadius;
    const float hitCooldownDuration = 0.35f;

    Vec2 playerCenter = { player.pos.x + player.size / 2.0f, player.pos.y + player.size / 2.0f };

    int writeIdx = 0;
    for (int i = 0; i < player.orbiterCount; ++i) {
        Orbiter& o = player.orbiters[i];
        o.timeRemaining -= dt;
        if (o.timeRemaining <= 0.0f) continue; // drop expired orbiter

        o.angle += angularSpeed * dt;
        if (o.hitCooldown > 0.0f) o.hitCooldown -= dt;

        Vec2 orbPos = {
            playerCenter.x + std::cos(o.angle) * orbitRadius,
            playerCenter.y + std::sin(o.angle) * orbitRadius
        };

        if (o.hitCooldown <= 0.0f) {
            if (roomEnemies) {
                for (Enemy& enemy : *roomEnemies) {
                    if (!enemy.alive) continue;
                    Vec2 center = { enemy.pos.x + enemy.w * 0.5f, enemy.pos.y + enemy.h * 0.5f };
                    float dx = center.x - orbPos.x;
                    float dy = center.y - orbPos.y;
                    if (dx * dx + dy * dy <= hitRadiusSq) {
                        enemy.hp -= o.damage;
                        if (enemy.hp <= 0) enemy.alive = false;
                        o.hitCooldown = hitCooldownDuration;
                        break;
                    }
                }
            }
            if (boss && boss->alive && o.hitCooldown <= 0.0f) {
                Vec2 center = { boss->pos.x + boss->w * 0.5f, boss->pos.y + boss->h * 0.5f };
                float dx = center.x - orbPos.x;
                float dy = center.y - orbPos.y;
                if (dx * dx + dy * dy <= hitRadiusSq) {
                    boss->hp -= o.damage;
                    if (boss->hp <= 0) { boss->hp = 0; boss->alive = false; }
                    o.hitCooldown = hitCooldownDuration;
                }
            }
        }

        if (writeIdx != i) player.orbiters[writeIdx] = o;
        writeIdx++;
    }
    player.orbiterCount = writeIdx;
}

void DrawOrbiters(const Player& player, Vec2 offset) {
    Vec2 playerCenter = { player.pos.x + player.size / 2.0f, player.pos.y + player.size / 2.0f };
    for (int i = 0; i < player.orbiterCount; ++i) {
        const Orbiter& o = player.orbiters[i];
        Vec2 orbPos = {
            playerCenter.x + std::cos(o.angle) * 20.0f,
            playerCenter.y + std::sin(o.angle) * 20.0f
        };
        int baseX = (int)std::lround(orbPos.x + offset.x);
        int baseY = (int)std::lround(orbPos.y + offset.y);
        Renderer::DrawRect(baseX - 2, baseY - 2, 4, 4, 0xFF66CCFF);

        // A tiny leading sparkle makes the shard's rotation visible instead of
        // looking like a plain square.
        int tipX = (int)std::lround(orbPos.x + std::cos(o.angle) * 4.0f + offset.x);
        int tipY = (int)std::lround(orbPos.y + std::sin(o.angle) * 4.0f + offset.y);
        Renderer::DrawRect(tipX - 1, tipY - 1, 2, 2, 0xFFFFFFFF);
    }
}

} // namespace ProjectileSystem
