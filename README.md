# FloppyRogue

A 2D top-down roguelite built for the 2P_GAME_ARCADE **1.44MB GAME_DEV
CONTEST** — the entire game, as a standalone Windows executable, must fit
within 1,474,560 bytes.

## Concept

Procedurally generated dungeon rooms, one player character, enemies driven
by a small set of shared AI behaviors (not one class per monster), item
pickups that modify stats/behavior, and boss encounters. Content is
data-driven: new enemies/items/rooms are new data entries, not new code.

## Tech stack

- C++20, raw Win32 API (no engine, no external libraries)
- Software-rendered framebuffer (320x180 internal resolution), blitted to
  the window via GDI `StretchDIBits`, upscaled nearest-neighbor
- Audio: `waveOut` with procedurally synthesized SFX/music (planned —
  not yet implemented)
- Build: CMake + Ninja, MinGW (`x86_64-w64-mingw32-g++`)
- No external asset files — sprites and audio are generated in code
  (`engine/procgen`, planned) to keep the build small

## Why no engine / no assets?

The 1.44MB cap includes the executable and any runtime, so file-based
assets and heavy dependencies (raylib, SDL, JSON libraries) are avoided by
design. Current release build size: **123KB (~8% of the cap)**.

## Building

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++
cmake --build build


## Controls (current build)

- `WASD` — move
- `Space` — shoot (fires upward)
- `R` — restart after game over

## Folder structure

FloppyRogue/
├── src/main.cpp # WinMain, game loop, orchestration only
├── engine/ # window, renderer, input, audio, camera,
│ # animation, collision, procgen, core/
├── game/ # player, enemies, bosses, items, rooms,
│ # dungeon, ui — all data-driven, one class
│ # per category, not per content item
├── data/ # enemies.txt, items.txt, rooms.txt
└── CMakeLists.txt


## Status

See `PROGRESS.md` for what's done and what's next.