#pragma once
#include "ModuleConfig.h"

namespace cmangos_module
{
    class AttunementModuleConfig : public ModuleConfig
    {
    public:
        AttunementModuleConfig();
        bool OnLoad() override;

        bool enabled;
        float defaultRate;
        float minRate;
        float maxRate;

        // Visible aura per rate bucket (0 disables aura for that bucket).
        // Buckets: 1× (default, no aura), >1×–2×, >2×–5×, >5×–25×, >25×.
        uint32 auraTier1SpellId;
        uint32 auraTier2SpellId;
        uint32 auraTier3SpellId;
        uint32 auraTier4SpellId;
        uint32 auraTier5SpellId;
    };
}
