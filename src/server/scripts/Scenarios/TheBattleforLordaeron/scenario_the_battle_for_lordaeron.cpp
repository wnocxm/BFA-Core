/*
 * Copyright (C) 2021 BfaCore Reforged
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

#include "the_battle_for_lordaeron.h"
#include "Conversation.h"
#include "CreatureAI.h"
#include "DB2Stores.h"
#include "DBCEnums.h"
#include "GameObject.h"
#include "GameTime.h"
#include "InstanceScenario.h"
#include "Log.h"
#include "Map.h"
#include "MotionMaster.h"
#include "MoveSpline.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "PhasingHandler.h"
#include "Player.h"
#include "Chat.h"
#include "RBAC.h"
#include "SceneMgr.h"
#include "ScriptedGossip.h"
#include "ScriptMgr.h"
#include "SpellScript.h"
#include "TaskScheduler.h"
#include "TemporarySummon.h"
#include "WorldSession.h"
#include <limits>
#include <map>
#include <unordered_set>

using namespace BattleForLordaeron;

namespace
{
    constexpr float REPORT_TO_SAURFANG_RANGE    = 15.0f;
    constexpr float ANASTASIA_GREET_RANGE       = 30.0f;
    constexpr float RESCUE_RANGE                = 10.0f;
    constexpr float RESCUE_SPY_CLEAR_RANGE      = 10.0f;
    constexpr float COURTYARD_ARRIVAL_RANGE     = 25.0f;
    constexpr float START_TELEPORT_RANGE        = 40.0f;
    constexpr uint32 FACTION_SCENARIO_ENEMY     = 2502;

    // creature guids of the Alliance scenario's sniffed spawns on map 1760 (build 27219)
    constexpr ObjectGuid::LowType FIRST_ALLIANCE_VIEW_SPAWN = 210440933;
    constexpr ObjectGuid::LowType LAST_ALLIANCE_VIEW_SPAWN  = 210442002;
    constexpr uint32 FIRST_SCENARIO_CREATURE_ENTRY = 130000; // lower entries are map 0 Undercity residents

    constexpr Milliseconds SPIES_VANISH_DELAY   = 11533ms; // "<whistles>" line of CONVERSATION_SPIES_CORNERED
    constexpr Milliseconds REINFORCEMENTS_DELAY = 10s;
    constexpr size_t MIN_SPIES_LEFT             = 8;
    constexpr size_t MIN_CAPTIVES_LEFT          = 4;
    constexpr size_t MIN_HEART_ENEMIES_LEFT     = 6;
    constexpr size_t MIN_HEART_CITIZENS_LEFT    = 8;
    constexpr Milliseconds SAURFANG_BRIEFING_LENGTH = 38809ms;
    constexpr Milliseconds UNDERCITY_EVACUATED_LENGTH = 35675ms;
    constexpr Milliseconds LOKTAR_OGAR_LINE_START = 32694ms; // Saurfang's "Lok'tar ogar!" in CONVERSATION_UNDERCITY_EVACUATED
    constexpr Milliseconds GATE_WAIT_BEFORE_ORDER = 3s;  // Saurfang stands at the gate a moment
    constexpr Milliseconds GATE_OPEN_IN_ORDER    = 2s;  // "...Open the gate!" ends this long into the line
    constexpr float GATE_FRONT_DISTANCE         = 5.0f; // where he stops, in front of the portcullis
    constexpr float GATE_INSIDE_DISTANCE        = 6.0f;

    // ---- Steps 6-13 ----
    constexpr uint32 FACTION_SCENARIO_ALLY      = 2501;
    constexpr float ARMY_ENGAGE_RANGE           = 40.0f;  // opponents looked for around a soldier's post
    constexpr float ARMY_ADVANCE_RANGE          = 150.0f; // no opponent near its post: it advances towards one
    constexpr float ARMY_MARCH_ENGAGE_RANGE     = 15.0f;  // marching soldiers fight what they run into
    constexpr uint32 ARMY_MAX_ATTACKERS         = 2;      // per opponent, so the fights spread over the field
    constexpr float ARMY_HEAL_BELOW_PCT         = 35.0f;  // NPC fights never end; only players kill
    constexpr float ARMY_WALL_RANGE             = 70.0f;  // archers on the walls shoot down at the field
    constexpr float ESCORT_POINT_REACHED        = 3.0f;
    constexpr float ESCORT_WAIT_RANGE           = 35.0f;  // leaders wait for the players
    constexpr float ARRIVAL_CREDIT_RANGE        = 30.0f;
    constexpr size_t MIN_FRONT_ENEMIES_LEFT     = 25;
    constexpr size_t MIN_FIELD_ENEMIES_LEFT     = 25;
    constexpr size_t MIN_CHOKEPOINT_ENEMIES_LEFT = 15;
    constexpr Milliseconds WAR_MACHINE_ARRIVES_LENGTH = 24513ms; // CONVERSATION_WAR_MACHINE_ARRIVES
    constexpr Milliseconds NEXT_WAVE_DELAY      = 8s;
    constexpr Milliseconds WAVE_GIVE_UP_DELAY   = 120s;  // a wave with a member stuck out of reach still counts
    constexpr Milliseconds SCENE_FALLBACK       = 60s;   // in case no player reports the end of a scene
    constexpr Milliseconds MOVIE_FALLBACK       = 200s;
    constexpr Milliseconds HEROES_FOLLOW_ME     = 29294ms; // CONVERSATION_CUT_THEM_OFF "Heroes, follow me!"
    constexpr Milliseconds CHAMPIONS_WITH_ME    = 51479ms; // CONVERSATION_NATHANOS_BAINE "Champions, with me!"
    constexpr Milliseconds WITH_PLEASURE        = 5762ms;  // CONVERSATION_NATHANOS_LORTHEMAR
    constexpr Milliseconds BLIGHT_THROWERS_LENGTH = 9348ms;
    constexpr Milliseconds HOLD_YOUR_GROUND     = 24889ms; // CONVERSATION_OUTNUMBERED "ALLIANCE, HOLD YOUR GROUND!"
    constexpr Milliseconds OUTNUMBERED_LENGTH   = 28828ms;
    constexpr Milliseconds CHAMPIONS_BEAR_WITNESS = 64649ms; // CONVERSATION_FINALLY_SHOW_UP last line
    constexpr uint32 KING_PIECE_HALFWAY         = 75;      // of 150: Lor'themar's "Their assault will soon falter"

    // The road south of the keep, from the courtyard to the chokepoint west of the keep: guide
    // points with the line of plague gas clouds of the Alliance-view sniff in between (ground ~61).
    std::vector<Position> const NathanosRouteToSide = { { 1804.60f, 180.64f, 61.5f, 0.0f } };
    std::vector<Position> const NathanosRouteToFlank =
    {
        { 1792.30f, 142.40f, 61.5f, 0.0f },
        { 1741.90f, 122.10f, 61.5f, 0.0f },
        { 1684.30f, 124.10f, 61.5f, 0.0f },
        { 1626.60f, 131.40f, 61.5f, 0.0f },
        { 1603.50f, 130.90f, 61.5f, 0.0f },
    };
    std::vector<Position> const NathanosRouteToChokepoint =
    {
        { 1556.30f, 120.30f, 61.5f, 0.0f },
        { 1529.80f, 126.10f, 61.5f, 0.0f },
        { 1500.00f, 140.00f, 61.5f, 0.0f },
        { 1483.00f, 160.00f, 61.5f, 0.0f },
        { 1470.00f, 190.00f, 61.5f, 0.0f },
        { 1460.00f, 225.00f, 61.5f, 0.0f },
        { 1449.20f, 258.60f, 61.7f, 0.0f },
    };
    // Cry Havoc: the same road back.
    std::vector<Position> const NathanosRetreatLeg1 =
    {
        { 1460.00f, 225.00f, 61.5f, 0.0f },
        { 1470.00f, 190.00f, 61.5f, 0.0f },
        { 1483.00f, 160.00f, 61.5f, 0.0f },
        { 1500.00f, 140.00f, 61.5f, 0.0f },
        { 1529.80f, 126.10f, 61.5f, 0.0f },
        { 1558.42f, 123.92f, 61.5f, 0.0f },
        { 1603.50f, 130.90f, 61.5f, 0.0f },
        { 1684.30f, 124.10f, 61.5f, 0.0f },
        { 1741.90f, 122.10f, 61.5f, 0.0f }, // "Keep moving!"
    };
    std::vector<Position> const NathanosRetreatLeg2 =
    {
        { 1792.30f, 142.40f, 61.5f, 0.0f },
        { 1806.80f, 158.20f, 61.5f, 0.0f }, // "We're almost there!"
    };
    std::vector<Position> const NathanosRetreatLeg3 =
    {
        { 1804.60f, 180.64f, 61.5f, 0.0f },
        { 1780.90f, 238.30f, 61.0f, 0.0f },
    };

    // Static spawns that only belong to the Alliance scenario's view of the battle (any guid):
    // the Horde scenario summons its own. Only spawns on the surface (z > 0) are affected,
    // so nothing in the Undercity changes.
    std::unordered_set<uint32> const AllianceViewEntries =
    {
        124856, 125196, 124979, 125231, 124565, 124324, 124480, 126778, 125622, 124855, 133948, 124982, 125316, 138714,
        128398, 125097, 126513, 128374, 124984, 137174, 125140, 126605, 125126, 126780, 134111, 124281, 126614, 138806,
        124985, 136612, 124282, 125221, 138804, 138691, 144022, 125134, 126613, 125839, 124292, 138807, 127147, 130372,
        129017, 124674, 126786, 124657, 138720, 97275, 144144, 137107, 144500, 125357, 126309, 138805, 133517, 128953,
        138454, 136146, 125496, 126777, 136611, 125124, 124651, 138455, 125515, 125138, 124787, 124792, 124981, 133949,
        134015, 124297, 134142, 140128, 132079, 124299, 136394, 137939, 129043, 126795, 129179, 134016, 124980,
        // Horde scenario leaders, summoned by the script
        130365, 130367, 133240, 133243, 137109, 133249, 133250, 133251, 136392, 133410, 125516, 132693,
    };

    // Saurfang's routes. Short legs: each one is walked with pathfinding, or in a straight
    // line if no path is found. Dugi guide points (1653.6, 117.9) and (1597.0, 106.2).
    // Spies in Our Midst: after his first fight Saurfang waits here (placed in game).
    std::vector<Position> const RouteToSpiesWatch =
    {
        { 1700.00f,  66.00f, -62.29f, 0.0f }, // Saurfang's room
        { 1691.11f,  72.12f, -62.29f, 0.0f }, // north portcullis of the room
        { 1692.334473f, 141.002945f, -55.214329f, 0.0f },
    };

    std::vector<Position> const RouteToAmbush =
    {
        { 1653.60f, 117.90f, -62.00f, 0.0f },
        { 1597.00f, 106.20f, -62.18f, 0.0f },
        { 1596.00f, 126.00f, -62.18f, 1.57f }, // Mirabelle and Renzik cornered just ahead
    };

    std::vector<Position> const RouteToGate =
    {
        { 1595.30f, 160.00f, -62.18f, 0.0f },
        { 1596.50f, 167.00f, -62.18f, 1.57f },
    };

    std::vector<Position> const RouteToHeart =
    {
        { 1595.27f, 215.00f, -62.02f, 0.0f },
        SaurfangHeart,
    };

    std::vector<Position> const RouteToFrontLine = { SaurfangFrontLine };

    constexpr float SAURFANG_ENGAGE_RANGE       = 20.0f;
    constexpr float SAURFANG_POINT_REACHED      = 3.0f;
    constexpr float SAURFANG_LEASH_RANGE        = 40.0f;

    // Undercity elevators, where evacuated citizens head before leaving.
    Position const HeartExits[] =
    {
        { 1544.24f, 240.77f, -40.78f, 0.0f },
        { 1595.26f, 188.64f, -40.78f, 0.0f },
        { 1596.21f, 302.40f, -40.66f, 0.0f },
    };
}

enum class SaurfangMode : uint8
{
    Idle,
    FirstFight, // Spies in Our Midst: one fight, then to the watch point
    Route,
    Hold,       // steps 6-7: stays where he is and fights whoever comes close
};

enum class SaurfangRouteEnd : uint8
{
    SpiesWatch,
    Ambush,
    Gate,
    Heart,
    FrontLine,
    PushLine,
    WarMachine,
};

// Steps 6-13: someone walking a route set by the script (Nathanos, Lor'themar, Baine, Sylvanas).
enum class EscortEnd : uint8
{
    None,
    NathanosAtSide,
    NathanosAtFlank,
    NathanosAtChokepoint,
    NathanosRetreatLeg1,
    NathanosRetreatLeg2,
    NathanosAtCourtyard,
};

struct Escort
{
    ObjectGuid Guid;
    std::vector<Position> Points;
    size_t Index = 0;
    EscortEnd End = EscortEnd::None;
    bool WaitForPlayers = false;
};

// A step event sent once a player is near someone (e.g. "Follow Nathanos to the keep").
struct ArrivalCredit
{
    uint32 EventId;
    ObjectGuid Guid;
};

struct scenario_the_battle_for_lordaeron : public InstanceScript
{
    scenario_the_battle_for_lordaeron(InstanceMap* map) : InstanceScript(map) { }

    // The sniffed static spawns of map 1760 show the battle from the Alliance scenario's
    // point of view; the Horde scenario summons its own creatures step by step instead.
    // Other spawns on the map (e.g. added with .npc add) are kept.
    uint32 GetCreatureEntry(ObjectGuid::LowType spawnId, CreatureData const* data) override
    {
        bool allianceViewSpawn = spawnId >= FIRST_ALLIANCE_VIEW_SPAWN && spawnId <= LAST_ALLIANCE_VIEW_SPAWN;
        if (IsHordeScenario() && !allianceViewSpawn && data->posZ > 0.0f)
        {
            if (AllianceViewEntries.count(data->id))
                allianceViewSpawn = true;
            // Nothing on the surface that would attack the Horde players unless the script puts it there.
            else if (CreatureTemplate const* creatureTemplate = sObjectMgr->GetCreatureTemplate(data->id))
                if (FactionTemplateEntry const* faction = sFactionTemplateStore.LookupEntry(creatureTemplate->faction))
                    if (faction->EnemyGroup & (FACTION_MASK_PLAYER | FACTION_MASK_HORDE))
                        allianceViewSpawn = true;
        }
        return IsHordeScenario() && allianceViewSpawn ? 0 : data->id;
    }

    // Blight clouds on the map wait for Sylvanas' blight (Our Secret Weapon).
    uint32 GetGameObjectEntry(ObjectGuid::LowType spawnId, uint32 entry) override
    {
        if (!spawnId || !IsHordeScenario() || !IsBlightCloud(entry))
            return entry;

        if (GameObjectData const* data = sObjectMgr->GetGOData(spawnId))
            _sniffedBlight[spawnId] = { entry, Position(data->posX, data->posY, data->posZ, data->orientation) };
        return 0;
    }

    static bool IsBlightCloud(uint32 entry)
    {
        return entry == GO_BLIGHT_01 || entry == GO_BLIGHT_02 || entry == GO_BLIGHT_03 || entry == GO_BLIGHT_04 || entry == GO_BLIGHT_01_FIELD;
    }

    void OnPlayerEnter(Player* player) override
    {
        InstanceScript::OnPlayerEnter(player);

        TC_LOG_INFO("scripts", "Battle for Lordaeron: %s entered (scenario %u, step %u)", player->GetName().c_str(),
            instance->GetInstanceScenario() && instance->GetInstanceScenario()->GetEntry() ? instance->GetInstanceScenario()->GetEntry()->ID : 0, GetCurrentStepId());

        if (!IsHordeScenario())
            return;

        // LFG already places the player at the start (lfg_dungeon_template); only move
        // players who entered elsewhere, after the loading screen.
        if (GetCurrentStepId() == STEP_H_UNDER_SIEGE)
        {
            ObjectGuid playerGuid = player->GetGUID();
            _scheduler.Schedule(5s, [this, playerGuid](TaskContext /*context*/)
            {
                if (Player* p = ObjectAccessor::GetPlayer(instance, playerGuid))
                    if (!p->IsBeingTeleported() && p->GetDistance(PlayerStartPos) > START_TELEPORT_RANGE)
                        p->NearTeleportTo(PlayerStartPos);
            });
        }

        if (_started)
            return;

        _started = true;
        // Original Undercity residents (map 0 entries) are civilians; the scenario's own
        // Horde soldiers (130000+) fight.
        for (ObjectGuid const& guid : SummonGroup(SUMMON_GROUP_MAGIC_QUARTER))
            if (Creature* creature = instance->GetCreature(guid))
                if (creature->GetEntry() < FIRST_SCENARIO_CREATURE_ENTRY)
                    MakeCivilian(creature);

        _magicQuarterCitizens = SummonGroup(SUMMON_GROUP_MAGIC_QUARTER_EVAC);
        for (ObjectGuid const& guid : _magicQuarterCitizens)
            if (Creature* creature = instance->GetCreature(guid))
                MakeCivilian(creature);

        if (Creature* saurfang = SummonSaurfang(SaurfangMagicQuarter))
        {
            TC_LOG_INFO("scripts", "Battle for Lordaeron: Saurfang summoned with AI '%s'", saurfang->GetScriptName().c_str());
        }

        _scheduler.Schedule(1s, [this](TaskContext context)
        {
            UpdatePlayerChecks();
            context.Repeat(1s);
        });
    }

    void OnGameObjectCreate(GameObject* go) override
    {
        InstanceScript::OnGameObjectCreate(go);

        switch (go->GetSpawnId())
        {
            case GO_SPAWN_MAGIC_QUARTER_PORTCULLIS_1:
            case GO_SPAWN_MAGIC_QUARTER_PORTCULLIS_2:
            case GO_SPAWN_MAGIC_QUARTER_PORTCULLIS_3:
                _magicQuarterPortcullises.insert(go->GetGUID());
                break;
            case GO_SPAWN_CANAL_PORTCULLIS:
                _canalPortcullises.insert(go->GetGUID());
                break;
            case GO_SPAWN_HEART_PORTCULLIS_1:
            case GO_SPAWN_HEART_PORTCULLIS_2:
                _heartPortcullises.insert(go->GetGUID());
                break;
            case GO_SPAWN_KEEP_FRONT_GATE_INNER:
            case GO_SPAWN_KEEP_FRONT_GATE_OUTER:
                _keepFrontGates.insert(go->GetGUID());
                break;
            case GO_SPAWN_THRONE_ROOM_GATE:
            case GO_SPAWN_COURTYARD_GATE_SOUTH:
            case GO_SPAWN_COURTYARD_GATE_NORTH:
                _throneRoomGates.insert(go->GetGUID());
                break;
            default:
                break;
        }
    }

    void Update(uint32 diff) override
    {
        _scheduler.Update(diff);
    }

    void OnUnitDeath(Unit* unit) override
    {
        Creature* creature = unit->ToCreature();
        if (!creature)
            return;

        ObjectGuid guid = creature->GetGUID();
        if (_spies.erase(guid))
            DoSendScenarioEvent(SCENARIO_EVENT_H_SPY_KILLED);
        else if (_heartEnemies.erase(guid))
            DoSendScenarioEvent(SCENARIO_EVENT_H_HEART_ENEMY_KILLED);
        else if (_druids.erase(guid) && _druids.empty() && GetCurrentStepId() == STEP_H_TO_THE_HEART)
        {
            DoSendScenarioEvent(SCENARIO_EVENT_H_AMBUSH_CLEARED);
            BuildHeartGateRoutes();
            StartSaurfangRoute(_gateRoute, SaurfangRouteEnd::Gate);
        }
        else
            OnArmyUnitDeath(creature);
    }

    void SetData(uint32 type, uint32 data) override
    {
        switch (type)
        {
            case DATA_JUMP_TO_STEP:
                JumpToStep(data);
                break;
            case DATA_MOVIE_COMPLETE:
                if (data == MOVIE_TURN_THE_TIDE)
                    BeginLikeWildfire();
                break;
            case DATA_SCENE_COMPLETE:
                if (data == SCENE_WAR_MACHINE_DESTROYED)
                    OnWarMachineSceneDone();
                else if (data == SCENE_SYLVANAS_DUMPS_BLIGHT)
                    OnBlightSceneDone();
                break;
            default:
                break;
        }
    }

    void SetGuidData(uint32 type, ObjectGuid data) override
    {
        if (type == DATA_EVACUATE_CITIZEN)
        {
            if (Creature* citizen = instance->GetCreature(data))
                EvacuateCitizen(citizen);
            return;
        }

        if (type == DATA_WOUNDED_RESCUED)
        {
            if (Creature* wounded = instance->GetCreature(data))
                RescueWounded(wounded);
            return;
        }

        if (type == DATA_BLIGHT_SPRAY_HIT)
        {
            // Each Alliance soldier caught in the blight counts once.
            if (GetCurrentStepId() == STEP_H_OUR_SECRET_WEAPON && _armyEnemies.count(data) && _blightedEnemies.insert(data).second)
                DoSendScenarioEvent(SCENARIO_EVENT_H_BLIGHT_USED);
            return;
        }

        Player* player = ObjectAccessor::GetPlayer(instance, data);
        if (!player)
            return;

        switch (type)
        {
            case DATA_PLAYER_USED_KEEP_PORTAL:
                CreditPortalUse(player);
                break;
            default:
                break;
        }
    }

