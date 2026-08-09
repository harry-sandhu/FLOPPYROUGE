#include "projectile_system.h"
#include <algorithm>
#include <cmath>
#include "../../engine/collision.h"
#include "../../engine/renderer.h"
#include "../enemies/enemy_database.h"

namespace {
    // Generous off-room bounds check — internal resolution is 320x180.
    bool OutOfBounds(Vec2 pos) {
        return pos.x < -16.0f || pos.x > 336.0f || pos.y < -16.0f || pos.y > 196.0f;
    }

    Vec2 Normalize(Vec2 v) {
        float lenSq = v.x * v.x + v.y * v.y;
        if (lenSq > 0.0001f) {
            float len = std::sqrt(lenSq);
            v.x /= len;
            v.y /= len;
        }
        return v;
    }

    Vec2 FindHomingTarget(const std::vector<Enemy>* roomEnemies, const Boss* boss, Vec2 from, bool& found) {
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

        return target;
    }

    void AdvanceProjectile(Projectile& p, float dt, const std::vector<Enemy>* roomEnemies, const Boss* boss) {
        if (p.homing) {
            bool found = false;
            Vec2 target = FindHomingTarget(roomEnemies, boss, p.pos, found);
            if (found) {
                Vec2 desired = Normalize({ target.x - p.pos.x, target.y - p.pos.y });
                float speed = std::sqrt(p.vel.x * p.vel.x + p.vel.y * p.vel.y);
                Vec2 newVel = { desired.x * speed, desired.y * speed };
                const float turn = std::min(1.0f, 10.0f * dt);
                p.vel.x += (newVel.x - p.vel.x) * turn;
                p.vel.y += (newVel.y - p.vel.y) * turn;
            }
        }

        float stepX = p.vel.x * dt;
        float stepY = p.vel.y * dt;
        p.pos.x += stepX;
        p.pos.y += stepY;

        float travel = std::sqrt(stepX * stepX + stepY * stepY);
        p.remainingRange -= travel;

        if (p.remainingRange <= 0.0f || OutOfBounds(p.pos)) {
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

    void ApplyPoison(Boss& boss, int damage) {
        boss.poisonTimer = std::max(boss.poisonTimer, 2.5f);
        boss.poisonTickTimer = 0.45f;
        boss.poisonDamage = std::max(boss.poisonDamage, std::max(1, damage));
    }

    void ApplySticky(Boss& boss) {
        boss.stickyTimer = std::max(boss.stickyTimer, 1.4f);
        boss.stickySpeedMultiplier = 0.60f;
    }
}

namespace ProjectileSystem {

void Spawn(std::vector<Projectile>& projectiles, Vec2 pos, Vec2 vel, int damage,
           float remainingRange, float lifeRemaining, bool homing, bool poison,
           bool sticky, bool piercing, bool explosive) {
    Projectile p;
    p.pos = pos;
    p.vel = vel;
    p.damage = damage;
    p.remainingRange = remainingRange;
    p.lifeRemaining = lifeRemaining;
    p.homing = homing;
    p.poison = poison;
    p.sticky = sticky;
    p.piercing = piercing;
    p.explosive = explosive;
    p.pierceCount = piercing ? 1 : 0;
    projectiles.push_back(p);
}

void Advance(std::vector<Projectile>& projectiles, float dt, const std::vector<Enemy>* roomEnemies, const Boss* boss) {
    for (auto& p : projectiles) {
        if (!p.alive) continue;
        AdvanceProjectile(p, dt, roomEnemies, boss);
    }

    projectiles.erase(
        std::remove_if(projectiles.begin(), projectiles.end(),
            [](const Projectile& p) { return !p.alive; }),
        projectiles.end()
    );
}

void UpdateAndCollideVsEnemy(std::vector<Projectile>& projectiles, std::vector<Enemy>& roomEnemies, Enemy& enemy,
                             float projectileSize, std::vector<Enemy>& spawnedEnemies,
                             std::vector<Projectile>& enemyProjectiles, float dt) {
    bool wasAlive = enemy.alive;
    for (auto& p : projectiles) {
        if (!p.alive) continue;
        if (enemy.alive && p.alive && Collision::CheckAABB(p.GetRect(projectileSize), enemy.GetRect())) {
            if (enemy.shielded) {
                enemy.shielded = false;
                p.alive = false;
                continue;
            }
            enemy.hp -= p.damage;
            if (p.poison) ApplyPoison(enemy, p.damage);
            if (p.sticky) ApplySticky(enemy);
            if (p.explosive) {
                const float splashRadiusSq = 18.0f * 18.0f;
                const Vec2 center = { enemy.pos.x + enemy.w * 0.5f, enemy.pos.y + enemy.h * 0.5f };
                for (Enemy& other : roomEnemies) {
                    if (!other.alive) continue;
                    Vec2 otherCenter = { other.pos.x + other.w * 0.5f, other.pos.y + other.h * 0.5f };
                    float dx = otherCenter.x - center.x;
                    float dy = otherCenter.y - center.y;
                    if (dx * dx + dy * dy <= splashRadiusSq) {
                        other.hp -= std::max(1, p.damage / 2);
                        if (other.hp <= 0) other.alive = false;
                    }
                }
            }

            if (p.piercing && p.pierceCount > 0) {
                p.pierceCount--;
                p.pos.x += p.vel.x * dt * 0.5f;
                p.pos.y += p.vel.y * dt * 0.5f;
                if (p.pierceCount <= 0) {
                    p.alive = false;
                }
            } else {
                p.alive = false;
            }

            if (enemy.hp <= 0) enemy.alive = false;
        }
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

void UpdateAndCollideVsBoss(std::vector<Projectile>& projectiles, Boss& boss, float projectileSize, float dt) {
    for (auto& p : projectiles) {
        if (!p.alive) continue;
        if (boss.alive && p.alive && Collision::CheckAABB(p.GetRect(projectileSize), boss.GetRect())) {
            boss.hp -= p.damage;
            if (p.poison) ApplyPoison(boss, p.damage);
            if (p.sticky) ApplySticky(boss);
            if (p.piercing && p.pierceCount > 0) {
                p.pierceCount--;
                p.pos.x += p.vel.x * dt * 0.5f;
                p.pos.y += p.vel.y * dt * 0.5f;
                if (p.pierceCount <= 0) {
                    p.alive = false;
                }
            } else {
                p.alive = false;
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
        Renderer::DrawRect((int)(p.pos.x + offset.x), (int)(p.pos.y + offset.y), size, size, color);
    }
}

} // namespace ProjectileSystem
