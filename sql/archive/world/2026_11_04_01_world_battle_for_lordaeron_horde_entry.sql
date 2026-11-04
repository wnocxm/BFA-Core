-- The Battle for Lordaeron (Horde): queue from Isabella and enter at the Magic Quarter

-- LFG 1759 (LFGDungeons.db2, map 1760, difficulty 12): entrance in the Undercity Magic Quarter
DELETE FROM `lfg_dungeon_template` WHERE `dungeonId` = 1759;
INSERT INTO `lfg_dungeon_template` (`dungeonId`, `name`, `position_x`, `position_y`, `position_z`, `orientation`, `requiredItemLevel`, `VerifiedBuild`) VALUES
(1759, 'The Battle for Lordaeron (Horde)', 1770.206665, 63.413601, -46.320221, 2.450299, 0, 0);

-- Isabella (Orgrimmar): "I am ready to sail to Lordaeron." queues the scenario
UPDATE `creature_template` SET `AIName` = '', `ScriptName` = 'npc_isabella_battle_for_lordaeron' WHERE `entry` = 139093;

-- Friendly Horde Saurfang used inside the scenario (unused elsewhere, was level 1)
UPDATE `creature_template` SET `minlevel` = 120, `maxlevel` = 120, `HealthScalingExpansion` = 7 WHERE `entry` = 124798;