private:
    bool IsHordeScenario() const
    {
        InstanceScenario* scenario = instance->GetInstanceScenario();
        return scenario && scenario->GetEntry() && scenario->GetEntry()->ID == SCENARIO_FALL_OF_LORDAERON;
    }

    uint32 GetCurrentStepId() const
    {
        if (InstanceScenario* scenario = instance->GetInstanceScenario())
            if (ScenarioStepEntry const* step = scenario->GetStep())
                return step->ID;
        return 0;
    }

    Creature* GetSaurfang() const { return instance->GetCreature(_saurfangGuid); }

    // ---- Summons ---------------------------------------------------------------------

    // Summons with the position snapped to the floor right below it, so a slightly
    // wrong height never leaves a creature floating.
    Creature* Summon(uint32 entry, Position pos)
    {
        float groundZ = instance->GetHeight(PhasingHandler::GetEmptyPhaseShift(), pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ() + 2.0f, true, 10.0f);
        if (groundZ > INVALID_HEIGHT)
            pos.m_positionZ = groundZ;

        TempSummon* summon = instance->SummonCreature(entry, pos);
        if (summon)
            summon->SetHomePosition(pos);
        return summon;
    }

    // creature_summon_groups (map 1760). Summoned one by one instead of with
    // Map::SummonCreatureGroup so the members do not form a formation.
    GuidSet SummonGroup(uint8 groupId)
    {
        GuidSet guids;
        if (std::vector<TempSummonData> const* data = sObjectMgr->GetSummonGroup(MAP_LORDAERON_SCENARIO, SUMMONER_TYPE_MAP, groupId))
            for (TempSummonData const& summonData : *data)
                if (Creature* creature = Summon(summonData.entry, summonData.pos))
                    guids.insert(creature->GetGUID());
        return guids;
    }

    void DespawnAll(GuidSet& guids)
    {
        for (ObjectGuid const& guid : guids)
            if (Creature* creature = instance->GetCreature(guid))
                creature->DespawnOrUnsummon();
        guids.clear();
    }

    // Citizens are never attacked and never fight back.
    static void MakeCivilian(Creature* creature)
    {
        creature->SetReactState(REACT_PASSIVE);
        creature->AddUnitFlag(UnitFlags(UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_IMMUNE_TO_PC | UNIT_FLAG_IMMUNE_TO_NPC));
        creature->SetNpcFlags(UNIT_NPC_FLAG_NONE);
    }

    void StartConversation(uint32 conversationId, Unit* creator)
    {
        if (!creator)
            return;

        GuidUnorderedSet participants;
        for (auto& ref : instance->GetPlayers())
            if (Player* player = ref.GetSource())
                participants.insert(player->GetGUID());

        Conversation::CreateConversation(conversationId, creator, *creator, std::move(participants));
    }

    // Sends a civilian to the nearest exit and removes it there.
    static void LeaveTowards(Creature* creature, Position const& exit)
    {
        creature->SetEmoteState(EMOTE_STATE_NONE);
        creature->SetWalk(false);
        creature->GetMotionMaster()->MovePoint(0, exit);
        creature->DespawnOrUnsummon(8s);
    }

    // ---- Steps -----------------------------------------------------------------------

    // Spies in Our Midst: kill SI:7 Operatives and free the citizens they hold.
    void StartSpiesInOurMidst()
    {
        _spies = SummonGroup(SUMMON_GROUP_SPIES);
        SummonCaptives();

        _saurfangMode = SaurfangMode::FirstFight;
        _saurfangFought = false;
    }

    void SummonCaptives()
    {
        GuidSet captives = SummonGroup(SUMMON_GROUP_CAPTIVES);
        Merge(_captives, captives);
        for (ObjectGuid const& guid : captives)
        {
            if (Creature* captive = instance->GetCreature(guid))
            {
                MakeCivilian(captive);
                captive->SetEmoteState(EMOTE_STATE_COWER);
            }
        }
    }

    // To the Heart!: Saurfang leads the way through the canals, finds Mirabelle and
    // Renzik cornered, they escape and the Druids of the Claw attack.
    void StartToTheHeart()
    {
        DespawnAll(_spies);
        _ambush = SummonGroup(SUMMON_GROUP_AMBUSH);
        // Mirabelle and Renzik stay put until they escape during the conversation.
        for (ObjectGuid const& guid : _ambush)
            if (Creature* creature = instance->GetCreature(guid))
                if (creature->GetEntry() == NPC_MIRABELLE || creature->GetEntry() == NPC_RENZIK_THE_SHIV)
                    MakeCivilian(creature);
        _gateGuards = SummonGroup(SUMMON_GROUP_GATE_GUARDS);
        OpenPortcullises(_canalPortcullises, "canal");

        StartSaurfangRoute(RouteToAmbush, SaurfangRouteEnd::Ambush);
    }

    void OnSaurfangReachedAmbush()
    {
        StartConversation(CONVERSATION_SPIES_CORNERED, GetSaurfang());

        _scheduler.Schedule(SPIES_VANISH_DELAY, [this](TaskContext /*context*/)
        {
            // Mirabelle and Renzik vanish; the Royal Dreadguards stay at the corner.
            for (ObjectGuid const& guid : _ambush)
                if (Creature* creature = instance->GetCreature(guid))
                    if (creature->GetEntry() == NPC_MIRABELLE || creature->GetEntry() == NPC_RENZIK_THE_SHIV)
                        creature->DespawnOrUnsummon();

            _druids = SummonGroup(SUMMON_GROUP_DRUIDS);
            for (ObjectGuid const& guid : _druids)
            {
                if (Creature* druid = instance->GetCreature(guid))
                {
                    druid->SetFaction(FACTION_SCENARIO_ENEMY);
                    if (druid->GetEntry() == NPC_DRUID_OF_THE_CLAW_LEADER)
                        StartConversation(CONVERSATION_DRUIDS_ATTACK, druid);
                }
            }
        });
    }

    // Evacuate the Heart of the City: citizens are told to get to the mage portal (gossip)
    // and the infiltrators are cut down.
    void StartEvacuateTheHeart()
    {
        DespawnAll(_ambush);
        DespawnAll(_captives);

        SummonHeartCitizens();
        _heartEnemies = SummonGroup(SUMMON_GROUP_HEART_ENEMIES);

        // The gate guards make way and move into the city.
        for (ObjectGuid const& guid : _gateGuards)
            if (Creature* guard = instance->GetCreature(guid))
                guard->GetMotionMaster()->MovePoint(0, SaurfangHeart);

        if (_heartRoute.empty())
            BuildHeartGateRoutes();
        StartSaurfangRoute(_heartRoute, SaurfangRouteEnd::Heart);
    }

    // The heart gate is the pair of portcullises (280729): Saurfang stops right in front of
    // them on his side and later walks through to the heart.
    void BuildHeartGateRoutes()
    {
        _gateRoute = RouteToGate;
        _heartRoute = RouteToHeart;

        Position center;
        float facing = 0.0f;
        uint32 count = 0;
        for (ObjectGuid const& guid : _heartPortcullises)
        {
            if (GameObject* go = instance->GetGameObject(guid))
            {
                center.m_positionX += go->GetPositionX();
                center.m_positionY += go->GetPositionY();
                center.m_positionZ += go->GetPositionZ();
                facing = go->GetOrientation();
                ++count;
            }
        }

        Creature* saurfang = GetSaurfang();
        if (!count || !saurfang)
        {
            TC_LOG_ERROR("scripts", "Battle for Lordaeron: heart portcullises not found, Saurfang uses the fixed gate route");
            return;
        }

        center.m_positionX /= count;
        center.m_positionY /= count;
        center.m_positionZ /= count;

        // The portcullis faces along the tunnel; Saurfang's side is the one he comes from.
        float const dirX = std::cos(facing);
        float const dirY = std::sin(facing);
        float const side = (saurfang->GetPositionX() - center.GetPositionX()) * dirX
            + (saurfang->GetPositionY() - center.GetPositionY()) * dirY < 0.0f ? -1.0f : 1.0f;

        auto pointAt = [&](float distance)
        {
            float x = center.GetPositionX() + dirX * side * distance;
            float y = center.GetPositionY() + dirY * side * distance;
            float z = instance->GetHeight(PhasingHandler::GetEmptyPhaseShift(), x, y, center.GetPositionZ() + 2.0f, true, 10.0f);
            if (z <= INVALID_HEIGHT)
                z = center.GetPositionZ();
            return Position(x, y, z);
        };

        _gateFacing = Position::NormalizeOrientation(side > 0.0f ? facing + float(M_PI) : facing);
        Position front = pointAt(GATE_FRONT_DISTANCE);
        front.SetOrientation(_gateFacing);

        _gateRoute = { front };
        _heartRoute = { pointAt(-GATE_INSIDE_DISTANCE), SaurfangHeart };

        TC_LOG_INFO("scripts", "Battle for Lordaeron: heart gate at %.2f %.2f %.2f, Saurfang stops at %.2f %.2f %.2f",
            center.GetPositionX(), center.GetPositionY(), center.GetPositionZ(),
            front.GetPositionX(), front.GetPositionY(), front.GetPositionZ());
    }

    void SummonHeartCitizens()
    {
        GuidSet citizens = SummonGroup(SUMMON_GROUP_HEART_CITIZENS);
        Merge(_heartCitizens, citizens);
        // Talking to them offers "The Undercity is under attack! Get to the mage portal!"
        // (npc_lordaeron_undercity_citizen).
        for (ObjectGuid const& guid : citizens)
        {
            if (Creature* citizen = instance->GetCreature(guid))
            {
                MakeCivilian(citizen);
                citizen->SetNpcFlags(UNIT_NPC_FLAG_GOSSIP);
                // Without this ScriptName the gossip has no "Get to the mage portal!" option.
                if (citizen->GetScriptName() != "npc_lordaeron_undercity_citizen")
                    TC_LOG_ERROR("scripts", "Battle for Lordaeron: citizen %u has ScriptName '%s', expected 'npc_lordaeron_undercity_citizen' "
                        "(run 2026_11_05_00_world_battle_for_lordaeron_undercity.sql on the world DB and restart)",
                        citizen->GetEntry(), citizen->GetScriptName().c_str());
            }
        }
    }

    void EvacuateCitizen(Creature* citizen)
    {
        if (GetCurrentStepId() != STEP_H_EVACUATE_HEART || !_heartCitizens.erase(citizen->GetGUID()))
            return;

        DoSendScenarioEvent(SCENARIO_EVENT_H_CITIZEN_EVACUATED);
        LeaveTowards(citizen, *std::min_element(std::begin(HeartExits), std::end(HeartExits), [citizen](Position const& a, Position const& b)
        {
            return citizen->GetExactDistSq(a) < citizen->GetExactDistSq(b);
        }));
    }

    // To Battle!: Anastasia opens a portal to the keep, where Sylvanas waits.
    void StartToBattle()
    {
        DespawnAll(_heartEnemies);

        Creature* saurfang = GetSaurfang();
        StartConversation(CONVERSATION_PORTAL_TO_THE_KEEP, saurfang);

        if (Creature* anastasia = Summon(NPC_ANASTASIA_HARTWELL, AnastasiaHeart))
            MakeCivilian(anastasia);

        _scheduler.Schedule(5s, [this](TaskContext /*context*/)
        {
            if (Creature* portal = Summon(NPC_PORTAL_TO_THE_KEEP, KeepPortal))
            {
                // Its template carries unrelated auras from an old map 650 spawn; the model is the portal.
                portal->RemoveAllAuras();
                portal->SetNpcFlags(UNIT_NPC_FLAG_SPELLCLICK);
                _keepPortalOpen = true;
            }
        });

        _courtyard = SummonGroup(SUMMON_GROUP_COURTYARD);
    }

    void OnFirstPlayerInCourtyard()
    {
        // Saurfang came through the portal as well.
        if (Creature* saurfang = GetSaurfang())
            saurfang->DespawnOrUnsummon();

        Creature* saurfang = SummonSaurfang(SaurfangCourtyard);
        if (!saurfang)
            return;

        Creature* sylvanas = nullptr;
        for (ObjectGuid const& guid : _courtyard)
            if (Creature* creature = instance->GetCreature(guid))
                if (creature->GetEntry() == NPC_SYLVANAS)
                    sylvanas = creature;

        StartConversation(CONVERSATION_UNDERCITY_EVACUATED, sylvanas ? sylvanas : saurfang);

        // "Lok'tar ogar!": the keep gates open and he charges; Follow Saurfang to battle is done.
        _scheduler.Schedule(LOKTAR_OGAR_LINE_START, [this](TaskContext /*context*/)
        {
            OpenPortcullises(_keepFrontGates, "keep front");
            DoSendScenarioEvent(SCENARIO_EVENT_H_FOLLOW_SAURFANG_BATTLE);
            _followSaurfangToBattleDone = true;
            TryCompleteToBattle();
            StartSaurfangRoute(RouteToFrontLine, SaurfangRouteEnd::FrontLine);
        });
    }

    // ---- Saurfang ---------------------------------------------------------------------
    // Driven from here (not from his AI) so he moves whatever AI the creature runs.

    Creature* SummonSaurfang(Position const& pos)
    {
        _saurfangMode = SaurfangMode::Idle;
        Creature* saurfang = Summon(NPC_SAURFANG, pos);
        if (!saurfang)
            return nullptr;

        _saurfangGuid = saurfang->GetGUID();
        // The template is disarmed and not selectable; here he fights.
        saurfang->RemoveUnitFlag(UnitFlags(UNIT_FLAG_DISARMED | UNIT_FLAG_NOT_SELECTABLE));
        saurfang->SetReactState(REACT_AGGRESSIVE);
        return saurfang;
    }

    void StartSaurfangRoute(std::vector<Position> const& route, SaurfangRouteEnd end)
    {
        _saurfangRoute = &route;
        _saurfangRouteIndex = 0;
        _saurfangRouteEnd = end;
        _saurfangMode = SaurfangMode::Route;
        TC_LOG_INFO("scripts", "Battle for Lordaeron: Saurfang sets off (route end %u)", uint32(end));
    }

    void UpdateSaurfang()
    {
        Creature* saurfang = GetSaurfang();
        if (!saurfang || !saurfang->IsAlive())
            return;

        // Whatever AI he runs, keep him able to fight and running, not walking.
        saurfang->RemoveUnitFlag(UnitFlags(UNIT_FLAG_DISARMED | UNIT_FLAG_NOT_SELECTABLE));
        saurfang->SetReactState(REACT_AGGRESSIVE);
        saurfang->SetWalk(false);

        if (saurfang->IsInCombat())
        {
            _saurfangFought = true;

            // A fight he cannot finish (no target, or one far out of reach) must not stop him.
            Unit* victim = saurfang->GetVictim();
            if (victim && saurfang->GetDistance(victim) <= SAURFANG_LEASH_RANGE)
                return;
            saurfang->AI()->EnterEvadeMode();
        }

        // After a fight he carries on from where he is instead of walking back.
        saurfang->SetHomePosition(*saurfang);

        // He fights whoever is in his way while fighting or walking; waiting, he only
        // defends himself. Targets come from the scenario's own enemy lists: factions
        // 2501/2502 are not hostile by template.
        Creature* enemy = _saurfangMode != SaurfangMode::Idle ? NearestEnemyOf(saurfang) : nullptr;
        if (enemy)
        {
            TC_LOG_INFO("scripts", "Battle for Lordaeron: Saurfang attacks %s (%u)", enemy->GetName().c_str(), enemy->GetEntry());
            saurfang->AI()->AttackStart(enemy);
            if (!enemy->IsInCombat() && enemy->IsAIEnabled)
                enemy->AI()->AttackStart(saurfang);
            return;
        }

        switch (_saurfangMode)
        {
            case SaurfangMode::FirstFight:
                // His first fight is over: he takes position and waits for the next step.
                if (_saurfangFought)
                    StartSaurfangRoute(RouteToSpiesWatch, SaurfangRouteEnd::SpiesWatch);
                break;
            case SaurfangMode::Route:
            {
                Position const& target = (*_saurfangRoute)[_saurfangRouteIndex];
                if (saurfang->GetDistance(target) > SAURFANG_POINT_REACHED)
                {
                    if (saurfang->movespline->Finalized())
                        saurfang->GetMotionMaster()->MovePoint(0, target);
                    return;
                }

                if (++_saurfangRouteIndex < _saurfangRoute->size())
                {
                    saurfang->GetMotionMaster()->MovePoint(0, (*_saurfangRoute)[_saurfangRouteIndex]);
                    return;
                }

                _saurfangMode = SaurfangMode::Idle;
                OnSaurfangRouteEnd(_saurfangRouteEnd);
                break;
            }
            default:
                break;
        }
    }

    void OnSaurfangRouteEnd(SaurfangRouteEnd end)
    {
        TC_LOG_INFO("scripts", "Battle for Lordaeron: Saurfang arrived (route end %u)", uint32(end));

        switch (end)
        {
            case SaurfangRouteEnd::Ambush:
                OnSaurfangReachedAmbush();
                break;
            case SaurfangRouteEnd::PushLine:
            case SaurfangRouteEnd::WarMachine:
                _saurfangMode = SaurfangMode::Hold;
                break;
            case SaurfangRouteEnd::Gate:
                // He stops in front of the portcullis, waits a moment, orders it open and
                // it opens as he says so.
                if (Creature* saurfang = GetSaurfang())
                    saurfang->SetFacingTo(_gateFacing);
                _scheduler.Schedule(GATE_WAIT_BEFORE_ORDER, [this](TaskContext context)
                {
                    StartConversation(CONVERSATION_OPEN_THE_GATE, GetSaurfang());
                    context.Schedule(GATE_OPEN_IN_ORDER, [this](TaskContext /*context*/)
                    {
                        OpenPortcullises(_heartPortcullises, "heart");
                        DoSendScenarioEvent(SCENARIO_EVENT_H_FOLLOW_SAURFANG_HEART);
                    });
                });
                break;
            default:
                break;
        }
    }

    Creature* NearestEnemyOf(Creature* saurfang) const
    {
        Creature* nearest = nullptr;
        for (GuidSet const* enemies : { &_spies, &_druids, &_heartEnemies, &_armyEnemies })
            for (ObjectGuid const& guid : *enemies)
                if (Creature* enemy = instance->GetCreature(guid))
                    if (enemy->IsAlive() && saurfang->IsWithinDist(enemy, SAURFANG_ENGAGE_RANGE)
                        && (!nearest || saurfang->GetDistance(enemy) < saurfang->GetDistance(nearest)))
                        nearest = enemy;
        return nearest;
    }

    // ---- Per-second checks -----------------------------------------------------------

    // Steps are started from here rather than from OnCompletedCriteriaTree, so they also
    // start when a GM skips a step (.instance complete_scenario_step).
    void OnStepChanged(uint32 stepId)
    {
        TC_LOG_INFO("scripts", "Battle for Lordaeron: step changed to %u", stepId);

        switch (stepId)
        {
            case STEP_H_SPIES_IN_OUR_MIDST:
                StartSpiesInOurMidst();
                break;
            case STEP_H_TO_THE_HEART:
                StartToTheHeart();
                break;
            case STEP_H_EVACUATE_HEART:
                StartEvacuateTheHeart();
                break;
            case STEP_H_TO_BATTLE:
                StartToBattle();
                break;
            case STEP_H_PUSH_THEM_BACK:
                StartPushThemBack();
                break;
            case STEP_H_DEFEND_WAR_MACHINE:
                StartDefendWarMachine();
                break;
            case STEP_H_OUR_SECRET_WEAPON:
                StartOurSecretWeapon();
                break;
            case STEP_H_LIKE_WILDFIRE:
                StartLikeWildfire();
                break;
            case STEP_H_HOLDING_THEM_BACK:
                StartHoldingThemBack();
                break;
            case STEP_H_THE_KING_PIECE:
                StartTheKingPiece();
                break;
            case STEP_H_CRY_HAVOC:
                StartCryHavoc();
                break;
            case STEP_H_THE_DARK_LADY:
                StartTheDarkLady();
                break;
            case STEP_H_FINALE:
                StartFinale();
                break;
            default:
                break;
        }
    }

    // Weighted steps never run dry: when few targets of a kind are left while the step is
    // still running, a new group of them arrives. A threshold is used rather than "none
    // left" so a target stuck out of reach can never block the step.
    void CheckReinforcements(uint32 stepId)
    {
        if (stepId == STEP_H_SPIES_IN_OUR_MIDST)
        {
            PruneGone(_spies);
            PruneGone(_captives);
            if (_spies.size() < MIN_SPIES_LEFT)
                ScheduleReinforcements(stepId, SUMMON_GROUP_SPIES);
            if (_captives.size() < MIN_CAPTIVES_LEFT)
                ScheduleReinforcements(stepId, SUMMON_GROUP_CAPTIVES);
        }
        else if (stepId == STEP_H_EVACUATE_HEART)
        {
            PruneGone(_heartEnemies);
            PruneGone(_heartCitizens);
            if (_heartEnemies.size() < MIN_HEART_ENEMIES_LEFT)
                ScheduleReinforcements(stepId, SUMMON_GROUP_HEART_ENEMIES);
            if (_heartCitizens.size() < MIN_HEART_CITIZENS_LEFT)
                ScheduleReinforcements(stepId, SUMMON_GROUP_HEART_CITIZENS);
        }
    }

    void ScheduleReinforcements(uint32 stepId, uint8 groupId)
    {
        if (!_pendingReinforcements.insert(groupId).second)
            return;

        _scheduler.Schedule(REINFORCEMENTS_DELAY, [this, stepId, groupId](TaskContext /*context*/)
        {
            _pendingReinforcements.erase(groupId);
            if (GetCurrentStepId() != stepId)
                return;

            TC_LOG_INFO("scripts", "Battle for Lordaeron: reinforcements arrive (summon group %u, step %u)", uint32(groupId), stepId);

            // New arrivals join the ones still standing, so those keep giving credit.
            switch (groupId)
            {
                case SUMMON_GROUP_SPIES:          Merge(_spies, SummonGroup(groupId)); break;
                case SUMMON_GROUP_CAPTIVES:       SummonCaptives(); break;
                case SUMMON_GROUP_HEART_ENEMIES:  Merge(_heartEnemies, SummonGroup(groupId)); break;
                case SUMMON_GROUP_HEART_CITIZENS: SummonHeartCitizens(); break;
                default: break;
            }
        });
    }

    static void Merge(GuidSet& into, GuidSet const& from)
    {
        into.insert(from.begin(), from.end());
    }

    void PruneGone(GuidSet& guids)
    {
        for (auto itr = guids.begin(); itr != guids.end();)
        {
            Creature* creature = instance->GetCreature(*itr);
            if (!creature || !creature->IsAlive())
                itr = guids.erase(itr);
            else
                ++itr;
        }
    }

    void UpdatePlayerChecks()
    {
        uint32 stepId = GetCurrentStepId();
        if (stepId != _currentStepId)
        {
            _currentStepId = stepId;
            OnStepChanged(stepId);
        }

        CheckReinforcements(stepId);
        UpdateSaurfang();
        UpdateArmies(stepId);
        UpdateEscorts();
        UpdateArrivalCredits();

        for (auto& ref : instance->GetPlayers())
        {
            Player* player = ref.GetSource();
            if (!player || !player->IsAlive() || player->IsBeingTeleported())
                continue;

            if (!_anastasiaGreeted)
                GreetFromAnastasia(player);

            switch (stepId)
            {
                case STEP_H_UNDER_SIEGE:
                    if (!_briefingStarted)
                        if (Creature* saurfang = GetSaurfang())
                            if (player->IsWithinDist(saurfang, REPORT_TO_SAURFANG_RANGE))
                                StartBriefing(saurfang);
                    break;
                case STEP_H_SPIES_IN_OUR_MIDST:
                    RescueCaptivesNear(player);
                    break;
                default:
                    break;
            }

            // Not tied to the step: "Follow Saurfang to battle" has amount 0 in Criteria.db2,
            // so To Battle! may already be completed once the portal is used.
            // The first player through the portal starts the courtyard scene.
            if (_keepPortalOpen && !_courtyardReached && player->GetDistance(CourtyardArrival) <= COURTYARD_ARRIVAL_RANGE)
            {
                _courtyardReached = true;
                OnFirstPlayerInCourtyard();
            }
            // Whoever came through the portal used it (also credited by the portal's spell script).
            if (_keepPortalOpen && player->GetDistance(CourtyardArrival) <= COURTYARD_ARRIVAL_RANGE)
                CreditPortalUse(player);
        }
    }

    // "Use the Portal" is a CRITERIA_TYPE_SEND_EVENT criteria, once per player.
    void CreditPortalUse(Player* player)
    {
        if (!_portalUsers.insert(player->GetGUID()).second)
            return;
        player->UpdateCriteria(CRITERIA_TYPE_SEND_EVENT, GAME_EVENT_H_USE_PORTAL);
        TC_LOG_INFO("scripts", "Battle for Lordaeron: %s used the portal to the keep", player->GetName().c_str());
        TryCompleteToBattle();
    }

    // To Battle! needs "Use the Portal" (CRITERIA_TYPE_SEND_EVENT) and "Follow Saurfang to battle".
    // CriteriaHandler::IsCompletedCriteria has no case for CRITERIA_TYPE_SEND_EVENT, so the
    // core never sees the step as done: it is completed here, the way the core would do it.
    void TryCompleteToBattle()
    {
        if (_portalUsers.empty() || !_followSaurfangToBattleDone)
            return;

        InstanceScenario* scenario = instance->GetInstanceScenario();
        if (!scenario)
            return;

        ScenarioStepEntry const* step = scenario->GetStep();
        if (!step || step->ID != STEP_H_TO_BATTLE)
            return;

        TC_LOG_INFO("scripts", "Battle for Lordaeron: To Battle! completed");
        scenario->SetStepState(step, SCENARIO_STEP_DONE);
        scenario->CompleteStep(step);
    }

    // =====================================================================================
    // Steps 6-13: the battle on the surface
    // =====================================================================================

    // ---- Helpers -------------------------------------------------------------------------

    std::vector<Position> SnapRoute(std::vector<Position> const& points) const
    {
        std::vector<Position> snapped;
        for (Position const& point : points)
            snapped.push_back(GroundPos(point.GetPositionX(), point.GetPositionY(), point.GetPositionZ(), point.GetOrientation()));
        return snapped;
    }

    Player* AnyPlayer() const
    {
        for (auto& ref : instance->GetPlayers())
            if (Player* player = ref.GetSource())
                return player;
        return nullptr;
    }

    bool IsPlayerNear(WorldObject const* object, float range) const
    {
        for (auto& ref : instance->GetPlayers())
            if (Player* player = ref.GetSource())
                if (player->IsAlive() && player->IsWithinDist(object, range))
                    return true;
        return false;
    }

    // Conversations of steps 6-13 use actor templates, so any creator near the players works.
    void StartConversationNear(uint32 conversationId, Unit* creator)
    {
        StartConversation(conversationId, creator ? creator : AnyPlayer());
    }

    void PlaySceneForPlayers(uint32 sceneId)
    {
        for (auto& ref : instance->GetPlayers())
            if (Player* player = ref.GetSource())
                player->GetSceneMgr().PlayScene(sceneId);
    }

    // ---- Blight ----------------------------------------------------------------------------

    // Ground height from high above: the field outside the gates goes from 60 down to 33.
    Position FieldGround(float x, float y) const
    {
        float z = instance->GetHeight(PhasingHandler::GetEmptyPhaseShift(), x, y, 75.0f, true, 60.0f);
        return Position(x, y, z > INVALID_HEIGHT ? z : 40.0f, 0.0f);
    }

    void SummonBlightCloud(Position const& pos)
    {
        static uint32 const clouds[] = { GO_BLIGHT_01, GO_BLIGHT_02, GO_BLIGHT_03, GO_BLIGHT_04 };
        uint32 entry = clouds[_blightClouds.size() % 4];
        float orientation = frand(0.0f, float(2 * M_PI));
        if (GameObject* cloud = instance->SummonGameObject(entry, pos, QuaternionData::fromEulerAnglesZYX(orientation, 0.0f, 0.0f), 0))
            _blightClouds.insert(cloud->GetGUID());
    }

    // Our Secret Weapon: the blight rains down on the Alliance army outside the gates.
    void SpreadFieldBlight()
    {
        if (_fieldBlightSpread)
            return;
        _fieldBlightSpread = true;

        for (auto const& sniffed : _sniffedBlight)
            if (GameObject* cloud = instance->SummonGameObject(sniffed.second.first, sniffed.second.second,
                QuaternionData::fromEulerAnglesZYX(sniffed.second.second.GetOrientation(), 0.0f, 0.0f), 0))
                _blightClouds.insert(cloud->GetGUID());

        for (float x = 1945.0f; x <= 2185.0f; x += 40.0f)
            for (float y = 200.0f; y <= 290.0f; y += 30.0f)
                SummonBlightCloud(FieldGround(x + frand(-8.0f, 8.0f), y + frand(-8.0f, 8.0f)));

        TC_LOG_INFO("scripts", "Battle for Lordaeron: the blight covers the field (%u clouds)", uint32(_blightClouds.size()));
    }

    // Like Wildfire: "Blight throwers, you will create a swath of blight between us and the enemy!"
    void SpreadChokepointSwath()
    {
        for (float x = 1430.0f; x <= 1510.0f; x += 16.0f)
            SummonBlightCloud(GroundPos(x, 346.0f + frand(-4.0f, 4.0f), 61.7f, 0.0f));
    }

    // Cry Havoc: Sylvanas plagues the chokepoint and the road behind the retreat.
    void SpreadBlightAlong(std::vector<Position> const& points)
    {
        for (Position const& point : points)
            SummonBlightCloud(GroundPos(point.GetPositionX(), point.GetPositionY(), point.GetPositionZ(), 0.0f));
    }

    // Leaders talk and walk, they never fight (and never die).
    static void MakeLeader(Creature* creature)
    {
        creature->SetReactState(REACT_PASSIVE);
        creature->AddUnitFlag(UnitFlags(UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_IMMUNE_TO_PC | UNIT_FLAG_IMMUNE_TO_NPC));
    }

    Creature* SummonLeader(uint32 entry, Position const& pos)
    {
        Creature* creature = Summon(entry, pos);
        if (creature)
        {
            // Horde leaders are on the players' side whatever their template says.
            bool alliance = entry == NPC_ANDUIN || entry == NPC_JAINA || entry == NPC_GENN || entry == NPC_ALLERIA || entry == NPC_MEKKATORQUE;
            creature->SetFaction(alliance ? FACTION_SCENARIO_ENEMY : FACTION_SCENARIO_ALLY);
            MakeLeader(creature);
            _phase2Npcs.insert(creature->GetGUID());
        }
        return creature;
    }

    // Many soldier templates come with flags of their ambient spawns (immune to players or to
    // NPCs, not selectable); in the battle every soldier can be fought.
    void PrepareSoldier(Creature* creature, bool alliance, Position const& post)
    {
        creature->RemoveUnitFlag(UnitFlags(UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_ATTACKABLE_1 | UNIT_FLAG_IMMUNE_TO_PC
            | UNIT_FLAG_IMMUNE_TO_NPC | UNIT_FLAG_NOT_SELECTABLE));
        creature->SetFaction(alliance ? FACTION_SCENARIO_ENEMY : FACTION_SCENARIO_ALLY);
        creature->SetReactState(REACT_AGGRESSIVE);
        creature->SetHomePosition(post);
        _armyPost[creature->GetGUID()] = post;
    }

    // Soldiers of a summon group: Horde ones join the scenario's side, Alliance ones fight it.
    // Wounded soldiers and plague wagons only stand there.
    GuidSet SummonArmy(uint8 groupId, bool alliance)
    {
        GuidSet soldiers;
        for (ObjectGuid const& guid : SummonGroup(groupId))
        {
            Creature* creature = instance->GetCreature(guid);
            if (!creature)
                continue;

            if (creature->GetEntry() == NPC_WOUNDED_HORDE_SOLDIER)
            {
                // Saurfang: "Your brothers and sisters lie wounded on the battlefield!"
                // Bandaging them (spellclick, like the Alliance scenario's wounded) gets them back on their feet.
                creature->SetReactState(REACT_PASSIVE);
                creature->AddUnitFlag(UnitFlags(UNIT_FLAG_IMMUNE_TO_PC | UNIT_FLAG_IMMUNE_TO_NPC));
                creature->SetNpcFlags(UNIT_NPC_FLAG_SPELLCLICK);
                creature->AddAura(SPELL_WOUNDED, creature);
                _armyDecor.insert(guid);
                continue;
            }

            if (creature->GetEntry() == NPC_PLAGUE_WAGON)
            {
                MakeCivilian(creature);
                _armyDecor.insert(guid);
                continue;
            }

            PrepareSoldier(creature, alliance, *creature);

            // The soldiers on the walls shoot from up there.
            if (groupId == SUMMON_GROUP_KEEP_WALLS)
                creature->SetControlled(true, UNIT_STATE_ROOT);
            soldiers.insert(guid);
        }
        return soldiers;
    }

    void DespawnArmies()
    {
        DespawnAll(_armyAllies);
        DespawnAll(_armyEnemies);
        DespawnAll(_heldEnemies);
        DespawnAll(_armyDecor);
        DespawnAll(_waveUnits);
        DespawnAll(_phase2Npcs);
        DespawnAll(_roadReserves);
        _chokepointAllies.clear();
        _chokepointEnemies.clear();
        _marchTargets.clear();
        _armyPost.clear();
        _armyTarget.clear();
        _escorts.clear();
        _arrivalCredits.clear();
        _waveActive = false;
    }

    // The battle: every soldier keeps its post from the sniffed layout and fights an opponent
    // near it, at most ARMY_MAX_ATTACKERS on the same one, so fights go on all over the field
    // instead of everyone piling up on the first enemy. Soldiers with nobody near their post
    // advance towards the closest opponent. Factions 2501/2502 are not hostile by template,
    // so the script starts the fights. NPCs never finish each other off: the kills are the
    // players' (a soldier a player hits is no longer healed).
    void UpdateArmies(uint32 stepId)
    {
        PruneGone(_armyAllies);
        PruneGone(_armyEnemies);
        PruneGone(_waveUnits);

        std::map<ObjectGuid, uint32> attackers;
        for (auto itr = _armyTarget.begin(); itr != _armyTarget.end();)
        {
            if ((!_armyAllies.count(itr->first) && !_armyEnemies.count(itr->first))
                || (!_armyAllies.count(itr->second) && !_armyEnemies.count(itr->second)))
                itr = _armyTarget.erase(itr);
            else
            {
                ++attackers[itr->second];
                ++itr;
            }
        }

        for (ObjectGuid const& guid : _armyAllies)
            if (Creature* soldier = instance->GetCreature(guid))
                UpdateSoldier(soldier, _armyEnemies, attackers);
        for (ObjectGuid const& guid : _armyEnemies)
            if (Creature* soldier = instance->GetCreature(guid))
                UpdateSoldier(soldier, _armyAllies, attackers);

        // A wave with a soldier stuck out of reach still ends.
        if (_waveActive && GameTime::GetGameTimeMS() - _waveStartTime > uint32(WAVE_GIVE_UP_DELAY.count()))
        {
            TC_LOG_INFO("scripts", "Battle for Lordaeron: wave %u ends with %u soldiers out of reach", _waveNumber, uint32(_waveUnits.size()));
            DespawnAll(_waveUnits);
            OnWaveDefeated();
        }

        CheckArmyReinforcements(stepId);
    }

    void UpdateSoldier(Creature* soldier, GuidSet const& opponents, std::map<ObjectGuid, uint32>& attackers)
    {
        if (!soldier->IsAlive() || !soldier->IsAIEnabled)
            return;

        ObjectGuid guid = soldier->GetGUID();

        // Fights between NPCs never end with a death; a soldier a player has hit can die.
        if (!_waveUnits.count(guid) && !soldier->HasLootRecipients() && soldier->GetHealthPct() < ARMY_HEAL_BELOW_PCT)
            soldier->SetFullHealth();

        // Fighting a player: left alone.
        if (Unit* victim = soldier->GetVictim())
            if (victim->GetCharmerOrOwnerPlayerOrPlayerItself())
                return;

        // Marching (waves, the Alliance charge at the chokepoint): fights only what it runs into.
        auto march = _marchTargets.find(guid);
        if (march != _marchTargets.end())
        {
            if (Creature* opponent = FindOpponent(soldier, *soldier, ARMY_MARCH_ENGAGE_RANGE, opponents, attackers))
            {
                Engage(soldier, opponent, attackers);
                return;
            }
            if (soldier->IsInCombat())
                return;
            if (soldier->GetDistance(march->second) <= ESCORT_POINT_REACHED)
            {
                _armyPost[guid] = march->second;
                soldier->SetHomePosition(march->second);
                _marchTargets.erase(march);
            }
            else if (soldier->movespline->Finalized())
            {
                soldier->SetWalk(false);
                soldier->GetMotionMaster()->MovePoint(0, march->second);
            }
            return;
        }

        Position post = _armyPost.count(guid) ? _armyPost[guid] : Position(*soldier);

        // Keeps fighting its opponent while that one is still around its post.
        auto current = _armyTarget.find(guid);
        if (current != _armyTarget.end())
        {
            Creature* opponent = instance->GetCreature(current->second);
            if (opponent && opponent->IsAlive() && opponent->GetDistance(post) <= ARMY_ENGAGE_RANGE)
            {
                if (soldier->GetVictim() != opponent)
                    soldier->AI()->AttackStart(opponent);
                return;
            }
            --attackers[current->second];
            _armyTarget.erase(current);
        }

        bool onWall = soldier->HasUnitState(UNIT_STATE_ROOT);
        if (Creature* opponent = FindOpponent(soldier, post, onWall ? ARMY_WALL_RANGE : ARMY_ENGAGE_RANGE, opponents, attackers))
        {
            Engage(soldier, opponent, attackers);
            return;
        }

        // Nobody near its post: it advances to the closest opponent and holds there.
        if (soldier->IsInCombat() || onWall)
            return;
        Creature* opponent = FindOpponent(soldier, *soldier, ARMY_ADVANCE_RANGE, opponents, attackers);
        // Every opponent around already has enough attackers: it joins one of those fights
        // rather than standing idle.
        if (!opponent)
        {
            if (Creature* crowded = FindOpponent(soldier, post, ARMY_ENGAGE_RANGE, opponents, attackers, std::numeric_limits<uint32>::max()))
            {
                Engage(soldier, crowded, attackers);
                return;
            }
            opponent = FindOpponent(soldier, *soldier, ARMY_ADVANCE_RANGE, opponents, attackers, std::numeric_limits<uint32>::max());
        }
        if (opponent)
        {
            float angle = opponent->GetAngle(soldier);
            Position front = GroundPos(opponent->GetPositionX() + std::cos(angle) * 6.0f, opponent->GetPositionY() + std::sin(angle) * 6.0f,
                opponent->GetPositionZ(), soldier->GetOrientation());
            _armyPost[guid] = front;
            soldier->SetHomePosition(front);
            soldier->SetWalk(false);
            soldier->GetMotionMaster()->MovePoint(0, front);
        }
    }

    // The closest opponent around a point that is not already fought by enough soldiers.
    Creature* FindOpponent(Creature* soldier, Position const& around, float range, GuidSet const& opponents,
        std::map<ObjectGuid, uint32> const& attackers, uint32 maxAttackers = ARMY_MAX_ATTACKERS) const
    {
        Creature* nearest = nullptr;
        float nearestDist = range;
        for (ObjectGuid const& guid : opponents)
        {
            auto count = attackers.find(guid);
            if (count != attackers.end() && count->second >= maxAttackers)
                continue;

            Creature* opponent = instance->GetCreature(guid);
            if (!opponent || !opponent->IsAlive() || opponent == soldier)
                continue;

            float dist = opponent->GetDistance(around);
            if (dist < nearestDist)
            {
                nearest = opponent;
                nearestDist = dist;
            }
        }
        return nearest;
    }

    void Engage(Creature* soldier, Creature* opponent, std::map<ObjectGuid, uint32>& attackers)
    {
        _armyTarget[soldier->GetGUID()] = opponent->GetGUID();
        ++attackers[opponent->GetGUID()];
        soldier->AI()->AttackStart(opponent);
    }

    void OnArmyUnitDeath(Creature* creature)
    {
        ObjectGuid guid = creature->GetGUID();
        _armyAllies.erase(guid);
        _armyPost.erase(guid);
        _armyTarget.erase(guid);
        _marchTargets.erase(guid);

        bool waveUnit = _waveUnits.erase(guid) > 0;
        if (!_armyEnemies.erase(guid))
            return;

        if (waveUnit && _waveActive && _waveUnits.empty())
            OnWaveDefeated();

        // Kills count when a player took part (the soldier was tapped), not when the
        // Horde army finished the job alone.
        if (!creature->HasLootRecipients())
            return;

        switch (GetCurrentStepId())
        {
            case STEP_H_PUSH_THEM_BACK:
                DoSendScenarioEvent(SCENARIO_EVENT_H_ALLIANCE_DEFEATED);
                break;
            case STEP_H_OUR_SECRET_WEAPON:
                DoSendScenarioEvent(SCENARIO_EVENT_H_ALLIANCE_DRIVEN_BACK);
                if (_blightedEnemies.insert(guid).second)
                    DoSendScenarioEvent(SCENARIO_EVENT_H_BLIGHT_USED);
                break;
            case STEP_H_THE_KING_PIECE:
            {
                // Weights 1, 3 and 10 (Criteria.db2): soldiers, Ren'dorei and mages, Flametrons.
                uint32 eventId = SCENARIO_EVENT_H_DEFEAT_ANDUIN;
                uint32 weight = 1;
                if (creature->GetEntry() == NPC_FLAMETRON_5000)
                {
                    eventId = SCENARIO_EVENT_H_DEFEAT_ANDUIN_ALLY_2;
                    weight = 10;
                }
                else if (creature->GetEntry() == NPC_RENDOREI_RANGER || creature->GetEntry() == NPC_ALLIANCE_MAGE)
                {
                    eventId = SCENARIO_EVENT_H_DEFEAT_ANDUIN_ALLY;
                    weight = 3;
                }
                DoSendScenarioEvent(eventId);

                uint32 before = _kingPieceProgress;
                _kingPieceProgress += weight;
                if (before < KING_PIECE_HALFWAY && _kingPieceProgress >= KING_PIECE_HALFWAY)
                    StartConversationNear(CONVERSATION_ASSAULT_WILL_FALTER, instance->GetCreature(_lorthemarGuid));
                break;
            }
            default:
                break;
        }
    }

    void CheckArmyReinforcements(uint32 stepId)
    {
        switch (stepId)
        {
            case STEP_H_PUSH_THEM_BACK:
                if (_armyEnemies.size() < MIN_FRONT_ENEMIES_LEFT)
                    ScheduleArmyReinforcements(stepId, SUMMON_GROUP_FRONT_ALLIANCE);
                break;
            case STEP_H_OUR_SECRET_WEAPON:
                if (_armyEnemies.size() < MIN_FIELD_ENEMIES_LEFT)
                    ScheduleArmyReinforcements(stepId, SUMMON_GROUP_FIELD_ALLIANCE);
                break;
            case STEP_H_THE_KING_PIECE:
                if (_armyEnemies.size() < MIN_CHOKEPOINT_ENEMIES_LEFT)
                    ScheduleArmyReinforcements(stepId, SUMMON_GROUP_CHOKEPOINT_ALLIANCE);
                break;
            default:
                break;
        }
    }

    void ScheduleArmyReinforcements(uint32 stepId, uint8 groupId)
    {
        if (!_pendingReinforcements.insert(groupId).second)
            return;

        _scheduler.Schedule(REINFORCEMENTS_DELAY, [this, stepId, groupId](TaskContext /*context*/)
        {
            _pendingReinforcements.erase(groupId);
            if (GetCurrentStepId() != stepId)
                return;

            TC_LOG_INFO("scripts", "Battle for Lordaeron: Alliance reinforcements arrive (summon group %u, step %u)", uint32(groupId), stepId);
            GuidSet soldiers = SummonArmy(groupId, true);
            if (groupId == SUMMON_GROUP_CHOKEPOINT_ALLIANCE)
            {
                MarchTo(soldiers, AllianceMarchTarget);
                Merge(_chokepointEnemies, soldiers);
            }
            Merge(_armyEnemies, soldiers);
        });
    }

    void MarchTo(GuidSet const& soldiers, Position const& target)
    {
        uint32 i = 0;
        for (ObjectGuid const& guid : soldiers)
        {
            // Spread out around the target instead of piling up on one spot.
            float angle = float(i % 8) * float(M_PI) / 4.0f;
            float dist = 3.0f + float(i / 8) * 3.0f;
            _marchTargets[guid] = GroundPos(target.GetPositionX() + std::cos(angle) * dist, target.GetPositionY() + std::sin(angle) * dist,
                target.GetPositionZ(), target.GetOrientation());
            ++i;
        }
    }

    // ---- Escorts -------------------------------------------------------------------------

    void StartEscort(Creature* creature, std::vector<Position> const& points, EscortEnd end, bool waitForPlayers)
    {
        if (!creature)
            return;

        _escorts.erase(std::remove_if(_escorts.begin(), _escorts.end(), [creature](Escort const& escort)
        {
            return escort.Guid == creature->GetGUID();
        }), _escorts.end());

        Escort escort;
        escort.Guid = creature->GetGUID();
        escort.Points = SnapRoute(points);
        escort.End = end;
        escort.WaitForPlayers = waitForPlayers;
        _escorts.push_back(escort);
    }

    void UpdateEscorts()
    {
        std::vector<std::pair<EscortEnd, ObjectGuid>> finished;
        for (auto itr = _escorts.begin(); itr != _escorts.end();)
        {
            Creature* creature = instance->GetCreature(itr->Guid);
            if (!creature || !creature->IsAlive())
            {
                itr = _escorts.erase(itr);
                continue;
            }

            creature->SetWalk(false);
            Position const& target = itr->Points[itr->Index];
            if (creature->GetDistance(target) > ESCORT_POINT_REACHED)
            {
                // Leading the players: waits for them when they fall behind.
                if (itr->WaitForPlayers && !IsPlayerNear(creature, ESCORT_WAIT_RANGE))
                {
                    if (!creature->movespline->Finalized())
                        creature->StopMoving();
                }
                else if (creature->movespline->Finalized())
                    creature->GetMotionMaster()->MovePoint(0, target);
                ++itr;
                continue;
            }

            if (++itr->Index < itr->Points.size())
            {
                ++itr;
                continue;
            }

            if (target.GetOrientation() != 0.0f)
                creature->SetFacingTo(target.GetOrientation());
            finished.emplace_back(itr->End, itr->Guid);
            itr = _escorts.erase(itr);
        }

        for (auto const& end : finished)
            OnEscortEnd(end.first, instance->GetCreature(end.second));
    }

    static bool IsInThroneRoom(Player const* player)
    {
        return player->GetPositionX() >= 1595.0f && player->GetPositionX() <= 1664.0f
            && player->GetPositionY() >= 205.0f && player->GetPositionY() <= 275.0f
            && player->GetPositionZ() >= 55.0f && player->GetPositionZ() <= 80.0f;
    }

    void UpdateArrivalCredits()
    {
        // The Dark Lady: done when a player follows Sylvanas into the throne room.
        if (_throneRoomOpen && !_throneRoomEntered && GetCurrentStepId() == STEP_H_THE_DARK_LADY)
        {
            for (auto& ref : instance->GetPlayers())
            {
                Player* player = ref.GetSource();
                if (player && player->IsAlive() && IsInThroneRoom(player))
                {
                    _throneRoomEntered = true;
                    TC_LOG_INFO("scripts", "Battle for Lordaeron: %s entered the throne room", player->GetName().c_str());
                    DoSendScenarioEvent(SCENARIO_EVENT_H_FOLLOW_SYLVANAS);
                    break;
                }
            }
        }

        for (auto itr = _arrivalCredits.begin(); itr != _arrivalCredits.end();)
        {
            Creature* creature = instance->GetCreature(itr->Guid);
            if (creature && IsPlayerNear(creature, ARRIVAL_CREDIT_RANGE))
            {
                DoSendScenarioEvent(itr->EventId);
                itr = _arrivalCredits.erase(itr);
            }
            else
                ++itr;
        }
    }

    void OnEscortEnd(EscortEnd end, Creature* creature)
    {
        TC_LOG_INFO("scripts", "Battle for Lordaeron: escort end %u", uint32(end));

        switch (end)
        {
            case EscortEnd::NathanosAtSide:
                // Nathanos sends Baine away, then leads the players to Lor'themar.
                StartConversationNear(CONVERSATION_NATHANOS_BAINE, creature);
                _scheduler.Schedule(CHAMPIONS_WITH_ME, [this](TaskContext /*context*/)
                {
                    StartEscort(instance->GetCreature(_nathanosGuid), NathanosRouteToFlank, EscortEnd::NathanosAtFlank, true);
                    StartEscort(instance->GetCreature(_baineGuid), { BaineCourtyard }, EscortEnd::None, false);
                    if (!instance->GetCreature(_lorthemarGuid))
                        if (Creature* lorthemar = SummonLeader(NPC_LORTHEMAR, LorthemarFlank))
                            _lorthemarGuid = lorthemar->GetGUID();
                });
                break;
            case EscortEnd::NathanosAtFlank:
                StartConversationNear(CONVERSATION_NATHANOS_LORTHEMAR, creature);
                _scheduler.Schedule(WITH_PLEASURE, [this](TaskContext /*context*/)
                {
                    StartEscort(instance->GetCreature(_lorthemarGuid), { LorthemarChokepoint }, EscortEnd::None, false);
                    StartEscort(instance->GetCreature(_nathanosGuid), NathanosRouteToChokepoint, EscortEnd::NathanosAtChokepoint, true);
                });
                break;
            case EscortEnd::NathanosAtChokepoint:
                // The reserves take position; "Soldiers, fall into position!"
                SummonChokepointArmy();
                StartConversationNear(CONVERSATION_BLIGHT_THROWERS, creature);
                SpreadChokepointSwath();
                _scheduler.Schedule(BLIGHT_THROWERS_LENGTH, [this](TaskContext /*context*/)
                {
                    if (Creature* nathanos = instance->GetCreature(_nathanosGuid))
                    {
                        nathanos->Yell(BROADCAST_TEXT_FALL_INTO_POSITION);
                        _arrivalCredits.push_back({ SCENARIO_EVENT_H_FOLLOW_NATHANOS_KEEP, nathanos->GetGUID() });
                    }
                });
                break;
            case EscortEnd::NathanosRetreatLeg1:
                SpreadBlightAlong(NathanosRetreatLeg1);
                StartConversationNear(CONVERSATION_KEEP_MOVING, creature);
                StartEscort(creature, NathanosRetreatLeg2, EscortEnd::NathanosRetreatLeg2, true);
                break;
            case EscortEnd::NathanosRetreatLeg2:
                SpreadBlightAlong(NathanosRetreatLeg2);
                StartConversationNear(CONVERSATION_ALMOST_THERE, creature);
                StartEscort(creature, NathanosRetreatLeg3, EscortEnd::NathanosAtCourtyard, true);
                break;
            case EscortEnd::NathanosAtCourtyard:
                if (creature)
                    _arrivalCredits.push_back({ SCENARIO_EVENT_H_FOLLOW_NATHANOS_SAFETY, creature->GetGUID() });
                break;
            default:
                break;
        }
    }

    Creature* CourtyardLeader(uint32 entry) const
    {
        for (ObjectGuid const& guid : _courtyard)
            if (Creature* creature = instance->GetCreature(guid))
                if (creature->GetEntry() == entry && creature->IsAlive())
                    return creature;
        return nullptr;
    }

    // ---- Push Them Back --------------------------------------------------------------------
    // Out of the gates: the Horde army meets the Alliance front line.
    void StartPushThemBack()
    {
        // Sylvanas and Baine wait in the courtyard; they never fight the players.
        for (uint32 entry : { uint32(NPC_SYLVANAS), uint32(NPC_BAINE) })
            if (Creature* leader = CourtyardLeader(entry))
            {
                leader->SetFaction(FACTION_SCENARIO_ALLY);
                MakeLeader(leader);
            }

        Merge(_armyAllies, SummonArmy(SUMMON_GROUP_FRONT_HORDE, false));
        Merge(_armyEnemies, SummonArmy(SUMMON_GROUP_FRONT_ALLIANCE, true));
        SummonArmy(SUMMON_GROUP_KEEP_WALLS, false);

        if (!GetSaurfang())
            SummonSaurfang(SaurfangFrontLine);
        _phase2SaurfangRoute = SnapRoute({ SaurfangPushLine });
        StartSaurfangRoute(_phase2SaurfangRoute, SaurfangRouteEnd::PushLine);
    }

    // ---- Defend the Azerite War Machine ----------------------------------------------------
    void StartDefendWarMachine()
    {
        DespawnAll(_armyEnemies);

        PlaySceneForPlayers(SCENE_AZERITE_MACHINE_ARRIVES);
        if (Creature* warMachine = Summon(NPC_AZERITE_WAR_MACHINE, WarMachinePos))
        {
            MakeLeader(warMachine);
            _warMachineGuid = warMachine->GetGUID();
            _phase2Npcs.insert(_warMachineGuid);
        }

        Creature* saurfang = GetSaurfang();
        StartConversationNear(CONVERSATION_WAR_MACHINE_ARRIVES, saurfang);
        _phase2SaurfangRoute = SnapRoute({ WarMachineDefense });
        StartSaurfangRoute(_phase2SaurfangRoute, SaurfangRouteEnd::WarMachine);

        _waveNumber = 0;
        _scheduler.Schedule(WAR_MACHINE_ARRIVES_LENGTH, [this](TaskContext /*context*/)
        {
            SpawnWave(1);
        });
    }

    void SpawnWave(uint32 number)
    {
        if (GetCurrentStepId() != STEP_H_DEFEND_WAR_MACHINE)
            return;

        static std::vector<uint32> const waves[3] =
        {
            { NPC_KNIGHT_CAPTAIN_EMERY, NPC_STORMWIND_FOOTMAN, NPC_STORMWIND_FOOTMAN, NPC_STORMWIND_FOOTMAN, NPC_STORMWIND_FOOTMAN,
              NPC_DWARVEN_RIFLEMAN, NPC_DWARVEN_RIFLEMAN, NPC_DWARVEN_RIFLEMAN, NPC_DWARVEN_RIFLEMAN },
            { NPC_MASTER_SHAPESHIFTER_LYARA, NPC_DARNASSIAN_HUNTRESS, NPC_DARNASSIAN_HUNTRESS, NPC_DARNASSIAN_HUNTRESS,
              NPC_DARNASSIAN_HUNTRESS, NPC_GLAIVE_THROWER, NPC_DRUID_OF_THE_CLAW_2, NPC_DRUID_OF_THE_CLAW_2, NPC_DRUID_OF_THE_CLAW_2 },
            { NPC_MECHANORANGER_COGSPARK, NPC_EXODAR_PEACEKEEPER, NPC_EXODAR_PEACEKEEPER, NPC_EXODAR_PEACEKEEPER,
              NPC_AUCHENAI_BALLISTA, NPC_AUCHENAI_BALLISTA, NPC_GRYPHON_RIDER, NPC_GRYPHON_RIDER, NPC_GILNEAN_MAULER, NPC_GILNEAN_MAULER },
        };
        static uint32 const waveConversations[3] = { CONVERSATION_WAVE_EMERY, CONVERSATION_WAVE_LYARA, CONVERSATION_WAVE_COGSPARK };

        _waveNumber = number;
        _waveActive = true;
        _waveStartTime = GameTime::GetGameTimeMS();

        Creature* leader = nullptr;
        GuidSet wave;
        uint32 i = 0;
        for (uint32 entry : waves[number - 1])
        {
            // Rows of three, the leader in front.
            float x = WaveSpawn.GetPositionX() + float(i / 3) * 4.0f;
            float y = WaveSpawn.GetPositionY() + (float(i % 3) - 1.0f) * 4.0f;
            if (Creature* soldier = Summon(entry, GroundPos(x, y, WaveSpawn.GetPositionZ(), WaveSpawn.GetOrientation())))
            {
                PrepareSoldier(soldier, true, *soldier);
                wave.insert(soldier->GetGUID());
                if (!leader)
                    leader = soldier;
            }
            ++i;
        }

        MarchTo(wave, WaveTarget);
        Merge(_waveUnits, wave);
        Merge(_armyEnemies, wave);
        TC_LOG_INFO("scripts", "Battle for Lordaeron: wave %u of the Alliance attacks (%u soldiers)", number, uint32(wave.size()));

        StartConversationNear(waveConversations[number - 1], leader);
        if (number == 1)
            _scheduler.Schedule(6s, [this](TaskContext /*context*/)
            {
                StartConversationNear(CONVERSATION_BLAST_THE_TOWERS, GetSaurfang());
            });
    }

    void OnWaveDefeated()
    {
        _waveActive = false;
        DoSendScenarioEvent(SCENARIO_EVENT_H_WAVE_DEFEATED);
        TC_LOG_INFO("scripts", "Battle for Lordaeron: wave %u defeated", _waveNumber);

        if (_waveNumber < 3)
        {
            uint32 next = _waveNumber + 1;
            _scheduler.Schedule(NEXT_WAVE_DELAY, [this, next](TaskContext /*context*/)
            {
                SpawnWave(next);
            });
        }
        else
            DoSendScenarioEvent(SCENARIO_EVENT_H_WAR_MACHINE_DEFENDED);
    }

    // ---- Our Secret Weapon -----------------------------------------------------------------
    // Anduin breaks the War Machine, Sylvanas answers with the blight; Trizz hands out gas
    // masks and blight canisters while the Alliance is driven back.
    void StartOurSecretWeapon()
    {
        DespawnAll(_waveUnits);
        DespawnAll(_armyEnemies);
        _waveActive = false;
        _blightedEnemies.clear();
        _blightSceneStage = 0;

        PlaySceneForPlayers(SCENE_WAR_MACHINE_DESTROYED);
        _scheduler.Schedule(SCENE_FALLBACK, [this](TaskContext /*context*/) { OnWarMachineSceneDone(); });

        Merge(_armyEnemies, SummonArmy(SUMMON_GROUP_FIELD_ALLIANCE, true));
        Merge(_armyAllies, SummonArmy(SUMMON_GROUP_FIELD_HORDE, false));

        if (Creature* trizz = Summon(NPC_SLUDGE_GUARD_TRIZZ, TrizzPos))
        {
            MakeLeader(trizz);
            trizz->SetNpcFlags(UNIT_NPC_FLAG_GOSSIP);
            _trizzGuid = trizz->GetGUID();
            _phase2Npcs.insert(_trizzGuid);
        }
    }

    void OnWarMachineSceneDone()
    {
        if (_blightSceneStage != 0 || GetCurrentStepId() != STEP_H_OUR_SECRET_WEAPON)
            return;

        _blightSceneStage = 1;
        if (Creature* warMachine = instance->GetCreature(_warMachineGuid))
            warMachine->DespawnOrUnsummon();

        PlaySceneForPlayers(SCENE_SYLVANAS_DUMPS_BLIGHT);
        _scheduler.Schedule(SCENE_FALLBACK, [this](TaskContext /*context*/) { OnBlightSceneDone(); });
    }

    void OnBlightSceneDone()
    {
        if (_blightSceneStage != 1 || GetCurrentStepId() != STEP_H_OUR_SECRET_WEAPON)
            return;

        _blightSceneStage = 2;
        SpreadFieldBlight();
        if (Creature* trizz = instance->GetCreature(_trizzGuid))
            trizz->Yell(BROADCAST_TEXT_TRIZZ_GAS_MASKS);
        StartConversationNear(CONVERSATION_WOUNDED_ON_THE_FIELD, GetSaurfang());
    }

    // ---- Like Wildfire ---------------------------------------------------------------------
    // "Turn the Tide": Jaina's ship breaches the wall. After the movie the players are back in
    // the courtyard, where Nathanos takes the reserves to the chokepoint.
    void StartLikeWildfire()
    {
        // The battle outside the gates goes on; Trizz, the War Machine and the waves are done.
        DespawnAll(_waveUnits);
        DespawnAll(_phase2Npcs);
        _escorts.clear();
        _arrivalCredits.clear();
        _waveActive = false;
        if (Creature* saurfang = GetSaurfang())
            saurfang->DespawnOrUnsummon();
        _saurfangGuid.Clear();
        _saurfangMode = SaurfangMode::Idle;
        _likeWildfireStarted = false;

        for (auto& ref : instance->GetPlayers())
        {
            Player* player = ref.GetSource();
            if (!player)
                continue;

            ObjectGuid playerGuid = player->GetGUID();
            player->SendMovieStart(MOVIE_TURN_THE_TIDE);
            player->AddMovieDelayedAction(MOVIE_TURN_THE_TIDE, [playerGuid]()
            {
                if (Player* p = ObjectAccessor::FindPlayer(playerGuid))
                    if (p->GetMapId() == MAP_LORDAERON_SCENARIO)
                        p->NearTeleportTo(CourtyardArrival);
            });
        }

        _scheduler.Schedule(MOVIE_FALLBACK, [this](TaskContext /*context*/) { BeginLikeWildfire(); });
    }

    void BeginLikeWildfire()
    {
        if (_likeWildfireStarted || GetCurrentStepId() != STEP_H_LIKE_WILDFIRE)
            return;
        _likeWildfireStarted = true;

        // Players that never reported the end of the movie are brought along as well.
        for (auto& ref : instance->GetPlayers())
            if (Player* player = ref.GetSource())
                if (!player->IsBeingTeleported() && player->GetDistance(CourtyardArrival) > 60.0f)
                    player->NearTeleportTo(CourtyardArrival);

        if (_courtyard.empty())
            _courtyard = SummonGroup(SUMMON_GROUP_COURTYARD);

        Creature* sylvanas = CourtyardLeader(NPC_SYLVANAS);
        if (Creature* baine = CourtyardLeader(NPC_BAINE))
            _baineGuid = baine->GetGUID();
        if (Creature* nathanos = SummonLeader(NPC_NATHANOS, NathanosCourtyard))
            _nathanosGuid = nathanos->GetGUID();

        // The reserves: posts along the road south of the keep and the army at the chokepoint.
        SummonRoadReserves();
        SummonChokepointArmy();

        StartConversationNear(CONVERSATION_CUT_THEM_OFF, sylvanas);
        _scheduler.Schedule(HEROES_FOLLOW_ME, [this](TaskContext /*context*/)
        {
            StartEscort(instance->GetCreature(_nathanosGuid), NathanosRouteToSide, EscortEnd::NathanosAtSide, false);
            StartEscort(instance->GetCreature(_baineGuid), { BaineSide }, EscortEnd::None, false);
        });
    }

    // ---- Holding Them Back -----------------------------------------------------------------
    // Anduin, Jaina and Genn reach the chokepoint; Alleria and Mekkatorque come through a void tear.
    void StartHoldingThemBack()
    {
        EnsureChokepoint();

        _allianceLeaders.clear();
        for (auto const& leader : { std::make_pair(NPC_ANDUIN, AnduinChokepoint), std::make_pair(NPC_JAINA, JainaChokepoint), std::make_pair(NPC_GENN, GennChokepoint) })
            if (Creature* creature = SummonLeader(leader.first, leader.second))
                _allianceLeaders.insert(creature->GetGUID());

        // The Alliance army waits for the end of the talk.
        Merge(_heldEnemies, SummonArmy(SUMMON_GROUP_CHOKEPOINT_ALLIANCE, true));
        for (ObjectGuid const& guid : _heldEnemies)
            if (Creature* soldier = instance->GetCreature(guid))
                soldier->SetReactState(REACT_PASSIVE);

        StartConversationNear(CONVERSATION_OUTNUMBERED, instance->GetCreature(_nathanosGuid));

        _scheduler.Schedule(HOLD_YOUR_GROUND, [this](TaskContext /*context*/)
        {
            SummonVoidTear();
        });
        _scheduler.Schedule(OUTNUMBERED_LENGTH, [this](TaskContext /*context*/)
        {
            if (GetCurrentStepId() == STEP_H_HOLDING_THEM_BACK)
                DoSendScenarioEvent(SCENARIO_EVENT_H_INTERCEPT_ALLIANCE);
        });
    }

    // Nathanos, Lor'themar and the Horde reserves at the chokepoint (also after a .lordaeron step).
    void EnsureChokepoint()
    {
        Creature* nathanos = instance->GetCreature(_nathanosGuid);
        if (!nathanos)
        {
            nathanos = SummonLeader(NPC_NATHANOS, NathanosChokepoint);
            if (nathanos)
                _nathanosGuid = nathanos->GetGUID();
        }
        else
            StartEscort(nathanos, { NathanosChokepoint }, EscortEnd::None, false);

        Creature* lorthemar = instance->GetCreature(_lorthemarGuid);
        if (!lorthemar)
        {
            lorthemar = SummonLeader(NPC_LORTHEMAR, LorthemarChokepoint);
            if (lorthemar)
                _lorthemarGuid = lorthemar->GetGUID();
        }

        SummonChokepointArmy();
        if (_roadReserves.empty())
            SummonRoadReserves();
    }

    void SummonChokepointArmy()
    {
        if (!_chokepointAllies.empty())
            return;
        _chokepointAllies = SummonArmy(SUMMON_GROUP_CHOKEPOINT_HORDE, false);
        Merge(_armyAllies, _chokepointAllies);
    }

    // Horde soldiers posted along the road; they hold their posts and do not join the fights.
    void SummonRoadReserves()
    {
        if (!_roadReserves.empty())
            return;
        for (ObjectGuid const& guid : SummonGroup(SUMMON_GROUP_ROAD_RESERVES))
        {
            if (Creature* soldier = instance->GetCreature(guid))
            {
                soldier->SetFaction(FACTION_SCENARIO_ALLY);
                soldier->SetReactState(REACT_DEFENSIVE);
                _roadReserves.insert(guid);
            }
        }
    }

    void SummonVoidTear()
    {
        if (_voidTearOpen)
            return;
        _voidTearOpen = true;

        for (auto const& leader : { std::make_pair(NPC_ALLERIA, AlleriaVoidTear), std::make_pair(NPC_MEKKATORQUE, MekkatorqueVoidTear) })
            if (Creature* creature = SummonLeader(leader.first, leader.second))
                _allianceLeaders.insert(creature->GetGUID());

        GuidSet arrivals = SummonArmy(SUMMON_GROUP_VOID_TEAR, true);
        for (ObjectGuid const& guid : arrivals)
            if (Creature* soldier = instance->GetCreature(guid))
                soldier->SetReactState(REACT_PASSIVE);
        Merge(_heldEnemies, arrivals);
    }

    // ---- The King Piece --------------------------------------------------------------------
    void StartTheKingPiece()
    {
        EnsureChokepoint();
        if (_allianceLeaders.empty())
            for (auto const& leader : { std::make_pair(NPC_ANDUIN, AnduinChokepoint), std::make_pair(NPC_JAINA, JainaChokepoint), std::make_pair(NPC_GENN, GennChokepoint) })
                if (Creature* creature = SummonLeader(leader.first, leader.second))
                    _allianceLeaders.insert(creature->GetGUID());
        if (_heldEnemies.empty() && _armyEnemies.empty())
            Merge(_heldEnemies, SummonArmy(SUMMON_GROUP_CHOKEPOINT_ALLIANCE, true));
        SummonVoidTear();

        StartConversationNear(CONVERSATION_EVEN_THE_ODDS, instance->GetCreature(_nathanosGuid));

        // The Alliance charges the reserves.
        _kingPieceProgress = 0;
        for (ObjectGuid const& guid : _heldEnemies)
            if (Creature* soldier = instance->GetCreature(guid))
                soldier->SetReactState(REACT_AGGRESSIVE);
        MarchTo(_heldEnemies, AllianceMarchTarget);
        Merge(_armyEnemies, _heldEnemies);
        Merge(_chokepointEnemies, _heldEnemies);
        _heldEnemies.clear();
    }

    // ---- Cry Havoc -------------------------------------------------------------------------
    // Sylvanas calls the retreat and blocks the Alliance with exploding cisterns; Nathanos
    // leads everyone back to the courtyard.
    void StartCryHavoc()
    {
        EnsureChokepoint();

        Creature* sylvanas = SummonLeader(NPC_SYLVANAS, SylvanasCryHavoc);
        Creature* baine = SummonLeader(NPC_BAINE, BaineCryHavoc);
        ObjectGuid baineGuid = baine ? baine->GetGUID() : ObjectGuid::Empty;
        ObjectGuid sylvanasGuid = sylvanas ? sylvanas->GetGUID() : ObjectGuid::Empty;

        _scheduler.Schedule(2s, [this, baineGuid](TaskContext /*context*/)
        {
            if (Creature* b = instance->GetCreature(baineGuid))
                b->Yell(BROADCAST_TEXT_BAINE_RETREAT);
        });
        _scheduler.Schedule(4s, [this](TaskContext /*context*/)
        {
            PlaySceneForPlayers(SCENE_SYLVANAS_BLOCKS_ALLIANCE);
            for (float x = 1440.0f; x <= 1500.0f; x += 20.0f)
                for (float y = 250.0f; y <= 330.0f; y += 20.0f)
                    SummonBlightCloud(GroundPos(x + frand(-5.0f, 5.0f), y + frand(-5.0f, 5.0f), 61.7f, 0.0f));
            for (ObjectGuid const& guid : _allianceLeaders)
                if (Creature* leader = instance->GetCreature(guid))
                    if (leader->GetEntry() == NPC_JAINA)
                        leader->Yell(BROADCAST_TEXT_JAINA_MUST_NOT_ESCAPE);
        });
        _scheduler.Schedule(7s, [this, sylvanasGuid](TaskContext /*context*/)
        {
            // The blight swallows the chokepoint; the battle outside the gates goes on.
            DespawnAll(_chokepointEnemies);
            DespawnAll(_heldEnemies);
            DespawnAll(_allianceLeaders);
            DespawnAll(_chokepointAllies);
            StartConversationNear(CONVERSATION_FOLLOW_NATHANOS, instance->GetCreature(sylvanasGuid));
            StartEscort(instance->GetCreature(_nathanosGuid), NathanosRetreatLeg1, EscortEnd::NathanosRetreatLeg1, true);
            if (Creature* lorthemar = instance->GetCreature(_lorthemarGuid))
                lorthemar->DespawnOrUnsummon(3s);
        });
        _scheduler.Schedule(20s, [this, sylvanasGuid, baineGuid](TaskContext /*context*/)
        {
            // Sylvanas holds the Alliance back and meets everyone in the courtyard.
            for (ObjectGuid const& guid : { sylvanasGuid, baineGuid })
                if (Creature* creature = instance->GetCreature(guid))
                    creature->DespawnOrUnsummon();
        });
    }

    // ---- The Dark Lady ---------------------------------------------------------------------
    // Saurfang is not there any more: he stayed out on the battlefield (the Alliance takes him;
    // the sniff plays his lines as a talking head, he is no actor in the world). Sylvanas
    // answers him, then Baine, Nathanos and the champions follow her to the throne room.
    void StartTheDarkLady()
    {
        if (_courtyard.empty())
            _courtyard = SummonGroup(SUMMON_GROUP_COURTYARD);

        Creature* sylvanas = CourtyardLeader(NPC_SYLVANAS);
        if (!sylvanas)
            sylvanas = SummonLeader(NPC_SYLVANAS, SylvanasCourtyard);
        if (!sylvanas)
            return;
        MakeLeader(sylvanas);
        sylvanas->SetFaction(FACTION_SCENARIO_ALLY);
        _sylvanasGuid = sylvanas->GetGUID();

        if (Creature* baine = CourtyardLeader(NPC_BAINE))
            _baineGuid = baine->GetGUID();
        if (!instance->GetCreature(_nathanosGuid))
            if (Creature* nathanos = SummonLeader(NPC_NATHANOS, NathanosRetreatLeg3.back()))
                _nathanosGuid = nathanos->GetGUID();

        StartConversationNear(CONVERSATION_FINALLY_SHOW_UP, sylvanas);
        // "Champions, with me": the gates open, Sylvanas, Baine and Nathanos walk into the
        // throne room and wait there; the step is done when a player comes in.
        _scheduler.Schedule(CHAMPIONS_BEAR_WITNESS, [this](TaskContext /*context*/)
        {
            OpenPortcullises(_throneRoomGates, "throne room");
            _throneRoomOpen = true;
            StartEscort(instance->GetCreature(_sylvanasGuid), { SylvanasThroneDoor, ThroneRoomEntrance, SylvanasThroneRoom }, EscortEnd::None, true);
            StartEscort(instance->GetCreature(_baineGuid), { BaineThroneDoor, ThroneRoomEntrance, BaineThroneRoom }, EscortEnd::None, false);
            StartEscort(instance->GetCreature(_nathanosGuid), { NathanosThroneDoor, ThroneRoomEntrance, NathanosThroneRoom }, EscortEnd::None, false);
        });
    }

    // Our Secret Weapon: a bandaged soldier gets up and heads back to the keep.
    void RescueWounded(Creature* wounded)
    {
        if (wounded->GetEntry() != NPC_WOUNDED_HORDE_SOLDIER || !_armyDecor.count(wounded->GetGUID()) || !_rescuedWounded.insert(wounded->GetGUID()).second)
            return;

        // Not in Criteria.db2 (the step only has "drive back" and "use blight"); counted as
        // driving the Alliance back so saving the wounded moves the progress bar too.
        if (GetCurrentStepId() == STEP_H_OUR_SECRET_WEAPON)
            DoSendScenarioEvent(SCENARIO_EVENT_H_ALLIANCE_DRIVEN_BACK);

        wounded->RemoveAurasDueToSpell(SPELL_WOUNDED);
        wounded->SetNpcFlags(UNIT_NPC_FLAG_NONE);
        wounded->SetStandState(UNIT_STAND_STATE_STAND);
        wounded->HandleEmoteCommand(EMOTE_ONESHOT_SALUTE);
        wounded->SetWalk(false);
        wounded->GetMotionMaster()->MovePoint(0, SaurfangFrontLine);
        wounded->DespawnOrUnsummon(12s);
    }

    // ---- Finale ----------------------------------------------------------------------------
    // The throne room movie; the scenario and the quest objective are done and the players
    // fly back to Orgrimmar.
    void StartFinale()
    {
        for (auto& ref : instance->GetPlayers())
        {
            Player* player = ref.GetSource();
            if (!player)
                continue;

            player->KilledMonsterCredit(NPC_SCENARIO_COMPLETE_CREDIT);
            ObjectGuid playerGuid = player->GetGUID();
            player->SendMovieStart(MOVIE_THRONE_ROOM_HORDE);
            player->AddMovieDelayedAction(MOVIE_THRONE_ROOM_HORDE, [playerGuid]()
            {
                if (Player* p = ObjectAccessor::FindPlayer(playerGuid))
                    if (p->GetMapId() == MAP_LORDAERON_SCENARIO)
                        p->TeleportTo(1, OrgrimmarGates);
            });
        }

        if (InstanceScenario* scenario = instance->GetInstanceScenario())
            if (ScenarioStepEntry const* step = scenario->GetStep())
                if (step->ID == STEP_H_FINALE)
                {
                    TC_LOG_INFO("scripts", "Battle for Lordaeron: scenario completed");
                    scenario->SetStepState(step, SCENARIO_STEP_DONE);
                    scenario->CompleteStep(step);
                }

        _scheduler.Schedule(MOVIE_FALLBACK, [this](TaskContext /*context*/)
        {
            for (auto& ref : instance->GetPlayers())
                if (Player* player = ref.GetSource())
                    player->TeleportTo(1, OrgrimmarGates);
        });
    }

    void OpenPortcullises(GuidSet const& portcullises, char const* name)
    {
        uint32 opened = 0;
        for (ObjectGuid const& guid : portcullises)
        {
            if (GameObject* portcullis = instance->GetGameObject(guid))
            {
                portcullis->SetGoState(GO_STATE_ACTIVE);
                ++opened;
            }
        }
        TC_LOG_INFO("scripts", "Battle for Lordaeron: %u %s portcullis opened", opened, name);
    }

    // ---- Test helper: .lordaeron step <2-13> ----------------------------------------
    // Skips forward to a step: what the earlier steps leave behind is set up (gates open,
    // their NPCs gone), Saurfang and the players are put where the step starts and the step
    // then starts as usual (OnStepChanged on the next tick).
    Position GroundPos(float x, float y, float z, float o) const
    {
        float ground = instance->GetHeight(PhasingHandler::GetEmptyPhaseShift(), x, y, z + 2.0f, true, 10.0f);
        return Position(x, y, ground > INVALID_HEIGHT ? ground : z, o);
    }

    void JumpToStep(uint32 number)
    {
        static uint32 const stepIds[] = { STEP_H_UNDER_SIEGE, STEP_H_SPIES_IN_OUR_MIDST, STEP_H_TO_THE_HEART, STEP_H_EVACUATE_HEART, STEP_H_TO_BATTLE,
            STEP_H_PUSH_THEM_BACK, STEP_H_DEFEND_WAR_MACHINE, STEP_H_OUR_SECRET_WEAPON, STEP_H_LIKE_WILDFIRE, STEP_H_HOLDING_THEM_BACK,
            STEP_H_THE_KING_PIECE, STEP_H_CRY_HAVOC, STEP_H_THE_DARK_LADY };
        if (number < 2 || number > 13)
            return;

        InstanceScenario* scenario = instance->GetInstanceScenario();
        ScenarioStepEntry const* target = sScenarioStepStore.LookupEntry(stepIds[number - 1]);
        ScenarioStepEntry const* current = scenario ? scenario->GetStep() : nullptr;
        if (!target || !current || current->OrderIndex >= target->OrderIndex)
        {
            TC_LOG_ERROR("scripts", "Battle for Lordaeron: cannot jump to step %u (only forward)", number);
            return;
        }

        _briefingStarted = true;
        OpenPortcullises(_magicQuarterPortcullises, "Magic Quarter");
        if (number >= 3)
        {
            DespawnAll(_spies);
            DespawnAll(_captives);
            OpenPortcullises(_canalPortcullises, "canal");
        }
        if (number >= 4)
        {
            DespawnAll(_ambush);
            DespawnAll(_druids);
            OpenPortcullises(_heartPortcullises, "heart");
        }
        if (number >= 5)
        {
            DespawnAll(_heartEnemies);
            DespawnAll(_heartCitizens);
        }
        if (number >= 6)
        {
            // To Battle! is behind: the courtyard is set and the keep gates are open.
            _keepPortalOpen = true;
            _courtyardReached = true;
            _followSaurfangToBattleDone = true;
            OpenPortcullises(_keepFrontGates, "keep front");
            if (_courtyard.empty())
                _courtyard = SummonGroup(SUMMON_GROUP_COURTYARD);
            DespawnArmies();
        }

        if (number >= 9)
        {
            SpreadFieldBlight();
            // From Like Wildfire on, Saurfang is gone (he shows up again for The Dark Lady).
            if (Creature* saurfang = GetSaurfang())
                saurfang->DespawnOrUnsummon();
            _saurfangGuid.Clear();
            _saurfangMode = SaurfangMode::Idle;

            Position playerPos = (number >= 10 && number <= 12) ? GroundPos(1458.0f, 262.0f, 61.7f, 1.0f) : CourtyardArrival;
            for (auto& ref : instance->GetPlayers())
                if (Player* player = ref.GetSource())
                    player->NearTeleportTo(playerPos);

            for (ScenarioStepEntry const* step : sScenarioStepStore)
                if (step->ScenarioID == target->ScenarioID && !step->IsBonusObjective() && step->OrderIndex < target->OrderIndex)
                    scenario->SetStepState(step, SCENARIO_STEP_DONE);
            scenario->SetStep(target);

            TC_LOG_INFO("scripts", "Battle for Lordaeron: jumped to step %u (%u)", number, target->ID);
            return;
        }

        Position saurfangPos;
        Position playerPos;
        switch (number)
        {
            case 2:
                saurfangPos = SaurfangMagicQuarter;
                playerPos = GroundPos(1706.0f, 60.0f, -62.29f, 0.0f);
                break;
            case 3:
                saurfangPos = GroundPos(1692.334473f, 141.002945f, -55.214329f, 3.14f);
                playerPos = GroundPos(1696.0f, 141.0f, -55.21f, 3.14f);
                break;
            case 4: // in front of the heart gate, on the canal side
                saurfangPos = GroundPos(1556.3f, 201.5f, -60.75f, 0.77f);
                playerPos = GroundPos(1552.5f, 198.0f, -60.75f, 0.77f);
                break;
            case 6: // out of the keep gates
                saurfangPos = GroundPos(SaurfangFrontLine.GetPositionX(), SaurfangFrontLine.GetPositionY(), SaurfangFrontLine.GetPositionZ(), 0.0f);
                playerPos = GroundPos(1850.0f, 238.3f, 62.1f, 0.0f);
                break;
            case 7:
                saurfangPos = GroundPos(SaurfangPushLine.GetPositionX(), SaurfangPushLine.GetPositionY(), SaurfangPushLine.GetPositionZ(), 0.0f);
                playerPos = GroundPos(1990.0f, 243.4f, 36.0f, 0.0f);
                break;
            case 8:
                saurfangPos = GroundPos(WarMachineDefense.GetPositionX(), WarMachineDefense.GetPositionY(), WarMachineDefense.GetPositionZ(), 0.0f);
                playerPos = GroundPos(1912.0f, 236.0f, 56.0f, 0.0f);
                break;
            default:
                saurfangPos = SaurfangHeart;
                playerPos = GroundPos(1592.0f, 228.0f, -62.08f, 2.4f);
                break;
        }

        _saurfangMode = SaurfangMode::Idle;
        if (Creature* saurfang = GetSaurfang())
        {
            saurfang->CombatStop(true);
            saurfang->GetMotionMaster()->Clear();
            saurfang->NearTeleportTo(saurfangPos);
            saurfang->SetHomePosition(saurfangPos);
        }
        else
            SummonSaurfang(saurfangPos);

        for (auto& ref : instance->GetPlayers())
            if (Player* player = ref.GetSource())
                player->NearTeleportTo(playerPos);

        for (ScenarioStepEntry const* step : sScenarioStepStore)
            if (step->ScenarioID == target->ScenarioID && !step->IsBonusObjective() && step->OrderIndex < target->OrderIndex)
                scenario->SetStepState(step, SCENARIO_STEP_DONE);
        scenario->SetStep(target);

        TC_LOG_INFO("scripts", "Battle for Lordaeron: jumped to step %u (%u)", number, target->ID);
    }

    // Under Siege: Saurfang briefs the players; when he is done the portcullis to the
    // canals opens and the step is reported.
    void StartBriefing(Creature* saurfang)
    {
        _briefingStarted = true;
        StartConversation(CONVERSATION_SAURFANG_BRIEFING, saurfang);
        TC_LOG_INFO("scripts", "Battle for Lordaeron: Saurfang briefing started");

        _scheduler.Schedule(SAURFANG_BRIEFING_LENGTH, [this](TaskContext /*context*/)
        {
            OpenPortcullises(_magicQuarterPortcullises, "Magic Quarter");

            DoSendScenarioEvent(SCENARIO_EVENT_H_REPORT_TO_SAURFANG);
        });
    }

    // "My portal is open, we must evacuate!" once the first player reaches the Magic Quarter.
    void GreetFromAnastasia(Player* player)
    {
        for (ObjectGuid const& guid : _magicQuarterCitizens)
        {
            Creature* anastasia = instance->GetCreature(guid);
            if (!anastasia || anastasia->GetEntry() != NPC_ANASTASIA_HARTWELL || !player->IsWithinDist(anastasia, ANASTASIA_GREET_RANGE))
                continue;

            _anastasiaGreeted = true;
            anastasia->AI()->Talk(0);
            return;
        }
    }

    // A captive is freed once no spy is left around it and a player comes close.
    void RescueCaptivesNear(Player* player)
    {
        for (auto itr = _captives.begin(); itr != _captives.end();)
        {
            Creature* captive = instance->GetCreature(*itr);
            if (!captive || !player->IsWithinDist(captive, RESCUE_RANGE) || IsSpyNear(captive))
            {
                ++itr;
                continue;
            }

            itr = _captives.erase(itr);
            DoSendScenarioEvent(SCENARIO_EVENT_H_CITIZEN_RESCUED);
            captive->HandleEmoteCommand(EMOTE_ONESHOT_CHEER);
            LeaveTowards(captive, MagicQuarterPortal);
        }
    }

    bool IsSpyNear(Creature* captive) const
    {
        for (ObjectGuid const& guid : _spies)
            if (Creature* spy = instance->GetCreature(guid))
                if (spy->IsAlive() && captive->IsWithinDist(spy, RESCUE_SPY_CLEAR_RANGE))
                    return true;
        return false;
    }

    TaskScheduler _scheduler;
    bool _started = false;
    bool _briefingStarted = false;
    uint32 _currentStepId = 0;
    bool _anastasiaGreeted = false;
    bool _keepPortalOpen = false;
    bool _courtyardReached = false;

    ObjectGuid _saurfangGuid;
    SaurfangMode _saurfangMode = SaurfangMode::Idle;
    std::vector<Position> const* _saurfangRoute = nullptr;
    size_t _saurfangRouteIndex = 0;
    SaurfangRouteEnd _saurfangRouteEnd = SaurfangRouteEnd::Ambush;
    bool _saurfangFought = false;
    GuidSet _magicQuarterPortcullises;
    GuidSet _canalPortcullises;
    GuidSet _heartPortcullises;
    GuidSet _keepFrontGates;
    GuidSet _portalUsers;
    bool _followSaurfangToBattleDone = false;
    std::vector<Position> _gateRoute;
    std::vector<Position> _heartRoute;
    float _gateFacing = 0.0f;
    std::set<uint8> _pendingReinforcements;
    GuidSet _magicQuarterCitizens;
    GuidSet _spies;
    GuidSet _captives;
    GuidSet _ambush;
    GuidSet _druids;
    GuidSet _gateGuards;
    GuidSet _heartCitizens;
    GuidSet _heartEnemies;
    GuidSet _courtyard;

    // Steps 6-13
    GuidSet _armyAllies;
    GuidSet _armyEnemies;
    GuidSet _heldEnemies;       // Alliance soldiers waiting for the end of a talk
    GuidSet _armyDecor;         // wounded soldiers, plague wagons, soldiers on the walls
    GuidSet _waveUnits;
    GuidSet _phase2Npcs;        // leaders, Trizz, the War Machine
    GuidSet _allianceLeaders;
    GuidSet _blightedEnemies;
    std::map<ObjectGuid, Position> _marchTargets;
    std::map<ObjectGuid, Position> _armyPost;      // where a soldier holds
    std::map<ObjectGuid, ObjectGuid> _armyTarget;  // the opponent it fights
    GuidSet _rescuedWounded;
    std::map<ObjectGuid::LowType, std::pair<uint32, Position>> _sniffedBlight;
    GuidSet _blightClouds;
    bool _fieldBlightSpread = false;
    GuidSet _throneRoomGates;
    bool _throneRoomOpen = false;
    bool _throneRoomEntered = false;
    GuidSet _roadReserves;
    GuidSet _chokepointAllies;
    GuidSet _chokepointEnemies;
    std::vector<Escort> _escorts;
    std::vector<ArrivalCredit> _arrivalCredits;
    std::vector<Position> _phase2SaurfangRoute;
    ObjectGuid _warMachineGuid;
    ObjectGuid _trizzGuid;
    ObjectGuid _nathanosGuid;
    ObjectGuid _lorthemarGuid;
    ObjectGuid _baineGuid;
    ObjectGuid _sylvanasGuid;
    uint32 _waveNumber = 0;
    bool _waveActive = false;
    uint32 _waveStartTime = 0;
    uint8 _blightSceneStage = 0;
    bool _likeWildfireStarted = false;
    bool _voidTearOpen = false;
    uint32 _kingPieceProgress = 0;
};

