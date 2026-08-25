# FloppyRogue — Procedural Room Generation: What's Decided, What's Not Yet Built

This document exists to capture everything discussed about the procedural room
generation rework — decisions already made, the reasoning behind them, and
exactly what's missing from the current codebase versus what the design
requires. Nothing in this document is code. It's the plan the code should
eventually match.

Context: the game must stay under a 1.44 MB total size budget, which rules
out an Isaac-style library of thousands of hand-authored rooms. The
alternative is a small set of terrain types, generation rules, and
interaction rules that combine to *feel* like a huge room library while
actually costing kilobytes, not megabytes.

---

## 1. Where the current code already stands

Before listing what's missing, it's worth being precise about what
`dungeon.cpp` / `room.h` already do, so nothing gets rebuilt by accident.

**Already implemented:**
- Room identity: `RoomType` (START/NORMAL/BOSS/TREASURE/CURSE/SHOP),
  `RoomArchetype` (OPEN_ARENA/PILLAR_FIELD/BROKEN_ARENA/GAUNTLET/
  HAZARD_ROOM/RITUAL_ROOM), `RoomEncounterFamily` (RUSH/ARTILLERY/SWARM/
  GUARDIAN/AMBUSH/MIXED/ELITE), `DungeonTheme` (RUINS/FORGE/CRYPT/FUNGAL/
  DRACONIC).
- A unified terrain representation: `RoomTerrainFeature` with a `type`
  enum (`RoomTerrainType`), plus helper methods `BlocksMovement()`,
  `IsBombable()`, `IsTrap()`, `IsHazard()`. This is already the right shape
  — a small number of reusable behaviors rather than one class per object —
  it just doesn't cover enough categories yet (see section 4).
- Room-graph growth via random walk from a START room, with a soft
  per-room `targetDegree` bias and a hard second pass (`BuildConnections`)
  that wires actual N/S/E/W connections between grid-adjacent rooms.
  Growth is retried (bounded, `MAX_GENERATION_ATTEMPTS` = 64) at the whole
  *floor* level if it can't hit its room-count target.
- Special room placement: BOSS/TREASURE/CURSE/SHOP are always assigned to
  dead-end (degree-1) rooms; BOSS is specifically the dead end furthest
  (by BFS distance) from START.
- Theme-driven bonuses: each `DungeonTheme` currently applies generic
  *amount* multipliers (rock/pit/trap/special-enemy/reward bonuses), but
  does **not** yet change *which* terrain types get selected per theme.
- Hazard *selection* (not placement) is randomized: rocks are chosen from
  8 fixed cluster layouts, traps from 8 fixed spots, pits from 5 fixed
  spots, all filtered against a single fixed "player core" no-spawn rect
  via rect-overlap checks. This candidate-list + exclusion-rect pattern is
  exactly the mechanism the new door-safety-zone and combat-space rules
  will extend, not replace.
- A known cleanup item: `Room::rocks` and `Room::traps` (the older,
  type-specific vectors) are populated in parallel with `Room::terrain`
  (the current unified vector) but are never read anywhere in `main.cpp`.
  They're dead storage and should be removed once the terrain rework
  starts touching this code anyway.

**Explicitly not implemented yet** (this is what the rest of this document
is about):
- Any door-to-door connectivity guarantee.
- Any door entry safety zone.
- Any enemy-to-door minimum distance rule.
- Any combat-space / open-area validation.
- Any terrain interaction system (push, explode, trigger, chain-react).
- Any new terrain types beyond the current 8 (3 rock variants, pit, 4 trap
  variants).
- Any per-room threat budget tying enemy count and hazard count together.
- Any stable ID system for terrain features.
- Theme-based terrain *type* selection (only amount bonuses exist today).

---

## 2. The core design philosophy (why this approach, restated)

The goal is: **make procedurally generated rooms feel intentionally
designed, not randomly populated** — and get that from combining a small
set of rules, not from storing thousands of layouts. This is the same
instinct already present in the archetype/family/theme system; the new
work extends that instinct into terrain placement and validation instead
of stopping at room *identity*.

The generation flow, as agreed, has five conceptual layers, always in this
order, each one constrained by everything before it:

1. **Guaranteed playable structure** — doors, door safety zones, guaranteed
   connectivity, minimum combat space. Nothing after this layer is allowed
   to violate it.
2. **Terrain** — rocks, pits, breakables, explosives, pushables, slow/damage
   terrain. Must respect layer 1.
3. **Interactions** — rules that let terrain objects affect each other
   (push fills pit, explosion destroys breakable, switch triggers,
   teleport pads link).
4. **Combat** — enemy types, positions, traps, encounter composition. Must
   respect layers 1–3.
5. **Validation** — check everything, reject and reroll (bounded retry) if
   any hard rule is violated.

---

## 3. Hard rules (non-negotiable invariants)

These are the rules a generated room must *always* satisfy. Each is stated
here with the reasoning, since the reasoning is what prevents future
"but this edge case seems fine" mistakes.

### Rule 1 — All active doors must be connected
There must be a walkable route between every pair of active doors
(North↔South, East↔West, and diagonal combinations where applicable), and
that route must work **immediately** — no bombs, no shooting an obstacle,
no switch, no lily pad, no timer, no teleporter. Those mechanics may create
*additional, optional* routes, but never the *only* route.

### Rule 2 — No softlocks
The player must never become permanently trapped: no unreachable doors, no
permanently isolated areas, no one-way terrain that traps the player, no
required route that depends on a cooldown or a consumable/temporary
mechanic. Optional mechanics are allowed to fail or disappear — the *main*
path must always remain valid regardless.

### Rule 3 — Door entry safety zones
Every active door has a small protected zone around it. Inside that zone,
none of the following are allowed: enemies, indestructible rocks, pits,
dangerous traps, blocking objects, explosive terrain, or any other
immediate hazard. The player must always be able to walk into a room
safely, with zero risk in the first moment after entry.

### Rule 4 — Enemies cannot spawn adjacent to doors
Enemy spawn positions must respect a minimum distance from every active
door — not just "outside the safety zone," but far enough that the player
can't be hit essentially the instant they walk in. This prevents unfair
instant damage on room entry.

### Rule 5 — Minimum combat space
Connectivity alone isn't sufficient — a room could be "technically
connected" through a single-tile-wide snake path and still be an unfair
combat space. There must be enough open, *connected* area for dodging and
fighting. How much "enough" means scales with archetype, encounter family,
enemy type, and floor/difficulty (concrete starting numbers are in section
6).

### Rule 6 — Main path vs. optional paths
Every room has two conceptual kinds of traversal:
- **Main path**: connects required doors, always available, requires no
  special action or item.
- **Optional paths**: may use breakables, bombs, pushable blocks, lily
  pads, teleporters, bridges, switches, or timed mechanics. These can lead
  to shortcuts, loot, tactical positions, or secret/reward areas — but
  must never be required for basic traversal between doors.

### Rule 7 — Prevent unfair terrain/enemy combinations
The generator must evaluate *combinations*, not just individual objects in
isolation. Examples of bad combinations: too many chargers in a narrow
terrain layout; excessive ranged enemies positioned behind terrain the
player can't reach or see past; pits + traps + movement-heavy enemies with
no dodge space; explosive terrain placed right at room entry; dangerous
terrain surrounding the player's likely position. Enemy composition has to
be evaluated against the *generated geometry*, not chosen independently of
it.

### Rule 8 — Threat budget
A room should have one overall difficulty/threat budget that enemy count,
enemy strength, trap count, pit count, and blocking-terrain amount all draw
from — not each maximized independently. A hard room should be hard
*intentionally* (e.g. many weak enemies + simple terrain, OR few strong
enemies + complex terrain, OR moderate enemies + significant hazards) —
never an RNG accident where every axis rolls its maximum at once.

**This is a real gap in the current code.** Enemy spawn count today comes
from `base + perFloor*floor + deep-room bonus + family bias + theme
bonus`, and hazard count comes from a separate
`archetype/theme/floor`-scaled roll — two independent random rolls with no
shared ceiling. Tying these together is one of the larger pieces of new
design work, not just new code — the actual point-costing formula (how many
"threat points" does one ELITE-tier enemy cost vs. one indestructible rock
vs. one trap) doesn't exist yet and needs to be designed before it can be
implemented.

