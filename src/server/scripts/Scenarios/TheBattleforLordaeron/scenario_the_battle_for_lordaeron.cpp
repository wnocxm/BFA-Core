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
#include "ScriptedGossip.h"
#include "ScriptMgr.h"
#include "TaskScheduler.h"
#include "TemporarySummon.h"
#include "WorldSession.h"

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
};

enum class SaurfangRouteEnd : uint8
{
    SpiesWatch,
    Ambush,
    Gate,
    Heart,
    FrontLine,
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
        return IsHordeScenario() && allianceViewSpawn ? 0 : data->id;
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
    }

    void SetData(uint32 type, uint32 data) override
    {
        if (type == DATA_JUMP_TO_STEP)
            JumpToStep(data);
    }

    void SetGuidData(uint32 type, ObjectGuid data) override
    {
        if (type == DATA_EVACUATE_CITIZEN)
        {
            if (Creature* citizen = instance->GetCreature(data))
                EvacuateCitizen(citizen);
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
        for (GuidSet const* enemies : { &_spies, &_druids, &_heartEnemies })
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

    // ---- Test helper: .lordaeron step <2-5> -----------------------------------------
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
        static uint32 const stepIds[] = { STEP_H_UNDER_SIEGE, STEP_H_SPIES_IN_OUR_MIDST, STEP_H_TO_THE_HEART, STEP_H_EVACUATE_HEART, STEP_H_TO_BATTLE };
        if (number < 2 || number > 5)
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

// .lordaeron step <2-5>: test command, skips forward to a step of the Horde scenario.
// 2 Spies in Our Midst, 3 To the Heart!, 4 Evacuate the Heart of the City, 5 To Battle!
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
        if (number < 2 || number > 5)
        {
            handler->SendSysMessage("Usage: .lordaeron step <2-5>  (2 Spies, 3 To the Heart, 4 Evacuate, 5 To Battle)");
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
    RegisterInstanceScript(scenario_the_battle_for_lordaeron, MAP_LORDAERON_SCENARIO);
}