// Undercity citizens in the heart of the city (Evacuate the Heart of the City).
// Registered here, next to the instance script that summons them.
// "The Undercity is under attack! Get to the mage portal!" sends them away.
class npc_lordaeron_undercity_citizen : public CreatureScript
{
public:
    npc_lordaeron_undercity_citizen() : CreatureScript("npc_lordaeron_undercity_citizen") { }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        ClearGossipMenuFor(player);

        // The option text comes straight from the client's BroadcastText, in the player's locale.
        std::string optionText = "The Undercity is under attack! Get to the mage portal!";
        if (BroadcastTextEntry const* text = sBroadcastTextStore.LookupEntry(BROADCAST_TEXT_GET_TO_THE_PORTAL))
            optionText = DB2Manager::GetBroadcastTextValue(text, player->GetSession()->GetSessionDbcLocale());
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, optionText, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF);

        SendGossipMenuFor(player, player->GetGossipTextId(GOSSIP_MENU_UNDERCITY_CITIZEN, creature), creature->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 /*sender*/, uint32 action) override
    {
        CloseGossipMenuFor(player);
        if (action != GOSSIP_ACTION_INFO_DEF)
            return false;

        if (InstanceScript* instance = creature->GetInstanceScript())
            instance->SetGuidData(DATA_EVACUATE_CITIZEN, creature->GetGUID());
        return true;
    }
};

