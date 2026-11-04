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

#ifndef DEF_THE_BATTLE_FOR_LORDAERON_H
#define DEF_THE_BATTLE_FOR_LORDAERON_H

#include "ScriptMgr.h"
#include "Creature.h"
#include "GameObject.h"
#include "InstanceScript.h"
#include "Scenario.h"

// Map.db2 1760 "LordaeronScenario" is shared by both factions.
// Scenario.db2: 1371 "The Siege of Lordaeron" (Alliance), 1482 "The Fall of Lordaeron" (Horde).
enum BattleForLordaeronMisc
{
    MAP_LORDAERON_SCENARIO          = 1760,
    SCENARIO_SIEGE_OF_LORDAERON     = 1371, // Alliance
    SCENARIO_FALL_OF_LORDAERON      = 1482, // Horde
};

// ScenarioStep.db2 entries for scenario 1482 (Horde), in order.
enum HordeScenarioSteps
{
    STEP_H_UNDER_SIEGE              = 3628, // Find Saurfang.
    STEP_H_SPIES_IN_OUR_MIDST       = 3654, // Secure the Mage District.
    STEP_H_TO_THE_HEART             = 3660, // Follow Saurfang to the Heart of the City
    STEP_H_EVACUATE_HEART           = 3658, // Evacuate the Remainder of Undercity
    STEP_H_TO_BATTLE                = 3661, // Use the Portal to meet with Sylvanas Windrunner.
    STEP_H_PUSH_THEM_BACK           = 3677, // Break their front line
    STEP_H_DEFEND_WAR_MACHINE       = 3704, // Defend the Azerite War Machine
    STEP_H_OUR_SECRET_WEAPON        = 3678, // Use blight on Alliance forces to drive them away.
    STEP_H_LIKE_WILDFIRE            = 3679, // Meet with Nathanos Blightcaller
    STEP_H_HOLDING_THEM_BACK        = 3680, // Intercept the Alliance with Nathanos Blightcaller.
    STEP_H_THE_KING_PIECE           = 3681, // Defeat King Anduin Wrynn and his Allies.
    STEP_H_CRY_HAVOC                = 3687, // Sylvanas has called for the retreat.
    STEP_H_THE_DARK_LADY            = 3688, // Follow Sylvanas.
};

// CriteriaTree.db2 root trees of each Horde step (handy for OnCompletedCriteriaTree).
enum HordeCriteriaTrees
{
    CRITERIA_TREE_H_UNDER_SIEGE         = 62528,
    CRITERIA_TREE_H_SPIES_IN_OUR_MIDST  = 62680, // weighted sum, 120 needed
    CRITERIA_TREE_H_TO_THE_HEART        = 62813,
    CRITERIA_TREE_H_EVACUATE_HEART      = 62810, // weighted sum, 110 needed
    CRITERIA_TREE_H_TO_BATTLE           = 62872,
    CRITERIA_TREE_H_PUSH_THEM_BACK      = 63971, // weighted sum, 100 needed
    CRITERIA_TREE_H_DEFEND_WAR_MACHINE  = 64876,
    CRITERIA_TREE_H_OUR_SECRET_WEAPON   = 63974, // weighted sum, 250 needed
    CRITERIA_TREE_H_LIKE_WILDFIRE       = 63976,
    CRITERIA_TREE_H_HOLDING_THEM_BACK   = 64018,
    CRITERIA_TREE_H_THE_KING_PIECE      = 64020, // weighted sum, 150 needed
    CRITERIA_TREE_H_CRY_HAVOC           = 64022,
    CRITERIA_TREE_H_THE_DARK_LADY       = 64024,
};

// Criteria.db2 assets. Each one is progressed with DoSendScenarioEvent(id).
// Weight = how much one event adds to a weighted-sum step.
enum HordeScenarioEvents
{
    // Under Siege
    SCENARIO_EVENT_H_REPORT_TO_SAURFANG     = 61422,
    // Spies in Our Midst (120)
    SCENARIO_EVENT_H_MAGE_DISTRICT_A        = 61487, // weight 4
    SCENARIO_EVENT_H_MAGE_DISTRICT_B        = 61488, // weight 3
    // To the Heart! (both required)
    SCENARIO_EVENT_H_FOLLOW_SAURFANG_HEART  = 62155,
    SCENARIO_EVENT_H_HEART_REACHED          = 61527,
    // Evacuate the Heart of the City (110)
    SCENARIO_EVENT_H_EVACUATE_CITIZEN       = 61525, // weight 2
    SCENARIO_EVENT_H_EVACUATE_B             = 61526, // weight 3
    // To Battle!
    GAME_EVENT_H_USE_PORTAL                 = 61568, // CRITERIA_TYPE_SEND_EVENT (spell effect SEND_EVENT), not a scenario event
    SCENARIO_EVENT_H_FOLLOW_SAURFANG_BATTLE = 62174,
    // Push Them Back (100)
    SCENARIO_EVENT_H_ALLIANCE_DEFEATED      = 62175, // weight 2
    // Defend the Azerite War Machine (both required)
    SCENARIO_EVENT_H_WAVE_DEFEATED          = 62289, // x3
    SCENARIO_EVENT_H_WAR_MACHINE_DEFENDED   = 62350,
    // Our Secret Weapon (250)
    SCENARIO_EVENT_H_ALLIANCE_DRIVEN_BACK   = 62177, // weight 2
    SCENARIO_EVENT_H_BLIGHT_USED            = 59597, // weight 2
    // Like Wildfire
    SCENARIO_EVENT_H_FOLLOW_NATHANOS_KEEP   = 62178,
    // Holding Them Back
    SCENARIO_EVENT_H_INTERCEPT_ALLIANCE     = 62202,
    // The King Piece (150)
    SCENARIO_EVENT_H_DEFEAT_ANDUIN          = 62482, // weight 1
    SCENARIO_EVENT_H_DEFEAT_ANDUIN_ALLY     = 62483, // weight 3
    SCENARIO_EVENT_H_DEFEAT_ANDUIN_ALLY_2   = 64503, // weight 10
    // Cry Havoc
    SCENARIO_EVENT_H_FOLLOW_NATHANOS_SAFETY = 62203,
    // The Dark Lady
    SCENARIO_EVENT_H_FOLLOW_SYLVANAS        = 62204,
};

