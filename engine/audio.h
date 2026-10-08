#pragma once

namespace Audio {

enum class Cue {
    SHOT,
    SPECIAL_SHOT,
    HIT,
    DASH,
    PLAYER_HURT,
    PICKUP,
    CHEST,
    BOMB_PLACE,
    BOMB_EXPLODE,
    ROOM_CLEAR,
    TELEPORT
};

bool Init();
void Play(Cue cue);
void Shutdown();

} // namespace Audio
