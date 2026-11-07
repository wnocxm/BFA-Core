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

// The Battle for Lordaeron (map 1760). See RESEARCH.md for the sources of every id below.

#ifndef DEF_THE_BATTLE_FOR_LORDAERON_H
#define DEF_THE_BATTLE_FOR_LORDAERON_H

#include "ScriptMgr.h"
#include "Creature.h"
#include "GameObject.h"
#include "InstanceScript.h"
#include "Scenario.h"

enum BattleForLordaeronMisc
{
    MAP_LORDAERON_SCENARIO              = 1760,
    SCENARIO_SIEGE_OF_LORDAERON         = 1371, // Alliance
    SCENARIO_FALL_OF_LORDAERON          = 1482, // Horde

    QUEST_THE_BATTLE_FOR_LORDAERON_H    = 51796,
    QUEST_THE_BATTLE_FOR_LORDAERON_A    = 51795,

    LFG_DUNGEON_BATTLE_FOR_LORDAERON_A  = 1758,
    LFG_DUNGEON_BATTLE_FOR_LORDAERON_H  = 1759,
};

// ScenarioStep.db2, scenario 1482 (Horde), in order.
enum HordeScenarioSteps
{
    STEP_H_UNDER_SIEGE                  = 3628,
    STEP_H_SPIES_IN_OUR_MIDST           = 3654,
    STEP_H_TO_THE_HEART                 = 3660,
    STEP_H_EVACUATE_HEART               = 3658,
    STEP_H_TO_BATTLE                    = 3661,
    STEP_H_PUSH_THEM_BACK               = 3677,
    STEP_H_DEFEND_WAR_MACHINE           = 3704,
    STEP_H_OUR_SECRET_WEAPON            = 3678,
    STEP_H_LIKE_WILDFIRE                = 3679,
    STEP_H_HOLDING_THEM_BACK            = 3680,
    STEP_H_THE_KING_PIECE               = 3681,
    STEP_H_CRY_HAVOC                    = 3687,
    STEP_H_THE_DARK_LADY                = 3688,
};

// CriteriaTree.db2 root tree of each Horde step.
enum HordeCriteriaTrees
{
    CRITERIA_TREE_H_UNDER_SIEGE         = 62528,
    CRITERIA_TREE_H_SPIES_IN_OUR_MIDST  = 62680,
    CRITERIA_TREE_H_TO_THE_HEART        = 62813,
    CRITERIA_TREE_H_EVACUATE_HEART      = 62810,
    CRITERIA_TREE_H_TO_BATTLE           = 62872,
    CRITERIA_TREE_H_PUSH_THEM_BACK      = 63971,
    CRITERIA_TREE_H_DEFEND_WAR_MACHINE  = 64876,
    CRITERIA_TREE_H_OUR_SECRET_WEAPON   = 63974,
    CRITERIA_TREE_H_LIKE_WILDFIRE       = 63976,
    CRITERIA_TREE_H_HOLDING_THEM_BACK   = 64018,
    CRITERIA_TREE_H_THE_KING_PIECE      = 64020,
    CRITERIA_TREE_H_CRY_HAVOC           = 64022,
    CRITERIA_TREE_H_THE_DARK_LADY       = 64024,
};

