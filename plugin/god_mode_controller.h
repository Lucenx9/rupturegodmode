#pragma once

#include <cstdint>

namespace RuptureGodMode
{
enum class NetworkMode
{
    Unknown,
    Standalone,
    ListenServer,
    RemoteClient,
};

enum class ApplyResult
{
    Applied,
    Maintained,
    Removed,
    Waiting,
    Unsupported,
    NoChange,
};

class IGodModeEffects
{
  public:
    virtual ~IGodModeEffects() = default;
    virtual bool SetEnabled(bool enabled) = 0;
    virtual void Maintain() = 0;
};

class GodModeController final
{
  public:
    explicit GodModeController(bool enabledAtStart) noexcept;

    [[nodiscard]] bool IsEnabled() const noexcept;
    void SetEnabled(bool enabled) noexcept;
    void Toggle() noexcept;
    ApplyResult Reconcile(NetworkMode mode, std::uintptr_t targetId, IGodModeEffects &effects);
    ApplyResult RemoveAppliedEffects(IGodModeEffects &effects);
    void ForgetTarget() noexcept;

  private:
    bool m_enabled;
    bool m_applied = false;
    std::uintptr_t m_appliedTarget = 0;
};
} // namespace RuptureGodMode
