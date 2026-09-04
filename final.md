FLOPPYROGUE — COMPLETE SPRITE IMPLEMENTATION PLAN 0. The overall goal

We are going to turn the current game from:

colored rectangles representing everything

into:

a proper pixel-art sprite-based game using the assets already inside data/assets/.

The important thing is that gameplay logic does not need to be rewritten.

The existing systems already know:

which item exists
which enemy exists
which boss exists
which terrain exists
which pickup exists
which room exists
which theme the dungeon is using

We are adding an asset/rendering layer between the existing game logic and the framebuffer.

So conceptually:

CURRENT GAME LOGIC
↓
Player / Enemy / Item / Boss / Terrain / Room
↓
SPRITE LOOKUP
↓
Sprite Sheet + Source Rectangle
↓
Renderer::DrawSprite()
↓
320 × 180 framebuffer
↓
StretchDIBits()
↓
Game window

That is the architecture we should follow.

1. FIRST — establish the permanent asset numbering system

This is the most important foundation.

We already have a very convenient numbering system.

Items

There are 168 items in items.txt.

The sheets contain:

item1.png → 01–24
item2.png → 25–48
item3.png → 49–72
item4.png → 73–96
item5.png → 97–120
item6.png → 121–144

The existing data file contains 168 items, so there are 24 items beyond the available 144 item sprites.

We should not change item IDs or reorder the database.

The mapping should remain:

Item database index 0 → sprite #1
Item database index 1 → sprite #2
...
Item database index 143 → sprite #144

For the remaining 24:

Item #145–168 → temporary fallback sprite

This should be handled by the sprite registry, not by modifying the item database.

That means later, if another item sheet is added, we can simply add coverage.

2. Enemy mapping

This one is perfect.

There are 168 enemies.

We have:

enemies1.png → 001–024
enemies2.png → 025–048
enemies3.png → 049–072
enemies4.png → 073–096
enemies5.png → 097–120
enemies6.png → 121–144
enemies7.png → 145–168

Every sheet is:

6 × 4

So there is a complete one-to-one mapping.

We should use:

EnemyDatabase index
↓
index + 1
↓
sprite number
↓
sheet + cell

Because the database loads enemies in declaration order, the order already gives us the permanent ID.

Do not match enemies by AI type.

For example, don't say:

every SHOOTER uses sprite X.

That would be wrong because several different enemies can have the same AI.

Instead:

enemy.templateName
↓
EnemyDatabase::IndexOf()
↓
sprite ID

or, preferably, eventually carry the database index/template index directly if convenient.

3. Boss mapping

There are 36 bosses in bosses.txt.

The intended mapping is:

bosses1.png → Boss 01–12
bosses2.png → Boss 13–24
bosses3.png → Boss 25–36

with:

4 × 3

per sheet.

So:

01 02 03 04
05 06 07 08
09 10 11 12

This gives us a complete 36-boss mapping.

BUT — boss sprites are special

These aren't normal gameplay sprites.

They are large portrait/card illustrations with the boss's name integrated into the artwork.

We are still going to use them.

We simply need a special boss rendering system.

4. Boss portrait cropping strategy

We should NOT try to remove the text from the PNG files themselves.

Instead:

Load boss sheet
↓
Select boss cell
↓
Draw only the portion containing the artwork
↓
Exclude the baked-in boss name

This means the sprite registry for bosses should contain:

sheet
source rectangle

rather than assuming the whole 1/12 cell is usable.

Important

The boss portrait source rectangles will be manually defined.

Not:

x = column _ cellWidth;
y = row _ cellHeight;

for the final boss artwork.

Instead:

Boss 1
sheet = bosses1.png
cell = 1
artRect = manually determined rectangle

Boss 2
sheet = bosses1.png
cell = 2
artRect = manually determined rectangle

...

This gives us complete control.

The actual boss portrait can then be resized to fit the gameplay boss dimensions.

5. Boss rendering in gameplay

We need to make an important distinction:

Collision size

The existing:

boss.w
boss.h

continues to control gameplay.

Visual size

