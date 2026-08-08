Module responsibilities

src/main.cpp
Entry point. Owns WinMain, creates the window, runs the fixed-timestep game loop, and owns the top-level game state machine (menu → dungeon → boss → win/lose). Should stay thin — orchestration only, no game logic living here.

engine/window.h/.cpp
Win32 boilerplate: window class registration, WndProc, message pump. Exposes something like bool Window_Create(int w, int h, const char* title) and bool Window_PollEvents(). Nothing about pixels lives here — just OS window/message plumbing.

engine/renderer.h/.cpp
Owns the 320×180 uint32_t framebuffer and the StretchDIBits blit to the real window each frame. Exposes primitives: SetPixel, DrawRect, DrawSprite(spriteData, x, y), Clear(color). This is where the internal-resolution → fullscreen upscale happens (nearest-neighbor, integer scale factor for crisp pixels).

engine/input.h/.cpp
Polls GetAsyncKeyState or handles WM_KEYDOWN/WM_KEYUP messages from the window's message loop, exposes a simple IsKeyDown(key) / IsKeyPressed(key) API so the rest of the code never touches Win32 input directly.

engine/audio.h/.cpp
waveOut (or XAudio2 if you want mixing/effects — heavier but still tiny) buffer submission and a small mixer for layering SFX over music. Takes raw PCM buffers — it doesn't generate sound, it just plays what procgen hands it.

engine/procgen.h/.cpp
The asset replacement layer. Two halves:

Visual: functions that write directly into pixel buffers — e.g. GenerateEnemySprite(AIType, uint32_t* out) using simple shape composition + palette swaps, so "Zombie" and "Fire Zombie" can literally be the same generator with a different palette argument.
Audio: waveform synths — square/saw/triangle/noise generators with envelope (attack/decay) for SFX, and a tiny step-sequencer for background loops. This is the module doing the heavy lifting for your size budget.

engine/camera.h/.cpp
Tracks a world-space offset (follow player, room-locked bounds), converts world coordinates to screen coordinates for the renderer.

engine/animation.h/.cpp
Frame-timer-driven sprite frame indexing — given a sprite sheet (procedurally generated) and an AnimState, returns which frame to draw this tick.

engine/collision.h/.cpp
AABB (and maybe circle) intersection tests, spatial queries for "what's near this point" (even a naive grid is fine at this scale — you won't have enough entities on screen to need anything fancier).

engine/core/types.h
Vec2, Rect, Color — shared primitives every other module needs. Keep this dependency-free (no engine calls) so anything can include it.

engine/core/rng.h
A seeded PRNG (xorshift or PCG — a few lines each) exposed as Rng, with a stored seed so runs can be reproduced/debugged. You'll call this constantly: room selection, enemy placement, loot rolls, procgen variation.

engine/core/timer.h
Delta-time tracking and a fixed-timestep accumulator for your update loop.

game/ modules

Each subfolder (player, enemies, bosses, items, rooms, dungeon, ui) holds the behavior code — the data-driven design means enemies/ should really just contain one Enemy class plus the AI-type dispatch table, not per-monster files. Same logic for bosses/ (one Boss class + attack-pattern data) and items/ (one Item effect-application system).

data/*.txt format

Since we dropped JSON, a simple line-oriented key=value block format works well and parses in well under 100 lines:

[Zombie]
ai=CHASER
hp=40
speed=2.1

[FireZombie]
ai=CHASER
hp=60
speed=2.4
burn=true

Parser just needs to: split on [...] headers → new record, split each line on = → key/value pairs, convert to int/float/bool as needed. No escaping, no nesting — keep it dumb on purpose.

Build notes to remember
Static-link the CRT unless you specifically test dynamic linking saves you space without submission risk — decide this once, early, in CMakeLists.txt, not late.
-Os or /O1 (size-optimized), LTO on, strip symbols in the final build.
No exceptions/RTTI (-fno-exceptions -fno-rtti or MSVC equivalents) — keeps binary lean and avoids exception-handling tables.
UPX-pack the final .exe as a packaging step — allowed per the contest rules, and it's free space.