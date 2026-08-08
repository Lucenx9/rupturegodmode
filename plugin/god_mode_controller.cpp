#include "god_mode_controller.h"

namespace RuptureGodMode
{
GodModeController::GodModeController(const bool enabledAtStart) noexcept : m_enabled(enabledAtStart)
{
}

bool GodModeController::IsEnabled() const noexcept
{
    return m_enabled;
}

void GodModeController::SetEnabled(const bool enabled) noexcept
{
    m_enabled = enabled;
}

void GodModeController::Toggle() noexcept
{
    m_enabled = !m_enabled;
}

ApplyResult GodModeController::Reconcile(const NetworkMode mode, const std::uintptr_t targetId,
                                         IGodModeEffects &effects)
{
    if (mode == NetworkMode::Unknown)
    {
        return ApplyResult::Unsupported;
    }
    if (targetId == 0)
    {
        return ApplyResult::Waiting;
    }
    if (!m_enabled)
    {
        if (m_applied)
        {
            if (!effects.SetEnabled(false))
            {
                return ApplyResult::Waiting;
            }
            m_applied = false;
            m_appliedTarget = 0;
            return ApplyResult::Removed;
        }
        return ApplyResult::NoChange;
    }
    if (!m_applied || m_appliedTarget != targetId)
    {
        if (!effects.SetEnabled(true))
        {
            effects.Maintain();
            return ApplyResult::Waiting;
        }
        m_applied = true;
        m_appliedTarget = targetId;
        effects.Maintain();
        return ApplyResult::Applied;
    }

    effects.Maintain();
    return ApplyResult::Maintained;
}

ApplyResult GodModeController::RemoveAppliedEffects(IGodModeEffects &effects)
{
    if (!m_applied)
    {
        return ApplyResult::NoChange;
    }
    if (!effects.SetEnabled(false))
    {
        return ApplyResult::Waiting;
    }
    m_applied = false;
    m_appliedTarget = 0;
    return ApplyResult::Removed;
}

void GodModeController::ForgetTarget() noexcept
{
    m_applied = false;
    m_appliedTarget = 0;
}
} // namespace RuptureGodMode
