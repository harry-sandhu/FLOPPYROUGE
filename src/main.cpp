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

    struct Bomb {
        Vec2 pos = { 0.0f, 0.0f };
        float fuseTimer = 3.0f;
        float flashTimer = 0.0f;
        bool exploded = false;
    };

    Player player;
    std::vector<Enemy> enemies;
    Boss boss;
    std::vector<Projectile> playerShots;
    std::vector<Projectile> enemyShots;
    std::vector<Bomb> bombs;
    float screenShakeTimer = 0.0f;
    float screenShakeStrength = 0.0f;

    const float invincibleDuration = 0.75f;
    const float projectileSize = 3.0f;
    const float bombFuseDuration = 3.0f;
    const float bombExplosionRadius = 34.0f;
    const int bombDamage = 40;

    // Contact damage in half-heart units: normal enemies poke for half a
    // heart, special-variant enemies (Reinforcer/Creeper/Death Ring) hit
    // for a full heart. Floor 3's existing DamagePlayer clamp bumps normal
    // hits up to a full heart automatically once these are this small.
    auto ContactDamageFor = [](const Enemy& enemy) -> int {
        return (enemy.specialType == EnemySpecialType::NONE) ? 1 : 2;
    };

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
        bombs.clear();
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

        if ((room.type == RoomType::NORMAL || room.IsEnemyCurseRoom()) && !room.cleared) {
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
       } else if ((room.type == RoomType::TREASURE || 
            (room.type == RoomType::CURSE && !room.IsEnemyCurseRoom())) 
           && !room.lootGranted) {
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

                case RoomPickupType::HEART:
                player.hp = std::min(player.hp + 2, player.maxHp); // full heart
                pickup.collected = true;
                player.actionFlashTimer = std::max(player.actionFlashTimer, 0.08f);
                AddScreenShake(0.06f, 0.8f);
                break;
            case RoomPickupType::BOMB:
                player.bombCount++;
                pickup.collected = true;
                player.actionFlashTimer = std::max(player.actionFlashTimer, 0.08f);
                AddScreenShake(0.06f, 0.8f);
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

            if (Input::IsPressed('B') && player.bombCount > 0) {
                player.bombCount--;
            
                Bomb bomb;
                bomb.pos = {
                    player.pos.x + player.size * 0.5f - 3.0f,
                    player.pos.y + player.size * 0.5f - 3.0f
                };
                bomb.fuseTimer = bombFuseDuration;
            
                bombs.push_back(bomb);
            }
            
            for (auto& bomb : bombs) {
                if (bomb.exploded) {
                    bomb.flashTimer -= dt;
                    continue;
                }
            
                bomb.fuseTimer -= dt;
            
                if (bomb.fuseTimer <= 0.0f) {
                    bomb.exploded = true;
                    bomb.flashTimer = 0.18f;
            
                    AddScreenShake(0.20f, 2.4f);
            
                    Vec2 bombCenter = {
                        bomb.pos.x + 3.0f,
                        bomb.pos.y + 3.0f
                    };
            
                    for (auto& enemy : enemies) {
                        if (!enemy.alive) continue;
            
                        Vec2 enemyCenter = {
                            enemy.pos.x + enemy.w * 0.5f,
                            enemy.pos.y + enemy.h * 0.5f
                        };
            
                        float dx = enemyCenter.x - bombCenter.x;
                        float dy = enemyCenter.y - bombCenter.y;
            
                        if (dx * dx + dy * dy <=
                            bombExplosionRadius * bombExplosionRadius) {
            
                            enemy.hp -= bombDamage;
            
                            if (enemy.hp <= 0)
                                enemy.alive = false;
                        }
                    }
            
                    if (room->type == RoomType::BOSS && boss.alive) {
                        Vec2 bossCenter = {
                            boss.pos.x + boss.w * 0.5f,
                            boss.pos.y + boss.h * 0.5f
                        };
            
                        float dx = bossCenter.x - bombCenter.x;
                        float dy = bossCenter.y - bombCenter.y;
            
                        if (dx * dx + dy * dy <=
                            bombExplosionRadius * bombExplosionRadius) {
            
                            boss.hp -= bombDamage;
            
                            if (boss.hp <= 0)
                                boss.alive = false;
                        }
                    }
                }
            }
            
            bombs.erase(
                std::remove_if(
                    bombs.begin(),
                    bombs.end(),
                    [](const Bomb& b) {
                        return b.exploded && b.flashTimer <= 0.0f;
                    }),
                bombs.end()
            );
            
            bool roomTransitioned = TryCollectCurrentRoomPickups();

            if (!roomTransitioned) {
                if ((room->type == RoomType::NORMAL || room->IsEnemyCurseRoom()) && !room->cleared) {
                    bool anyAlive = false;
                    std::vector<Enemy> spawnedEnemies;
                    for (auto& enemy : enemies) {
                        EnemyAI::Update(enemy, player.pos, dt, enemies, spawnedEnemies, enemyShots);
                        enemy.pos = room->ClampToRoom(enemy.pos, enemy.w, enemy.h);

                        ProjectileSystem::UpdateAndCollideVsEnemy(playerShots, enemies, enemy, projectileSize,
                                                                  spawnedEnemies, enemyShots, dt);

                        if (enemy.alive && Collision::CheckAABB(player.GetRect(), enemy.GetRect())) {
                            DamagePlayer(ContactDamageFor(enemy));
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
                        dungeon.MarkCurrentRoomCleared(true);
                        enemies.clear();
                        enemyShots.clear();
                        playerShots.clear();
                    }
                } else if (room->type == RoomType::BOSS) {
                    std::vector<Enemy> spawnedBossAdds;
                    BossAI::Update(boss, player.pos, dt, enemyShots, enemies, spawnedBossAdds);
                    if (!spawnedBossAdds.empty()) {
                        for (auto& add : spawnedBossAdds) {
                            ScaleEnemyForFloor(add);
                        }
                        enemies.insert(enemies.end(), spawnedBossAdds.begin(), spawnedBossAdds.end());
                    }

                    std::vector<Enemy> scratchSpawned; // boss-room adds never summon further adds
                    for (auto& add : enemies) {
                        EnemyAI::Update(add, player.pos, dt, enemies, scratchSpawned, enemyShots);
                        add.pos = room->ClampToRoom(add.pos, add.w, add.h);

                        ProjectileSystem::UpdateAndCollideVsEnemy(playerShots, enemies, add, projectileSize,
                                                                  scratchSpawned, enemyShots, dt);

                        if (add.alive && Collision::CheckAABB(player.GetRect(), add.GetRect())) {
                            DamagePlayer(ContactDamageFor(add));
                        }
                    }
                    enemies.erase(std::remove_if(enemies.begin(), enemies.end(),
                                                 [](const Enemy& e) { return !e.alive; }),
                                 enemies.end());

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
                        enemies.clear();
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
                } else if (pickup.type == RoomPickupType::HEART) {
                    color = 0xFFFF4D77;
                    size = 8;
                } else if (pickup.type == RoomPickupType::BOMB) {
                    color = 0xFF333333;
                    size = 9;
                }

                Renderer::DrawRect((int)(pickup.pos.x + shakeOffset.x), (int)(pickup.pos.y + shakeOffset.y),
                                   size, size, color);
                if (pickup.type == RoomPickupType::EXIT) {
                    Renderer::DrawRect((int)(pickup.pos.x + shakeOffset.x) + 2, (int)(pickup.pos.y + shakeOffset.y) + 2,
                                       size - 4, size - 4, 0xFF332211);
                }
            }
        }

                for (const auto& bomb : bombs) {
            if (bomb.exploded) {
                if (bomb.flashTimer > 0.0f) {
                    int flashSize =
                        (int)(bombExplosionRadius * 2.0f *
                              (bomb.flashTimer / 0.18f));

                    Renderer::DrawRect(
                        (int)(bomb.pos.x + shakeOffset.x) -
                            flashSize / 2 + 3,
                        (int)(bomb.pos.y + shakeOffset.y) -
                            flashSize / 2 + 3,
                        flashSize,
                        flashSize,
                        0xFFFFCC66
                    );
                }

                continue;
            }

            float fusePct =
                1.0f - (bomb.fuseTimer / bombFuseDuration);

            uint32_t glow =
                fusePct > 0.66f
                    ? 0xFFFF3333
                    : (fusePct > 0.33f
                        ? 0xFFFFAA33
                        : 0xFF666666);

            Renderer::DrawRect(
                (int)(bomb.pos.x + shakeOffset.x),
                (int)(bomb.pos.y + shakeOffset.y),
                6,
                6,
                glow
            );
        }

       if (roomPtr &&
    (roomPtr->type == RoomType::NORMAL ||
     roomPtr->type == RoomType::BOSS ||
     roomPtr->IsEnemyCurseRoom())) {
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
        if (roomPtr &&
            (roomPtr->type == RoomType::NORMAL ||
             roomPtr->IsEnemyCurseRoom()) &&
            roomPtr->cleared) {
            HUD::DrawRoomClearedBanner();
        }
        HUD::DrawFloorMap(dungeon);

        Renderer::Present();
    }

    Renderer::Shutdown();
    Window::Destroy();
    return 0;
}
