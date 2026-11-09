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

#include "ProgressionMgr.h"
#include "Common.h"
#include "Timer.h"
#include <cmath>
#include "Config.h"
#include "DatabaseEnv.h"
#include "DB2Stores.h"
#include "GameTime.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "QuestDef.h"
#include "BattlegroundPackets.h"
#include "World.h"
#include "WorldSession.h"
#include <algorithm>
#include <cstdio>

ProgressionMgr::ProgressionMgr() : _enabled(false), _currentPatch(PROGRESSION_PATCH_LATEST), _patchStartTime(0) { }

ProgressionMgr* ProgressionMgr::instance()
{
    static ProgressionMgr instance;
    return &instance;
}

std::string ProgressionMgr::PatchToString(uint32 patch)
{
    char buff[16];
    snprintf(buff, sizeof(buff), "%u.%u.%u", patch / 10000, (patch / 100) % 100, patch % 100);
    return buff;
}

uint32 ProgressionMgr::ParsePatch(std::string const& text)
{
    if (text.empty())
        return 0;

    if (text.find('.') == std::string::npos)
        return uint32(strtoul(text.c_str(), nullptr, 10));

    uint32 major = 0, minor = 0, build = 0;
    if (sscanf(text.c_str(), "%u.%u.%u", &major, &minor, &build) < 2)
        return 0;

    return major * 10000 + minor * 100 + build;
}

void ProgressionMgr::Initialize()
{
    _enabled = sConfigMgr->GetBoolDefault("Progression.Enable", false);
    if (!_enabled)
    {
        _currentPatch = PROGRESSION_PATCH_LATEST;
        TC_LOG_INFO("server.loading", ">> Progression is disabled, all content is open.");
        return;
    }

    LoadFromDB();

    if (_patches.empty())
    {
        TC_LOG_ERROR("server.loading", "Progression.Enable is set but `progression_patch` is empty. Progression is disabled.");
        _enabled = false;
        _currentPatch = PROGRESSION_PATCH_LATEST;
        return;
    }

    uint32 savedPatch = sWorld->getWorldState(WS_PROGRESSION_PATCH);
    uint32 startPatch = ParsePatch(sConfigMgr->GetStringDefault("Progression.StartPatch", "8.0.1"));

    if (savedPatch && _patches.count(savedPatch))
        _currentPatch = savedPatch;
    else if (_patches.count(startPatch))
        _currentPatch = startPatch;
    else
    {
        TC_LOG_ERROR("server.loading", "Progression.StartPatch %s is not in `progression_patch`, using the first patch.", PatchToString(startPatch).c_str());
        _currentPatch = _patches.begin()->first;
    }

    if (savedPatch != _currentPatch)
    {
        sWorld->setWorldState(WS_PROGRESSION_PATCH, _currentPatch);
        sWorld->setWorldState(WS_PROGRESSION_PATCH_START_TIME, uint32(GameTime::GetGameTime()));
    }

    _patchStartTime = sWorld->getWorldState(WS_PROGRESSION_PATCH_START_TIME);

    ApplyPatchSettings();

    ProgressionPatchInfo const* info = GetCurrentPatchInfo();
    TC_LOG_INFO("server.loading", ">> Progression is enabled. Content patch: %s (%s)", PatchToString(_currentPatch).c_str(), info ? info->Name.c_str() : "");
}

