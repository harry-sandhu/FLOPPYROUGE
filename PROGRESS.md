# FloppyRogue — Progress Tracker

Last updated: boss fight working end-to-end, data-driven enemies, real
text rendering, arrow-key directional shooting.

## Contest constraints (don't lose sight of these)
- Hard cap: 1,474,560 bytes, extracted/delivered executable
- Deadline: Sept 4, 2026, 23:59
- Judging order: (1) finished game, (2) under size cap, (3) fun
- Last measured release build size: **123KB (~8% of cap)** — recheck after
  the -static/-static-libstdc++/-static-libgcc link changes, size likely
  went up a bit but should still have huge headroom

## Done ✅
- [x] Win32 window creation + message pump (`engine/window`)
- [x] 320x180 software framebuffer + GDI blit, nearest-neighbor upscale
      (`engine/renderer`)
- [x] Input polling: held keys + "just pressed" (`engine/input`)
- [x] Fixed delta-time timer (`engine/core/timer.h`)
- [x] Shared math types: `Vec2`, `Rect` (`engine/core/types.h`)
- [x] AABB collision check (`engine/collision`)
- [x] Player struct + movement + damage/i-frames (`game/player/player`)
- [x] Projectile struct + spawn/update/collide/draw system, split into
      vs-enemy / vs-player / vs-boss collision paths
      (`game/player/projectile`, `projectile_system`)
- [x] Enemy struct + AI-type dispatch — **CHASER and SHOOTER both working**
      (`game/enemies/enemy`)
- [x] Contact damage (enemy -> player), HP bar, game over + restart
- [x] Room boundaries — player/enemy/boss clamped to 320x180 play area
      (`game/rooms/room`)
- [x] Bitmap font text rendering — real "HP", "GAME OVER", "ROOM CLEARED"
      text instead of colored rectangles (`engine/text`)
- [x] Custom data file parser — key=value blocks, no STL string, no JSON
      dependency (`engine/data_parser`)
- [x] Enemy database — enemies spawn from `data/enemies.txt` by name,
      not hardcoded structs in main.cpp (`game/enemies/enemy_database`)
- [x] Boss: 3 attack patterns (spread shot, radial burst, charge dash),
      phase 2 speedup at 50% HP, boss health bar (`game/bosses/boss`)
- [x] Full state machine: PLAYING -> (enemies cleared) -> BOSS_FIGHT ->
      WON or LOST -> restart on R
- [x] Directional shooting — arrow keys (Up/Down/Left/Right) instead of
      Space, cancels if more than one key pressed same frame
- [x] HUD module: player HP bar, boss HP bar, game-over/room-cleared
      banners (`game/ui/hud`)
- [x] main.cpp reduced to orchestration only (input -> systems -> draw)
- [x] Fixed MinGW/GCC 16 linker bug: removed std::string entirely from
      data_parser and enemy_database (fixed-size char buffers instead)
      after -static and -flto both triggered undefined `basic_string`
      move-constructor errors
- [x] Static-linked runtime (-static-libgcc -static-libstdc++ -static) —
      confirmed no libwinpthread-1.dll dependency at runtime under Wine

**This completes the full original Week 1 checklist**: player movement,
shooting, enemies, procedural-ish rooms (bounds only), a boss, and a
win/lose loop — all working together in one playable slice.

## In progress / next up 🔧
- [ ] Re-measure exe size after all the static-linking changes — do this
      before adding more content, to know actual remaining headroom
- [ ] Fire-rate/held-key shooting — currently one shot per key-press
      (IsPressed), not hold-to-fire (IsDown) — decide if this is the
      intended feel or needs changing
- [ ] Player aim only fires in 4 cardinal directions, no diagonals, no
      aiming toward cursor — fine for now, revisit if it feels bad

## Not started yet ⬜
- [ ] Remaining AI types: CHARGER, SUMMONER, EXPLODER
- [ ] `engine/procgen` — procedural sprite generation (replaces flat-color
      rects with actual pixel-art-style shapes) and audio synthesis
      (square/saw/noise SFX + simple music loop) — keeps the build small
      instead of adding asset files
- [ ] `engine/audio` — waveOut playback + mixing (currently no sound
      at all — no shoot/hit/death/boss SFX, no music)
- [ ] `engine/camera` — world -> screen offset tracking (not needed yet
      at single-room scale, becomes necessary once dungeon/rooms exist)
- [ ] `engine/animation` — frame-timer sprite indexing
- [ ] Room templates (`game/rooms`) — walls, doors, spawn points,
      decoration points per template (currently just one fixed bounding
      box, no real room variety)
- [ ] Dungeon assembly (`game/dungeon`) — room selection/graph, connects
      multiple rooms into a playable run (currently single arena only)
- [ ] Item system (`game/items`) — stat modifiers / on-hit/on-kill hooks,
      `data/items.txt` exists but is empty and unused
- [ ] Multiple enemies per encounter beyond the current fixed 2 (Zombie +
      Gunner) — no variety/randomization in enemy selection yet
- [ ] Second/third boss
- [ ] Juice/polish pass: screen shake, hit-stop, particle effects
- [ ] Difficulty tuning / playtesting pass
- [ ] UPX packaging step for final submission
- [ ] Final size check against the 1,474,560 byte cap + submission

## Known shortcuts taken (revisit before final polish)
- Boss and enemies are hardcoded spawn calls in main.cpp, not loaded from
  `data/rooms.txt` or a room/encounter definition — fine for now, will
  need restructuring once dungeon assembly exists
- `data/items.txt` and `data/rooms.txt` are unused placeholders
- Shooting is 4-directional only (no diagonal, no aim-toward-cursor)
- No audio at all yet — every hit/death/attack is silent
- No visual distinction beyond flat colored rectangles — no sprites,
  no animation frames
- Boss fight and regular-enemy fight share one fixed room size; nothing
  prevents boss projectiles/charge from interacting oddly with room edges
  since ClampToRoom is only applied to entities, not projectiles