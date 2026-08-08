#include "player.h"
#include "../../engine/input.h"
#include <windows.h>

namespace PlayerLogic {

void HandleMovement(Player& player, float dt) {
    if (Input::IsDown('W')) player.pos.y -= player.speed * dt;
    if (Input::IsDown('S')) player.pos.y += player.speed * dt;
    if (Input::IsDown('A')) player.pos.x -= player.speed * dt;
    if (Input::IsDown('D')) player.pos.x += player.speed * dt;
}

void TakeDamage(Player& player, int amount, float invincibleDuration) {
    if (player.IsInvincible()) return;
    player.hp -= amount;
    if (player.hp < 0) player.hp = 0;
    player.invincibleTimer = invincibleDuration;
}

void UpdateTimers(Player& player, float dt) {
    if (player.invincibleTimer > 0.0f) player.invincibleTimer -= dt;
}

} // namespace PlayerLogic