enum BattleForLordaeronQuests
{
    QUEST_THE_BATTLE_FOR_LORDAERON_H    = 51796, // Horde, next quest 53028
    QUEST_THE_BATTLE_FOR_LORDAERON_A    = 51795, // Alliance, next quest 52946
};

// LFGDungeons.db2 (map 1760, difficulty 12)
enum BattleForLordaeronLfg
{
    LFG_DUNGEON_BATTLE_FOR_LORDAERON_A  = 1758,
    LFG_DUNGEON_BATTLE_FOR_LORDAERON_H  = 1759,
};

// Entries spawned on map 1760 in the world DB (bfa_world-17.06.2024).
enum BattleForLordaeronNpcs
{
    NPC_HIGH_OVERLORD_SAURFANG          = 129179, // Alliance scenario enemy (hostile), despawned for Horde
    NPC_HORDE_SAURFANG                  = 124798, // friendly Horde version (faction 83)
    NPC_ISABELLA_ORGRIMMAR              = 139093, // Orgrimmar mage, queues the scenario
    NPC_LADY_SYLVANAS_WINDRUNNER        = 124651,
    NPC_NATHANOS_BLIGHTCALLER           = 136394,
    NPC_LORTHEMAR_THERON                = 137939,
    NPC_ANDUIN_WRYNN                    = 126309,
    NPC_GENN_GREYMANE                   = 125496,
    NPC_JAINA_PROUDMOORE                = 124787,
    NPC_ALLERIA_WINDRUNNER              = 125138,
    NPC_AZERITE_WAR_MACHINE             = 132079,
    NPC_FORSAKEN_BLIGHT_THROWER         = 127147,
    NPC_FORSAKEN_APOTHECARY             = 126780,
    NPC_ROYAL_DREADGUARD                = 125196,
    NPC_DARK_RANGER                     = 125097,
    NPC_ORGRIMMAR_RAIDER                = 124480,
    NPC_TAUREN_EARTHSHAKER              = 125231,
    NPC_PLAGUE_GAS                      = 124565,
    NPC_PLAGUE_WAGON                    = 126786,

    // Undercity part (Horde steps 1-5), summoned by the instance script
    NPC_SI7_OPERATIVE                   = 131957,
    NPC_GILNEAN_MAULER                  = 138714,
    NPC_STORMWIND_FOOTMAN               = 124979,
    NPC_FORSAKEN_REFUGEE                = 141119,
    NPC_UNDERCITY_PORTAL                = 126680, // generic "Portal"
};

enum BattleForLordaeronGameObjects
{
    GOB_PLAGUE_CISTERN                  = 281556,
    GOB_FRONT_GATE                      = 279041,
};

// SpellName.db2
enum BattleForLordaeronSpells
{
    SPELL_TELEPORT_TO_LORDAERON         = 259094,
    SPELL_LEAVE_FOR_LORDAERON_QUEUE     = 269807,
    SPELL_PORTAL_SIEGE_OF_LORDAERON     = 269811,
    SPELL_LEAVE_FOR_LORDAERON           = 272129,
    SPELL_BLIGHT_GAS                    = 247450,
    SPELL_SUMMON_NATHANOS               = 273822,
    SPELL_PING_LORDAERON_SCENARIO_RP    = 276832,
};

// scene_template SceneId (SceneScriptPackage.db2 name in comment)
enum BattleForLordaeronScenes
{
    SCENE_AZERITE_MACHINE_ARRIVES       = 3022, // 8.0 Lordaeron Scenario - Azerite Machine Arrives
    SCENE_SAURFANG_LAST_STAND           = 1945, // 8.0 [SLS] Saurfang's Last Stand - IGC
    SCENE_SYLVANAS_BLOCKS_ALLIANCE      = 1845, // 8.0 [SBA] Sylvanas Blocks Alliance (Exploding Cisterns)
    SCENE_SYLVANAS_DUMPS_BLIGHT         = 1785, // 8.0 [SDB] Sylvanas Dumps Blight (also 3026)
    SCENE_LORDAERON_ARMY_UNITS          = 3091, // 8.0 [BVID] Lordaeron Army Units
};

#endif // DEF_THE_BATTLE_FOR_LORDAERON_H
