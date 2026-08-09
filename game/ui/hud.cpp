#include "hud.h"
#include "../../engine/renderer.h"
#include "../../engine/text.h"
#include "../dungeon/dungeon.h"
#include <cstdio>
#include <climits>

namespace HUD {

namespace {
    void DrawHeart(int x, int y, int filledState) {
        const uint32_t dark = 0xFF331122;
        const uint32_t bright = 0xFFFF4D77;
        const uint32_t mid = 0xFFBB3355;

        uint32_t color = dark;
        if (filledState >= 2) color = bright;
        else if (filledState == 1) color = mid;

        Renderer::DrawRect(x + 1, y, 2, 2, color);
        Renderer::DrawRect(x + 4, y, 2, 2, color);
        Renderer::DrawRect(x, y + 2, 7, 3, color);
        Renderer::DrawRect(x + 1, y + 1, 1, 1, 0xFFFFFFFF);
    }

    uint32_t RoomColor(const Room& room, bool isCurrent) {
        uint32_t color = 0xFF444444;
        switch (room.type) {
            case RoomType::START:    color = 0xFF33CC66; break;
            case RoomType::NORMAL:   color = room.cleared ? 0xFF55AA55 : 0xFF444444; break;
            case RoomType::BOSS:     color = 0xFFBB55FF; break;
            case RoomType::TREASURE: color = 0xFFFFC84D; break;
            case RoomType::CURSE:    color = 0xFFFF5555; break;
        }

        if (room.cleared && room.type == RoomType::NORMAL) {
            color = 0xFF77CC77;
        }

        if (isCurrent) {
            color = 0xFFFFFFFF;
        }

        return color;
    }

    void DrawOutline(int x, int y, int w, int h, uint32_t color) {
        Renderer::DrawRect(x, y, w, 1, color);
        Renderer::DrawRect(x, y + h - 1, w, 1, color);
        Renderer::DrawRect(x, y, 1, h, color);
        Renderer::DrawRect(x + w - 1, y, 1, h, color);
    }

