#pragma once
#include "../player/player.h"
#include "../bosses/boss.h"

class Dungeon;

namespace HUD {
    void DrawHealthBar(const Player& player);
    void DrawGameOverBanner();
    void DrawRoomClearedBanner();
    void DrawBossHealthBar(const Boss& boss);
    void DrawFloorMap(const Dungeon& dungeon);
}