// Sludge Guard Trizz (Our Secret Weapon): a gas mask, or a gas mask and a blight canister.
class npc_lordaeron_sludge_guard_trizz : public CreatureScript
{
public:
    npc_lordaeron_sludge_guard_trizz() : CreatureScript("npc_lordaeron_sludge_guard_trizz") { }

    static std::string BroadcastText(Player* player, uint32 id, char const* fallback)
    {
        if (BroadcastTextEntry const* text = sBroadcastTextStore.LookupEntry(id))
            return DB2Manager::GetBroadcastTextValue(text, player->GetSession()->GetSessionDbcLocale());
        return fallback;
    }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        ClearGossipMenuFor(player);
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, BroadcastText(player, BROADCAST_TEXT_GAS_MASK, "Just a gas mask for me, please."),
            GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 1);
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, BroadcastText(player, BROADCAST_TEXT_GAS_MASK_AND_BLIGHT,
            "Gas mask and blight canister, please. I want to see what this thing can do."), GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 2);
        SendGossipMenuFor(player, player->GetGossipTextId(GOSSIP_MENU_SLUDGE_GUARD_TRIZZ, creature), creature->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* /*creature*/, uint32 /*sender*/, uint32 action) override
    {
        CloseGossipMenuFor(player);
        player->CastSpell(player, SPELL_BLIGHT_RESISTANT_GAS_MASK, true);
        if (action == GOSSIP_ACTION_INFO_DEF + 2)
            player->CastSpell(player, SPELL_BLIGHT_CANISTER, true);
        return true;
    }
};

