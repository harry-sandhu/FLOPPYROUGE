# FloppyRogue — Room Archetype Expansion: Implementation Plan

This document is a plan only — no code has been changed yet. It's grounded
in the actual current codebase (verified by reading the real files, not
assumed), so file names, line numbers, and function names below are real
as of this plan's writing. Line numbers will drift as edits land; treat
them as "where to look," not permanent anchors.

**Goal:** grow `RoomArchetype` from 6 entries to 20, backed by 6 reusable
generation *mechanisms* (not 20 bespoke ones), each mechanism parameterized
so that a huge number of distinct-feeling rooms (the user's target: "lakhs"
of effectively-unique rooms) fall out of a small amount of code, via RNG
sampling of each mechanism's parameters per room instance.

Current status:
- All planned phases are complete in code.

---

## 1. Where things stand today (verified)

| Concern | File | Location |
|---|---|---|
| `RoomArchetype` enum (6 values) | `game/rooms/room.h` | lines 62–69 |
| `RoomTerrainFeature` struct | `game/rooms/room.h` | lines 95–110 |
| `RollArchetype()` — picks archetype per room by floor/type | `game/dungeon/dungeon.cpp` | line 207 |
| `RollEncounterFamily()` — archetype → enemy family | `game/dungeon/dungeon.cpp` | line 246 |
| `TERRAIN_GRID_W/H` (16×9 combat grid) | `game/dungeon/dungeon.cpp` | lines 327–328 |
| `RoomThreatBudget()` — archetype → threat points | `game/dungeon/dungeon.cpp` | line 363 |
| `ArchetypeOpenTarget()` / `ArchetypeLargestRegionTarget()` | `game/dungeon/dungeon.cpp` | lines 439, 451 |
| `ROCK_CLUSTERS[][4]` / `PIT_SPOTS[]` (fixed candidate lists) | `game/dungeon/dungeon.cpp` | lines 685, 696 |
| `GenerateCellularTerrain` (CA lambda, currently only for HAZARD_ROOM/BROKEN_ARENA) | `game/dungeon/dungeon.cpp` | line 848 |
| `ValidateCombatSpace()` — BFS connectivity check | `game/dungeon/dungeon.cpp` | line 484 |
| Door safety zone clearing | `game/dungeon/dungeon.cpp` | lines 551–568 |
| `Dungeon::Generate()` — floor/room-graph growth, room-level retry loop | `game/dungeon/dungeon.cpp` | line 1134 onward |
| `MAX_GENERATION_ATTEMPTS = 64` | `game/dungeon/dungeon.cpp` | line 13 |
| RNG utility (`RNG::Range`, `RNG::Chance`, `RNG::NextU32`) | `engine/core/rng.h` | full file |

**Confirmed NOT yet present:** push-safety BFS re-check after a block push
(out of scope for this plan — see §6).

This matches what the project's own `implementation_plan.md` already
documents for the current 6-archetype system, so this plan extends that
document's approach rather than replacing it.

---

## 2. The 20 archetypes

**Existing (6, unchanged in identity, extended in tooling):**
OPEN_ARENA, PILLAR_FIELD, GAUNTLET, HAZARD_ROOM, RITUAL_ROOM, BROKEN_ARENA

**New (14):**
SPIRE_ASCENT, FLOODED_CHAMBER, COLLAPSED_VAULT, SENTRY_HALL, GARDEN_MAZE,
SHATTERED_BRIDGE, ECHO_ROOM, THRONE_APPROACH, TWIN_ISLANDS,
WHISPERING_STACKS, FORKING_PATH, RUNIC_LATTICE, NARROW_VEINS, AMPHITHEATER

### Archetype → mechanism → parameters

