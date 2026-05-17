#include "AttunementModule.h"
#include "Chat/Chat.h"
#include "Database/DatabaseEnv.h"
#include "Entities/Player.h"
#include "Entities/GossipDef.h"
#include "AI/ScriptDevAI/include/sc_gossip.h"
#include "Globals/ObjectAccessor.h"
#include "Server/DBCStores.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace cmangos_module
{
    enum AttunementActions
    {
        ACTION_MAIN_MENU       = 100,
        ACTION_RESET_DEFAULT   = 101,
        ACTION_CUSTOM_INPUT    = 102,
        ACTION_BOOST_TO_MAX    = 103,

        // Preset rate picks. Action codes encode rate × 100, offset by base.
        // e.g. 1× -> 100, 5× -> 500, 10× -> 1000, 100× -> 10000.
        ACTION_RATE_BASE       = 10000,
    };

    static const uint32 NPC_ENTRY_ATTUNEMENT = 190014;
    static const uint32 NPC_TEXT_GREETING    = 50930;
    static const uint32 NPC_TEXT_CUSTOM      = 50931;

    static const uint32 ATTUNEMENT_MAX_LEVEL = 60;
    static const uint32 BOOST_GOLD_COPPER    = 500 * 10000; // 500 gold

    // Preset rates surfaced as gossip options. Custom branch covers anything
    // in-between; the level-boost action is a separate branch.
    static const float PRESET_RATES[] = { 1.0f, 2.0f, 5.0f, 10.0f };
    static const size_t PRESET_RATES_COUNT = sizeof(PRESET_RATES) / sizeof(PRESET_RATES[0]);

    // Tier 0 ("dungeon set 1") starter sets per Classic class.
    // 8 pieces each: head, shoulders, chest, wrists, hands, waist, legs, feet.
    // Terminator 0 keeps iteration simple.
    static const uint32 T0_WARRIOR[] = {16730, 16731, 16732, 16733, 16734, 16735, 16736, 16737, 0};
    static const uint32 T0_PALADIN[] = {16722, 16723, 16724, 16725, 16726, 16727, 16728, 16729, 0};
    static const uint32 T0_HUNTER[]  = {16674, 16675, 16676, 16677, 16678, 16679, 16680, 16681, 0};
    static const uint32 T0_ROGUE[]   = {16707, 16708, 16709, 16710, 16711, 16712, 16713, 16721, 0};
    static const uint32 T0_PRIEST[]  = {16690, 16691, 16692, 16693, 16694, 16695, 16696, 16697, 0};
    // Shaman D1 "The Elements" — full 8-piece blue mail set (matches the
    // other classes' T0 sets in quality and item-level).
    static const uint32 T0_SHAMAN[]  = {
        16667, // Coif of Elements (head)
        16669, // Pauldrons of Elements (shoulders)
        16666, // Vest of Elements (chest)
        16668, // Kilt of Elements (legs)
        16670, // Boots of Elements (feet)
        16671, // Bindings of Elements (wrists)
        16672, // Gauntlets of Elements (hands)
        16673, // Cord of Elements (waist)
        0
    };
    static const uint32 T0_MAGE[]    = {16682, 16683, 16684, 16685, 16686, 16687, 16688, 16689, 0};
    static const uint32 T0_WARLOCK[] = {16698, 16699, 16700, 16701, 16702, 16703, 16704, 16705, 0};
    static const uint32 T0_DRUID[]   = {16706, 16714, 16715, 16716, 16717, 16718, 16719, 16720, 0};

    static const uint32* GetTierZeroSet(uint32 classId)
    {
        switch (classId)
        {
            case CLASS_WARRIOR: return T0_WARRIOR;
            case CLASS_PALADIN: return T0_PALADIN;
            case CLASS_HUNTER:  return T0_HUNTER;
            case CLASS_ROGUE:   return T0_ROGUE;
            case CLASS_PRIEST:  return T0_PRIEST;
            case CLASS_SHAMAN:  return T0_SHAMAN;
            case CLASS_MAGE:    return T0_MAGE;
            case CLASS_WARLOCK: return T0_WARLOCK;
            case CLASS_DRUID:   return T0_DRUID;
            default:            return nullptr;
        }
    }

    // Class-specific weapons / off-hands / ranged. All blue (Q3) level 55-60.
    // mainHand / offHand / ranged; 0 means slot is empty for this class.
    struct ClassWeapons { uint32 mainHand; uint32 offHand; uint32 ranged; };

    // Demonshear (2H sword), Hammer of the Titans (2H mace), Eaglehorn Long Bow,
    // Heartseeker (dagger), Fang of the Crystal Spider (dagger), Skullforge Reaver (1H sword),
    // Draconian Deflector (shield), Staff of Hale Magefire, Banshee Finger (wand),
    // Rod of the Ogre Magi (staff), Oblivion's Touch (wand), Whiteout Staff.
    static const ClassWeapons WEAPONS_WARRIOR  = { 13348,     0,     0 };
    static const ClassWeapons WEAPONS_PALADIN  = { 13361, 12602,     0 };
    static const ClassWeapons WEAPONS_HUNTER   = { 13348,     0, 13023 };
    static const ClassWeapons WEAPONS_ROGUE    = { 12783, 13218,     0 };
    static const ClassWeapons WEAPONS_PRIEST   = { 13000,     0, 13534 };
    static const ClassWeapons WEAPONS_SHAMAN   = { 12796,     0,     0 };
    static const ClassWeapons WEAPONS_MAGE     = { 18534,     0, 18761 };
    static const ClassWeapons WEAPONS_WARLOCK  = { 18534,     0, 18761 };
    static const ClassWeapons WEAPONS_DRUID    = { 19101,     0,     0 };

    static const ClassWeapons* GetClassWeapons(uint32 classId)
    {
        switch (classId)
        {
            case CLASS_WARRIOR: return &WEAPONS_WARRIOR;
            case CLASS_PALADIN: return &WEAPONS_PALADIN;
            case CLASS_HUNTER:  return &WEAPONS_HUNTER;
            case CLASS_ROGUE:   return &WEAPONS_ROGUE;
            case CLASS_PRIEST:  return &WEAPONS_PRIEST;
            case CLASS_SHAMAN:  return &WEAPONS_SHAMAN;
            case CLASS_MAGE:    return &WEAPONS_MAGE;
            case CLASS_WARLOCK: return &WEAPONS_WARLOCK;
            case CLASS_DRUID:   return &WEAPONS_DRUID;
            default:            return nullptr;
        }
    }

    // Weapon-skill IDs per class (null-terminated). Lifted from twinkmaster.
    static const uint16 WEAPON_SKILLS_WARRIOR[] = { 43, 44, 45, 46, 54, 55, 136, 160, 162, 172, 173, 176, 226, 229, 473, 0 };
    static const uint16 WEAPON_SKILLS_PALADIN[] = { 43, 44, 54, 55, 160, 162, 229, 0 };
    static const uint16 WEAPON_SKILLS_HUNTER[]  = { 43, 44, 45, 46, 55, 136, 162, 172, 173, 176, 226, 229, 473, 0 };
    static const uint16 WEAPON_SKILLS_ROGUE[]   = { 43, 45, 46, 54, 162, 173, 176, 226, 473, 0 };
    static const uint16 WEAPON_SKILLS_PRIEST[]  = { 54, 136, 162, 173, 228, 0 };
    static const uint16 WEAPON_SKILLS_SHAMAN[]  = { 44, 54, 136, 160, 162, 172, 173, 473, 0 };
    static const uint16 WEAPON_SKILLS_MAGE[]    = { 43, 136, 162, 173, 228, 0 };
    static const uint16 WEAPON_SKILLS_WARLOCK[] = { 43, 136, 162, 173, 228, 0 };
    static const uint16 WEAPON_SKILLS_DRUID[]   = { 54, 136, 160, 162, 173, 473, 0 };

    static const uint16 SKILL_DEFENSE = 95;

    static const uint16* GetWeaponSkillsForClass(uint8 classId)
    {
        switch (classId)
        {
            case CLASS_WARRIOR: return WEAPON_SKILLS_WARRIOR;
            case CLASS_PALADIN: return WEAPON_SKILLS_PALADIN;
            case CLASS_HUNTER:  return WEAPON_SKILLS_HUNTER;
            case CLASS_ROGUE:   return WEAPON_SKILLS_ROGUE;
            case CLASS_PRIEST:  return WEAPON_SKILLS_PRIEST;
            case CLASS_SHAMAN:  return WEAPON_SKILLS_SHAMAN;
            case CLASS_MAGE:    return WEAPON_SKILLS_MAGE;
            case CLASS_WARLOCK: return WEAPON_SKILLS_WARLOCK;
            case CLASS_DRUID:   return WEAPON_SKILLS_DRUID;
            default:            return nullptr;
        }
    }

    // SpellFamilyName per class (Blizzard mapping). Used to filter
    // npc_trainer_template hits to spells that actually belong to this class.
    static const uint32 CLASS_SPELL_FAMILY[] = {
        0, 4, 10, 9, 8, 6, 0, 11, 3, 5, 0, 7
    };

    // Per-class boost accessories. 6 fixed slots:
    //   0 = cloak, 1 = neck, 2 = ring, 3 = ring, 4 = trinket, 5 = trinket.
    // 0 leaves the slot empty (GiveItem no-ops on 0). All picks are blue
    // Q3, lvl 55-60. The previous single class-agnostic list handed
    // Primalist's Seal (caster ring) to Rogues, Hakkari Loa Cloak
    // (healing cloak) to Warriors, etc. — split per class so each
    // archetype receives stat-appropriate gear.
    //
    // Physical pool (AP / agi / crit):
    //   12968 Cape of the Black Baron     +6 agi  +9 sta
    //   12846 Mark of Fordring            +26 AP  +4 def
    //   17713 Painweaver Band             +18 AP  +1% crit
    //   13098 Don Julio's Band            +4 AP   +1% crit
    //   11815 Hand of Justice             +20 AP  +2% chance extra attack
    //   18370 Vigilance Charm             +14 sta
    //   13382 Cannonball Runner           on-use ranged dmg (hunter-only)
    //
    // Caster pool (int / healing / spell power):
    //   19870 Hakkari Loa Cloak           +18 healing  +8 spell dmg
    //   19871 Talisman of Protection      +14 sta neck
    //   19863 Primalist's Seal            +12 sta +12 int +healing/sd
    //   18404 Underworld Band             +8 int   +11 healing / +5 sd
    //   18820 Talisman of Ephemeral Power on-use +40 spell dmg (15s)
    static const size_t ACCESSORY_SLOTS = 6;

    static const uint32 ACCESSORIES_WARRIOR[ACCESSORY_SLOTS] = {12968, 12846, 17713, 13098, 11815, 18370};
    static const uint32 ACCESSORIES_PALADIN[ACCESSORY_SLOTS] = {12968, 12846, 17713, 13098, 11815, 18370};
    static const uint32 ACCESSORIES_HUNTER[ACCESSORY_SLOTS]  = {12968, 12846, 17713, 13098, 11815, 13382};
    static const uint32 ACCESSORIES_ROGUE[ACCESSORY_SLOTS]   = {12968, 12846, 17713, 13098, 11815, 18370};
    static const uint32 ACCESSORIES_PRIEST[ACCESSORY_SLOTS]  = {19870, 19871, 19863, 18404, 18820, 18370};
    static const uint32 ACCESSORIES_SHAMAN[ACCESSORY_SLOTS]  = {12968, 12846, 17713, 13098, 11815, 18370};
    static const uint32 ACCESSORIES_MAGE[ACCESSORY_SLOTS]    = {19870, 19871, 19863, 18404, 18820, 18370};
    static const uint32 ACCESSORIES_WARLOCK[ACCESSORY_SLOTS] = {19870, 19871, 19863, 18404, 18820, 18370};
    static const uint32 ACCESSORIES_DRUID[ACCESSORY_SLOTS]   = {12968, 12846, 17713, 13098, 11815, 18370};

    static const uint32* GetBoostAccessories(uint32 classId)
    {
        switch (classId)
        {
            case CLASS_WARRIOR: return ACCESSORIES_WARRIOR;
            case CLASS_PALADIN: return ACCESSORIES_PALADIN;
            case CLASS_HUNTER:  return ACCESSORIES_HUNTER;
            case CLASS_ROGUE:   return ACCESSORIES_ROGUE;
            case CLASS_PRIEST:  return ACCESSORIES_PRIEST;
            case CLASS_SHAMAN:  return ACCESSORIES_SHAMAN;
            case CLASS_MAGE:    return ACCESSORIES_MAGE;
            case CLASS_WARLOCK: return ACCESSORIES_WARLOCK;
            case CLASS_DRUID:   return ACCESSORIES_DRUID;
            default:            return nullptr;
        }
    }

    static bool IsAttunementNPC(Creature* creature)
    {
        return creature && creature->GetEntry() == NPC_ENTRY_ATTUNEMENT;
    }

    static void GiveItem(Player* player, uint32 itemId)
    {
        if (!itemId)
            return;

        // Prefer auto-equip so a fresh character with a 16-slot backpack
        // doesn't run out of room before all items are granted.
        uint16 eqDest = 0;
        InventoryResult eqRes = player->CanEquipNewItem(NULL_SLOT, eqDest, itemId, false);
        if (eqRes == EQUIP_ERR_OK)
        {
            if (Item* item = player->EquipNewItem(eqDest, itemId, true))
            {
                player->AutoUnequipOffhandIfNeed();
                player->SendNewItem(item, 1, true, false);
                return;
            }
        }

        // Fall back to bag storage if not equippable (or no slot available).
        ItemPosCountVec dest;
        InventoryResult res = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, itemId, 1);
        if (res != EQUIP_ERR_OK)
            return;
        if (Item* item = player->StoreNewItem(dest, itemId, true))
            player->SendNewItem(item, 1, true, false);
    }

    AttunementModule::AttunementModule()
    : Module("Attunement", new AttunementModuleConfig())
    {
    }

    const AttunementModuleConfig* AttunementModule::GetConfig() const
    {
        return (const AttunementModuleConfig*)Module::GetConfig();
    }

    bool AttunementModule::IsEnabled() const
    {
        return GetConfig()->enabled;
    }

    float AttunementModule::GetXpRate(uint32 guid) const
    {
        auto it = m_playerRates.find(guid);
        return it != m_playerRates.end() ? it->second : GetConfig()->defaultRate;
    }

    float AttunementModule::ClampRate(float rate) const
    {
        const AttunementModuleConfig* cfg = GetConfig();
        if (!std::isfinite(rate) || rate <= 0.0f)
            return cfg->defaultRate;
        if (rate < cfg->minRate)
            rate = cfg->minRate;
        if (cfg->maxRate > 0.0f && rate > cfg->maxRate)
            rate = cfg->maxRate;
        return rate;
    }

    uint32 AttunementModule::PickAuraSpell(float rate) const
    {
        const AttunementModuleConfig* cfg = GetConfig();
        if (rate <= 1.0f)  return cfg->auraTier1SpellId;
        if (rate <= 2.0f)  return cfg->auraTier2SpellId;
        if (rate <= 5.0f)  return cfg->auraTier3SpellId;
        if (rate <= 25.0f) return cfg->auraTier4SpellId;
        return cfg->auraTier5SpellId;
    }

    void AttunementModule::OnInitialize()
    {
    }

    void AttunementModule::LearnWeaponSkills(Player* player, uint32 targetLevel)
    {
        uint16 maxSkill = static_cast<uint16>(targetLevel * 5);

        if (const uint16* skills = GetWeaponSkillsForClass(player->getClass()))
            for (const uint16* s = skills; *s; ++s)
                player->SetSkill(*s, maxSkill, maxSkill);

        player->SetSkill(SKILL_DEFENSE, maxSkill, maxSkill);
    }

    void AttunementModule::LearnClassSpells(Player* player, uint32 targetLevel)
    {
        uint32 classId = player->getClass();
        uint32 expectedFamily = (classId < 12) ? CLASS_SPELL_FAMILY[classId] : 0;

        auto result = WorldDatabase.PQuery(
            "SELECT DISTINCT ntt.spell FROM npc_trainer_template ntt "
            "JOIN creature_template ct ON ct.trainertemplateid = ntt.entry "
            "WHERE ct.trainer_class = %u AND ntt.reqlevel <= %u AND ntt.reqlevel > 0",
            classId, targetLevel);

        if (!result)
            return;

        do
        {
            Field* fields = result->Fetch();
            uint32 spellId = fields[0].GetUInt32();

            const SpellEntry* spell = sSpellTemplate.LookupEntry<SpellEntry>(spellId);
            if (!spell)
                continue;

            // Skip spells that belong to a different class family
            if (spell->SpellFamilyName != 0 && spell->SpellFamilyName != expectedFamily)
                continue;

            if (!player->HasSpell(spellId))
                player->learnSpell(spellId, false);
        } while (result->NextRow());
    }

    void AttunementModule::OnLoadFromDB(Player* player)
    {
        if (!IsEnabled() || !player)
            return;

        uint32 guid = player->GetGUIDLow();

        auto result = CharacterDatabase.PQuery(
            "SELECT `value` FROM `custom_attunement_player_config` "
            "WHERE `guid` = %u AND `option_key` = 'xp_rate'", guid);

        if (result)
        {
            Field* fields = result->Fetch();
            float rate = ClampRate(fields[0].GetFloat());
            m_playerRates[guid] = rate;
            RefreshAura(player, rate);
        }
    }

    void AttunementModule::OnLogOut(Player* player)
    {
        if (player)
        {
            uint32 guid = player->GetGUIDLow();
            m_playerRates.erase(guid);
            m_lastSelection.erase(guid);
        }
    }

    void AttunementModule::OnDeleteFromDB(uint32 playerId)
    {
        // Wipe all per-player config when the character is deleted so a
        // recycled GUID doesn't inherit stale flags (e.g. 'boosted').
        CharacterDatabase.PExecute(
            "DELETE FROM `custom_attunement_player_config` WHERE `guid` = %u",
            playerId);
    }

    void AttunementModule::OnRegenerate(Player* player, uint8 /*power*/, uint32 /*diff*/, float& /*addedValue*/)
    {
        if (!IsEnabled() || !player || !player->IsInWorld())
            return;

        uint32 inspectorGuid = player->GetGUIDLow();
        ObjectGuid currentSel = player->GetSelectionGuid();

        auto it = m_lastSelection.find(inspectorGuid);
        ObjectGuid lastSel = (it != m_lastSelection.end()) ? it->second : ObjectGuid();

        if (currentSel == lastSel)
            return;
        m_lastSelection[inspectorGuid] = currentSel;

        if (currentSel.IsEmpty() || currentSel == player->GetObjectGuid())
            return;

        Player* target = sObjectAccessor.FindPlayer(currentSel);
        if (!target)
            return;

        float rate = GetXpRate(target->GetGUIDLow());
        if (rate == GetConfig()->defaultRate)
            return;

        ChatHandler(player).PSendSysMessage(
            "|cff1eff00[Attunement]|r %s walks the path of %.2gx XP.",
            target->GetName(), rate);
    }

    bool AttunementModule::OnPreGiveXP(Player* player, uint32& xp, Creature* /*victim*/)
    {
        if (!IsEnabled() || !player || xp == 0)
            return false;

        float rate = GetXpRate(player->GetGUIDLow());
        if (rate == 1.0f)
            return false;

        // Round to nearest. cmangos returns false to mean "did not override";
        // mutating xp in-place and returning false lets the engine continue
        // with the multiplied value while preserving downstream hooks.
        double scaled = static_cast<double>(xp) * static_cast<double>(rate);
        if (scaled < 0.0)
            scaled = 0.0;
        if (scaled > static_cast<double>(UINT32_MAX))
            scaled = static_cast<double>(UINT32_MAX);
        xp = static_cast<uint32>(scaled + 0.5);
        return false;
    }

    void AttunementModule::SetXpRate(Player* player, float rate)
    {
        if (!player)
            return;

        rate = ClampRate(rate);
        uint32 guid = player->GetGUIDLow();

        if (rate == GetConfig()->defaultRate)
        {
            CharacterDatabase.PExecute(
                "DELETE FROM `custom_attunement_player_config` "
                "WHERE `guid` = %u AND `option_key` = 'xp_rate'", guid);
            m_playerRates.erase(guid);
        }
        else
        {
            CharacterDatabase.PExecute(
                "REPLACE INTO `custom_attunement_player_config` (`guid`, `option_key`, `value`) "
                "VALUES (%u, 'xp_rate', %f)", guid, rate);
            m_playerRates[guid] = rate;
        }

        RefreshAura(player, rate);

        char msg[128];
        snprintf(msg, sizeof(msg), "XP rate set to %.2fx.", rate);
        player->GetSession()->SendNotification("%s", msg);
    }

    void AttunementModule::RefreshAura(Player* player, float rate)
    {
        if (!player)
            return;

        const AttunementModuleConfig* cfg = GetConfig();
        const uint32 allTierSpells[] = {
            cfg->auraTier1SpellId,
            cfg->auraTier2SpellId,
            cfg->auraTier3SpellId,
            cfg->auraTier4SpellId,
            cfg->auraTier5SpellId,
        };

        for (uint32 spellId : allTierSpells)
            if (spellId != 0)
                player->RemoveAurasDueToSpell(spellId);

        uint32 picked = PickAuraSpell(rate);
        if (picked != 0)
            player->CastSpell(player, picked, TRIGGERED_OLD_TRIGGERED);
    }

    bool AttunementModule::OnPreGossipHello(Player* player, Creature* creature)
    {
        if (!IsEnabled() || !player || !IsAttunementNPC(creature))
            return false;

        PlayerMenu* playerMenu = player->GetPlayerMenu();
        if (!playerMenu)
            return false;

        playerMenu->ClearMenus();

        float current = GetXpRate(player->GetGUIDLow());

        char header[128];
        snprintf(header, sizeof(header), "Current XP rate: %.2fx", current);
        playerMenu->GetGossipMenu().AddMenuItem(GOSSIP_ICON_CHAT, header, GOSSIP_SENDER_MAIN, ACTION_MAIN_MENU, "", false);

        for (size_t i = 0; i < PRESET_RATES_COUNT; ++i)
        {
            char label[64];
            snprintf(label, sizeof(label), "Set rate to %.2gx", PRESET_RATES[i]);
            uint32 action = ACTION_RATE_BASE + static_cast<uint32>(PRESET_RATES[i] * 100.0f + 0.5f);
            playerMenu->GetGossipMenu().AddMenuItem(GOSSIP_ICON_BATTLE, label, GOSSIP_SENDER_MAIN, action, "", false);
        }

        playerMenu->GetGossipMenu().AddMenuItem(GOSSIP_ICON_INTERACT_1, "Custom rate...", GOSSIP_SENDER_MAIN, ACTION_CUSTOM_INPUT, "", true);

        if (player->GetLevel() < ATTUNEMENT_MAX_LEVEL)
        {
            char boostLabel[64];
            snprintf(boostLabel, sizeof(boostLabel), "Boost me to level %u", ATTUNEMENT_MAX_LEVEL);
            playerMenu->GetGossipMenu().AddMenuItem(GOSSIP_ICON_BATTLE, boostLabel, GOSSIP_SENDER_MAIN, ACTION_BOOST_TO_MAX, "", false);
        }

        if (current != GetConfig()->defaultRate)
            playerMenu->GetGossipMenu().AddMenuItem(GOSSIP_ICON_INTERACT_2, "Reset to default", GOSSIP_SENDER_MAIN, ACTION_RESET_DEFAULT, "", false);

        playerMenu->SendGossipMenu(NPC_TEXT_GREETING, creature->GetObjectGuid());
        return true;
    }

    bool AttunementModule::OnGossipSelect(Player* player, Creature* creature, uint32 /*sender*/, uint32 action, const std::string& code, uint32 /*gossipListId*/)
    {
        if (!IsEnabled() || !player || !IsAttunementNPC(creature))
            return false;

        PlayerMenu* playerMenu = player->GetPlayerMenu();
        if (!playerMenu)
            return false;

        playerMenu->ClearMenus();

        if (action == ACTION_MAIN_MENU)
        {
            OnPreGossipHello(player, creature);
            return true;
        }

        if (action == ACTION_RESET_DEFAULT)
        {
            SetXpRate(player, GetConfig()->defaultRate);
            playerMenu->CloseGossip();
            return true;
        }

        if (action == ACTION_CUSTOM_INPUT)
        {
            float rate = static_cast<float>(std::atof(code.c_str()));
            if (!std::isfinite(rate) || rate <= 0.0f)
            {
                player->GetSession()->SendNotification("Invalid rate. Enter a positive number such as 1.5 or 7.");
                playerMenu->CloseGossip();
                return true;
            }
            SetXpRate(player, rate);
            playerMenu->CloseGossip();
            return true;
        }

        if (action == ACTION_BOOST_TO_MAX)
        {
            uint32 guid = player->GetGUIDLow();

            // One-shot per character. Re-clicking is a no-op.
            auto already = CharacterDatabase.PQuery(
                "SELECT 1 FROM `custom_attunement_player_config` "
                "WHERE `guid` = %u AND `option_key` = 'boosted'", guid);
            if (already)
            {
                player->GetSession()->SendNotification("You have already received the boost.");
                playerMenu->CloseGossip();
                return true;
            }

            if (player->GetLevel() < ATTUNEMENT_MAX_LEVEL)
            {
                player->GiveLevel(ATTUNEMENT_MAX_LEVEL);
                player->InitTalentForLevel();
                player->SetUInt32Value(PLAYER_XP, 0);
            }

            // Teach trainer spells (Mail/Plate Specialization, etc.) and cap
            // weapon skills so the auto-equip path that follows doesn't fail
            // with EQUIP_ERR_PROFICIENCY_NEEDED.
            LearnClassSpells(player, ATTUNEMENT_MAX_LEVEL);
            LearnWeaponSkills(player, ATTUNEMENT_MAX_LEVEL);

            player->ModifyMoney(BOOST_GOLD_COPPER);

            uint32 classId = player->getClass();

            // Tier 0 dungeon set (8 pieces of head/shoulders/chest/wrists/hands/waist/legs/feet)
            if (const uint32* gear = GetTierZeroSet(classId))
                for (size_t i = 0; gear[i] != 0; ++i)
                    GiveItem(player, gear[i]);

            // Class-appropriate accessories (cloak, neck, 2 rings, 2 trinkets).
            // Fixed-length walk so empty slots (0) don't terminate iteration.
            if (const uint32* accessories = GetBoostAccessories(classId))
                for (size_t i = 0; i < ACCESSORY_SLOTS; ++i)
                    GiveItem(player, accessories[i]);

            // Class-appropriate weapons (main / off / ranged)
            if (const ClassWeapons* w = GetClassWeapons(classId))
            {
                GiveItem(player, w->mainHand);
                GiveItem(player, w->offHand);
                GiveItem(player, w->ranged);
            }

            CharacterDatabase.PExecute(
                "REPLACE INTO `custom_attunement_player_config` (`guid`, `option_key`, `value`) "
                "VALUES (%u, 'boosted', 1)", guid);

            // Force a save immediately so level + gear + money persist together
            // with the boosted flag. Otherwise an early disconnect leaves the
            // boosted flag (direct SQL) committed but the in-memory level
            // change unsaved, locking the character out of re-boosting.
            player->SaveToDB();

            player->GetSession()->SendNotification("Boosted to 60. 500 gold and starter gear equipped.");
            playerMenu->CloseGossip();
            return true;
        }

        if (action >= ACTION_RATE_BASE)
        {
            float rate = static_cast<float>(action - ACTION_RATE_BASE) / 100.0f;
            SetXpRate(player, rate);
            playerMenu->CloseGossip();
            return true;
        }

        playerMenu->CloseGossip();
        return true;
    }
}
