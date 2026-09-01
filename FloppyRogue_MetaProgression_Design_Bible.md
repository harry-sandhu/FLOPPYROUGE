# FloppyRogue — Meta-Progression & Unlock System Design Bible

**Status:** Design/analysis only. Nothing in this document has been implemented. No file has been modified, no code has been written, no data file has been changed.

**Audit basis:** This document is built entirely from the actual current repository content — `data/items.txt`, `data/enemies.txt`, `data/bosses.txt`, `data/themes.txt`, `data/rooms.txt`, and the C++ that consumes them (`game/dungeon/dungeon.cpp`, `game/rooms/room.h`, `game/items/item_database.cpp`, `game/items/item_system.cpp`, `game/bosses/boss_database.*`, `game/enemies/enemy_database.h`). Old planning docs in the repo root (`v2.md`, `inital.md`, `prgess.md`, `lastplan.md`, `currentplan.md`, `reamaingwrok.md`, `implementation_plan.md`, `agent.md`, `enemy_roster_plan.md`, `command.md`, `PROGRESS.md`) were intentionally **not** used as sources of truth — several of them describe systems and numbers that no longer match the code (e.g. `dungeon.h`'s default `totalFloors = 5` while `data/rooms.txt` actually sets `total_floors=8`). Every number, name, and mechanic below was verified against the live files.

---

## Part 0 — Executive Summary

FloppyRogue currently has **no meta-progression, no save/profile system, and no achievement system of any kind.** A grep across the whole codebase for `save`, `profile`, `achievement`, and `unlock` turns up nothing except the *item* mechanic already called `type=unlock` (which just means "grants a permanent run-flag like `dash` or `homing`" — it has nothing to do with cross-run unlocking). Every item, every enemy, every boss, and every floor is reachable in the very first run a new player starts. This is the blank slate the user asked to design against.

While auditing the actual data and code, five architectural facts turned up that should **drive the entire design**, because they are pre-existing hooks the game already has — we don't need to invent a new gating system, we need to fill in gates that already exist mechanically but are currently wide open (or, in a couple of cases, accidentally starved):

1. **Per-theme enemy pools already exist, are already the only thing that matters, and are currently tiny.** `dungeon.cpp`'s enemy-selection code always prefers the current theme's `enemy_pools` list over the full tier-eligible roster — and only falls back to the broad roster if the theme's list resolves to *zero* valid names. Right now `RUINS` (Floor 1) resolves to just **Zombie, Gunner, Fly** (a fourth name, `Spider`, doesn't exist in `enemies.txt` and is silently dropped). `FORGE` (Floor 2) resolves to **BombKnight, Warlord, JuggernautPrime, HexMatron**. `CRYPT` (Floor 3) resolves to **Mimic, BlightGrub, HexMatron** (`Reaper` doesn't exist). `FUNGAL` (Floor 4) resolves to **BlightGrub, VenomEye, SplitterLord, SnareTurret**. This is out of a total roster of **167 enemies**. The overwhelming majority of the enemy roster currently never spawns on Floors 1–4 at all, purely because the theme lists are short.
2. **`DRACONIC` (Floors 5–8) is broken in a way that happens to be useful.** Its `enemy_pools` line lists `Ravager, Stormeye, Railwing, Titan, Eclipse` — every one of those is a **boss** name, not an enemy name. None of them exist in `enemies.txt`. Because the themed list resolves to zero valid entries, the game's own fallback kicks in and Floors 5–8 draw from the **entire tier-eligible enemy roster** instead. So today, ironically, the "end-game" floors have full enemy variety and the "early game" floors are starved. This is a genuine content bug worth flagging to the user, and it's also the single cleanest natural lever for a Floor-cap-based unlock system: raising the floor cap can be paired with deliberately building out `DRACONIC`'s enemy pool for the first time, rather than leaving it to an accidental fallback.
3. **Boss pools work the same way, and 16 of the game's 36 bosses are never selected by any theme today.** Every theme's `boss_pools` line is well-formed (unlike the enemy lists), so the game's `PickBossVariantForTheme` always finds real boss names and never falls back to the full tier list. Across all five themes' `boss_pools`, only **20 of 36 bosses** are ever referenced: `Runt, Pinwheel, Spinner, GlassKing, Patroller, IronSaint, Graveforge, Emberlord, Coward, Grief, Hollowqueen, Voidcrown, Linker, Mire, Spite, Harvester, Nullsire, SwarmLeader, DragonSovereign, Eclipse`. The other **16 — `Hexcaller, Bulwark, Ravager, Stormeye, Mirrorfiend, Siren, Burrower, Railwing, Riftmother, Titan, WebMother, Chorus, RedWarden, Ashcaller, Frostgrip, Duskfang` — are fully implemented (stats, attack cycles, phases) but structurally unreachable in the current game.** This is, functionally, a ready-made "secret boss" pool sitting in the code waiting for a reason to exist. It is the centerpiece of the boss-unlock design below.
4. **Item tiers are already floor-gated per chest/reward type**, via `PickItemForRoom(pools, minTier, maxTier)` calls in `dungeon.cpp`. Wooden chests already cap at tier `min(3, 1+floor)`, Devil chests already start at tier 3, Angel chests at tier 4, and so on. The five item pools (`TREASURE`, `SHOP`, `CHEST`, `BOSS`, `CURSE`) and five tiers (1–5) are a complete, working weighting system. This means an unlock system for items doesn't need new mechanics — it needs a **sixth axis**: "is this item currently a member of its pool(s) at all," which is exactly how Isaac's `itempool.xml` unlock mechanic works (Isaac removes/adds items from `<Item Name="..." Weight="...">` pool listings via the completion-mark save file). We can do the same thing to `pools=` in `items.txt`.
5. **Room archetype variety is already floor-gated, informally.** `RollArchetype()` only rolls from the original 6 archetypes (`OPEN_ARENA, PILLAR_FIELD, GAUNTLET, HAZARD_ROOM, BROKEN_ARENA`, plus `RITUAL_ROOM`/`CURSE` specials) on Floors 1–2. The 14 newer archetypes (`WHISPERING_STACKS, RUNIC_LATTICE, FORKING_PATH, SPIRE_ASCENT, FLOODED_CHAMBER, COLLAPSED_VAULT, SENTRY_HALL, GARDEN_MAZE, SHATTERED_BRIDGE, ECHO_ROOM, THRONE_APPROACH, TWIN_ISLANDS, NARROW_VEINS, AMPHITHEATER`) only start appearing (at low weight) on Floor 3, and become the majority of what's rolled from Floor 4 onward. This split lines up almost exactly with the Floor-3/Floor-5/Floor-8 cap structure the user asked for, and is a natural (if currently soft) precedent for hard-gating it by save progression instead of just floor number.
6. **One chest type is fully implemented and never used.** `ChestType::GAMBLE` has full render/color/behavior code in `main.cpp` and a full item-pool rule in `dungeon.cpp` (`PickChestItem`: pulls from `TREASURE,BOSS,SHOP,CHEST` at any tier 1–5 — the single best chest odds in the game), but `RollChestTypeForFloor()` — the only function that decides which chest type actually spawns — never returns `GAMBLE`. It is dead content today. It is a perfect, zero-new-code "secret chest type" unlock.

Everything below is built around these six facts. The core mechanic recommended throughout this document is **not** "give the player a locked item screen." It's the same mechanic Isaac itself uses: **unlocking edits the pool membership strings that the generator already reads** (`items.txt` → `pools=`, `themes.txt` → `enemy_pools=` / `boss_pools=`, plus a small number of new gating fields described in Part 8). The generator code does not need to change; only the data it's fed, plus a small save/progress layer, needs to be added.

---

## Part 1 — Complete Content Inventory (What Exists Right Now)

### 1.1 Items — 168 total

Parsed directly from `data/items.txt`. Every item has a `type` (`stat_mod`, `unlock`, or the one-off `proc_synergy`), a `tier` (1–5), and one or more `pools` (`TREASURE`, `SHOP`, `CHEST`, `BOSS`, `CURSE`).

**By tier:**

| Tier | Count | Notes |
|---|---|---|
| 1 | 104 | The overwhelming majority of the game's items. All in `TREASURE,SHOP,CHEST`. |
| 2 | 19 | First "real" power step — first `damageReduction`, `dodgeChance`, defensive unlock flags. |
| 3 | 20 | First appearance of weapon-transformation unlocks (`RocketRounds`, `SpectralShots`, `BurstCannon`) and `BOSS` pool membership. |
| 4 | 22 | Where `CURSE` pool membership concentrates; big transformative unlocks (`LaserLens`, `BladeArc`, `DroneChip`). |
| 5 | 3 | `BlackSun`, `CrimsonRay`, `ExecutionersCoin` — the entire top of the power curve. |

**By pool membership** (an item can be in more than one pool): `CHEST` 168 (every item can drop from a chest of *some* type), `TREASURE` 145, `SHOP` 128, `BOSS` 41, `CURSE` 16.

**The 16 `CURSE`-pool items** (drop only from Devil-type rewards/curse-adjacent sources today): `GravLens, VoidTalisman, CursedRelic, BlackSun, BerserkerRelic, LeechVial, GravitySeed, VortexSeed, BloodPact, BloodSword, CerberusCharm, MutagenicCore, GlassKnife, VoidBulb, PhoenixFeather, CrimsonRay`.

**Gameplay categories** (grouped by the actual `stat=` field or `flag=` semantics, not invented buckets):

- **Basic stat upgrades** (single clean stat, no drawback): `DamageUp, FireRateUp, RangeUp, MoveBoots, MultiShot, VitalityUp, SwiftShot, QuickFire, Barrage, LongStride, LongSight, SteelNail, LuckyFuse, WarmSoup, RustySprinter, PaperClip, CopperLatch, RustyKeyring` and similar tier-1 single-stat items — this is the largest single category and the backbone of the starting pool.
- **Risk/reward dual-stat items** (a gain paired with a real cost): `GlassCannon, CursedVigor, AdrenalRush, HeavyRounds, PolyphemusScowl, NumberTwo, BrokenGlasses, TwistedPacifier, CursedLedger, GlassHeart, Overclock, ScatterCore, BunkerStance, GildedHeart` (positive/positive, an exception), `BloodPact, CursedRelic, BerserkerRelic, BlackSun, GlassKnife` — this category spans tiers 1–5 and is the natural home for "curse-flavored" unlocks.
- **Proc/status-effect modifiers** (elemental & on-hit procs): poison (`ToxinGland, NumberTwo, VenomLoad, StickyVenom, MutagenicCore`), burn (`Hellfire, BloodSpark, EmberNeedle, CerberusCharm`), freeze (`FrostBit, Frostbite, IceNeedle`), pierce (`PiercingRounds, TechLens, RazorMirror, PhantomCore, BulletBender`), explosive (`Explosivo, IpecacVial, BoomSeed, EmberRounds, GunpowderIdol`), chain (`ChainCapacitor, ArcBattery, ChainRelay, ChainPrayer`), gravity/vortex (`GravityWell, GravityCore, BlackHoleCore, GravLens, GravitySeed, VoidTalisman, VortexSeed, VoidBulb`), split (`SplitterLens, FractureCell, SplitSpark`), bounce (`RicochetChip, MirrorShard, RicochetCore, RubberTear, BounceGlass, HunterGrenade`), homing (`HomingBoost, HomingCore, BulletBender`), sticky (`StickyShot`), mark (`ReapersMark, MarkCharm, BossSigil`), boomerang (`ReturnTicket, BoomerTwig`), magnet (`MomsOldSpoon, MagnetCore, MagnetShard`), grow/shrink (`HeavyDroplet, NeedleCore, NeedleStorm`), lifesteal (`BloodBattery, LeechVial, BloodTithe`).
- **Weapon-transformation unlocks** (`type=unlock`, change how the gun fundamentally fires): `DiagonalFire (diagonal_fire), Capacitor (charged_shots), RocketRounds (rocket_rounds), LaserLens (laser_lens), SpectralShots (spectral_shots), CrimsonRay (crimson_ray), BladeArc (blade_arc), BurstCannon (burst_shots), TwinSoul (twin_soul), Satellites/DroneChip (satellites), HomingCore (homing), Devastator (devastator), InfiniteLoop (infinite_loop), ChaosEngine (chaos_engine), HollowCore (hollow_core), SecondSun (second_sun), LastShot/RewindCharm (last_shot), ParasiteCore (parasite_core), VoidHeart (void_heart)`. This is the single most "hype" category for reveal-style unlocks — each is a genuinely new way to play.
- **Defensive items**: `AegisShell, TurtleMail, MenderCharm, MirrorWard, PhoenixFeather, ArmorPlating, StoneGuard, BulwarkCore, ThornMantle, GuardianAngel, SpikedArmor, LastStand, IronWill, ShieldCharm, LuckyFoot` — spans a clean tier ramp from tier-1 basics to tier-4 `PhoenixFeather` (once-per-run death save).
- **Movement/mobility items**: `MoveBoots, LongStride, RustySprinter, Featherweight, AdrenalRush` (risk), `Dash/UnderworldDash (dash)`, `PitBoots (pit_walker)`, `AshWard (hazard_shroud)` — the last two are notable: `pit_walker` and `hazard_shroud` are terrain-interaction unlocks (let the player cross pits / ignore hazard tiles), which ties movement items directly to the environmental-hazard system in Part 1.5.
- **Summon/orbit items**: `Satellites/DroneChip` (orbiting shard), `TwinSoul` (mirrored twin shot), `ParasiteCore` (enemy deaths spawn extra shots) — FloppyRogue has no traditional "familiar" NPC companion system; these proc/orbit items are the closest analogue and should be treated as the "familiar" category for unlock-design purposes.
- **Economy / information / utility items**: `RustyKeyring, CopperLatch, LuckyFuse, GreedMark, MercyVoucher, ExecutionersCoin` (luck/crit economy), `Compass (compass), TreasureSense (treasure_sense)` (map/reward information).
- **Rare/endgame items** (tier 4–5, mostly `BOSS`/`CURSE` pool): `BlackSun, CrimsonRay, ExecutionersCoin` (the only tier-5 items in the game), plus the tier-4 `BOSS,CURSE,CHEST` cluster (`BerserkerRelic, LeechVial, GravLens, VoidTalisman, CursedRelic, GravitySeed, VortexSeed, BloodPact, BloodSword, CerberusCharm, MutagenicCore, VoidBulb, PhoenixFeather`) and the tier-4 `BOSS,CHEST`-only cluster (`SplitSpark, ChainPrayer, MarkCharm, BulletBender, BossSigil, DroneChip, RewindCharm, LaserLens, BladeArc`).

### 1.2 Enemies — 167 total

Parsed from `data/enemies.txt`. Every enemy has an `ai` behavior type and a `tier` (1–6).

**By tier:** Tier 1: 29 · Tier 2: 26 · Tier 3: 26 · Tier 4: 26 · Tier 5: 30 · Tier 6: 30.

**By AI archetype** (18 distinct behaviors): `SHOOTER` 25, `CHASER` 15, `CHARGER` 14, `SPAWNER` 14, `SUMMONER` 13, `DASHER` 12, `EXPLODER` 11, `STRAFER` 9, `LURKER` 8, `GUARDIAN` 6, `BURROWER` 6, `MIMIC` 6, `TELEPORTER` 5, `LINKER` 5, `COWARD` 5, `PATROLLER` 5, `ARTILLERY` 4, `SWARM_LEADER` 4.

**Special-flagged enemies** (candidates for "advanced enemy variant" unlocks, since these are mechanically distinct from their base AI type): shielded (`Fatty, Host, Warden, AmbushMaw, CryptMimic, StoneMimic, SootMimic, BlightKnight, Ashguard, DreadWarlord, BoneTitan, AbyssMimic, WardenPrime, ApexWarden, NullTitan, VoidGuardian, RuinSovereign, BulwarkOracle` and more), splits-on-death (`SplitterLord, SplitterZombie`), homing shots (`VenomEye, HomingWisp, FrostEye, VoidLurker, PrismStrafer, HaloStrafer, BoneLurker, RiftStalker, BrambleEye, DreadHarrier, RiftHarbinger, OblivionEye, CatacombReaper, DeepBurrower`), bounces-off-walls (`Ramrod, HookBrute, RiftJuggernaut`), timed explosions (`BombKnight, TimedKeg`).

**Spawner chains** (enemies whose `spawn_enemy=` field creates a dependency graph — relevant because unlocking a spawner is meaningless without its spawned unit also being available): `Nest→Fly, Hive→Grub, Broodkeeper→Zombie, BroodShed→Fly, CryptShed→SplitterZombie, BroodSilo→Fly, WebNest→Grub, NestMatron→Grub, GloomShed→Fly, WardenHive→Zombie, UmbralHexer→Gunner, SplitterPrime→RiftGrub, GraveSplitter→Fly, BoneWarden→Zombie, NightSovereign→Grub, DoomLattice→RiftGrub, HexApex→Zombie, DreadPillar→RiftGrub, SwarmApex→Grub, SwarmIcon→Fly, BroodMonarch→Fly, SwarmMarshal→Fly`. `RiftGrub` (tier 4, 14 HP) exists almost entirely as spawner-fodder for tier 5–6 spawners and should always unlock alongside the first spawner that targets it.

**Theme-pool status today** (see Executive Summary #1–2 for the mechanic): only **11 unique enemy names** are currently theme-reachable at all across `RUINS/FORGE/CRYPT/FUNGAL` (`Zombie, Gunner, Fly, BombKnight, Warlord, JuggernautPrime, HexMatron, Mimic, BlightGrub, VenomEye, SplitterLord, SnareTurret` — 12 counting the CRYPT/FUNGAL shared `HexMatron`/`BlightGrub`). `DRACONIC` (Floors 5–8) draws from the full tier-eligible pool due to the pool-resolution bug described above.

### 1.3 Bosses — 36 total

Parsed from `data/bosses.txt`. Bosses don't carry an explicit `tier=` field in the data file; tier comes from a hardcoded array in `dungeon.cpp` (`BOSS_VARIANT_TIER`), which assigns tiers **by file order**, not by name or by any per-boss field:

| Tier (by file order) | Count | Bosses |
|---|---|---|
| 1 | 6 | `Runt, Pinwheel, Spinner, Hexcaller, Bulwark, Grief` |
| 2 | 6 | `Ravager, Stormeye, Mirrorfiend, Siren, Burrower, Railwing` |
| 3 | 8 | `Riftmother, Harvester, Titan, Eclipse, GlassKing, Spite, WebMother, IronSaint` |
| 4 | 6 | `Mire, Chorus, RedWarden, Nullsire, Ashcaller, Frostgrip` |
| 5 | 6 | `Voidcrown, Duskfang, Emberlord, Hollowqueen, Graveforge, DragonSovereign` |
| 6 | 4 | `Linker, SwarmLeader, Patroller, Coward` |

**Important data quirk to flag to the user:** this tier assignment does **not** track HP or difficulty cleanly. Tier-5 bosses are enormous (`DragonSovereign` 1400 HP, `Hollowqueen` 1200 HP, `Emberlord` 1150 HP, `Voidcrown` 1100 HP) — noticeably bigger than the tier-6 bucket (`SwarmLeader` 590 HP, `Linker` 560 HP, `Patroller` 520 HP, `Coward` 470 HP, all *lower* than several tier-3/4 bosses). In practice, `Linker/SwarmLeader/Patroller/Coward` behave like a **side "specialist" tier** rather than a true final tier — they have unusually complex phase-2 attack cycles (3 patterns instead of the usual 2) relative to their HP. For progression-design purposes this document treats that quartet as **"Elite Specialists"** — mid-power bosses with the most technical attack patterns in the game — rather than as the top of the ladder. `DragonSovereign` (highest HP, 5-pattern phase-2 cycle: `RADIAL_BURST, CARDINAL_BURST, GAPPED_RING, TELEPORT_BURST, MIRROR_SHOT`) is the true top-of-ladder boss and is treated as such below.

**Boss-pool reachability today** (see Executive Summary #3): only **20 of 36 bosses** are reachable through any theme's `boss_pools`: `Runt, Pinwheel, Spinner, GlassKing, Patroller, IronSaint, Graveforge, Emberlord, Coward, Grief, Hollowqueen, Voidcrown, Linker, Mire, Spite, Harvester, Nullsire, SwarmLeader, DragonSovereign, Eclipse`.

**The 16 currently-unreachable bosses** — fully coded, zero spawn path: `Hexcaller, Bulwark, Ravager, Stormeye, Mirrorfiend, Siren, Burrower, Railwing, Riftmother, Titan, WebMother, Chorus, RedWarden, Ashcaller, Frostgrip, Duskfang`.

### 1.4 Themes & Floors

**5 themes** defined in `data/themes.txt`: `RUINS, FORGE, CRYPT, FUNGAL, DRACONIC`. Each carries `special_enemy_bonus`, `trap_chance_bonus`, `rock_bonus`, `pit_bonus`, `reward_bonus` — all currently used to *scale* danger/reward, never to *gate* anything by save progress.

**Floor→theme mapping is fixed, not random**, per `RollDungeonTheme()`: Floor 1 = `RUINS`, Floor 2 = `FORGE`, Floor 3 = `CRYPT`, Floor 4 = `FUNGAL`, **Floors 5, 6, 7, and 8 all = `DRACONIC`.** There are only 5 visual/mechanical themes stretched across 8 floors, and the back half of the game (Floors 5–8) is currently a single repeated theme. This is directly relevant to the Floor-5→Floor-8 expansion milestone: rather than only raising a floor cap number, that milestone is a natural place to introduce **DRACONIC sub-variants or an additional theme identity for the Floor 6–8 stretch**, addressed in Part 8.

**`data/rooms.txt`** confirms `total_floors=8` (overriding the `DungeonSettings` struct default of 5 in `dungeon.h`, which is stale/unused once the data file loads) — Floor 8 is the actual current final floor, matching the user's requested end-state cap exactly.

### 1.5 Rooms, Terrain, Hazards, Traps, Chests

From `game/rooms/room.h`, already implemented in full:

- **6 room types**: `START, NORMAL, BOSS, TREASURE, CURSE, SHOP`.
- **20 room archetypes** already exist in code (this matches — and appears to already be the result of — the prior 6→20 archetype expansion discussed for this project): `OPEN_ARENA, PILLAR_FIELD, BROKEN_ARENA, GAUNTLET, HAZARD_ROOM, RITUAL_ROOM, WHISPERING_STACKS, RUNIC_LATTICE, FORKING_PATH, SPIRE_ASCENT, FLOODED_CHAMBER, COLLAPSED_VAULT, SENTRY_HALL, GARDEN_MAZE, SHATTERED_BRIDGE, ECHO_ROOM, THRONE_APPROACH, TWIN_ISLANDS, NARROW_VEINS, AMPHITHEATER`.
- **7 encounter families**: `RUSH, ARTILLERY, SWARM, GUARDIAN, AMBUSH, MIXED, ELITE` — each archetype maps to a specific family (e.g. `HAZARD_ROOM→ARTILLERY`, `GAUNTLET→SWARM`, `RITUAL_ROOM→ELITE`), and family determines both enemy-selection weighting (`PickEnemyIndex`) and special-spawn chance (`EncounterFamilySpecialChance`, ranging 4%–22%).
- **23 terrain feature types**: 6 rock/block variants (`ROCK_BOMBABLE_COIN, ROCK_BOMBABLE_HEART, ROCK_BOMBABLE, ROCK_INDESTRUCTIBLE, ROCK_EXPLOSIVE, CRATE_DESTRUCTIBLE`), `BLOCK_PUSHABLE`, 2 bridge types (`BRIDGE_TEMPORARY, BRIDGE_FRAGILE`), 3 traversal aids (`TELEPORT_PAD, PRESSURE_PLATE, LILY_PAD`), 6 elemental/slow terrains (`TERRAIN_WEB, TERRAIN_SLIME, TERRAIN_MUD, TERRAIN_FIRE, TERRAIN_ACID, TERRAIN_POISON`), `PIT`, and 4 traps (`TRAP_POISON, TRAP_TELEPORT, TRAP_SUMMON, TRAP_SPIKE`).
- **7 chest types**: `WOODEN, IRON, STONE, GOLDEN, DEVIL, ANGEL, GAMBLE`. As documented above, `GAMBLE` is fully coded but never rolled — a ready-made secret chest.
- **Rocks, pits, and traps are currently ungated by save progress** — all 4 trap types and all 6 rock/crate variants can appear starting Floor 1, scaled only by floor number and theme bonus (see `GenerateTraps`/`GenerateRuleBasedTerrain` in `dungeon.cpp`). The 6 elemental terrains and 3 traversal-aid tiles appear to be wired specifically into the newer archetypes (`FLOODED_CHAMBER`, `SHATTERED_BRIDGE`, `TWIN_ISLANDS`, `WHISPERING_STACKS`, etc.), which — per Part 1.4 — are themselves already soft-gated to Floor 3+/4+. This means elemental terrain variety is **already indirectly floor-gated today**, which the Floor-cap system can simply formalize.

---

## Part 2 — Design Philosophy: Applying Isaac's Model to FloppyRogue's Actual Hooks

Binding of Isaac's unlock philosophy rests on a small number of ideas, each of which maps onto something FloppyRogue's code already does or almost does:

1. **Pools, not player inventories, are what's unlocked.** Isaac's `unlock.xml`/save data doesn't hand the player an item — it adds an `<Item>` line's weight into the relevant `itempool.xml` pool, and every future run can then roll it. FloppyRogue's `pools=` string on each item and `enemy_pools=`/`boss_pools=` strings per theme are the exact same mechanism, just serialized as comma-separated names in `.txt` files instead of XML. **The unlock system's core write operation is: append a name to a pool string (or flip an item from "inert" to "in-pool").**
2. **A fresh save is intentionally smaller than the full game, but still fun.** Isaac ships with a real item pool on a fresh save (it isn't literally empty) — it's a curated *subset*. FloppyRogue's situation is almost the mirror image: today's fresh save is *already* accidentally small on Floors 1–4 (11 enemies, and only 20 of 36 bosses reachable at all), while Floors 5–8 are accidentally *huge* because of the `DRACONIC` pool bug. The design goal is to make the *intentional* starting slice deliberate and pick better constituents than the current accidental one, and to make sure the *later* slice is deliberately built out (not just inherited from a bug) as the floor cap rises.
3. **Big victories matter more than once.** Isaac tracks defeat counts per boss/path, not just "beaten: yes/no." FloppyRogue has no persistence at all yet, so this is purely additive — a small counter map (`bossId → timesDefeated`) is all that's needed, and it directly powers the repeated-victory milestones in Part 6.
4. **Achievements are the "why," pools are the "what."** Isaac's achievements almost always resolve to exactly one pool mutation. This document follows the same discipline: every achievement below names the exact pool string(s) it edits.
5. **Pool dilution is a real balancing constraint, not a footnote.** Isaac is infamous for diluting its own pools over time as DLC added items; FloppyRogue's numbers make this concrete and are handled in Part 9.

---

## Part 3 — New Save: Exact Starting-State Proposal

This section (and Part 4) is the literal deliverable requested in the brief: not "some basic items," but the exact named content.

### 3.1 Starting floor cap
Maximum reachable floor: **3** (`RUINS → FORGE → CRYPT`).

### 3.2 Starting item pools
Every item currently in the game keeps its existing `pools=` string **except** the ones explicitly withheld below. Concretely:

- **All 104 Tier-1 items remain in their pools from the start**, with three exceptions withheld as "secret/joke/parity" items (see Part 4): `Martyrdom` (doubles all incoming damage — a hardcore/challenge-flavored item that should feel like a discovered secret, not a random early roll), `ChaosEngine` (every shot gets a random modifier — high-variance/build-defining, better as a mid-game surprise), and `HollowCore` ("a surge of random power floods your shots" — same reasoning).
- **All 19 Tier-2 items remain available from the start.** Tier 2 is where the first real defensive kit lives (`AegisShell, TurtleMail, MenderCharm, MirrorWard, BulwarkCore, ThornMantle`) and new players need access to it to survive Floor 2–3 theme difficulty scaling.
- **Tier 3, 4, and 5 items (20 + 22 + 3 = 45 items) start locked.** This is the single biggest, cleanest lever available: it's already a first-class field (`tier=`) read by the generator, so "locking tier 3+" requires zero new item-side infrastructure — only a save-side gate on `PickFiltered`'s effective `maxTier` argument (see Part 8).
- **All 16 `CURSE`-pool items start locked**, tier notwithstanding — they're thematically "you found something the game didn't want to give you," which fits an unlock reveal far better than a random Devil Room roll on a brand new save.

### 3.3 Starting enemy content
Fresh save keeps the current `RUINS/FORGE/CRYPT` theme pools **as-is** for the floors that are reachable (Floor 1–3): `Zombie, Gunner, Fly` (RUINS), `BombKnight, Warlord, JuggernautPrime, HexMatron` (FORGE), `Mimic, BlightGrub, HexMatron` (CRYPT). This is deliberately the same small, already-tested set the game ships with today — no regression risk, and it's already balanced against Floor 1–3 difficulty.

### 3.4 Starting boss content
Fresh save keeps `RUINS`'s existing `boss_pools` as-is: `Runt, Pinwheel, Spinner, GlassKing, Patroller`. `FORGE`'s and `CRYPT`'s pools (`IronSaint, Graveforge, Emberlord, Coward` and `Grief, Hollowqueen, Voidcrown, Linker`) also stay active since Floors 2–3 are reachable from run one.

### 3.5 Starting environmental/room content
- All 6 room types, all 4 base archetypes (`OPEN_ARENA, PILLAR_FIELD, GAUNTLET, BROKEN_ARENA`) plus `HAZARD_ROOM` and `RITUAL_ROOM`, and all 7 encounter families remain available — these already gate in cleanly by floor number today (see Part 1.5) and don't need additional locking.
- All rock/crate terrain variants, both pit types' worth of hazard, and all 4 trap types stay available from Floor 1, matching current behavior.
- `ChestType::GAMBLE` starts **locked** (currently dead content anyway — no regression, pure upside once unlocked).
- The 14 advanced archetypes and their associated elemental terrain (`TERRAIN_WEB/SLIME/MUD/FIRE/ACID/POISON`, `TELEPORT_PAD, PRESSURE_PLATE, LILY_PAD`, both bridge types) stay reachable starting Floor 3 exactly as they do today (small chance) — no change needed here since Floor 3 is already the fresh-save ceiling.

### 3.6 What this yields
A brand-new save can already: fight 3 floors across 3 themes, face 13 possible bosses, use 123 items (104 unlocked tier-1 items minus 3 withheld, plus all 19 tier-2 items), encounter roughly a dozen enemy types, and run into all 6 base room archetypes with full hazard/trap variety. That is comfortably enough for a fun, replayable early game while leaving **45 items, 16 bosses, 156 enemies (167 minus the ~11 theme-reachable), `DRACONIC`'s entire enemy variety, `GAMBLE` chests, and Floors 4–8** as things to discover.

---

## Part 4 — New Save: Exact Locked-Content List

- **Floors 4–8** (and by extension, the `FUNGAL` and `DRACONIC` themes, and any Floor-4+-weighted archetype distribution).
- **45 items**: all 20 Tier-3 items (`AshWard, BoomerTwig, BounceGlass, BurstCannon, ChainRelay, EmberNeedle, GlassKnife, GreedMark, GunpowderIdol, HunterGrenade, IceNeedle, OlympianFavor, PhantomCore, RazorCharm, RocketRounds, SharpenedCrown, SniperLens, SpartanSpear, SpectralShots, UnderworldDash`), all 22 Tier-4 items (`BerserkerRelic, BladeArc, BloodPact, BloodSword, BossSigil, BulletBender, CerberusCharm, ChainPrayer, CursedRelic, DroneChip, GravLens, GravitySeed, LaserLens, LeechVial, MarkCharm, MutagenicCore, PhoenixFeather, RewindCharm, SplitSpark, VoidBulb, VoidTalisman, VortexSeed`), all 3 Tier-5 items (`BlackSun, CrimsonRay, ExecutionersCoin`), plus the 3 Tier-1 items withheld as secrets (`Martyrdom, ChaosEngine, HollowCore`).
- **16 bosses**: `Hexcaller, Bulwark, Ravager, Stormeye, Mirrorfiend, Siren, Burrower, Railwing, Riftmother, Titan, WebMother, Chorus, RedWarden, Ashcaller, Frostgrip, Duskfang` — plus, structurally, the entirety of `FUNGAL`'s boss pool (`Mire, Spite, Harvester, Nullsire, SwarmLeader`) and `DRACONIC`'s boss pool (`DragonSovereign, Eclipse, Emberlord, Linker, SwarmLeader, Coward`), since those floors aren't reachable yet.
- **~156 enemies**: everything outside the current `RUINS/FORGE/CRYPT` theme lists, including the entire `FUNGAL` list (`VenomEye, SplitterLord, SnareTurret`, plus already-locked-by-floor `BlightGrub`) and, most importantly, `DRACONIC`'s de-facto full-roster access.
- **`GAMBLE` chests.**

---

## Part 5 — Repeated-Victory Structure at Each Progression Endpoint

Per the brief, each cap-raising floor's boss is not a single "beat once, unlock everything" gate. Below, "the Floor N boss" means whichever boss is rolled from that floor's `boss_pools` on a given run — since boss selection is already randomized per theme, "defeat the Floor 3 boss" naturally means "defeat any boss from `CRYPT`'s pool," which already gives runs variety while counting toward the same milestone.

### Floor 3 endpoint (`CRYPT`, bosses `Grief / Hollowqueen / Voidcrown / Linker`)
- **1st Floor-3 clear:** Unlock all 20 Tier-3 items into their existing pools (`AshWard` through `UnderworldDash`, listed in Part 4) — first taste of weapon transformations (`RocketRounds, SpectralShots, BurstCannon`) and the first `BOSS`-pool items.
- **2nd Floor-3 clear:** Unlock `FUNGAL`'s full enemy pool (add `VenomEye, SplitterLord, SnareTurret` to `FUNGAL`'s `enemy_pools`, since `BlightGrub` is already shared with `CRYPT`) and `FUNGAL`'s boss pool (`Mire, Spite, Harvester, Nullsire, SwarmLeader`) — this is a "your reward is the next stretch of the game becoming real," staged one clear ahead of literally unlocking Floor 4.
- **3rd Floor-3 clear (repeat-farming reward):** Unlock `Martyrdom, ChaosEngine, HollowCore` — the three withheld Tier-1 "joke/hardcore" items — into their pools. This rewards a player who goes back and clears Floor 3 again after already moving on, exactly like Isaac rewarding repeat Mom's Heart kills.
- **Milestone — Floor cap raised to 5:** requires the 2nd Floor-3 clear **and** a first clear of Floor 4 (`FUNGAL`). This two-part condition (repeat-clear *and* forward-progress) prevents a player from farming Floor 3 forever to "skip" Floor 4 content — they still have to go there.

### Floor 5 endpoint (start of `DRACONIC`, but treated as its own milestone before the theme is "complete")
- **1st Floor-5 clear:** Unlock all 22 Tier-4 items into their pools (`BerserkerRelic` through `VortexSeed`), and unlock the 4-boss "Elite Specialist" quartet (`Linker, SwarmLeader, Patroller, Coward`) into `DRACONIC`'s `boss_pools`. These four are chosen deliberately for this milestone because (a) they're already balanced as mid-power/high-complexity bosses per Part 1.3, and (b) two of them (`Linker`, `SwarmLeader`) are already present in `FUNGAL`'s and/or `DRACONIC`'s pool today, so this milestone is really "complete the set" rather than introducing all-new names.
- **2nd Floor-5 clear:** Unlock a first wave of the 16 orphaned bosses into `DRACONIC`'s pool — recommend the 6 that were tier-2/tier-3 by file order and therefore power-appropriate for a still-early `DRACONIC`: `Mirrorfiend, Siren, Burrower, Railwing, Riftmother, WebMother`. This is where the "secret boss pool" starts paying off.
- **3rd Floor-5 clear:** Unlock the fixed `DRACONIC` `enemy_pools` bug **properly** — instead of continuing to rely on the accidental full-roster fallback, explicitly populate `DRACONIC`'s pool with a curated Tier 5–6 enemy list (see Part 8.2 for the exact recommended list). This converts an accident into an intentional reward beat.
- **Milestone — Floor cap raised to 8:** requires the 2nd Floor-5 clear **and** a first clear of Floor 6, 7, and 8 individually (all still `DRACONIC` today) reached via the newly-raised cap. Framed to the player as "push all the way through the Dragon's floors once."

### Floor 8 endpoint (final floor, still `DRACONIC`, top bosses `DragonSovereign`/`Eclipse`)
- **1st Floor-8 clear:** Unlock the 3 Tier-5 items (`BlackSun, CrimsonRay, ExecutionersCoin`) and `GAMBLE` chests (add `GAMBLE` as a possible result in `RollChestTypeForFloor` for save files that have this unlock).
- **2nd Floor-8 clear:** Unlock the remaining 10 orphaned bosses (`Hexcaller, Bulwark, Ravager, Stormeye, Titan, Chorus, RedWarden, Ashcaller, Frostgrip, Duskfang`) into `DRACONIC`'s pool. At this point all 36 bosses in the game are reachable somewhere.
- **3rd Floor-8 clear:** Unlock a "true final" alternate encounter — see Part 6, Post-Game.
- Beyond the 3rd clear, Floor-8 clears roll into the Post-Game achievement track (Part 6) rather than new milestone-specific unlocks, matching Isaac's pattern of eventually converting "beat the final boss again" into "do it under a harder condition."

---

## Part 6 — Post-Floor-8 Progression

The brief specifically asks not to let progression stop after Floor 8. Because FloppyRogue's own content ceiling (36 bosses, 167 enemies, 168 items, 8 floors) is now fully known from the audit, post-game content should draw from **what's left over after Parts 3–5**, rather than inventing brand-new systems the codebase doesn't support yet:

- **Alternate/rare boss variants on repeat runs:** once all 36 bosses are unlocked (Part 5, Floor 8 2nd clear), post-game milestones can raise the *odds* of the rarer/higher-tier bosses within `PickBossVariantForTheme` rather than unlocking new names — e.g. an achievement that biases `DRACONIC` rolls toward `DragonSovereign`/`Eclipse` more often. This requires only a small weighting extension to the existing random-pick call, not new content.
- **Enemy variant depth**: post-game achievements can promote specific special-flagged enemies (Part 1.2 — shielded/homing/splitting/wall-bouncing variants) from "sometimes in the tier-eligible pool" to "guaranteed to appear at least once per floor," using the existing `EncounterFamilySpecialChance` hook (already ranges 4–22% by family) rather than a new mechanic.
- **Harder room variants**: nothing in the current archetype/encounter-family system distinguishes "hard mode" from normal mode. Post-game is the appropriate place to introduce a `threatBudget` multiplier (the field already exists per-room in `Room`) rather than new archetypes — i.e., an unlockable "Ascended" difficulty flag that raises `RoomThreatBudget()` output, reusable across every existing archetype.
- **New environmental hazard density, not new hazard types**: since all 23 terrain types already exist and are unlocked by Floor 8 naturally, post-game is better spent raising `rock_bonus`/`pit_bonus`/`trap_chance_bonus` via an unlockable "Corrupted Dragon" theme-profile override for `DRACONIC`, rather than adding a 24th terrain type that would need new C++ before it could exist.
- **Secret/challenge encounters**: the `GAMBLE` chest (Part 5, Floor 8 1st clear) and the "true final alternate boss" (below) are the natural secret-content slots; a third could be a guaranteed `RITUAL_ROOM`-archetype standalone superboss room unlockable via a specific hidden combination of achievements (see Part 7's Secret category), reusing `RITUAL_ROOM`/`ELITE` exactly as `BOSS`-type rooms already do.
- **"True final alternate boss":** recommend `DragonSovereign` itself gets a phase-2-only "true form" flag once a player has beaten the normal Floor 8 boss 3 times — mechanically this can be implemented as a save flag that causes `PickBossVariantForTheme` to force `DragonSovereign` instead of rolling `DRACONIC`'s pool, combined with a boss-side flag that always applies its phase-2 attack cycle (`RADIAL_BURST, CARDINAL_BURST, GAPPED_RING, TELEPORT_BURST, MIRROR_SHOT`) from the start of the fight rather than only after `phase2_hp_ratio` (0.40) is crossed. This is the closest FloppyRogue's current boss-encounter model can get to an Isaac-style "Mega Satan"/"The Lamb" escalation without adding a new boss template.

---

## Part 7 — Achievement List

Every achievement below names its exact mechanical unlock target. Achievements are grouped per the brief's categories; categories with no realistic mechanical support in the current codebase are marked as needing a small new tracking counter (never a new gameplay system) rather than invented.

### Main Progression
| Achievement | Requirement | Unlocks | Visible/Secret |
|---|---|---|---|
| Ruins Cleared | 1st Floor 3 clear | All 20 Tier-3 items (Part 5) | Visible |
| Fungal Threshold | 2nd Floor 3 clear | `FUNGAL` enemy+boss pool completion (Part 5) | Visible |
| Old Bones | 3rd Floor 3 clear | `Martyrdom, ChaosEngine, HollowCore` | Secret |
| Dragon's Doorstep | 1st Floor 5 clear | All 22 Tier-4 items + Elite Specialist boss quartet | Visible |
| Twin Serpents | 2nd Floor 5 clear | 6 orphaned bosses into `DRACONIC` (Part 5) | Visible |
| Curated Wilds | 3rd Floor 5 clear | Curated `DRACONIC` enemy pool (Part 8.2) | Visible |
| Sovereign's End | 1st Floor 8 clear | Tier-5 items + `GAMBLE` chests | Visible |
| Full Bestiary Unlocked | 2nd Floor 8 clear | Remaining 10 orphaned bosses | Visible |
| Dragon Unbound | 3rd Floor 8 clear | True-form `DragonSovereign` flag (Part 6) | Secret |

### Boss Completion
| Achievement | Requirement | Unlocks | Visible/Secret |
|---|---|---|---|
| Ruins Rogues' Gallery | Defeat all 5 of `RUINS`'s current bosses (`Runt, Pinwheel, Spinner, GlassKing, Patroller`) across any runs | +5% odds toward rarer boss rolls in `RUINS` (Part 6 weighting hook) | Visible |
| Forge Full Clear | Defeat all 4 of `FORGE`'s bosses (`IronSaint, Graveforge, Emberlord, Coward`) | Same weighting bonus, `FORGE` | Visible |
| Crypt Full Clear | Defeat all 4 of `CRYPT`'s bosses (`Grief, Hollowqueen, Voidcrown, Linker`) | Same weighting bonus, `CRYPT` | Visible |
| Every Boss, Once | Defeat all 36 bosses at least once (across all themes, post-unlock) | Cosmetic "Boss Slayer" flag + guarantees `GAMBLE` chest on next Floor-8 clear | Secret |

### Enemy Discovery
| Achievement | Requirement | Unlocks | Visible/Secret |
|---|---|---|---|
| Spore Cycle | Kill 50 `BlightGrub` (shared `CRYPT`/`FUNGAL` enemy) | Adds `SnareTurret` to `CRYPT`'s pool a floor early | Visible |
| Splitting Headache | Kill 25 enemies with `splits_on_death=true` (`SplitterLord`/`SplitterZombie`) after they've already split at least once | Adds `CryptShed` (spawner whose `spawn_enemy=SplitterZombie`) to `CRYPT`'s pool | Secret — needs a small "post-split kill" counter |
| Ghost Rounds | Survive 3 rooms containing a `homing_shots=true` enemy without taking damage from its shots | Adds one homing-flagged enemy (`VoidLurker`) to `FUNGAL` pool | Secret — needs a per-projectile-source damage-attribution flag |

### Environmental
| Achievement | Requirement | Unlocks | Visible/Secret |
|---|---|---|---|
| Demolition Crew | Destroy 100 `ROCK_BOMBABLE`/`CRATE_DESTRUCTIBLE` terrain features (lifetime) | +1 `rock_bonus` unlockable modifier, togglable in a future settings/challenge menu | Visible |
| Chain Reaction | Destroy 3 `ROCK_EXPLOSIVE` features in one room within 2 seconds of each other | Unlock `IpecacVial`/`BoomSeed` early (move from Tier-3-locked to always-available, if not already unlocked by floor progress) | Secret — needs a small terrain-chain timer already adjacent to existing `AddFeature`/`threatSpent` bookkeeping |
| Bridge Burner | Break both `BRIDGE_FRAGILE` tiles in a `SHATTERED_BRIDGE` room in a single run | Cosmetic unlock: guarantees `SHATTERED_BRIDGE` archetype appears at least once on the player's next Floor 4 | Visible |

### Rocks/Objects
| Achievement | Requirement | Unlocks | Visible/Secret |
|---|---|---|---|
| Coin Cracker | Open 20 `ROCK_BOMBABLE_COIN` rocks | +1 `coin_drop_chance` roll on Floor 1–2 for that save | Visible |
| Heart of Stone | Open 10 `ROCK_BOMBABLE_HEART` rocks | +1 `heart_drop_chance` roll on Floor 1–2 | Visible |
| Unbreakable | Fail to destroy 5 different `ROCK_INDESTRUCTIBLE` rocks with bombs in a single run (i.e., confirm you tried) | Secret cosmetic unlock only (no pool effect) | Secret — needs a "bomb hit an indestructible rock" event, trivial addition next to existing `IsBombable()`/blocking checks |

### Traps/Hazards
| Achievement | Requirement | Unlocks | Visible/Secret |
|---|---|---|---|
| Turnabout | Lure an enemy into triggering a `TRAP_SPIKE`/`TRAP_POISON` tile (enemy takes the damage instead of the player) | Unlock `AshWard` (`hazard_shroud`) one milestone early | Secret — needs trap-vs-enemy collision to also check "was this an enemy, log it," which the trap collision code already distinguishes by actor type |
| Pit Walker's Trial | Cross a `PIT` tile using `pit_walker` in 10 different rooms | Cosmetic title only | Visible |
| Teleported Twice | Get moved by a `TRAP_TELEPORT` tile twice in the same room | Secret cosmetic unlock | Secret |

### Room Discovery
| Achievement | Requirement | Unlocks | Visible/Secret |
|---|---|---|---|
| Cartographer | Fully clear one room of each of the 6 base archetypes in a single run | Unlock `Compass` one tier early (already tier 1, so this converts to "guaranteed in next Treasure Room" instead) | Visible |
| Deep Architecture | Encounter all 14 advanced archetypes at least once (lifetime, across runs) | Raises advanced-archetype roll weight on Floor 3 specifically (currently only 4–24% per roll — see `RollArchetype`) by a flat modifier for that save | Secret |
| Vault Breaker | Clear a `COLLAPSED_VAULT` room without breaking any `ROCK_INDESTRUCTIBLE` (i.e., without needing to — navigate around them) | Cosmetic title | Secret |

### Combat
| Achievement | Requirement | Unlocks | Visible/Secret |
|---|---|---|---|
| Century | Land 100 critical hits (lifetime) | Unlock `BlackCatEye`/`LuckyChamber` guaranteed appearance in next Shop | Visible |
| Elemental Overload | Land a burn, freeze, and poison proc all within one room-clear | Unlock `MutagenicCore` one tier early | Secret |
| Chain Master | Trigger `chainChance` procs on 5 enemies in a single chain | Unlock `ChainPrayer` one tier early | Secret |

### Survival
| Achievement | Requirement | Unlocks | Visible/Secret |
|---|---|---|---|
| Untouchable Boss | Defeat any boss without taking damage | Unlock `GuardianAngel`/`IronWill` guaranteed in the following Treasure Room | Visible |
| Last Breath | Win a run after `PhoenixFeather` or `LastStand`/`second_wind` triggers | Unlock `PhoenixFeather` into starting-available pool for future runs (moves it out of `CURSE`-locked status specifically) | Secret |
| Glass Cannon Gauntlet | Clear Floor 3 while holding `GlassCannon`, `GlassHeart`, or `GlassKnife` (any glass-family item) | Cosmetic "Fragile Victor" title | Secret |

### Curse/Risk
| Achievement | Requirement | Unlocks | Visible/Secret |
|---|---|---|---|
| Devil's Bargain | Clear a run after picking up 3+ `CURSE`-pool items in a single run | Unlock 3 additional `CURSE`-pool items ahead of the normal Floor-5/8 schedule (recommend `BloodPact, GlassKnife, CerberusCharm` — mid-power, good early curse-taste) | Secret |
| Martyr's Path | Win a run with `Martyrdom` active for the entire final floor | Unlock the true-form `DragonSovereign` flag one clear early (folds into Part 6's Post-Game track) | Secret |
| Overclocked | Win a run with `Overclock`/`OverdriveCore` (`overclock` flag) active | Cosmetic "Redlined" title | Secret |

### Secret Achievements
| Achievement | Requirement | Unlocks | Visible/Secret |
|---|---|---|---|
| The Gambler | Find a `GAMBLE` chest (only possible after it's unlocked) and open it 5 times across runs | Increases `GAMBLE`'s roll odds relative to other chest types in `RollChestTypeForFloor` for that save | Secret |
| Bestiary Complete | Encounter (not necessarily kill) all 167 enemies across all runs | Cosmetic "Naturalist" title + unlocks a bestiary-viewer menu entry | Secret |
| Boss Rush | Defeat 3 different bosses back-to-back with no Normal-room clears between them (via repeated Floor-8 farming with the floor cap already at 8) | Unlocks the standalone `RITUAL_ROOM` superboss encounter described in Part 6 | Secret |

### Post-Game
| Achievement | Requirement | Unlocks | Visible/Secret |
|---|---|---|---|
| Ascended | Clear Floor 8 with the post-game `threatBudget` multiplier active (Part 6) | Unlocks a second, harder multiplier tier | Visible (only after first Post-Game unlock) |
| Dragon Hunter | Defeat true-form `DragonSovereign` | Permanently biases all `DRACONIC` boss rolls toward the highest-HP bosses (`DragonSovereign, Eclipse, Hollowqueen, Emberlord`) | Secret |
| Full Unlock | Every item, enemy, boss, chest type, and archetype unlocked simultaneously | Cosmetic completion flag only — no further pool effect (matches Isaac's "Platinum God" being a display-only capstone) | Secret |

---

## Part 8 — Detailed Unlock Table (Condition → Pool Mutation)

| Unlock Condition | Permanent Unlock | Content Type | What Changes in Future Runs | Reason |
|---|---|---|---|---|
| 1st Floor 3 clear | All 20 Tier-3 items | Items | Tier-3 items become eligible in `PickFiltered`/`PickItemForRoom` calls whose `maxTier` reads from save state instead of a hardcoded 5 | First real power step, timed to the first cap-adjacent milestone |
| 2nd Floor 3 clear | `FUNGAL` enemy pool completed (`VenomEye, SplitterLord, SnareTurret`) + `FUNGAL` boss pool (`Mire, Spite, Harvester, Nullsire, SwarmLeader`) | Enemies/Bosses | `FUNGAL`'s `enemy_pools`/`boss_pools` strings gain entries | Telegraphs Floor 4 content one clear ahead of the cap actually rising |
| 3rd Floor 3 clear | `Martyrdom, ChaosEngine, HollowCore` | Items | 3 withheld Tier-1 items rejoin their pools | Rewards repeat-farming instead of only forward progress |
| 1st Floor 4 clear (combined with 2nd Floor 3 clear) | Floor cap → 5 | Progression | `totalFloors`-equivalent save cap raised from 3 to 5 | Two-part gate prevents Floor-3 farming from substituting for real Floor-4 progress |
| 1st Floor 5 clear | All 22 Tier-4 items + Elite Specialist bosses (`Linker, SwarmLeader, Patroller, Coward`) into `DRACONIC` | Items/Bosses | Tier-4 unlocked; 4 bosses added to `DRACONIC`'s `boss_pools` | Matches the jump in boss complexity (3-pattern phase-2 cycles) with the jump in floor difficulty |
| 2nd Floor 5 clear | 6 orphaned bosses (`Mirrorfiend, Siren, Burrower, Railwing, Riftmother, WebMother`) into `DRACONIC` | Bosses | `DRACONIC`'s `boss_pools` grows from 6 to 12 names | First wave of the "secret boss" reveal |
| 3rd Floor 5 clear | Curated `DRACONIC` enemy pool (Part 8.2 list) replaces the accidental full-roster fallback | Enemies | `DRACONIC`'s `enemy_pools` string is populated for the first time with valid names | Converts a bug into an intentional reward |
| 2nd Floor 5 clear + 1st clear of Floors 6/7/8 | Floor cap → 8 | Progression | Save cap raised from 5 to 8 | Requires pushing all the way through, not just re-clearing Floor 5 |
| 1st Floor 8 clear | Tier-5 items (`BlackSun, CrimsonRay, ExecutionersCoin`) + `GAMBLE` chest enabled | Items/Chests | Tier cap raised to 5; `RollChestTypeForFloor` gains a `GAMBLE` branch for unlocked saves | Top of the power curve arrives with the true final floor |
| 2nd Floor 8 clear | Remaining 10 orphaned bosses into `DRACONIC` | Bosses | All 36 bosses now reachable somewhere | Completes the boss roster |
| 3rd Floor 8 clear | True-form `DragonSovereign` flag | Post-Game | `PickBossVariantForTheme` can be forced to `DragonSovereign` with an always-phase-2 flag | Isaac-style capstone escalation |
| Defeat all bosses in a theme (any order) | +weighting bonus toward that theme's rarer bosses | Bosses | Existing random `themedVariants[...]` pick gains a soft weight table instead of uniform odds | Rewards completionists without adding new content |
| Destroy N environmental objects (Part 7, Environmental/Rocks) | Various early item unlocks / bonus drop-chance nudges | Items/Economy | Small `pools=`/drop-chance edits | Ties the "smash everything" playstyle to real progression, as requested |
| Kill an enemy using a trap (Part 7, Traps/Hazards) | `AshWard` one milestone early | Items | Moves `AshWard` from Tier-3-locked to always-available for that save | Rewards environmental-kill experimentation |
| Discover/clear a rare room (Part 7, Room Discovery) | Weight nudges toward advanced archetypes | Rooms | `RollArchetype`'s Floor-3 branch gains a per-save additive bonus | Rewards exploring the newer 14 archetypes |
| Secret achievement (Part 7, Secret) | `GAMBLE` odds increase / bestiary viewer / superboss room | Mixed | Small save-side multipliers / new menu entry / one-off `RITUAL_ROOM` spawn flag | Isaac-style hidden capstones |

### 8.1 Recommended item-pool unlock batching (avoids single-item spam)
Rather than 45 individual unlock conditions for 45 locked items, this design batches by **tier**, matching the game's own tier field exactly as shown in Part 5 — this is both the simplest implementation (one `minTier`/`maxTier` check per save) and the cleanest signal to the player ("you just unlocked Tier 3 items," not a scroll of 20 names).

### 8.2 Recommended curated `DRACONIC` enemy list (for the 3rd Floor-5-clear unlock)
Since `DRACONIC` is Floors 5–8 and the tier-eligible fallback already effectively serves Tier 5–6 enemies there today, the curated replacement list should keep that spirit but hand-pick recognizable, thematically "draconic/apex" named units rather than the entire roster: `AshTitan, EmberColossus, DragonSovereign`-adjacent trash mobs like `CinderReaper, CinderMarshal, AshCathedral, AshSentinel, RiftEmperor, RiftSovereign, VoidCrown, ApexWarden, BoneTitan, NullTitan`. This is a ~12-enemy curated pool — small enough to feel curated, large enough to avoid repetition, and entirely drawn from existing Tier 4–6 enemies already in `enemies.txt`.

---

## Part 9 — Item Pool Dilution Analysis

The concern raised in the brief is real and quantifiable with this data:

- **`TREASURE,SHOP,CHEST` (the default Tier-1 pool) already has 104 members.** Adding all 3 currently-withheld Tier-1 items (`Martyrdom, ChaosEngine, HollowCore`) back in only grows it to 107 — a 3% change, negligible dilution. This confirms withholding only 3 items at the Tier-1 layer is the right size: small enough to feel like a real secret, too small to meaningfully thin the pool once returned.
- **The `BOSS` pool (41 members today) is the one to watch.** Unlocking all 22 Tier-4 items (many of which are `BOSS`-pool) plus the Tier-4/5 orphaned-boss-adjacent items at once (Floor-5 1st clear) would nearly double `BOSS`-pool size in a single milestone. Recommendation: stagger Tier-4 item unlocks across the 1st and 2nd Floor-5 clears (e.g., unlock the `stat_mod` half of Tier-4 on the 1st clear, the `unlock`-type/transformation half — `BladeArc, LaserLens, DroneChip, RewindCharm, BulletBender` — on the 2nd) rather than all 22 at once, to keep `BOSS`-pool growth gradual.
- **The `CURSE` pool (16 members) is small and shouldn't be diluted at all in one shot.** Unlocking all 16 simultaneously would be a 16x jump from a 0-member locked pool. This document's staggered plan (3 via the Devil's Bargain achievement, the rest folded into Tier-4's normal unlock at Floor 5, per Part 5/8) avoids a single-turn flood.
- **Enemy theme pools are the opposite problem — they need to grow, not be protected from growing.** `RUINS/FORGE/CRYPT` sit at 3–4 valid names each; even doubling them (Part 5's `FUNGAL` completion, Part 8.2's `DRACONIC` curation) doesn't approach dilution territory the way item pools can. The unlock plan is deliberately more generous with enemy-pool growth than item-pool growth for this reason.
- **Boss pools should grow the slowest of all**, since each individual boss represents a much larger content commitment (a full floor-ending encounter) than a single item roll. The 3-stage reveal of the 16 orphaned bosses (4 at Floor-5 1st clear via the Elite Specialists, 6 at Floor-5 2nd clear, 10 at Floor-8 2nd clear — technically 4+6+10=20, but 4 of those 20 are already-live bosses being formally added to `DRACONIC` rather than new reveals, so the true "brand new boss" count unlocked is 16, matching exactly) is intentionally the slowest-paced unlock track in the whole document.

---

## Part 10 — Implementation Requirements (Not Being Built Now — Reference Only)

Listed only so a future implementation pass has a checklist; **nothing here should be started yet.**

1. **A save/profile file.** Does not exist today. Needs, at minimum: floor cap (int), a per-item unlocked flag or unlocked-tier ceiling, per-theme unlocked-enemy-name lists, per-theme unlocked-boss-name lists, a `GAMBLE`-chest-enabled flag, a per-boss defeat counter map, and a set of achievement-completion flags.
2. **A save-aware wrapper around `PickFiltered`/`PickItemForRoom`** so `maxTier` is clamped by the save's unlocked tier ceiling in addition to the existing floor-based clamps already in `dungeon.cpp`.
3. **A save-aware wrapper around theme pool resolution** so `enemy_pools`/`boss_pools` strings used by `ThemeProfileFor`/`PickBossVariantForTheme` are built from `(base themes.txt string) + (save's unlocked-additions string)` rather than reading `themes.txt` verbatim.
4. **Fixing the two theme-pool data bugs as part of this work, not before it**: `RUINS`'s `Spider` reference and `CRYPT`'s `Reaper` reference should be corrected or removed; `DRACONIC`'s `enemy_pools` line should be replaced with the curated list in Part 8.2 (today it accidentally works via fallback, but an unlock system should not depend on an accidental fallback path).
5. **A small number of new tracking counters** for achievements that need them, flagged individually as "Secret — needs a counter" throughout Part 7. None require new gameplay systems — all are one or two new fields on existing structs (`Room`, `Player`, or a new lightweight `RunStats` struct) incremented at points where the relevant event (trap-kills-enemy, split-then-rekill, boss-no-damage) already has to be detected for other reasons (damage source is already distinguished in the collision code, `splits_on_death` is already a per-enemy flag, boss HP/damage-taken is already tracked for phase transitions).
6. **A `GAMBLE` branch in `RollChestTypeForFloor`**, gated behind the save's `GAMBLE`-enabled flag, using the odds table already fully specified in `PickChestItem`'s existing `GAMBLE` case (Part 1.5) — no new chest logic, only a new roll branch.
7. **A "true-form" override flag consumed by `PickBossVariantForTheme`** plus a boss-side "start in phase 2 attack cycle" flag for the Post-Game `DragonSovereign` capstone (Part 6) — the smallest net-new mechanic in this entire document, and still just a boolean toggle on existing systems (`phase2_hp_ratio` check, boss variant selection).
8. **A weighting extension to boss/enemy random selection** (currently uniform `RNG::Range(0, size-1)` picks) to support the "bias toward rarer/bigger bosses" Post-Game achievements — a simple weight table keyed by boss/enemy index, defaulting to uniform when no save-side bias is active.
9. **UI/UX for reveals** (a "New Unlock!" toast, an unlock log/collection screen) — explicitly out of scope for this design document but flagged since Isaac's own philosophy leans heavily on the moment-to-moment reveal, not just the mechanical unlock.

---

## Part 11 — Summary Table: Stage-by-Stage Content Counts

| Stage | Floor Cap | Items Unlocked (of 168) | Bosses Reachable (of 36) | Enemies Theme-Reachable | Chest Types (of 7) |
|---|---|---|---|---|---|
| Fresh Save | 3 | 123 (104 T1 − 3 withheld + 19 T2) | 13 (`RUINS`+`FORGE`+`CRYPT` pools) | ~10 (`RUINS`+`FORGE`+`CRYPT` lists) | 6 (`GAMBLE` locked) |
| After Floor-3 milestones | 5 | 143 (+20 T3, +3 withheld returned) | 22 (+`FUNGAL`'s 5) | ~13 (+`FUNGAL` completed) | 6 |
| After Floor-5 milestones | 8 | 165 (+22 T4) | 32 (+4 Elites, +6 orphans) | ~25 (+curated `DRACONIC` list) | 6 |
| After Floor-8 milestones | 8 (max) | 168 (+3 T5, all) | 36 (all, +10 remaining orphans) | ~25 (max under this plan) | 7 (`GAMBLE` unlocked) |
| Post-Game | 8 + hard mode | 168 | 36 | ~25 (with rebalanced weighting) | 7 |

---

*End of design document. Per the brief, no code, data files, or save systems have been created or modified in the course of this analysis — this file is the only artifact produced.*
