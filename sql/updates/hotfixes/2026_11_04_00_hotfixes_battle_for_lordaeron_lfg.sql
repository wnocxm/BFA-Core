-- The Battle for Lordaeron (Horde), LFG 1759
-- LFGDungeons.db2 has MaxLevel 0 for this entry, so the LFG system says
-- "You do not meet the requirements for the chosen dungeons." to every player.
-- This hotfix row overrides the DB2 record with MaxLevel 120; every other
-- value is copied from LFGDungeons.db2 (build 8.3.7).
-- Targets the hotfixes database by name (bfa_hotfixes); change it if yours is named differently.
DELETE FROM `bfa_hotfixes`.`lfg_dungeons` WHERE `ID` = 1759;
INSERT INTO `bfa_hotfixes`.`lfg_dungeons` (`ID`, `Name`, `Description`, `MinLevel`, `MaxLevel`, `TypeID`, `Subtype`, `Faction`, `IconTextureFileID`, `RewardsBgTextureFileID`, `PopupBgTextureFileID`, `ExpansionLevel`, `MapID`, `DifficultyID`, `MinGear`, `GroupID`, `OrderIndex`, `RequiredPlayerConditionId`, `TargetLevel`, `TargetLevelMin`, `TargetLevelMax`, `RandomID`, `ScenarioID`, `FinalEncounterID`, `CountTank`, `CountHealer`, `CountDamage`, `MinCountTank`, `MinCountHealer`, `MinCountDamage`, `BonusReputationAmount`, `MentorItemLevel`, `MentorCharLevel`, `Flags1`, `Flags2`, `VerifiedBuild`) VALUES
(1759, 'The Battle for Lordaeron', 'Join Warchief Sylvanas Windrunner in the battle for Lordaeron Keep.', 110, 120, 1, 3, -1, 136349, 615223, 615222, 0, 1760, 12, 0, 38, 10, 61673, 110, 110, 0, 0, 1482, 0, 0, 0, 20, 0, 0, 1, 0, 0, 0, 1279265795, 2, 0);
