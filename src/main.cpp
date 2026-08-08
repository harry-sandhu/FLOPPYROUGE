#include <windows.h>
#include <vector>
#include "../engine/window.h"
#include "../engine/renderer.h"
#include "../engine/input.h"
#include "../engine/core/timer.h"
#include "../game/player/player.h"
#include "../game/player/projectile_system.h"
#include "../game/enemies/enemy.h"
#include "../game/ui/hud.h"
#include "../engine/collision.h"

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    if (!Window::Create(1280, 720, "FloppyRogue")) return 1;
    if (!Renderer::Init(Window::GetHandle())) return 1;

    Timer timer;

    Player player;
    player.pos = { 150.0f, 130.0f };
    const int contactDamage = 10;
    const float invincibleDuration = 0.75f;
    bool gameOver = false;

    Enemy enemy;
    enemy.pos = { 150.0f, 30.0f };
    enemy.aiType = AIType::CHASER;

    std::vector<Projectile> projectiles;
    const float projectileSpeed = 140.0f;
    const float projectileSize = 3.0f;

    while (Window::PollEvents()) {
        float dt = timer.Tick();
        Input::Update();

        if (!gameOver) {
            PlayerLogic::HandleMovement(player, dt);
            PlayerLogic::UpdateTimers(player, dt);

            if (Input::IsPressed(VK_SPACE)) {
                ProjectileSystem::Spawn(projectiles,
                    { player.pos.x + player.size / 2.0f, player.pos.y },
                    { 0.0f, -projectileSpeed });
            }

            EnemyAI::Update(enemy, player.pos, dt);
            ProjectileSystem::UpdateAndCollide(projectiles, enemy, projectileSize, dt);

            if (enemy.alive && Collision::CheckAABB(player.GetRect(), enemy.GetRect())) {
                PlayerLogic::TakeDamage(player, contactDamage, invincibleDuration);
                if (player.hp <= 0) gameOver = true;
            }
        } else {
            if (Input::IsPressed('R')) {
                player = Player{};
                player.pos = { 150.0f, 130.0f };
                enemy = Enemy{};
                enemy.pos = { 150.0f, 30.0f };
                enemy.aiType = AIType::CHASER;
                projectiles.clear();
                gameOver = false;
            }
        }

        Renderer::Clear(0xFF1A1A1A);

        uint32_t playerColor = player.IsInvincible() ? 0xFFFF8888 : 0xFF00FF88;
        Renderer::DrawRect((int)player.pos.x, (int)player.pos.y, player.size, player.size, playerColor);

        if (enemy.alive) {
            Renderer::DrawRect((int)enemy.pos.x, (int)enemy.pos.y, (int)enemy.w, (int)enemy.h, 0xFFFF3333);
        }

        ProjectileSystem::Draw(projectiles, (int)projectileSize);
        HUD::DrawHealthBar(player);
        if (gameOver) HUD::DrawGameOverBanner();

        Renderer::Present();
    }

    Renderer::Shutdown();
    Window::Destroy();
    return 0;
}