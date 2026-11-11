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
    STEP_H_FINALE                       = 3790, // no criteria: completed by the script after the last step
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
    NPC_NATHANOS                        = 133243,
    NPC_LORTHEMAR                       = 137109,
    NPC_SLUDGE_GUARD_TRIZZ              = 125516, // gas masks and blight canisters (Our Secret Weapon)
    NPC_AZERITE_WAR_MACHINE             = 132693,
    NPC_WOUNDED_HORDE_SOLDIER           = 127065,
    NPC_PLAGUE_WAGON                    = 126786,
    NPC_HORDE_DEMOLISHER                = 124674,
    NPC_FORSAKEN_APOTHECARY_RESERVE     = 133142,

    // Civilians
    NPC_UNDERCITY_EVACUEE               = 130687,
    NPC_PORTAL_TO_ORGRIMMAR             = 130688,
    NPC_PORTAL_TO_THE_KEEP              = 131087, // "Portal to Lordaeron", spellclick casts SPELL_TELEPORT_TO_LORDAERON

    // Alliance enemies
    NPC_SI7_OPERATIVE                   = 131137,
    NPC_MIRABELLE                       = 131736,
    NPC_RENZIK_THE_SHIV                 = 131737,
    NPC_DRUID_OF_THE_CLAW_LEADER        = 144380, // "Druids of the Claw, ATTACK!"
    NPC_DRUID_OF_THE_CLAW_1             = 131138,
    NPC_DRUID_OF_THE_CLAW_2             = 131139,
    NPC_ANDUIN                          = 133249,
    NPC_GENN                            = 133250,
    NPC_JAINA                           = 133251,
    NPC_ALLERIA                         = 136392,
    NPC_MEKKATORQUE                     = 133410,
    NPC_FLAMETRON_5000                  = 133370,
    NPC_RENDOREI_RANGER                 = 133368,
    NPC_ALLIANCE_MAGE                   = 134111,
    NPC_KNIGHT_CAPTAIN_EMERY            = 144382, // wave 1 of Defend the Azerite War Machine
    NPC_MASTER_SHAPESHIFTER_LYARA       = 140129, // wave 2
    NPC_MECHANORANGER_COGSPARK          = 139491, // wave 3
    NPC_STORMWIND_FOOTMAN               = 130385,
    NPC_DWARVEN_RIFLEMAN                = 132404,
    NPC_DARNASSIAN_HUNTRESS             = 132403,
    NPC_GLAIVE_THROWER                  = 132405,
    NPC_GILNEAN_MAULER                  = 133526,
    NPC_EXODAR_PEACEKEEPER              = 133508,
    NPC_AUCHENAI_BALLISTA               = 133507,
    NPC_GRYPHON_RIDER                   = 133502,
    NPC_WORGEN_BATTLEMAGE               = 133528,
    NPC_EXODAR_HIGH_PRIEST              = 138067,
    NPC_ALLIANCE_SIEGE_TANK             = 124657,

    // Alliance scenario point of view (static spawns of map 1760)
    NPC_SAURFANG_ALLIANCE_SCENARIO      = 129179,

    NPC_ISABELLA_ORGRIMMAR              = 139093, // SmartAI makes the player cast SPELL_LEAVE_FOR_LORDAERON_QUEUE
    NPC_SCENARIO_COMPLETE_CREDIT        = 139094, // quest 51796 objective "Complete scenario"
};

enum BattleForLordaeronGameObjects
{
    // Portcullises of Saurfang's room, opened when his briefing ends.
    GO_SPAWN_MAGIC_QUARTER_PORTCULLIS_1 = 210406635, // Portcullis 278810 (1691.11, 72.12)
    GO_SPAWN_MAGIC_QUARTER_PORTCULLIS_2 = 210406625, // Portcullis 278768 (1731.04, 102.81)
    GO_SPAWN_MAGIC_QUARTER_PORTCULLIS_3 = 210406656, // Portcullis 278734 (1638.82, 66.29)

    // Canal on Saurfang's way, opened when Spies in Our Midst ends.
    GO_SPAWN_CANAL_PORTCULLIS           = 210406648, // Portcullis 278921 (1632.62, 108.90)

