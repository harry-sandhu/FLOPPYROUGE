#include <windows.h>
#include <vector>
#include "../engine/window.h"
#include "../engine/renderer.h"
#include "../engine/input.h"
#include "../engine/core/timer.h"
#include "../engine/text.h"
#include "../game/player/player.h"
#include "../game/player/projectile_system.h"
#include "../game/enemies/enemy.h"
#include "../game/enemies/enemy_database.h"
#include "../game/bosses/boss.h"
#include "../game/rooms/room.h"
#include "../game/ui/hud.h"
#include "../engine/collision.h"

enum class GameState { PLAYING, BOSS_FIGHT, WON, LOST };

Boss SpawnBoss() {
    Boss boss;
    boss.pos = { 146.0f, 20.0f };
    return boss;
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    if (!Window::Create(1280, 720, "FloppyRogue")) return 1;
    if (!Renderer::Init(Window::GetHandle())) return 1;

    EnemyDatabase::Load("data/enemies.txt");

    Timer timer;
    Room room;

    Player player;
    player.pos = { 150.0f, 130.0f };
    const int contactDamage = 10;
    const int enemyShotDamage = 8;
    const float invincibleDuration = 0.75f;

    GameState state = GameState::PLAYING;

    std::vector<Enemy> enemies;
    enemies.push_back(EnemyDatabase::Spawn("Zombie", { 100.0f, 30.0f }));
    enemies.push_back(EnemyDatabase::Spawn("Gunner", { 220.0f, 30.0f }));

    Boss boss;

    std::vector<Projectile> playerShots;
    std::vector<Projectile> enemyShots;
    const float projectileSpeed = 140.0f;
    const float projectileSize = 3.0f;

    auto ResetGame = [&]() {
        player = Player{};
        player.pos = { 150.0f, 130.0f };
        enemies.clear();
        enemies.push_back(EnemyDatabase::Spawn("Zombie", { 100.0f, 30.0f }));
        enemies.push_back(EnemyDatabase::Spawn("Gunner", { 220.0f, 30.0f }));
        boss = Boss{};
        boss.alive = false; // not spawned until PLAYING is cleared
        playerShots.clear();
        enemyShots.clear();
        state = GameState::PLAYING;
    };

    while (Window::PollEvents()) {
        float dt = timer.Tick();
        Input::Update();

        if (state == GameState::PLAYING || state == GameState::BOSS_FIGHT) {
            PlayerLogic::HandleMovement(player, dt);
            PlayerLogic::UpdateTimers(player, dt);
            player.pos = room.ClampToRoom(player.pos, (float)player.size, (float)player.size);

            int shootKeysPressed = 0;
Vec2 shootDir = { 0.0f, 0.0f };

if (Input::IsPressed(VK_UP))    { shootKeysPressed++; shootDir = { 0.0f, -1.0f }; }
if (Input::IsPressed(VK_DOWN))  { shootKeysPressed++; shootDir = { 0.0f,  1.0f }; }
if (Input::IsPressed(VK_LEFT))  { shootKeysPressed++; shootDir = { -1.0f, 0.0f }; }
if (Input::IsPressed(VK_RIGHT)) { shootKeysPressed++; shootDir = { 1.0f,  0.0f }; }

if (shootKeysPressed == 1) {
    Vec2 spawnPos = {
        player.pos.x + player.size / 2.0f,
        player.pos.y + player.size / 2.0f
    };
    ProjectileSystem::Spawn(playerShots, spawnPos,
        { shootDir.x * projectileSpeed, shootDir.y * projectileSpeed });
}
        }

        if (state == GameState::PLAYING) {
            bool anyAlive = false;
            for (auto& enemy : enemies) {
                EnemyAI::Update(enemy, player.pos, dt, enemyShots);
                enemy.pos = room.ClampToRoom(enemy.pos, enemy.w, enemy.h);

                ProjectileSystem::UpdateAndCollideVsEnemy(playerShots, enemy, projectileSize, dt);

                if (enemy.alive && Collision::CheckAABB(player.GetRect(), enemy.GetRect())) {
                    PlayerLogic::TakeDamage(player, contactDamage, invincibleDuration);
                }
                if (enemy.alive) anyAlive = true;
            }

            ProjectileSystem::UpdateAndCollideVsPlayer(enemyShots, player, projectileSize,
                                                        enemyShotDamage, invincibleDuration, dt);

            if (player.hp <= 0) {
                state = GameState::LOST;
            } else if (!anyAlive) {
                boss = SpawnBoss();
                enemyShots.clear();
                state = GameState::BOSS_FIGHT;
            }
        } else if (state == GameState::BOSS_FIGHT) {
            BossAI::Update(boss, player.pos, dt, enemyShots);
            ProjectileSystem::UpdateAndCollideVsBoss(playerShots, boss, projectileSize, dt);
            ProjectileSystem::UpdateAndCollideVsPlayer(enemyShots, player, projectileSize,
                                                        enemyShotDamage, invincibleDuration, dt);

            if (boss.alive && Collision::CheckAABB(player.GetRect(), boss.GetRect())) {
                int dmg = boss.isCharging ? boss.chargeContactDamage : boss.contactDamage;
                PlayerLogic::TakeDamage(player, dmg, invincibleDuration);
            }

            if (player.hp <= 0) {
                state = GameState::LOST;
            } else if (!boss.alive) {
                state = GameState::WON;
            }
        } else {
            if (Input::IsPressed('R')) ResetGame();
        }

        Renderer::Clear(0xFF1A1A1A);

        uint32_t playerColor = player.IsInvincible() ? 0xFFFF8888 : 0xFF00FF88;
        Renderer::DrawRect((int)player.pos.x, (int)player.pos.y, player.size, player.size, playerColor);

        if (state == GameState::PLAYING) {
            for (auto& enemy : enemies) {
                if (!enemy.alive) continue;
                uint32_t color = (enemy.aiType == AIType::SHOOTER) ? 0xFFFF9933 : 0xFFFF3333;
                Renderer::DrawRect((int)enemy.pos.x, (int)enemy.pos.y, (int)enemy.w, (int)enemy.h, color);
            }
        }

        if (state == GameState::BOSS_FIGHT && boss.alive) {
            uint32_t bossColor = boss.isCharging ? 0xFFFF3399 : 0xFFAA33FF;
            Renderer::DrawRect((int)boss.pos.x, (int)boss.pos.y, (int)boss.w, (int)boss.h, bossColor);
            HUD::DrawBossHealthBar(boss);
        }

        ProjectileSystem::Draw(playerShots, (int)projectileSize, 0xFFFFFF00);
        ProjectileSystem::Draw(enemyShots, (int)projectileSize, 0xFFFF66FF);

        HUD::DrawHealthBar(player);
        if (state == GameState::LOST) HUD::DrawGameOverBanner();
        if (state == GameState::WON) HUD::DrawRoomClearedBanner();

        Renderer::Present();
    }

    Renderer::Shutdown();
    Window::Destroy();
    return 0;
}