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
#include "Log.h"
#include "Player.h"

struct scenario_the_battle_for_lordaeron : public InstanceScript
{
    scenario_the_battle_for_lordaeron(InstanceMap* map) : InstanceScript(map) { }

    void OnPlayerEnter(Player* player) override
    {
        if (!player->GetScenario())
            return;

        // TODO: intro (Saurfang in the Undercity, step STEP_H_UNDER_SIEGE).
    }

    // Fires when a step's root criteria tree is completed, i.e. right before
    // the scenario moves to the next step. Use it to set up the next step.
    void OnCompletedCriteriaTree(CriteriaTree const* tree) override
    {
        switch (tree->ID)
        {
            case CRITERIA_TREE_H_UNDER_SIEGE:        // -> Spies in Our Midst
            case CRITERIA_TREE_H_SPIES_IN_OUR_MIDST: // -> To the Heart!
            case CRITERIA_TREE_H_TO_THE_HEART:       // -> Evacuate the Heart of the City
            case CRITERIA_TREE_H_EVACUATE_HEART:     // -> To Battle! (portal)
            case CRITERIA_TREE_H_TO_BATTLE:          // -> Push Them Back
            case CRITERIA_TREE_H_PUSH_THEM_BACK:     // -> Defend the Azerite War Machine
            case CRITERIA_TREE_H_DEFEND_WAR_MACHINE: // -> Our Secret Weapon
            case CRITERIA_TREE_H_OUR_SECRET_WEAPON:  // -> Like Wildfire
            case CRITERIA_TREE_H_LIKE_WILDFIRE:      // -> Holding Them Back
            case CRITERIA_TREE_H_HOLDING_THEM_BACK:  // -> The King Piece
            case CRITERIA_TREE_H_THE_KING_PIECE:     // -> Cry Havoc
            case CRITERIA_TREE_H_CRY_HAVOC:          // -> The Dark Lady
            case CRITERIA_TREE_H_THE_DARK_LADY:      // scenario complete
                TC_LOG_DEBUG("scripts", "Battle for Lordaeron: criteria tree %u completed", tree->ID);
                break;
            default:
                break;
        }
    }

    // Creature/GameObject/AreaTrigger scripts call instance->SetData(SCENARIO_EVENT_*, 1)
    // so the instance can react and progress the scenario in one place.
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
};

void AddSC_scenario_the_battle_for_lordaeron()
{
    RegisterInstanceScript(scenario_the_battle_for_lordaeron, MAP_LORDAERON_SCENARIO);
}
