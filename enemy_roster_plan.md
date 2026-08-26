# FloppyRogue - Enemy Roster Expansion Plan

## Goal
Expand the late-game enemy pool so floors 4-8 feel like a real escalation instead of a repeat of the early-game roster.

Current enemy data already gives us a solid base, but the late tiers are too small:
- Tier 1: 29 enemies
- Tier 2: 21 enemies
- Tier 3: 26 enemies
- Tier 4: 4 enemies
- Tier 5: 4 enemies
- Tier 6: 0 enemies

The immediate goal is to add enough tier 4 and tier 5 content that the late floors stop repeating too often, then introduce tier 6 as the true endgame pool.

Updated target:
- at least 20 new enemies across tiers 4, 5, and 6 combined
- enough variety that floor 6-8 can mix new roles instead of recycling the same few late-game templates

## Design Rules
- Each tier should feel meaningfully different, not just numerically stronger.
- Late floors should introduce new enemy behavior, not only higher HP.
- Tier 4 should feel dangerous but still readable.
- Tier 5 should feel oppressive and reward clean movement.
- Tier 6 should be rare, scary, and clearly endgame.
- New enemies should support the existing room archetypes and boss pacing.
- Some enemies can combine two existing roster roles, like shooter-plus-summoner or charger-plus-exploder, if the combo is readable.

## Tier Targets

### Tier 4
Role: first real late-floor enemy tier.

Recommended target:
- Add 8 to 10 new enemies
- Keep current tier 4 enemies as anchors
- Focus on pressure, ranged denial, and mixed mobility

Suggested tier 4 themes:
- heavy chargers
- area controllers
- tanky shooters
- summoners with slow but dangerous adds
- enemies that punish standing still

Suggested tier 4 enemy ideas:
- `CinderMage`-type caster that throws fire lanes or burn zones
- `VoidWraith`-type evasive enemy that phases or teleports
- `HexSpawner`-type room control enemy that creates weaker adds
- `Deathcoil`-type shooter with spiral or ring pressure
- `Stonewarden`-type slow tank that blocks movement and soaks damage
- `EmberLancer`-type charger with a clear telegraph and heavy impact
- `RiftStalker`-type ambush enemy that punishes backtracking

### Tier 5
Role: high-end late-game pool for the deepest normal floors and harder boss-adjacent rooms.

Recommended target:
- Add 8 to 10 new enemies
- Make them more distinct than tier 4
- Mix in disruptive attack patterns and stronger special variants

Suggested tier 5 themes:
- elite summoners
- multi-phase attackers
- persistent hazards
- enemies with shield, split, or revive-style pressure
- enemies that create safe-zone denial

Suggested tier 5 enemy ideas:
- `SoulEater`-type drain enemy that rewards quick focus fire
- `Cataclysm`-type explosive bruiser with delayed room pressure
- `GraveKnight`-type armored elite charger
- `RiftCaptain`-type shooter that commands adds or line attacks
- `DreadHarrier`-type aerial pressure enemy with constant repositioning
- `NullPriest`-type support enemy that buffs nearby threats
- `AbyssMimic`-type mimic variant that punishes greed

### Tier 6
Role: true endgame tier.

Recommended target:
- Add 4 to 6 enemies
- Use them sparingly so they feel special
- Reserve them for floor 7-8 or equivalent deepest content

Suggested tier 6 themes:
- apex versions of existing archetypes
- enemies with stacked mechanics
- enemies that combine movement denial and lethal burst
- enemies that feel closer to minibosses than normal mobs

Suggested tier 6 enemy ideas:
- `ApexWarden`-type fortress enemy with layered defenses
- `VoidCrown`-type bosslike caster with sweeping attacks
- `AshTitan`-type massive charger with trail hazards
- `NightSovereign`-type elite summoner with add waves
- `DoomLattice`-type room controller that turns the room into a hazard puzzle

## Additional 15 Enemy Candidates

These are meant to stay close to the existing tier 3 vibe:
- familiar enemy roles
- clearer upgrades, not exotic new systems
- late-game pressure built from movement, projectiles, summons, and bruiser bodies

### Tier 4 additions
- `Ashrunner` - fast charger, like a sharper `Runner`/`Rusher` variant
- `CoilEye` - upgraded shooter with denser burst patterns
- `BlightKnight` - armored chaser that forces focus fire
- `NestMatron` - summoner that replaces simple add pressure with steady room control
- `RailSentinel` - turret-style lane controller that punishes straight movement
- `RiftGrub` - swarm enemy that splits or leaves pressure behind
- `SootMimic` - mimic variant that punishes greedy pickups in later floors
- `ThornHost` - exploder with stronger area denial than `Host`

### Tier 5 additions
- `DreadWarlord` - elite charger, same family as `Warlord` but more punishing
- `UmbralHexer` - stronger summoner/support unit with faster add cycles
- `CatapultEye` - long-range artillery enemy that owns open rooms
- `GraveTurret` - stationary pressure piece with heavier projectile control
- `SplitterPrime` - late-game splitter that turns one kill into several threats
- `CinderReaper` - hybrid mover/shooter that pressures both range and space
- `BoneTitan` - heavy bruiser with a big body and slow, dangerous advances

## Combined Roster Types

These are useful when we want more than 20 total late-game enemies without inventing totally new mechanics every time.

### Good combo patterns
- `charger + shooter`
- `charger + exploder`
- `shooter + summoner`
- `shooter + turret`
- `summoner + support`
- `bruiser + hazard`
- `mimic + ambush`
- `splitter + swarm`

