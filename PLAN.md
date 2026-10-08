# Project Improvement Plan

## Current State
FloppyRogue is a 2D top-down roguelite built in raw C++20 Win32 (no engine, no external libraries) for a "1.44MB GAME_DEV CONTEST" — the full standalone Windows executable must fit in 1,474,560 bytes. It uses a software-rendered framebuffer (320x180 internal resolution) blitted via GDI `StretchDIBits`. Content (enemies, items, rooms, bosses) is data-driven rather than hardcoded per-entity. The repo has 15+ commits showing clear iterative progress (terrain generation phases, enemy/boss rebalancing, item/chest systems, UI). Current committed release build is ~123KB against the 1,474,560-byte budget, per the README. The git history and working tree are clean of secrets; only minor uncommitted/in-progress changes exist locally (see Issues Found).

## What Is Already Good
- README is excellent: states the contest constraint plainly, explains the "why no engine/no assets" design rationale, documents the exact build commands, controls, and folder structure, and defers detailed status to `PROGRESS.md`.
- `PROGRESS.md` exists and is used as the living status document — a good practice that keeps the README stable while development details change frequently.
- Commit history shows genuine, well-sequenced iterative development (e.g., "Phase 1-2: terrain data model...", "Phase 3: document terrain algorithm implementation", "Rebalance enemy roster and AI types"), which is excellent for presenting real engineering process.
- The size-budget discipline (123KB of 1.44MB) with no engine/libraries is a genuinely strong, differentiated technical story for a portfolio.

## Issues Found
- **README factual gap (fixed)**: the README stated "No external asset files — sprites and audio are generated in code" but the codebase actually loads sprite sheets from external PNG files in `data/assets/` (confirmed via `src/main.cpp` calls to `Renderer::LoadSheet(..., L"data/assets/...png")`, and these PNGs are tracked in git). This has been corrected to accurately state that sprites are external PNGs while audio remains the planned procedurally-generated piece.
- **Audio not yet implemented**: confirmed both in the README ("planned — not yet implemented") and in `PROGRESS.md` ("No audio yet, so hits, shots, deaths, and boss attacks are silent"). This is the main remaining gap before the game feels complete.
- **Uncommitted local work-in-progress**: at the time of this review, the working tree had uncommitted modifications to `engine/audio.cpp`/`engine/audio.h` and two new untracked `.mp3` files under `data/assets/`, suggesting audio work may already be starting locally. This was left untouched (no commits made to code in this pass) — Harry should review and commit this separately when ready.
- No screenshots, GIFs, or video of actual gameplay anywhere in the repo or README.

## Documentation
README and PROGRESS.md are both in good shape. Only the asset-file factual inaccuracy above was corrected; no other rewrite was performed.

## Code Quality
Not audited in depth in this pass (docs-focused task). The in-progress audio-related changes in the working tree were left for Harry to commit on his own timeline.

## Testing
No automated tests observed. Given this is a from-scratch Win32 game under a strict size budget, this is reasonable; not a priority.

## Security
No secrets or credentials found. Nothing in this project touches network services or stores sensitive data.

## Architecture
Matches the README's description: software-rendered framebuffer, data-driven content, no engine/libraries. Sound architecturally for the contest's constraints.

## Screenshots / Visual Assets
None currently in the repo. Given how polished and differentiated this project already is, a short demo GIF or video (e.g., a dungeon run showing procedural generation, combat, and a boss fight) would substantially strengthen its portfolio presentation — this is the single highest-leverage addition.

## README
Classification: **Excellent**. No rewrite performed; only the two factual-gap lines about external asset files were corrected (sprites are external PNGs, not code-generated; audio remains planned/not implemented).

## Priority Roadmap

### P0 — Critical
- None. This project is close to portfolio-ready as-is.

### P1 — Important
- Implement the planned procedurally-synthesized audio (SFX at minimum: hits, shots, deaths, boss attacks) — the one functionally incomplete area per the project's own README/PROGRESS.md.
- Decide on and commit the in-progress audio work currently sitting uncommitted in the working tree (`engine/audio.cpp/.h`, new `.mp3` assets) rather than leaving it dangling.

### P2 — Nice to Have
- Add a short gameplay demo GIF or video to the README — this is the single most effective remaining presentation improvement given how strong the underlying project already is.
- Consider documenting the current vs. budget size trend over time (e.g., a small table in PROGRESS.md) to show the size-discipline story quantitatively.

## Recommended Next Steps
1. Finish and commit the audio implementation currently in progress locally.
2. Record a short gameplay clip/GIF and add it to the README.
3. No further documentation rewrites needed — this project's docs are already strong.