---

## 4. Terrain type inventory

### 4.1 Currently implemented (8 types, in `RoomTerrainType`)
| Type | Blocks movement | Destructible | Notes |
|---|---|---|---|
| `ROCK_INDESTRUCTIBLE` | yes | no | permanent cover/chokepoint |
| `ROCK_BOMBABLE_COIN` | yes | bomb only | drops a coin reward when destroyed |
| `ROCK_BOMBABLE_HEART` | yes | bomb only | drops a heart reward when destroyed |
| `PIT` | yes (unless flying/pit-crossing) | no | permanent terrain separation |
| `TRAP_POISON` | no (walkable, triggers) | n/a | damage-over-time effect |
| `TRAP_TELEPORT` | no (walkable, triggers) | n/a | hostile/unwanted teleport |
| `TRAP_SUMMON` | no (walkable, triggers) | n/a | summons additional enemies |
| `TRAP_SPIKE` | no (walkable, triggers) | n/a | direct positional damage |

### 4.2 Planned new types, by priority tier

**Tier S (highest priority):**
- **`ROCK_BOMBABLE` (plain, new)** — blocks movement, destroyed only by a
  bomb, gives **no reward** (no coin, no heart). This matters because it
  lets a room use destructible terrain purely as an obstacle, without
  every bombable rock implying "there's loot here" — currently every
  bombable rock variant in code has a reward attached, so this is a new
  option, not a variant of an existing one.
- **Explosive barrels / explosive rocks** (`EXPLOSIVE_BARREL` /
  `ROCK_EXPLOSIVE`) — blocks movement, can be shot/detonated, explodes in
  an area dealing damage to enemies (and potentially the player), can
  destroy compatible breakable terrain in range, and can chain-react with
  other explosives in range. This is the single highest-value addition
  because it's the first terrain type that creates real terrain↔combat
  interaction rather than just being a static obstacle.
- **Pushable blocks** (`BLOCK_PUSHABLE`) — blocks movement, the player can
  push it in the direction they approach from, changing room geometry
  dynamically. Interactions: can be pushed into a pit (filling it), can
  create temporary cover, can block enemy movement, can open or close
  tactical routes. Must never be *required* to maintain basic door
  connectivity (see Rule 6 and the baseline-validation discussion in
  section 5).
- **Slow terrain** (`TERRAIN_WEB`, `TERRAIN_SLIME`, `TERRAIN_MUD`) —
  walkable, but slows movement speed while standing on it.
- **Damage terrain** (`TERRAIN_FIRE`, `TERRAIN_ACID`, `TERRAIN_POISON`) —
  walkable or conditionally walkable, damages entities standing/moving on
  it. Together with slow terrain, this establishes a terrain taxonomy of
  four distinct movement categories: walkable / walkable-but-dangerous /
  walkable-but-slow / blocking.
- **Terrain interaction rules** — this is conceptually more important than
  adding more independent objects. The goal is *a small number of terrain
  objects with many interactions between them*, not many single-purpose
  objects. Concrete examples: explosion destroys breakable objects;
  pushable block fills a pit; a traversal object (bridge/lily pad) changes
  whether a pit is crossable; a switch activates another terrain feature;
  explosions can chain-react with other explosives.

**Tier A:**
- **Destructible crates / shoot-breakable obstacles**
  (`OBSTACLE_BREAKABLE`, `CRATE_DESTRUCTIBLE`) — blocks movement, destroyed
  by player shots (not bombs). Later variants could differ by material:
  wooden (low health), stone (higher health), themed/cursed (special
  behavior on break).
- **Teleport pads** (`TELEPORT_PAD`) — distinct from `TRAP_TELEPORT`: a
  teleport pad is *intentionally* used by the player, not a hostile
  effect. Two or more pads link to each other (see the linkId system,
  section 5.4) to connect two locations in a room — for shortcuts or
  tactical repositioning. Never required for guaranteed door connectivity.
- **Switches / pressure plates** (`PRESSURE_PLATE`) — can trigger bridges,
  remove barriers, arm/disarm hazards, or unlock optional reward areas.
  Particularly useful for puzzle-style or special rooms.