    // Gate of the heart of the city, opened when Saurfang reaches it ("Open the gate!").
    GO_SPAWN_HEART_PORTCULLIS_1         = 210406650, // Portcullis 280729 (1559.52, 208.51)
    GO_SPAWN_HEART_PORTCULLIS_2         = 210406654, // Portcullis 280729 (1563.16, 204.13)
    GO_SPAWN_KEEP_FRONT_GATE_INNER      = 210406716, // Front Gate 271743 (1849.43, 237.98)
    GO_SPAWN_KEEP_FRONT_GATE_OUTER      = 210406633, // Front Gate 271743 (1873.76, 237.65)
    // Throne room, opened when Sylvanas takes the champions inside (The Dark Lady).
    GO_SPAWN_THRONE_ROOM_GATE           = 210406636, // Front Gate 278719 (1667.93, 239.83)
    GO_SPAWN_COURTYARD_GATE_SOUTH       = 210406715, // Front Gate 279041 (1726.80, 220.09)
    GO_SPAWN_COURTYARD_GATE_NORTH       = 210406717, // Front Gate 279041 (1726.86, 258.04)

    // Blight clouds. The sniffed map has them outside the gates from the start; in the Horde
    // scenario they only appear once Sylvanas has dumped the blight.
    GO_BLIGHT_01                        = 281723,
    GO_BLIGHT_02                        = 281724,
    GO_BLIGHT_03                        = 281725,
    GO_BLIGHT_04                        = 281726,
    GO_BLIGHT_01_FIELD                  = 281727,
};

// Undercity citizens in the heart of the city (custom ids, texts are BroadcastText 141822-141825).
enum BattleForLordaeronGossip
{
    GOSSIP_MENU_UNDERCITY_CITIZEN       = 600001, // npc_text 600001: citizen greetings 141822-141824
    BROADCAST_TEXT_GET_TO_THE_PORTAL    = 141825, // "The Undercity is under attack! Get to the mage portal!"

    GOSSIP_MENU_SLUDGE_GUARD_TRIZZ      = 600002, // npc_text 600002: BroadcastText 154880
    BROADCAST_TEXT_GAS_MASK             = 154903, // "Just a gas mask for me, please."
    BROADCAST_TEXT_GAS_MASK_AND_BLIGHT  = 154904, // "Gas mask and blight canister, please. ..."
    BROADCAST_TEXT_TRIZZ_GAS_MASKS      = 150265, // "Gas masks! Get your gas masks here! ..."
    BROADCAST_TEXT_FALL_INTO_POSITION   = 153660, // Nathanos: "Soldiers, fall into position! The Alliance approaches!"
    BROADCAST_TEXT_BAINE_RETREAT        = 148208, // "RETREAT!"
    BROADCAST_TEXT_JAINA_MUST_NOT_ESCAPE = 148209, // "No! They must not escape!"
};

enum BattleForLordaeronSpells
{
    SPELL_LEAVE_FOR_LORDAERON_QUEUE     = 269807, // joins LFG 1759
    SPELL_TELEPORT_TO_LORDAERON         = 259094, // Undercity -> courtyard (spell_target_position)
    SPELL_BLIGHT_RESISTANT_GAS_MASK     = 271541,
    SPELL_BLIGHT_CANISTER               = 261723, // override spells 994 (261724, which sprays 261748)
    SPELL_BLIGHT_SPRAY                  = 261748,
    SPELL_BANDAGING                     = 249366, // spellclick of the wounded soldiers
    SPELL_WOUNDED                       = 255521, // the wounded soldiers' aura (Alliance scenario's 125622)

    // The soldier templates have no spells; ranged soldiers use the generic NPC ones.
    SPELL_SOLDIER_SHOOT                 = 6660,
    SPELL_SOLDIER_FIREBALL              = 9053,
    SPELL_SOLDIER_LIGHTNING_BOLT        = 9532,
    SPELL_SOLDIER_HOLY_SMITE            = 9734,
    SPELL_SOLDIER_SHADOW_BOLT           = 9613,
};

