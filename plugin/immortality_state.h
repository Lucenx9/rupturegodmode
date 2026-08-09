#pragma once

namespace RuptureGodMode
{
class IImmortalityToggle
{
  public:
    virtual ~IImmortalityToggle() = default;
    virtual bool ToggleImmortality() = 0;
};

class ImmortalityState final
{
  public:
    bool SetEnabled(bool enabled, IImmortalityToggle &nativeToggle);
    [[nodiscard]] bool IsEnabled() const noexcept;

  private:
    bool m_enabled = false;
};
} // namespace RuptureGodMode
