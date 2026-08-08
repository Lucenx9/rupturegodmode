#pragma once

#include "god_mode_controller.h"

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
                       float safeTemperature) noexcept;

    bool SetEnabled(bool enabled) override;
    void Maintain() override;

  private:
    SDK::ACrPlayerControllerBase *m_controller;
    SDK::ACrCharacterPlayerBase *m_player;
    bool m_allowNativeCheats;
    float m_safeTemperature;
};
} // namespace RuptureGodMode