    const char* RoomTypeName(RoomType type) {
        switch (type) {
            case RoomType::START:    return "START";
            case RoomType::NORMAL:   return "NORMAL";
            case RoomType::BOSS:     return "BOSS";
            case RoomType::TREASURE: return "TREASURE";
            case RoomType::CURSE:    return "CURSE";
        }
        return "?";
    }
}

void DrawHealthBar(const Player& player) {
    Text::DrawString("HP", 5, 5, 0xFFCCCCCC, 1);
    int hearts = std::max(1, player.maxHp / 2);
    for (int i = 0; i < hearts; ++i) {
        int x = 18 + i * 9;
        int hpLeft = player.hp - i * 2;
        int state = 0;
        if (hpLeft >= 2) state = 2;
        else if (hpLeft == 1) state = 1;
        DrawHeart(x, 4, state);
    }
}

void DrawRunStatus(const Dungeon& dungeon, const Player& player, const Room& room, const Boss* boss) {
    char line[64];

    std::snprintf(line, sizeof(line), "DMG %d FIRE %.1f SPD %d RNG %.0f",
                  player.damage, player.fireRate, (int)player.moveSpeed, player.range);
    Text::DrawString(line, 5, 24, 0xFFEAEAEA, 1);

    std::snprintf(line, sizeof(line), "LCK %d FLOOR %d/%d", player.luck, dungeon.CurrentFloor(), dungeon.MaxFloors());
    Text::DrawString(line, 5, 33, 0xFFEAEAEA, 1);

    std::snprintf(line, sizeof(line), "ROOM %s", RoomTypeName(room.type));
    Text::DrawString(line, 5, 42, 0xFFEAEAEA, 1);

    if (player.hasDash) {
        if (player.isDashing) {
            std::snprintf(line, sizeof(line), "DASH ACTIVE");
        } else if (player.dashCooldownRemaining > 0.0f) {
            std::snprintf(line, sizeof(line), "DASH CD %.1f", player.dashCooldownRemaining);
        } else {
            std::snprintf(line, sizeof(line), "DASH READY");
        }
    } else {
        std::snprintf(line, sizeof(line), "DASH LOCKED");
    }
    Text::DrawString(line, 5, 51, 0xFFEAEAEA, 1);

    std::snprintf(line, sizeof(line), "ITEMS %d", player.ownedItemCount);
    Text::DrawString(line, 5, 60, 0xFFEAEAEA, 1);

    if (player.hasHomingShots || player.poisonChance > 0.0f || player.stickyChance > 0.0f ||
        player.piercingChance > 0.0f || player.explosiveChance > 0.0f) {
        char fx[48] = {};
        std::snprintf(fx, sizeof(fx), "FX%s%s%s%s%s",
                      player.hasHomingShots ? " HOM" : "",
                      player.poisonChance > 0.0f ? " P" : "",
                      player.stickyChance > 0.0f ? " S" : "",
                      player.piercingChance > 0.0f ? " PI" : "",
                      player.explosiveChance > 0.0f ? " EX" : "");
        Text::DrawString(fx, 5, 69, 0xFFBBBBFF, 1);
    }

    if (player.pickupMessageTimer > 0.0f && player.pickupName[0] != '\0') {
        std::snprintf(line, sizeof(line), "PICKUP %s", player.pickupName);
        Text::DrawString(line, 5, 78, 0xFFFFFF88, 1);
    }

    if (boss && boss->alive) {
        std::snprintf(line, sizeof(line), "BOSS PHASE %d%s", boss->phase, boss->isCharging ? " CHARGE" : "");
        int w = Text::MeasureWidth(line, 1);
        Text::DrawString(line, (320 - w) / 2, 16, 0xFFFFFFFF, 1);
    }
}

void DrawItemPreview(const char* name, const char* desc) {
    int nameW = Text::MeasureWidth(name, 1);
    int descW = Text::MeasureWidth(desc, 1);
    int boxW = std::max(nameW, descW) + 12;
    if (boxW > 300) boxW = 300;
    int boxX = (320 - boxW) / 2;
    int boxY = 146;

    Renderer::DrawRect(boxX, boxY, boxW, 20, 0xFF141118);
    Renderer::DrawRect(boxX, boxY, boxW, 1, 0xFF555566);
    Renderer::DrawRect(boxX, boxY + 19, boxW, 1, 0xFF555566);
    Renderer::DrawRect(boxX, boxY, 1, 20, 0xFF555566);
    Renderer::DrawRect(boxX + boxW - 1, boxY, 1, 20, 0xFF555566);

    Text::DrawString(name, (320 - nameW) / 2, boxY + 3, 0xFFFFC84D, 1);
    Text::DrawString(desc, (320 - descW) / 2, boxY + 12, 0xFFCCCCCC, 1);
}

void DrawTitleScreen() {
    Renderer::DrawRect(0, 0, 320, 180, 0xFF0F0D12);
    Renderer::DrawRect(0, 0, 320, 18, 0xFF221933);
    Renderer::DrawRect(0, 162, 320, 18, 0xFF221933);

    const char* title = "FloppyRogue";
    int titleW = Text::MeasureWidth(title, 3);
    Text::DrawString(title, (320 - titleW) / 2, 34, 0xFFFFD16A, 3);

    const char* subtitle = "Top-down roguelite";
    int subtitleW = Text::MeasureWidth(subtitle, 1);
    Text::DrawString(subtitle, (320 - subtitleW) / 2, 56, 0xFFFFFFFF, 1);

    Text::DrawString("ENTER  START RUN", 92, 82, 0xFFEAEAEA, 1);
    Text::DrawString("WASD   MOVE", 92, 94, 0xFFEAEAEA, 1);
    Text::DrawString("ARROWS SHOOT", 92, 106, 0xFFEAEAEA, 1);
    Text::DrawString("SPACE  DASH", 92, 118, 0xFFEAEAEA, 1);
    Text::DrawString("PICK UP ITEMS IN ROOMS", 62, 136, 0xFFFFC84D, 1);
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

void DrawFloorMap(const Dungeon& dungeon) {
    const std::vector<Room>& rooms = dungeon.Rooms();
    if (rooms.empty()) return;

    int minGridX = INT_MAX;
    int minGridY = INT_MAX;
    int maxGridX = INT_MIN;
    int maxGridY = INT_MIN;

    for (const Room& room : rooms) {
        int gx = (int)room.gridPos.x;
        int gy = (int)room.gridPos.y;
        if (gx < minGridX) minGridX = gx;
        if (gy < minGridY) minGridY = gy;
        if (gx > maxGridX) maxGridX = gx;
        if (gy > maxGridY) maxGridY = gy;
    }

    const int cellSize = 5;
    const int gap = 2;
    const int cols = maxGridX - minGridX + 1;
    const int rows = maxGridY - minGridY + 1;
    const int mapW = cols * cellSize + (cols - 1) * gap;
    const int mapH = rows * cellSize + (rows - 1) * gap;
    const int mapX = 320 - mapW - 6;
    const int mapY = 6;

    Renderer::DrawRect(mapX - 3, mapY - 3, mapW + 6, mapH + 6, 0xFF111111);
    Renderer::DrawRect(mapX - 2, mapY - 2, mapW + 4, 1, 0xFF444444);
    Renderer::DrawRect(mapX - 2, mapY + mapH + 1, mapW + 4, 1, 0xFF444444);
    Renderer::DrawRect(mapX - 2, mapY - 2, 1, mapH + 4, 0xFF444444);
    Renderer::DrawRect(mapX + mapW + 1, mapY - 2, 1, mapH + 4, 0xFF444444);

    auto CellOrigin = [&](const Room& room) {
        int gx = (int)room.gridPos.x - minGridX;
        int gy = (int)room.gridPos.y - minGridY;
        return Vec2{ (float)(mapX + gx * (cellSize + gap)), (float)(mapY + gy * (cellSize + gap)) };
    };

    for (const Room& room : rooms) {
        Vec2 origin = CellOrigin(room);
        int x = (int)origin.x;
        int y = (int)origin.y;

        if (room.east >= 0 && room.east < (int)rooms.size()) {
            Renderer::DrawRect(x + cellSize, y + 2, gap, 1, 0xFF666666);
        }
        if (room.south >= 0 && room.south < (int)rooms.size()) {
            Renderer::DrawRect(x + 2, y + cellSize, 1, gap, 0xFF666666);
        }
    }

    for (int i = 0; i < (int)rooms.size(); ++i) {
        const Room& room = rooms[i];
        Vec2 origin = CellOrigin(room);
        int x = (int)origin.x;
        int y = (int)origin.y;
        uint32_t color = RoomColor(room, i == dungeon.CurrentRoomIndex());
        Renderer::DrawRect(x, y, cellSize, cellSize, color);
        if (i == dungeon.CurrentRoomIndex()) {
            DrawOutline(x - 1, y - 1, cellSize + 2, cellSize + 2, 0xFFFFFFFF);
        }
    }

    const int legendX = mapX - 1;
    const int legendY = mapY + mapH + 6;
    Text::DrawString("FLOOR MAP", legendX, legendY, 0xFFFFFFFF, 1);

    struct LegendEntry {
        const char* label;
        uint32_t color;
    };

    const LegendEntry entries[] = {
        { "S Start", 0xFF33CC66 },
        { "N Normal", 0xFF77CC77 },
        { "B Boss", 0xFFBB55FF },
        { "T Treasure", 0xFFFFC84D },
        { "C Curse", 0xFFFF5555 }
    };

    int lineY = legendY + 9;
    for (const LegendEntry& entry : entries) {
        Renderer::DrawRect(legendX, lineY + 1, 4, 4, entry.color);
        Text::DrawString(entry.label, legendX + 7, lineY, 0xFFEAEAEA, 1);
        lineY += 8;
    }
}

} // namespace HUD
