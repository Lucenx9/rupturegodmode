#define WIN32_LEAN_AND_MEAN
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#endif
#include <windows.h>

#include "plugin.h"

#include "god_mode_controller.h"
#include "star_rupture_effects.h"

#include "Chimera_classes.hpp"
#include "Engine_classes.hpp"
#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include <cmath>
#include <cstdint>
#include <iterator>
#include <limits>
#include <memory>
#include <unordered_map>

namespace
{
using RuptureGodMode::ApplyResult;
using RuptureGodMode::GodModeController;
using RuptureGodMode::NetworkMode;
using RuptureGodMode::StarRuptureEffects;

#ifndef MODLOADER_BUILD_TAG
#define MODLOADER_BUILD_TAG "dev"
#endif

constexpr float HostedRosterRefreshSeconds = 1.0f;
constexpr float NoSafeTemperature = std::numeric_limits<float>::quiet_NaN();

IPluginSelf *g_self = nullptr;
SDK::UWorld *g_world = nullptr;
std::unique_ptr<GodModeController> g_controller;
char g_toggleKey[64] = "F8";
bool g_hooksRegistered = false;
bool g_tickFailureLogged = false;
bool g_remoteModeLogged = false;
bool g_protectAllPlayersWhenHosting = true;
float g_hostedRosterAccumulator = HostedRosterRefreshSeconds;
SDK::ACrPlayerControllerBase *g_appliedLocalController = nullptr;
bool g_appliedLocalHadAuthority = false;
std::unordered_map<SDK::ACrPlayerControllerBase *, std::unique_ptr<GodModeController>>
    g_hostedPlayers;

struct TemperatureLatch
{
    SDK::ACrCharacterPlayerBase *player;
    float value;
};
std::unordered_map<SDK::ACrPlayerControllerBase *, TemperatureLatch> g_safeTemperatures;

PluginInfo g_pluginInfo = {
    "RuptureGodMode",
    MODLOADER_BUILD_TAG,
    "Lucenx9",
    "Host-authoritative God Mode with protected survival vitals",
    PLUGIN_INTERFACE_VERSION,
    PLUGIN_TARGET_CLIENT,
};

const ConfigEntry ConfigEntries[] = {
    {"General", "Enabled", ConfigValueType::Boolean, "true", "Load Rupture God Mode", 0.0f, 0.0f},
    {"GodMode", "EnabledAtStart", ConfigValueType::Boolean, "true",
     "Initial God Mode state when the plugin loads", 0.0f, 0.0f},
    {"GodMode", "ToggleKey", ConfigValueType::Keybind, "F8",
     "Enable or disable God Mode while playing", 0.0f, 0.0f},
    {"Multiplayer", "ProtectAllPlayersWhenHosting", ConfigValueType::Boolean, "true",
     "Apply authoritative God Mode to every player when you host", 0.0f, 0.0f},
};

const ConfigSchema ConfigSchemaDefinition = {
    ConfigEntries,
    static_cast<int>(std::size(ConfigEntries)),
};

void LogInfo(const char *message)
{
    if (g_self != nullptr && g_self->logger != nullptr)
    {
        g_self->logger->Info(g_self, "%s", message);
    }
}

void LogWarning(const char *message)
{
    if (g_self != nullptr && g_self->logger != nullptr)
    {
        g_self->logger->Warn(g_self, "%s", message);
    }
}

void LogError(const char *message)
{
    if (g_self != nullptr && g_self->logger != nullptr)
    {
        g_self->logger->Error(g_self, "%s", message);
    }
}

NetworkMode CurrentNetworkMode()
{
    if (g_self == nullptr || g_self->hooks == nullptr || g_self->hooks->NetMode == nullptr)
    {
        return NetworkMode::Unknown;
    }
    switch (g_self->hooks->NetMode->GetNetMode())
    {
    case EPluginNetMode::Standalone:
        return NetworkMode::Standalone;
    case EPluginNetMode::ListenServer:
        return NetworkMode::ListenServer;
    case EPluginNetMode::Client:
        return NetworkMode::RemoteClient;
    case EPluginNetMode::DedicatedServer:
    default:
        return NetworkMode::Unknown;
    }
}

bool HasAuthority(const NetworkMode mode)
{
    return mode == NetworkMode::Standalone || mode == NetworkMode::ListenServer;
}

SDK::ACrPlayerControllerBase *FindLocalController()
{
    if (g_world == nullptr)
    {
        return nullptr;
    }
    SDK::APlayerController *baseController = SDK::UGameplayStatics::GetPlayerController(g_world, 0);
    if (baseController == nullptr ||
        !baseController->IsA(SDK::ACrPlayerControllerBase::StaticClass()))
    {
        return nullptr;
    }
    return static_cast<SDK::ACrPlayerControllerBase *>(baseController);
}

SDK::ACrCharacterPlayerBase *FindPlayerForController(SDK::ACrPlayerControllerBase *controller)
{
    if (controller == nullptr)
    {
        return nullptr;
    }
    SDK::APawn *pawn = controller->K2_GetPawn();
    if (pawn == nullptr || !pawn->IsA(SDK::ACrCharacterPlayerBase::StaticClass()))
    {
        return nullptr;
    }
    return static_cast<SDK::ACrCharacterPlayerBase *>(pawn);
}

bool FindLocalPlayer(SDK::ACrPlayerControllerBase *&controller,
                     SDK::ACrCharacterPlayerBase *&player)
{
    controller = FindLocalController();
    player = FindPlayerForController(controller);
    return controller != nullptr && player != nullptr;
}

float RememberSafeTemperature(SDK::ACrPlayerControllerBase *controller,
                              SDK::ACrCharacterPlayerBase *player)
{
    if (controller == nullptr || player == nullptr || player->TemperatureAttributes == nullptr)
    {
        return NoSafeTemperature;
    }
    const float current = player->TemperatureAttributes->CurrentTemperature.CurrentValue;
    if (!std::isfinite(current))
    {
        return NoSafeTemperature;
    }
    const auto found = g_safeTemperatures.find(controller);
    if (found == g_safeTemperatures.end() || found->second.player != player)
    {
        g_safeTemperatures.insert_or_assign(controller, TemperatureLatch{player, current});
        return current;
    }
    return found->second.value;
}

void TrackHostedController(SDK::ACrPlayerControllerBase *controller)
{
    if (controller == nullptr || g_controller == nullptr)
    {
        return;
    }
    g_hostedPlayers.try_emplace(controller,
                                std::make_unique<GodModeController>(g_controller->IsEnabled()));
}

void RefreshHostedPlayers()
{
    if (g_world == nullptr)
    {
        return;
    }
    SDK::TArray<SDK::AActor *> actors;
    SDK::UGameplayStatics::GetAllActorsOfClass(g_world, SDK::ACrCharacterPlayerBase::StaticClass(),
                                               &actors);
    for (SDK::AActor *actor : actors)
    {
        if (actor == nullptr || !actor->IsA(SDK::ACrCharacterPlayerBase::StaticClass()))
        {
            continue;
        }
        auto *player = static_cast<SDK::ACrCharacterPlayerBase *>(actor);
        SDK::AController *baseController = player->GetController();
        if (baseController != nullptr &&
            baseController->IsA(SDK::ACrPlayerControllerBase::StaticClass()))
        {
            TrackHostedController(static_cast<SDK::ACrPlayerControllerBase *>(baseController));
        }
    }
}

void DisableCurrentPlayerEffects()
{
    SDK::ACrPlayerControllerBase *controller = g_appliedLocalController;
    if (controller == nullptr)
    {
        controller = FindLocalController();
    }
    if (g_controller != nullptr && controller != nullptr)
    {
        StarRuptureEffects effects(controller, FindPlayerForController(controller),
                                   g_appliedLocalHadAuthority, NoSafeTemperature);
        g_controller->RemoveAppliedEffects(effects);
    }
    if (g_controller != nullptr)
    {
        g_controller->ForgetTarget();
    }
    g_appliedLocalController = nullptr;
    g_appliedLocalHadAuthority = false;
}

void DisableHostedPlayerEffects()
{
    for (auto &[controller, state] : g_hostedPlayers)
    {
        StarRuptureEffects effects(controller, FindPlayerForController(controller), true,
                                   NoSafeTemperature);
        state->RemoveAppliedEffects(effects);
    }
}

void OnPlayerJoined(void *rawController)
{
    if (rawController == nullptr)
    {
        return;
    }
    auto *controller = static_cast<SDK::ACrPlayerControllerBase *>(rawController);
    if (controller->IsA(SDK::ACrPlayerControllerBase::StaticClass()))
    {
        TrackHostedController(controller);
    }
}

void OnPlayerLeft(void *rawController)
{
    auto *controller = static_cast<SDK::ACrPlayerControllerBase *>(rawController);
    const auto found = g_hostedPlayers.find(controller);
    if (found == g_hostedPlayers.end())
    {
        return;
    }
    SDK::ACrCharacterPlayerBase *player = FindPlayerForController(controller);
    StarRuptureEffects effects(controller, player, true, NoSafeTemperature);
    found->second->RemoveAppliedEffects(effects);
    g_safeTemperatures.erase(controller);
    g_hostedPlayers.erase(found);
}

void OnWorldBeginPlay(SDK::UWorld *world)
{
    g_world = world;
    g_remoteModeLogged = false;
    g_hostedRosterAccumulator = HostedRosterRefreshSeconds;
    if (g_controller != nullptr)
    {
        g_controller->ForgetTarget();
    }
}

void OnWorldEndPlay(SDK::UWorld *world, const char *)
{
    if (world != g_world)
    {
        return;
    }
    DisableCurrentPlayerEffects();
    DisableHostedPlayerEffects();
    g_hostedPlayers.clear();
    g_safeTemperatures.clear();
    g_world = nullptr;
}

void OnEngineTick(const float deltaSeconds)
{
    if (g_world == nullptr || g_controller == nullptr)
    {
        return;
    }

    try
    {
        const NetworkMode mode = CurrentNetworkMode();
        if (mode == NetworkMode::RemoteClient)
        {
            if (!g_remoteModeLogged)
            {
                LogWarning("Multiplayer client protection is active locally; install the plugin "
                           "on the host too for server-authoritative protection");
                g_remoteModeLogged = true;
            }
        }
        else
        {
            g_remoteModeLogged = false;
        }

        SDK::ACrPlayerControllerBase *controller = nullptr;
        SDK::ACrCharacterPlayerBase *player = nullptr;
        const bool hasPlayer = FindLocalPlayer(controller, player);
        const float safeTemperature = RememberSafeTemperature(controller, player);
        StarRuptureEffects effects(controller, player, HasAuthority(mode), safeTemperature);
        const ApplyResult result = g_controller->Reconcile(
            mode, hasPlayer ? reinterpret_cast<std::uintptr_t>(player) : 0, effects);
        if (result == ApplyResult::Applied)
        {
            g_appliedLocalController = controller;
            g_appliedLocalHadAuthority = HasAuthority(mode);
            LogInfo("God Mode enabled");
        }
        else if (result == ApplyResult::Removed)
        {
            g_appliedLocalController = nullptr;
            g_appliedLocalHadAuthority = false;
            LogInfo("God Mode disabled");
        }

        if (mode == NetworkMode::ListenServer && g_protectAllPlayersWhenHosting)
        {
            if (deltaSeconds > 0.0f && deltaSeconds <= 5.0f)
            {
                g_hostedRosterAccumulator += deltaSeconds;
            }
            else
            {
                g_hostedRosterAccumulator = HostedRosterRefreshSeconds;
            }
            if (g_hostedRosterAccumulator >= HostedRosterRefreshSeconds)
            {
                g_hostedRosterAccumulator = 0.0f;
                RefreshHostedPlayers();
            }

            for (auto &[hostedController, state] : g_hostedPlayers)
            {
                if (hostedController == controller)
                {
                    continue;
                }
                SDK::ACrCharacterPlayerBase *hostedPlayer =
                    FindPlayerForController(hostedController);
                state->SetEnabled(g_controller->IsEnabled());
                const float hostedSafeTemperature =
                    RememberSafeTemperature(hostedController, hostedPlayer);
                StarRuptureEffects hostedEffects(hostedController, hostedPlayer, true,
                                                 hostedSafeTemperature);
                state->Reconcile(NetworkMode::ListenServer,
                                 reinterpret_cast<std::uintptr_t>(hostedPlayer), hostedEffects);
            }
        }
        g_tickFailureLogged = false;
    }
    catch (...)
    {
        if (!g_tickFailureLogged)
        {
            LogError("God Mode tick failed; it will retry on the next frame");
            g_tickFailureLogged = true;
        }
    }
}

void ToggleGodMode(EModKey, EModKeyEvent)
{
    if (g_controller == nullptr)
    {
        return;
    }
    g_controller->Toggle();
    LogInfo(g_controller->IsEnabled() ? "God Mode requested: ON" : "God Mode requested: OFF");
}
} // namespace