void ProgressionMgr::LoadFromDB()
{
    uint32 oldMSTime = getMSTime();

    _patches.clear();
    _rules.clear();
    _lootItemLevels.clear();
    _mythicPlusItemLevels.clear();

    //                                                 0      1     2                 3          4                5
    if (QueryResult result = WorldDatabase.Query("SELECT Patch, Name, MythicPlusSeason, PvpSeason, AzeriteLevelCap, ItemLevelCap FROM progression_patch"))
    {
        do
        {
            Field* fields = result->Fetch();
            ProgressionPatchInfo info;
            info.Patch = fields[0].GetUInt32();
            info.Name = fields[1].GetString();
            info.MythicPlusSeason = fields[2].GetUInt32();
            info.PvpSeason = fields[3].GetUInt32();
            info.AzeriteLevelCap = fields[4].GetUInt32();
            info.ItemLevelCap = fields[5].GetUInt32();
            _patches[info.Patch] = info;
        } while (result->NextRow());
    }

    //                                                 0     1      2         3
    if (QueryResult result = WorldDatabase.Query("SELECT Type, Entry, MinPatch, MaxPatch FROM progression_content"))
    {
        do
        {
            Field* fields = result->Fetch();
            uint8 type = fields[0].GetUInt8();
            uint32 entry = fields[1].GetUInt32();
            if (type >= MAX_PROGRESSION_CONTENT_TYPE)
            {
                TC_LOG_ERROR("sql.sql", "Table `progression_content` has unknown Type %u (Entry %u), skipped.", type, entry);
                continue;
            }

            Rule& rule = _rules[MakeKey(ProgressionContentType(type), entry)];
            rule.MinPatch = fields[2].GetUInt32();
            rule.MaxPatch = fields[3].GetUInt32();
        } while (result->NextRow());
    }

    //                                                 0      1      2             3
    if (QueryResult result = WorldDatabase.Query("SELECT Patch, MapID, DifficultyID, ItemLevel FROM progression_item_level"))
    {
        do
        {
            Field* fields = result->Fetch();
            uint64 key = (uint64(fields[1].GetUInt32()) << 32) | fields[2].GetUInt32();
            _lootItemLevels[key][fields[0].GetUInt32()] = fields[3].GetUInt32();
        } while (result->NextRow());
    }

    //                                                 0       1              2                  3
    if (QueryResult result = WorldDatabase.Query("SELECT Season, KeystoneLevel, EndOfRunItemLevel, WeeklyItemLevel FROM progression_mythic_plus"))
    {
        do
        {
            Field* fields = result->Fetch();
            ProgressionMythicPlusItemLevel& itemLevels = _mythicPlusItemLevels[fields[0].GetUInt32()][fields[1].GetUInt32()];
            itemLevels.EndOfRun = fields[2].GetUInt32();
            itemLevels.WeeklyChest = fields[3].GetUInt32();
        } while (result->NextRow());
    }

    TC_LOG_INFO("server.loading", ">> Loaded %u progression patches, %u content rules, %u item level rows and %u Mythic+ seasons in %u ms",
        uint32(_patches.size()), uint32(_rules.size()), uint32(_lootItemLevels.size()), uint32(_mythicPlusItemLevels.size()), GetMSTimeDiffToNow(oldMSTime));
}

ProgressionPatchInfo const* ProgressionMgr::GetPatchInfo(uint32 patch) const
{
    auto itr = _patches.find(patch);
    return itr != _patches.end() ? &itr->second : nullptr;
}

ProgressionPatchInfo const* ProgressionMgr::GetCurrentPatchInfo() const
{
    if (!_enabled)
        return nullptr;

    return GetPatchInfo(_currentPatch);
}

uint32 ProgressionMgr::GetNextPatch() const
{
    auto itr = _patches.upper_bound(_currentPatch);
    return itr != _patches.end() ? itr->first : 0;
}

uint32 ProgressionMgr::GetPreviousPatch() const
{
    auto itr = _patches.lower_bound(_currentPatch);
    if (itr == _patches.begin())
        return 0;

    return (--itr)->first;
}

bool ProgressionMgr::SetCurrentPatch(uint32 patch)
{
    if (!_enabled || !_patches.count(patch))
        return false;

    _currentPatch = patch;
    _patchStartTime = uint32(GameTime::GetGameTime());
    sWorld->setWorldState(WS_PROGRESSION_PATCH, _currentPatch);
    sWorld->setWorldState(WS_PROGRESSION_PATCH_START_TIME, _patchStartTime);

    ApplyPatchSettings();

    for (auto const& sessionPair : sWorld->GetAllSessions())
    {
        Player* player = sessionPair.second->GetPlayer();
        if (!player || !player->IsInWorld())
            continue;

        SendSeasonInfo(player);

        // players standing in content that just closed are sent home
        if (!player->IsGameMaster() && (!IsMapAvailable(player->GetMapId()) || !IsZoneAvailable(player->GetZoneId(), player->GetAreaId())))
            player->TeleportTo(player->m_homebindMapId, player->m_homebindX, player->m_homebindY, player->m_homebindZ, player->GetOrientation());
    }

    ProgressionPatchInfo const* info = GetCurrentPatchInfo();
    TC_LOG_INFO("server.worldserver", "Progression: content patch changed to %s (%s)", PatchToString(_currentPatch).c_str(), info ? info->Name.c_str() : "");
    return true;
}

