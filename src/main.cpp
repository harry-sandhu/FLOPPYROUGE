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
#include "../game/rooms/room.h"
#include "../game/ui/hud.h"
#include "../engine/collision.h"

enum class GameState { PLAYING, WON, LOST };

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
        playerShots.clear();
        enemyShots.clear();
        state = GameState::PLAYING;
    };

    while (Window::PollEvents()) {
        float dt = timer.Tick();
        Input::Update();

        if (state == GameState::PLAYING) {
            PlayerLogic::HandleMovement(player, dt);
            PlayerLogic::UpdateTimers(player, dt);
            player.pos = room.ClampToRoom(player.pos, (float)player.size, (float)player.size);

            if (Input::IsPressed(VK_SPACE)) {
                ProjectileSystem::Spawn(playerShots,
                    { player.pos.x + player.size / 2.0f, player.pos.y },
                    { 0.0f, -projectileSpeed });
            }

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
                state = GameState::WON;
            }
        } else {
            if (Input::IsPressed('R')) {
                ResetGame();
            }
        }

        Renderer::Clear(0xFF1A1A1A);

        uint32_t playerColor = player.IsInvincible() ? 0xFFFF8888 : 0xFF00FF88;
        Renderer::DrawRect((int)player.pos.x, (int)player.pos.y, player.size, player.size, playerColor);

        for (auto& enemy : enemies) {
            if (!enemy.alive) continue;
            uint32_t color = (enemy.aiType == AIType::SHOOTER) ? 0xFFFF9933 : 0xFFFF3333;
            Renderer::DrawRect((int)enemy.pos.x, (int)enemy.pos.y, (int)enemy.w, (int)enemy.h, color);
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