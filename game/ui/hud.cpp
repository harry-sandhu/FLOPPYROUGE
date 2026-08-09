#include "hud.h"
#include "../../engine/renderer.h"
#include "../../engine/text.h"
#include "../dungeon/dungeon.h"
#include <cstdio>
#include <climits>

namespace HUD {

namespace {
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
    int barWidth = 60;
    int fillWidth = (int)(barWidth * (player.hp / (float)player.maxHp));
    Renderer::DrawRect(5, 5, barWidth, 6, 0xFF444444);
    Renderer::DrawRect(5, 5, fillWidth, 6, 0xFF33FF33);
    Text::DrawString("HP", 5, 14, 0xFFCCCCCC, 1);
}

void DrawRunStatus(const Dungeon& dungeon, const Player& player, const Room& room, const Boss* boss) {
    char line[64];

    std::snprintf(line, sizeof(line), "DMG %d FIRE %.1f SPD %d RNG %.0f",
                  player.damage, player.fireRate, (int)player.moveSpeed, player.range);
    Text::DrawString(line, 5, 24, 0xFFEAEAEA, 1);

    std::snprintf(line, sizeof(line), "FLOOR %d/%d", dungeon.CurrentFloor(), dungeon.MaxFloors());
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

    if (player.pickupMessageTimer > 0.0f && player.pickupName[0] != '\0') {
        std::snprintf(line, sizeof(line), "PICKUP %s", player.pickupName);
        Text::DrawString(line, 5, 69, 0xFFFFFF88, 1);
    }

    if (boss && boss->alive) {
        std::snprintf(line, sizeof(line), "BOSS PHASE %d%s", boss->phase, boss->isCharging ? " CHARGE" : "");
        int w = Text::MeasureWidth(line, 1);
        Text::DrawString(line, (320 - w) / 2, 16, 0xFFFFFFFF, 1);
    }
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
