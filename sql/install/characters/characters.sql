-- ============================================================
-- Attunement: per-player option storage
-- ============================================================
-- Generic key/value table so future opt-ins can slot in without a
-- schema change. Today the only key is 'xp_rate' (FLOAT).
-- ============================================================

CREATE TABLE IF NOT EXISTS `custom_attunement_player_config` (
  `guid` int(11) unsigned NOT NULL COMMENT 'Character GUID',
  `option_key` varchar(64) NOT NULL COMMENT 'Option name (e.g. xp_rate)',
  `value` float NOT NULL DEFAULT 0 COMMENT 'Option value',
  `updated_at` datetime DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`guid`, `option_key`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_bin COMMENT='Attunement per-player options';