extern "C"
{
    __declspec(dllexport) PluginInfo *GetPluginInfo()
    {
        return &g_pluginInfo;
    }

    __declspec(dllexport) bool PluginInit(IPluginSelf *self)
    {
        try
        {
            g_self = self;
            if (self == nullptr || self->hooks == nullptr || self->config == nullptr)
            {
                return false;
            }
            self->config->InitializeFromSchema(self, &ConfigSchemaDefinition);
            if (!self->config->ReadBool(self, "General", "Enabled", true))
            {
                LogInfo("Plugin is disabled in config");
                return true;
            }
            if (self->hooks->Engine == nullptr || self->hooks->World == nullptr ||
                self->hooks->Input == nullptr || self->hooks->NetMode == nullptr)
            {
                LogError("Required game, world, input, or network hooks are unavailable");
                return false;
            }

            const bool enabledAtStart =
                self->config->ReadBool(self, "GodMode", "EnabledAtStart", true);
            self->config->ReadString(self, "GodMode", "ToggleKey", g_toggleKey,
                                     static_cast<int>(std::size(g_toggleKey)), "F8");
            g_protectAllPlayersWhenHosting =
                self->config->ReadBool(self, "Multiplayer", "ProtectAllPlayersWhenHosting", true);
            g_controller = std::make_unique<GodModeController>(enabledAtStart);

            self->hooks->Engine->RegisterOnTick(&OnEngineTick);
            self->hooks->World->RegisterOnWorldBeginPlay(&OnWorldBeginPlay);
            self->hooks->World->RegisterOnBeforeWorldEndPlay(&OnWorldEndPlay);
            self->hooks->Input->RegisterKeybindByName(g_toggleKey, EModKeyEvent::Pressed,
                                                      &ToggleGodMode);
            if (self->hooks->Players != nullptr)
            {
                self->hooks->Players->RegisterOnPlayerJoined(&OnPlayerJoined);
                self->hooks->Players->RegisterOnPlayerLeft(&OnPlayerLeft);
            }
            else if (g_protectAllPlayersWhenHosting)
            {
                g_protectAllPlayersWhenHosting = false;
                LogWarning("Player lifecycle hooks are unavailable; host protection is limited "
                           "to the local player");
            }
            g_hooksRegistered = true;
            LogInfo("Plugin initialized; press the configured key to toggle God Mode");
            return true;
        }
        catch (...)
        {
            LogError("Plugin initialization failed unexpectedly");
            return false;
        }
    }

    __declspec(dllexport) void PluginShutdown()
    {
        try
        {
            DisableCurrentPlayerEffects();
            DisableHostedPlayerEffects();
            if (g_hooksRegistered && g_self != nullptr && g_self->hooks != nullptr)
            {
                if (g_self->hooks->Input != nullptr)
                {
                    g_self->hooks->Input->UnregisterKeybindByName(
                        g_toggleKey, EModKeyEvent::Pressed, &ToggleGodMode);
                }
                if (g_self->hooks->Engine != nullptr)
                {
                    g_self->hooks->Engine->UnregisterOnTick(&OnEngineTick);
                }
                if (g_self->hooks->World != nullptr)
                {
                    g_self->hooks->World->UnregisterOnWorldBeginPlay(&OnWorldBeginPlay);
                    g_self->hooks->World->UnregisterOnBeforeWorldEndPlay(&OnWorldEndPlay);
                }
                if (g_self->hooks->Players != nullptr)
                {
                    g_self->hooks->Players->UnregisterOnPlayerJoined(&OnPlayerJoined);
                    g_self->hooks->Players->UnregisterOnPlayerLeft(&OnPlayerLeft);
                }
            }
        }
        catch (...)
        {
        }
        g_hooksRegistered = false;
        g_controller.reset();
        g_hostedPlayers.clear();
        g_safeTemperatures.clear();
        g_appliedLocalController = nullptr;
        g_appliedLocalHadAuthority = false;
        g_world = nullptr;
        g_self = nullptr;
    }
}