The portrait can be rendered larger than the collision box.

For example:

Boss collision box
↓
small logical gameplay rectangle

Boss artwork
↓
larger centered visual

This is important because the portrait artwork is much more detailed/larger than the current colored rectangle.

We should not let the artwork determine collision.

So:

Physics ≠ sprite dimensions

The boss sprite is visual only.

6. Boss name problem

The HUD already displays:

Boss name
Boss health bar

So the baked-in image text is actually undesirable.

Therefore the gameplay boss sprite must use the cropped artwork region.

We do not want:

[PORTRAIT]
RUNT

inside the sprite while the HUD also says:

RUNT
████████

The crop should remove that bottom/name region.

7. Renderer foundation

This is the first actual engine change.

Currently the renderer has:

SetPixel()
DrawRect()
Present()

but no concept of an image.

We need to add:

Image / Texture

conceptually containing:

width
height
pixel data

and a loader.

8. PNG loading

The game currently has no PNG decoder.

We should add a small single-header PNG loader such as stb_image.

The objective is:

PNG file
↓
decode
↓
RGBA pixel buffer
↓
Image object

The loader should convert everything to a consistent format, ideally:

RGBA8

regardless of the source PNG format.

9. Asset manager

We should NOT have every game system manually loading PNGs.

Instead create one central asset system.

Something conceptually like:

AssetManager

responsible for:

Load all sprite sheets
Get sprite sheet
Release/free assets

At startup:

item1.png
item2.png
...
item6.png

enemies1.png
...
enemies7.png

m1.png
m2.png

bosses1.png
bosses2.png
bosses3.png

Ruines.png
Forge.png
crypt.png
fungus.png
dragonuc.png

are loaded once.

We absolutely do not load a PNG every frame.

10. Sprite abstraction

After images are loaded, we need a common description of a sprite.

Conceptually:

Sprite
├── sheet/image
├── source X
├── source Y
├── source width
├── source height
├── optional pivot
└── optional flip

Then the game doesn't care whether the sprite comes from:

item sheet
enemy sheet
boss sheet
m1
m2
theme sheet

It simply asks:

DrawSprite(sprite, position, size) 11. Renderer::DrawSprite

Add a renderer operation approximately conceptually:

DrawSprite(
image,
source rectangle,
destination rectangle
)

It needs to support:

source cropping
destination scaling
clipping against screen
transparency
optional horizontal flip

The renderer will copy pixels from:

source PNG

into:

320×180 framebuffer

while respecting alpha.

12. Alpha blending

This is critical.

The PNGs are RGBA.

The current renderer simply overwrites pixels.

That won't work correctly for sprites with transparent backgrounds.

We need:

source alpha
↓
blend with framebuffer

So transparent pixels don't create boxes around sprites.

The normal case becomes:

alpha = 0
→ don't modify framebuffer

alpha = 255
→ replace framebuffer pixel

0 < alpha < 255
→ blend 13. Keep DrawRect

We should NOT remove DrawRect.

It remains useful for:

debug
HUD
health bars
map
outlines
fallback rendering
collision visualization if needed

We're replacing gameplay visuals, not deleting primitive rendering.

14. Pixel-art scaling

The game runs internally at:

320 × 180

and then uses:

COLORONCOLOR

for nearest-neighbor scaling.

We should preserve that.

Sprite rendering should therefore use:

integer-friendly scaling
nearest-neighbor
no smoothing

The artwork should remain crisp.

15. m1.png implementation

m1.png contains 35 defined sprites with slot 36 empty.

The documented layout is:

9 × 4

and the sprites are numbered:

Player
01 Player
02 Idle / Walk
03 Shooting
04 Dash
05 Hurt / Death
Projectiles
06 Bullet
07 Rocket
08 Laser
09 Crimson Ray
10 Slash
Orbiters
11 Orbiter
12 Second Sun
Terrain
13 Temporary Bridge
14 Fragile Bridge
15 Teleport Pad
16 Pressure Plate
17 Lily Pad
18 Mud
19 Acid
20 Poison Terrain
Traps
21 Poison Trap
22 Teleport Trap
23 Summon Trap
24 Spike Closed
25 Spike Activated
VFX
26 Explosion
27 Hit
28 Break
29 Teleport
30 Summon
31 Fire
32 Poison
33 Freeze
34 Critical Hit
Special
35 Trophy 16. m1 sprite registry

