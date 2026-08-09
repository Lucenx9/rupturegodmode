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
#include <utility>

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

template <typename Notify>
void SetToValue(SDK::FGameplayAttributeData &current, const float value, Notify &&notify)
{
    if (!std::isfinite(value) || (current.BaseValue == value && current.CurrentValue == value))
    {
        return;
    }
    const SDK::FGameplayAttributeData old = current;
    current.BaseValue = value;
    current.CurrentValue = value;
    notify(old);
}

template <typename Notify>
void SetToLimit(SDK::FGameplayAttributeData &current, const SDK::FGameplayAttributeData &limit,
                Notify &&notify)
{
    SetToValue(current, UsableLimit(limit, current.CurrentValue), std::forward<Notify>(notify));
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

class NativeImmortalityToggle final : public IImmortalityToggle
{
  public:
    explicit NativeImmortalityToggle(SDK::UCrCheatManager &cheats) noexcept : m_cheats(cheats)
    {
    }

    bool ToggleImmortality() override
    {
        m_cheats.Immortal();
        return true;
    }

  private:
    SDK::UCrCheatManager &m_cheats;
};
} // namespace

StarRuptureEffects::StarRuptureEffects(SDK::ACrPlayerControllerBase *controller,
                                       SDK::ACrCharacterPlayerBase *player,
                                       const bool allowNativeCheats,
                                       const float safeTemperature,
                                       ImmortalityState *immortalityState) noexcept
    : m_controller(controller), m_player(player), m_allowNativeCheats(allowNativeCheats),
      m_safeTemperature(safeTemperature), m_immortalityState(immortalityState)
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
        return !enabled &&
               (m_immortalityState == nullptr || !m_immortalityState->IsEnabled());
    }
    if (m_immortalityState == nullptr)
    {
        return false;
    }

    NativeImmortalityToggle immortality(*cheats);
    if (enabled)
    {
        if (!m_immortalityState->SetEnabled(true, immortality))
        {
            return false;
        }
        ApplyNativeCheats(*cheats, true);
        return true;
    }

    ApplyNativeCheats(*cheats, false);
    return m_immortalityState->SetEnabled(false, immortality);
}

void StarRuptureEffects::Maintain()
{
    if (m_player == nullptr)
    {
        return;
    }

    if (m_player->HealthAttributes != nullptr)
    {
        auto *attributes = m_player->HealthAttributes;
        SetToLimit(attributes->CurrentHealth, attributes->MaxHealth,
                   [attributes](const SDK::FGameplayAttributeData &old) {
                       attributes->OnRep_CurrentHealth(old);
                   });
    }
    if (m_player->EnergyAttributes != nullptr)
    {
        auto *attributes = m_player->EnergyAttributes;
        SetToLimit(attributes->CurrentEnergy, attributes->MaxEnergy,
                   [attributes](const SDK::FGameplayAttributeData &old) {
                       attributes->OnRep_CurrentEnergy(old);
                   });
    }
    if (m_player->ShieldAttributes != nullptr)
    {
        auto *attributes = m_player->ShieldAttributes;
        SetToLimit(attributes->CurrentShield, attributes->MaxShield,
                   [attributes](const SDK::FGameplayAttributeData &old) {
                       attributes->OnRep_CurrentShield(old);
                   });
    }
    if (m_player->OxygenAttributes != nullptr)
    {
        auto *attributes = m_player->OxygenAttributes;
        SetToLimit(attributes->CurrentOxygen, attributes->MaxOxygen,
                   [attributes](const SDK::FGameplayAttributeData &old) {
                       attributes->OnRep_CurrentOxygen(old);
                   });
    }
    if (m_player->HydrationAttributes != nullptr)
    {
        auto *attributes = m_player->HydrationAttributes;
        SetToLimit(attributes->CurrentHydration, attributes->MaxHydration,
                   [attributes](const SDK::FGameplayAttributeData &old) {
                       attributes->OnRep_CurrentHydration(old);
                   });
    }
    if (m_player->CaloriesAttributes != nullptr)
    {
        auto *attributes = m_player->CaloriesAttributes;
        SetToLimit(attributes->CurrentCalories, attributes->MaxCalories,
                   [attributes](const SDK::FGameplayAttributeData &old) {
                       attributes->OnRep_CurrentCalories(old);
                   });
    }
    if (m_player->MedToolChargeAttributes != nullptr)
    {
        auto *attributes = m_player->MedToolChargeAttributes;
        SetToLimit(attributes->CurrentMedToolCharge, attributes->MaxMedToolCharge,
                   [attributes](const SDK::FGameplayAttributeData &old) {
                       attributes->OnRep_CurrentMedToolCharge(old);
                   });
    }
    if (m_player->GrenadeChargeAttributes != nullptr)
    {
        auto *attributes = m_player->GrenadeChargeAttributes;
        SetToLimit(attributes->CurrentGrenadeCharge, attributes->MaxGrenadeCharge,
                   [attributes](const SDK::FGameplayAttributeData &old) {
                       attributes->OnRep_CurrentGrenadeCharge(old);
                   });
    }

    if (m_player->ToxicityAttributes != nullptr)
    {
        auto *attributes = m_player->ToxicityAttributes;
        SetToLimit(attributes->CurrentToxicity, attributes->MinToxicity,
                   [attributes](const SDK::FGameplayAttributeData &old) {
                       attributes->OnRep_CurrentToxicity(old);
                   });
    }
    if (m_player->RadiationAttributes != nullptr)
    {
        auto *attributes = m_player->RadiationAttributes;
        SetToLimit(attributes->CurrentRadiation, attributes->MinRadiation,
                   [attributes](const SDK::FGameplayAttributeData &old) {
                       attributes->OnRep_CurrentRadiation(old);
                   });
    }
    if (m_player->HeatAttributes != nullptr)
    {
        auto *attributes = m_player->HeatAttributes;
        SetToLimit(attributes->CurrentHeat, attributes->MinHeat,
                   [attributes](const SDK::FGameplayAttributeData &old) {
                       attributes->OnRep_CurrentHeat(old);
                   });
    }
    if (m_player->DrainAttributes != nullptr)
    {
        auto *attributes = m_player->DrainAttributes;
        SetToLimit(attributes->CurrentDrain, attributes->MinDrain,
                   [attributes](const SDK::FGameplayAttributeData &old) {
                       attributes->OnRep_CurrentDrain(old);
                   });
    }
    if (m_player->CorrosionAttributes != nullptr)
    {
        auto *attributes = m_player->CorrosionAttributes;
        SetToLimit(attributes->CurrentCorrosion, attributes->MinCorrosion,
                   [attributes](const SDK::FGameplayAttributeData &old) {
                       attributes->OnRep_CurrentCorrosion(old);
                   });
    }
    if (m_player->InfectionAttributes != nullptr)
    {
        auto *attributes = m_player->InfectionAttributes;
        SetToLimit(attributes->CurrentInfection, attributes->MinInfection,
                   [attributes](const SDK::FGameplayAttributeData &old) {
                       attributes->OnRep_CurrentInfection(old);
                   });
    }
    if (m_player->TemperatureAttributes != nullptr)
    {
        auto *attributes = m_player->TemperatureAttributes;
        SetToValue(attributes->CurrentTemperature, m_safeTemperature,
                   [attributes](const SDK::FGameplayAttributeData &old) {
                       attributes->OnRep_CurrentTemperature(old);
                   });
    }
}
} // namespace RuptureGodMode
