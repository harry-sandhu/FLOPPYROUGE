# FloppyRogue - Current Content Plan

## Goal
Make the game feel much richer mostly through data-driven content first:
- stronger and more varied bosses, with nastier phase 2s
- a tougher late-floor enemy pool
- a bigger and more interesting item pool
- chest types with distinct rules and rewards
- a shop room with currency-based purchases
- item rewards split by tier and room pool, with items allowed in multiple pools

The aim is to keep the code changes small where possible, and put the bulk of the variation into text data files.

## 1. Boss Rework
### What we want
Bosses should feel less predictable and more threatening without rewriting the whole game.

### Current limitation
Boss behavior is still mostly hardcoded in `game/bosses/boss.cpp`. That means:
- stats can be tuned in data
- attack order and attack names can be made more data-driven
- completely new attack logic still needs code support

### Planned approach
- Move boss stat presets into a data file, or a boss config block.
- Keep the existing attack functions, but make each boss reference an attack cycle by name.
- Tune boss phase 1 and phase 2 so phase 2 is meaningfully harder.
- Let some bosses enter phase 2 earlier or later with a per-boss phase threshold.

### Suggested boss data fields
- `hp`
- `drift_speed`
- `attack_cd_phase1`
- `attack_cd_phase2`
- `charge_speed`
- `contact_damage`
- `charge_damage`
- `max_adds`
- `attack_cycle`
- `phase2_hp_ratio`

### Suggested boss attack names
- `SPREAD_SHOT`
- `RADIAL_BURST`
- `CHARGE`
- `LASER_SWEEP`
- `SUMMON_WAVE`
- `FLOOR_HAZARD`
- `MIRROR_SHOT`
- `CARDINAL_BURST`
- `SPIRAL_BURST`
- `TRIPLE_SPREAD`

### Boss design target
Each boss should have:
- one core identity
- one pressure tool
- one movement/dodge test
- one phase-2 escalation
- one signature move the player remembers
- one visual silhouette / theme

### Boss identity rule
Right now the bosses are still mostly "attack cycle + stats". The next pass
should give each boss a readable role, like a current roguelike guardian:
- a charger that feels like a duel
- a shooter that owns space
- a summoner that controls the room
- a hazard boss that changes safe ground
- a trick boss that teleports or fakes positions
- floor 1 bosses should be single-phase openings
- later-floor bosses can add phase 2 and more complex patterns

### Boss inspiration targets
- Hades II-style guardians: one clear gimmick, then a phase-based escalation
  with adds or environmental pressure
- pattern-loadout bosses: attacks should feel like a specific kit, not just
  random bullet spam
- elemental / theme bosses: fire, poison, mirror, shadow, curse, machine

### Reference games
- Binding of Isaac: item pools are strongly room-based, and pool dilution
  matters. Great model for our tier/pool item system and reward identity.
- Enter the Gungeon: bosses feel like personalities, not stat blocks, and
  the room/fight shape is part of the boss identity.
- Hades II: guardian bosses use clear telegraphs, phase changes, and
  environmental pressure to make each fight feel like a set piece.
- Dead Cells: bosses are progression gates with strong attack patterns and
  late-fight pressure, not just bigger health bars.
- Noobs Are Coming: boss-pattern loadouts are modular and combinable, which
  is a good reference for future boss-kit expansion.

### Suggested boss identities
- `Runt` - beginner duelist, clean charge + spread shot pattern
- `Pinwheel` - cardinal lancer, lines and lanes, more "aim and punish"
- `Spinner` - circular pressure and rotating shot walls
- `Hexcaller` - summoner, adds plus pressure
- `Bulwark` - arena controller, dense rings and hard charge punishment
- `Grief` - swarm boss, keeps pumping minions and chip damage
- `Ravager` - hazard boss, floor traps and forced movement
- `Stormeye` - beam boss, laser sweeps and summon support
- `Mirrorfiend` - mirror boss, gap rings and deceptive spacing
- `Siren` - spiral/beam hybrid with late-fight pressure
- `Burrower` - teleport boss, appears in a new spot and fires on arrival
- `Railwing` - teleport + laser pressure, like a fast ambush boss
- `Riftmother` - teleport summoner, room control through adds and space denial
- `Harvester` - add-heavy spiral pressure boss
- `Titan` - heavy arena boss, dense ring and charge punishment
- `Eclipse` - final hybrid boss, mixes several identities but should still
  read as a single apex encounter

## 2. Tier 3 Enemies
### What we want
Add a late-floor enemy tier that feels like a real threat instead of just higher numbers.

### Existing tiers
- Tier 1: early and mid-floor enemies
- Tier 2: harder variants already present
- Tier 3: new late-floor pool to add next

### Suggested tier 3 enemy ideas
- `Warlord` - fast charger, high HP, heavy contact damage
- `VenomEye` - shooter with poison pressure
- `SplitterLord` - splits into smaller enemies on death
- `BombKnight` - tanky exploder with a dangerous blast
- `HexMatron` - stronger summoner with faster add pressure
- `JuggernautPrime` - upgraded charger with higher speed and HP
- `SnareTurret` - turret-style shooter that controls space
- `BlightGrub` - very fast swarm pressure enemy