We should create explicit constants/IDs.

For example conceptually:

M1_PLAYER
M1_PLAYER_IDLE
M1_PLAYER_SHOOT
M1_PLAYER_DASH
M1_PLAYER_HURT

M1_BULLET
M1_ROCKET
...

The actual game code then asks for:

M1_BULLET

instead of:

x = 5 \* something
y = something

This makes the implementation much safer.

17. Player implementation

Current player drawing is a rectangle.

Replace it with m1 player sprites.

We have five player states:

idle/walk
shoot
dash
hurt/death
base player

The first implementation should be state-based, not a complex animation system.

For example:

normal
→ idle/walk sprite

shooting
→ shooting sprite

dashing
→ dash sprite

hurt/dead
→ hurt sprite

This is enough for the current artwork.

18. Player direction

The player needs to visually face the shooting/movement direction.

Because we don't have separate left/right artwork, use:

DrawSprite(... flipX)

for horizontal direction.

Vertical direction can simply use the same sprite unless the generated artwork actually provides something different.

Do not build a giant directional animation framework right now.

19. Projectile implementation

Current projectile rendering is in:

projectile_system.cpp

and uses rectangles.

We should map projectile type → m1 sprite:

normal bullet → 06
rocket → 07
laser → 08
crimson ray → 09
slash → 10

The important point:

Projectile physics remains unchanged.

Only:

ProjectileSystem::Draw()

changes visually.

20. Orbiter / Second Sun

Current code manually draws tiny rectangles.

Replace:

orbiter

with:

m1 #11

and:

second sun

with:

m1 #12

The orbital positions remain exactly as they are.

21. Terrain implementation

Existing terrain types already correspond nicely to the m1 sheet.

Map:

BRIDGE_TEMPORARY → 13
BRIDGE_FRAGILE → 14
TELEPORT_PAD → 15
PRESSURE_PLATE → 16
LILY_PAD → 17
MUD → 18
ACID → 19
POISON_TERRAIN → 20

The current terrain logic should remain.

Only rendering changes.

22. Trap implementation

Use:

POISON_TRAP → 21
TELEPORT_TRAP → 22
SUMMON_TRAP → 23
SPIKE CLOSED → 24
SPIKE ACTIVE → 25

The spike trap is especially important.

Instead of trying to animate it immediately:

inactive → #24
active → #25

This gives us a convincing state change without needing a full animation framework.

23. VFX system

The m1 sheet gives us:

Explosion
Hit
Break
Teleport
Summon
Fire
Poison
Freeze
Critical Hit

We should add a lightweight effect system.

Something like:

Effect
├── type
├── position
├── lifetime
├── elapsed time
└── optional scale

Initially the effect can simply:

spawn
→ draw sprite
→ expire

Then later we can animate/flash them.

This gives us a foundation for visual feedback.

24. m2.png implementation

m2 has 32 sprites.

It is a clean:

8 × 4

sheet.

Map exactly:

01 Nickel
02 Silver
03 Gold
04 Key
05 Bomb
06 Half Heart
07 Full Heart
08 Wooden Chest Closed

09 Wooden Chest Open
10 Stone Chest Closed
11 Stone Chest Open
12 Iron Chest Closed
13 Iron Chest Open
14 Gold Chest Closed
15 Gold Chest Open
16 Angel Chest Closed

17 Angel Chest Open
18 Curse Chest Closed
19 Curse Chest Open
20 Normal Door Closed
21 Normal Door Open
22 Treasure Door Closed
23 Treasure Door Open
24 Curse Door Closed

25 Curse Door Open
26 Boss Door Closed
27 Boss Door Open
28 Bombable Stone
29 Unbreakable Stone
30 Money Stone
31 Heart Stone
32 Movable Stone 25. Pickup implementation

