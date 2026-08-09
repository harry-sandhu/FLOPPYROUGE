#include <windows.h>
#include <vector>
#include "../engine/window.h"
#include "../engine/renderer.h"
#include "../engine/input.h"
#include "../engine/core/timer.h"
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

Boss SpawnBoss() {
    Boss boss;
    boss.pos = { 146.0f, 20.0f };
    return boss;
}

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

        if (room.type == RoomType::NORMAL) {
            if (!room.enemySpawnList.empty()) {
                for (int i = 0; i < (int)room.enemySpawnList.size(); ++i) {
                    Vec2 spawnPos = spawnPoints[i % (int)(sizeof(spawnPoints) / sizeof(spawnPoints[0]))];
                    enemies.push_back(EnemyDatabase::Spawn(room.enemySpawnList[i], spawnPos));
                }
            } else {
                enemies.push_back(EnemyDatabase::Spawn("Zombie", { 100.0f, 30.0f }));
                enemies.push_back(EnemyDatabase::Spawn("Gunner", { 220.0f, 30.0f }));
            }
        } else if (room.type == RoomType::BOSS) {
            boss = SpawnBossVariant(room.bossVariant);
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
            PlayerLogic::HandleMovement(player, dt);

            if (dungeon.TryTransition(player)) {
                LoadRoomEncounter();
            }

            const Room& room = dungeon.CurrentRoom();
            player.pos = room.ClampToRoom(player.pos, (float)player.size, (float)player.size);
            PlayerLogic::HandleShooting(player, dt, playerShots);

            if (room.type == RoomType::NORMAL) {
                bool anyAlive = false;
                std::vector<Enemy> spawnedEnemies;
                for (auto& enemy : enemies) {
                    EnemyAI::Update(enemy, player.pos, dt, enemies, spawnedEnemies, enemyShots);
                    enemy.pos = room.ClampToRoom(enemy.pos, enemy.w, enemy.h);

                    ProjectileSystem::UpdateAndCollideVsEnemy(playerShots, enemy, projectileSize, dt);

                    if (enemy.alive && Collision::CheckAABB(player.GetRect(), enemy.GetRect())) {
                        PlayerLogic::TakeDamage(player, contactDamage, invincibleDuration);
                    }
                    if (enemy.alive) anyAlive = true;
                }

                if (!spawnedEnemies.empty()) {
                    enemies.insert(enemies.end(), spawnedEnemies.begin(), spawnedEnemies.end());
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
                    state = GameState::WON;
                }
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

        Renderer::Present();
    }

    Renderer::Shutdown();
    Window::Destroy();
    return 0;
}