// Criteria.db2 assets (CRITERIA_TYPE_SEND_EVENT_SCENARIO unless noted).
// Weighted steps add (counter x weight) until the step amount is reached.
enum HordeScenarioEvents
{
    SCENARIO_EVENT_H_REPORT_TO_SAURFANG     = 61422,
    SCENARIO_EVENT_H_SPY_KILLED             = 61487, // weight 4, Spies in Our Midst (120)
    SCENARIO_EVENT_H_CITIZEN_RESCUED        = 61488, // weight 3
    SCENARIO_EVENT_H_FOLLOW_SAURFANG_HEART  = 62155, // To the Heart! (both needed)
    SCENARIO_EVENT_H_AMBUSH_CLEARED         = 61527,
    SCENARIO_EVENT_H_CITIZEN_EVACUATED      = 61525, // weight 2, Evacuate the Heart (110)
    SCENARIO_EVENT_H_HEART_ENEMY_KILLED     = 61526, // weight 3
    GAME_EVENT_H_USE_PORTAL                 = 61568, // CRITERIA_TYPE_SEND_EVENT
    SCENARIO_EVENT_H_FOLLOW_SAURFANG_BATTLE = 62174,
    SCENARIO_EVENT_H_ALLIANCE_DEFEATED      = 62175, // weight 2, Push Them Back (100)
    SCENARIO_EVENT_H_WAVE_DEFEATED          = 62289, // x3
    SCENARIO_EVENT_H_WAR_MACHINE_DEFENDED   = 62350,
    SCENARIO_EVENT_H_ALLIANCE_DRIVEN_BACK   = 62177, // weight 2, Our Secret Weapon (250)
    SCENARIO_EVENT_H_BLIGHT_USED            = 59597, // weight 2
    SCENARIO_EVENT_H_FOLLOW_NATHANOS_KEEP   = 62178,
    SCENARIO_EVENT_H_INTERCEPT_ALLIANCE     = 62202,
    SCENARIO_EVENT_H_DEFEAT_ANDUIN          = 62482, // weight 1, The King Piece (150)
    SCENARIO_EVENT_H_DEFEAT_ANDUIN_ALLY     = 62483, // weight 3
    SCENARIO_EVENT_H_DEFEAT_ANDUIN_ALLY_2   = 64503, // weight 10
    SCENARIO_EVENT_H_FOLLOW_NATHANOS_SAFETY = 62203,
    SCENARIO_EVENT_H_FOLLOW_SYLVANAS        = 62204,
};

enum BattleForLordaeronNpcs
{
    // Horde scenario allies (faction 2501)
    NPC_SAURFANG                        = 130367,
    NPC_SYLVANAS                        = 130365,
    NPC_BAINE                           = 133240,
    NPC_DARK_RANGER                     = 130379,
    NPC_UNDERCITY_GUARDIAN              = 130585,
    NPC_ROYAL_DREADGUARD                = 130590,
    NPC_ORGRIMMAR_RAIDER                = 130389,
    NPC_TAUREN_EARTHSHAKER              = 130390,
    NPC_DARKSPEAR_SHAMAN                = 130391,
    NPC_FORSAKEN_APOTHECARY             = 130393,
    NPC_GOBLIN_SAPPER                   = 130394,
    NPC_ANASTASIA_HARTWELL              = 130938,

    // Civilians
    NPC_UNDERCITY_EVACUEE               = 130687,
    NPC_PORTAL_TO_ORGRIMMAR             = 130688,
    NPC_PORTAL_TO_THE_KEEP              = 126680, // "Portal", spellclick casts SPELL_TELEPORT_TO_LORDAERON

    // Alliance enemies
    NPC_SI7_OPERATIVE                   = 131137,
    NPC_MIRABELLE                       = 131736,
    NPC_RENZIK_THE_SHIV                 = 131737,
    NPC_DRUID_OF_THE_CLAW_LEADER        = 144380, // "Druids of the Claw, ATTACK!"
    NPC_DRUID_OF_THE_CLAW_1             = 131138,
    NPC_DRUID_OF_THE_CLAW_2             = 131139,

    // Alliance scenario point of view (static spawns of map 1760)
    NPC_SAURFANG_ALLIANCE_SCENARIO      = 129179,

    NPC_ISABELLA_ORGRIMMAR              = 139093, // SmartAI makes the player cast SPELL_LEAVE_FOR_LORDAERON_QUEUE
};

enum BattleForLordaeronGameObjects
{
    // Portcullises of Saurfang's room, opened when his briefing ends.
    GO_SPAWN_MAGIC_QUARTER_PORTCULLIS_1 = 210406635, // Portcullis 278810 (1691.11, 72.12)
    GO_SPAWN_MAGIC_QUARTER_PORTCULLIS_2 = 210406625, // Portcullis 278768 (1731.04, 102.81)
    GO_SPAWN_MAGIC_QUARTER_PORTCULLIS_3 = 210406656, // Portcullis 278734 (1638.82, 66.29)

    // Portcullises on Saurfang's way to the heart of the city, opened when To the Heart! starts.
    GO_SPAWN_CANAL_PORTCULLIS           = 210406648, // Portcullis 278921 (1632.62, 108.90)
    GO_SPAWN_HEART_PORTCULLIS_1         = 210406650, // Portcullis 280729 (1559.52, 208.51)
    GO_SPAWN_HEART_PORTCULLIS_2         = 210406654, // Portcullis 280729 (1563.16, 204.13)
};

