#ifndef CMANGOS_MODULE_ATTUNEMENT_H
#define CMANGOS_MODULE_ATTUNEMENT_H

#include "Module.h"
#include "AttunementModuleConfig.h"

#include <unordered_map>

namespace cmangos_module
{
    class AttunementModule : public Module
    {
    public:
        AttunementModule();
        const AttunementModuleConfig* GetConfig() const override;

        // Module hooks
        void OnInitialize() override;

        // Player hooks
        void OnLoadFromDB(Player* player) override;
        void OnLogOut(Player* player) override;
        bool OnPreGiveXP(Player* player, uint32& xp, Creature* victim) override;
        bool OnPreGossipHello(Player* player, Creature* creature) override;
        bool OnGossipSelect(Player* player, Creature* creature, uint32 sender, uint32 action, const std::string& code, uint32 gossipListId) override;
        void OnRegenerate(Player* player, uint8 power, uint32 diff, float& addedValue) override;

        // Public helpers
        bool IsEnabled() const;
        float GetXpRate(uint32 guid) const;

    private:
        void SetXpRate(Player* player, float rate);
        void RefreshAura(Player* player, float rate);
        uint32 PickAuraSpell(float rate) const;
        float ClampRate(float rate) const;

        // guid -> rate (only present when non-default; default is implicit)
        std::unordered_map<uint32, float> m_playerRates;

        // inspector guid -> last seen target. Used by OnRegenerate to whisper
        // an attunement readout when the inspector targets a non-default player.
        std::unordered_map<uint32, ObjectGuid> m_lastSelection;
    };
}

#endif
