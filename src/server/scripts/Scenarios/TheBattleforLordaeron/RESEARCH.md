# The Battle for Lordaeron (Horde) - research notes

Scenario 1482 "The Fall of Lordaeron", map 1760, difficulty 12, LFG 1759, quest 51796
(Saurfang 14720 in Grommash Hold gives it, Isabella 139093 queues, Nathanos 140176 at the
Orgrimmar gates takes it, next quest 53028).

No open source core has this scenario scripted (TrinityCore, AshamaneCore, AzgathCore,
DekkCore, CypherCore searched). No public creature spawn sniff for the Horde side was found.

## Sources

| Source | What it gives |
|---|---|
| Client DB2 (8.3.7, uploaded) | Scenario, steps, criteria, LFG, spells |
| `sql/base/bfa_hotfixes` dump | Full BroadcastText (all dialogue text) |
| `ThingEngineering/wow-dumps` (GitHub) | SpellEffect, OverrideSpellData, UiMapAssignment, ConversationLine CSVs |
| `TCDEVEmu/AtalDazar_BFA` `sql/Ataldazar/world/2020_12_03_00_Start_bfa_horde.sql` | WPP parse of a real Lordaeron sniff (build 32722): conversation templates, lines, actors, timings |
| DugiGuides `10_Battle_for_Lordaeron_H.lua` | Step order, NPC ids, UI map coordinates |
| TrinityCore `2019_10_26_01_world.sql` | WorldSafeLocs from client data (start point, graveyards) |
| World DB `creature_template` (build 35662) | Horde scenario NPC templates (faction 2501 = Horde side, 2502/2732 = Alliance enemies) |

UI map to world: UiMap 998 (Undercity) region X 1237.84..1877.95, Y -86.18..873.19;
UiMap 908 (Ruins of Lordaeron) region X 1254.17..3277.08, Y -1335.42..1697.92.
`worldX = maxX - uiY * (maxX - minX)`, `worldY = maxY - uiX * (maxY - minY)`.

## WorldSafeLocs (client data)

| Id | Position | Name |
|---|---|---|
| 6605 | 1770.82, 62.2167, -46.3213, 216.2° | Lordaeron Scenario - Horde Start |
| 6425 | 1792.62, 128.297, -63.2433 | Horde GY 1 (Magic Quarter) |
| 6637 | 1595.13, 274.372, -55.343 | Horde GY 1b (heart of the city) |
| 6426 | 1844.98, 198.852, 70.3996 | Horde GY 2 (walls) |
| 6427 | 1596.88, 162.622, 62.2175 | Horde GY 3 |
| 6636 | 1841.41, 138.347, 70.3996 | Horde GY 4 |
| 6638 | map 1: 1568.61, -4398.67, 16.1283 | Scenario exit (Horde) |

## Steps (guide coordinates converted to world X, Y)

