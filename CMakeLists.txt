cmake_minimum_required(VERSION 3.20)
project(FloppyRogue CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(FloppyRogue WIN32
    src/main.cpp
    engine/window.cpp
    engine/renderer.cpp
    engine/input.cpp
    engine/collision.cpp
    engine/text.cpp
    engine/data_parser.cpp
    game/player/player.cpp
    game/player/projectile_system.cpp
    game/items/item_database.cpp
    game/items/item_system.cpp
    game/dungeon/dungeon.cpp
    game/dungeon/terrain_gen.cpp
    game/enemies/enemy.cpp
    game/enemies/enemy_database.cpp
    game/bosses/boss.cpp
    game/bosses/boss_database.cpp
    game/ui/hud.cpp
    game/rooms/room.cpp
)

target_compile_options(FloppyRogue PRIVATE
    -fno-exceptions
    -fno-rtti
    -Os
)

target_link_options(FloppyRogue PRIVATE
    -s
    -static-libgcc
    -static-libstdc++
    -static
)

target_link_libraries(FloppyRogue PRIVATE gdi32 user32 kernel32)
