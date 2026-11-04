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
#include "DBCEnums.h"
#include "InstanceScenario.h"
#include "Log.h"
#include "Map.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "TaskScheduler.h"
#include "TemporarySummon.h"

namespace
{
    // Map 1760 uses the same coordinates as map 0, so every Undercity position
    // below is taken from an existing map 0 Undercity spawn (known walkable ground).
    Position const PlayerStartPos       = { 1770.206665f, 63.413601f, -46.320221f, 2.450299f }; // Magic Quarter
    Position const SaurfangMagicPos     = { 1788.19f,  57.52f, -61.41f, 3.93f };
    Position const HeartOfCityPos       = { 1599.95f, 240.65f, -52.06f, 3.14f }; // bank ring
    Position const UndercityPortalPos   = { 1591.49f, 240.33f, -52.06f, 3.04f };
    // Above ground, next to Sylvanas' static spawn (1628.46, 240.08, 63.93).
    Position const WallsArrivalPos      = { 1645.00f, 240.00f,  62.60f, 0.00f };
    Position const FrontLinePos         = { 1830.00f, 238.00f,  62.50f, 0.00f };
    // Where the hostile (Alliance scenario) Saurfang stands; the Horde one starts here.
    Position const WallsSaurfangPos     = { 1744.64f, 238.92f,  62.13f, 6.26f };

    Position const ElevatorPositions[] =
    {
        { 1595.26f, 188.64f, -40.78f, 0.0f },
        { 1596.21f, 302.40f, -40.66f, 0.0f },
    };

    Position const MagicQuarterSpyPositions[] =
    {
        { 1766.35f,  65.66f, -46.24f, 5.39f },
        { 1775.00f,  51.00f, -61.41f, 6.21f },
        { 1775.98f,  46.13f, -61.41f, 0.89f },
        { 1776.40f,  20.25f, -47.31f, 1.94f },
        { 1776.60f,  79.05f, -59.25f, 2.29f },
        { 1778.19f,  65.48f, -46.24f, 2.79f },
        { 1780.31f,  44.04f, -61.41f, 1.33f },
        { 1791.48f,  53.41f, -61.41f, 2.95f },
        { 1813.74f,  56.41f, -47.30f, 3.05f },
    };

    // Upper ring of the city heart.
    Position const CitizenPositions[] =
    {
        { 1543.83f, 222.60f, -42.98f, 4.97f },
        { 1554.30f, 257.97f, -43.02f, 5.93f },
        { 1558.67f, 243.31f, -43.02f, 0.02f },
        { 1564.76f, 209.60f, -43.02f, 0.77f },
        { 1566.80f, 275.30f, -43.02f, 5.38f },
        { 1572.02f, 287.89f, -43.02f, 0.45f },
        { 1579.65f, 193.26f, -43.02f, 1.57f },
        { 1610.45f, 283.26f, -43.02f, 4.92f },
        { 1612.13f, 189.22f, -43.02f, 0.47f },
        { 1623.77f, 272.45f, -43.02f, 4.56f },
        { 1627.97f, 204.57f, -43.02f, 2.23f },
        { 1637.61f, 239.54f, -43.02f, 3.11f },
    };

    // Lower ring of the city heart.
    Position const HeartAttackerPositions[] =
    {
        { 1542.40f, 225.16f, -56.79f, 5.24f },
        { 1542.45f, 255.20f, -56.79f, 1.01f },
        { 1579.11f, 187.98f, -56.79f, 2.64f },
        { 1580.67f, 293.06f, -56.79f, 3.35f },
        { 1590.47f, 277.01f, -55.26f, 5.45f },
        { 1591.05f, 208.10f, -55.26f, 4.76f },
        { 1602.13f, 275.44f, -55.26f, 5.72f },
        { 1601.69f, 203.62f, -55.26f, 2.34f },
        { 1609.71f, 187.18f, -56.79f, 0.05f },
        { 1605.35f, 244.58f, -52.15f, 6.05f },
    };