Replace current colored pickup rectangles.

Map:

COIN

based on denomination:

1–4 → Nickel
5–9 → Silver
10+ → Gold

Or, if the existing amount logic already distinguishes the three, use that directly.

Then:

KEY → #04
BOMB → #05
HALF HEART → #06
FULL HEART → #07 26. Chest rendering

Chest type + open state should determine sprite.

For example:

WOOD + CLOSED → 08
WOOD + OPEN → 09

STONE + CLOSED → 10
STONE + OPEN → 11

IRON + CLOSED → 12
IRON + OPEN → 13

GOLD + CLOSED → 14
GOLD + OPEN → 15

ANGEL + CLOSED → 16
ANGEL + OPEN → 17

CURSE + CLOSED → 18
CURSE + OPEN → 19

No new gameplay logic is necessary.

27. Door rendering

Same concept:

Normal closed → 20
Normal open → 21

Treasure closed → 22
Treasure open → 23

Curse closed → 24
Curse open → 25

Boss closed → 26
Boss open → 27

The existing room/gate logic determines the state.

28. Stones

Replace rectangle rocks with:

Bombable → 28
Unbreakable → 29
Money → 30
Heart → 31
Movable → 32

The existing collision/destruction/push logic stays unchanged.

29. Trophy

The trophy should use:

m1 #35

instead of the current generic rectangle.

30. Items

For items, use the master item ID mapping.

The algorithm:

item database index
↓
item ID = index + 1
↓
if ID <= 144:
sheet = (ID - 1) / 24
slot = (ID - 1) % 24
else:
fallback

Each 256-cell sheet is:

6 columns × 4 rows

So:

column = slot % 6
row = slot / 6

The source rectangle is therefore straightforward.

31. The 24 missing item sprites

We should not block implementation because items 145–168 don't have artwork.

For those 24:

Temporary solution

Use a clearly defined fallback.

Preferably:

generic item sprite

from one existing item slot rather than bringing back the ugly colored rectangle.

So:

145–168
↓
fallback item artwork

We should keep this mapping centralized.

Later:

new sheet added
↓
change 24 mappings

No gameplay changes.

32. Enemy rendering

Enemy rendering should use:

EnemyDatabase index

to resolve the sprite.

The current logic that changes color according to:

AI
attack pattern
special type
shield

should mostly disappear visually.

Instead:

actual enemy artwork

identifies the enemy.

But important gameplay overlays remain.

For example:

shielded
↓
draw shield ring/outline over sprite

rather than tinting the whole enemy.

33. Enemy dimensions

This needs careful handling.

Current enemies have:

enemy.w
enemy.h

which are gameplay/collision dimensions.

Do not assume the sprite must be rendered exactly at those dimensions.

Instead define a visual scale rule.

Something like:

sprite visual size based on enemy logical size

with a default scale.

The sprite should be centered around:

enemy.pos

so the existing collision position remains valid.

34. Enemy special effects

Current enemy visuals include:

shield
mimic
special types
attack-pattern color variations

After sprite implementation:

Keep
shield indicator
Remove/reduce
AI color coding
attack pattern color modification

because the actual sprite now provides identity.

Mimic

A Mimic enemy should still use its actual enemy sprite if one exists.

Its gameplay behavior remains unchanged.

35. Theme implementation — the big one

We have five theme sheets:

Ruines.png
Forge.png
crypt.png
fungus.png
dragonuc.png

representing:

RUINS
FORGE
CRYPT
FUNGAL
DRACONIC

The TXT defines a shared:

72-slot master asset system

This is exactly what we want.

36. Theme abstraction

The game should first determine:

current theme

Then:

theme + asset ID

resolves the visual.

For example:

RUINS + 01
→ Ruines floor

FORGE + 01
→ Forge floor

CRYPT + 01
→ crypt floor

FUNGAL + 01
→ fungus floor

DRACONIC + 01
→ dragonuc floor

Same logical asset.

Different image.

37. DO NOT rely on a fake uniform grid for all 72

This is important.

