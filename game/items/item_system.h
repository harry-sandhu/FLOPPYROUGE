#pragma once
#include "item_database.h"
#include "../player/player.h"

namespace ItemSystem {
    void ApplyItem(Player& player, const ItemTemplate& item);
    void GrantItem(Player& player, int itemId);
}
