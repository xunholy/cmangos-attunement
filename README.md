# cmangos-attunement

Per-player opt-in XP rate module for [CMaNGOS Classic](https://github.com/cmangos/mangos-classic).

Players visit the **Attuner of Paths** NPC at any starting zone to set their personal XP multiplier. Quest, kill, and exploration XP all funnel through cmangos' `OnPreGiveXP` hook so the multiplier applies uniformly. A configurable visible aura tracks the player's current rate so other players can see the choice — social signaling instead of a hidden cheat console.

## Build

Clone this repo into the cmangos source tree:

```sh
git clone https://github.com/xunholy/cmangos-attunement.git src/modules/attunement
```

Register the module in `src/modules/modules/CMakeLists.txt` (or the equivalent module loader for your fork) — same pattern as other custom modules.

CMake flags consumed:
- `-DENABLE_ATTUNEMENT` — defined automatically by this module's `CMakeLists.txt`.

## Configure

Copy `attunement.conf.dist` to `attunement.conf` next to your `mangosd.conf` and set:

```ini
Attunement.Enable = 1
Attunement.DefaultRate = 1.0
Attunement.MinRate = 0.1
Attunement.MaxRate = 0          # 0 = uncapped
```

Optional visible-aura tiers (operator-chosen spell IDs):

```ini
Attunement.Aura.Tier1.SpellId = 0       # 1.0× (default — no aura)
Attunement.Aura.Tier2.SpellId = 0       # >1× to 2×
Attunement.Aura.Tier3.SpellId = 0       # >2× to 5×
Attunement.Aura.Tier4.SpellId = 0       # >5× to 25×
Attunement.Aura.Tier5.SpellId = 0       # >25×
```

Pick spell IDs whose visuals match the flavor you want (any positive aura with a permanent or long-duration effect works). Setting a tier to `0` disables the aura for that bucket.

## Database

Apply the migrations:

```sh
mariadb -u mangos -pmangos classiccharacters < sql/install/characters/characters.sql
mariadb -u mangos -pmangos classicmangos    < sql/install/world/world.sql
```

The world script creates a single NPC entry (`190014`, "Attuner of Paths") with a friendly faction and spawns it in:

| Zone               | Map | Race(s)             |
|--------------------|-----|---------------------|
| Northshire Abbey   | 0   | Human               |
| Coldridge Valley   | 0   | Dwarf, Gnome        |
| Deathknell         | 0   | Undead              |
| Shadowglen         | 1   | Night Elf           |
| Valley of Trials   | 1   | Orc, Troll          |
| Camp Narache       | 1   | Tauren              |

Spawn coordinates are approximate; adjust the `creature` rows if you want different placement.

## How it works

- `OnLoadFromDB` reads the player's saved rate from `custom_attunement_player_config` and applies the matching tier aura.
- `OnPreGiveXP` multiplies all XP gain by the saved rate (default `1.0` if no row exists).
- The gossip menu offers preset rates plus a `Custom rate...` option that opens a chat input box — the entered string is parsed as a float.
- Setting the rate back to the default deletes the row (rather than storing `1.0`), keeping the table sparse.

## Schema

The config table is generic so future opt-ins (e.g., new toggle keys) can be added without migrations:

```sql
CREATE TABLE custom_attunement_player_config (
  guid       INT UNSIGNED NOT NULL,
  option_key VARCHAR(64)  NOT NULL,
  value      FLOAT        NOT NULL DEFAULT 0,
  updated_at DATETIME     DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (guid, option_key)
);
```

## Scope

Intentionally narrow: **only** XP rate. Other rates (reputation, money drop, loot drop, skill-up) impact the shared economy and are better left as global server settings.
