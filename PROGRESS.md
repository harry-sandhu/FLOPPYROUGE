# FloppyRogue — Progress Tracker

Last updated: post-phase-10 — dungeon generation rewritten to a proper
dead-end algorithm with per-floor grid scaling, plus a new curse-room
risk/reward system and placeable bombs. Still comfortably under the
contest limit.

## Contest constraints (don't lose sight of these)
- Hard cap: 1,474,560 bytes, extracted/delivered executable
- Deadline: Sept 4, 2026, 23:59
- Judging order: (1) finished game, (2) under size cap, (3) fun
- Last measured release build size: **281,600 bytes**,
  **275.0 KB**, **19.1%** of the 1,474,560-byte cap,
  with **1,192,960 bytes** remaining
  *(pending re-measure after the dungeon/curse/bomb changes below)*

## Done
- [x] Win32 window creation + message pump (`engine/window`)
- [x] 320x180 software framebuffer + GDI blit, nearest-neighbor upscale
      (`engine/renderer`)
- [x] Input polling: held keys + "just pressed" (`engine/input`)
- [x] Fixed delta-time timer (`engine/core/timer.h`)
- [x] Shared math types: `Vec2`, `Rect` (`engine/core/types.h`)
- [x] AABB collision check (`engine/collision`)
- [x] Player movement + damage/i-frames + stat block rework
      (`game/player/player`)
- [x] Hold-to-fire shooting with fire-rate cooldowns
- [x] Diagonal-fire unlock and dash unlock hooks
- [x] Projectile damage/range now comes from player stats at fire time
- [x] Projectile struct + spawn/update/collide/draw system, split into
      vs-enemy / vs-player / vs-boss collision paths
      (`game/player/projectile`, `projectile_system`)
- [x] Enemy struct + AI dispatch
- [x] Remaining enemy AI types: CHARGER, SUMMONER, EXPLODER
- [x] Enemy database — enemies spawn from `data/enemies.txt` by name
      (`game/enemies/enemy_database`)
- [x] Boss system with 3 variants / 3 attack pattern sets
- [x] Contact damage, HP bar, game over + restart
- [x] Room boundaries — player/enemy/boss clamped to 320x180 play area
      (`game/rooms/room`)
- [x] Dungeon system — generated room graph, transitions, room types,
      room-cleared state, treasure/curse loot hooks (`game/dungeon`)
- [x] Floor mini-map in the top-right corner with a legend
- [x] Three-floor progression with boss floors 1 and 2, floor 3 as the finale
- [x] Rooms can stay empty, and normal-room enemy counts scale by floor
- [x] Enemies and bosses wait briefly after spawning before attacking
- [x] Projectile movement now advances separately from collision handling
- [x] Special enemy variants: reinforcers, creepers, and death-burst enemies
- [x] Bitmap font text rendering (`engine/text`)
- [x] Custom data file parser — key=value blocks, no STL string, no JSON
      dependency (`engine/data_parser`)
- [x] Item database + apply/grant logic (`game/items`)
- [x] Treasure rooms grant item rewards, curse rooms can also award loot
- [x] HUD module: player HP bar, boss HP bar, game-over/room-cleared
      banners, floor map (`game/ui/hud`)
- [x] Run status HUD: floor, room, dash, item count, pickup name, boss phase
- [x] Screen shake and action flash feedback for hits, pickups, dashes, and
      boss kills
- [x] main.cpp reduced to orchestration only (input -> systems -> draw)
- [x] Fixed MinGW/GCC 16 linker bug: removed std::string entirely from
      data_parser and enemy_database (fixed-size char buffers instead)
- [x] Static-linked runtime (-static-libgcc -static-libstdc++ -static) —
      confirmed no libwinpthread-1.dll dependency at runtime under Wine
- [x] Dungeon generation rewritten to the proper dead-end algorithm: START
      placed at grid center, normal rooms grown outward from any existing
      room (not one linear path), boss placed on the farthest degree-1 dead
      end from start, treasure and curse placed on two other distinct
      dead ends so every special room has exactly one door (`game/dungeon`)