| Archetype | Mechanism | Key parameters that get RNG-sampled per room |
|---|---|---|
| OPEN_ARENA | CA + bias mask | seedChance (near 0), edge-ring width |
| PILLAR_FIELD | CA + repulsion | seedChance, minSpacing, 2×2-vs-1×1 mix ratio |
| GAUNTLET | CA + bias mask (axial bands) | band count, per-band seedChance, axis (derived from door layout) |
| HAZARD_ROOM | CA + bias mask | seedChance, passes, threshold (existing, unchanged) |
| BROKEN_ARENA | CA + bias mask | seedChance, passes, threshold (existing, unchanged) |
| RITUAL_ROOM | Mirror-half | mirror axis, focal-point clear radius, per-half CA params |
| SPIRE_ASCENT | CA + radial bias | corner/origin point, falloff rate |
| AMPHITHEATER | CA + radial bias (inverted) | same as SPIRE_ASCENT, falloff inverted |
| FLOODED_CHAMBER | CA + bias mask (aggressive smoothing) | high passes, high threshold → one big blob |
| SHATTERED_BRIDGE | CA + bias mask (inverted GAUNTLET) | band count, gap-vs-solid ratio per band |
| ECHO_ROOM | CA + bias mask (very sparse) + focal exclusion | seedChance (very low), 1–2 focal rects kept clear |
| THRONE_APPROACH | CA + bias mask (funnel) | width-taper rate, taper direction (toward exit door) |
| COLLAPSED_VAULT | Branching walk (ray/line from center) | ray count, ray length falloff, jitter angle |
| NARROW_VEINS | Branching walk (recursive fork) | seed point count, fork probability, max depth |
| SENTRY_HALL | Mirror-half | mirror axis, lane width, lane count |
| TWIN_ISLANDS | Mirror-half + bridge carve | mirror axis, bridge width, bridge count |
| GARDEN_MAZE | Maze-carve (drunkard's walk) | corridor width, turn bias, dead-end trim pass |
| WHISPERING_STACKS | Grid-rule (lattice) | spacing, jitter, gap probability |
| RUNIC_LATTICE | Grid-rule (checkerboard) | phase offset, cell size |
| FORKING_PATH | Grid-rule (parallel walls) | lane count, wall thickness, gap position per wall |

### The 6 underlying mechanisms (build these, not 20 archetypes)

1. **CA + bias mask** — generalizes the existing `GenerateCellularTerrain`.
   Covers 9 archetypes. Struct: `CAParams{ seedChance, neighborThreshold,
   passes, BiasFn maskFn }` where `maskFn(x, y) -> float` multiplies the
   base seed chance per-cell (axial band, radial falloff, edge-ring, funnel,
   sparse-uniform, or flat/no-op for the two existing archetypes).
2. **CA + repulsion** — CA + a post-pass that clears any solid cell within
   `minSpacing` of another solid cell. Covers PILLAR_FIELD only, but is a
   ~10-line addition on top of mechanism 1, not a separate system.
3. **Mirror-half** — generate one half of the grid (via mechanism 1's CA,
   parameterized per-archetype), mirror it across an axis, then punch a
   focal-clear region and/or bridge lanes through the result. Covers
   RITUAL_ROOM, SENTRY_HALL, TWIN_ISLANDS.
4. **Branching walk** — recursive/iterative walk from 1–2 seed points,
   forking with some probability, laying a thin line of solid or pit cells
   as it goes. Covers COLLAPSED_VAULT (few long rays, low fork chance),
   NARROW_VEINS (many forks, shorter segments).
5. **Maze-carve** — standard corridor-carving walk (drunkard's walk or
   randomized Prim's) inside the room bounds, producing winding sub-paths.
   Covers GARDEN_MAZE only — this is the one truly new algorithm with no
   overlap with the CA family.
6. **Grid-rule** — deterministic modulo/lattice pattern with a randomized
   phase/jitter/gap-probability, no smoothing pass. Covers WHISPERING_STACKS,
   RUNIC_LATTICE, FORKING_PATH — cheapest mechanism to implement and to
   compute at runtime.

---

## 3. Files to add or change

**New file: `game/dungeon/terrain_gen.h` + `game/dungeon/terrain_gen.cpp`**
Reason: `dungeon.cpp` is already large; a 6-archetype CA lambda living
inline was fine, but 6 mechanisms × 20 parameter sets should not all be
inlined into `Dungeon::Generate()`'s translation unit. This new pair of
files should hold:
- `CAParams` struct and the generalized CA-with-bias function (mechanism 1)
- The repulsion post-pass (mechanism 2)
- Mirror-half helper (mechanism 3)
- Branching-walk generator (mechanism 4)
- Maze-carve generator (mechanism 5)
- Grid-rule generator (mechanism 6)
- One dispatch function, e.g. `GenerateRoomTerrain(Room&, RoomArchetype, int floor)`,
  that switches on archetype and calls the right mechanism with that
  archetype's parameter ranges.

**`game/rooms/room.h`**
- Extend the `RoomArchetype` enum (lines 62–69) with the 14 new values.
- No changes needed to `RoomTerrainFeature` — the existing field set
  (confirmed in §1) already covers what all 6 mechanisms need to emit.

**`game/dungeon/dungeon.cpp`**
- `RollArchetype()` (line 207): extend the per-floor/per-type probability
  tables to include the new archetypes. Keep existing weights for the
  original 6 roughly proportional; don't silently starve them to zero.
- `RollEncounterFamily()` (line 246): add a case for each new archetype.
- `RoomThreatBudget()` (line 363): add a budget value per new archetype,
  calibrated relative to the existing 6 (e.g. GARDEN_MAZE and
  WHISPERING_STACKS likely lower — more movement, less open combat space;
  FLOODED_CHAMBER and THRONE_APPROACH likely higher).
- `ArchetypeOpenTarget()` / `ArchetypeLargestRegionTarget()` (lines 439,
  451): add per-archetype target values — these feed the existing
  `ValidateCombatSpace()` accept/reject logic, which stays untouched.
- Replace the direct call to `GenerateCellularTerrain()` (line 947-ish)
  with a call to the new dispatch function in `terrain_gen.cpp`.
- **Do not** delete `ROCK_CLUSTERS`/`PIT_SPOTS` in this pass — see §6.

**No changes needed to:**
- `ValidateCombatSpace()`, door-safety-zone clearing, `TERRAIN_GRID_W/H`,
  `MAX_GENERATION_ATTEMPTS`, or anything in the room-graph/floor growth
  loop (`Dungeon::Generate()`'s grid-walk section). All new mechanisms
  produce the same grid-of-cells shape the existing validation already
  consumes — that boundary is the whole point of keeping this to "new
  generators, same pipeline."

---

## 4. How this gets to "lakhs" of distinct rooms

The combinatorics, so the target is concrete rather than aspirational:

- 20 archetypes
- Each archetype's mechanism has 3–5 continuously-sampled parameters
  (seed chance, thresholds, spacing, axis choice, fork probability, etc.),
  each drawn per-room from `RNG::Range(...)` within an archetype-specific
  band (not a fixed constant)
- The CA/branching/maze mechanisms are also seeded per-room by
  `RNG::NextU32()`-derived state, so even *identical* parameters produce
  different cell layouts room to room

Even a conservative estimate — say 8–12 meaningfully-different discretized
outcomes per parameter, 3 parameters actually driving visual variety per
archetype — gives roughly 8³–12³ (≈500–1,700) distinguishable layouts per
archetype before even counting RNG-seed variety within a fixed parameter
set. Across 20 archetypes that's already in the tens of thousands, and the
per-seed CA/walk variance (not a discretized parameter, genuinely
continuous) pushes real perceptual variety well past that. "Lakhs" is
realistic without needing a 21st archetype or hand-authored content —
it's a property of parameterizing the mechanisms, not of adding more of
them.

---

## 5. Suggested build order

1. [x] Land `terrain_gen.h/.cpp` with mechanism 1 (CA + bias) only, covering
   the existing 6 archetypes with zero behavior change (regression check:
   old and new should produce statistically similar rooms). This proves
   the dispatch pattern works before any new archetype is added.
2. [x] Add mechanism 6 (grid-rule) + its 3 archetypes (WHISPERING_STACKS,
   RUNIC_LATTICE, FORKING_PATH) — cheapest mechanism, good second proof
   point, no smoothing/validation edge cases.
3. [x] Add mechanism 1's remaining bias variants (radial, funnel, aggressive-
   blob, inverted-band) for SPIRE_ASCENT, AMPHITHEATER, FLOODED_CHAMBER,
   SHATTERED_BRIDGE, ECHO_ROOM, THRONE_APPROACH — reuses mechanism 1's
   code, just new `maskFn` implementations.
4. [x] Add mechanism 2 (repulsion) for PILLAR_FIELD's improved version.
5. [x] Add mechanism 3 (mirror-half) for RITUAL_ROOM (upgrade), SENTRY_HALL,
   TWIN_ISLANDS.
6. [x] Add mechanism 4 (branching walk) for COLLAPSED_VAULT, NARROW_VEINS.
7. [x] Add mechanism 5 (maze-carve) for GARDEN_MAZE — last, since it's the
   only mechanism with no code reuse from the others.
8. [x] Recalibrate `RollArchetype()` weights and `RoomThreatBudget()` values
   across all 20 together, once all are generating valid rooms — doing
   this per-mechanism as you go would mean re-tuning repeatedly.

Each step should pass `ValidateCombatSpace()` and the existing bounded
retry loop before moving to the next — no new validation logic is being
introduced, so failures at any step point at the new mechanism's params,
not the pipeline.

---

## 6. Explicitly out of scope for this plan

- **Push-safety BFS re-check** for pushable blocks — real, confirmed
  missing, but unrelated to archetype variety. Separate plan if wanted.
- **Deleting `ROCK_CLUSTERS`/`PIT_SPOTS`** — leave as a fallback path.
  If a new mechanism's bounded retry (reusing `MAX_GENERATION_ATTEMPTS`-
  style logic, scoped to room-level) exhausts its attempts, falling back
  to the old fixed-list generator for that single room is safer than
  leaving a room unsolvable.
- **Floor/room-graph growth changes** (the `Dungeon::Generate()` grid-walk
  that decides room *count and connectivity*, not room *interior*) — this
  plan only touches what happens inside a single room's terrain grid.
  Floor-shape variety (branchy vs. blobby dungeons) is a separate,
  previously-discussed idea and isn't required for 20 archetypes to work.
- **Per-theme archetype gating** (e.g. WHISPERING_STACKS only in a library
  biome) — mentioned as a future idea, not decided yet; `RollArchetype()`
  changes above assume all 20 remain theme-agnostic for this pass.
- **Special-feature (teleporter/plate) position randomization** — a
  separate, smaller piece of earlier discussion; not part of the
  archetype/terrain-shape work this plan covers.