### Tier 3 design target
Tier 3 enemies should:
- appear mostly on later floors
- force movement, not just soak damage
- combine well with special enemy variants
- be readable at a glance
- be tuned so later floors feel dangerous, not just longer

## 3. Item Expansion
### What we want
The item pool should have:
- weak/common items that smooth runs
- strong items that define builds
- weird items that create memorable runs
- tradeoff items that are tempting but dangerous

### Current item shape
Most existing items are stat mods or unlocks. That is fine, but it makes the pool skew toward flat upgrades.

### Suggested item groups
#### Item metadata
- `tier=1` to `tier=5`
- `pools=TREASURE,BOSS,SHOP,CHEST,CURSE`
- items can belong to more than one pool

#### Common / weak items
- small damage up
- small fire rate up
- small range up
- small move speed up
- small luck up
- small shot speed up

#### Strong items
- crit chance
- lifesteal chance
- burn chance
- freeze chance
- chain chance
- gravity or vortex power
- projectile count upgrades with a drawback

#### Weird / build-defining items
- split shots
- wall bounce
- enemy bounce
- boomerang shots
- charged shots
- kill-triggered effects
- curse-style items with powerful upside and a downside

### Suggested item design target
Every new item should answer one of these:
- Does it make the run stronger?
- Does it change how you shoot?
- Does it create a new combo?
- Does it create a risk/reward decision?
- Which pools should it appear in?

## 4. Chest System
### What we want
Add several chest types with distinct behavior and reward tables.

### Chest types
#### Wooden Chest
- Basic chest
- Opens freely
- Contains basic loot:
  - hearts
  - bombs
  - keys
  - low chance item
  - coins

#### Iron Chest
- half a heart of damage to open
- Better loot than wooden
- About 5% item chance

#### Stone Chest
- Also bomb-openable
- Better than iron
- Can have a small chance for a rare item

#### Golden Chest
- Requires a key to open
- Better loot than iron
- About 10% item chance

#### Devil Chest
- Risky chest
- Can give:
  - strong loot
  - cursed loot
  - enemy-themed loot
  - mixed reward tables
- Suggested idea: 50% chance of lower-tier or cursed reward, 50% chance of tier 1 ememies

#### Angel Chest
- Multi-step chest with escalating reward rolls
- First open: 10% item chance
- Second open: 20% item chance
- Third open: 30% chaznce 
- Fourth open: 40 percent chnce
- Fifth open: 50 percent chjce of item final roll, then empty
- Once the item is taken or the fifth try is spent, the chest becomes empty

### Chest design target
- Wooden = baseline
- Iron / stone = resource cost, better loot
- Golden = key-gated reward
- Devil = danger/reward
- Angel = progression chest with escalating hope

## 5. Dungeon Hazards
### What we want
Make the dungeon itself feel more dangerous and more room-shaped.

### Supported ideas
- bombable rocks and unbreakable rocks
- bombable rocks can drop coins or hearts when destroyed
- one-shot room traps such as poison or teleport traps
- mimic enemies that can masquerade as chests
- `E` as the main bomb-drop key

### Dungeon hazard design target
- rocks should create small tactical blockers, not dead ends
- traps should be readable once the player sees them, but still punish
  careless movement
- mimics should feel like a chest surprise, not just a normal enemy skin
- future boss rooms can borrow the same dungeon-hazard language, so bosses
  feel like part of the cave instead of a detached arena fight

## 6. Shop Room
### What we want
Add a special room type where the player can buy items and supplies.

### Room behavior
- New room type: `SHOP`
- Shop items spawn on the ground
- The player walks over them to buy
- Purchase requires enough coins

### Shop stock ideas
- items
- hearts
- bombs
- keys
- maybe a rare shop-only item pool later

### Shop design target
- Simple to understand
- Rewarding but not required
- Good place to spend excess currency

## 7. Coins / Currency
### What we want
Add a simple coin stack system with multiple denominations.

### Coin types
- Nickel = 1
- Silver = 5
- Gold = 10

### Suggested use
- Small pickups can drop low-value coins
- Shop prices can be expressed in total value
- Better rooms and chests can drop higher denominations more rarely

## 8. Recommended Order
1. Add tier 3 enemies
2. Expand the item pool with common, strong, and weird items
3. Move boss stat presets and attack presets toward data
4. Add chest types and reward tables
5. Add coins, keys, and the shop room

## 9. Implementation Split
### Mostly text/data
- item additions
- enemy additions
- boss stat tuning
- chest reward tables
- shop item stock tables

### Needs code
- shop room type
- chest open/consume rules
- coin pickup handling
- boss data loading if we fully move bosses out of hardcoded presets
- any brand-new boss attack behavior

## 10. Notes
- The safest path is to make the game content-rich first, then do deeper systems refactors only where they pay off.
- Bosses are the biggest code-bound piece right now.
- Items and enemies are the easiest place to get a lot of value fast.