    constexpr uint32 FACTION_MONSTER            = 14;
    constexpr float  REPORT_TO_SAURFANG_RANGE   = 8.0f;
    constexpr float  HEART_REACHED_RANGE        = 20.0f;
    constexpr float  SAURFANG_ARRIVED_RANGE     = 6.0f;
    constexpr float  EVACUATE_RANGE             = 5.0f;
    constexpr float  PORTAL_USE_RANGE           = 3.0f;
    constexpr float  FRONT_LINE_RANGE           = 15.0f;
}

struct scenario_the_battle_for_lordaeron : public InstanceScript
{
    scenario_the_battle_for_lordaeron(InstanceMap* map) : InstanceScript(map) { }

    // One respawning spawn point. Killing (or evacuating) its creature sends EventId.
    struct SpawnSlot
    {
        SpawnSlot(uint32 entry, Position const& pos, uint32 eventId, bool hostile)
            : Entry(entry), Pos(pos), EventId(eventId), Hostile(hostile) { }

        uint32 Entry;
        Position Pos;
        uint32 EventId;
        bool Hostile;
        ObjectGuid Guid;
    };

    void OnPlayerEnter(Player* player) override
    {
        InstanceScript::OnPlayerEnter(player);

        if (!IsHordeScenario() || GetCurrentStepId() != STEP_H_UNDER_SIEGE)
            return;

        // The Horde scenario starts inside the Undercity.
        ObjectGuid playerGuid = player->GetGUID();
        _scheduler.Schedule(1s, [this, playerGuid](TaskContext /*context*/)
        {
            if (Player* p = ObjectAccessor::GetPlayer(instance, playerGuid))
                p->NearTeleportTo(PlayerStartPos);
        });

        if (_started)
            return;

        _started = true;
        if (TempSummon* saurfang = instance->SummonCreature(NPC_HORDE_SAURFANG, SaurfangMagicPos))
            _undercitySaurfangGuid = saurfang->GetGUID();

        _scheduler.Schedule(1s, [this](TaskContext context)
        {
            UpdateProximityEvents();
            context.Repeat(1s);
        });
    }

    void OnCreatureCreate(Creature* creature) override
    {
        InstanceScript::OnCreatureCreate(creature);

        // The static Saurfang on map 1760 is the Alliance scenario boss and is hostile
        // to Horde players, so remove him from the Horde scenario.
        if (creature->GetEntry() == NPC_HIGH_OVERLORD_SAURFANG && IsHordeScenario())
        {
            ObjectGuid guid = creature->GetGUID();
            _scheduler.Schedule(1ms, [this, guid](TaskContext /*context*/)
            {
                if (Creature* hostileSaurfang = instance->GetCreature(guid))
                    hostileSaurfang->DespawnOrUnsummon();
            });
        }
    }

    void Update(uint32 diff) override
    {
        _scheduler.Update(diff);
    }

    // Fires when a step's root criteria tree is completed, i.e. right before
    // the scenario moves to the next step. Used to set up the next step.
    void OnCompletedCriteriaTree(CriteriaTree const* tree) override
    {
        switch (tree->ID)
        {
            case CRITERIA_TREE_H_UNDER_SIEGE:        // -> Spies in Our Midst
                StartSpiesInOurMidst();
                break;
            case CRITERIA_TREE_H_SPIES_IN_OUR_MIDST: // -> To the Heart!
                DespawnSlots(_spySlots);
                StartToTheHeart();
                break;
            case CRITERIA_TREE_H_TO_THE_HEART:       // -> Evacuate the Heart of the City
                StartEvacuateHeart();
                break;
            case CRITERIA_TREE_H_EVACUATE_HEART:     // -> To Battle! (portal)
                DespawnSlots(_citizenSlots);
                DespawnSlots(_heartAttackerSlots);
                StartToBattle();
                break;
            case CRITERIA_TREE_H_TO_BATTLE:          // -> Push Them Back
                if (Creature* portal = instance->GetCreature(_portalGuid))
                    portal->DespawnOrUnsummon();
                break;
            // TODO: steps from Push Them Back onwards (above ground).
            default:
                break;
        }
    }

