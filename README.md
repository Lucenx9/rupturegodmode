# Rupture God Mode

StarRupture ModLoader plugin that protects the local player's survival state. It starts enabled by default and can be toggled with `F8`, without adding another in-game window.

## Protected values

While enabled, the plugin keeps health, energy, shield, oxygen, hydration, calories, med-tool charge, and grenade charge full. Toxicity, radiation, heat, drain, corrosion, and infection are held at their minimum. Native StarRupture cheats also provide unlimited ammo and prevent weapon heat.

Multiplayer is supported too. A listen-server host protects every connected player by default. A remote client maintains its local predicted attributes; for reliable server-authoritative health and survival protection, the host should also run the plugin.

## Configuration

ModLoader creates `ModLoader/Plugins/config/RuptureGodMode.ini` on first launch:

```ini
[General]
Enabled=1

[GodMode]
EnabledAtStart=1
ToggleKey=F8

[Multiplayer]
ProtectAllPlayersWhenHosting=1
```

The key can be changed from ModLoader's configuration UI or in the INI file. ModLoader rebinds it automatically.

## Build

Visual Studio 2022:

```powershell
msbuild RuptureGodMode.sln `
  /p:Configuration="Client Release" `
  /p:Platform=x64 `
  /p:GameSDKRoot="C:\path\to\StarRupture-Game-SDK" `
  /p:PluginSDKInclude="C:\path\to\StarRupture-Plugin-SDK\include" `
  /m
```

Linux cross-build with `clang-cl`, `lld-link`, and an xwin SDK splat:

```bash
SDK_DIR=/path/to/StarRupture-Plugin-SDK \
WIN_SDK_DIR=/path/to/xwin-sdk \
scripts/build_cross.sh
```

The cross-build writes `build/RuptureGodMode.dll`. Run `scripts/test.sh` for the platform-independent behavior tests.

## Install

Copy `RuptureGodMode.dll` to `StarRupture/Binaries/Win64/ModLoader/Plugins/`, or run:

```bash
scripts/install_local.sh /path/to/StarRupture
```

Compatibility: ModLoader plugin interface 60 and StarRupture Game SDK build CL121391.
