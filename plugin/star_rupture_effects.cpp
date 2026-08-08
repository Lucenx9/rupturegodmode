#include "star_rupture_effects.h"

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#endif
#include "Chimera_classes.hpp"
#include "Engine_classes.hpp"
#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include <cmath>

namespace RuptureGodMode
{
namespace
{
float UsableLimit(const SDK::FGameplayAttributeData &limit, const float fallback) noexcept
{
    if (std::isfinite(limit.CurrentValue))
    {
        return limit.CurrentValue;
    }
    if (std::isfinite(limit.BaseValue))
    {
        return limit.BaseValue;
    }
    return fallback;
}

void SetToLimit(SDK::FGameplayAttributeData &current,
                const SDK::FGameplayAttributeData &limit) noexcept
{
    const float value = UsableLimit(limit, current.CurrentValue);
    current.BaseValue = value;
    current.CurrentValue = value;
}

SDK::UCrCheatManager *GetCheatManager(SDK::ACrPlayerControllerBase *controller,
                                      const bool createIfMissing)
{
    if (controller == nullptr)
    {
        return nullptr;
    }
    if (controller->CheatManager == nullptr && createIfMissing)
    {
        controller->EnableCheats();
    }
    if (controller->CheatManager == nullptr ||
        !controller->CheatManager->IsA(SDK::UCrCheatManager::StaticClass()))
    {
        return nullptr;
    }
    return static_cast<SDK::UCrCheatManager *>(controller->CheatManager);
}

void ApplyNativeCheats(SDK::UCrCheatManager &cheats, const bool enabled)
{
    const int state = enabled ? 1 : 0;
    cheats.UnlimitedHealth(state);
    cheats.UnlimitedEnergy(state);
    cheats.UnlimitedShield(state);
    cheats.UnlimitedOxygen(state);
    cheats.UnlimitedHydration(state);
    cheats.UnlimitedCalories(state);
    cheats.UnlimitedMedTool(state);
    cheats.UnlimitedGrenades(state);
    cheats.UnlimitedAmmo(state);
    cheats.UnlimitedWeaponHeat(state);
    cheats.RestrictedDrain(state);
    cheats.RestrictedRadiation(state);
    cheats.RestrictedTemperature(state);
    cheats.RestrictedToxicity(state);
}
} // namespace

StarRuptureEffects::StarRuptureEffects(SDK::ACrPlayerControllerBase *controller,
                                       SDK::ACrCharacterPlayerBase *player,
                                       const bool allowNativeCheats) noexcept
    : m_controller(controller), m_player(player), m_allowNativeCheats(allowNativeCheats)
{
}

bool StarRuptureEffects::SetEnabled(const bool enabled)
{
    if (!m_allowNativeCheats)
    {
        return true;
    }
    SDK::UCrCheatManager *cheats = GetCheatManager(m_controller, enabled);
    if (cheats == nullptr)
    {
        return !enabled;
    }
    ApplyNativeCheats(*cheats, enabled);
    return true;
}

void StarRuptureEffects::Maintain()
{
    if (m_player == nullptr)
    {
        return;
    }

    if (m_player->HealthAttributes != nullptr)
    {
        SetToLimit(m_player->HealthAttributes->CurrentHealth,
                   m_player->HealthAttributes->MaxHealth);
    }
    if (m_player->EnergyAttributes != nullptr)
    {
        SetToLimit(m_player->EnergyAttributes->CurrentEnergy,
                   m_player->EnergyAttributes->MaxEnergy);
    }
    if (m_player->ShieldAttributes != nullptr)
    {
        SetToLimit(m_player->ShieldAttributes->CurrentShield,
                   m_player->ShieldAttributes->MaxShield);
    }
    if (m_player->OxygenAttributes != nullptr)
    {
        SetToLimit(m_player->OxygenAttributes->CurrentOxygen,
                   m_player->OxygenAttributes->MaxOxygen);
    }
    if (m_player->HydrationAttributes != nullptr)
    {
        SetToLimit(m_player->HydrationAttributes->CurrentHydration,
                   m_player->HydrationAttributes->MaxHydration);
    }
    if (m_player->CaloriesAttributes != nullptr)
    {
        SetToLimit(m_player->CaloriesAttributes->CurrentCalories,
                   m_player->CaloriesAttributes->MaxCalories);
    }
    if (m_player->MedToolChargeAttributes != nullptr)
    {
        SetToLimit(m_player->MedToolChargeAttributes->CurrentMedToolCharge,
                   m_player->MedToolChargeAttributes->MaxMedToolCharge);
    }
    if (m_player->GrenadeChargeAttributes != nullptr)
    {
        SetToLimit(m_player->GrenadeChargeAttributes->CurrentGrenadeCharge,
                   m_player->GrenadeChargeAttributes->MaxGrenadeCharge);
    }

    if (m_player->ToxicityAttributes != nullptr)
    {
        SetToLimit(m_player->ToxicityAttributes->CurrentToxicity,
                   m_player->ToxicityAttributes->MinToxicity);
    }
    if (m_player->RadiationAttributes != nullptr)
    {
        SetToLimit(m_player->RadiationAttributes->CurrentRadiation,
                   m_player->RadiationAttributes->MinRadiation);
    }
    if (m_player->HeatAttributes != nullptr)
    {
        SetToLimit(m_player->HeatAttributes->CurrentHeat, m_player->HeatAttributes->MinHeat);
    }
    if (m_player->DrainAttributes != nullptr)
    {
        SetToLimit(m_player->DrainAttributes->CurrentDrain, m_player->DrainAttributes->MinDrain);
    }
    if (m_player->CorrosionAttributes != nullptr)
    {
        SetToLimit(m_player->CorrosionAttributes->CurrentCorrosion,
                   m_player->CorrosionAttributes->MinCorrosion);
    }
    if (m_player->InfectionAttributes != nullptr)
    {
        SetToLimit(m_player->InfectionAttributes->CurrentInfection,
                   m_player->InfectionAttributes->MinInfection);
    }
}
} // namespace RuptureGodMode