void ProgressionMgr::ApplyPatchSettings()
{
    ProgressionPatchInfo const* info = GetCurrentPatchInfo();
    if (!info)
        return;

    if (info->PvpSeason)
        sWorld->setIntConfig(CONFIG_ARENA_SEASON_ID, info->PvpSeason);

    // The Heart of Azeroth catch-up counts weeks from the day it opened
    if (IsFeatureAvailable(PROGRESSION_FEATURE_AZERITE_KNOWLEDGE))
    {
        if (!sWorld->getWorldState(WS_PROGRESSION_AZERITE_KNOWLEDGE_START))
            sWorld->setWorldState(WS_PROGRESSION_AZERITE_KNOWLEDGE_START, uint32(GameTime::GetGameTime()));
    }
    else if (sWorld->getWorldState(WS_PROGRESSION_AZERITE_KNOWLEDGE_START))
        sWorld->setWorldState(WS_PROGRESSION_AZERITE_KNOWLEDGE_START, 0);
}

bool ProgressionMgr::GetRule(ProgressionContentType type, uint32 entry, uint32& minPatch, uint32& maxPatch) const
{
    auto itr = _rules.find(MakeKey(type, entry));
    if (itr == _rules.end())
        return false;

    minPatch = itr->second.MinPatch;
    maxPatch = itr->second.MaxPatch;
    return true;
}

bool ProgressionMgr::IsAvailable(ProgressionContentType type, uint32 entry) const
{
    if (!_enabled)
        return true;

    auto itr = _rules.find(MakeKey(type, entry));
    if (itr == _rules.end())
        return true;

    Rule const& rule = itr->second;
    if (_currentPatch < rule.MinPatch)
        return false;

    if (rule.MaxPatch && _currentPatch >= rule.MaxPatch)
        return false;

    return true;
}

bool ProgressionMgr::IsQuestAvailable(Quest const* quest) const
{
    if (!_enabled || !quest)
        return true;

    if (!IsAvailable(PROGRESSION_CONTENT_QUEST, quest->GetQuestId()))
        return false;

    if (quest->GetZoneOrSort() > 0 && !IsAvailable(PROGRESSION_CONTENT_QUEST_SORT, uint32(quest->GetZoneOrSort())))
        return false;

    return true;
}

bool ProgressionMgr::IsZoneAvailable(uint32 zoneId, uint32 areaId) const
{
    if (!_enabled)
        return true;

    return IsAvailable(PROGRESSION_CONTENT_ZONE, zoneId) && IsAvailable(PROGRESSION_CONTENT_ZONE, areaId);
}

bool ProgressionMgr::IsCreatureSpawnAvailable(uint64 spawnId) const
{
    if (!_enabled)
        return true;

    if (!IsAvailable(PROGRESSION_CONTENT_CREATURE_SPAWN, uint32(spawnId)))
        return false;

    if (CreatureData const* data = sObjectMgr->GetCreatureData(spawnId))
        return IsAvailable(PROGRESSION_CONTENT_CREATURE, data->id);

    return true;
}

bool ProgressionMgr::IsGameObjectSpawnAvailable(uint64 spawnId) const
{
    if (!_enabled)
        return true;

    if (!IsAvailable(PROGRESSION_CONTENT_GAMEOBJECT_SPAWN, uint32(spawnId)))
        return false;

    if (GameObjectData const* data = sObjectMgr->GetGOData(spawnId))
        return IsAvailable(PROGRESSION_CONTENT_GAMEOBJECT, data->id);

    return true;
}

uint32 ProgressionMgr::GetLootItemLevel(uint32 mapId, uint32 difficultyId) const
{
    if (!_enabled)
        return 0;

    auto findLevel = [this](uint32 map, uint32 difficulty) -> uint32
    {
        auto itr = _lootItemLevels.find((uint64(map) << 32) | difficulty);
        if (itr == _lootItemLevels.end())
            return 0;

        // newest row that is not newer than the live patch
        auto patchItr = itr->second.upper_bound(_currentPatch);
        if (patchItr == itr->second.begin())
            return 0;

        return (--patchItr)->second;
    };

    if (uint32 itemLevel = findLevel(mapId, difficultyId))
        return itemLevel;

    // MapID 0 rows are defaults for every Battle for Azeroth dungeon or raid
    MapEntry const* mapEntry = sMapStore.LookupEntry(mapId);
    if (mapEntry && mapEntry->IsDungeon() && mapEntry->Expansion() == EXPANSION_BATTLE_FOR_AZEROTH)
        return findLevel(0, difficultyId);

    return 0;
}