- **Temporary / fragile bridges** (`BRIDGE_FRAGILE`, `BRIDGE_TEMPORARY`) —
  cross pits temporarily; possible mechanics include limited uses,
  breaking after crossing, or existing only for a limited time. Should
  normally serve optional paths, shortcuts, or tactical routes — never the
  only required route between doors.

**Tier B (later, lower priority — noted for completeness, not near-term
work):**
- Moving terrain/hazards (moving spikes, rolling objects, sliding
  barriers, rotating hazards).
- One-way terrain (one-way gates, ledges) — requires more advanced
  directional connectivity validation than a simple undirected BFS.
- Ice/sliding physics (`TERRAIN_ICE`, movement continues until hitting an
  obstacle or a stopping tile) — requires specialized room validation
  beyond what's planned for the initial pass.
- Complex concealment mechanics (tall grass, fog, concealment zones,
  hidden enemies/hazards) — must be designed carefully to avoid unfair
  hidden damage; flagged as needing extra care, not just extra code.

**Lily pad system** (`LILY_PAD`) — used alongside pits as conditional
traversal. Possible behavior: one-use crossing, temporary activation, or a
cooldown/regeneration cycle. Recommended use: optional shortcuts, tactical
escape routes, reward areas. As with bridges, never the only mandatory
path, and the exact timing/usage rule should stay simple enough that a
player understands it immediately without explanation.

### 4.3 Terrain category taxonomy (conceptual, not new enum values yet)
The unified terrain system should conceptually support these behavior
categories, composed rather than duplicated per object:
- **Movement**: WALKABLE / BLOCKING / CONDITIONAL
- **Destruction**: INDESTRUCTIBLE / BOMBABLE / SHOOT_BREAKABLE / EXPLOSIVE
- **Effects**: HAZARD / SLOW / DAMAGE / TELEPORT
- **Interaction**: PUSHABLE / FILLS_PIT / TRIGGERS / CHAIN_REACTS
- **Lifetime**: PERMANENT / TEMPORARY / ONE_USE / DESTRUCTIBLE

The architectural principle behind this list: **do not build an entirely
separate terrain system for every new object.** A new terrain "thing"
should be describable as a combination of these categories rather than a
bespoke class or bespoke logic path. This is the same principle already
proven out by `RoomTerrainFeature`'s existing helper methods — the goal is
to extend that pattern, not abandon it.

### 4.4 Theme-based terrain selection (not yet implemented)
Themes should influence *which* terrain types appear, not just *how many*
(the current implementation only does amount-scaling, not type
selection):
- **RUINS**: rocks, destructible debris, pits, old/worn traps.
- **FORGE**: fire terrain, explosive objects, industrial hazards, hot
  ground.
- **CRYPT**: poison, cursed terrain, traps, destructible bones/debris.
- **FUNGAL**: slime, slow terrain, poison terrain, fungal growth.
- **DRACONIC**: stronger terrain combinations generally, explosive
  hazards, fire, more difficult combat geometry.

This gives visual and gameplay variety per theme without needing separate
room files — consistent with the overall "rules produce variety, not
storage" philosophy.

---

## 5. Validation architecture (the part most likely to be built wrong on a first pass)

### 5.1 Baseline vs. dynamic terrain — the critical distinction
Terrain needs to be thought of in two layers:

- **Layer A — Baseline terrain**: determines whether the room is
  fundamentally playable *right now*, at the instant of entry. Includes
  normal floor, permanent open paths, indestructible walls/rocks, pits,
  and any other blocking object.
- **Layer B — Optional/dynamic terrain**: bombable rocks, shoot-breakable
  crates, explosive barrels, pushable blocks, bridges, lily pads, teleport
  pads, switches, temporary terrain. These can create shortcuts, optional
  routes, rewards, or tactical changes — but they can never be the thing
  that makes an otherwise-invalid room valid.

**The golden rule, stated precisely:** *the guaranteed door-to-door
connectivity check must be performed against the room's initial state,
before any optional, destructible, movable, temporary, or interactive
terrain has been used or modified. A room that only becomes connected
after using an interaction is invalid.*