// SceneScriptPackage / Movie.db2
enum BattleForLordaeronCinematics
{
    SCENE_AZERITE_MACHINE_ARRIVES       = 3022, // package 2009 "Lordaeron Scenario - Azerite Machine Arrives"
    SCENE_WAR_MACHINE_DESTROYED         = 3025, // package 2028 "[AWM] Azerite War Machine"
    SCENE_SYLVANAS_DUMPS_BLIGHT         = 3026, // package 2032 "[SDB] Sylvanas Dumps Blight"
    SCENE_SYLVANAS_BLOCKS_ALLIANCE      = 1845, // package 2031 "[SBA] Sylvanas Blocks Alliance (Exploding Cisterns)"
    MOVIE_TURN_THE_TIDE                 = 856,  // "Battle for Lordaeron: Turn the Tide"
    MOVIE_THRONE_ROOM_HORDE             = 855,  // "Lordaeron Throne Room Confrontation - Horde"
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
    CONVERSATION_WAR_MACHINE_ARRIVES    = 5500, // Anduin: It is over, Sylvanas!
    CONVERSATION_WAVE_EMERY             = 8690, // Riflemen! Lock and load! Advance!
    CONVERSATION_BLAST_THE_TOWERS       = 9675, // Forget the army... Blast the siege towers!
    CONVERSATION_WAVE_LYARA             = 8694, // Archers, nock your arrows! Advance!
    CONVERSATION_WAVE_COGSPARK          = 8691, // Um, ladies and gentlemen, rev those engines!
    CONVERSATION_WOUNDED_ON_THE_FIELD   = 8164, // Saurfang: Your brothers and sisters lie wounded...
    CONVERSATION_CUT_THEM_OFF           = 7736, // Sylvanas: Everyone, move! Cut them off with the reserves!
    CONVERSATION_NATHANOS_BAINE         = 7760, // Nathanos: Baine! Gather the catapults...
    CONVERSATION_NATHANOS_LORTHEMAR     = 7762, // Nathanos: Lor'themar, command our rangers from the flank!
    CONVERSATION_BLIGHT_THROWERS        = 7759, // Nathanos: Blight throwers, you are our front line!
    CONVERSATION_OUTNUMBERED            = 5926, // Nathanos: It is you who is outnumbered now.
    CONVERSATION_EVEN_THE_ODDS          = 7659, // Alleria: Let us even the odds, King Wrynn!
    CONVERSATION_ASSAULT_WILL_FALTER    = 8166, // Lor'themar: Their assault will soon falter.
    CONVERSATION_FOLLOW_NATHANOS        = 7928, // Sylvanas: Everyone! Follow Nathanos to the courtyard!
    CONVERSATION_KEEP_MOVING            = 8159, // Sylvanas: Keep moving!
    CONVERSATION_ALMOST_THERE           = 8160, // Sylvanas: We're almost there! Make haste, heroes!
    CONVERSATION_FINALLY_SHOW_UP        = 6787, // Sylvanas: So good of you to finally show up, High Overlord.
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
    // Alliance-view layout of the same battle (build 27219) with Horde scenario entries
    SUMMON_GROUP_FRONT_HORDE            = 11, // the whole field outside the gates, from Push Them Back on
    SUMMON_GROUP_FRONT_ALLIANCE         = 12,
    SUMMON_GROUP_BLIGHT_HORDE           = 13, // Our Secret Weapon, after the blight: Risen Skeletons, Sludge Guards, Blight Throwers
    SUMMON_GROUP_WOUNDED                = 14, // Our Secret Weapon: wounded Horde soldiers
    SUMMON_GROUP_CHOKEPOINT_HORDE       = 15, // Holding Them Back / The King Piece
    SUMMON_GROUP_CHOKEPOINT_ALLIANCE    = 16,
    SUMMON_GROUP_VOID_TEAR              = 17, // Ren'dorei rangers and Flametrons with Alleria
    SUMMON_GROUP_KEEP_WALLS             = 18, // dark rangers and blight throwers on the walls
    SUMMON_GROUP_ROAD_RESERVES          = 19, // Horde reserves along the road south of the keep
};

enum BattleForLordaeronData
{
    DATA_EVACUATE_CITIZEN               = 4, // SetGuidData: citizen told to get to the mage portal
    DATA_JUMP_TO_STEP                   = 5, // SetData: test command .lordaeron step <2-5>
    DATA_PLAYER_USED_KEEP_PORTAL        = 3, // SetGuidData: player that used the portal
    DATA_MOVIE_COMPLETE                 = 6, // SetData: a player finished (or skipped) a movie
    DATA_SCENE_COMPLETE                 = 7, // SetData: a player finished (or skipped) a scene
    DATA_BLIGHT_SPRAY_HIT               = 8, // SetGuidData: creature hit by Blight Spray
    DATA_WOUNDED_RESCUED                = 9, // SetGuidData: wounded soldier bandaged by a player
};