| # | Step | Guide / data |
|---|---|---|
| 1 | Under Siege - report to Saurfang | Saurfang at (1715.0, 63.2) Magic Quarter. Conversation 6430 |
| 2 | Spies in Our Midst (120) | Mage District (1691.6, 89.4) (1717.6, 115.6) (1693.6, 140.1). Kill SI:7 = event 61487 (x4); SI:7 Intelligence Report spell 260012 sends 61488 (x3). Conversation 6458 |
| 3 | To the Heart! | Follow Saurfang (1653.6, 117.9) (1597.0, 106.2) (1595.3, 172.5). Gate conversation 6680 (Saurfang + Royal Dreadguard 140039) |
| 4 | Evacuate the Heart (110) | (1595.3, 231.1). Talk to a citizen: gossip option "The Undercity is under attack! Get to the mage portal!" (BroadcastText 141825; citizen texts 141822-141824, script in scenario_the_battle_for_lordaeron.cpp) evacuates it, x2 (61525). Hostile NPCs x3 (61526). Spell 261807 "Evacuate!" (override bar 995 of 261808) exists in client data but retail videos show the gossip |
| 5 | To Battle! | Portal to Lordaeron 131087 at (1582.5, 228.9), conversation 6487 ("Open a portal to the keep"). Walls: conversation 6789. Follow Saurfang to (1856.8, 238.3) |
| 6 | Push Them Back (100) | (1920.7, 244.6). Conversation 8164 |
| 7 | Defend the Azerite War Machine | (2011.3, 243.4). Waves led by Lyara 140129 (6790/8694), Emery 144382 (8690), Cogspark 139491 (8691). Conversation 9675 |
| 8 | Our Secret Weapon (250) | Sludge Guard Trizz 125516 at (1905.7, 233.1) gives Blight Resistant Gas Mask 271541 + Blight Canister 261723 (override bar 994 = 261724, Blight Spray 261748). Use at (2174.2, 219.8) |
| 9 | Like Wildfire | Nathanos 133243: (1804.6, 180.6) (1792.3, 142.4) (1741.9, 122.1) (1603.5, 130.9) (1449.2, 258.6). Conversations 7736, 7760, 7762, 7759 |
| 10 | Holding Them Back | Intercept at (1455.6, 264.7) with Nathanos and Lor'themar 137109. Conversation 8166 |
| 11 | The King Piece (150) | (1464.6, 275.0). Conversations 5926, 7659: Anduin 133249, Jaina 133251, Genn 133250, Alleria 136392, Mekkatorque 126794 |
| 12 | Cry Havoc | Follow Nathanos (1558.4, 123.9) (1806.8, 158.2) (1780.9, 238.3). Conversation 7928 |
| 13 | The Dark Lady | Follow Sylvanas 139104 to (1712.6, 239.2). Conversations 8159, 8160, 6787 (final) |

## Conversations (spell that creates them, actors by index)

| Conv | Spell | Actors | First line |
|---|---|---|---|
| 6430 | 273582 | 0 Saurfang 130367 | Soldiers of the Horde, the time has come! |
| 6458 | 273584 | 0 Saurfang, 1 Mirabelle 131736, 2 Renzik 131737 | More spies! We have them cornered! |
| 6680 | 273586 | 0 Saurfang, 1 Royal Dreadguard 140039 | The tunnels have been secured. Open the gate! |
| 6487 | 273585 | 0 Saurfang | No time to waste! Open a portal to the keep... |
| 6789 | 273588 | 0 Sylvanas 124651, 1 Saurfang | Is the Undercity evacuated? |
| 8164 | - | 0 Saurfang, 1 Sylvanas | Soldiers of the Horde! Your brothers and sisters lie wounded... |
| 9675 | - | 0 Sylvanas | Forget the army... Blast the siege towers! |
| 6790 / 8690 / 8691 / 8694 | - | wave leaders | Druids of the Claw, ATTACK! / Riflemen! / rev those engines / Archers |
| 7736 | 273589 | 0 Sylvanas, 1 Nathanos 133243 | Everyone, move! Cut them off with the reserves! |
| 7760 | - | 0 Nathanos, 1 Baine 133240 | Baine! Gather the catapults... |
| 7762 | - | 0 Nathanos, 1 Lor'themar 137109 | Lor'themar, command our rangers from the flank! |
| 7759 | - | 0 Nathanos | Blight throwers, you are our front line! |
| 8166 | - | 0 Lor'themar | Their assault will soon falter. Remain steadfast! |
| 5926 | 273594 | 0 Nathanos, 1 Jaina 124787, 2 Anduin 126309 | It is you who is outnumbered now. |
| 7659 | 273596 | Alleria 125138, Anduin, Mekkatorque 126794, Nathanos, Alleria 136392, Lor'themar | Let us even the odds, King Wrynn! |
| 7928 | - | 0 Sylvanas | Everyone! Follow Nathanos to the courtyard! |
| 8159 / 8160 | 273590 / 273591 | 0 Sylvanas | Keep moving! / We're almost there! |
| 6787 | 273592 | 0 Sylvanas, 1 Saurfang | So good of you to finally show up, High Overlord. |

## Horde scenario NPC templates (faction 2501, not spawned in the world DB)