### 5.2 Why this distinction is necessary (not just pedantic)
Without it, a naive connectivity check — especially a "place hazards, then
try to path between doors" BFS-with-retry approach — could accept a room
where a pit sits directly across the only route between two doors, with a
pushable block positioned nearby, on the reasoning that *some* sequence of
player actions makes the doors reachable. That's a different (weaker)
guarantee than what Rule 1 actually promises: a route available with zero
preconditions, from the moment the player walks in.

Concrete invalid example:
```
DOOR ─── 🕳️ PIT ─── DOOR
         📦
   Push block here
```
Even though the player *can* push the block into the pit and then cross,
this is invalid — at the moment the player enters, Door A cannot reach
Door B. Pushability doesn't rescue it.

Concrete valid example (pit as an optional side-challenge, not a blocker):
```
DOOR ─────────────── DOOR
          │
          │
        🕳️ PIT
          │
       💰 LOOT
```
Here the main route (top, straight across) is already open regardless of
the pit; the pit only gates optional loot below it.

### 5.3 What counts as "blocking" during baseline validation
This is the sharpest, easiest-to-miss part of the whole system. During
baseline validation, the following **all** count as blocking, with no
exceptions for "but it's destructible":
- `PIT`
- `ROCK_INDESTRUCTIBLE`
- Bombable rocks (all variants, including the new plain `ROCK_BOMBABLE`)
- Shoot-breakable crates
- Explosive barrels
- Pushable blocks that haven't been moved yet

The reasoning: at the instant the player enters the room, a bombable rock
and an indestructible rock are physically identical obstacles. Only their
*eventual* removability differs — and eventual removability is exactly
what makes something Layer B / an optional path, not a reason to treat it
as already-passable during the one check that's supposed to guarantee
immediate playability.

**Good news for implementation**: `RoomTerrainFeature::BlocksMovement()`
already exists and already returns exactly the right set for this
purpose — it doesn't care about bombability, only about "does this block
movement right now." The baseline validator can and should use this
existing method unmodified rather than inventing a parallel concept of
blocking.

### 5.4 Stable terrain feature IDs
Once switches, teleport pads, and bridges exist, terrain features need to
reference *other* terrain features (a switch needs to know which bridge it
controls; a teleport pad needs to know its paired destination pad).
Referencing by `vector` index is fragile, because during generation,
terrain can be inserted, rejected, removed, or the whole layout can be
rerolled — any of which can silently reshuffle indices.

**Decision**: every `RoomTerrainFeature` gets a small stable `featureId`
(unique within its room) at creation time, plus an optional `linkId` that
groups related features. Example: two paired teleport pads both get the
same `linkId`; a switch and the bridge/obstacle it controls share a
`linkId`. This should be added now, before any Tier A linked feature is
built, even though most features won't use `linkId` yet — retrofitting IDs
onto an existing terrain vector later is much more disruptive than adding
an always-present field now. IDs should be scoped per-room (reset per
room, not globally unique across the floor) since nothing currently
described needs a link to cross room boundaries. Given the small terrain
counts per room, both `featureId` and `linkId` can be small integer types
(no need for anything like a GUID).

### 5.5 The full validation pipeline, in order
```
Generation-time:
  Room Identity (type, archetype, encounter family, theme, threat budget)
        ↓
  Doors (read connectivity: north/south/east/west)
        ↓
  Door Safety Zones (reserve protected entry areas)
        ↓
  Baseline Main Path (reserve/guarantee a route between all active doors)
        ↓
  Terrain Generation (place rocks/pits/breakables/explosives/slow/damage/etc,
                       respecting all reservations above)
        ↓
  Baseline Grid BFS (Validation A — connectivity, using only Layer A / blocking terrain)
        ↓
  Combat Space Validation (Validation C — total open % + largest connected region,
                            measured EXCLUDING reserved door-zone cells)
        ↓
  Terrain Interaction Validation (Validation D — do links/targets/destinations make sense)
        ↓
  Enemy Generation (respecting door-adjacency rule and threat budget)
        ↓
  Final Validation (all checks together)
        ↓
  Accept / Reject-and-Reroll (bounded retry count, same pattern as the
                               existing MAX_GENERATION_ATTEMPTS floor-level retry)

Runtime (after the room is accepted and entered):
  Dynamic terrain changes (push / destroy / explode / activate)
        ↓
  Runtime safety restrictions (e.g. reject a push that would seal the only
                                remaining route through a corridor)
        ↓
  Guarantee: dynamic interactions can never create what generation-time
             validation was designed to prevent — a permanent softlock
```

