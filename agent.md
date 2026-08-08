markdown
# FloppyRogue — Project Instructions

## What this is
A 2D top-down roguelite ("Isaac-like"): procedural rooms, one player, enemies driven
by shared AI-type behaviors, item pickups that modify stats/behavior, and boss
encounters. Built for the **2P_GAME_ARCADE 1.44MB GAME_DEV CONTEST**.

## The one constraint that overrides everything else
The entire game, as a standalone Windows executable (extracted/delivered size),
must be **≤ 1,474,560 bytes**. This is not a soft target — it is the primary
judging criterion after "did you finish the game." Every dependency, every
asset, every library must be evaluated against this budget first.

Consequences for how code should be written:
- **No external asset files.** No .png/.wav/.xm files. All sprites and audio
  are generated procedurally at runtime (see `engine/procgen`).
- **No heavy dependencies.** No raylib/SDL/JSON libraries. Raw Win32 API only.
  No STL containers/algorithms that pull in large template instantiations
  unless clearly justified — prefer small hand-written containers where it
  meaningfully affects binary size.
- **No exceptions, no RTTI.** Compile with `-fno-exceptions -fno-rtti` (or
  MSVC `/EHs-c- /GR-`). Never throw, never use `dynamic_cast` or `typeid`.
- **Size-optimized builds.** `-Os` (or MSVC `/O1`), LTO enabled, symbols
  stripped in release builds.
- Prefer static linking against a minimal CRT; if using ucrt dynamically,
  this must be a deliberate, tested decision — not a default.

## Tech stack
- Language: C++20, but written in a restrained/embedded style (avoid iostream,
  avoid exceptions, avoid heavy STL).
- Platform: Windows only, raw Win32 API (`WinMain`, `WndProc`).
- Rendering: software framebuffer (320×180 `uint32_t` buffer), blitted to the
  window via `StretchDIBits` (GDI), upscaled with nearest-neighbor to
  fullscreen/window size. No OpenGL/DirectX, no shaders.
- Audio: `waveOut` (or XAudio2 if mixing needs grow) fed by procedurally
  synthesized PCM buffers (square/saw/triangle/noise + simple envelopes).
- Build: CMake.

## Folder structure

FloppyRogue/
├── src/main.cpp # WinMain, game loop, top-level state machine
├── engine/
│ ├── window.h/.cpp # Win32 window + message pump only
│ ├── renderer.h/.cpp # framebuffer + blit + draw primitives
│ ├── input.h/.cpp # keyboard state polling
│ ├── audio.h/.cpp # waveOut playback + mixing
│ ├── camera.h/.cpp # world->screen offset tracking
│ ├── animation.h/.cpp # frame timing/indexing
│ ├── collision.h/.cpp # AABB/circle tests, simple spatial queries
│ ├── procgen.h/.cpp # generates sprite pixel data + synthesizes SFX/music
│ └── core/
│ ├── types.h # Vec2, Rect, Color — dependency-free
│ ├── rng.h # seeded PRNG (xorshift/PCG)
│ └── timer.h # delta time, fixed-timestep accumulator
├── game/
│ ├── player/
│ ├── enemies/ # ONE Enemy class + AI-type dispatch, not per-monster files
│ ├── bosses/ # ONE Boss class + attack-pattern/phase data
│ ├── items/ # ONE Item effect-application system
│ ├── rooms/
│ ├── dungeon/ # room-graph assembly / selection logic
│ └── ui/
├── data/
│ ├── enemies.txt
│ ├── items.txt
│ └── rooms.txt
└── CMakeLists.txt


## Data-driven design — non-negotiable
Do **not** create a new C++ class per enemy, boss, or item. Content is data:

- **Enemies**: one `Enemy` struct/class with fields (HP, speed, attack, sprite
  descriptor, AI type) + a small set of AI-type behavior functions
  (`CHASER`, `SHOOTER`, `CHARGER`, `SUMMONER`, `EXPLODER`). New enemies are
  new rows in `data/enemies.txt`, never new code.
- **Bosses**: one `Boss` class with an array of attack patterns + phases +
  summon lists, driven by data.
- **Items**: pure effect modifiers (stat multipliers, or hooks into
  on-hit/on-kill/on-shoot events on the player). Adding an item should never
  require touching unrelated systems.
- **Rooms**: template-based (walls/doors/spawn points/decoration points)
  with randomized enemy selection, obstacle placement, and pickups layered
  on top at instantiation time.

## Data file format (`data/*.txt`)
Simple key=value blocks, no JSON, no external parser:

[Zombie]
ai=CHASER
hp=40
speed=2.1

[FireZombie]
ai=CHASER
hp=60
speed=2.4
burn=true

Parser: split on `[Name]` headers to start a new record, split each line on
`=` for key/value, coerce to int/float/bool by field name. No escaping, no
nesting, no comments needed — keep the parser under ~100 lines.

## Priorities (in this order — matches contest judging criteria)
1. **Finish a complete, playable loop** (menu → dungeon → boss → win/lose).
   A smaller finished game beats a bigger unfinished one.
2. **Stay under the byte cap.** Check actual build size after every
   significant change, not just at the end.
3. **Make it fun** — juice (screen shake, hit-stop, particles), fair
   difficulty curve, readable enemy telegraphs.

Breadth (more enemies/items/rooms/bosses) is the last thing to add, and only
once 1 and 2 are solid.

## What NOT to do
- Don't add raylib/SDL/any external asset-loading engine.
- Don't add nlohmann::json or any JSON library.
- Don't create per-enemy or per-boss C++ classes.
- Don't load sprites/audio from files — generate them in `procgen`.
- Don't use STL exceptions-based error handling.
- Don't add a scripting language/embedded interpreter — data files + the
  dispatch-table pattern are sufficient at this scope.