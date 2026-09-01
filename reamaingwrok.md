FloppyRogue — Remaining Work

Status snapshot from our session. "Done" items were confirmed applied by you; everything else is either partially done (template only) or not started.

✅ Done
 Duplicate item names fixed — Overclock/OverdriveCore, SecondWind/LastStand, duplicate RicochetCore block removed (data/items.txt)
 Dead JSON files deleted (data/enemies.json, data/items.json, data/rooms.json)
 Chest open odds fixed so paid chests (Iron/Stone) beat the free Wooden chest (src/main.cpp, ChestItemChance)
 Dash Strike mechanic added — dashing through enemies now deals chip damage + knockback (player.h, player.cpp, src/main.cpp contact block)
 Difficulty pacing — turtling in a room past ~25s spawns a reinforcement enemy (game/rooms/room.h, src/main.cpp)
⚠️ Partially done — template applied, not rolled out everywhere
 Boss roster (24 → 32): 8 new boss blocks added to data/bosses.txt to fix the tier-4/5 modulo-wraparound bug. Verify BossDatabase::Count() now returns 32 and that floor 4/5 bosses no longer resolve to tier-1 templates.
 Boss signature moves: only Titan got a real unique mechanic (VORTEX_PULL). The other 31 bosses (including the 8 new ones) still differ by stats/cycle-order only, not by mechanic. Repeat the VORTEX_PULL pattern (new BossAttackType + dedicated boss field + wiring in boss.cpp/main.cpp) for however many more signature moves you want — 3–5 more marquee bosses (floor 3+) would go a long way.
 MIRROR_SHOT rework: confirm FireMirrorShot was actually added and the ExecuteAttack switch case in boss.cpp was repointed away from FireGappedRing.
 Item synergies — boss side: the four synergy pairs (Shatter, Toxic Blast, Mark-on-chain) were written for UpdateAndCollideVsEnemy only. Mirror the same three blocks into the boss-collision function (UpdateAndCollideVsBoss or equivalent, ~line 594 in projectile_system.cpp) so bosses aren't exempt from those combos.
 Chain + Mark synergy placement: double-check the best->markStacks++ block landed directly after best->hp -= inside the chain-jump loop and that it compiles — this one was described by location, not shown as a full verbatim diff.
 Tier-4/5 enemies: 8 new blocks added to data/enemies.txt as a starting batch. Consider a second pass if floors 4/5+ still feel like reused floor-3 content.
❌ Not started
 More floors (total_floors in data/rooms.txt): hold off until the boss/enemy tier-4/5 content above is actually filled out, or the new floors will just repeat existing content.
 Meta-progression system: discussed conceptually only (e.g. persistent currency that unlocks new items into the pool across runs). This needs real save-file I/O in the Win32 app — a new feature to design and build, not a data-file patch.
 Grid size scaling past floor 5 (grid_size_max in data/rooms.txt): only relevant once floor count is actually raised — currently caps at 30x30.
Untouched by request
Curse rooms — deliberately left as-is, per your instruction.
Suggested next order
Verify the "partially done" boss/synergy items above actually compile and behave as intended (send me the updated zip and I'll check).
Decide how many more bosses get real signature moves vs. staying stat-only.
Then raise total_floors.
Meta-progression last — biggest scope, least urgent for the contest deadline.