Note the explicit ordering constraint: combat-space validation must run
*after* door safety zones are carved out, and must measure open area
excluding those reserved zones — otherwise a room could hit its target
open-percentage using space that's actually mandatory door buffer, leaving
the *real* usable combat area smaller than the validation thinks.

### 5.6 No duplicate frozen snapshot needed
It might seem like guaranteeing "the room was valid at entry, forever"
requires storing a second, permanent copy of the original terrain layout
to compare against. **Decision: don't do this** — it's unnecessary
duplication and directly works against the size budget. Instead:
- Baseline validation happens once, during generation, before the room is
  ever accepted.
- Once accepted, the generated terrain *is* the baseline — there's no need
  for a second copy unless the game ever needs to fully reset a room to
  its original state (not currently a planned feature).
- The generation-time guarantee (room was valid when first entered) and
  the runtime guarantee (dynamic interactions can't destroy that validity
  later) are treated as two separate responsibilities with two separate
  enforcement points, not one mechanism trying to do both.

### 5.7 Validation grid mechanics
- **Grid size**: 16×9 logical cells over each 320×180 pixel room, giving
  20×20 pixel cells. This grid exists purely for generation-time and
  runtime-safety validation — it does not replace or change the
  continuous-space rendering/movement/collision system already in place.
- **Cell blocking rule**: a cell should not be marked blocked just because
  a terrain object's *center point* happens to fall inside it. The check
  needs to account for the terrain object's actual collision bounds and
  the player's collision size, so that a gap which technically exists
  between two rocks but is physically too narrow for the player is
  correctly treated as unusable. The simplest correct implementation is
  "any part of a blocking terrain rect overlaps this cell" (conservative —
  may cause some unnecessary regeneration retries, but is far simpler than
  a percentage-of-cell-covered approach, and the existing retry loop
  already absorbs some rejected layouts as a matter of course).
