#include "AttunementModule.h"
#include "Database/DatabaseEnv.h"
#include "Entities/Player.h"
#include "Entities/GossipDef.h"
#include "AI/ScriptDevAI/include/sc_gossip.h"
#include "Server/DBCStores.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace cmangos_module
{
    enum AttunementActions
    {
        ACTION_MAIN_MENU       = 100,
        ACTION_RESET_DEFAULT   = 101,
        ACTION_CUSTOM_INPUT    = 102,

        // Preset rate picks. Action codes encode rate × 100, offset by base.
        // e.g. 0.5× -> 50, 1× -> 100, 5× -> 500, 100× -> 10000.
        ACTION_RATE_BASE       = 1000,
    };

    static const uint32 NPC_ENTRY_ATTUNEMENT = 190014;
    static const uint32 NPC_TEXT_GREETING    = 50930;
    static const uint32 NPC_TEXT_CUSTOM      = 50931;

    // Preset rates surfaced as gossip options. Operators can extend the
    // list freely; the custom input branch covers anything in-between.
    static const float PRESET_RATES[] = { 0.5f, 1.0f, 2.0f, 3.0f, 5.0f, 10.0f, 25.0f, 50.0f, 100.0f };
    static const size_t PRESET_RATES_COUNT = sizeof(PRESET_RATES) / sizeof(PRESET_RATES[0]);

    static bool IsAttunementNPC(Creature* creature)
    {
        return creature && creature->GetEntry() == NPC_ENTRY_ATTUNEMENT;
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
            m_playerRates.erase(player->GetGUIDLow());
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
            player->CastSpell(player, picked, true);
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
            if (rate <= 0.0f)
            {
                player->GetSession()->SendNotification("Invalid rate. Enter a positive number such as 1.5 or 7.");
                playerMenu->CloseGossip();
                return true;
            }
            SetXpRate(player, rate);
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
