-- Progressive realm (ProgressionMgr): content opens patch by patch on the 8.3.7 client.
-- Turn it on with Progression.Enable = 1 in worldserver.conf, then use .progression next / .progression set 8.1.5
-- Patches are written as major*10000 + minor*100 + build: 8.0.1 = 80001, 8.1.5 = 80105.
--
-- Numbers marked "check" are best known values. Compare them with your own data before the realm opens.

-- ---------------------------------------------------------------------------------------------
-- Patches
-- ---------------------------------------------------------------------------------------------
DROP TABLE IF EXISTS `progression_patch`;
CREATE TABLE `progression_patch` (
  `Patch` int(10) unsigned NOT NULL COMMENT 'major*10000 + minor*100 + build',
  `Name` varchar(100) NOT NULL DEFAULT '',
  `MythicPlusSeason` int(10) unsigned NOT NULL DEFAULT 0 COMMENT 'MythicPlusSeason.db2 ID, also picks the creature stat mods of ContentTuningXExpected',
  `PvpSeason` int(10) unsigned NOT NULL DEFAULT 0 COMMENT 'Arena season, overrides Arena.ArenaSeason.ID',
  `AzeriteLevelCap` int(10) unsigned NOT NULL DEFAULT 0 COMMENT 'Heart of Azeroth max level, 0 = no cap',
  `ItemLevelCap` int(10) unsigned NOT NULL DEFAULT 0 COMMENT 'Highest item level loot can have, 0 = no cap',
  PRIMARY KEY (`Patch`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COMMENT='ProgressionMgr: content patches';

-- PvP seasons 26-29 are BfA Season 1-4. MythicPlusSeason IDs: check them against MythicPlusSeason.db2.
-- AzeriteLevelCap: check.
INSERT INTO `progression_patch` (`Patch`, `Name`, `MythicPlusSeason`, `PvpSeason`, `AzeriteLevelCap`, `ItemLevelCap`) VALUES
(80001, 'Battle for Azeroth',     1, 26,  0, 385),
(80100, 'Tides of Vengeance',     2, 27,  0, 415),
(80105, 'Crucible of Storms',     2, 27,  0, 420),
(80200, 'Rise of Azshara',        3, 28, 70, 445),
(80205, 'Shadows Rising',         3, 28, 70, 445),
(80300, 'Visions of N''Zoth',     4, 29,  0, 475),
(80307, 'Visions of N''Zoth (8.3.7)', 4, 29, 0, 475);

-- ---------------------------------------------------------------------------------------------
-- Content rules. Content without a row is always open.
-- Type: 0 map, 1 quest, 2 quest zone (QuestSortID), 3 zone/area, 4 creature entry, 5 gameobject entry,
--       6 creature guid, 7 gameobject guid, 8 item, 9 race, 10 feature, 11 LFG dungeon
-- Open while MinPatch <= live patch and (MaxPatch = 0 or live patch < MaxPatch).
-- ---------------------------------------------------------------------------------------------
DROP TABLE IF EXISTS `progression_content`;
CREATE TABLE `progression_content` (
  `Type` tinyint(3) unsigned NOT NULL,
  `Entry` int(10) unsigned NOT NULL,
  `MinPatch` int(10) unsigned NOT NULL DEFAULT 0 COMMENT 'Opens in this patch',
  `MaxPatch` int(10) unsigned NOT NULL DEFAULT 0 COMMENT 'Closes in this patch, 0 = never',
  `Comment` varchar(255) NOT NULL DEFAULT '',
  PRIMARY KEY (`Type`, `Entry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COMMENT='ProgressionMgr: what opens in which patch';

INSERT INTO `progression_content` (`Type`, `Entry`, `MinPatch`, `MaxPatch`, `Comment`) VALUES
-- 8.1.0 Tides of Vengeance
(0, 2070, 80100, 0, 'Raid: Battle of Dazar''alor'),
(0, 2105, 80100, 0, 'Warfront: Battle for Darkshore'),
-- 8.1.5 Crucible of Storms
(0, 2096, 80105, 0, 'Raid: Crucible of Storms'),
(9,   31, 80105, 0, 'Allied race: Zandalari Troll'),
(9,   32, 80105, 0, 'Allied race: Kul Tiran'),
-- 8.2.0 Rise of Azshara
(0, 1718, 80200, 0, 'Zone: Nazjatar'),
(0, 2097, 80200, 0, 'Dungeon: Operation: Mechagon'),
(0, 2164, 80200, 0, 'Raid: The Eternal Palace'),
(3, 1355, 80200, 0, 'Zone: Nazjatar'),
(3, 1462, 80200, 0, 'Zone: Mechagon Island'),
(2, 1355, 80200, 0, 'Quests: Nazjatar'),
(2, 1462, 80200, 0, 'Quests: Mechagon'),
(10,   1, 80200, 0, 'Feature: Azerite essences and the Heart Forge'),
-- 8.3.0 Visions of N'Zoth
(0, 2217, 80300, 0, 'Raid: Ny''alotha, the Waking City'),
(0, 2212, 80300, 0, 'Horrific Vision of Orgrimmar'),
(0, 2213, 80300, 0, 'Horrific Vision of Stormwind'),
(3, 1527, 80300, 0, 'Zone: Uldum (N''Zoth assault version) - check'),
(3, 1530, 80300, 0, 'Zone: Vale of Eternal Blossoms (N''Zoth assault version) - check'),
(2, 1527, 80300, 0, 'Quests: Uldum assaults - check'),
(2, 1530, 80300, 0, 'Quests: Vale assaults - check'),
(8, 169223, 80300, 0, 'Item: Ashjra''kamas, Shroud of Resolve (legendary cloak)'),
(9,   35, 80300, 0, 'Allied race: Vulpera'),
(9,   37, 80300, 0, 'Allied race: Mechagnome'),
(10,   2, 80300, 0, 'Feature: Heart of Azeroth weekly catch-up'),
(10,   3, 80300, 0, 'Feature: Corruption');

-- ---------------------------------------------------------------------------------------------
-- Loot item levels per patch. The newest row that is not newer than the live patch is used.
-- MapID 0 = every Battle for Azeroth dungeon/raid without its own row.
-- DifficultyID: 1 normal, 2 heroic, 23 mythic, 8 mythic keystone, 17 LFR, 14 normal raid, 15 heroic raid, 16 mythic raid
-- ---------------------------------------------------------------------------------------------
DROP TABLE IF EXISTS `progression_item_level`;
CREATE TABLE `progression_item_level` (
  `Patch` int(10) unsigned NOT NULL,
  `MapID` int(10) unsigned NOT NULL DEFAULT 0 COMMENT '0 = every BfA dungeon/raid',
  `DifficultyID` int(10) unsigned NOT NULL,
  `ItemLevel` int(10) unsigned NOT NULL,
  `Comment` varchar(255) NOT NULL DEFAULT '',
  PRIMARY KEY (`Patch`, `MapID`, `DifficultyID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COMMENT='ProgressionMgr: loot item level per patch';

INSERT INTO `progression_item_level` (`Patch`, `MapID`, `DifficultyID`, `ItemLevel`, `Comment`) VALUES
-- BfA dungeons (Season 1-2 values; 8.2 and 8.3: check)
(80001, 0,  1, 310, 'Dungeon normal'),
(80001, 0,  2, 325, 'Dungeon heroic'),
(80001, 0, 23, 340, 'Dungeon mythic'),
(80001, 0,  8, 340, 'Mythic keystone boss loot'),
(80100, 0,  1, 340, 'Dungeon normal'),
(80100, 0,  2, 355, 'Dungeon heroic'),
(80100, 0, 23, 370, 'Dungeon mythic'),
(80100, 0,  8, 370, 'Mythic keystone boss loot'),
(80200, 0,  1, 355, 'Dungeon normal - check'),
(80200, 0,  2, 370, 'Dungeon heroic - check'),
(80200, 0, 23, 385, 'Dungeon mythic - check'),
(80200, 0,  8, 385, 'Mythic keystone boss loot - check'),
(80300, 0,  1, 380, 'Dungeon normal - check'),
(80300, 0,  2, 395, 'Dungeon heroic - check'),
(80300, 0, 23, 410, 'Dungeon mythic - check'),
(80300, 0,  8, 410, 'Mythic keystone boss loot - check'),
-- Uldir
(80001, 1861, 17, 340, 'Uldir LFR'),
(80001, 1861, 14, 355, 'Uldir normal'),
(80001, 1861, 15, 370, 'Uldir heroic'),
(80001, 1861, 16, 385, 'Uldir mythic'),
-- Battle of Dazar'alor
(80100, 2070, 17, 370, 'Battle of Dazar''alor LFR'),
(80100, 2070, 14, 385, 'Battle of Dazar''alor normal'),
(80100, 2070, 15, 400, 'Battle of Dazar''alor heroic'),
(80100, 2070, 16, 415, 'Battle of Dazar''alor mythic'),
-- Crucible of Storms (check)
(80105, 2096, 17, 375, 'Crucible of Storms LFR - check'),
(80105, 2096, 14, 390, 'Crucible of Storms normal - check'),
(80105, 2096, 15, 405, 'Crucible of Storms heroic - check'),
(80105, 2096, 16, 420, 'Crucible of Storms mythic - check'),
-- The Eternal Palace
(80200, 2164, 17, 400, 'The Eternal Palace LFR'),
(80200, 2164, 14, 415, 'The Eternal Palace normal'),
(80200, 2164, 15, 430, 'The Eternal Palace heroic'),
(80200, 2164, 16, 445, 'The Eternal Palace mythic'),
-- Ny'alotha
(80300, 2217, 17, 430, 'Ny''alotha LFR'),
(80300, 2217, 14, 445, 'Ny''alotha normal'),
(80300, 2217, 15, 460, 'Ny''alotha heroic'),
(80300, 2217, 16, 475, 'Ny''alotha mythic');

-- ---------------------------------------------------------------------------------------------
-- Mythic+ item levels per season. Keystones above the last row use the last row.
-- Season 1-2 are best known values, Season 3-4: check.
-- ---------------------------------------------------------------------------------------------
DROP TABLE IF EXISTS `progression_mythic_plus`;
CREATE TABLE `progression_mythic_plus` (
  `Season` int(10) unsigned NOT NULL COMMENT 'progression_patch.MythicPlusSeason',
  `KeystoneLevel` int(10) unsigned NOT NULL,
  `EndOfRunItemLevel` int(10) unsigned NOT NULL DEFAULT 0,
  `WeeklyItemLevel` int(10) unsigned NOT NULL DEFAULT 0,
  PRIMARY KEY (`Season`, `KeystoneLevel`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COMMENT='ProgressionMgr: Mythic+ item levels';

INSERT INTO `progression_mythic_plus` (`Season`, `KeystoneLevel`, `EndOfRunItemLevel`, `WeeklyItemLevel`) VALUES
(1,  2, 345, 355), (1,  3, 345, 355), (1,  4, 350, 360), (1,  5, 355, 365), (1,  6, 355, 370),
(1,  7, 360, 370), (1,  8, 365, 375), (1,  9, 365, 380), (1, 10, 370, 385),
(2,  2, 375, 380), (2,  3, 375, 380), (2,  4, 380, 385), (2,  5, 385, 390), (2,  6, 385, 395),
(2,  7, 390, 395), (2,  8, 395, 400), (2,  9, 395, 405), (2, 10, 400, 410),
(3,  2, 405, 410), (3,  3, 405, 415), (3,  4, 410, 420), (3,  5, 415, 425), (3,  6, 415, 425),
(3,  7, 420, 430), (3,  8, 425, 435), (3,  9, 425, 435), (3, 10, 430, 440),
(4,  2, 430, 435), (4,  3, 430, 440), (4,  4, 435, 445), (4,  5, 440, 450), (4,  6, 440, 450),
(4,  7, 445, 455), (4,  8, 450, 460), (4,  9, 450, 460), (4, 10, 455, 465);

-- ---------------------------------------------------------------------------------------------
-- Command help
-- ---------------------------------------------------------------------------------------------
DELETE FROM `command` WHERE `name` LIKE 'progression%';
INSERT INTO `command` (`name`, `permission`, `help`) VALUES
('progression',        2011, 'Syntax: .progression $subcommand\nType .progression to see the list of subcommands.'),
('progression info',   2012, 'Syntax: .progression info\nShows the live content patch, seasons and caps.'),
('progression list',   2012, 'Syntax: .progression list\nLists every content patch.'),
('progression check',  2012, 'Syntax: .progression check $type $entry\nShows when a map, quest, item, race... opens.'),
('progression set',    2013, 'Syntax: .progression set $patch\nMakes $patch (8.1.5 or 80105) the live content patch.'),
('progression next',   2013, 'Syntax: .progression next\nOpens the next content patch.'),
('progression reload', 2014, 'Syntax: .progression reload\nReloads the progression tables.');
