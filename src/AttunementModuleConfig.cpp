#include "AttunementModuleConfig.h"

namespace cmangos_module
{
    AttunementModuleConfig::AttunementModuleConfig()
    : ModuleConfig("attunement.conf")
    , enabled(false)
    , defaultRate(1.0f)
    , minRate(0.1f)
    , maxRate(0.0f) // 0 = uncapped
    , auraTier1SpellId(0)
    , auraTier2SpellId(0)
    , auraTier3SpellId(0)
    , auraTier4SpellId(0)
    , auraTier5SpellId(0)
    {
    }

    bool AttunementModuleConfig::OnLoad()
    {
        enabled          = config.GetBoolDefault("Attunement.Enable", false);
        defaultRate      = config.GetFloatDefault("Attunement.DefaultRate", 1.0f);
        minRate          = config.GetFloatDefault("Attunement.MinRate", 0.1f);
        maxRate          = config.GetFloatDefault("Attunement.MaxRate", 0.0f);
        auraTier1SpellId = config.GetIntDefault("Attunement.Aura.Tier1.SpellId", 0);
        auraTier2SpellId = config.GetIntDefault("Attunement.Aura.Tier2.SpellId", 0);
        auraTier3SpellId = config.GetIntDefault("Attunement.Aura.Tier3.SpellId", 0);
        auraTier4SpellId = config.GetIntDefault("Attunement.Aura.Tier4.SpellId", 0);
        auraTier5SpellId = config.GetIntDefault("Attunement.Aura.Tier5.SpellId", 0);
        return true;
    }
}
