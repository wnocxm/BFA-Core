-- The Battle for Lordaeron: link map 1760 to its scenarios
-- difficulty 12 = DIFFICULTY_NORMAL_SCENARIO
-- 1371 = The Siege of Lordaeron (Alliance), 1482 = The Fall of Lordaeron (Horde)
DELETE FROM `scenarios` WHERE `map` = 1760;
INSERT INTO `scenarios` (`map`, `difficulty`, `scenario_A`, `scenario_H`) VALUES
(1760, 12, 1371, 1482);
