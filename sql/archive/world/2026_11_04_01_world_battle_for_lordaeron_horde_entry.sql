-- The Battle for Lordaeron (Horde): queue from Isabella and enter at the Magic Quarter

-- LFG 1759 (LFGDungeons.db2, map 1760, difficulty 12): entrance in the Undercity Magic Quarter
DELETE FROM `lfg_dungeon_template` WHERE `dungeonId` = 1759;
INSERT INTO `lfg_dungeon_template` (`dungeonId`, `name`, `position_x`, `position_y`, `position_z`, `orientation`, `requiredItemLevel`, `VerifiedBuild`) VALUES
(1759, 'The Battle for Lordaeron (Horde)', 1770.206665, 63.413601, -46.320221, 2.450299, 0, 0);

-- Isabella (Orgrimmar): gossip 19870 option 1 ("I am ready to go to the Undercity.")
-- The player casts 269807 "Leave for Lordaeron Queue"; its spell script joins LFG 1759.
-- (SMART_ACTION_ENTER_LFG_QUEUE = 1005 does not fit in action_type, so it cannot be used.)
-- Only adds SmartAI rows 100-101, so other gossip scripts on Isabella (e.g. the skip option) keep working.
UPDATE `creature_template` SET `AIName` = 'SmartAI' WHERE `entry` = 139093;
DELETE FROM `smart_scripts` WHERE `entryorguid` = 139093 AND `source_type` = 0 AND `id` IN (100, 101);
INSERT INTO `smart_scripts` (`entryorguid`, `source_type`, `id`, `link`, `event_type`, `event_phase_mask`, `event_chance`, `event_flags`, `event_param1`, `event_param2`, `event_param3`, `event_param4`, `event_param5`, `event_param_string`, `action_type`, `action_param1`, `action_param2`, `action_param3`, `action_param4`, `action_param5`, `action_param6`, `target_type`, `target_param1`, `target_param2`, `target_param3`, `target_x`, `target_y`, `target_z`, `target_o`, `comment`) VALUES
(139093, 0, 100, 101, 62, 0, 100, 0, 19870, 1, 0, 0, 0, '', 85, 269807, 2, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 'Isabella - On Gossip Option 1 Selected - Invoker Cast Leave for Lordaeron Queue (triggered)'),
(139093, 0, 101, 0, 61, 0, 100, 0, 0, 0, 0, 0, 0, '', 72, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 'Isabella - Linked - Close Gossip');

-- The spell script that joins the LFG queue is registered in
-- 2026_11_05_00_world_battle_for_lordaeron_undercity.sql.