enum BattleForLordaeronSpells
{
    SPELL_LEAVE_FOR_LORDAERON_QUEUE     = 269807, // joins LFG 1759
    SPELL_TELEPORT_TO_LORDAERON         = 259094, // Undercity -> courtyard (spell_target_position)
    SPELL_EVACUATE_OVERRIDE             = 261808, // override bar 995 = SPELL_EVACUATE
    SPELL_EVACUATE                      = 261807,
};

// Conversation ids from a Lordaeron sniff (build 32722), created by spells 273582+ on retail.
enum BattleForLordaeronConversations
{
    CONVERSATION_SAURFANG_BRIEFING      = 6430, // Soldiers of the Horde, the time has come!
    CONVERSATION_SPIES_CORNERED         = 6458, // More spies! We have them cornered!
    CONVERSATION_DRUIDS_ATTACK          = 6790, // Druids of the Claw, ATTACK!
    CONVERSATION_OPEN_THE_GATE          = 6680, // The tunnels have been secured. Open the gate!
    CONVERSATION_PORTAL_TO_THE_KEEP     = 6487, // No time to waste! Open a portal to the keep...
    CONVERSATION_UNDERCITY_EVACUATED    = 6789, // Is the Undercity evacuated?
};

// creature_summon_groups, summonerType 2 (map 1760).
enum BattleForLordaeronSummonGroups
{
    SUMMON_GROUP_MAGIC_QUARTER          = 1,  // trainers, rangers and guards around Saurfang
    SUMMON_GROUP_MAGIC_QUARTER_EVAC     = 2,  // Anastasia, evacuees and the portal to Orgrimmar
    SUMMON_GROUP_SPIES                  = 3,
    SUMMON_GROUP_CAPTIVES               = 4,
    SUMMON_GROUP_AMBUSH                 = 5,  // Mirabelle, Renzik and the Royal Dreadguards
    SUMMON_GROUP_DRUIDS                 = 6,
    SUMMON_GROUP_GATE_GUARDS            = 7,
    SUMMON_GROUP_HEART_CITIZENS         = 8,
    SUMMON_GROUP_HEART_ENEMIES          = 9,
    SUMMON_GROUP_COURTYARD              = 10, // Sylvanas, Baine and the Horde army
};

enum BattleForLordaeronData
{
    DATA_EVACUATE_NEAR                  = 2, // SetGuidData: player that cast SPELL_EVACUATE
    DATA_EVACUATE_CITIZEN               = 4, // SetGuidData: creature hit by SPELL_EVACUATE
    DATA_PLAYER_USED_KEEP_PORTAL        = 3, // SetGuidData: player that used the portal
};

namespace BattleForLordaeron
{
    // Map 1760 shares map 0 coordinates; Undercity positions come from map 0 spawns,
    // the rest from WorldSafeLocs and guides (see RESEARCH.md).
    Position const PlayerStartPos       = { 1770.206665f, 63.413601f, -46.320221f, 2.450299f }; // lfg_dungeon_template 1759 (WorldSafeLocs 6605 is 1770.82, 62.22)
    Position const SaurfangMagicQuarter = { 1713.66f,  63.30f, -62.28f, 0.0f };        // placed in game
    Position const SaurfangHeart        = { 1588.00f, 232.00f, -62.08f, 0.00f };
    Position const MagicQuarterPortal   = { 1705.62f,  41.75f, -63.84f, 0.0f };        // placed in game
    Position const AnastasiaHeart       = { 1585.00f, 225.50f, -62.09f, 1.40f };
    Position const KeepPortal           = { 1582.50f, 229.00f, -62.09f, 1.40f };
    Position const CourtyardArrival     = { 1645.00f, 240.00f,  62.67f, 3.14f };       // spell_target_position of SPELL_TELEPORT_TO_LORDAERON
    Position const SaurfangCourtyard    = { 1636.50f, 240.00f,  62.67f, 3.14f };
    Position const SaurfangFrontLine    = { 1856.80f, 238.30f,  62.13f, 0.00f };
}

#endif // DEF_THE_BATTLE_FOR_LORDAERON_H
