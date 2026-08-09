#include "hud.h"
#include "../../engine/renderer.h"
#include "../../engine/text.h"

namespace HUD {

void DrawHealthBar(const Player& player) {
    int barWidth = 60;
    int fillWidth = (int)(barWidth * (player.hp / (float)player.maxHp));
    Renderer::DrawRect(5, 5, barWidth, 6, 0xFF444444);
    Renderer::DrawRect(5, 5, fillWidth, 6, 0xFF33FF33);
    Text::DrawString("HP", 5, 14, 0xFFCCCCCC, 1);
}

void DrawGameOverBanner() {
    const char* msg = "GAME OVER - PRESS R";
    int w = Text::MeasureWidth(msg, 1);
    Text::DrawString(msg, (320 - w) / 2, 85, 0xFFFF4444, 1);
}

void DrawRoomClearedBanner() {
    const char* msg = "ROOM CLEARED";
    int w = Text::MeasureWidth(msg, 1);
    Text::DrawString(msg, (320 - w) / 2, 85, 0xFF44FF88, 1);
}

void DrawBossHealthBar(const Boss& boss) {
    int barWidth = 200;
    int fillWidth = (int)(barWidth * (boss.hp / (float)boss.maxHp));
    int barX = (320 - barWidth) / 2;
    Renderer::DrawRect(barX, 8, barWidth, 5, 0xFF444444);
    Renderer::DrawRect(barX, 8, fillWidth, 5, 0xFFAA33FF);
}

} // namespace HUD