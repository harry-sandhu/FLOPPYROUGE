#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
#include "../engine/window.h"
#include "../engine/renderer.h"
#include "../engine/input.h"
#include "../engine/core/timer.h"
#include "../engine/core/rng.h"
#include "../engine/text.h"
#include "../game/player/player.h"
#include "../game/player/projectile_system.h"
#include "../game/items/item_database.h"
#include "../game/items/item_system.h"
#include "../game/enemies/enemy.h"
#include "../game/enemies/enemy_database.h"
#include "../game/bosses/boss.h"
#include "../game/bosses/boss_database.h"
#include "../game/dungeon/dungeon.h"
#include "../game/ui/hud.h"
#include "../engine/collision.h"

namespace {
    void ResolveEntitySolidCollision(Vec2& pos, float entityW, float entityH, const Rect& obstacleRect) {
        Rect entityRect = { pos.x, pos.y, entityW, entityH };
        float pLeft = entityRect.x, pRight = entityRect.x + entityRect.w;
        float pTop = entityRect.y, pBottom = entityRect.y + entityRect.h;
        float oLeft = obstacleRect.x, oRight = obstacleRect.x + obstacleRect.w;
        float oTop = obstacleRect.y, oBottom = obstacleRect.y + obstacleRect.h;

        float overlapX = std::min(pRight, oRight) - std::max(pLeft, oLeft);
        float overlapY = std::min(pBottom, oBottom) - std::max(pTop, oTop);
        if (overlapX <= 0.0f || overlapY <= 0.0f) return;

        if (overlapX < overlapY) {
            float entityCenterX = pLeft + entityRect.w * 0.5f;
            float obstacleCenterX = oLeft + obstacleRect.w * 0.5f;
            pos.x += (entityCenterX < obstacleCenterX) ? -overlapX : overlapX;
        } else {
            float entityCenterY = pTop + entityRect.h * 0.5f;
            float obstacleCenterY = oTop + obstacleRect.h * 0.5f;
            pos.y += (entityCenterY < obstacleCenterY) ? -overlapY : overlapY;
        }
    }

    void ResolvePlayerSolidCollision(Player& player, const Rect& obstacleRect) {
        ResolveEntitySolidCollision(player.pos, (float)player.size, (float)player.size, obstacleRect);
    }

    void BuildTreasureSenseLine(const Dungeon& dungeon, const Player& player, char* out, size_t outSize) {
        if (!out || outSize == 0) return;
        out[0] = '\0';
        if (!player.hasTreasureSense) return;

        const Room* treasureRoom = nullptr;
        for (const Room& room : dungeon.Rooms()) {
            if (room.type == RoomType::TREASURE) {
                treasureRoom = &room;
                break;
            }
        }
        if (!treasureRoom) return;

        auto Append = [&](const char* text) {
            if (!text || text[0] == '\0') return;
            size_t len = std::strlen(out);
            if (len >= outSize - 1) return;
            std::snprintf(out + len, outSize - len, "%s", text);
        };

        Append("TREASURE: ");
        if (treasureRoom->itemSpawnList.empty()) {
            Append("EMPTY");
            return;
        }

        int shown = 0;
        for (int itemId : treasureRoom->itemSpawnList) {
            const ItemTemplate* item = ItemDatabase::Get(itemId);
            if (!item || item->name[0] == '\0') continue;

            if (shown > 0) Append(", ");
            Append(item->name);
            shown++;
        }

        if (shown == 0) {
            Append("EMPTY");
        }
    }

    void NormalizeCoins(Player& player) {
        int total = std::max(0, player.nickelCoins + player.silverCoins * 5 + player.goldCoins * 10);
        player.goldCoins = total / 10;
        total %= 10;
        player.silverCoins = total / 5;
        total %= 5;
        player.nickelCoins = total;
    }

    void AddCoinValue(Player& player, int value) {
        if (value <= 0) return;
        int total = player.nickelCoins + player.silverCoins * 5 + player.goldCoins * 10 + value;
        player.goldCoins = total / 10;
        total %= 10;
        player.silverCoins = total / 5;
        total %= 5;
        player.nickelCoins = total;
    }

    bool SpendCoins(Player& player, int cost) {
        if (cost <= 0) return true;
        int total = player.nickelCoins + player.silverCoins * 5 + player.goldCoins * 10;
        if (total < cost) return false;
        total -= cost;
        player.goldCoins = total / 10;
        total %= 10;
        player.silverCoins = total / 5;
        total %= 5;
        player.nickelCoins = total;
        return true;
    }

    const char* ChestTypeName(ChestType type) {
        switch (type) {
            case ChestType::WOODEN: return "WOODEN";
            case ChestType::IRON:   return "IRON";
            case ChestType::STONE:  return "STONE";
            case ChestType::GOLDEN: return "GOLDEN";
            case ChestType::DEVIL:  return "DEVIL";
            case ChestType::ANGEL:  return "ANGEL";
            case ChestType::GAMBLE: return "GAMBLE";
        }
        return "CHEST";
    }

    uint32_t ChestColor(ChestType type) {
        switch (type) {
            case ChestType::WOODEN: return 0xFF8B5A2B;
            case ChestType::IRON:   return 0xFF9BA4B5;
            case ChestType::STONE:  return 0xFF777777;
            case ChestType::GOLDEN: return 0xFFFFC84D;
            case ChestType::DEVIL:  return 0xFFFF5555;
            case ChestType::ANGEL:  return 0xFFFFFF99;
            case ChestType::GAMBLE: return 0xFFFF00FF;
        }
        return 0xFFFFFFFF;
    }

    float ChestItemChance(ChestType type, int attempt = 0) {
        switch (type) {
            case ChestType::WOODEN: return 0.10f;
            case ChestType::IRON:   return 0.05f;
            case ChestType::STONE:  return 0.08f;
            case ChestType::GOLDEN: return 0.10f;
            case ChestType::DEVIL:  return 0.50f;
            case ChestType::ANGEL:
                return std::clamp(0.10f + 0.10f * (float)attempt, 0.10f, 0.50f);
            case ChestType::GAMBLE: return 1.0f;
        }
        return 0.0f;
    }

    int ShopPriceForItem(const ItemTemplate* item, int floor) {
        if (!item) return 0;
        int base = 8 + floor * 2;
        base += (item->tier - 1) * 4;
        switch (item->type) {
            case ItemType::UNLOCK: base += 8; break;
            case ItemType::PROC_SYNERGY: base += 6; break;
            case ItemType::STAT_MOD: base += 2; break;
        }

        if (item->flag != ItemFlag::UNKNOWN) base += 5;
        if (item->stat == ItemStat::MAX_HP || item->stat == ItemStat::DAMAGE || item->stat == ItemStat::PROJECTILE_COUNT) {
            base += 3;
        }
        if (base < 1) base = 1;
        return base;
    }

    int PickItemForPools(const char* pools, int minTier, int maxTier) {
        return ItemDatabase::Pick(pools, minTier, maxTier);
    }
}

