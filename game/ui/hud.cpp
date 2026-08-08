#include "hud.h"
#include "../../engine/renderer.h"

namespace HUD {

void DrawHealthBar(const Player& player) {
    int barWidth = 60;
    int fillWidth = (int)(barWidth * (player.hp / (float)player.maxHp));
    Renderer::DrawRect(5, 5, barWidth, 6, 0xFF444444);
    Renderer::DrawRect(5, 5, fillWidth, 6, 0xFF33FF33);
}

void DrawGameOverBanner() {
    Renderer::DrawRect(0, 80, 320, 20, 0xFF660000);
}

} // namespace HUD