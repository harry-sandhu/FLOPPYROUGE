#include <windows.h>
#include <vector>
#include "../engine/window.h"
#include "../engine/renderer.h"
#include "../engine/input.h"
#include "../engine/core/timer.h"
#include "../engine/core/rng.h"
#include "../engine/text.h"
#include "../game/player/player.h"
#include "../game/player/projectile_system.h"
#include "../game/items/item_database.h"
#include "../game/items/item_system.h"
#include "../game/enemies/enemy.h"
#include "../game/enemies/enemy_database.h"
#include "../game/bosses/boss.h"
#include "../game/dungeon/dungeon.h"
#include "../game/ui/hud.h"
#include "../engine/collision.h"

enum class GameState { RUNNING, WON, LOST };

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    if (!Window::Create(1280, 720, "FloppyRogue")) return 1;
    if (!Renderer::Init(Window::GetHandle())) return 1;

    EnemyDatabase::Load("data/enemies.txt");
    ItemDatabase::Load("data/items.txt");

    Timer timer;
    Dungeon dungeon;

    Player player;
    std::vector<Enemy> enemies;
    Boss boss;
    std::vector<Projectile> playerShots;
    std::vector<Projectile> enemyShots;

    const int contactDamage = 10;
    const float invincibleDuration = 0.75f;
    const float projectileSize = 3.0f;

    GameState state = GameState::RUNNING;

    auto ScaleEnemyForFloor = [&](Enemy& enemy) {
        int floor = dungeon.CurrentFloor();
        float floorOffset = (float)(floor - 1);

        enemy.hp = (int)(enemy.hp * (1.0f + 0.30f * floorOffset));
        if (enemy.hp < 1) enemy.hp = 1;
        enemy.maxHp = enemy.hp;
        enemy.speed *= 1.0f + 0.10f * floorOffset;
        enemy.shootCooldown *= 1.0f - 0.08f * floorOffset;
        if (enemy.shootCooldown < 0.45f) enemy.shootCooldown = 0.45f;
        enemy.shootRange *= 1.0f + 0.05f * floorOffset;
        enemy.preferredDistance *= 1.0f + 0.03f * floorOffset;
        enemy.shotSpeed *= 1.0f + 0.05f * floorOffset;
        enemy.spawnDelayRemaining = 0.5f;
    };

    auto ScaleBossForFloor = [&](Boss& boss) {
        int floor = dungeon.CurrentFloor();
        float floorOffset = (float)(floor - 1);

        boss.hp = (int)(boss.hp * (1.0f + 0.45f * floorOffset));
        if (boss.hp < 1) boss.hp = 1;
        boss.maxHp = boss.hp;
        boss.driftSpeed *= 1.0f + 0.08f * floorOffset;
        boss.attackCooldownPhase1 *= 1.0f - 0.06f * floorOffset;
        boss.attackCooldownPhase2 *= 1.0f - 0.06f * floorOffset;
        if (boss.attackCooldownPhase1 < 0.8f) boss.attackCooldownPhase1 = 0.8f;
        if (boss.attackCooldownPhase2 < 0.55f) boss.attackCooldownPhase2 = 0.55f;
        boss.chargeSpeed *= 1.0f + 0.08f * floorOffset;
        boss.spawnDelayRemaining = 0.5f;
        boss.attackTimer = boss.attackCooldownPhase1;
    };

    auto MakeSpecialEnemy = [&](Enemy& enemy) {
        if (!RNG::Chance(0.05f)) return;

        enemy.specialType = (EnemySpecialType)RNG::Range(1, 3);
        enemy.creepDropTimer = 0.0f;

        switch (enemy.specialType) {
            case EnemySpecialType::REINFORCER:
                enemy.hp = (int)(enemy.hp * 1.55f) + 8;
                enemy.speed *= 1.08f;
                enemy.shootCooldown *= 0.88f;
                enemy.shootRange *= 1.05f;
                break;
            case EnemySpecialType::CREEPER:
                enemy.hp = (int)(enemy.hp * 1.30f) + 4;
                enemy.speed *= 1.15f;
                enemy.shootCooldown *= 0.92f;
                enemy.shootRange *= 1.10f;
                break;
            case EnemySpecialType::DEATH_RING:
                enemy.hp = (int)(enemy.hp * 1.40f) + 6;
                enemy.speed *= 1.05f;
                enemy.shootCooldown *= 0.90f;
                enemy.shotSpeed *= 1.10f;
                break;
            case EnemySpecialType::NONE:
            default:
                break;
        }

        if (enemy.hp < 1) enemy.hp = 1;
        enemy.maxHp = enemy.hp;
        enemy.spawnDelayRemaining = 0.5f;
    };

    auto LoadRoomEncounter = [&]() {
        const Room& room = dungeon.CurrentRoom();
        const Vec2 spawnPoints[] = {
            { 60.0f, 30.0f },
            { 220.0f, 30.0f },
            { 60.0f, 110.0f },
            { 220.0f, 110.0f },
            { 140.0f, 60.0f }
        };

        enemies.clear();
        enemyShots.clear();
        playerShots.clear();
        boss = Boss{};
        boss.alive = false;

        if (room.type == RoomType::NORMAL && !room.cleared) {
            if (!room.enemySpawnList.empty()) {
                for (int i = 0; i < (int)room.enemySpawnList.size(); ++i) {
                    Vec2 spawnPos = spawnPoints[i % (int)(sizeof(spawnPoints) / sizeof(spawnPoints[0]))];
                    Enemy enemy = EnemyDatabase::Spawn(room.enemySpawnList[i], spawnPos);
                    ScaleEnemyForFloor(enemy);
                    MakeSpecialEnemy(enemy);
                    enemies.push_back(enemy);
                }
            } else {
                Enemy zombie = EnemyDatabase::Spawn("Zombie", { 100.0f, 30.0f });
                Enemy gunner = EnemyDatabase::Spawn("Gunner", { 220.0f, 30.0f });
                ScaleEnemyForFloor(zombie);
                ScaleEnemyForFloor(gunner);
                MakeSpecialEnemy(zombie);
                MakeSpecialEnemy(gunner);
                enemies.push_back(zombie);
                enemies.push_back(gunner);
            }
        } else if (room.type == RoomType::BOSS && !room.cleared) {
            boss = SpawnBossVariant(room.bossVariant);
            ScaleBossForFloor(boss);
        } else if ((room.type == RoomType::TREASURE || room.type == RoomType::CURSE) && !room.lootGranted) {
            for (int itemId : room.itemSpawnList) {
                ItemSystem::GrantItem(player, itemId);
            }
            dungeon.CurrentRoom().lootGranted = true;
        }
    };

    auto ResetRun = [&]() {
        player = Player{};
        state = GameState::RUNNING;

        uint32_t seedBase = (uint32_t)GetTickCount();
        bool generated = false;
        for (int attempt = 0; attempt < 8 && !generated; ++attempt) {
            generated = dungeon.Generate(seedBase + (uint32_t)attempt * 17u);
        }

        if (!generated) {
            state = GameState::LOST;
            return;
        }

        dungeon.PlacePlayerAtCurrentRoomCenter(player);
        LoadRoomEncounter();
    };

    ResetRun();

    while (Window::PollEvents()) {
        float dt = timer.Tick();
        Input::Update();

        if (state == GameState::RUNNING) {
            PlayerLogic::UpdateTimers(player, dt);
            ProjectileSystem::Advance(playerShots, dt);
            ProjectileSystem::Advance(enemyShots, dt);
            PlayerLogic::HandleMovement(player, dt);

            if (dungeon.TryTransition(player)) {
                LoadRoomEncounter();
            }

            const Room& room = dungeon.CurrentRoom();
            player.pos = room.ClampToRoom(player.pos, (float)player.size, (float)player.size);
            PlayerLogic::HandleShooting(player, dt, playerShots);

            if (room.type == RoomType::NORMAL && !room.cleared) {
                bool anyAlive = false;
                std::vector<Enemy> spawnedEnemies;
                for (auto& enemy : enemies) {
                    EnemyAI::Update(enemy, player.pos, dt, enemies, spawnedEnemies, enemyShots);
                    enemy.pos = room.ClampToRoom(enemy.pos, enemy.w, enemy.h);

                    ProjectileSystem::UpdateAndCollideVsEnemy(playerShots, enemy, projectileSize,
                                                              spawnedEnemies, enemyShots, dt);

                    if (enemy.alive && Collision::CheckAABB(player.GetRect(), enemy.GetRect())) {
                        PlayerLogic::TakeDamage(player, contactDamage, invincibleDuration);
                    }
                    if (enemy.alive) anyAlive = true;
                }

                if (!spawnedEnemies.empty()) {
                    for (auto& spawned : spawnedEnemies) {
                        ScaleEnemyForFloor(spawned);
                    }
                    enemies.insert(enemies.end(), spawnedEnemies.begin(), spawnedEnemies.end());
                    anyAlive = true;
                }

                ProjectileSystem::UpdateAndCollideVsPlayer(enemyShots, player, projectileSize,
                                                            invincibleDuration, dt);

                if (!anyAlive) {
                    dungeon.MarkCurrentRoomCleared();
                    enemies.clear();
                    enemyShots.clear();
                    playerShots.clear();
                }
            } else if (room.type == RoomType::BOSS) {
                BossAI::Update(boss, player.pos, dt, enemyShots);
                ProjectileSystem::UpdateAndCollideVsBoss(playerShots, boss, projectileSize, dt);
                ProjectileSystem::UpdateAndCollideVsPlayer(enemyShots, player, projectileSize,
                                                            invincibleDuration, dt);

                if (boss.alive && Collision::CheckAABB(player.GetRect(), boss.GetRect())) {
                    int dmg = boss.isCharging ? boss.chargeContactDamage : boss.contactDamage;
                    PlayerLogic::TakeDamage(player, dmg, invincibleDuration);
                }

                if (!boss.alive) {
                    dungeon.MarkCurrentRoomCleared();
                    enemyShots.clear();
                    playerShots.clear();
                    if (dungeon.AdvanceFloor((uint32_t)GetTickCount() + 97u * (uint32_t)dungeon.CurrentFloor())) {
                        dungeon.PlacePlayerAtCurrentRoomCenter(player);
                        LoadRoomEncounter();
                    } else {
                        state = GameState::WON;
                    }
                }
            }

            if (dungeon.IsFinalFloor() && dungeon.AllCombatRoomsCleared()) {
                state = GameState::WON;
            }

            if (player.hp <= 0) {
                state = GameState::LOST;
            }
        } else {
            if (Input::IsPressed('R')) ResetRun();
        }

        const Room& room = dungeon.CurrentRoom();

        Renderer::Clear(0xFF1A1A1A);

        uint32_t playerColor = player.IsInvincible() ? 0xFFFF8888 : 0xFF00FF88;
        Renderer::DrawRect((int)player.pos.x, (int)player.pos.y, player.size, player.size, playerColor);

        if (room.type == RoomType::NORMAL) {
            for (auto& enemy : enemies) {
                if (!enemy.alive) continue;
                uint32_t color = (enemy.aiType == AIType::SHOOTER) ? 0xFFFF9933 : 0xFFFF3333;
                if (enemy.specialType == EnemySpecialType::REINFORCER) color = 0xFF66DDFF;
                if (enemy.specialType == EnemySpecialType::CREEPER) color = 0xFF55FF55;
                if (enemy.specialType == EnemySpecialType::DEATH_RING) color = 0xFFFFFF66;
                Renderer::DrawRect((int)enemy.pos.x, (int)enemy.pos.y, (int)enemy.w, (int)enemy.h, color);
            }
        }

        if (room.type == RoomType::BOSS && boss.alive) {
            uint32_t baseColor = 0xFFAA33FF;
            if (boss.variant == 1) baseColor = 0xFFFF9933;
            if (boss.variant == 2) baseColor = 0xFF33FFCC;
            uint32_t bossColor = boss.isCharging ? 0xFFFF3399 : baseColor;
            Renderer::DrawRect((int)boss.pos.x, (int)boss.pos.y, (int)boss.w, (int)boss.h, bossColor);
            HUD::DrawBossHealthBar(boss);
        }

        ProjectileSystem::Draw(playerShots, (int)projectileSize, 0xFFFFFF00);
        ProjectileSystem::Draw(enemyShots, (int)projectileSize, 0xFFFF66FF);

        HUD::DrawHealthBar(player);
        if (state == GameState::LOST) HUD::DrawGameOverBanner();
        if (state == GameState::WON || (state == GameState::RUNNING && room.type == RoomType::NORMAL && room.cleared)) {
            HUD::DrawRoomClearedBanner();
        }
        HUD::DrawFloorMap(dungeon);

        Renderer::Present();
    }

    Renderer::Shutdown();
    Window::Destroy();
    return 0;
}