The theme sheets are not uniformly arranged for all 72 assets.

The first portions are much easier to map.

The later:

walls
arches
pillars
banners
statues
props

are irregular packed artwork.

Therefore we should use:

ThemeSpriteDefinition

with explicit:

source x
source y
source width
source height

for each asset.

38. Hand-authored theme rectangle table

This is where we spend some manual effort once.

For each theme:

72 entries

Each entry:

asset 01 → x/y/w/h
asset 02 → x/y/w/h
...
asset 72 → x/y/w/h

The five themes therefore have:

5 × 72
= 360 source rectangles

But this is data, not complicated code.

Once created, it becomes permanent.

39. Theme floors first

The first implementation pass should prioritize:

01–22

because these are the things that actually form room floors/hazards.

We need:

basic floor
floor variants
cracks
decorative floors
runes
hazards
pits
hazard borders
corners

This gives the dungeon immediate visual improvement.

40. Theme walls second

Then implement:

23–34

including:

straight wall
variants
corners
ends
caps
junctions
T junction
cross junction

These should replace the giant rectangular room walls currently generated in main.cpp.

41. Theme arches / door structures

Then:

35–44

for:

archways
doorways
gates
portcullis
entrance structures

This should work together with the m2 door sprites.

Important distinction:

Theme sheet

Provides the architectural structure/frame.

m2

Provides the gameplay door object/state.

We should not confuse the two.

42. Theme pillars

Then:

45–52

for:

pillars
broken pillars
damaged pillars
bases
fragments

These become standalone room decorations/structures.

43. Theme decorations

Then:

53–62

for:

banners
emblems
skulls
statues
altars
shrines
monuments

These can initially be placed wherever the procedural generator already creates decorative objects.

If the generator doesn't currently use all of them, we don't need to invent gameplay systems for them.

They can simply become available assets.

44. Theme props

Finally:

63–72

for:

barrels
crates
treasure piles
braziers
torches
cages
barriers
debris
destructible props
special theme props

Again:

visual integration first, gameplay integration only where an existing gameplay object corresponds to it.

45. Room floor rendering must be separated from room logic

Currently main.cpp appears to construct the visual room walls/floor directly.

We should avoid making main.cpp responsible for every sprite detail.

Create a visual room renderer layer conceptually:

DungeonRenderer / RoomRenderer

responsible for:

DrawRoomFloor()
DrawRoomWalls()
DrawRoomDoors()
DrawRoomTerrain()
DrawRoomProps()

Then main.cpp primarily says:

draw current room
draw entities
draw effects
draw HUD

This will make the code dramatically easier to maintain.

46. Rendering order

This is VERY important.

The final frame should be rendered in a deliberate order.

Recommended:

1. Clear framebuffer

2. Room floor

3. Floor decorations

4. Pits / hazards

5. Walls / room architecture

6. Environmental props

7. Doors / gates

8. Chests / stones

9. Pickup objects

10. Enemies

11. Boss

12. Player

13. Projectiles

14. Orbiters

15. VFX

16. Damage/shield overlays

17. HUD

18. Map

19. Screen banners

20. Present

Some of these may need slight adjustment depending on existing gameplay layering.

But the principle is:

background → environment → gameplay objects → effects → UI.

47. Screen shake

Existing screen shake should continue working.

The sprite renderer needs to accept the same:

shakeOffset

currently used by DrawRect.

So every world sprite gets:

position + shakeOffset

HUD does not get screen shake.

That preserves current behavior.

48. Camera/world coordinate handling

We should not introduce a complicated camera system unless necessary.

The current game already calculates positions for the 320×180 framebuffer.

Sprite drawing should use the existing coordinates.

The only new operation is:

world position
→ destination rectangle 49. Sprite pivots

For consistency, sprites should have a defined pivot.

For gameplay entities, ideally:

center

For room tiles:

top-left

For pickups:

center

For boss portraits:

center-bottom / center

This avoids every caller having to manually calculate offsets.

50. Item preview UI

Current item preview only displays:

name
description

We should eventually add the item sprite there.

So:

