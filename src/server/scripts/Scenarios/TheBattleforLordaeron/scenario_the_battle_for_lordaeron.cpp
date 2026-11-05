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
#include "DBCEnums.h"
#include "GameObject.h"
#include "InstanceScenario.h"
#include "Log.h"
#include "Map.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "PhasingHandler.h"
#include "Player.h"
#include "TaskScheduler.h"
#include "TemporarySummon.h"

using namespace BattleForLordaeron;

namespace
{
    constexpr float REPORT_TO_SAURFANG_RANGE    = 15.0f;
    constexpr float ANASTASIA_GREET_RANGE       = 30.0f;
    constexpr float RESCUE_RANGE                = 10.0f;
    constexpr float RESCUE_SPY_CLEAR_RANGE      = 10.0f;
    constexpr float EVACUATE_RANGE              = 10.0f;
    constexpr float COURTYARD_ARRIVAL_RANGE     = 25.0f;
    constexpr float FRONT_LINE_RANGE            = 15.0f;
    constexpr float START_TELEPORT_RANGE        = 40.0f;
    constexpr uint32 FACTION_SCENARIO_ENEMY     = 2502;
    constexpr uint32 FIRST_SCENARIO_CREATURE_ENTRY = 130000; // lower entries are map 0 Undercity residents

    constexpr Milliseconds SPIES_VANISH_DELAY   = 11533ms; // "<whistles>" line of CONVERSATION_SPIES_CORNERED
    constexpr Milliseconds REINFORCEMENTS_DELAY = 10s;
    constexpr Milliseconds SAURFANG_BRIEFING_LENGTH = 38809ms;
    constexpr Milliseconds UNDERCITY_EVACUATED_LENGTH = 35675ms;

    // Undercity elevators, where evacuated citizens head before leaving.
    Position const HeartExits[] =
    {
        { 1544.24f, 240.77f, -40.78f, 0.0f },
        { 1595.26f, 188.64f, -40.78f, 0.0f },
        { 1596.21f, 302.40f, -40.66f, 0.0f },
    };
}

struct scenario_the_battle_for_lordaeron : public InstanceScript
{
    scenario_the_battle_for_lordaeron(InstanceMap* map) : InstanceScript(map) { }

    // The static spawns of map 1760 show the battle from the Alliance scenario's point of
    // view. The Horde scenario summons its own creatures step by step instead.
    uint32 GetCreatureEntry(ObjectGuid::LowType /*spawnId*/, CreatureData const* data) override
    {
        return IsHordeScenario() ? 0 : data->id;
    }

    void OnPlayerEnter(Player* player) override
    {
        InstanceScript::OnPlayerEnter(player);

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

        if (GetCurrentStepId() == STEP_H_EVACUATE_HEART)
            player->CastSpell(player, SPELL_EVACUATE_OVERRIDE, true);

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

        if (Creature* saurfang = Summon(NPC_SAURFANG, SaurfangMagicQuarter))
            _saurfangGuid = saurfang->GetGUID();

        _scheduler.Schedule(1s, [this](TaskContext context)
        {
            UpdatePlayerChecks();
            context.Repeat(1s);
        });
    }

    void OnGameObjectCreate(GameObject* go) override
    {
        InstanceScript::OnGameObjectCreate(go);

        if (go->GetSpawnId() == GO_SPAWN_MAGIC_QUARTER_PORTCULLIS)
            _magicQuarterPortcullisGuid = go->GetGUID();
    }

