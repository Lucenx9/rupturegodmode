#pragma once

#include "god_mode_controller.h"
#include "immortality_state.h"

namespace SDK
{
class ACrCharacterPlayerBase;
class ACrPlayerControllerBase;
} // namespace SDK

namespace RuptureGodMode
{
class StarRuptureEffects final : public IGodModeEffects
{
  public:
    StarRuptureEffects(SDK::ACrPlayerControllerBase *controller,
                       SDK::ACrCharacterPlayerBase *player, bool allowNativeCheats,
                       float safeTemperature, ImmortalityState *immortalityState) noexcept;

    bool SetEnabled(bool enabled) override;
    void Maintain() override;

  private:
    SDK::ACrPlayerControllerBase *m_controller;
    SDK::ACrCharacterPlayerBase *m_player;
    bool m_allowNativeCheats;
    float m_safeTemperature;
    ImmortalityState *m_immortalityState;
};
} // namespace RuptureGodMode
