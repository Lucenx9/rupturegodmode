#include "immortality_state.h"

namespace RuptureGodMode
{
bool ImmortalityState::SetEnabled(const bool enabled, IImmortalityToggle &nativeToggle)
{
    if (m_enabled == enabled)
    {
        return true;
    }
    if (!nativeToggle.ToggleImmortality())
    {
        return false;
    }
    m_enabled = enabled;
    return true;
}

bool ImmortalityState::IsEnabled() const noexcept
{
    return m_enabled;
}
} // namespace RuptureGodMode
