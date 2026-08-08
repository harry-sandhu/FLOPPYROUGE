# FloppyRogue — Progress Tracker

Last updated: build confirmed working with movement, shooting, chaser
enemy, HP/lose loop, and module refactor out of main.cpp.

## Contest constraints (don't lose sight of these)
- Hard cap: 1,474,560 bytes, extracted/delivered executable
- Deadline: Sept 4, 2026, 23:59
- Judging order: (1) finished game, (2) under size cap, (3) fun
- Current release build size: **123KB (~8% of cap)** — huge headroom

## Done ✅
- [x] Win32 window creation + message pump (`engine/window`)
- [x] 320x180 software framebuffer + GDI blit, nearest-neighbor upscale
      (`engine/renderer`)
- [x] Input polling: held keys + "just pressed" (`engine/input`)
- [x] Fixed delta-time timer (`engine/core/timer.h`)
- [x] Shared math types: `Vec2`, `Rect` (`engine/core/types.h`)
- [x] AABB collision check (`engine/collision`)
- [x] Player struct + movement + damage/i-frames (`game/player/player`)
- [x] Projectile struct + spawn/update/collide/draw system
      (`game/player/projectile`, `projectile_system`)
- [x] Enemy struct + AI-type dispatch, CHASER implemented
      (`game/enemies/enemy`)
- [x] Contact damage (enemy -> player), HP bar, game over + restart
- [x] HUD module for health bar / game-over banner (`game/ui/hud`)
- [x] main.cpp reduced to orchestration only (input -> systems -> draw)

## In progress / next up 🔧
- [ ] Second AI type (SHOOTER is the natural next one — fires projectiles
      at the player instead of/while chasing)
- [ ] Room boundaries — player/enemy currently can wander off the 320x180
      play area entirely; needs wall collision or clamping
- [ ] Text rendering — game-over is currently a placeholder red bar, no
      real text exists yet (needed for HUD, menus, "GAME OVER" message)

## Not started yet ⬜
- [ ] Remaining AI types: CHARGER, SUMMONER, EXPLODER
- [ ] `engine/procgen` — procedural sprite generation (replaces flat-color
      rects with actual pixel-art-style shapes) and audio synthesis
      (square/saw/noise SFX + simple music loop) — this is the module that
      keeps the build small instead of adding asset files
- [ ] `engine/audio` — waveOut playback + mixing (currently no sound at all)
- [ ] `engine/camera` — world -> screen offset tracking (not needed yet at
      single-room scale, becomes necessary once dungeon/rooms exist)
- [ ] `engine/animation` — frame-timer sprite indexing
- [ ] Room system (`game/rooms`) — walls, doors, spawn points, decoration
      points per template
- [ ] Dungeon assembly (`game/dungeon`) — room selection/graph, connects
      rooms into a playable run
- [ ] Data file parser for `data/*.txt` (key=value block format) — needed
      before enemies/items/rooms can be authored as data instead of
      hardcoded structs
- [ ] Item system (`game/items`) — stat modifiers / on-hit/on-kill hooks
- [ ] Boss system (`game/bosses`) — one Boss class, attack patterns/phases
- [ ] Win condition (currently only lose exists — no way to "win" a run yet)
- [ ] Juice/polish pass: screen shake, hit-stop, particle effects
- [ ] Difficulty tuning / playtesting pass
- [ ] Final size check + UPX packaging + submission

## Known shortcuts taken (revisit before final polish)
- Enemy is a single hardcoded instance in main.cpp, not spawned from data
  or a list — fine for testing AI, not a real "one enemy in a room" yet
- Shooting always fires straight up — no aim direction (mouse or facing)
- Game-over/HUD text is colored rectangles, not real text rendering
- No walls/room boundaries — entities can leave the visible play area