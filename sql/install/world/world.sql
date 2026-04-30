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
  -- Coords anchor on the canonical race-start NPC in each zone, offset
  -- a few units so the Attuner doesn't overlap the existing creature.
  -- Northshire Abbey (near Marshal McBride)         map 0
  (@AttunementEntry, 0, 1, -8902.6, -158.6,   82.0, 3.14, 300, 300, 0, 0),
  -- Coldridge Valley (near Sten Stoutarm)            map 0
  (@AttunementEntry, 0, 1, -6214.9,  332.2,  383.7, 3.14, 300, 300, 0, 0),
  -- Deathknell (near Shadow Priest Sarvis)           map 0
  (@AttunementEntry, 0, 1,  1843.3, 1643.9,   97.8, 3.14, 300, 300, 0, 0),
  -- Shadowglen (near Conservator Ilthalaine)         map 1
  (@AttunementEntry, 1, 1, 10328.9,  830.1, 1326.5, 3.14, 300, 300, 0, 0),
  -- Valley of Trials (near Kaltunk)                  map 1
  (@AttunementEntry, 1, 1,  -607.4,-4247.3,   39.0, 3.14, 300, 300, 0, 0),
  -- Camp Narache (near Grull Hawkwind)               map 1
  (@AttunementEntry, 1, 1, -2912.7, -253.5,   53.0, 3.14, 300, 300, 0, 0);

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