// 261748 - Blight Spray: every Alliance soldier caught in it counts for Our Secret Weapon.
class spell_lordaeron_blight_spray : public SpellScript
{
    PrepareSpellScript(spell_lordaeron_blight_spray);

    void HandleHit(SpellEffIndex /*effIndex*/)
    {
        Creature* target = GetHitCreature();
        if (!target)
            return;
        if (InstanceScript* instance = target->GetInstanceScript())
            instance->SetGuidData(DATA_BLIGHT_SPRAY_HIT, target->GetGUID());
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_lordaeron_blight_spray::HandleHit, EFFECT_1, SPELL_EFFECT_SCHOOL_DAMAGE);
    }
};

// 249366 - Bandaging... (spellclick of the wounded Horde soldiers in Our Secret Weapon)
class spell_lordaeron_bandaging : public SpellScript
{
    PrepareSpellScript(spell_lordaeron_bandaging);

    void HandleHit(SpellEffIndex /*effIndex*/)
    {
        Creature* target = GetHitCreature();
        if (!target || target->GetMapId() != MAP_LORDAERON_SCENARIO)
            return;
        if (InstanceScript* instance = target->GetInstanceScript())
            instance->SetGuidData(DATA_WOUNDED_RESCUED, target->GetGUID());
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_lordaeron_bandaging::HandleHit, EFFECT_0, SPELL_EFFECT_HEAL_PCT);
    }
};