[ITEM SPRITE] Item Name
Description

This will make the item system feel much more finished.

The item sprite is already available through the item registry.

51. HUD health icons

The existing HUD heart drawing can remain for now.

The m2 heart sprites are gameplay pickups:

Half Heart
Full Heart

They don't necessarily need to replace the tiny HUD hearts.

This is an example of something we shouldn't over-engineer.

Use sprite assets where they improve the actual world.

Keep primitive rendering where it works better for UI.

52. Bombs

There are two concepts:

Bomb pickup

Use:

m2 #05
Active bomb in the world

Current bomb rendering is a small rectangle plus explosion flash.

Use:

m2 #05

for the bomb itself.

For explosion:

m1 #26

instead of the current large rectangle flash.

That will immediately improve visual quality.

53. Damage effects

The current game already has many places where things happen visually.

We should connect them to:

m1 VFX

For example:

enemy hit → #27 Hit
enemy destroyed → #28 Break
explosion → #26 Explosion
teleport → #29 Teleport
summon → #30 Summon
critical → #34 Critical Hit

This is where the sprite system starts making the game feel dramatically more polished.

54. Don't build a giant animation engine yet

The existing animation files are empty.

We do not need a full animation framework right now.

First implementation:

static sprite
state sprite
temporary effect sprite

That's enough.

Then, after everything is visually working, we can add:

frame animation
timers
loops
animation definitions

if the sheets actually contain multiple useful frames.

55. Asset loading failure handling

If an image fails to load:

don't crash

Instead:

log missing asset
use fallback
continue

This is especially important during development.

For example:

bosses3.png missing

should produce a clear error.

Not a mysterious blank game.

56. Asset validation at startup

We should add a startup validation pass.

It should verify:

item sheets exist
enemy sheets exist
m1 exists
m2 exists
boss sheets exist
theme sheets exist

and report missing files.

Then verify expected dimensions where appropriate.

For example:

item sheet = 1536×1024
enemy sheet = 1536×1024
m1 = expected dimensions
m2 = expected dimensions
theme sheets = 1536×1024

This will save us huge debugging time.

57. Sprite registry validation

We should also validate:

Item IDs
Enemy IDs
Boss IDs
M1 IDs
M2 IDs
Theme IDs

so invalid references don't silently render garbage.

58. File organization

The implementation should have a clean structure.

Conceptually:

engine/
renderer._
image._
sprite._
asset_manager._

game/
rendering/
sprite_registry._
room_renderer._
effects.\*

We don't necessarily have to use exactly those filenames, but the responsibilities should be separated.

59. What NOT to change

This is equally important.

We should not rewrite:

enemy AI
enemy spawning
item effects
item database
boss attack logic
dungeon generation
collision
procedural room generation
player movement
projectile physics
progression
room progression

unless a tiny rendering integration requires it.

The goal is:

same game

- new visual layer

not:

rewrite game 60. Implementation order

This is the order I recommend we actually execute.

PHASE A — Renderer
Add PNG decoding.
Add image representation.
Add asset loading.
Add alpha-aware sprite rendering.
Add source rectangle support.
Add destination scaling.
Add horizontal flip.
Add clipping.
Validate with one test sprite.

Do not touch every gameplay object yet.

First prove:

PNG → framebuffer

works.

61. PHASE B — Sprite registry

Create the central mapping system.

Implement:

M1 registry
M2 registry
item registry
enemy registry
boss registry
theme registry

At this stage we should be able to ask:

GetItemSprite(37)
GetEnemySprite(150)
GetBossSprite(17)
GetM1Sprite(M1_BULLET)
GetM2Sprite(M2_GOLD_CHEST_OPEN)
GetThemeSprite(theme, 27)

and receive a valid sprite definition.

62. PHASE C — basic world objects

Replace:

player rectangle
enemy rectangles
projectile rectangles
pickup rectangles
chest rectangles
stone rectangles
bomb rectangles

with actual sprites.

This gives us the fastest visual improvement.

63. PHASE D — terrain

Replace:

bridges
pads
terrain hazards
traps
pits

