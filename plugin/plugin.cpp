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

#include <cstdint>
#include <iterator>
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

IPluginSelf *g_self = nullptr;
SDK::UWorld *g_world = nullptr;
std::unique_ptr<GodModeController> g_controller;
char g_toggleKey[64] = "F8";
bool g_hooksRegistered = false;
bool g_tickFailureLogged = false;
bool g_remoteModeLogged = false;
bool g_protectAllPlayersWhenHosting = true;
std::unordered_map<SDK::ACrPlayerControllerBase *, std::unique_ptr<GodModeController>>
    g_hostedPlayers;

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
     "Enable God Mode whenever a playable world starts", 0.0f, 0.0f},
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

bool FindLocalPlayer(SDK::ACrPlayerControllerBase *&controller,
                     SDK::ACrCharacterPlayerBase *&player)
{
    controller = nullptr;
    player = nullptr;
    if (g_world == nullptr)
    {
        return false;
    }

    SDK::APlayerController *baseController = SDK::UGameplayStatics::GetPlayerController(g_world, 0);
    SDK::APawn *basePawn = SDK::UGameplayStatics::GetPlayerPawn(g_world, 0);
    if (baseController == nullptr ||
        !baseController->IsA(SDK::ACrPlayerControllerBase::StaticClass()) || basePawn == nullptr ||
        !basePawn->IsA(SDK::ACrCharacterPlayerBase::StaticClass()))
    {
        return false;
    }

    controller = static_cast<SDK::ACrPlayerControllerBase *>(baseController);
    player = static_cast<SDK::ACrCharacterPlayerBase *>(basePawn);
    return true;
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

void DisableCurrentPlayerEffects()
{
    SDK::ACrPlayerControllerBase *controller = nullptr;
    SDK::ACrCharacterPlayerBase *player = nullptr;
    if (g_controller != nullptr && FindLocalPlayer(controller, player))
    {
        const bool hasAuthority = CurrentNetworkMode() != NetworkMode::RemoteClient;
        StarRuptureEffects effects(controller, player, hasAuthority);
        g_controller->RemoveAppliedEffects(effects);
    }
    if (g_controller != nullptr)
    {
        g_controller->ForgetTarget();
    }
}

void DisableHostedPlayerEffects()
{
    for (auto &[controller, state] : g_hostedPlayers)
    {
        StarRuptureEffects effects(controller, FindPlayerForController(controller), true);
        state->RemoveAppliedEffects(effects);
    }
}

void OnPlayerJoined(void *rawController)
{
    if (rawController == nullptr || g_controller == nullptr)
    {
        return;
    }
    auto *controller = static_cast<SDK::ACrPlayerControllerBase *>(rawController);
    if (!controller->IsA(SDK::ACrPlayerControllerBase::StaticClass()))
    {
        return;
    }
    g_hostedPlayers.try_emplace(controller,
                                std::make_unique<GodModeController>(g_controller->IsEnabled()));
}

void OnPlayerLeft(void *rawController)
{
    auto *controller = static_cast<SDK::ACrPlayerControllerBase *>(rawController);
    const auto found = g_hostedPlayers.find(controller);
    if (found == g_hostedPlayers.end())
    {
        return;
    }
    StarRuptureEffects effects(controller, FindPlayerForController(controller), true);
    found->second->RemoveAppliedEffects(effects);
    g_hostedPlayers.erase(found);
}

void OnWorldBeginPlay(SDK::UWorld *world)
{
    g_world = world;
    g_remoteModeLogged = false;
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
    g_world = nullptr;
}

void OnEngineTick(float)
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
                LogWarning("Multiplayer client protection is active locally; install "
                           "the plugin on "
                           "the host too for server-authoritative protection");
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
        StarRuptureEffects effects(controller, player, mode != NetworkMode::RemoteClient);
        const ApplyResult result = g_controller->Reconcile(
            mode, hasPlayer ? reinterpret_cast<std::uintptr_t>(player) : 0, effects);
        if (result == ApplyResult::Applied)
        {
            LogInfo("God Mode enabled");
        }
        else if (result == ApplyResult::Removed)
        {
            LogInfo("God Mode disabled");
        }

        if (mode == NetworkMode::ListenServer && g_protectAllPlayersWhenHosting)
        {
            for (auto &[hostedController, state] : g_hostedPlayers)
            {
                if (hostedController == controller)
                {
                    continue;
                }
                SDK::ACrCharacterPlayerBase *hostedPlayer =
                    FindPlayerForController(hostedController);
                state->SetEnabled(g_controller->IsEnabled());
                StarRuptureEffects hostedEffects(hostedController, hostedPlayer, true);
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
        g_world = nullptr;
        g_self = nullptr;
    }
}