// Tells the scenario when a player is done with (or skipped) its movies and scenes.
class lordaeron_player_script : public PlayerScript
{
public:
    lordaeron_player_script() : PlayerScript("lordaeron_player_script") { }

    static InstanceScript* Scenario(Player* player)
    {
        return player->GetMapId() == MAP_LORDAERON_SCENARIO ? player->GetInstanceScript() : nullptr;
    }

    void OnMovieComplete(Player* player, uint32 movieId) override
    {
        if (InstanceScript* instance = Scenario(player))
            instance->SetData(DATA_MOVIE_COMPLETE, movieId);
    }

    void OnSceneComplete(Player* player, uint32 sceneInstanceId) override
    {
        SceneEnded(player, sceneInstanceId);
    }

    void OnSceneCancel(Player* player, uint32 sceneInstanceId) override
    {
        SceneEnded(player, sceneInstanceId);
    }

    static void SceneEnded(Player* player, uint32 sceneInstanceId)
    {
        InstanceScript* instance = Scenario(player);
        if (!instance)
            return;
        if (SceneTemplate const* scene = player->GetSceneMgr().GetSceneTemplateFromInstanceId(sceneInstanceId))
            instance->SetData(DATA_SCENE_COMPLETE, scene->SceneId);
    }
};

// .lordaeron step <2-13>: test command, skips forward to a step of the Horde scenario.
// 2 Spies in Our Midst, 3 To the Heart!, 4 Evacuate the Heart of the City, 5 To Battle!,
// 6 Push Them Back, 7 Defend the Azerite War Machine, 8 Our Secret Weapon, 9 Like Wildfire,
// 10 Holding Them Back, 11 The King Piece, 12 Cry Havoc, 13 The Dark Lady
class lordaeron_commandscript : public CommandScript
{
public:
    lordaeron_commandscript() : CommandScript("lordaeron_commandscript") { }