with m1 assets.

Then add the appropriate VFX.

64. PHASE E — doors and room architecture

Replace the current:

DrawRect wall
DrawRect doorway

logic with:

theme wall assets
theme architecture
m2 door sprites

This is where the game should stop looking like colored rectangles and start looking like a real dungeon.

65. PHASE F — themes

Implement:

RUINS
FORGE
CRYPT
FUNGAL
DRACONIC

through the shared 72-ID theme system.

First:

floors
walls

Then:

arches
pillars
decorations
props

The procedural generator keeps deciding what appears.

The sprite system decides how it looks.

66. PHASE G — bosses

Implement:

boss ID
→ correct sheet
→ correct portrait
→ crop artwork
→ remove baked-in name
→ scale
→ center on boss position

Then test all:

36 bosses

This needs a dedicated validation pass because these assets aren't normal gameplay sprite sheets.

67. Boss crop implementation specifically

For each boss:

Boss ID
↓
sheet
↓
12-cell location
↓
manual art crop
↓
sprite

The crop table should be data-driven.

Something conceptually like:

Boss 01:
sheet = bosses1
source = (...)

Boss 02:
sheet = bosses1
source = (...)

...

Boss 36:
sheet = bosses3
source = (...)

This is far safer than attempting to automatically detect the name text.

We should manually define the crop rectangles once.

The user explicitly wants the names cropped in code, and this is the correct way to do it.

68. Important boss filename cleanup

The uploaded ZIP currently has:

bosses.png
bosses1.png
bossses2.png

with the typo:

bossses2.png

and no:

bosses3.png

Your stated final state is:

bosses1.png
bosses2.png
bosses3.png

Before boss implementation, the actual assets need to conform to that.

But we don't need to redesign the mapping.

The intended 1–36 ordering remains exactly the same.

69. PHASE H — VFX

Once the basic sprites work:

Implement:

hit
explosion
break
teleport
summon
fire
poison
freeze
critical

using the m1 sheet.

This should be lightweight.

70. PHASE I — player polish

Then:

idle/walk
shoot
dash
hurt/death
horizontal flip

Tie them to existing player states.

No giant animation framework yet.

71. PHASE J — item preview / UI polish

Then add:

item sprite to item preview

while keeping the existing text system.

72. PHASE K — fallback/debug system

Add:

missing sprite fallback
sprite ID debug
asset load logging

Possibly a developer-only option to display:

sprite ID
entity name

over an entity while debugging.

Then disable it normally.

73. Testing strategy

We should NOT wait until everything is implemented before testing.

Test after each layer.

Test 1

One PNG:

DrawSprite(m1 player)

If this works, renderer is good.

Test 2

All m1 sprites.

Test 3

m2.

Test 4

one item/enemy.

Test 5

all items/enemies.

Test 6

theme floors/walls.

Test 7

boss portraits.

Test 8

complete gameplay run.

74. Sprite-sheet validation screen

I strongly recommend temporarily creating a developer sprite-test mode.

Something like:

SPRITE TEST

that displays:

01
02
03
...

for each sheet.

This makes it extremely easy to detect:

wrong row
wrong column
wrong filename
wrong crop
wrong scaling
wrong alpha
wrong boss mapping

We can remove/hide this once finished.

75. Full entity mapping verification

Before calling the sprite implementation complete, we should automatically verify:

168 item database entries
168 enemy database entries
36 boss database entries

against their sprite mappings.

Expected:

Items:
1–144 valid
145–168 fallback

Enemies:
1–168 valid

Bosses:
1–36 valid

No missing IDs.

76. Theme verification

For every theme:

72 asset definitions

must exist.

So:

RUINS 72
FORGE 72
CRYPT 72
FUNGAL 72
DRACONIC 72

The same logical number must always mean the same thing.

77. Performance

The internal resolution is only:

320 × 180 = 57,600 pixels

so software sprite rendering should be completely manageable.

However, we should still:

load PNGs once
avoid allocations during rendering
avoid decoding during rendering
use direct framebuffer access where useful
clip source/destination rectangles
keep sprite data in memory