    void OnPlayerExit(Player* player) override
    {
        InstanceScript::OnPlayerExit(player);
        player->RemoveAurasDueToSpell(SPELL_EVACUATE_OVERRIDE);
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
            if (Creature* saurfang = GetSaurfang())
                saurfang->AI()->DoAction(ACTION_SAURFANG_GO_TO_GATE);
        }
    }

    void SetData(uint32 type, uint32 value) override
    {
        if (type != DATA_SAURFANG_REACHED)
            return;

        switch (value)
        {
            case POINT_SAURFANG_AMBUSH:
                OnSaurfangReachedAmbush();
                break;
            case POINT_SAURFANG_GATE:
                StartConversation(CONVERSATION_OPEN_THE_GATE, GetSaurfang());
                DoSendScenarioEvent(SCENARIO_EVENT_H_FOLLOW_SAURFANG_HEART);
                break;
            default:
                break;
        }
    }

    void SetGuidData(uint32 type, ObjectGuid data) override
    {
        Player* player = ObjectAccessor::GetPlayer(instance, data);
        if (!player)
            return;

        switch (type)
        {
            case DATA_EVACUATE_NEAR:
                EvacuateCitizensNear(player);
                break;
            case DATA_PLAYER_USED_KEEP_PORTAL:
                // "Use the Portal" is a CRITERIA_TYPE_SEND_EVENT criteria.
                player->UpdateCriteria(CRITERIA_TYPE_SEND_EVENT, GAME_EVENT_H_USE_PORTAL);
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
    }

    void SummonCaptives()
    {
        _captives = SummonGroup(SUMMON_GROUP_CAPTIVES);
        for (ObjectGuid const& guid : _captives)
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
        if (Creature* saurfang = GetSaurfang())
            saurfang->AI()->DoAction(ACTION_SAURFANG_GO_TO_AMBUSH);
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

    // Evacuate the Heart of the City: the gate opens, citizens are sent away with the
    // "Evacuate!" ability and the infiltrators are cut down.
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

        if (Creature* saurfang = GetSaurfang())
            saurfang->AI()->DoAction(ACTION_SAURFANG_GO_TO_HEART);

        DoCastSpellOnPlayers(SPELL_EVACUATE_OVERRIDE);
    }

    void SummonHeartCitizens()
    {
        _heartCitizens = SummonGroup(SUMMON_GROUP_HEART_CITIZENS);
        for (ObjectGuid const& guid : _heartCitizens)
            if (Creature* citizen = instance->GetCreature(guid))
                MakeCivilian(citizen);
    }

    void EvacuateCitizensNear(Player* player)
    {
        if (GetCurrentStepId() != STEP_H_EVACUATE_HEART)
            return;

        for (auto itr = _heartCitizens.begin(); itr != _heartCitizens.end();)
        {
            Creature* citizen = instance->GetCreature(*itr);
            if (!citizen || !player->IsWithinDist(citizen, EVACUATE_RANGE))
            {
                ++itr;
                continue;
            }

            itr = _heartCitizens.erase(itr);
            DoSendScenarioEvent(SCENARIO_EVENT_H_CITIZEN_EVACUATED);
            LeaveTowards(citizen, *std::min_element(std::begin(HeartExits), std::end(HeartExits), [citizen](Position const& a, Position const& b)
            {
                return citizen->GetExactDistSq(a) < citizen->GetExactDistSq(b);
            }));
        }
    }

    // To Battle!: Anastasia opens a portal to the keep, where Sylvanas waits.
    void StartToBattle()
    {
        DoRemoveAurasDueToSpellOnPlayers(SPELL_EVACUATE_OVERRIDE);
        DespawnAll(_heartEnemies);

        Creature* saurfang = GetSaurfang();
        StartConversation(CONVERSATION_PORTAL_TO_THE_KEEP, saurfang);

        if (Creature* anastasia = Summon(NPC_ANASTASIA_HARTWELL, AnastasiaHeart))
            MakeCivilian(anastasia);

        _scheduler.Schedule(5s, [this](TaskContext /*context*/)
        {
            if (Creature* portal = Summon(NPC_PORTAL_TO_THE_KEEP, KeepPortal))
            {
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

        Creature* saurfang = Summon(NPC_SAURFANG, SaurfangCourtyard);
        if (!saurfang)
            return;

        _saurfangGuid = saurfang->GetGUID();

        Creature* sylvanas = nullptr;
        for (ObjectGuid const& guid : _courtyard)
            if (Creature* creature = instance->GetCreature(guid))
                if (creature->GetEntry() == NPC_SYLVANAS)
                    sylvanas = creature;

        StartConversation(CONVERSATION_UNDERCITY_EVACUATED, sylvanas ? sylvanas : saurfang);

        _scheduler.Schedule(UNDERCITY_EVACUATED_LENGTH, [this](TaskContext /*context*/)
        {
            if (Creature* s = GetSaurfang())
                s->AI()->DoAction(ACTION_SAURFANG_GO_TO_FRONT_LINE);
        });
    }

    // ---- Per-second checks -----------------------------------------------------------

    // Steps are started from here rather than from OnCompletedCriteriaTree, so they also
    // start when a GM skips a step (.instance complete_scenario_step).
    void OnStepChanged(uint32 stepId)
    {
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

    // Weighted steps never run dry: once a kind of target is used up while the step is
    // still running, a new group of it arrives.
    void CheckReinforcements(uint32 stepId)
    {
        if (stepId == STEP_H_SPIES_IN_OUR_MIDST)
        {
            PruneGone(_spies);
            PruneGone(_captives);
            if (_spies.empty())
                ScheduleReinforcements(stepId, SUMMON_GROUP_SPIES);
            if (_captives.empty())
                ScheduleReinforcements(stepId, SUMMON_GROUP_CAPTIVES);
        }
        else if (stepId == STEP_H_EVACUATE_HEART)
        {
            PruneGone(_heartEnemies);
            PruneGone(_heartCitizens);
            if (_heartEnemies.empty())
                ScheduleReinforcements(stepId, SUMMON_GROUP_HEART_ENEMIES);
            if (_heartCitizens.empty())
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

            switch (groupId)
            {
                case SUMMON_GROUP_SPIES:          _spies = SummonGroup(groupId); break;
                case SUMMON_GROUP_CAPTIVES:       SummonCaptives(); break;
                case SUMMON_GROUP_HEART_ENEMIES:  _heartEnemies = SummonGroup(groupId); break;
                case SUMMON_GROUP_HEART_CITIZENS: SummonHeartCitizens(); break;
                default: break;
            }
        });
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
            if (_keepPortalOpen && !_courtyardReached && player->GetDistance(CourtyardArrival) <= COURTYARD_ARRIVAL_RANGE)
            {
                _courtyardReached = true;
                OnFirstPlayerInCourtyard();
            }
            if (_courtyardReached && !_frontLineReached && player->GetDistance(SaurfangFrontLine) <= FRONT_LINE_RANGE)
            {
                _frontLineReached = true;
                DoSendScenarioEvent(SCENARIO_EVENT_H_FOLLOW_SAURFANG_BATTLE);
            }
        }
    }

    // Under Siege: Saurfang briefs the players; when he is done the portcullis to the
    // canals opens and the step is reported.
    void StartBriefing(Creature* saurfang)
    {
        _briefingStarted = true;
        StartConversation(CONVERSATION_SAURFANG_BRIEFING, saurfang);

        _scheduler.Schedule(SAURFANG_BRIEFING_LENGTH, [this](TaskContext /*context*/)
        {
            if (GameObject* portcullis = instance->GetGameObject(_magicQuarterPortcullisGuid))
                portcullis->SetGoState(GO_STATE_ACTIVE);

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
    bool _frontLineReached = false;

    ObjectGuid _saurfangGuid;
    ObjectGuid _magicQuarterPortcullisGuid;
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

void AddSC_scenario_the_battle_for_lordaeron()
{
    RegisterInstanceScript(scenario_the_battle_for_lordaeron, MAP_LORDAERON_SCENARIO);
}
