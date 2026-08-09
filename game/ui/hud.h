#pragma once
#include "../player/player.h"
#include "../bosses/boss.h"

class Dungeon;
struct Room;

namespace HUD {
    void DrawHealthBar(const Player& player);
    void DrawRunStatus(const Dungeon& dungeon, const Player& player, const Room& room, const Boss* boss);
    void DrawTitleScreen();
    void DrawGameOverBanner();
    void DrawRoomClearedBanner();
    void DrawBossHealthBar(const Boss& boss);
    void DrawFloorMap(const Dungeon& dungeon);
    void DrawItemPreview(const char* name, const char* desc);
}