- [x] Grid size and normal-room count now scale per floor instead of being
      fixed: 10x10/14 rooms on floor 1, 20x20/20 rooms on floor 2,
      30x30/26 rooms on floor 3 (`game/dungeon`)
- [x] Curse rooms split into two variants on generation: enemy-only (locked
      like a normal combat room until cleared) or item-only (walk-in, as
      before) — real risk/reward instead of guaranteed free loot
      (`game/dungeon`, `game/rooms/room`)
- [x] Curse damage now floor-aware and heart-accurate: half a heart on
      entry and half a heart on exit for floors 1-2, a full heart each way
      on the final floor, with a proper 0.5s invincibility window on both
      hits so they can't double-trigger (`game/dungeon`)
- [x] Heart and bomb pickups: clearing an enemy-curse room has a chance to
      drop one or the other (`game/dungeon`, `game/rooms/room`)
- [x] Placeable bombs: player can hold multiple, drop one with a 3-second
      fuse, and it deals AoE damage to enemies/boss in radius on
      detonation, with fuse-glow and explosion-flash rendering
      (`src/main.cpp`)
- [x] Tier 3 enemies added: Warlord, VenomEye, SplitterLord, BombKnight,
      HexMatron, JuggernautPrime, SnareTurret, BlightGrub
      (`data/enemies.txt`)
- [x] Item pool expansion: 80+ items with common, strong, weird, and build-defining
      categories (`data/items.txt`)
- [x] Boss attack patterns data-driven: attack cycles now loaded from
      `data/bosses.txt` with support for phase-specific rotations
      (`game/bosses/boss.cpp`, `game/bosses/boss_database.cpp`)
- [x] Chest system implementation: Wooden, Iron, Stone, Golden, Devil, Angel
      with distinct reward tables and requirements (`src/main.cpp`)
- [x] Coin/key/bomb pickup system with drop chances per room type
      (`src/main.cpp`, `game/dungeon/dungeon.cpp`)
- [x] Shop room type with pricing logic for items, hearts, bombs, and keys
      (`src/main.cpp`, `game/dungeon/dungeon.cpp`)
- [x] Bomb count display in HUD (B [count] added to status line)
      (`game/ui/hud.cpp`)

## In progress / next up
- [x] Re-measured exe size after the dungeon/item/enemy/boss additions
- [x] Smoke-tested the release executable under Wine; no startup crash
      observed before the timeout
- [x] Final size report recorded with exact bytes, KB, budget usage, and
      remaining headroom
- [ ] Re-measure exe size and re-run the Wine smoke test after all the
      content expansion and data-driven boss system additions
- [ ] Procedural sprite generation (`engine/procgen`) to replace flat-color
      rectangles with real pixel-art-style shapes
- [ ] Audio system (`engine/audio`) for shoot/hit/death/boss SFX and music
- [ ] Camera system (`engine/camera`) for world -> screen offset tracking
- [ ] Animation system (`engine/animation`) for frame-timer sprite indexing
- [ ] Juice/polish pass: hit-stop, particles, screen shake refinement
- [ ] Difficulty tuning / playtesting pass with new tier 3 enemies and expanded
      item pools, including the new bigger floor 2/3 grids and higher room counts
- [ ] UPX packaging step for final submission
- [x] Final size check against the 1,474,560 byte cap + submission
- [ ] Full interactive run verification with all content additions

## Known shortcuts taken
- Rooms are generated as a graph and shown in the mini-map, but the playfield
  itself is still a single 320x180 room view with no wall/door art yet
- Enemy and boss visuals are still flat-colored rectangles
- No audio yet, so hits, shots, deaths, and boss attacks are silent
- No camera layer yet because the gameplay still fits inside one active room
- Item rewards are functional, but there is no full inventory UI yet
- The dungeon now spans three floors with per-floor grid/room-count scaling,
  but the enemy mix and curse/heart/bomb drop rates still need playtesting
  and tuning
- Room struct still stores full int connection indices and float bounds
  rather than the packed 2-byte room representation suggested for the size
  budget — not yet needed since the build is well under the size cap, but
  worth revisiting if headroom gets tight