// Routes that can be set in game: waypoint_data paths (.wp add <id>, then .wp reload <id>).
// A path that has no points uses the route written in the script.
enum BattleForLordaeronPaths
{
    PATH_NATHANOS_TO_SIDE               = 1760901, // Like Wildfire: courtyard -> where he talks to Baine
    PATH_NATHANOS_TO_LORTHEMAR          = 1760902, // -> Lor'themar on the road south of the keep
    PATH_NATHANOS_TO_CHOKEPOINT         = 1760903, // -> the chokepoint west of the keep
    PATH_NATHANOS_RETREAT_1             = 1760904, // Cry Havoc: chokepoint -> "Keep moving!"
    PATH_NATHANOS_RETREAT_2             = 1760905, // -> "We're almost there!"
    PATH_NATHANOS_RETREAT_3             = 1760906, // -> courtyard
    PATH_LORTHEMAR_TO_CHOKEPOINT        = 1760907,
    PATH_SYLVANAS_TO_THRONE_ROOM        = 1760908, // The Dark Lady
    PATH_BAINE_TO_THRONE_ROOM           = 1760909,
    PATH_NATHANOS_TO_THRONE_ROOM        = 1760910,
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
    // Keep courtyard, outside the throne room: Saurfang faces Sylvanas (in-game .gps)
    Position const CourtyardArrival     = { 1782.00f, 238.60f,  62.00f, 6.248f };      // spell_target_position of SPELL_TELEPORT_TO_LORDAERON
    Position const SaurfangCourtyard    = { 1792.471436f, 238.566727f, 60.586876f, 0.0f };
    Position const SaurfangFrontLine    = { 1856.80f, 238.30f,  62.13f, 0.00f };

    // Steps 6-13: Dugi guide points converted to world coordinates, leaders from the
    // Alliance-view sniff (Nathanos 136394, Lor'themar 137939, Alleria 125138, Mekkatorque 129043).
    Position const SaurfangPushLine     = { 1920.72f, 244.65f,  50.00f, 0.00f };       // Push Them Back
    Position const WarMachinePos        = { 2011.34f, 243.43f,  36.00f, 0.00f };       // Defend the Azerite War Machine (guide point)
    Position const WarMachineDefense    = { 2003.00f, 234.00f,  36.00f, 0.00f };       // Saurfang, next to it
    Position const TrizzPos             = { 1905.75f, 233.12f,  56.00f, 3.14f };       // Our Secret Weapon
    Position const NathanosCourtyard    = { 1794.00f, 233.50f,  60.60f, 0.68f };       // next to Sylvanas
    Position const BaineSide            = { 1800.50f, 184.00f,  61.00f, 1.25f };
    Position const BaineCourtyard       = { 1797.50f, 243.00f,  62.00f, 3.86f };       // group 10
    Position const LorthemarFlank       = { 1598.50f, 135.50f,  61.00f, 0.00f };
    Position const NathanosChokepoint   = { 1466.97f, 273.03f,  61.70f, 1.40f };       // Holding Them Back
    Position const LorthemarChokepoint  = { 1463.42f, 274.44f,  61.68f, 1.45f };
    Position const AnduinChokepoint     = { 1468.00f, 352.00f,  61.60f, 4.61f };
    Position const JainaChokepoint      = { 1462.00f, 356.00f,  61.60f, 4.61f };
    Position const GennChokepoint       = { 1474.00f, 356.00f,  61.60f, 4.61f };
    Position const AlleriaVoidTear      = { 1524.02f, 401.36f,  61.60f, 4.40f };
    Position const MekkatorqueVoidTear  = { 1518.83f, 397.75f,  61.13f, 4.40f };
    Position const AllianceMarchTarget  = { 1470.00f, 305.00f,  61.70f, 4.61f };
    Position const SylvanasCryHavoc     = { 1472.00f, 262.00f,  61.70f, 1.20f };       // Cry Havoc
    Position const BaineCryHavoc        = { 1477.00f, 258.00f,  61.70f, 1.20f };
    Position const SylvanasCourtyard    = { 1799.169678f, 237.653152f, 62.551392f, 3.142f }; // in-game .gps
    Position const SylvanasThroneDoor   = { 1712.56f, 239.19f,  62.10f, 3.14f };       // The Dark Lady
    Position const BaineThroneDoor      = { 1716.50f, 243.50f,  62.10f, 3.14f };
    Position const NathanosThroneDoor   = { 1716.50f, 235.00f,  62.10f, 3.14f };
    // Inside the throne room (gate at 1667.93, throne at 1628.46 240.08 in the sniff); they wait facing the door.
    Position const ThroneRoomEntrance   = { 1660.00f, 239.80f,  62.60f, 3.14f };
    Position const SylvanasThroneRoom   = { 1638.00f, 240.00f,  62.60f, 0.00f };
    Position const BaineThroneRoom      = { 1645.00f, 246.00f,  62.60f, 0.00f };
    Position const NathanosThroneRoom   = { 1645.00f, 234.00f,  62.60f, 0.00f };
    Position const OrgrimmarGates       = { 1568.61f, -4398.67f, 16.13f, 0.60f };      // map 1, WorldSafeLocs 6638
}

#endif // DEF_THE_BATTLE_FOR_LORDAERON_H
