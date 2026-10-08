#include "hud.h"
#include "../../engine/renderer.h"
#include "../../engine/text.h"
#include "../dungeon/dungeon.h"
#include "../items/item_database.h"
#include "../progression/meta_progression.h"
#include <cstdio>
#include <cstring>
#include <climits>
#include <queue>

namespace HUD {

namespace {
    void CenterText(const char* text, int y, uint32_t color, int scale = 1) {
        if (!text) return;
        int width = Text::MeasureWidth(text, scale);
        Text::DrawString(text, (320 - width) / 2, y, color, scale);
    }

    void DrawPickupIcon(int slot0, int x, int y, int size) {
        Renderer::DrawSprite(Renderer::MakeGridSprite(Renderer::kPickupSheetId, slot0, 8, 4),
                             x, y, size, size);
    }

    void DrawGameplayIcon(int slot1, int x, int y, int size) {
        int index = slot1 - 1;
        Renderer::DrawSprite(Renderer::MakeGridSprite(Renderer::kGameplaySheetId, index, 9, 4),
                             x, y, size, size);
    }

    int ThemeSheetId(DungeonTheme theme) {
        switch (theme) {
            case DungeonTheme::FORGE: return Renderer::kThemeForgeSheetId;
            case DungeonTheme::CRYPT: return Renderer::kThemeCryptSheetId;
            case DungeonTheme::FUNGAL: return Renderer::kThemeFungalSheetId;
            case DungeonTheme::DRACONIC: return Renderer::kThemeDraconicSheetId;
            case DungeonTheme::RUINS:
            default: return Renderer::kThemeRuinsSheetId;
        }
    }

    int RoomIconSlot(const Room& room) {
        switch (room.type) {
            case RoomType::START: return 15;
            case RoomType::BOSS: return 13;
            case RoomType::TREASURE: return 17;
            case RoomType::CURSE: return 20;
            case RoomType::SHOP: return 7;
            case RoomType::NORMAL:
            default: return room.cleared ? 2 : 1;
        }
    }

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
            case RoomType::SHOP:     color = 0xFF55DDEE; break;
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
            case RoomType::SHOP:     return "SHOP";
        }
        return "?";
    }

    void ShotModeText(const Player& player, char* out, size_t size) {
        out[0] = '\0';

        auto append = [&](const char* mode) {
            if (out[0] == '\0') {
                std::snprintf(out, size, "%s", mode);
                return;
            }
            size_t len = std::strlen(out);
            if (len + 1 >= size) return;
            std::snprintf(out + len, size - len, "+%s", mode);
        };

        if (player.hasRocketShots) append("ROCKET");
        if (player.hasBurstShots) append("BURST");
        if (player.hasLaserShots) append("LASER");
        if (player.hasCrimsonRay) append("RAY");
        if (player.hasBladeArc) append("BLADE");

        if (out[0] == '\0') {
            std::snprintf(out, size, "BULLET");
        }
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
        DrawPickupIcon(state == 1 ? 5 : state == 2 ? 6 : 5, x - 1, 2, 9);
    }
}

