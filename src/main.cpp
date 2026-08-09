#include <windows.h>
#include <algorithm>
#include <cmath>
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

enum class GameState { TITLE, PLAYING, GAME_OVER };

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    if (!Window::Create(1280, 720, "FloppyRogue")) return 1;
    if (!Renderer::Init(Window::GetHandle())) return 1;

    EnemyDatabase::Load("data/enemies.txt");
    ItemDatabase::Load("data/items.txt");

    Timer timer;
    Dungeon dungeon;
    dungeon.LoadSettings("data/rooms.txt");

    Player player;
    std::vector<Enemy> enemies;
    Boss boss;
    std::vector<Projectile> playerShots;
    std::vector<Projectile> enemyShots;
    float screenShakeTimer = 0.0f;
    float screenShakeStrength = 0.0f;

    const int contactDamage = 8;
    const float invincibleDuration = 0.75f;
    const float projectileSize = 3.0f;

    GameState state = GameState::TITLE;

    auto AddScreenShake = [&](float duration, float strength) {
        screenShakeTimer = std::max(screenShakeTimer, duration);
        screenShakeStrength = std::max(screenShakeStrength, strength);
    };

    auto DamagePlayer = [&](int amount) {
        if (dungeon.CurrentFloor() >= 3) {
            amount = std::max(amount, 2);
        }
        if (PlayerLogic::TakeDamage(player, amount, invincibleDuration)) {
            AddScreenShake(0.14f, 2.0f);
        }
    };

    auto SpawnItemPickups = [&](Room& room) {
        room.pickups.clear();

        const Vec2 center = {
            room.x + room.width * 0.5f - 4.0f,
            room.y + room.height * 0.5f - 4.0f
        };

        const Vec2 offsets[] = {
            { 0.0f, 0.0f },
            { -18.0f, 0.0f },
            { 18.0f, 0.0f },
            { 0.0f, -14.0f },
            { 0.0f, 14.0f }
        };

        for (int i = 0; i < (int)room.itemSpawnList.size(); ++i) {
            RoomPickup pickup;
            pickup.type = RoomPickupType::ITEM;
            pickup.itemId = room.itemSpawnList[i];
            pickup.pos = {
                center.x + offsets[i % 5].x + (float)(i / 5) * 8.0f,
                center.y + offsets[i % 5].y
            };
            room.pickups.push_back(pickup);
        }
    };

    auto SpawnBossRewards = [&](Room& room) {
        room.pickups.clear();

        const Vec2 center = {
            room.x + room.width * 0.5f - 4.0f,
            room.y + room.height * 0.5f - 4.0f
        };

        if (dungeon.CurrentFloor() < dungeon.MaxFloors()) {
            if (!room.itemSpawnList.empty()) {
                RoomPickup itemPickup;
                itemPickup.type = RoomPickupType::ITEM;
                itemPickup.itemId = room.itemSpawnList[0];
                itemPickup.pos = { center.x, center.y - 12.0f };
                room.pickups.push_back(itemPickup);
            }

            RoomPickup exitPickup;
            exitPickup.type = RoomPickupType::EXIT;
            exitPickup.pos = { center.x, center.y + 10.0f };
            room.pickups.push_back(exitPickup);
        } else {
            RoomPickup trophyPickup;
            trophyPickup.type = RoomPickupType::TROPHY;
            trophyPickup.pos = { center.x, center.y };
            room.pickups.push_back(trophyPickup);
        }

        room.lootGranted = true;
    };

    auto StartRun = [&]() {
        player = Player{};
        state = GameState::PLAYING;
        screenShakeTimer = 0.0f;
        screenShakeStrength = 0.0f;

        uint32_t seedBase = (uint32_t)GetTickCount();
        bool generated = false;
        for (int attempt = 0; attempt < 8 && !generated; ++attempt) {
            generated = dungeon.Generate(seedBase + (uint32_t)attempt * 17u);
        }

        if (!generated) {
            state = GameState::GAME_OVER;
            return;
        }

        dungeon.PlacePlayerAtCurrentRoomCenter(player);
        enemies.clear();
        enemyShots.clear();
        playerShots.clear();
        boss = Boss{};
        boss.alive = false;
    };

    auto ScaleEnemyForFloor = [&](Enemy& enemy) {
        int floor = dungeon.CurrentFloor();
        float floorOffset = (float)(floor - 1);

        enemy.hp = (int)(enemy.hp * (1.0f + 0.25f * floorOffset));
        if (enemy.hp < 1) enemy.hp = 1;
        enemy.maxHp = enemy.hp;
        enemy.speed *= 1.0f + 0.08f * floorOffset;
        enemy.shootCooldown *= 1.0f - 0.06f * floorOffset;
        if (enemy.shootCooldown < 0.50f) enemy.shootCooldown = 0.50f;
        enemy.shootRange *= 1.0f + 0.04f * floorOffset;
        enemy.preferredDistance *= 1.0f + 0.02f * floorOffset;
        enemy.shotSpeed *= 1.0f + 0.04f * floorOffset;
        enemy.spawnDelayRemaining = 0.5f;
    };

    auto ScaleBossForFloor = [&](Boss& boss) {
        int floor = dungeon.CurrentFloor();
        float floorOffset = (float)(floor - 1);

        boss.hp = (int)(boss.hp * (1.0f + 0.35f * floorOffset));
        if (boss.hp < 1) boss.hp = 1;
        boss.maxHp = boss.hp;
        boss.driftSpeed *= 1.0f + 0.06f * floorOffset;
        boss.attackCooldownPhase1 *= 1.0f - 0.05f * floorOffset;
        boss.attackCooldownPhase2 *= 1.0f - 0.05f * floorOffset;
        if (boss.attackCooldownPhase1 < 0.85f) boss.attackCooldownPhase1 = 0.85f;
        if (boss.attackCooldownPhase2 < 0.60f) boss.attackCooldownPhase2 = 0.60f;
        boss.chargeSpeed *= 1.0f + 0.06f * floorOffset;
        boss.spawnDelayRemaining = 0.5f;
        boss.attackTimer = boss.attackCooldownPhase1;
    };

    auto MakeSpecialEnemy = [&](Enemy& enemy) {
        if (!RNG::Chance(dungeon.SpecialEnemyChance())) return;

        enemy.specialType = (EnemySpecialType)RNG::Range(1, 3);
        enemy.creepDropTimer = 0.0f;

        switch (enemy.specialType) {
            case EnemySpecialType::REINFORCER:
                enemy.hp = (int)(enemy.hp * 1.45f) + 6;
                enemy.speed *= 1.06f;
                enemy.shootCooldown *= 0.92f;
                enemy.shootRange *= 1.05f;
                break;
            case EnemySpecialType::CREEPER:
                enemy.hp = (int)(enemy.hp * 1.20f) + 3;
                enemy.speed *= 1.10f;
                enemy.shootCooldown *= 0.94f;
                enemy.shootRange *= 1.08f;
                break;
            case EnemySpecialType::DEATH_RING:
                enemy.hp = (int)(enemy.hp * 1.30f) + 5;
                enemy.speed *= 1.04f;
                enemy.shootCooldown *= 0.90f;
                enemy.shotSpeed *= 1.05f;
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
            SpawnItemPickups(dungeon.CurrentRoom());
            dungeon.CurrentRoom().lootGranted = true;
        }
    };

    auto TryCollectCurrentRoomPickups = [&]() -> bool {
        Room& room = dungeon.CurrentRoom();
        const float pickupSize = 8.0f;

        for (auto& pickup : room.pickups) {
            if (pickup.collected) continue;
            Rect pickupRect = { pickup.pos.x, pickup.pos.y, pickupSize, pickupSize };
            if (!Collision::CheckAABB(player.GetRect(), pickupRect)) continue;

            switch (pickup.type) {
                case RoomPickupType::ITEM:
                    if (pickup.itemId >= 0) {
                        ItemSystem::GrantItem(player, pickup.itemId);
                        player.actionFlashTimer = std::max(player.actionFlashTimer, 0.08f);
                        AddScreenShake(0.06f, 0.8f);
                    }
                    pickup.collected = true;
                    break;

                case RoomPickupType::EXIT:
                    if (room.type == RoomType::BOSS && room.cleared) {
                        pickup.collected = true;
                        enemies.clear();
                        enemyShots.clear();
                        playerShots.clear();

                        uint32_t nextSeed = (uint32_t)GetTickCount() + 97u * (uint32_t)(dungeon.CurrentFloor() + 1);
                        if (dungeon.AdvanceFloor(nextSeed)) {
                            dungeon.PlacePlayerAtCurrentRoomCenter(player);
                            LoadRoomEncounter();
                        } else {
                            state = GameState::TITLE;
                        }
                        return true;
                    }
                    break;

                case RoomPickupType::TROPHY:
                    pickup.collected = true;
                    state = GameState::TITLE;
                    enemies.clear();
                    enemyShots.clear();
                    playerShots.clear();
                    boss = Boss{};
                    boss.alive = false;
                    return true;
            }
        }

        return false;
    };

    while (Window::PollEvents()) {
        float dt = timer.Tick();
        Input::Update();

        Room* activeRoom = nullptr;

        if (state == GameState::TITLE) {
            if (Input::IsPressed(VK_RETURN) || Input::IsPressed(VK_SPACE)) {
                StartRun();
                LoadRoomEncounter();
            }
        } else if (state == GameState::PLAYING) {
            activeRoom = &dungeon.CurrentRoom();
            Room* room = activeRoom;

            PlayerLogic::UpdateTimers(player, dt);
            ProjectileSystem::Advance(playerShots, dt, &enemies, (room->type == RoomType::BOSS && boss.alive) ? &boss : nullptr);
            ProjectileSystem::Advance(enemyShots, dt);
            PlayerLogic::HandleMovement(player, dt);

            if (screenShakeTimer > 0.0f) {
                screenShakeTimer -= dt;
                if (screenShakeTimer < 0.0f) screenShakeTimer = 0.0f;
            }

            if (dungeon.TryTransition(player)) {
                LoadRoomEncounter();
            }

            activeRoom = &dungeon.CurrentRoom();
            room = activeRoom;
            player.pos = room->ClampToRoom(player.pos, (float)player.size, (float)player.size);
            PlayerLogic::HandleShooting(player, dt, playerShots);
            bool roomTransitioned = TryCollectCurrentRoomPickups();

            if (!roomTransitioned) {
                if (room->type == RoomType::NORMAL && !room->cleared) {
                    bool anyAlive = false;
                    std::vector<Enemy> spawnedEnemies;
                    for (auto& enemy : enemies) {
                        EnemyAI::Update(enemy, player.pos, dt, enemies, spawnedEnemies, enemyShots);
                        enemy.pos = room->ClampToRoom(enemy.pos, enemy.w, enemy.h);

                        ProjectileSystem::UpdateAndCollideVsEnemy(playerShots, enemies, enemy, projectileSize,
                                                                  spawnedEnemies, enemyShots, dt);

                        if (enemy.alive && Collision::CheckAABB(player.GetRect(), enemy.GetRect())) {
                            DamagePlayer(contactDamage);
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

                    if (ProjectileSystem::UpdateAndCollideVsPlayer(enemyShots, player, projectileSize,
                                                                   invincibleDuration, dungeon.CurrentFloor(), dt)) {
                        AddScreenShake(0.14f, 2.0f);
                    }

                    if (!anyAlive) {
                        dungeon.MarkCurrentRoomCleared();
                        enemies.clear();
                        enemyShots.clear();
                        playerShots.clear();
                    }
                } else if (room->type == RoomType::BOSS) {
                    BossAI::Update(boss, player.pos, dt, enemyShots);
                    ProjectileSystem::UpdateAndCollideVsBoss(playerShots, boss, projectileSize, dt);
                    if (ProjectileSystem::UpdateAndCollideVsPlayer(enemyShots, player, projectileSize,
                                                                   invincibleDuration, dungeon.CurrentFloor(), dt)) {
                        AddScreenShake(0.14f, 2.0f);
                    }

                    if (boss.alive && Collision::CheckAABB(player.GetRect(), boss.GetRect())) {
                        int dmg = boss.isCharging ? boss.chargeContactDamage : boss.contactDamage;
                        DamagePlayer(dmg);
                    }

                    if (!boss.alive) {
                        dungeon.MarkCurrentRoomCleared();
                        enemyShots.clear();
                        playerShots.clear();
                        AddScreenShake(0.22f, 2.6f);
                        if (!room->lootGranted) {
                            SpawnBossRewards(*room);
                        }
                    }
                }

                if (player.hp <= 0) {
                    state = GameState::GAME_OVER;
                }
            }
        } else if (state == GameState::GAME_OVER) {
            if (Input::IsPressed('R')) {
                StartRun();
                LoadRoomEncounter();
            }
        }

        const Room* roomPtr = activeRoom;
        Vec2 shakeOffset = { 0.0f, 0.0f };
        if (screenShakeTimer > 0.0f) {
            float shakeScale = screenShakeTimer / 0.14f;
            if (shakeScale < 0.0f) shakeScale = 0.0f;
            if (shakeScale > 1.0f) shakeScale = 1.0f;
            shakeOffset.x = std::sin(screenShakeTimer * 97.0f) * screenShakeStrength * shakeScale;
            shakeOffset.y = std::cos(screenShakeTimer * 131.0f) * screenShakeStrength * shakeScale;
        }

        Renderer::Clear(0xFF1A1A1A);

        if (state == GameState::TITLE) {
            HUD::DrawTitleScreen();
            Renderer::Present();
            continue;
        }

        uint32_t playerColor = player.IsInvincible() ? 0xFFFF8888 : 0xFF00FF88;
        if (player.actionFlashTimer > 0.0f) {
            Renderer::DrawRect((int)(player.pos.x + shakeOffset.x) - 1, (int)(player.pos.y + shakeOffset.y) - 1,
                               player.size + 2, player.size + 2, 0xFFFFFFAA);
        }
        Renderer::DrawRect((int)(player.pos.x + shakeOffset.x), (int)(player.pos.y + shakeOffset.y),
                           player.size, player.size, playerColor);

        if (roomPtr) {
            for (const auto& pickup : roomPtr->pickups) {
                if (pickup.collected) continue;

                uint32_t color = 0xFFFFC84D;
                int size = 8;
                if (pickup.type == RoomPickupType::EXIT) {
                    color = 0xFF776655;
                    size = 10;
                } else if (pickup.type == RoomPickupType::TROPHY) {
                    color = 0xFFFFFF99;
                    size = 12;
                }

                Renderer::DrawRect((int)(pickup.pos.x + shakeOffset.x), (int)(pickup.pos.y + shakeOffset.y),
                                   size, size, color);
                if (pickup.type == RoomPickupType::EXIT) {
                    Renderer::DrawRect((int)(pickup.pos.x + shakeOffset.x) + 2, (int)(pickup.pos.y + shakeOffset.y) + 2,
                                       size - 4, size - 4, 0xFF332211);
                }
            }
        }

        if (roomPtr && roomPtr->type == RoomType::NORMAL) {
            for (auto& enemy : enemies) {
                if (!enemy.alive) continue;

                uint32_t color;
                switch (enemy.aiType) {
                    case AIType::SHOOTER:  color = 0xFFFF9933; break;
                    case AIType::CHARGER:  color = 0xFFFFCC33; break;
                    case AIType::SUMMONER: color = 0xFFCC66FF; break;
                    case AIType::EXPLODER: color = 0xFF33DD66; break;
                    case AIType::CHASER:
                    default:                color = 0xFFFF3333; break;
                }

                // Nudge the hue by attack pattern so, e.g., a triple-shot
                // Gunner and a spiral-shot Gunner read as different enemies.
                if (enemy.attackPattern == AttackPattern::TRIPLE) color ^= 0x00202000;
                if (enemy.attackPattern == AttackPattern::RADIAL) color ^= 0x00002020;
                if (enemy.attackPattern == AttackPattern::SPIRAL) color ^= 0x00200020;

                if (enemy.specialType == EnemySpecialType::REINFORCER) color = 0xFF66DDFF;
                if (enemy.specialType == EnemySpecialType::CREEPER) color = 0xFF55FF55;
                if (enemy.specialType == EnemySpecialType::DEATH_RING) color = 0xFFFFFF66;

                if (enemy.shielded) {
                    Renderer::DrawRect((int)(enemy.pos.x + shakeOffset.x) - 1, (int)(enemy.pos.y + shakeOffset.y) - 1,
                                       (int)enemy.w + 2, (int)enemy.h + 2, 0xFFAADDFF);
                }
                Renderer::DrawRect((int)(enemy.pos.x + shakeOffset.x), (int)(enemy.pos.y + shakeOffset.y),
                                   (int)enemy.w, (int)enemy.h, color);
            }
        }

        if (roomPtr && roomPtr->type == RoomType::BOSS && boss.alive) {
            uint32_t baseColor = 0xFFAA33FF;
            if (boss.variant == 1) baseColor = 0xFFFF9933;
            if (boss.variant == 2) baseColor = 0xFF33FFCC;
            uint32_t bossColor = boss.isCharging ? 0xFFFF3399 : baseColor;
            Renderer::DrawRect((int)(boss.pos.x + shakeOffset.x), (int)(boss.pos.y + shakeOffset.y),
                               (int)boss.w, (int)boss.h, bossColor);
            HUD::DrawBossHealthBar(boss);
        }

        ProjectileSystem::Draw(playerShots, (int)projectileSize, 0xFFFFFF00, shakeOffset);
        ProjectileSystem::Draw(enemyShots, (int)projectileSize, 0xFFFF66FF, shakeOffset);

        HUD::DrawHealthBar(player);
        if (roomPtr) {
            HUD::DrawRunStatus(dungeon, player, *roomPtr, (roomPtr->type == RoomType::BOSS && boss.alive) ? &boss : nullptr);
        }
        if (state == GameState::GAME_OVER) HUD::DrawGameOverBanner();
        if (roomPtr && roomPtr->type == RoomType::NORMAL && roomPtr->cleared) {
            HUD::DrawRoomClearedBanner();
        }
        HUD::DrawFloorMap(dungeon);

        Renderer::Present();
    }

    Renderer::Shutdown();
    Window::Destroy();
    return 0;
}
