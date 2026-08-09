#include "god_mode_controller.h"
#include "immortality_state.h"

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace
{
using RuptureGodMode::ApplyResult;
using RuptureGodMode::GodModeController;
using RuptureGodMode::IGodModeEffects;
using RuptureGodMode::IImmortalityToggle;
using RuptureGodMode::ImmortalityState;
using RuptureGodMode::NetworkMode;

class FakeEffects final : public IGodModeEffects
{
  public:
    bool SetEnabled(const bool enabled) override
    {
        ++setEnabledCalls;
        lastEnabled = enabled;
        return setEnabledSucceeds;
    }

    void Maintain() override
    {
        ++maintainCalls;
    }

    int setEnabledCalls = 0;
    int maintainCalls = 0;
    bool lastEnabled = false;
    bool setEnabledSucceeds = true;
};

class FakeImmortalityToggle final : public IImmortalityToggle
{
  public:
    bool ToggleImmortality() override
    {
        ++toggleCalls;
        if (!toggleSucceeds)
        {
            return false;
        }
        immortal = !immortal;
        return true;
    }

    void ApplyLethalDamage()
    {
        if (!immortal)
        {
            alive = false;
        }
    }

    int toggleCalls = 0;
    bool toggleSucceeds = true;
    bool immortal = false;
    bool alive = true;
};

void Require(const bool condition, const std::string_view message)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

void EnabledAtStartAppliesOnceAndMaintainsEveryTick()
{
    GodModeController controller(true);
    FakeEffects effects;

    const ApplyResult first = controller.Reconcile(NetworkMode::Standalone, 101, effects);
    const ApplyResult second = controller.Reconcile(NetworkMode::Standalone, 101, effects);

    Require(first == ApplyResult::Applied, "first valid tick applies God Mode");
    Require(second == ApplyResult::Maintained, "later valid tick maintains God Mode");
    Require(effects.setEnabledCalls == 1, "God Mode is enabled only once for one player");
    Require(effects.lastEnabled, "the applied state is enabled");
    Require(effects.maintainCalls == 2, "vitals are maintained on every valid tick");
}

void FailedApplicationIsRetried()
{
    GodModeController controller(true);
    FakeEffects effects;
    effects.setEnabledSucceeds = false;

    const ApplyResult failed = controller.Reconcile(NetworkMode::Standalone, 202, effects);
    effects.setEnabledSucceeds = true;
    const ApplyResult retried = controller.Reconcile(NetworkMode::Standalone, 202, effects);

    Require(failed == ApplyResult::Waiting, "failed application remains waiting");
    Require(retried == ApplyResult::Applied, "a later tick retries application");
    Require(effects.setEnabledCalls == 2, "failed application is not cached as active");
    Require(effects.maintainCalls == 2, "vitals are maintained while native activation is retried");
}

void ToggleOffRemovesGodModeOnce()
{
    GodModeController controller(true);
    FakeEffects effects;

    controller.Reconcile(NetworkMode::Standalone, 303, effects);
    controller.Toggle();
    const ApplyResult removed = controller.Reconcile(NetworkMode::Standalone, 303, effects);
    const ApplyResult idle = controller.Reconcile(NetworkMode::Standalone, 303, effects);

    Require(removed == ApplyResult::Removed, "first disabled tick removes God Mode");
    Require(idle == ApplyResult::NoChange, "later disabled ticks are idle");
    Require(effects.setEnabledCalls == 2, "enable and disable are each applied once");
    Require(!effects.lastEnabled, "the final applied state is disabled");
    Require(effects.maintainCalls == 1, "disabled God Mode stops maintaining vitals");
}

void RemoteClientMaintainsItsLocalPrediction()
{
    GodModeController controller(true);
    FakeEffects effects;

    const ApplyResult remote = controller.Reconcile(NetworkMode::RemoteClient, 404, effects);
    const ApplyResult maintained = controller.Reconcile(NetworkMode::RemoteClient, 404, effects);

    Require(remote == ApplyResult::Applied, "remote multiplayer applies its client-side path");
    Require(maintained == ApplyResult::Maintained, "remote multiplayer maintains local vitals");
    Require(effects.setEnabledCalls == 1, "remote setup is idempotent");
    Require(effects.maintainCalls == 2, "remote client maintains vitals every frame");
}

void UnknownNetworkModeIsNeverMutated()
{
    GodModeController controller(true);
    FakeEffects effects;

    const ApplyResult result = controller.Reconcile(NetworkMode::Unknown, 405, effects);

    Require(result == ApplyResult::Unsupported, "unknown network state is unsupported");
    Require(effects.setEnabledCalls == 0, "unknown network state changes no effects");
    Require(effects.maintainCalls == 0, "unknown network state changes no vitals");
}

void UnknownTransitionPreservesOwnershipForCleanup()
{
    GodModeController controller(true);
    FakeEffects effects;

    controller.Reconcile(NetworkMode::Standalone, 406, effects);
    controller.Reconcile(NetworkMode::Unknown, 406, effects);
    const ApplyResult removed = controller.RemoveAppliedEffects(effects);

    Require(removed == ApplyResult::Removed,
            "an unknown travel state does not lose ownership of native effects");
    Require(effects.setEnabledCalls == 2, "cleanup still disables effects after travel");
}

void RespawnReappliesGodModeToTheNewPawn()
{
    GodModeController controller(true);
    FakeEffects effects;

    controller.Reconcile(NetworkMode::Standalone, 501, effects);
    const ApplyResult respawned = controller.Reconcile(NetworkMode::Standalone, 502, effects);

    Require(respawned == ApplyResult::Applied, "a new pawn receives God Mode");
    Require(effects.setEnabledCalls == 2, "respawn reapplies the native effects");
    Require(effects.maintainCalls == 2, "the replacement pawn is normalized immediately");
}

void ListenServerHostIsSupported()
{
    GodModeController controller(true);
    FakeEffects effects;

    const ApplyResult result = controller.Reconcile(NetworkMode::ListenServer, 601, effects);

    Require(result == ApplyResult::Applied, "the listen-server host can use God Mode");
    Require(effects.setEnabledCalls == 1, "the host applies native effects");
    Require(effects.maintainCalls == 1, "the host maintains vitals");
}

void FailedRemovalIsRetried()
{
    GodModeController controller(true);
    FakeEffects effects;

    controller.Reconcile(NetworkMode::Standalone, 701, effects);
    controller.SetEnabled(false);
    effects.setEnabledSucceeds = false;
    const ApplyResult failed = controller.Reconcile(NetworkMode::Standalone, 701, effects);
    effects.setEnabledSucceeds = true;
    const ApplyResult retried = controller.Reconcile(NetworkMode::Standalone, 701, effects);

    Require(failed == ApplyResult::Waiting, "failed removal remains pending");
    Require(retried == ApplyResult::Removed, "a later tick retries removal");
    Require(effects.setEnabledCalls == 3, "failed removal is not cached as complete");
    Require(effects.maintainCalls == 1, "removal attempts never maintain vitals");
}

void WorldCleanupRemovesOnlyOwnedEffectsAndPreservesPreference()
{
    GodModeController enabledController(true);
    FakeEffects enabledEffects;
    enabledController.Reconcile(NetworkMode::Standalone, 801, enabledEffects);

    const ApplyResult removed = enabledController.RemoveAppliedEffects(enabledEffects);
    const ApplyResult reapplied =
        enabledController.Reconcile(NetworkMode::Standalone, 802, enabledEffects);

    Require(removed == ApplyResult::Removed, "world cleanup removes applied native effects");
    Require(reapplied == ApplyResult::Applied, "the next world retains the enabled preference");

    GodModeController disabledController(false);
    FakeEffects disabledEffects;
    const ApplyResult untouched = disabledController.RemoveAppliedEffects(disabledEffects);

    Require(untouched == ApplyResult::NoChange, "cleanup ignores effects it does not own");
    Require(disabledEffects.setEnabledCalls == 0, "cleanup never disables another owner's cheats");
}

void ImmortalityBlocksLethalDamageWithoutDoubleTogglingOnRespawn()
{
    ImmortalityState state;
    FakeImmortalityToggle native;

    Require(state.SetEnabled(true, native), "native immortality can be enabled");
    native.ApplyLethalDamage();
    Require(native.alive, "native immortality blocks a lethal damage event");

    Require(state.SetEnabled(true, native), "respawn can reapply God Mode safely");
    Require(native.toggleCalls == 1, "respawn does not toggle native immortality off");
    Require(native.immortal, "native immortality remains active after respawn");

    Require(state.SetEnabled(false, native), "native immortality can be disabled");
    Require(native.toggleCalls == 2, "disable owns exactly one matching native toggle");
    native.ApplyLethalDamage();
    Require(!native.alive, "lethal damage is effective again after disabling God Mode");
}

void FailedImmortalityToggleIsRetried()
{
    ImmortalityState state;
    FakeImmortalityToggle native;
    native.toggleSucceeds = false;

    Require(!state.SetEnabled(true, native), "a failed native toggle reports failure");
    native.toggleSucceeds = true;
    Require(state.SetEnabled(true, native), "a failed native toggle is retried");
    Require(native.toggleCalls == 2, "failure is not cached as active immortality");
    Require(native.immortal, "the successful retry enables native immortality");
}
} // namespace

int main()
{
    EnabledAtStartAppliesOnceAndMaintainsEveryTick();
    FailedApplicationIsRetried();
    ToggleOffRemovesGodModeOnce();
    RemoteClientMaintainsItsLocalPrediction();
    UnknownNetworkModeIsNeverMutated();
    UnknownTransitionPreservesOwnershipForCleanup();
    RespawnReappliesGodModeToTheNewPawn();
    ListenServerHostIsSupported();
    FailedRemovalIsRetried();
    WorldCleanupRemovesOnlyOwnedEffectsAndPreservesPreference();
    ImmortalityBlocksLethalDamageWithoutDoubleTogglingOnRespawn();
    FailedImmortalityToggleIsRetried();
    std::cout << "god_mode_controller_tests: PASS\n";
    return 0;
}