    void OnUnitDeath(Unit* unit) override
    {
        Creature* creature = unit->ToCreature();
        if (!creature)
            return;

        for (std::vector<SpawnSlot>* slots : { &_spySlots, &_heartAttackerSlots })
        {
            for (size_t i = 0; i < slots->size(); ++i)
            {
                SpawnSlot& slot = (*slots)[i];
                if (slot.Guid != creature->GetGUID())
                    continue;

                slot.Guid.Clear();
                DoSendScenarioEvent(slot.EventId);
                ScheduleRespawn(*slots, i, 15s);
                return;
            }
        }
    }

    // Creature/GameObject/AreaTrigger scripts can call instance->SetData(SCENARIO_EVENT_*, 1)
    // to progress the scenario from one place.
    void SetData(uint32 type, uint32 /*value*/) override
    {
        switch (type)
        {
            case SCENARIO_EVENT_H_REPORT_TO_SAURFANG:
            case SCENARIO_EVENT_H_MAGE_DISTRICT_A:
            case SCENARIO_EVENT_H_MAGE_DISTRICT_B:
            case SCENARIO_EVENT_H_FOLLOW_SAURFANG_HEART:
            case SCENARIO_EVENT_H_HEART_REACHED:
            case SCENARIO_EVENT_H_EVACUATE_CITIZEN:
            case SCENARIO_EVENT_H_EVACUATE_B:
            case SCENARIO_EVENT_H_FOLLOW_SAURFANG_BATTLE:
            case SCENARIO_EVENT_H_ALLIANCE_DEFEATED:
            case SCENARIO_EVENT_H_WAVE_DEFEATED:
            case SCENARIO_EVENT_H_WAR_MACHINE_DEFENDED:
            case SCENARIO_EVENT_H_ALLIANCE_DRIVEN_BACK:
            case SCENARIO_EVENT_H_BLIGHT_USED:
            case SCENARIO_EVENT_H_FOLLOW_NATHANOS_KEEP:
            case SCENARIO_EVENT_H_INTERCEPT_ALLIANCE:
            case SCENARIO_EVENT_H_DEFEAT_ANDUIN:
            case SCENARIO_EVENT_H_DEFEAT_ANDUIN_ALLY:
            case SCENARIO_EVENT_H_DEFEAT_ANDUIN_ALLY_2:
            case SCENARIO_EVENT_H_FOLLOW_NATHANOS_SAFETY:
            case SCENARIO_EVENT_H_FOLLOW_SYLVANAS:
                DoSendScenarioEvent(type);
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

    // ---- Step setup ----------------------------------------------------

    // Spies in Our Midst: 120 points. SI:7 Operatives give 4, Gilnean Maulers give 3.
    // Slots respawn while the step is active, so the exact count does not matter.
    void StartSpiesInOurMidst()
    {
        _spySlots.clear();
        uint32 index = 0;
        for (Position const& pos : MagicQuarterSpyPositions)
        {
            bool operative = (index++ % 2) == 0;
            _spySlots.emplace_back(operative ? uint32(NPC_SI7_OPERATIVE) : uint32(NPC_GILNEAN_MAULER), pos,
                operative ? uint32(SCENARIO_EVENT_H_MAGE_DISTRICT_A) : uint32(SCENARIO_EVENT_H_MAGE_DISTRICT_B), true);
        }
        SpawnSlots(_spySlots);
    }

    // To the Heart!: Saurfang walks to the bank ring; the player has to get there too.
    void StartToTheHeart()
    {
        _saurfangArrivedAtHeart = false;
        _playerReachedHeart = false;

        Creature* saurfang = instance->GetCreature(_undercitySaurfangGuid);
        if (!saurfang)
            return;

        saurfang->SetWalk(false);
        saurfang->GetMotionMaster()->MovePoint(0, HeartOfCityPos);

        // Fallback if pathing fails (e.g. no mmaps for map 1760).
        ObjectGuid guid = _undercitySaurfangGuid;
        _scheduler.Schedule(90s, [this, guid](TaskContext /*context*/)
        {
            if (GetCurrentStepId() != STEP_H_TO_THE_HEART)
                return;
            if (Creature* s = instance->GetCreature(guid))
                if (s->GetDistance(HeartOfCityPos) > SAURFANG_ARRIVED_RANGE)
                    s->NearTeleportTo(HeartOfCityPos);
        });
    }

    // Evacuate the Heart of the City: 110 points. Each evacuated citizen gives 2,
    // each Alliance attacker killed gives 3.
    void StartEvacuateHeart()
    {
        _citizenSlots.clear();
        for (Position const& pos : CitizenPositions)
            _citizenSlots.emplace_back(NPC_FORSAKEN_REFUGEE, pos, SCENARIO_EVENT_H_EVACUATE_CITIZEN, false);
        SpawnSlots(_citizenSlots);

        _heartAttackerSlots.clear();
        for (Position const& pos : HeartAttackerPositions)
            _heartAttackerSlots.emplace_back(NPC_STORMWIND_FOOTMAN, pos, SCENARIO_EVENT_H_EVACUATE_B, true);
        SpawnSlots(_heartAttackerSlots);
    }

    // To Battle!: a portal in the heart takes the player up to Sylvanas, then
    // Saurfang leads the way to the front line.
    void StartToBattle()
    {
        _reachedFrontLine = false;

        if (TempSummon* portal = instance->SummonCreature(NPC_UNDERCITY_PORTAL, UndercityPortalPos))
        {
            _portalGuid = portal->GetGUID();
            portal->SetUnitFlags(UnitFlags(UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_IMMUNE_TO_PC | UNIT_FLAG_IMMUNE_TO_NPC));
        }

        if (Creature* saurfang = instance->GetCreature(_undercitySaurfangGuid))
            saurfang->DespawnOrUnsummon();

        if (TempSummon* saurfang = instance->SummonCreature(NPC_HORDE_SAURFANG, WallsSaurfangPos))
        {
            _wallsSaurfangGuid = saurfang->GetGUID();
            saurfang->SetWalk(false);
            saurfang->GetMotionMaster()->MovePoint(0, FrontLinePos);
            saurfang->SetHomePosition(FrontLinePos);
        }
    }

    // ---- Per-second checks -----------------------------------------------

    void UpdateProximityEvents()
    {
        uint32 stepId = GetCurrentStepId();

        for (auto& ref : instance->GetPlayers())
        {
            Player* player = ref.GetSource();
            if (!player || !player->IsAlive())
                continue;

            switch (stepId)
            {
                case STEP_H_UNDER_SIEGE:
                    if (Creature* saurfang = instance->GetCreature(_undercitySaurfangGuid))
                        if (player->IsWithinDist(saurfang, REPORT_TO_SAURFANG_RANGE))
                            DoSendScenarioEvent(SCENARIO_EVENT_H_REPORT_TO_SAURFANG);
                    break;
                case STEP_H_TO_THE_HEART:
                    if (!_playerReachedHeart && player->GetDistance(HeartOfCityPos) <= HEART_REACHED_RANGE)
                    {
                        _playerReachedHeart = true;
                        DoSendScenarioEvent(SCENARIO_EVENT_H_HEART_REACHED);
                    }
                    break;
                case STEP_H_EVACUATE_HEART:
                    EvacuateCitizensNear(player);
                    break;
                case STEP_H_TO_BATTLE:
                    if (Creature* portal = instance->GetCreature(_portalGuid))
                    {
                        if (player->IsWithinDist(portal, PORTAL_USE_RANGE))
                        {
                            // "Use the Portal" is a CRITERIA_TYPE_SEND_EVENT criteria, not a scenario event.
                            player->UpdateCriteria(CRITERIA_TYPE_SEND_EVENT, GAME_EVENT_H_USE_PORTAL);
                            player->NearTeleportTo(WallsArrivalPos);
                        }
                    }
                    if (!_reachedFrontLine && player->GetDistance(FrontLinePos) <= FRONT_LINE_RANGE)
                    {
                        _reachedFrontLine = true;
                        DoSendScenarioEvent(SCENARIO_EVENT_H_FOLLOW_SAURFANG_BATTLE);
                    }
                    break;
                default:
                    break;
            }
        }

        if (stepId == STEP_H_TO_THE_HEART && !_saurfangArrivedAtHeart)
        {
            if (Creature* saurfang = instance->GetCreature(_undercitySaurfangGuid))
            {
                if (saurfang->GetDistance(HeartOfCityPos) <= SAURFANG_ARRIVED_RANGE)
                {
                    _saurfangArrivedAtHeart = true;
                    saurfang->SetHomePosition(HeartOfCityPos);
                    DoSendScenarioEvent(SCENARIO_EVENT_H_FOLLOW_SAURFANG_HEART);
                }
            }
        }
    }

    // A citizen next to the player runs to the nearest elevator and is counted as evacuated.
    void EvacuateCitizensNear(Player* player)
    {
        for (size_t i = 0; i < _citizenSlots.size(); ++i)
        {
            SpawnSlot& slot = _citizenSlots[i];
            Creature* citizen = instance->GetCreature(slot.Guid);
            if (!citizen || !player->IsWithinDist(citizen, EVACUATE_RANGE))
                continue;

            slot.Guid.Clear();
            DoSendScenarioEvent(slot.EventId);

            Position const& elevator = citizen->GetDistance(ElevatorPositions[0]) < citizen->GetDistance(ElevatorPositions[1])
                ? ElevatorPositions[0] : ElevatorPositions[1];
            citizen->SetWalk(false);
            citizen->GetMotionMaster()->MovePoint(0, elevator);
            citizen->DespawnOrUnsummon(6s);

            ScheduleRespawn(_citizenSlots, i, 30s);
        }
    }

    // ---- Spawn slot helpers ----------------------------------------------

    void SpawnSlot_(SpawnSlot& slot)
    {
        if (TempSummon* summon = instance->SummonCreature(slot.Entry, slot.Pos))
        {
            slot.Guid = summon->GetGUID();
            if (slot.Hostile)
                summon->SetFaction(FACTION_MONSTER);
        }
    }

    void SpawnSlots(std::vector<SpawnSlot>& slots)
    {
        for (SpawnSlot& slot : slots)
            SpawnSlot_(slot);
    }

    void DespawnSlots(std::vector<SpawnSlot>& slots)
    {
        for (SpawnSlot& slot : slots)
            if (Creature* creature = instance->GetCreature(slot.Guid))
                creature->DespawnOrUnsummon();
        slots.clear();
    }

    // Respawns slot `index` after `delay`, as long as the same step is still running.
    void ScheduleRespawn(std::vector<SpawnSlot>& slots, size_t index, Milliseconds delay)
    {
        uint32 stepId = GetCurrentStepId();
        std::vector<SpawnSlot>* slotsPtr = &slots;
        _scheduler.Schedule(delay, [this, slotsPtr, index, stepId](TaskContext /*context*/)
        {
            if (GetCurrentStepId() != stepId || index >= slotsPtr->size())
                return;
            SpawnSlot& slot = (*slotsPtr)[index];
            if (slot.Guid.IsEmpty())
                SpawnSlot_(slot);
        });
    }

    TaskScheduler _scheduler;
    bool _started = false;
    bool _playerReachedHeart = false;
    bool _saurfangArrivedAtHeart = false;
    bool _reachedFrontLine = false;

    ObjectGuid _undercitySaurfangGuid;
    ObjectGuid _wallsSaurfangGuid;
    ObjectGuid _portalGuid;

    std::vector<SpawnSlot> _spySlots;
    std::vector<SpawnSlot> _heartAttackerSlots;
    std::vector<SpawnSlot> _citizenSlots;
};

void AddSC_scenario_the_battle_for_lordaeron()
{
    RegisterInstanceScript(scenario_the_battle_for_lordaeron, MAP_LORDAERON_SCENARIO);
}