void DrawRunStatus(const Dungeon& dungeon, const Player& player, const Room& room, const Boss* boss) {
    char line[64];
    char shotLine[64];
    ShotModeText(player, shotLine, sizeof(shotLine));

    std::snprintf(line, sizeof(line), "FLOOR %d/%d  %s", dungeon.CurrentFloor(), dungeon.MaxFloors(), RoomTypeName(room.type));
    CenterText(line, 16, 0xFFFFD16A, 1);

    DrawGameplayIcon(6, 76, 28, 8);
    Text::DrawString(shotLine, 87, 29, 0xFFEAEAEA, 1);

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
    DrawGameplayIcon(4, 5, 67, 7);
    Text::DrawString(line, 15, 68, 0xFFEAEAEA, 1);

    std::snprintf(line, sizeof(line), "DMG %d SPD %d", player.damage, (int)player.moveSpeed);
    Text::DrawString(line, 76, 50, 0xFFEAEAEA, 1);

    DrawPickupIcon(player.CoinValue() >= 10 ? 2 : 0, 5, 27, 9);
    DrawPickupIcon(3, 5, 44, 9);
    DrawPickupIcon(4, 5, 61, 9);
    std::snprintf(line, sizeof(line), "%d", player.CoinValue());
    Text::DrawString(line, 16, 29, 0xFFFFD16A, 1);
    std::snprintf(line, sizeof(line), "%d", player.keyCount);
    Text::DrawString(line, 16, 46, 0xFF9FE8FF, 1);
    std::snprintf(line, sizeof(line), "%d", player.bombCount);
    Text::DrawString(line, 16, 63, 0xFFFF9B66, 1);

    bool hasAnyFx = player.hasHomingShots || player.poisonChance > 0.0f || player.stickyChance > 0.0f ||
        player.piercingChance > 0.0f || player.explosiveChance > 0.0f || player.burnChance > 0.0f ||
        player.freezeChance > 0.0f || player.magnetChance > 0.0f || player.boomerangChance > 0.0f ||
        player.growingChance > 0.0f || player.shrinkingChance > 0.0f || player.chainChance > 0.0f ||
        player.gravityChance > 0.0f || player.vortexChance > 0.0f || player.critChance > 0.0f ||
        player.lifestealChance > 0.0f || player.markChance > 0.0f || player.wallBounceChance > 0.0f ||
        player.enemyBounceChance > 0.0f || player.splitChance > 0.0f || player.hasVoidHeart ||
        player.hasTwinSoul || player.hasParasiteCore || player.hasLastShot || player.hasDevastator ||
        player.hasInfiniteLoop || player.hasChaosEngine || player.hasSatellites || player.hasChargedShots ||
        player.hasBurstShots || player.hasMirrorWard || player.hasPhoenixFeather;

    if (hasAnyFx) {
        char fx[160] = {};
        std::snprintf(fx, sizeof(fx), "FX%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
                      player.hasHomingShots ? " HOM" : "",
                      player.poisonChance > 0.0f ? " P" : "",
                      player.stickyChance > 0.0f ? " S" : "",
                      player.piercingChance > 0.0f ? " PI" : "",
                      player.explosiveChance > 0.0f ? " EX" : "",
                      player.burnChance > 0.0f ? " BRN" : "",
                      player.freezeChance > 0.0f ? " FRZ" : "",
                      player.magnetChance > 0.0f ? " MAG" : "",
                      player.boomerangChance > 0.0f ? " BOO" : "",
                      player.chainChance > 0.0f ? " CHN" : "",
                      player.gravityChance > 0.0f || player.vortexChance > 0.0f ? " GRV" : "",
                      player.critChance > 0.0f ? " CRT" : "",
                      player.lifestealChance > 0.0f ? " LIF" : "",
                      player.markChance > 0.0f ? " MRK" : "",
                      player.wallBounceChance > 0.0f ? " RIC" : "",
                      player.enemyBounceChance > 0.0f ? " RUB" : "",
                      player.splitChance > 0.0f ? " SPL" : "",
                      player.hasVoidHeart ? " VOID" : "",
                      player.hasTwinSoul ? " TWIN" : "",
                      player.hasMirrorWard ? " WARD" : "",
                      player.hasPhoenixFeather ? " 1UP" : "",
                      (player.hasParasiteCore || player.hasLastShot || player.hasDevastator ||
                       player.hasInfiniteLoop || player.hasChaosEngine || player.hasSatellites ||
                       player.hasChargedShots || player.hasBurstShots) ? " +" : "");
        // Active modifiers are represented by the item/inventory screen.
    }

    if (player.pickupMessageTimer > 0.0f && player.pickupName[0] != '\0') {
        std::snprintf(line, sizeof(line), "PICKUP %s", player.pickupName);
        CenterText(line, 72, 0xFFFFFF88, 1);
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
    int lobbyW = 0;
    int lobbyH = 0;
    Renderer::GetSheetSize(Renderer::kLobbySheetId, lobbyW, lobbyH);
    Renderer::DrawSprite(Renderer::MakeSprite(Renderer::kLobbySheetId, 0, 0, lobbyW, lobbyH),
                         0, 0, 320, 180);
    Renderer::DrawRect(0, 128, 320, 52, 0xE6090A10);
    Renderer::DrawRect(0, 128, 320, 1, 0xFFFFB347);

    const char* subtitle = "A PIXEL DUNGEON RUN";
    int subtitleW = Text::MeasureWidth(subtitle, 1);
    Text::DrawString(subtitle, (320 - subtitleW) / 2, 134, 0xFFFFD16A, 1);

    CenterText("ENTER  START RUN", 146, 0xFFFFFFFF, 1);
    CenterText("WASD MOVE   ARROWS AIM", 157, 0xFFEAEAEA, 1);
    CenterText("SPACE DASH", 168, 0xFFB9C7E8, 1);

}

void DrawFloorTransition(int floor, int maxFloors, const char* treasureLine, bool canContinue) {
    Renderer::DrawRect(0, 0, 320, 180, 0xFF0C1016);
    Renderer::DrawRect(0, 0, 320, 18, 0xFF1C2A44);
    Renderer::DrawRect(0, 162, 320, 18, 0xFF1C2A44);
    Renderer::DrawRect(18, 18, 284, 1, 0xFF42638F);
    Renderer::DrawRect(18, 161, 284, 1, 0xFF42638F);

    const char* title = "FLOOR";
    int titleW = Text::MeasureWidth(title, 2);
    Text::DrawString(title, (320 - titleW) / 2, 28, 0xFFFFFFFF, 2);

    char floorLine[32];
    std::snprintf(floorLine, sizeof(floorLine), "%d/%d", floor, maxFloors);
    int floorLineW = Text::MeasureWidth(floorLine, 3);
    Text::DrawString(floorLine, (320 - floorLineW) / 2, 52, 0xFFFFD16A, 3);

    CenterText(canContinue ? "THE NEXT DESCENT AWAITS" : "PREPARING THE NEXT DESCENT",
               92, 0xFFEAEAEA, 1);

    Renderer::DrawRect(74, 126, 172, 4, 0xFF182237);
    Renderer::DrawRect(74, 126, canContinue ? 172 : 92, 4, 0xFFFFB347);

    if (treasureLine && treasureLine[0] != '\0') {
        CenterText(treasureLine, 112, 0xFFFFC84D, 1);
    }

    const char* prompt = canContinue ? "ENTER / SPACE TO START" : "LOADING...";
    CenterText(prompt, 140, 0xFFFFFFFF, 1);
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

void DrawPauseScreen(const Player& player) {
    Renderer::DrawRect(30, 20, 260, 140, 0xFF0B0D14);
    Renderer::DrawRect(30, 20, 260, 2, 0xFFFFB347);
    Renderer::DrawRect(30, 158, 260, 2, 0xFF42638F);
    CenterText("PAUSED", 30, 0xFFFFD16A, 2);
    CenterText("P  RESUME", 54, 0xFFFFFFFF, 1);
    CenterText("INVENTORY", 72, 0xFF9FC5FF, 1);

    if (player.ownedItemIds.empty()) {
        CenterText("NO ITEMS YET", 90, 0xFF888899, 1);
    } else {
        int row = 0;
        for (int itemId : player.ownedItemIds) {
            const ItemTemplate* item = ItemDatabase::Get(itemId);
            if (!item || row >= 5) continue;
            char line[64];
            std::snprintf(line, sizeof(line), "%d  %s", row + 1, item->name);
            CenterText(line, 88 + row * 10, 0xFFEAEAEA, 1);
            row++;
        }
        if ((int)player.ownedItemIds.size() > 5) CenterText("... MORE ITEMS ...", 140, 0xFF888899, 1);
    }
    CenterText("SETTINGS: P RESUME", 150, 0xFF7788AA, 1);
}

void DrawBossHealthBar(const Boss& boss, const char* bossName) {
    const char* name = (bossName && bossName[0] != '\0') ? bossName : "BOSS";
    char line[64];
    std::snprintf(line, sizeof(line), "%s  PHASE %d%s", name, boss.phase, boss.isCharging ? " CHARGE" : "");
    int w = Text::MeasureWidth(line, 1);
    Text::DrawString(line, (320 - w) / 2, 0, 0xFFFFFFFF, 1);

    int barWidth = 200;
    int fillWidth = (int)(barWidth * (boss.hp / (float)boss.maxHp));
    int barX = (320 - barWidth) / 2;
    Renderer::DrawRect(barX, 8, barWidth, 5, 0xFF444444);
    Renderer::DrawRect(barX, 8, fillWidth, 5, 0xFFAA33FF);
}

void DrawFloorMap(const Dungeon& dungeon, const Player& player) {
    const std::vector<Room>& rooms = dungeon.Rooms();
    if (rooms.empty()) return;

    int currentIndex = dungeon.CurrentRoomIndex();
    if (currentIndex < 0 || currentIndex >= (int)rooms.size()) return;

    std::vector<bool> revealed(rooms.size(), false);
    std::vector<bool> outlined(rooms.size(), false);

    if (player.hasCompass) {
        std::fill(revealed.begin(), revealed.end(), true);
    } else {
        std::queue<int> q;

        auto Reveal = [&](int index) {
            if (index < 0 || index >= (int)rooms.size()) return;
            if (revealed[index]) return;
            revealed[index] = true;
            q.push(index);
        };

        Reveal(currentIndex);
        for (int i = 0; i < (int)rooms.size(); ++i) {
            if (rooms[i].cleared) Reveal(i);
        }

        while (!q.empty()) {
            int index = q.front();
            q.pop();
            const Room& room = rooms[index];

            const int neighbors[4] = { room.north, room.south, room.east, room.west };
            for (int next : neighbors) {
                if (next < 0 || next >= (int)rooms.size()) continue;
                if (revealed[next]) continue;
                if (!rooms[next].cleared) continue;
                revealed[next] = true;
                q.push(next);
            }
        }

        for (int i = 0; i < (int)rooms.size(); ++i) {
            if (!revealed[i]) continue;
            const Room& room = rooms[i];
            const int neighbors[4] = { room.north, room.south, room.east, room.west };
            for (int next : neighbors) {
                if (next < 0 || next >= (int)rooms.size()) continue;
                if (revealed[next]) continue;
                outlined[next] = true;
            }
        }
    }

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

    const int cellSize = 6;
    const int gap = 2;
    const int cols = maxGridX - minGridX + 1;
    const int rows = maxGridY - minGridY + 1;
    const int mapW = cols * cellSize + (cols - 1) * gap;
    const int mapH = rows * cellSize + (rows - 1) * gap;
    const int mapX = 320 - mapW - 6;
    const int mapY = 14;

    Renderer::DrawRect(mapX - 3, mapY - 3, mapW + 6, mapH + 6, 0xFF111111);
    Renderer::DrawRect(mapX - 2, mapY - 2, mapW + 4, 1, 0xFF444444);
    Renderer::DrawRect(mapX - 2, mapY + mapH + 1, mapW + 4, 1, 0xFF444444);
    Renderer::DrawRect(mapX - 2, mapY - 2, 1, mapH + 4, 0xFF444444);
    Renderer::DrawRect(mapX + mapW + 1, mapY - 2, 1, mapH + 4, 0xFF444444);
    Text::DrawString("MAP", mapX, 5, 0xFFFFD16A, 1);

    auto CellOrigin = [&](const Room& room) {
        int gx = (int)room.gridPos.x - minGridX;
        int gy = (int)room.gridPos.y - minGridY;
        return Vec2{ (float)(mapX + gx * (cellSize + gap)), (float)(mapY + gy * (cellSize + gap)) };
    };

    for (int i = 0; i < (int)rooms.size(); ++i) {
        if (!revealed[i]) continue;
        const Room& room = rooms[i];
        Vec2 origin = CellOrigin(room);
        int x = (int)origin.x;
        int y = (int)origin.y;

        if (room.east >= 0 && room.east < (int)rooms.size() && revealed[room.east]) {
            Renderer::DrawRect(x + cellSize, y + 2, gap, 1, 0xFF666666);
        }
        if (room.south >= 0 && room.south < (int)rooms.size() && revealed[room.south]) {
            Renderer::DrawRect(x + 2, y + cellSize, 1, gap, 0xFF666666);
        }
    }

    for (int i = 0; i < (int)rooms.size(); ++i) {
        if (!revealed[i]) continue;
        const Room& room = rooms[i];
        Vec2 origin = CellOrigin(room);
        int x = (int)origin.x;
        int y = (int)origin.y;
        uint32_t color = RoomColor(room, i == dungeon.CurrentRoomIndex());
        Renderer::DrawRect(x, y, cellSize, cellSize, color);
        int iconSlot = RoomIconSlot(room);
        int themeCol = iconSlot % 12;
        int themeRow = iconSlot / 12;
        Renderer::DrawSprite(Renderer::MakeScaledSprite(ThemeSheetId(room.theme),
                                                         themeCol * 128, themeRow * 128,
                                                         128, 128, 1536, 1024),
                             x, y, cellSize, cellSize);
        if (i == dungeon.CurrentRoomIndex()) {
            DrawOutline(x - 1, y - 1, cellSize + 2, cellSize + 2, 0xFFFFFFFF);
        }
    }

    if (!player.hasCompass) {
        for (int i = 0; i < (int)rooms.size(); ++i) {
            if (!outlined[i] || revealed[i]) continue;
            const Room& room = rooms[i];
            Vec2 origin = CellOrigin(room);
            int x = (int)origin.x;
            int y = (int)origin.y;

            uint32_t outlineColor = 0xFF666666;
            if (room.type == RoomType::START) outlineColor = 0xFF4B7F5A;
            else if (room.type == RoomType::BOSS) outlineColor = 0xFF7A4A9E;
            else if (room.type == RoomType::TREASURE) outlineColor = 0xFF8A7330;
            else if (room.type == RoomType::CURSE) outlineColor = 0xFF8A4040;

            DrawOutline(x, y, cellSize, cellSize, outlineColor);
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
        { "C Curse", 0xFFFF5555 },
        { "Shop", 0xFF55DDEE }
    };

    int lineY = legendY + 9;
    for (const LegendEntry& entry : entries) {
        Renderer::DrawRect(legendX, lineY + 1, 4, 4, entry.color);
        Text::DrawString(entry.label, legendX + 7, lineY, 0xFFEAEAEA, 1);
        lineY += 8;
    }
}

} // namespace HUD
