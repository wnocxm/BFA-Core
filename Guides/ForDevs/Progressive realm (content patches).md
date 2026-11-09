# Progressive realm: content patches on the 8.3.7 client

Players keep the 8.3.7 client. The server decides which content patch is live:
8.0.1 → 8.1.0 → 8.1.5 → 8.2.0 → 8.2.5 → 8.3.0 → 8.3.7.
Class tuning stays 8.3.7 because it lives in the client data.

## Setup

1. Run `sql/archive/world/2026_11_09_00_world_progression.sql` on the world database.
2. Run `sql/updates/auth/2026_11_09_00_auth_progression_rbac.sql` on the auth database.
3. In `worldserver.conf`:
   ```
   Progression.Enable = 1
   Progression.StartPatch = "8.0.1"
   ```
4. Start the server. The live patch is saved in the `worldstates` table (ID 20100). After the first start, `Progression.StartPatch` is not used again.

## Commands

| Command | What it does |
|---|---|
| `.progression info` | Live patch, M+ and PvP season, item level cap, which features are open |
| `.progression list` | All patches; `>>` marks the live one |
| `.progression next` | Opens the next patch (the one-click step) |
| `.progression set 8.1.5` | Jumps to any patch (also `80105`) |
| `.progression check <type> <entry>` | When a map, quest, item... opens |
| `.progression reload` | Reloads the progression tables |

Most changes apply right away: maps, quests, LFG, loot, vendors, races, seasons, caps and features. Players standing in content that just closed are sent home.
NPC and object spawns (types 4–7) change when their grid loads again. Restart after a patch change to be sure, the same way Blizzard opens a patch with maintenance.

## Tables (world database)

### `progression_patch`
One row per patch: M+ season, PvP season (26–29 = BfA S1–S4), Heart of Azeroth level cap and loot item level cap.

### `progression_content`
What opens in which patch. Content without a row is always open.
Open when `MinPatch <= live patch` and (`MaxPatch = 0` or `live patch < MaxPatch`).
`MaxPatch` closes content again, for example pre-patch events.

| Type | Entry | Effect |
|---|---|---|
| 0 | Map ID | Teleport, instance entrance, LFG |
| 1 | Quest ID | Quest cannot be seen or taken |
| 2 | QuestSortID (zone) | Every quest of the zone is locked |
| 3 | Zone/area ID | Players are sent home |
| 4 | creature entry | Spawns are not loaded |
| 5 | gameobject entry | Spawns are not loaded |
| 6 | creature guid | One spawn is not loaded |
| 7 | gameobject guid | One spawn is not loaded |
| 8 | item ID | Does not drop, not sold |
| 9 | race ID | Character creation and race change locked |
| 10 | feature | 1 essences, 2 Heart of Azeroth catch-up, 3 corruption |
| 11 | LFGDungeons ID | Locked in the dungeon finder |

The SQL already has the raids, dungeons, Nazjatar, Mechagon, Horrific Visions, allied races, essences and corruption.
Still to add by hand: 8.1+ war campaign quests, quest givers and portals in the capitals, island expedition maps, new vendors and their items.

### `progression_item_level`
Item level of loot per patch, map and difficulty. The newest row that is not newer than the live patch is used.
`MapID = 0` rows are defaults for every BfA dungeon/raid that has no row of its own.
Difficulties: 1 normal, 2 heroic, 23 mythic, 8 keystone, 17 LFR, 14 normal raid, 15 heroic raid, 16 mythic raid.
The item level cap of the live patch is applied to all loot on top of that.

### `progression_mythic_plus`
End-of-run and weekly item level per season and keystone level. Keystones above the last row use the last row.

## What happens in code

| System | File |
|---|---|
| Manager, tables, commands' backend | `src/server/game/Progression/ProgressionMgr.*` |
| Commands | `src/server/scripts/Commands/cs_progression.cpp` |
| Maps, zones, quests, vendor buy | `Entities/Player/Player.cpp` |
| LFG lock | `DungeonFinding/LFGMgr.cpp` |
| Spawns | `Grids/ObjectGridLoader.cpp` |
| Loot items and item level | `Loot/Loot.cpp` |
| Vendor list | `Handlers/ItemHandler.cpp` |
| Races, season packet | `Handlers/CharacterHandler.cpp` |
| Heart of Azeroth cap, catch-up, essences | `Entities/Item/AzeriteItem/AzeriteItem.cpp` |
| Corruption | `Entities/Unit/StatSystem.cpp` |
| Mythic+ reward item level | `ChallengeMode/ChallengeModeMgr.cpp` |
| Creature stat mods per M+ season | `DataStores/DB2Stores.cpp` |

With `Progression.Enable = 0` everything is open and nothing changes.

## Known limits

- Numbers marked `check` in the SQL are best known values. Check them before opening the realm.
- M+ affixes are not implemented in this core yet (all affix values are 0). `MythicPlusSeason` is sent to the client and used for creature stats.
- Heart of Azeroth XP costs come from the 8.3.7 `AzeriteLevelInfo` data. Changing them needs hotfixes.
- The Adventure Guide still shows later raids because it reads client data.