enum class GameState { TITLE, FLOOR_TRANSITION, PLAYING, GAME_OVER };

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    if (!Window::Create(1280, 720, "FloppyRogue")) return 1;
    if (!Renderer::Init(Window::GetHandle())) return 1;

    EnemyDatabase::Load("data/enemies.txt");
    ItemDatabase::Load("data/items.txt");
    BossDatabase::Load("data/bosses.txt");

    Timer timer;
    Dungeon dungeon;
    dungeon.LoadSettings("data/rooms.txt");

    struct Bomb {
        Vec2 pos = { 0.0f, 0.0f };
        float fuseTimer = 3.0f;
        float flashTimer = 0.0f;
        bool exploded = false;
    };

    Player player;
    std::vector<Enemy> enemies;
    Boss boss;
    std::vector<Projectile> playerShots;
    std::vector<Projectile> enemyShots;
    std::vector<Bomb> bombs;
    float screenShakeTimer = 0.0f;
    float screenShakeStrength = 0.0f;
    float worldTime = 0.0f;

    const float invincibleDuration = 0.75f;
    const float projectileSize = 3.0f;
    const float bombFuseDuration = 3.0f;
    const float bombExplosionRadius = 34.0f;
    const int bombDamage = 40;

    // Contact damage in half-heart units: normal enemies poke for half a
    // heart, special-variant enemies (Reinforcer/Creeper/Death Ring) hit
    // for a full heart. Floor 3's existing DamagePlayer clamp bumps normal
    // hits up to a full heart automatically once these are this small.
    auto ContactDamageFor = [](const Enemy& enemy) -> int {
        return (enemy.specialType == EnemySpecialType::NONE) ? 1 : 2;
    };

    GameState state = GameState::TITLE;

    auto AddScreenShake = [&](float duration, float strength) {
        screenShakeTimer = std::max(screenShakeTimer, duration);
        screenShakeStrength = std::max(screenShakeStrength, strength);
    };

    float floorTransitionTimer = 0.0f;
    int floorTransitionFloor = 1;
    char floorTransitionTreasureLine[160] = {};

    auto ClearRunEntities = [&]() {
        enemies.clear();
        enemyShots.clear();
        playerShots.clear();
        bombs.clear();
        boss = Boss{};
        boss.alive = false;
    };

    auto BeginFloorTransition = [&]() {
        floorTransitionTimer = 0.0f;
        floorTransitionFloor = dungeon.CurrentFloor();
        BuildTreasureSenseLine(dungeon, player, floorTransitionTreasureLine, sizeof(floorTransitionTreasureLine));
        screenShakeTimer = 0.0f;
        screenShakeStrength = 0.0f;
        state = GameState::FLOOR_TRANSITION;
    };

    auto DamagePlayer = [&](int amount) {
        if (dungeon.CurrentFloor() >= 3) {
            amount = std::max(amount, 2);
        }
        if (PlayerLogic::TakeDamage(player, amount, invincibleDuration)) {
            AddScreenShake(0.14f, 2.0f);
        }
    };

    auto SpawnTreasurePickups = [&](Room& room) {
        room.pickups.clear();
        constexpr float ITEM_MIMIC_CHANCE = 0.10f;

        const Vec2 center = {
            room.x + room.width * 0.5f - 4.0f,
            room.y + room.height * 0.5f - 4.0f
        };

        const Vec2 offsets[] = {
            { 0.0f, 0.0f },
            { -18.0f, 0.0f },
            { 18.0f, 0.0f },
            { 0.0f, -14.0f },
            { 0.0f, 14.0f }
        };

        for (int i = 0; i < (int)room.itemSpawnList.size(); ++i) {
            RoomPickup pickup;
            pickup.type = RoomPickupType::ITEM;
            pickup.itemId = room.itemSpawnList[i];
            pickup.pos = {
                center.x + offsets[i % 5].x + (float)(i / 5) * 8.0f,
                center.y + offsets[i % 5].y
            };
            pickup.isMimic = RNG::Chance(ITEM_MIMIC_CHANCE);
            room.pickups.push_back(pickup);
        }
    };

    auto SpawnDevilChestRoom = [&](Room& room) {
        room.pickups.clear();

        const Vec2 center = {
            room.x + room.width * 0.5f - 4.0f,
            room.y + room.height * 0.5f - 4.0f
        };

        RoomPickup pickup;
        pickup.type = RoomPickupType::CHEST;
        pickup.chestType = ChestType::DEVIL;
        pickup.itemId = PickItemForPools("CURSE,BOSS,CHEST", 3, 5);
        pickup.pos = { center.x, center.y };
        room.pickups.push_back(pickup);
    };

    auto SpawnShopStock = [&](Room& room) {
        room.pickups.clear();

        const Vec2 center = {
            room.x + room.width * 0.5f - 4.0f,
            room.y + room.height * 0.5f - 4.0f
        };

        const Vec2 offsets[] = {
            { -34.0f, 0.0f },
            { 0.0f, 0.0f },
            { 34.0f, 0.0f },
            { -17.0f, 18.0f },
            { 17.0f, 18.0f }
        };

        int index = 0;
        auto AddShopPickup = [&](RoomPickupType type, int itemId, int cost) {
            RoomPickup pickup;
            pickup.type = type;
            pickup.itemId = itemId;
            pickup.cost = cost;
            pickup.pos = {
                center.x + offsets[index % 5].x,
                center.y + offsets[index % 5].y
            };
            index++;
            room.pickups.push_back(pickup);
        };

        int itemCount = ItemDatabase::Count();
        if (itemCount > 0) {
            int shopItems = std::min(2, itemCount);
            int maxTier = std::clamp(2 + dungeon.CurrentFloor(), 2, 5);
            for (int i = 0; i < shopItems; ++i) {
                int itemId = PickItemForPools("SHOP,TREASURE,CHEST", 1, maxTier);
                if (itemId < 0) itemId = RNG::Range(0, itemCount - 1);
                const ItemTemplate* item = ItemDatabase::Get(itemId);
                AddShopPickup(RoomPickupType::ITEM, itemId, ShopPriceForItem(item, dungeon.CurrentFloor()));
            }
        }

        AddShopPickup(RoomPickupType::HEART, -1, std::max(3, 4 + dungeon.CurrentFloor()));
        AddShopPickup(RoomPickupType::BOMB, -1, std::max(3, 5 + dungeon.CurrentFloor()));
        AddShopPickup(RoomPickupType::KEY, -1, std::max(4, 6 + dungeon.CurrentFloor()));
    };

    auto SpawnBossRewards = [&](Room& room) {
        room.pickups.clear();

        const Vec2 center = {
            room.x + room.width * 0.5f - 4.0f,
            room.y + room.height * 0.5f - 4.0f
        };

        if (dungeon.CurrentFloor() < dungeon.MaxFloors()) {
            if (!room.itemSpawnList.empty()) {
                RoomPickup itemPickup;
                itemPickup.type = RoomPickupType::ITEM;
                itemPickup.itemId = room.itemSpawnList[0];
                itemPickup.pos = { center.x, center.y - 12.0f };
                room.pickups.push_back(itemPickup);
            }

            RoomPickup exitPickup;
            exitPickup.type = RoomPickupType::EXIT;
            exitPickup.pos = { center.x, center.y + 10.0f };
            room.pickups.push_back(exitPickup);
        } else {
            RoomPickup trophyPickup;
            trophyPickup.type = RoomPickupType::TROPHY;
            trophyPickup.pos = { center.x, center.y };
            room.pickups.push_back(trophyPickup);
        }

        room.lootGranted = true;
    };

    auto StartRun = [&]() {
        player = Player{};
        ClearRunEntities();
        screenShakeTimer = 0.0f;
        screenShakeStrength = 0.0f;

        uint32_t seedBase = (uint32_t)GetTickCount();
        bool generated = false;
        for (int attempt = 0; attempt < 8 && !generated; ++attempt) {
            generated = dungeon.Generate(seedBase + (uint32_t)attempt * 17u);
        }

        if (!generated) {
            state = GameState::GAME_OVER;
            return;
        }

        dungeon.PlacePlayerAtCurrentRoomCenter(player);
        BeginFloorTransition();
    };

    auto ScaleEnemyForFloor = [&](Enemy& enemy) {
        int floor = dungeon.CurrentFloor();
        float floorOffset = (float)(floor - 1);

        enemy.hp = (int)(enemy.hp * (1.0f + 0.25f * floorOffset));
        if (enemy.hp < 1) enemy.hp = 1;
        enemy.maxHp = enemy.hp;
        enemy.speed *= 1.0f + 0.08f * floorOffset;
        enemy.shootCooldown *= 1.0f - 0.06f * floorOffset;
        if (enemy.shootCooldown < 0.50f) enemy.shootCooldown = 0.50f;
        enemy.shootRange *= 1.0f + 0.04f * floorOffset;
        enemy.preferredDistance *= 1.0f + 0.02f * floorOffset;
        enemy.shotSpeed *= 1.0f + 0.04f * floorOffset;
        enemy.spawnDelayRemaining = 1.0f;
        enemy.attackDelayRemaining = 1.5f;
    };

    auto ScaleBossForFloor = [&](Boss& boss) {
        int floor = dungeon.CurrentFloor();
        float floorOffset = (float)(floor - 1);

        boss.hp = (int)(boss.hp * (1.0f + 0.35f * floorOffset));
        if (boss.hp < 1) boss.hp = 1;
        boss.maxHp = boss.hp;
        boss.driftSpeed *= 1.0f + 0.06f * floorOffset;
        boss.attackCooldownPhase1 *= 1.0f - 0.05f * floorOffset;
        boss.attackCooldownPhase2 *= 1.0f - 0.05f * floorOffset;
        if (boss.attackCooldownPhase1 < 0.85f) boss.attackCooldownPhase1 = 0.85f;
        if (boss.attackCooldownPhase2 < 0.60f) boss.attackCooldownPhase2 = 0.60f;
        boss.chargeSpeed *= 1.0f + 0.06f * floorOffset;
        boss.spawnDelayRemaining = 1.0f;
        boss.attackDelayRemaining = 1.5f;
        boss.attackTimer = boss.attackCooldownPhase1;
    };

    auto MakeSpecialEnemy = [&](Enemy& enemy) {
        if (!RNG::Chance(dungeon.SpecialEnemyChance())) return;

        enemy.specialType = (EnemySpecialType)RNG::Range(1, 3);
        enemy.creepDropTimer = 0.0f;

        switch (enemy.specialType) {
            case EnemySpecialType::REINFORCER:
                enemy.hp = (int)(enemy.hp * 1.45f) + 6;
                enemy.speed *= 1.06f;
                enemy.shootCooldown *= 0.92f;
                enemy.shootRange *= 1.05f;
                break;
            case EnemySpecialType::CREEPER:
                enemy.hp = (int)(enemy.hp * 1.20f) + 3;
                enemy.speed *= 1.10f;
                enemy.shootCooldown *= 0.94f;
                enemy.shootRange *= 1.08f;
                break;

            
                
            case EnemySpecialType::DEATH_RING:
                enemy.hp = (int)(enemy.hp * 1.30f) + 5;
                enemy.speed *= 1.04f;
                enemy.shootCooldown *= 0.90f;
                enemy.shotSpeed *= 1.05f;
                break;
            case EnemySpecialType::NONE:
            default:
                break;
        }

        if (enemy.hp < 1) enemy.hp = 1;
        enemy.maxHp = enemy.hp;
        enemy.spawnDelayRemaining = 1.0f;
        enemy.attackDelayRemaining = 1.5f;
    };

    auto LoadRoomEncounter = [&]() {
        const Room& room = dungeon.CurrentRoom();
        const Vec2 spawnPoints[] = {
            { 60.0f, 30.0f },
            { 220.0f, 30.0f },
            { 60.0f, 110.0f },
            { 220.0f, 110.0f },
            { 140.0f, 60.0f }
        };

        enemies.clear();
        enemyShots.clear();
        playerShots.clear();
        boss = Boss{};
        boss.alive = false;

        if (player.hasShieldCharm) {
            player.shieldCharges = std::max(player.shieldCharges, player.hasBulwarkCore ? 2 : 1);
        }
        if (player.hasMirrorWard) {
            player.mirrorWardCharges = 1;
        }
        if (player.hasRegenCharm && player.hp < player.maxHp) {
            player.hp = std::min(player.maxHp, player.hp + 1);
            player.actionFlashTimer = std::max(player.actionFlashTimer, 0.05f);
        }

        if ((room.type == RoomType::NORMAL || room.IsEnemyCurseRoom()) && !room.cleared) {
            if (!room.enemySpawnList.empty()) {
                for (int i = 0; i < (int)room.enemySpawnList.size(); ++i) {
                    Vec2 spawnPos = spawnPoints[i % (int)(sizeof(spawnPoints) / sizeof(spawnPoints[0]))];
                    Enemy enemy = EnemyDatabase::Spawn(room.enemySpawnList[i], spawnPos);
                    ScaleEnemyForFloor(enemy);
                    MakeSpecialEnemy(enemy);
                    enemies.push_back(enemy);
                }
            } else {
                Enemy zombie = EnemyDatabase::Spawn("Zombie", { 100.0f, 30.0f });
                Enemy gunner = EnemyDatabase::Spawn("Gunner", { 220.0f, 30.0f });
                ScaleEnemyForFloor(zombie);
                ScaleEnemyForFloor(gunner);
                MakeSpecialEnemy(zombie);
                MakeSpecialEnemy(gunner);
                enemies.push_back(zombie);
                enemies.push_back(gunner);
            }
        } else if (room.type == RoomType::BOSS && !room.cleared) {
            boss = SpawnBossVariant(room.bossVariant);
            ScaleBossForFloor(boss);
       } else if (room.type == RoomType::TREASURE && !room.lootGranted) {
            SpawnTreasurePickups(dungeon.CurrentRoom());
            dungeon.CurrentRoom().lootGranted = true;
       } else if (room.type == RoomType::CURSE && !room.IsEnemyCurseRoom() && !room.lootGranted) {
            SpawnDevilChestRoom(dungeon.CurrentRoom());
            dungeon.CurrentRoom().lootGranted = true;
       } else if (room.type == RoomType::SHOP && !room.lootGranted) {
            SpawnShopStock(dungeon.CurrentRoom());
            dungeon.CurrentRoom().lootGranted = true;
        }
    };

    auto EnterGameplay = [&]() {
        state = GameState::PLAYING;
        LoadRoomEncounter();
    };

    auto TryCollectCurrentRoomPickups = [&]() -> bool {
        Room& room = dungeon.CurrentRoom();
        const float pickupSize = 8.0f;

        auto GrantHeart = [&]() {
            player.hp = std::min(player.hp + 2, player.maxHp);
            player.actionFlashTimer = std::max(player.actionFlashTimer, 0.08f);
            AddScreenShake(0.06f, 0.8f);
        };

        auto GrantBomb = [&]() {
            player.bombCount++;
            player.actionFlashTimer = std::max(player.actionFlashTimer, 0.08f);
            AddScreenShake(0.06f, 0.8f);
        };

        auto GrantKey = [&]() {
            player.keyCount++;
            player.actionFlashTimer = std::max(player.actionFlashTimer, 0.08f);
            AddScreenShake(0.06f, 0.8f);
        };

        auto GrantCoins = [&](int value) {
            AddCoinValue(player, value);
            player.actionFlashTimer = std::max(player.actionFlashTimer, 0.08f);
            AddScreenShake(0.06f, 0.8f);
        };

        auto GrantItemById = [&](int itemId) {
            if (itemId < 0) return false;
            ItemSystem::GrantItem(player, itemId);
            player.actionFlashTimer = std::max(player.actionFlashTimer, 0.08f);
            AddScreenShake(0.06f, 0.8f);
            return true;
        };

        auto PickFallbackItemId = [&]() -> int {
            if (!room.itemSpawnList.empty()) {
                return room.itemSpawnList[RNG::Range(0, (int)room.itemSpawnList.size() - 1)];
            }
            int itemCount = ItemDatabase::Count();
            if (itemCount <= 0) return -1;
            return RNG::Range(0, itemCount - 1);
        };

        auto SpawnMimicAmbush = [&](const RoomPickup& pickup) -> bool {
            if (!EnemyDatabase::Exists("Mimic")) return false;
            Enemy mimic = EnemyDatabase::Spawn("Mimic", { pickup.pos.x - 1.0f, pickup.pos.y - 1.0f });
            ScaleEnemyForFloor(mimic);
            mimic.spawnDelayRemaining = 0.10f;
            mimic.attackDelayRemaining = 0.35f;
            enemies.push_back(mimic);
            AddScreenShake(0.10f, 1.2f);
            return true;
        };

        auto SpawnItemMimicAmbush = [&](const RoomPickup& pickup) -> bool {
            if (!EnemyDatabase::Exists("ItemMimic")) return SpawnMimicAmbush(pickup);
            Enemy mimic = EnemyDatabase::Spawn("ItemMimic", { pickup.pos.x - 1.0f, pickup.pos.y - 1.0f });
            ScaleEnemyForFloor(mimic);
            mimic.spawnDelayRemaining = 0.10f;
            mimic.attackDelayRemaining = 0.35f;
            mimic.mimicsItemPickup = true;
            mimic.mimicItemId = pickup.itemId;
            enemies.push_back(mimic);
            AddScreenShake(0.10f, 1.2f);
            return true;
        };

        auto OpenChest = [&](RoomPickup& pickup) -> bool {
            if (RNG::Chance(0.12f)) {
                return SpawnMimicAmbush(pickup);
            }

            int rewardItemId = (pickup.itemId >= 0) ? pickup.itemId : PickFallbackItemId();

            switch (pickup.chestType) {
                case ChestType::WOODEN: {
                    if (RNG::Chance(ChestItemChance(pickup.chestType))) {
                        if (GrantItemById(rewardItemId)) return true;
                    }
                    int roll = RNG::Range(0, 99);
                    if (roll < 35) GrantCoins(1);
                    else if (roll < 65) GrantHeart();
                    else if (roll < 85) GrantBomb();
                    else GrantKey();
                    return true;
                }
                case ChestType::IRON:
                case ChestType::STONE: {
                    if (pickup.chestType == ChestType::IRON) {
                        // Iron chest: costs 1 heart damage to open
                        if (player.hp <= 1) return false; // Can't open if it would be fatal
                        player.hp--;
                    } else {
                        // Stone chest: costs a bomb to open
                        if (player.bombCount <= 0) return false;
                        player.bombCount--;
                    }
                    if (RNG::Chance(ChestItemChance(pickup.chestType))) {
                        return GrantItemById(rewardItemId);
                    }
                    if (pickup.chestType == ChestType::IRON) {
                        int roll = RNG::Range(0, 99);
                        if (roll < 50) GrantBomb();
                        else if (roll < 80) GrantCoins(5);
                        else GrantHeart();
                    } else {
                        int roll = RNG::Range(0, 99);
                        if (roll < 30) GrantCoins(10);
                        else if (roll < 70) GrantKey();
                        else GrantBomb();
                    }
                    return true;
                }
                case ChestType::GOLDEN: {
                    if (player.keyCount <= 0) return false;
                    player.keyCount--;
                    if (RNG::Chance(ChestItemChance(pickup.chestType))) {
                        return GrantItemById(rewardItemId);
                    }
                    int roll = RNG::Range(0, 99);
                    if (roll < 45) GrantCoins(10);
                    else if (roll < 75) GrantKey();
                    else GrantBomb();
                    return true;
                }
                case ChestType::DEVIL: {
                    bool combatRoom = (room.type == RoomType::NORMAL || room.IsEnemyCurseRoom());
                    if (combatRoom && RNG::Chance(0.50f)) {
                        room.cleared = false;
                        room.gateOpen = false;

                        const Vec2 spawnPoints[] = {
                            { 68.0f, 34.0f },
                            { 210.0f, 34.0f }
                        };
                        for (int i = 0; i < 2; ++i) {
                            const char* enemyName = (RNG::Chance(0.5f)) ? "Zombie" : "Fly";
                            Enemy enemy = EnemyDatabase::Spawn(enemyName, spawnPoints[i]);
                            ScaleEnemyForFloor(enemy);
                            MakeSpecialEnemy(enemy);
                            enemies.push_back(enemy);
                        }
                        return true;
                    }

                    if (RNG::Chance(ChestItemChance(pickup.chestType))) {
                        return GrantItemById(rewardItemId);
                    }

                    int roll = RNG::Range(0, 99);
                    if (roll < 50) GrantCoins(10);
                    else if (roll < 80) GrantBomb();
                    else GrantKey();
                    return true;
                }
                case ChestType::ANGEL: {
                    if (player.keyCount <= 0) return false;
                    player.keyCount--;
                    pickup.chestAttempts++;
                    if (RNG::Chance(ChestItemChance(pickup.chestType, pickup.chestAttempts - 1))) {
                        return GrantItemById(rewardItemId);
                    }
                    if (pickup.chestAttempts >= 5) {
                        return true;
                    }
                    return false;
                }
                case ChestType::GAMBLE: {
                    // Gamble chest: costs 4-10 random keys, gives 3 items if successful
                    int keyCost = RNG::Range(4, 10);
                    if (player.keyCount < keyCost) return false;
                    player.keyCount -= keyCost;
                    
                    // Grant 3 items
                    for (int i = 0; i < 3; ++i) {
                        int itemId = PickFallbackItemId();
                        GrantItemById(itemId);
                    }
                    return true;
                }
            }

            return false;
        };

        for (auto& pickup : room.pickups) {
            if (pickup.collected) continue;
            Rect pickupRect = { pickup.pos.x, pickup.pos.y, pickupSize, pickupSize };
            if (!Collision::CheckAABB(player.GetRect(), pickupRect)) continue;

            switch (pickup.type) {
                case RoomPickupType::ITEM:
                    if (pickup.cost > 0 && !SpendCoins(player, pickup.cost)) {
                        continue;
                    }
                    if (pickup.isMimic) {
                        if (!SpawnItemMimicAmbush(pickup)) continue;
                        pickup.collected = true;
                        break;
                    }
                    if (pickup.itemId >= 0) {
                        GrantItemById(pickup.itemId);
                    }
                    pickup.collected = true;
                    break;

                case RoomPickupType::HEART:
                    if (pickup.cost > 0 && !SpendCoins(player, pickup.cost)) {
                        continue;
                    }
                    GrantHeart();
                    pickup.collected = true;
                    break;

                case RoomPickupType::BOMB:
                    if (pickup.cost > 0 && !SpendCoins(player, pickup.cost)) {
                        continue;
                    }
                    GrantBomb();
                    pickup.collected = true;
                    break;

                case RoomPickupType::KEY:
                    if (pickup.cost > 0 && !SpendCoins(player, pickup.cost)) {
                        continue;
                    }
                    if (pickup.amount > 0) {
                        player.keyCount += pickup.amount;
                        player.actionFlashTimer = std::max(player.actionFlashTimer, 0.08f);
                        AddScreenShake(0.06f, 0.8f);
                    } else {
                        GrantKey();
                    }
                    pickup.collected = true;
                    break;

                case RoomPickupType::COIN:
                    if (pickup.cost > 0 && !SpendCoins(player, pickup.cost)) {
                        continue;
                    }
                    GrantCoins(pickup.amount > 0 ? pickup.amount : 1);
                    pickup.collected = true;
                    break;

                case RoomPickupType::CHEST:
                    if (OpenChest(pickup)) {
                        pickup.collected = true;
                        player.actionFlashTimer = std::max(player.actionFlashTimer, 0.08f);
                        AddScreenShake(0.08f, 1.0f);
                    } else {
                        continue;
                    }
                    break;

                case RoomPickupType::EXIT:
                    if (room.type == RoomType::BOSS && room.cleared) {
                        pickup.collected = true;
                        uint32_t nextSeed = (uint32_t)GetTickCount() + 97u * (uint32_t)(dungeon.CurrentFloor() + 1);
                        if (dungeon.AdvanceFloor(nextSeed)) {
                            dungeon.PlacePlayerAtCurrentRoomCenter(player);
                            ClearRunEntities();
                            BeginFloorTransition();
                        } else {
                            state = GameState::TITLE;
                        }
                        return true;
                    }
                    break;

                case RoomPickupType::TROPHY:
                    pickup.collected = true;
                    state = GameState::TITLE;
                    enemies.clear();
                    enemyShots.clear();
                    playerShots.clear();
                    boss = Boss{};
                    boss.alive = false;
                    return true;
            }
        }

        return false;
    };

    auto SpawnRockReward = [&](Room& room, const RoomRock& rock) {
        RoomPickup pickup;
        pickup.pos = { rock.pos.x + rock.w * 0.5f - 4.0f, rock.pos.y + rock.h * 0.5f - 4.0f };
        if (rock.type == RoomRockType::BOMBABLE_HEART) {
            pickup.type = RoomPickupType::HEART;
        } else {
            pickup.type = RoomPickupType::COIN;
            pickup.amount = std::max(1, rock.rewardAmount);
        }
        room.pickups.push_back(pickup);
    };

    auto BreakRoomRock = [&](Room& room, RoomRock& rock, bool fromExplosion) {
        if (rock.broken || !rock.IsBombable()) return false;
        rock.broken = true;
        SpawnRockReward(room, rock);
        AddScreenShake(fromExplosion ? 0.10f : 0.06f, fromExplosion ? 0.9f : 0.6f);
        return true;
    };

    auto ResolveAgainstRoomRocks = [&](Room& room, Vec2& pos, float entityW, float entityH) {
        for (int pass = 0; pass < 2; ++pass) {
            bool moved = false;
            for (const auto& rock : room.rocks) {
                if (rock.broken) continue;
                Rect rockRect = rock.GetRect();
                Vec2 before = pos;
                ResolveEntitySolidCollision(pos, entityW, entityH, rockRect);
                if (before.x != pos.x || before.y != pos.y) moved = true;
            }
            if (!moved) break;
        }
    };

    auto FindSafeTeleportPos = [&](const Room& room) -> Vec2 {
        static const Vec2 candidates[] = {
            { 46.0f, 40.0f }, { 274.0f, 40.0f },
            { 46.0f, 120.0f }, { 274.0f, 120.0f },
            { 120.0f, 52.0f }, { 200.0f, 52.0f },
            { 120.0f, 116.0f }, { 200.0f, 116.0f }
        };

        std::vector<int> options;
        for (int i = 0; i < (int)(sizeof(candidates) / sizeof(candidates[0])); ++i) {
            Rect test = { candidates[i].x, candidates[i].y, (float)player.size, (float)player.size };
            bool blocked = false;
            for (const auto& rock : room.rocks) {
                if (rock.broken) continue;
                if (Collision::CheckAABB(test, rock.GetRect())) {
                    blocked = true;
                    break;
                }
            }
            if (!blocked) {
                for (const auto& trap : room.traps) {
                    if (trap.triggered) continue;
                    if (Collision::CheckAABB(test, trap.GetRect())) {
                        blocked = true;
                        break;
                    }
                }
            }
            if (!blocked) options.push_back(i);
        }

        if (options.empty()) {
            return { room.x + room.width * 0.5f, room.y + room.height * 0.5f };
        }

        int choice = options[RNG::Range(0, (int)options.size() - 1)];
        return candidates[choice];
    };

    auto PickEligibleEnemyIndexForFloor = [&]() -> int {
        const int enemyCount = EnemyDatabase::Count();
        if (enemyCount <= 0) return -1;

        std::vector<int> eligible;
        for (int i = 0; i < enemyCount; ++i) {
            if (EnemyDatabase::Tier(i) <= dungeon.CurrentFloor()) eligible.push_back(i);
        }
        if (eligible.empty()) {
            for (int i = 0; i < enemyCount; ++i) eligible.push_back(i);
        }
        if (eligible.empty()) return -1;
        return eligible[RNG::Range(0, (int)eligible.size() - 1)];
    };

    auto SpawnTrapWave = [&](Room& room, const Vec2& center, int count) {
        int enemyIndex = PickEligibleEnemyIndexForFloor();
        if (enemyIndex < 0) return;

        room.cleared = false;
        room.gateOpen = false;

        static const Vec2 offsets[] = {
            { -14.0f, 0.0f }, { 14.0f, 0.0f }, { 0.0f, -14.0f }, { 0.0f, 14.0f }
        };

        for (int i = 0; i < count; ++i) {
            Vec2 spawnPos = {
                center.x + offsets[i % 4].x,
                center.y + offsets[i % 4].y
            };
            Enemy enemy = EnemyDatabase::Spawn(enemyIndex, spawnPos);
            ScaleEnemyForFloor(enemy);
            MakeSpecialEnemy(enemy);
            enemies.push_back(enemy);
            enemyIndex = PickEligibleEnemyIndexForFloor();
            if (enemyIndex < 0) break;
        }

        AddScreenShake(0.12f, 1.0f);
    };

    auto TriggerTrap = [&](Room& room, RoomTrap& trap) {
        if (trap.triggered) return;
        trap.triggered = true;

        Vec2 trapCenter = { trap.pos.x + trap.w * 0.5f, trap.pos.y + trap.h * 0.5f };

        switch (trap.type) {
            case RoomTrapType::POISON:
                player.poisonTimer = std::max(player.poisonTimer, 3.0f);
                player.poisonTickTimer = 0.5f;
                player.poisonDamage = std::max(player.poisonDamage, (dungeon.CurrentFloor() >= 4) ? 2 : 1);
                AddScreenShake(0.08f, 0.8f);
                break;
            case RoomTrapType::TELEPORT: {
                float safeChance = std::clamp(0.35f + 0.08f * (float)player.luck, 0.15f, 0.90f);
                Vec2 target = (RNG::Chance(safeChance)) ? FindSafeTeleportPos(room) : Vec2{
                    room.x + 18.0f + RNG::Range(0, 8) * 28.0f,
                    room.y + 18.0f + RNG::Range(0, 4) * 28.0f
                };
                player.pos = room.ClampPlayerToRoom(target, (float)player.size, (float)player.size);
                ResolveAgainstRoomRocks(room, player.pos, (float)player.size, (float)player.size);
                AddScreenShake(0.10f, 1.0f);
                break;
            }
            case RoomTrapType::SUMMON: {
                int spawnCount = 2 + std::min(2, dungeon.CurrentFloor() / 2);
                SpawnTrapWave(room, trapCenter, spawnCount);
                break;
            }
            case RoomTrapType::SPIKE:
            default:
                DamagePlayer((dungeon.CurrentFloor() >= 3) ? 2 : 1);
                break;
        }
    };

    const float itemPreviewRadius = 20.0f;

    auto FindNearbyPreview = [&](const Room& room, const char*& outName, const char*& outDesc) -> bool {
        Vec2 playerCenter = { player.pos.x + player.size * 0.5f, player.pos.y + player.size * 0.5f };
        float bestDistSq = itemPreviewRadius * itemPreviewRadius;
        const RoomPickup* nearest = nullptr;

        for (const auto& pickup : room.pickups) {
            if (pickup.collected) continue;
            if (pickup.type != RoomPickupType::ITEM &&
                pickup.type != RoomPickupType::HEART &&
                pickup.type != RoomPickupType::BOMB) continue;

            Vec2 pickupCenter = { pickup.pos.x + 4.0f, pickup.pos.y + 4.0f };
            float dx = pickupCenter.x - playerCenter.x;
            float dy = pickupCenter.y - playerCenter.y;
            float distSq = dx * dx + dy * dy;
            if (distSq <= bestDistSq) {
                bestDistSq = distSq;
                nearest = &pickup;
            }
        }

        if (!nearest) return false;

        static char descBuffer[96];
        descBuffer[0] = '\0';

        if (nearest->type == RoomPickupType::HEART) {
            outName = "Heart";
            outDesc = "Restores a full heart of health";
            return true;
        }
        if (nearest->type == RoomPickupType::BOMB) {
            outName = "Bomb";
            outDesc = "Place it, 3s fuse, damages nearby foes";
            return true;
        }
        if (nearest->type == RoomPickupType::KEY) {
            outName = "Key";
            if (nearest->cost > 0) {
                std::snprintf(descBuffer, sizeof(descBuffer), "Costs %d coins", nearest->cost);
            } else {
                std::snprintf(descBuffer, sizeof(descBuffer), "Opens locked chests");
            }
            outDesc = descBuffer;
            return true;
        }
        if (nearest->type == RoomPickupType::COIN) {
            outName = "Coin";
            std::snprintf(descBuffer, sizeof(descBuffer), "Worth %d coin%s", nearest->amount,
                          nearest->amount == 1 ? "" : "s");
            outDesc = descBuffer;
            return true;
        }
        if (nearest->type == RoomPickupType::CHEST) {
            outName = ChestTypeName(nearest->chestType);
            std::snprintf(descBuffer, sizeof(descBuffer), "%s chest%s",
                          ChestTypeName(nearest->chestType),
                          nearest->chestType == ChestType::WOODEN ? "" :
                          nearest->chestType == ChestType::IRON || nearest->chestType == ChestType::STONE ? " - costs a bomb" :
                          nearest->chestType == ChestType::GOLDEN || nearest->chestType == ChestType::ANGEL ? " - costs a key" :
                          " - risky");
            outDesc = descBuffer;
            return true;
        }

        const ItemTemplate* item = ItemDatabase::Get(nearest->itemId);
        if (!item) return false;
        outName = item->name;
        if (nearest->cost > 0) {
            std::snprintf(descBuffer, sizeof(descBuffer), "%s - costs %d coins", item->desc, nearest->cost);
            outDesc = descBuffer;
        } else {
            outDesc = item->desc;
        }
        return true;
    };

    while (Window::PollEvents()) {
        float dt = timer.Tick();
        Input::Update();

        Room* activeRoom = nullptr;
        bool enterGameplayAfterPresent = false;

        if (state == GameState::TITLE) {
            if (Input::IsPressed(VK_RETURN) || Input::IsPressed(VK_SPACE)) {
                StartRun();
            }
        } else if (state == GameState::FLOOR_TRANSITION) {
            floorTransitionTimer += dt;
            bool canContinue = floorTransitionTimer >= 0.75f;
            if ((canContinue && (Input::IsPressed(VK_RETURN) || Input::IsPressed(VK_SPACE))) ||
                floorTransitionTimer >= 1.75f) {
                enterGameplayAfterPresent = true;
            }
        } else if (state == GameState::PLAYING) {
            activeRoom = &dungeon.CurrentRoom();
            Room* room = activeRoom;

            worldTime += dt;
            PlayerLogic::UpdateTimers(player, dt);
            ProjectileSystem::Advance(playerShots, dt, &enemies, (room->type == RoomType::BOSS && boss.alive) ? &boss : nullptr);
            ProjectileSystem::Advance(enemyShots, dt, nullptr, nullptr, &player.pos);
            PlayerLogic::HandleMovement(player, dt);

            if (screenShakeTimer > 0.0f) {
                screenShakeTimer -= dt;
                if (screenShakeTimer < 0.0f) screenShakeTimer = 0.0f;
            }

            if (dungeon.TryTransition(player)) {
                LoadRoomEncounter();
            }

            activeRoom = &dungeon.CurrentRoom();
            room = activeRoom;
            player.pos = room->ClampPlayerToRoom(player.pos, (float)player.size, (float)player.size);
            ResolveAgainstRoomRocks(*room, player.pos, (float)player.size, (float)player.size);
            PlayerLogic::HandleShooting(player, dt, playerShots);

            if ((Input::IsPressed('B') || Input::IsPressed('E')) && player.bombCount > 0) {
                player.bombCount--;
            
                Bomb bomb;
                bomb.pos = {
                    player.pos.x + player.size * 0.5f - 3.0f,
                    player.pos.y + player.size * 0.5f - 3.0f
                };
                bomb.fuseTimer = bombFuseDuration;

                bombs.push_back(bomb);
            }

            auto ApplyExplosiveShotsToRocks = [&](std::vector<Projectile>& shots) {
                for (auto& shot : shots) {
                    if (!shot.alive || !shot.explosive) continue;
                    Rect shotRect = shot.GetRect(projectileSize);
                    for (auto& rock : room->rocks) {
                        if (rock.broken) continue;
                        if (!Collision::CheckAABB(shotRect, rock.GetRect())) continue;
                        if (rock.IsBombable()) {
                            BreakRoomRock(*room, rock, true);
                        }
                        shot.alive = false;
                        break;
                    }
                }

                shots.erase(
                    std::remove_if(
                        shots.begin(),
                        shots.end(),
                        [](const Projectile& p) { return !p.alive; }),
                    shots.end()
                );
            };

            ApplyExplosiveShotsToRocks(playerShots);
            ApplyExplosiveShotsToRocks(enemyShots);

            for (auto& trap : room->traps) {
                if (trap.triggered) continue;
                if (!Collision::CheckAABB(player.GetRect(), trap.GetRect())) continue;
                trap.triggered = true;

                if (trap.type == RoomTrapType::POISON) {
                    DamagePlayer(1);
                    AddScreenShake(0.08f, 0.8f);
                } else {
                    player.pos = FindSafeTeleportPos(*room);
                    player.pos = room->ClampPlayerToRoom(player.pos, (float)player.size, (float)player.size);
                    ResolveAgainstRoomRocks(*room, player.pos, (float)player.size, (float)player.size);
                    AddScreenShake(0.10f, 1.0f);
                }
                break;
            }
            
            for (auto& bomb : bombs) {
                if (bomb.exploded) {
                    bomb.flashTimer -= dt;
                    continue;
                }
            
                bomb.fuseTimer -= dt;
            
                if (bomb.fuseTimer <= 0.0f) {
                    bomb.exploded = true;
                    bomb.flashTimer = 0.18f;
            
                    AddScreenShake(0.20f, 2.4f);
            
                    Vec2 bombCenter = {
                        bomb.pos.x + 3.0f,
                        bomb.pos.y + 3.0f
                    };
            
                    for (auto& enemy : enemies) {
                        if (!enemy.alive) continue;
            
                        Vec2 enemyCenter = {
                            enemy.pos.x + enemy.w * 0.5f,
                            enemy.pos.y + enemy.h * 0.5f
                        };
            
                        float dx = enemyCenter.x - bombCenter.x;
                        float dy = enemyCenter.y - bombCenter.y;
            
                        if (dx * dx + dy * dy <=
                            bombExplosionRadius * bombExplosionRadius) {
            
                            enemy.hp -= bombDamage;
            
                            if (enemy.hp <= 0)
                                enemy.alive = false;
                        }
                    }

                    for (auto& rock : room->rocks) {
                        if (rock.broken || !rock.IsBombable()) continue;
                        Vec2 rockCenter = {
                            rock.pos.x + rock.w * 0.5f,
                            rock.pos.y + rock.h * 0.5f
                        };
                        float dx = rockCenter.x - bombCenter.x;
                        float dy = rockCenter.y - bombCenter.y;
                        if (dx * dx + dy * dy <= bombExplosionRadius * bombExplosionRadius) {
                            BreakRoomRock(*room, rock, true);
                        }
                    }
            
                    if (room->type == RoomType::BOSS && boss.alive) {
                        Vec2 bossCenter = {
                            boss.pos.x + boss.w * 0.5f,
                            boss.pos.y + boss.h * 0.5f
                        };
            
                        float dx = bossCenter.x - bombCenter.x;
                        float dy = bossCenter.y - bombCenter.y;
            
                        if (dx * dx + dy * dy <=
                            bombExplosionRadius * bombExplosionRadius) {
            
                            boss.hp -= bombDamage;
            
                            if (boss.hp <= 0)
                                boss.alive = false;
                        }
                    }
                }
            }
            
            bombs.erase(
                std::remove_if(
                    bombs.begin(),
                    bombs.end(),
                    [](const Bomb& b) {
                        return b.exploded && b.flashTimer <= 0.0f;
                    }),
                bombs.end()
            );
            
            bool roomTransitioned = TryCollectCurrentRoomPickups();

            if (!roomTransitioned) {
                if ((room->type == RoomType::NORMAL || room->IsEnemyCurseRoom()) && !room->cleared) {
                    bool anyAlive = false;
                    std::vector<Enemy> spawnedEnemies;
                  for (auto& enemy : enemies) {
                        EnemyAI::Update(enemy, player.pos, dt, enemies, spawnedEnemies, enemyShots);
                        Vec2 preClampPos = enemy.pos;
                        enemy.pos = room->ClampToRoom(enemy.pos, enemy.w, enemy.h);
                        ResolveAgainstRoomRocks(*room, enemy.pos, enemy.w, enemy.h);
                        if (enemy.bouncesOffWalls && enemy.isCharging) {
                            if (enemy.pos.x != preClampPos.x) enemy.chargeDir.x = -enemy.chargeDir.x;
                            if (enemy.pos.y != preClampPos.y) enemy.chargeDir.y = -enemy.chargeDir.y;
                        }

                        ProjectileSystem::UpdateAndCollideVsEnemy(playerShots, enemies, enemy, projectileSize,
                                                                  spawnedEnemies, room->pickups, enemyShots, dt, &player);

                        if (enemy.alive && Collision::CheckAABB(player.GetRect(), enemy.GetRect())) {
                            ResolvePlayerSolidCollision(player, enemy.GetRect());
                            if (enemy.attackDelayRemaining <= 0.0f) {
                                DamagePlayer(ContactDamageFor(enemy));
                            }
                            if (player.hasSpikedArmor && player.spikedArmorTickTimer <= 0.0f) {
                                float thornCooldown = player.hasThornMantle ? 0.25f : 0.4f;
                                int thornDamage = player.hasThornMantle ? 2 : 1;
                                player.spikedArmorTickTimer = thornCooldown;
                                enemy.hp -= thornDamage;
                                if (enemy.hp <= 0) enemy.alive = false;
                            }
                        }
                        if (enemy.alive) anyAlive = true;
                    }

                                        if (!spawnedEnemies.empty()) {
                        for (auto& spawned : spawnedEnemies) {
                            ScaleEnemyForFloor(spawned);
                        }
                        enemies.insert(enemies.end(), spawnedEnemies.begin(), spawnedEnemies.end());
                        anyAlive = true;
                    }

                    ProjectileSystem::UpdateOrbiters(player, dt, &enemies, nullptr);

                    if (ProjectileSystem::UpdateAndCollideVsPlayer(enemyShots, player, projectileSize,
                                                                   invincibleDuration, dungeon.CurrentFloor(), dt)) {
                        AddScreenShake(0.14f, 2.0f);
                    }

                    if (!anyAlive) {
                        dungeon.MarkCurrentRoomCleared(true);
                        enemies.clear();
                        enemyShots.clear();
                        playerShots.clear();
                    }
                } else if (room->type == RoomType::BOSS) {
                    std::vector<Enemy> spawnedBossAdds;
                    BossAI::Update(boss, player.pos, dt, enemyShots, enemies, spawnedBossAdds);
                    if (!spawnedBossAdds.empty()) {
                        for (auto& add : spawnedBossAdds) {
                            ScaleEnemyForFloor(add);
                        }
                        enemies.insert(enemies.end(), spawnedBossAdds.begin(), spawnedBossAdds.end());
                    }

                    std::vector<Enemy> scratchSpawned; // boss-room adds never summon further adds
                    for (auto& add : enemies) {
                        EnemyAI::Update(add, player.pos, dt, enemies, scratchSpawned, enemyShots);
                        Vec2 preClampPos = add.pos;
                        add.pos = room->ClampToRoom(add.pos, add.w, add.h);
                        ResolveAgainstRoomRocks(*room, add.pos, add.w, add.h);
                        if (add.bouncesOffWalls && add.isCharging) {
                            if (add.pos.x != preClampPos.x) add.chargeDir.x = -add.chargeDir.x;
                            if (add.pos.y != preClampPos.y) add.chargeDir.y = -add.chargeDir.y;
                        }

                        ProjectileSystem::UpdateAndCollideVsEnemy(playerShots, enemies, add, projectileSize,
                                                                  scratchSpawned, room->pickups, enemyShots, dt, &player);

                        if (add.alive && Collision::CheckAABB(player.GetRect(), add.GetRect())) {
                            ResolvePlayerSolidCollision(player, add.GetRect());
                            if (add.attackDelayRemaining <= 0.0f) {
                                DamagePlayer(ContactDamageFor(add));
                            }
                            if (player.hasSpikedArmor && player.spikedArmorTickTimer <= 0.0f) {
                                float thornCooldown = player.hasThornMantle ? 0.25f : 0.4f;
                                int thornDamage = player.hasThornMantle ? 2 : 1;
                                player.spikedArmorTickTimer = thornCooldown;
                                add.hp -= thornDamage;
                                if (add.hp <= 0) add.alive = false;
                            }
                        }
                    }
                    enemies.erase(
                        std::remove_if(
                            enemies.begin(),
                            enemies.end(),
                            [](const Enemy& e) { return !e.alive; }),
                        enemies.end()
                    );

                    ProjectileSystem::UpdateOrbiters(player, dt, &enemies, &boss);

                    ProjectileSystem::UpdateAndCollideVsBoss(playerShots, boss, projectileSize, dt, &player);
                    if (ProjectileSystem::UpdateAndCollideVsPlayer(enemyShots, player, projectileSize,
                                                                   invincibleDuration, dungeon.CurrentFloor(), dt)) {
                        AddScreenShake(0.14f, 2.0f);
                    }

                    if (boss.alive && Collision::CheckAABB(player.GetRect(), boss.GetRect())) {
                        ResolvePlayerSolidCollision(player, boss.GetRect());
                        if (boss.attackDelayRemaining <= 0.0f) {
                            int dmg = boss.isCharging ? boss.chargeContactDamage : boss.contactDamage;
                            DamagePlayer(dmg);
                        }
                        if (player.hasSpikedArmor && player.spikedArmorTickTimer <= 0.0f) {
                            float thornCooldown = player.hasThornMantle ? 0.25f : 0.4f;
                            int thornDamage = player.hasThornMantle ? 2 : 1;
                            player.spikedArmorTickTimer = thornCooldown;
                            boss.hp -= thornDamage;
                            if (boss.hp <= 0) { boss.hp = 0; boss.alive = false; }
                        }
                    }

                    if (boss.alive) {
                        boss.pos = room->ClampToRoom(boss.pos, boss.w, boss.h);
                        ResolveAgainstRoomRocks(*room, boss.pos, boss.w, boss.h);
                    }

                    for (auto& hazard : boss.hazards) {
                        float dx = (player.pos.x + player.size * 0.5f) - hazard.pos.x;
                        float dy = (player.pos.y + player.size * 0.5f) - hazard.pos.y;
                        if (dx * dx + dy * dy <= hazard.radius * hazard.radius) {
                            hazard.tickTimer -= dt;
                            if (hazard.tickTimer <= 0.0f) {
                                hazard.tickTimer = 0.5f;
                                DamagePlayer(hazard.tickDamage);
                            }
                        }
                    }

                    

                    if (!boss.alive) {
                        enemies.clear();
                        enemyShots.clear();
                        playerShots.clear();
                        AddScreenShake(0.22f, 2.6f);
                        if (!room->lootGranted) {
                            SpawnBossRewards(*room);
                        }
                        dungeon.MarkCurrentRoomCleared(true);
                    }
                }

                if (player.hp <= 0) {
                    state = GameState::GAME_OVER;
                }
            }
        } else if (state == GameState::GAME_OVER) {
            if (Input::IsPressed('R')) {
                StartRun();
            }
        }

        const Room* roomPtr = activeRoom;
        Vec2 shakeOffset = { 0.0f, 0.0f };
        if (screenShakeTimer > 0.0f) {
            float shakeScale = screenShakeTimer / 0.14f;
            if (shakeScale < 0.0f) shakeScale = 0.0f;
            if (shakeScale > 1.0f) shakeScale = 1.0f;
            shakeOffset.x = std::sin(screenShakeTimer * 97.0f) * screenShakeStrength * shakeScale;
            shakeOffset.y = std::cos(screenShakeTimer * 131.0f) * screenShakeStrength * shakeScale;
        }

        Renderer::Clear(0xFF1A1A1A);

        if (state == GameState::TITLE) {
            HUD::DrawTitleScreen();
            Renderer::Present();
            if (enterGameplayAfterPresent) EnterGameplay();
            continue;
        }

        if (state == GameState::FLOOR_TRANSITION) {
            HUD::DrawFloorTransition(floorTransitionFloor, dungeon.MaxFloors(), floorTransitionTreasureLine,
                                     floorTransitionTimer >= 0.75f);
            Renderer::Present();
            if (enterGameplayAfterPresent) EnterGameplay();
            continue;
        }

        if (roomPtr) {
            const Room& wr = *roomPtr;
            const float wt = Room::WALL_THICKNESS;
            const float dhw = Room::DOOR_HALF_WIDTH;
            const uint32_t wallColor = 0xFF3A2E22;
            const uint32_t doorOpenColor = 0xFFB8863B;
            const uint32_t doorLockedColor = 0xFF5A2A2A;

            int rx = (int)(wr.x + shakeOffset.x);
            int ry = (int)(wr.y + shakeOffset.y);
            int rw = (int)wr.width;
            int rh = (int)wr.height;
            int midX = rx + rw / 2;
            int midY = ry + rh / 2;
            int gapL = midX - (int)dhw, gapR = midX + (int)dhw;
            int gapT = midY - (int)dhw, gapB = midY + (int)dhw;
            uint32_t doorColor = wr.gateOpen ? doorOpenColor : doorLockedColor;

            // North wall
            if (wr.north >= 0) {
                Renderer::DrawRect(rx, ry, gapL - rx, (int)wt, wallColor);
                Renderer::DrawRect(gapR, ry, rx + rw - gapR, (int)wt, wallColor);
                Renderer::DrawRect(gapL, ry, gapR - gapL, (int)wt, doorColor);
            } else {
                Renderer::DrawRect(rx, ry, rw, (int)wt, wallColor);
            }
            // South wall
            int sy = ry + rh - (int)wt;
            if (wr.south >= 0) {
                Renderer::DrawRect(rx, sy, gapL - rx, (int)wt, wallColor);
                Renderer::DrawRect(gapR, sy, rx + rw - gapR, (int)wt, wallColor);
                Renderer::DrawRect(gapL, sy, gapR - gapL, (int)wt, doorColor);
            } else {
                Renderer::DrawRect(rx, sy, rw, (int)wt, wallColor);
            }
            // West wall
            if (wr.west >= 0) {
                Renderer::DrawRect(rx, ry, (int)wt, gapT - ry, wallColor);
                Renderer::DrawRect(rx, gapB, (int)wt, ry + rh - gapB, wallColor);
                Renderer::DrawRect(rx, gapT, (int)wt, gapB - gapT, doorColor);
            } else {
                Renderer::DrawRect(rx, ry, (int)wt, rh, wallColor);
            }
            // East wall
            int ex = rx + rw - (int)wt;
            if (wr.east >= 0) {
                Renderer::DrawRect(ex, ry, (int)wt, gapT - ry, wallColor);
                Renderer::DrawRect(ex, gapB, (int)wt, ry + rh - gapB, wallColor);
                Renderer::DrawRect(ex, gapT, (int)wt, gapB - gapT, doorColor);
            } else {
                Renderer::DrawRect(ex, ry, (int)wt, rh, wallColor);
            }

            for (const auto& rock : wr.rocks) {
                if (rock.broken) continue;
                uint32_t rockColor = 0xFF777777;
                if (rock.type == RoomRockType::BOMBABLE_COIN) rockColor = 0xFF8B6B3E;
                if (rock.type == RoomRockType::BOMBABLE_HEART) rockColor = 0xFF9E4B5F;
                Renderer::DrawRect((int)(rock.pos.x + shakeOffset.x), (int)(rock.pos.y + shakeOffset.y),
                                   (int)rock.w, (int)rock.h, rockColor);
                if (rock.type != RoomRockType::INDESTRUCTIBLE) {
                    Renderer::DrawRect((int)(rock.pos.x + shakeOffset.x) + 3,
                                       (int)(rock.pos.y + shakeOffset.y) + 3,
                                       (int)rock.w - 6, (int)rock.h - 6, 0xFF2A1D14);
                }
            }

            for (const auto& trap : wr.traps) {
                if (trap.triggered) continue;
                uint32_t trapColor = (trap.type == RoomTrapType::POISON) ? 0xFF5FD15F : 0xFF7A63FF;
                Renderer::DrawRect((int)(trap.pos.x + shakeOffset.x), (int)(trap.pos.y + shakeOffset.y),
                                   (int)trap.w, (int)trap.h, trapColor);
                Renderer::DrawRect((int)(trap.pos.x + shakeOffset.x) + 3,
                                   (int)(trap.pos.y + shakeOffset.y) + 3,
                                   (int)trap.w - 6, (int)trap.h - 6,
                                   (trap.type == RoomTrapType::POISON) ? 0xFF174B17 : 0xFF26155A);
            }
        }

        uint32_t playerColor = player.IsInvincible() ? 0xFFFF8888 : 0xFF00FF88;
        if (player.actionFlashTimer > 0.0f) {
            Renderer::DrawRect((int)(player.pos.x + shakeOffset.x) - 1, (int)(player.pos.y + shakeOffset.y) - 1,
                               player.size + 2, player.size + 2, 0xFFFFFFAA);
        }
        Renderer::DrawRect((int)(player.pos.x + shakeOffset.x), (int)(player.pos.y + shakeOffset.y),
                           player.size, player.size, playerColor);

        if (player.hasSecondSun) {
            Vec2 playerCenter = { player.pos.x + player.size / 2.0f, player.pos.y + player.size / 2.0f };
            Vec2 sunPos = {
                playerCenter.x + std::cos(player.secondSunAngle) * 28.0f,
                playerCenter.y + std::sin(player.secondSunAngle) * 28.0f
            };
            int sunX = (int)std::lround(sunPos.x + shakeOffset.x);
            int sunY = (int)std::lround(sunPos.y + shakeOffset.y);
            Renderer::DrawRect(sunX - 3, sunY - 3, 6, 6, 0xFFFFFF99);
            Renderer::DrawRect(sunX - 1, sunY - 1, 2, 2, 0xFFFFFFFF);
        }

        if (roomPtr) {
            for (const auto& pickup : roomPtr->pickups) {
                if (pickup.collected) continue;

                uint32_t color = 0xFFFFC84D;
                int size = 8;
                switch (pickup.type) {
                    case RoomPickupType::ITEM:  color = pickup.isMimic ? 0xFFB04A4A : 0xFFFFC84D; size = 8; break;
                    case RoomPickupType::HEART: color = 0xFFFF4D77; size = 8; break;
                    case RoomPickupType::BOMB:  color = 0xFF333333; size = 9; break;
                    case RoomPickupType::KEY:   color = 0xFF66FFFF; size = 8; break;
                    case RoomPickupType::COIN:  color = (pickup.amount >= 10) ? 0xFFFFD966 : (pickup.amount >= 5 ? 0xFFC0C0C0 : 0xFFFFCC66); size = 7; break;
                    case RoomPickupType::CHEST: color = ChestColor(pickup.chestType); size = 11; break;
                    case RoomPickupType::EXIT:  color = 0xFF776655; size = 10; break;
                    case RoomPickupType::TROPHY: color = 0xFFFFFF99; size = 12; break;
                }

                Renderer::DrawRect((int)(pickup.pos.x + shakeOffset.x), (int)(pickup.pos.y + shakeOffset.y),
                                   size, size, color);
                if (pickup.type == RoomPickupType::ITEM && pickup.isMimic) {
                    float pulse = 0.5f + 0.5f * std::sin(worldTime * 12.0f + pickup.pos.x * 0.27f + pickup.pos.y * 0.11f);
                    int x = (int)(pickup.pos.x + shakeOffset.x);
                    int y = (int)(pickup.pos.y + shakeOffset.y);
                    uint32_t outline = (pulse > 0.5f) ? 0xFF3B0A0A : 0xFF6A1515;
                    Renderer::DrawRect(x - 1, y - 1, size + 2, size + 2, outline);
                    Renderer::DrawRect(x + 1, y + 1, size - 2, size - 2, 0xFF1D0505);
                    Renderer::DrawRect(x + 2, y + 2, 2, 2, 0xFF000000);
                    Renderer::DrawRect(x + size - 4, y + 2, 2, 2, 0xFF000000);
                    Renderer::DrawRect(x + 2, y + size - 3, size - 4, 1, 0xFF000000);
                }
                if (pickup.type == RoomPickupType::EXIT) {
                    Renderer::DrawRect((int)(pickup.pos.x + shakeOffset.x) + 2, (int)(pickup.pos.y + shakeOffset.y) + 2,
                                       size - 4, size - 4, 0xFF332211);
                } else if (pickup.type == RoomPickupType::CHEST) {
                    Renderer::DrawRect((int)(pickup.pos.x + shakeOffset.x) + 2, (int)(pickup.pos.y + shakeOffset.y) + 2,
                                       size - 4, size - 4, 0xFF1A1020);
                }
            }

            const char* previewName = nullptr;
        const char* previewDesc = nullptr;
        if (roomPtr && FindNearbyPreview(*roomPtr, previewName, previewDesc)) {
            HUD::DrawItemPreview(previewName, previewDesc);
        }
        }

                for (const auto& bomb : bombs) {
            if (bomb.exploded) {
                if (bomb.flashTimer > 0.0f) {
                    int flashSize =
                        (int)(bombExplosionRadius * 2.0f *
                              (bomb.flashTimer / 0.18f));

                    Renderer::DrawRect(
                        (int)(bomb.pos.x + shakeOffset.x) -
                            flashSize / 2 + 3,
                        (int)(bomb.pos.y + shakeOffset.y) -
                            flashSize / 2 + 3,
                        flashSize,
                        flashSize,
                        0xFFFFCC66
                    );
                }

                continue;
            }

            float fusePct =
                1.0f - (bomb.fuseTimer / bombFuseDuration);

            uint32_t glow =
                fusePct > 0.66f
                    ? 0xFFFF3333
                    : (fusePct > 0.33f
                        ? 0xFFFFAA33
                        : 0xFF666666);

            Renderer::DrawRect(
                (int)(bomb.pos.x + shakeOffset.x),
                (int)(bomb.pos.y + shakeOffset.y),
                6,
                6,
                glow
            );
        }

       if (roomPtr &&
    (roomPtr->type == RoomType::NORMAL ||
     roomPtr->type == RoomType::BOSS ||
     roomPtr->IsEnemyCurseRoom())) {
            for (auto& enemy : enemies) {
                if (!enemy.alive) continue;

                uint32_t color;
                switch (enemy.aiType) {
                    case AIType::SHOOTER:  color = 0xFFFF9933; break;
                    case AIType::CHARGER:  color = 0xFFFFCC33; break;
                    case AIType::SUMMONER: color = 0xFFCC66FF; break;
                    case AIType::SPAWNER:  color = 0xFF66CCCC; break;
                    case AIType::EXPLODER: color = 0xFF33DD66; break;
                    case AIType::MIMIC:    color = 0xFF8B5A2B; break;
                    case AIType::CHASER:
                    default:                color = 0xFFFF3333; break;
                }

                // Nudge the hue by attack pattern so, e.g., a triple-shot
                // Gunner and a spiral-shot Gunner read as different enemies.
                if (enemy.attackPattern == AttackPattern::TRIPLE) color ^= 0x00202000;
                if (enemy.attackPattern == AttackPattern::RADIAL) color ^= 0x00002020;
                if (enemy.attackPattern == AttackPattern::SPIRAL) color ^= 0x00200020;

                if (enemy.specialType == EnemySpecialType::REINFORCER) color = 0xFF66DDFF;
                if (enemy.specialType == EnemySpecialType::CREEPER) color = 0xFF55FF55;
                if (enemy.specialType == EnemySpecialType::DEATH_RING) color = 0xFFFFFF66;

                if (enemy.shielded) {
                    Renderer::DrawRect((int)(enemy.pos.x + shakeOffset.x) - 1, (int)(enemy.pos.y + shakeOffset.y) - 1,
                                       (int)enemy.w + 2, (int)enemy.h + 2, 0xFFAADDFF);
                }
                if (enemy.aiType == AIType::MIMIC) {
                    Renderer::DrawRect((int)(enemy.pos.x + shakeOffset.x), (int)(enemy.pos.y + shakeOffset.y),
                                       (int)enemy.w, (int)enemy.h, color);
                    Renderer::DrawRect((int)(enemy.pos.x + shakeOffset.x), (int)(enemy.pos.y + shakeOffset.y),
                                       (int)enemy.w, 4, 0xFFC78A4A);
                    Renderer::DrawRect((int)(enemy.pos.x + shakeOffset.x) + 3,
                                       (int)(enemy.pos.y + shakeOffset.y) + 5,
                                       (int)enemy.w - 6, 2, 0xFF2B1608);
                } else {
                    Renderer::DrawRect((int)(enemy.pos.x + shakeOffset.x), (int)(enemy.pos.y + shakeOffset.y),
                                       (int)enemy.w, (int)enemy.h, color);
                }
            }
        }

        if (roomPtr && roomPtr->type == RoomType::BOSS && boss.alive) {
            const BossTemplate* bossTemplate = BossDatabase::Get(boss.variant % std::max(1, BossDatabase::Count()));
            uint32_t baseColor = 0xFFAA33FF;
            if (boss.variant == 1) baseColor = 0xFFFF9933;
            if (boss.variant == 2) baseColor = 0xFF33FFCC;
            if (boss.variant >= 10) baseColor = 0xFFFF77AA;
            uint32_t bossColor = boss.isCharging ? 0xFFFF3399 : baseColor;
            Renderer::DrawRect((int)(boss.pos.x + shakeOffset.x), (int)(boss.pos.y + shakeOffset.y),
                               (int)boss.w, (int)boss.h, bossColor);
            HUD::DrawBossHealthBar(boss, bossTemplate ? bossTemplate->name : nullptr);
        }

        ProjectileSystem::Draw(playerShots, (int)projectileSize, 0xFFFFFF00, shakeOffset);
        ProjectileSystem::DrawOrbiters(player, shakeOffset);
        ProjectileSystem::Draw(enemyShots, (int)projectileSize, 0xFFFF66FF, shakeOffset);

        HUD::DrawHealthBar(player);
        if (roomPtr) {
            HUD::DrawRunStatus(dungeon, player, *roomPtr, (roomPtr->type == RoomType::BOSS && boss.alive) ? &boss : nullptr);
        }
        if (state == GameState::GAME_OVER) HUD::DrawGameOverBanner();
        if (roomPtr &&
            (roomPtr->type == RoomType::NORMAL ||
             roomPtr->IsEnemyCurseRoom()) &&
            roomPtr->cleared) {
            HUD::DrawRoomClearedBanner();
        }
        HUD::DrawFloorMap(dungeon, player);

        Renderer::Present();
    }

    Renderer::Shutdown();
    Window::Destroy();
    return 0;
}