    std::vector<ChatCommand> GetCommands() const override
    {
        static std::vector<ChatCommand> lordaeronCommandTable =
        {
            { "step", rbac::RBAC_PERM_COMMAND_DEBUG, false, &HandleStepCommand, "" },
        };
        static std::vector<ChatCommand> commandTable =
        {
            { "lordaeron", rbac::RBAC_PERM_COMMAND_DEBUG, false, nullptr, "", lordaeronCommandTable },
        };
        return commandTable;
    }

    static bool HandleStepCommand(ChatHandler* handler, char const* args)
    {
        Player* player = handler->GetSession()->GetPlayer();
        uint32 number = (args && *args) ? uint32(atoi(args)) : 0;
        if (number < 2 || number > 13)
        {
            handler->SendSysMessage("Usage: .lordaeron step <2-13>  (2 Spies, 3 To the Heart, 4 Evacuate, 5 To Battle, 6 Push Them Back, "
                "7 War Machine, 8 Secret Weapon, 9 Like Wildfire, 10 Holding Them Back, 11 King Piece, 12 Cry Havoc, 13 Dark Lady)");
            return true;
        }

        InstanceScript* instance = player->GetInstanceScript();
        if (player->GetMapId() != MAP_LORDAERON_SCENARIO || !instance)
        {
            handler->SendSysMessage("You are not in the Battle for Lordaeron scenario.");
            return true;
        }

        instance->SetData(DATA_JUMP_TO_STEP, number);
        handler->PSendSysMessage("Battle for Lordaeron: jumping to step %u (forward only).", number);
        return true;
    }
};

void AddSC_scenario_the_battle_for_lordaeron()
{
    new lordaeron_commandscript();
    new npc_lordaeron_undercity_citizen();
    new npc_lordaeron_sludge_guard_trizz();
    new lordaeron_player_script();
    RegisterSpellScript(spell_lordaeron_blight_spray);
    RegisterSpellScript(spell_lordaeron_bandaging);
    RegisterInstanceScript(scenario_the_battle_for_lordaeron, MAP_LORDAERON_SCENARIO);
}
