#pragma once
#include "../player/player.h"

namespace HUD {
    void DrawHealthBar(const Player& player);
    void DrawGameOverBanner();
    void DrawRoomClearedBanner();
}