Sylvanas 130365, Saurfang 130367, Dark Ranger 130379, Orgrimmar Raider 130389,
Tauren Earthshaker 130390, Darkspear Shaman 130391, Forsaken Blight Thrower 130392,
Forsaken Apothecary 130393, Goblin Sapper 130394, Crawler Mine 130397, Sludge Guard 130444,
Undercity Guardian 130585, Royal Dreadguard 130590 / 144504, Nathanos 133243, Baine 133240,
Lor'themar 137109, Sludge Guard Trizz 125516, Undercity vendors 130938-130981.
Neutral: Undercity Evacuee 130687, Portal to Orgrimmar 130688.
Enemies: SI:7 Operative 131137, Mirabelle 131736, Renzik "The Shiv" 131737,
Druid of the Claw 131138 / 131139 / 144380, Stormwind Footman 130385, Gilnean Mauler 133526,
Exodar Peacekeeper 133508, Auchenai Ballista 133507, Gryphon Rider 133502, Goblin Shredder 134015,
Worgen Battlemage 133528, Ren'dorei Ranger 133368, Knight-Captain Emery 144382,
Mechanoranger Cogspark 139482 / 139491, Master Shapeshifter Lyara 140129,
Anduin 133249, Genn 133250, Jaina 133251, Alleria 136392, Mekkatorque 133410.

The 893 creature spawns on map 1760 in the world DB are the Alliance scenario's point of view
(build 27219). Both scenarios show the same battle, so the Horde scenario can reuse the layout by
swapping entries per team in `InstanceScript::GetCreatureEntry`.

## Steps 6-13 (scripted)

All of them are CRITERIA_TYPE_SEND_EVENT_SCENARIO, sent by the instance script. The step
after The Dark Lady (3790) has no criteria; the script completes it after the last step.

| # | Step | Script |
|---|---|---|
| 6 | Push Them Back (100) | Groups 11/12 (front line) and 18 (walls). Kills a player took part in send 62175 (weight 2) |
| 7 | Defend the Azerite War Machine | Scene 3022, conversation 5500, War Machine 132693 at (1998, 243). Waves: Emery 144382 (8690, then Sylvanas 9675), Lyara 140129 (8694), Cogspark 139491 (8691). Each wave 62289, then 62350 |
| 8 | Our Secret Weapon (250) | Scenes 3025 (AWM) and 3026 (SDB), conversation 8164. Trizz 125516 at (1905.75, 233.12): gas mask 271541 / blight canister 261723. Kills send 62177 and 59597, Blight Spray 261748 hits send 59597 |
| 9 | Like Wildfire | Movie 856 (Turn the Tide), courtyard. Conversations 7736, 7760 (Baine), 7762 (Lor'themar), 7759; Nathanos leads to (1449.2, 258.6), "Soldiers, fall into position!" (153660), 62178 |
| 10 | Holding Them Back | Anduin 133249, Jaina 133251, Genn 133250 north of the chokepoint; conversation 5926; void tear: Alleria 136392, Mekkatorque 133410, group 17; 62202 |
| 11 | The King Piece (150) | Conversation 7659, the Alliance (groups 16/17) charges the reserves (group 15). Weights: soldiers 1 (62482), Ren'dorei and mages 3 (62483), Flametrons 10 (64503). Lor'themar's 8166 at 75 |
| 12 | Cry Havoc | Baine "RETREAT!" (148208), scene 1845 (SBA), Jaina 148209, conversation 7928; Nathanos back to (1780.9, 238.3) with 8159/8160; 62203 |
| 13 | The Dark Lady | Saurfang walks to Sylvanas, conversation 6787, Sylvanas leads to (1712.6, 239.2); 62204 |
| - | Finale | Movie 855 (Throne Room - Horde), kill credit 139094 (quest objective), teleport to Orgrimmar (WorldSafeLocs 6638) |

Scenes: scene_template 3022/3025/3026/1845 use SceneScriptPackage 2009 "Azerite Machine Arrives",
2028 "[AWM] Azerite War Machine", 2032 "[SDB] Sylvanas Dumps Blight", 2031 "[SBA] Sylvanas Blocks
Alliance (Exploding Cisterns)". Movies 855/856 from Movie.db2 (wiki MovieID list).