- **Runtime reuse**: this same grid representation is needed again at
  runtime, not just at generation time — specifically to validate pushable
  block moves before allowing them (a push must be rejected if it would sever
  the room's only remaining route). Given the size budget, rebuilding the
  grid on-demand from the current `terrain` vector at the moment a push is
  attempted is preferable to keeping a second persistent grid buffer alive
  for every active room — pushes aren't a hot/frequent operation, so the
  extra CPU cost of rebuilding is cheap, and it avoids having two
  potentially-diverging sources of truth for walkability.

---

## 6. Combat space validation, in detail

Two checks are used together, not either one alone:

- **A. Minimum total walkable percentage** — the room must have enough
  open area overall, relative to the 16×9 = 144-cell validation grid
  (excluding reserved door-zone cells).
- **B. Minimum largest connected walkable region** — there must be *one*
  sufficiently large connected open area, not just a high total percentage
  spread across small disconnected pockets (which would satisfy check A
  while still being an unfair, chopped-up combat space).

**Starting percentage targets by archetype** (of the 144-cell grid,
excluding door zones):

| Archetype | Target open % |
|---|---|
| OPEN_ARENA | 65–80% |
| PILLAR_FIELD | 55–70% |
| BROKEN_ARENA | 50–65% |
| GAUNTLET | 45–60% (must additionally preserve clear movement lanes, not just hit a raw percentage) |
| HAZARD_ROOM | 50–65% physically walkable (note: dangerous-but-walkable terrain like damage/slow tiles further reduces *safe* space beyond this number, which is a separate consideration from raw walkability) |
| RITUAL_ROOM / BOSS | no single universal percentage — see below |

**Encounter family modifiers** (applied on top of the archetype target):
- RUSH / SWARM — need more open connected space and wider routes than the
  archetype baseline alone would suggest.
- ARTILLERY — can tolerate more cover and more terrain separation.
- GUARDIAN — can support structured obstacles and chokepoints.
- AMBUSH — can use denser terrain and hiding/approach structures.
- ELITE — depends heavily on the specific elite enemy in question; no
  single rule, needs case-by-case judgment when specific elites are
  designed.

**Boss rooms — no universal percentage.** Instead of the framing "boss
rooms can be tighter/looser because they're special," the correct framing
is: **boss rooms have boss-specific spatial requirements**, determined by
what that particular boss actually does. Examples:
- A charging boss needs long, clear lanes for its charge to be dodgeable.
- A bullet-hell boss needs a large, mostly open arena.
- A summoner boss needs enough spare space for its summoned adds to have
  room to spawn and act.
- A stationary boss can afford a more obstacle-dense arena, since it isn't
  covering ground itself.

This means boss combat-space requirements will need to be attached to
boss *design* (per boss or per boss-behavior-category), not derived from
the generic archetype table above.

---

## 7. Special room type rules

All special room types (START, SHOP, TREASURE, CURSE, BOSS) follow the
**same** baseline safety rules as normal rooms — connectivity, no
softlocks, door safety zones. What differs between them is combat/hazard
*density*, customized to each room's purpose:

- **START**: strictest of all — clear entry/exit, generous open space,
  minimal hazards. This is the player's first look at the floor; it
  shouldn't test them.
- **SHOP**: also strict — no dangerous terrain near the entry, plenty of
  navigation space, no hostile clutter. A shop should read as unambiguously
  safe.
- **TREASURE**: baseline connectivity rules still apply in full — the room
  itself is always safely, immediately accessible — but *optional* terrain
  can guard *extra* rewards (e.g. a breakable object standing between the
  open area and a bonus item). The core room and its primary reward are
  never gated.
- **CURSE**: same baseline connectivity and no-softlock rules as
  everything else, but can be visually and mechanically denser — more
  hazards, stranger terrain combinations, tighter combat space — while
  still respecting every hard rule. "Cursed" should read as *harder*, not
  *unfair*.
- **BOSS**: follows door safety, baseline walkability, and no-softlocks
  like everything else, but its combat-space requirement is boss-specific
  rather than archetype-generic (see section 6).

---

## 8. What needs to be designed (not just coded) before implementation starts

These are open design questions, distinct from the architectural decisions
already made — things that need a concrete answer, not just an approach,
before code can be written against them:

1. **The threat-budget formula.** How many "threat points" does one
   ELITE-tier enemy cost, versus one indestructible rock, versus one trap,
   versus one pit? This doesn't exist yet in any form and is a genuine
   game-design task, not an implementation task — the shape of the
   solution (a shared point pool that both enemy selection and hazard
   density draw from) is agreed, but the actual numbers aren't.
2. **The exact `RoomTerrainFeature` field set.** The current struct
   (`pos`, `w`, `h`, `type`, `rewardAmount`, `broken`, `triggered`) models
   static, single-state terrain. Layer B terrain needs fields this struct
   doesn't have yet:
   - A distance/duration or "currently walkable" state for temporary
     bridges (which start walkable and *become* blocking — the inverse of
     a rock's broken/unbroken transition).
   - A "consumed" flag for one-use lily pads (semantically distinct from
     `broken`, since a lily pad isn't destroyed by damage, it's used up by
     traversal).
   - A destination reference for teleport pads and a target reference for
     switches — this is what `featureId`/`linkId` (section 5.4) are
     designed to solve, but the exact usage pattern (does a switch store
     one `linkId` or a small list?) still needs deciding.
   - A hit-counter instead of a boolean for crates/barrels with more than
     one hit of health (generalizing `broken` from a flag into a derived
     check against a remaining-hits count).
   - A cooldown/timer field for anything that re-arms (some traps,
     pressure plates).
   This is explicitly called out as the next concrete step to lock down
   before any Tier S/A terrain type gets implemented, so the struct isn't
   redesigned repeatedly as each new feature gets added.
3. **Push-safety rules at runtime**, precisely. "Don't allow pushing a
   block into the only main corridor" is the stated principle, but the
   exact check (rerun the 16×9 BFS after a hypothetical push, before
   committing it, and reject the push if connectivity breaks) needs to be
   confirmed as the actual mechanism, and needs to account for the same
   baseline-vs-dynamic distinction discussed in section 5 — i.e., does a
   push-safety check need to know about *other* dynamic terrain that's
   already been used, or does it only care about currently-blocking
   terrain at the moment of the push attempt?
4. **Interaction rule specifics** — for each stated example (explosion
   destroys breakable, pushable fills pit, switch activates linked
   feature, chain reactions), the *range*/*radius*/*conditions* under
   which each interaction applies still need concrete numbers, not just
   the concept.

---

## 9. Suggested next step

Given everything above, the natural next unit of work is section 8, item
2 — locking the exact `RoomTerrainFeature` field set — since nearly
everything else (Tier S terrain implementation, the interaction system,
the linkId usage pattern) depends on that struct being settled first
rather than redesigned incrementally as each new terrain type gets added.

---

## 10. Which algorithm each layer should use

This is the clean decision summary for the generation stack:

| Layer | Needs an algorithm? | What to use | Why |
|---|---|---|---|
| Floor layout (room graph, outside the 5-layer room pipeline) | Already solved | Existing random-walk + degree-targeting | This is already closer to good BSP-alternative behavior than BSP itself would give you. Do not replace it with CA, WFC, or Drunkard's Walk. |
| Layer 1 - Guaranteed playable structure (doors, safety zones, connectivity, min combat space) | No | Pure geometry + the 16x9 BFS grid | This is a validator, not a generator. BFS/flood-fill is the whole algorithm here. |
| Layer 2 - Terrain | Yes, but only for some archetypes | Split by archetype | This is the only layer where "which algorithm" is a real choice. |
| Layer 3 - Interactions | No | Rule-based `linkId` system (switch -> target, pad -> pad) | This is relationship data, not spatial generation. |
| Layer 4 - Combat (enemy selection/placement) | No | Existing tier/family-biased candidate sampling + threat budget | This is selection-from-pool plus a budget constraint, not a spatial layout problem. |
| Layer 5 - Validation | No | BFS/flood-fill on the same grid as Layer 1, plus area/percentage checks | Deterministic checking, not generation. |

So the only real algorithm decision is Layer 2, and it splits by archetype:

| Archetype | Algorithm | Reasoning |
|---|---|---|
| `OPEN_ARENA` | Rule-based motif placement | Wants a legible, readable layout. |
| `PILLAR_FIELD` | Rule-based motif placement | Structured, evenly spaced obstacles read better than noise. |
| `GAUNTLET` | Rule-based motif placement, but chained/linear | A gauntlet should feel like a readable sequence of obstacles, not chaos. |
| `RITUAL_ROOM` / `BOSS` | Rule-based, boss-specific spatial requirements | The room has to match the boss's movement pattern, so this stays hand-tuned per boss. |
| `BROKEN_ARENA` | Cellular automata pass | This is the archetype where organic rubble/debris makes sense. |
| `HAZARD_ROOM` | Cellular automata pass for rocks/pits only | Traps stay rule-based, since they are point placements, not area fill. |

The two algorithms that do not fit this game are also worth stating plainly:

- WFC maps to no layer here. Its cost/complexity is not justified because the rule-based motif system already gives the same benefit for far less code.
- Drunkard's Walk maps to no layer here. It is a cave/corridor carving tool, which does not fit a fixed 320x180 rectangular room.

What the CA pass will actually do in practice:

1. Seed the non-reserved cells on the 16x9 validation grid as either solid or open, using an initial fill probability.
2. Run 3 to 5 smoothing passes, where each cell becomes solid or open based on how many neighboring cells are solid.
3. Re-clear reserved zones after smoothing, especially door safety zones and any connectivity spine cells.
4. Convert the final solid cells into actual terrain features, usually rocks, and sometimes pits depending on archetype and theme.
5. Run the existing BFS connectivity and combat-space validation. If the result fails, reroll the CA seed and try again, up to the normal bounded retry limit.

Implementation status:
- `BROKEN_ARENA` and `HAZARD_ROOM` now use the CA path in code.
- The other terrain archetypes still use the existing motif/candidate-slot placement path.
- Trap placement remains rule-based for every archetype.
- Room layouts are validated against the 16x9 grid and regenerated if the layout is invalid.