uint32 ProgressionMgr::GetItemLevelCap() const
{
    ProgressionPatchInfo const* info = GetCurrentPatchInfo();
    return info ? info->ItemLevelCap : 0;
}

void ProgressionMgr::AdjustLootItemLevel(uint32 itemId, uint32 mapId, uint32 difficultyId, std::vector<int32>& bonusListIDs) const
{
    if (!_enabled)
        return;

    ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
    if (!proto || proto->GetInventoryType() == INVTYPE_NON_EQUIP)
        return;

    if (proto->GetClass() != ITEM_CLASS_WEAPON && proto->GetClass() != ITEM_CLASS_ARMOR)
        return;

    uint32 targetItemLevel = GetLootItemLevel(mapId, difficultyId);
    uint32 cap = GetItemLevelCap();
    if (!targetItemLevel && !cap)
        return;

    BonusData bonusData;
    bonusData.Initialize(proto);
    for (int32 bonusListId : bonusListIDs)
        bonusData.AddBonusList(bonusListId);

    // items that scale with the player level are left alone
    if (bonusData.ScalingStatDistribution)
        return;

    int32 currentItemLevel = int32(Item::GetItemLevel(proto, bonusData, DEFAULT_MAX_LEVEL, 0, 0, 0, 0, false, 0));
    int32 wantedItemLevel = targetItemLevel ? int32(targetItemLevel) : currentItemLevel;
    if (cap && wantedItemLevel > int32(cap))
        wantedItemLevel = int32(cap);

    if (wantedItemLevel == currentItemLevel)
        return;

    if (uint32 deltaBonus = sDB2Manager.GetItemBonusListForItemLevelDelta(int16(wantedItemLevel - currentItemLevel)))
        bonusListIDs.push_back(int32(deltaBonus));
    else
        TC_LOG_DEBUG("misc", "Progression: no item level bonus for delta %d (item %u)", wantedItemLevel - currentItemLevel, itemId);
}

bool ProgressionMgr::GetMythicPlusItemLevel(uint32 keystoneLevel, ProgressionMythicPlusItemLevel& itemLevels) const
{
    if (!_enabled)
        return false;

    auto seasonItr = _mythicPlusItemLevels.find(GetMythicPlusSeason());
    if (seasonItr == _mythicPlusItemLevels.end())
        return false;

    // highest row at or below the keystone level
    auto itr = seasonItr->second.upper_bound(keystoneLevel);
    if (itr == seasonItr->second.begin())
        return false;

    itemLevels = (--itr)->second;
    return true;
}

uint32 ProgressionMgr::GetMythicPlusSeason() const
{
    ProgressionPatchInfo const* info = GetCurrentPatchInfo();
    return info ? info->MythicPlusSeason : 0;
}

uint32 ProgressionMgr::GetPvpSeason() const
{
    ProgressionPatchInfo const* info = GetCurrentPatchInfo();
    if (info && info->PvpSeason)
        return info->PvpSeason;

    return sWorld->getIntConfig(CONFIG_ARENA_SEASON_ID);
}

uint32 ProgressionMgr::GetAzeriteLevelCap(uint32 defaultCap) const
{
    ProgressionPatchInfo const* info = GetCurrentPatchInfo();
    if (!info || !info->AzeriteLevelCap)
        return defaultCap;

    return std::min(defaultCap, info->AzeriteLevelCap);
}

uint32 ProgressionMgr::GetAzeriteKnowledgeWeeks(uint32 maxKnowledge) const
{
    if (!IsFeatureAvailable(PROGRESSION_FEATURE_AZERITE_KNOWLEDGE))
        return 0;

    uint32 start = sWorld->getWorldState(WS_PROGRESSION_AZERITE_KNOWLEDGE_START);
    time_t now = GameTime::GetGameTime();
    if (!start || now <= time_t(start))
        return 0;

    return std::min(maxKnowledge, uint32((now - time_t(start)) / WEEK));
}

void ProgressionMgr::SendSeasonInfo(Player* player) const
{
    WorldPackets::Battleground::SeasonInfo seasonInfo;
    uint32 pvpSeason = GetPvpSeason();
    bool inProgress = sWorld->getBoolConfig(CONFIG_ARENA_SEASON_IN_PROGRESS);
    seasonInfo.PreviousSeason = pvpSeason - (inProgress ? 1 : 0);
    if (inProgress)
        seasonInfo.CurrentSeason = pvpSeason;

    seasonInfo.MythicPlusSeasonID = GetMythicPlusSeason();
    player->SendDirectMessage(seasonInfo.Write());
}
