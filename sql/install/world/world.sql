-- ============================================================
-- Attunement NPC - "Attuner of Paths"
-- Single faction-friendly NPC entry: 190014
-- Spawns at every Classic starting zone (6 distinct zones, 8 races)
-- ============================================================

SET @AttunementEntry := 190014;

-- ------------------------------------------------------------
-- NPC Template
-- faction 35 = Friendly to all (works for both factions)
-- DisplayId 7949 = Spirit Healer-style ethereal model
-- ScriptName 'npc_attunement' is consumed by the C++ module
-- ------------------------------------------------------------
DELETE FROM `creature_template` WHERE `entry` = @AttunementEntry;
INSERT INTO `creature_template`
  (`entry`, `DisplayId1`, `DisplayIdProbability1`, `name`, `subname`, `GossipMenuId`,
   `minlevel`, `maxlevel`, `faction`, `NpcFlags`, `scale`, `rank`,
   `DamageSchool`, `MeleeBaseAttackTime`, `RangedBaseAttackTime`, `unitClass`, `unitFlags`,
   `CreatureType`, `CreatureTypeFlags`, `ScriptName`, `lootid`, `PickpocketLootId`, `SkinningLootId`,
   `AIName`, `MovementType`, `RacialLeader`, `RegenerateStats`, `MechanicImmuneMask`, `ExtraFlags`)
VALUES
  (@AttunementEntry, 7949, 100, 'Attuner of Paths', 'Adjuster of Fate', 0,
   60, 60, 35, 1, 1.1, 0,
   0, 2000, 0, 1, 0,
   7, 138936390, 'npc_attunement', 0, 0, 0,
   '', 0, 0, 1, 0, 0);

-- ------------------------------------------------------------
-- Spawns: one per Classic starting zone
-- map 0 = Eastern Kingdoms, map 1 = Kalimdor
-- Coordinates are operator-tunable; these are approximate "near
-- the first NPC trainer" positions in each newbie zone.
-- ------------------------------------------------------------
DELETE FROM `creature` WHERE `id` = @AttunementEntry;
INSERT INTO `creature`
  (`id`, `map`, `spawnMask`, `position_x`, `position_y`, `position_z`, `orientation`,
   `spawntimesecsmin`, `spawntimesecsmax`, `spawndist`, `MovementType`)
VALUES
  -- Northshire Abbey (Human start)            map 0
  (@AttunementEntry, 0, 1, -8915.0, -135.0,  82.5, 0.0, 300, 300, 0, 0),
  -- Coldridge Valley / Anvilmar (Dwarf, Gnome) map 0
  (@AttunementEntry, 0, 1, -6240.0,  331.0, 383.0, 0.0, 300, 300, 0, 0),
  -- Deathknell (Undead start)                  map 0
  (@AttunementEntry, 0, 1,  1814.0, 1593.0,  97.0, 0.0, 300, 300, 0, 0),
  -- Shadowglen / Aldrassil (Night Elf start)   map 1
  (@AttunementEntry, 1, 1, 10316.0,  836.0,1326.0, 0.0, 300, 300, 0, 0),
  -- Valley of Trials (Orc, Troll start)         map 1
  (@AttunementEntry, 1, 1,  -616.0,-4264.0,  38.0, 0.0, 300, 300, 0, 0),
  -- Camp Narache (Tauren start)                 map 1
  (@AttunementEntry, 1, 1, -2917.0, -270.0,  53.0, 0.0, 300, 300, 0, 0);

-- ------------------------------------------------------------
-- NPC Text
-- ------------------------------------------------------------
SET @TEXT_ID := 50930;
DELETE FROM `npc_text` WHERE `ID` BETWEEN @TEXT_ID AND @TEXT_ID+1;
INSERT INTO `npc_text` (`ID`, `text0_0`) VALUES
  (@TEXT_ID,
   'Some walk this world swiftly, others savor every step. I can attune the pace at which experience flows to you, $N. Pick a path - or whisper your own.'),
  (@TEXT_ID+1,
   'Whisper the rate you desire, $N. A number such as 1.5 or 7.');
