/*
 * Copyright (C) 2020 BfaCore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef TRINITY_PROGRESSIONMGR_H
#define TRINITY_PROGRESSIONMGR_H

#include "Define.h"
#include "ItemDefines.h"
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

class Player;
class Quest;

// Content patches are written as major*10000 + minor*100 + build: 8.0.1 -> 80001
enum ProgressionPatch : uint32
{
    PROGRESSION_PATCH_8_0_1 = 80001,
    PROGRESSION_PATCH_8_1_0 = 80100,
    PROGRESSION_PATCH_8_1_5 = 80105,
    PROGRESSION_PATCH_8_2_0 = 80200,
    PROGRESSION_PATCH_8_2_5 = 80205,
    PROGRESSION_PATCH_8_3_0 = 80300,
    PROGRESSION_PATCH_8_3_7 = 80307,

    PROGRESSION_PATCH_LATEST = PROGRESSION_PATCH_8_3_7
};

// `progression_content`.`Type`
enum ProgressionContentType : uint8
{
    PROGRESSION_CONTENT_MAP              = 0,   // Entry = Map.db2 ID (teleport, instance entrance, LFG)
    PROGRESSION_CONTENT_QUEST            = 1,   // Entry = quest ID
    PROGRESSION_CONTENT_QUEST_SORT       = 2,   // Entry = quest_template.QuestSortID (zone ID) - locks every quest of the zone
    PROGRESSION_CONTENT_ZONE             = 3,   // Entry = AreaTable ID of a zone or area - players are sent home
    PROGRESSION_CONTENT_CREATURE         = 4,   // Entry = creature_template entry - spawns are not loaded
    PROGRESSION_CONTENT_GAMEOBJECT       = 5,   // Entry = gameobject_template entry - spawns are not loaded
    PROGRESSION_CONTENT_CREATURE_SPAWN   = 6,   // Entry = creature.guid
    PROGRESSION_CONTENT_GAMEOBJECT_SPAWN = 7,   // Entry = gameobject.guid
    PROGRESSION_CONTENT_ITEM             = 8,   // Entry = item ID - no loot drop, not sold by vendors
    PROGRESSION_CONTENT_RACE             = 9,   // Entry = race ID - character creation and race change
    PROGRESSION_CONTENT_FEATURE          = 10,  // Entry = ProgressionFeature
    PROGRESSION_CONTENT_LFG_DUNGEON      = 11,  // Entry = LFGDungeons.db2 ID

    MAX_PROGRESSION_CONTENT_TYPE
};

// Systems that are switched on by a patch. Their patch is set in `progression_content` (Type 10).
enum ProgressionFeature : uint32
{
    PROGRESSION_FEATURE_AZERITE_ESSENCES  = 1,  // Heart Forge, essences and essence slots (8.2)
    PROGRESSION_FEATURE_AZERITE_KNOWLEDGE = 2,  // weekly Heart of Azeroth catch-up (8.3)
    PROGRESSION_FEATURE_CORRUPTION        = 3,  // corruption effects (8.3)
};

struct ProgressionPatchInfo
{
    uint32 Patch = 0;
    std::string Name;
    uint32 MythicPlusSeason = 0;    // MythicPlusSeason.db2 ID sent to the client, 0 = none
    uint32 PvpSeason = 0;           // arena season, also written to Arena.ArenaSeason.ID
    uint32 AzeriteLevelCap = 0;     // Heart of Azeroth max level, 0 = no cap
    uint32 ItemLevelCap = 0;        // highest item level that loot can roll, 0 = no cap
};

struct ProgressionMythicPlusItemLevel
{
    uint32 EndOfRun = 0;
    uint32 WeeklyChest = 0;
};

class TC_GAME_API ProgressionMgr
{
    ProgressionMgr();
    ~ProgressionMgr() = default;

public:
    ProgressionMgr(ProgressionMgr const&) = delete;
    ProgressionMgr& operator=(ProgressionMgr const&) = delete;

    static ProgressionMgr* instance();

    // Loads tables and the saved patch. Call after World::LoadWorldStates.
    void Initialize();
    void LoadFromDB();

    bool IsEnabled() const { return _enabled; }
    uint32 GetCurrentPatch() const { return _currentPatch; }
    ProgressionPatchInfo const* GetCurrentPatchInfo() const;
    ProgressionPatchInfo const* GetPatchInfo(uint32 patch) const;
    std::map<uint32, ProgressionPatchInfo> const& GetPatches() const { return _patches; }
    uint32 GetNextPatch() const;
    uint32 GetPreviousPatch() const;

    // Changes the live patch, saves it and tells online players. Returns false for an unknown patch.
    bool SetCurrentPatch(uint32 patch);

    static std::string PatchToString(uint32 patch);
    static uint32 ParsePatch(std::string const& text);  // "8.1.5" or "80105", 0 when invalid

    // Rules
    bool IsAvailable(ProgressionContentType type, uint32 entry) const;
    bool GetRule(ProgressionContentType type, uint32 entry, uint32& minPatch, uint32& maxPatch) const;
    bool IsMapAvailable(uint32 mapId) const { return IsAvailable(PROGRESSION_CONTENT_MAP, mapId); }
    bool IsQuestAvailable(Quest const* quest) const;
    bool IsZoneAvailable(uint32 zoneId, uint32 areaId) const;
    bool IsCreatureSpawnAvailable(uint64 spawnId) const;
    bool IsGameObjectSpawnAvailable(uint64 spawnId) const;
    bool IsItemAvailable(uint32 itemId) const { return IsAvailable(PROGRESSION_CONTENT_ITEM, itemId); }
    bool IsRaceAvailable(uint8 race) const { return IsAvailable(PROGRESSION_CONTENT_RACE, race); }
    bool IsFeatureAvailable(ProgressionFeature feature) const { return IsAvailable(PROGRESSION_CONTENT_FEATURE, feature); }

    // Item levels
    // Item level that loot from this map and difficulty should have, 0 = leave the item as it is
    uint32 GetLootItemLevel(uint32 mapId, uint32 difficultyId) const;
    uint32 GetItemLevelCap() const;
    // Adds an item level bonus to bonusListIDs so the item ends at the progression item level and under the cap
    void AdjustLootItemLevel(uint32 itemId, uint32 mapId, uint32 difficultyId, std::vector<int32>& bonusListIDs) const;
    // Item level of a Mythic+ reward for the current season, false when the table has no row
    bool GetMythicPlusItemLevel(uint32 keystoneLevel, ProgressionMythicPlusItemLevel& itemLevels) const;

    // Seasons and caps
    uint32 GetMythicPlusSeason() const;
    uint32 GetPvpSeason() const;
    uint32 GetAzeriteLevelCap(uint32 defaultCap) const;
    // Weeks since the Heart of Azeroth catch-up started, or 0 when it is not open yet
    uint32 GetAzeriteKnowledgeWeeks(uint32 maxKnowledge) const;

    void SendSeasonInfo(Player* player) const;

private:
    static uint64 MakeKey(ProgressionContentType type, uint32 entry) { return (uint64(type) << 32) | entry; }
    void ApplyPatchSettings();

    struct Rule
    {
        uint32 MinPatch;
        uint32 MaxPatch;    // 0 = never closes
    };

    bool _enabled;
    uint32 _currentPatch;
    uint32 _patchStartTime;
    std::unordered_map<uint64, Rule> _rules;
    std::map<uint32, ProgressionPatchInfo> _patches;
    // (mapId << 32 | difficultyId) -> patch -> item level. mapId 0 = every Battle for Azeroth map
    std::unordered_map<uint64, std::map<uint32, uint32>> _lootItemLevels;
    // season -> keystone level -> item levels
    std::unordered_map<uint32, std::map<uint32, ProgressionMythicPlusItemLevel>> _mythicPlusItemLevels;
};

#define sProgressionMgr ProgressionMgr::instance()

#endif