The current software renderer is already extremely small.

78. Memory

The sheets are fairly large PNGs, but compressed PNG file size is not the same as decoded memory size.

For example:

1536 × 1024 × 4
≈ 6.3 MB

per decoded RGBA sheet.

We have multiple sheets.

So we should be conscious of memory, but on a normal desktop this is still reasonable.

We should load only the required asset sheets and keep them cached.

If needed later, themes can be loaded/unloaded between floors, but don't complicate the first implementation unless memory actually becomes a problem.

79. Final rendering architecture

The finished architecture should roughly be:

                    ┌────────────────────┐
                    │    AssetManager    │
                    └─────────┬──────────┘
                              │
          ┌───────────────────┼────────────────────┐
          │                   │                    │
      PNG Images         Sprite Registry       Theme Registry
          │                   │                    │
          └───────────────────┼────────────────────┘
                              │
                       Sprite Definition
                              │
                              ↓
                    Renderer::DrawSprite
                              │
                              ↓
                       320 × 180 Buffer
                              │
                              ↓
                       StretchDIBits

And game logic remains:

Player
Enemy
Boss
Item
Room
Terrain
Projectile

with no knowledge of PNG internals.

80. The final asset coverage

When we're finished, the game will have visual coverage for:

Player
5 m1 sprites
Projectiles
5 m1 sprites
Orbiters
2 m1 sprites
Terrain
8 m1 sprites
Traps
5 m1 sprites
VFX
9 m1 sprites
Trophy
1 m1 sprite
Pickups/chests/doors/stones
32 m2 sprites
Items
144 actual
24 fallback
Enemies
168 actual
Bosses
36 actual portraits
with code-defined artwork crops
Themes
5 themes × 72 logical assets

That is essentially the entire visual asset set you've provided.

81. What the implementation should NOT do

We should avoid these traps:

❌ Don't rewrite the dungeon generator.

❌ Don't rewrite enemy AI.

❌ Don't change item ordering.

❌ Don't change enemy ordering.

❌ Don't change boss ordering.

❌ Don't try to automatically recognize boss names.

❌ Don't modify the boss PNGs externally.

❌ Don't wait for new boss sprites.

❌ Don't regenerate the theme sheets.

❌ Don't force the irregular theme atlas into a fake uniform grid.

❌ Don't create a huge animation engine before the sprites are working.

❌ Don't make every gameplay system know about PNG coordinates.

❌ Don't load images every frame.

82. The actual implementation milestones

I would break the work into these concrete milestones:

M0 — Audit/asset contract

Confirm filenames, mappings and expected dimensions.

M1 — PNG/image loader

Get PNG → RGBA memory.

M2 — Sprite renderer

Get source rectangle → framebuffer with alpha.

M3 — Asset manager

Load/cache all sheets.

M4 — Sprite registry

Centralize every mapping.

M5 — m1 integration

Player/projectiles/orbiters/terrain/traps/VFX.

M6 — m2 integration

Pickups/chests/doors/stones.

M7 — Item integration

144 sprites + 24 fallback mappings.

M8 — Enemy integration

All 168 enemies.

M9 — Theme integration

All five themes, beginning with floors/walls.

M10 — Boss integration

All 36 portraits + manual artwork crop rectangles.

M11 — Rendering-order polish

Fix layering, offsets, scaling and screen shake.

M12 — VFX polish

Connect hit/explosion/etc. to actual gameplay events.

M13 — Validation

Test every asset ID and every major gameplay state.

M14 — Cleanup

Remove temporary debug rendering and unused rectangle visuals.

83. The most important implementation principle

The key design decision is this:

Game IDs are permanent. Sprite locations are implementation details.

For example:

Item #37

always means the 37th item in items.txt.

Today:

Item #37 → item2.png slot 13

If the artwork changes tomorrow:

Item #37 → completely different PNG

the game logic doesn't care.

Same for:

Enemy #150
Boss #17
Theme asset #27

This gives us a proper asset abstraction instead of scattering sprite coordinates all over the game.