### Example combined enemies
- `RiftCharger` - closes distance quickly, then fires a burst on contact
- `AshBomber` - heavy body that explodes after pressure builds
- `HexSentinel` - turret that also spawns weak adds
- `CinderMarshal` - shooter with a summon wave at low HP
- `MireMimic` - mimic that also leaves hazard tiles on reveal
- `GraveSplitter` - bruiser that splits into smaller threats when killed
- `NullHarrier` - fast flanker that alternates between dash and projectile pressure
- `WardenHive` - tanky support unit that keeps nearby enemies active longer

### Why combo types help
- They increase variety without requiring a new AI category for every enemy.
- They make the late floors feel less like a pile of stat upgrades.
- They let tier 4-6 scale difficulty through behavior, not just HP.

## More Roster Types

If we want the late floors to stay fresh, it is fine to create more combined roster types as long as they still feel readable in play.

### Additional combo families
- `charger + turret`
- `charger + summoner`
- `shooter + exploder`
- `shooter + mimic`
- `summoner + splitter`
- `bruiser + support`
- `ambush + hazard`
- `flanker + dash`
- `tank + lane control`
- `elite + add pressure`
- `teleport + shooter`
- `teleport + summoner`

### Design rule for combos
- One enemy should still have one primary job.
- The second role should support the first, not obscure it.
- A hybrid enemy should be understandable in under a second.
- If a combo becomes too busy, split it into two simpler enemies instead.

## Concrete 24-Enemy Draft

This is a full late-game target list, with the idea that tiers 4, 5, and 6 together should add at least 20 new enemies and ideally land around 24.

### Tier 4 draft
- `Ashrunner` - charger
- `CoilEye` - shooter
- `BlightKnight` - bruiser
- `NestMatron` - summoner
- `RailSentinel` - turret
- `RiftGrub` - splitter/swarm
- `SootMimic` - mimic/ambush
- `ThornHost` - exploder/hazard

### Tier 5 draft
- `DreadWarlord` - charger/bruiser
- `UmbralHexer` - summoner/support
- `CatapultEye` - shooter/artillery
- `GraveTurret` - turret/lane control
- `SplitterPrime` - splitter/swarm
- `CinderReaper` - flanker/shooter
- `BoneTitan` - bruiser/tank
- `RiftCaptain` - shooter/summoner

### Tier 6 draft
- `ApexWarden` - tank/lane control
- `VoidCrown` - shooter/teleport
- `AshTitan` - charger/hazard
- `NightSovereign` - summoner/support
- `DoomLattice` - hazard/room control
- `RiftHarbinger` - teleport/shooter
- `GraveOracle` - summon/beam pressure
- `CataclysmCore` - bruiser/exploder

### Draft summary
- Tier 4: 8 enemies
- Tier 5: 8 enemies
- Tier 6: 8 enemies
- Total: 24 enemies

That total is a good target if we want the late floors to feel meaningfully broader than the current tier 1-3 pool.

### Why these fit
- They read like upgraded versions of the current roster instead of brand-new fantasy archetypes.
- They preserve the current enemy grammar: chaser, shooter, charger, summoner, exploder, turret, mimic.
- They give the late floors more combinations without forcing the game to learn entirely new behavior classes.

## Proposed Roster Shape

### Tier 4
- 10 to 12 additions
- should cover at least one charger, one shooter, one summoner, one tank, and one space-control enemy

### Tier 5
- 10 to 12 additions
- should include at least one enemy that splits, one that summons, one that creates persistent hazards, and one that is heavily armored

### Tier 6
- 4 to 6 additions
- should lean into elite or miniboss-level identity

### Combined total target
- 20+ new enemies across tiers 4, 5, and 6 combined
- ideally closer to 24 if we want the deepest floors to stay fresh for longer runs

## Suggested Data Fields

Keep using the current enemy data format, and expand only when needed:
- `ai`
- `hp`
- `speed`
- `w`
- `h`
- `attack_pattern`
- `shoot_cooldown`
- `shoot_range`
- `preferred_distance`
- `shot_speed`
- `shielded`
- `homing_shots`
- `bounces_off_walls`
- `explodes_on_timer`
- `splits_on_death`
- `spawn_enemy`
- `spawn_count`
- `spawn_limit`
- `tier`

If a new behavior needs a new field, prefer adding one reusable flag or stat over making a one-off enemy hardcode.

## Floor Mapping

Recommended floor feel:
- Floors 1-2: mostly tier 1 and tier 2, with tier 3 starting to appear lightly
- Floor 3: tier 3 becomes common
- Floor 4: tier 4 starts entering the main pool
- Floor 5: tier 4 and tier 5 mix
- Floor 6: tier 5 becomes common
- Floors 7-8: tier 6 appears, with tier 5 still present but no longer dominant

## Implementation Order

1. Add more tier 4 enemies first.
2. Add enough tier 5 enemies to stop repetition in deep runs.
3. Add a small tier 6 pool for the final floors.
4. Rebalance `EnemyDatabase::MaxTier()` behavior if needed after the new data lands.
5. Playtest floor-by-floor and adjust spawn weights so tier 6 stays rare.

## Notes

- The goal is variety first, not just bigger numbers.
- Late floors should still include tier 3 and tier 4 enemies in mixed groups so the game doesn't become a pure stat wall.
- Tier 6 should feel like a reward for surviving long enough to see it.
