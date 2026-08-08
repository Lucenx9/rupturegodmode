# Rupture God Mode

StarRupture ModLoader plugin that protects the local player's survival state. It starts enabled by default and can be toggled with `F8`, without adding another in-game window. The toggle state persists across world travel for the current game session.

## Protected values

While enabled, the plugin keeps health, energy, shield, oxygen, hydration, calories, med-tool charge, and grenade charge full. Toxicity, radiation, heat, drain, corrosion, and infection are held at their minimum, while temperature is frozen at the safe value captured for the pawn. Native StarRupture cheats also provide unlimited ammo and prevent weapon heat.

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

## Installation

### Requirements

1. Install [StarRupture ModLoader](https://github.com/AlienXAXS/StarRupture-ModLoader).
2. Launch the game once and close it. This lets ModLoader create its directories.
3. Download `RuptureGodMode.dll` from the latest GitHub release.

Do not install the DLL while the game is running. The plugin is loaded only when StarRupture starts.

### Linux / Steam Deck

StarRupture runs through Proton, but the plugin is still the same Windows DLL.

1. Open the StarRupture installation directory. A common path is:

   ```text
   ~/.local/share/Steam/steamapps/common/StarRupture
   ```

   Additional Steam libraries are commonly mounted elsewhere, for example:

   ```text
   /mnt/storage/SteamLibrary/steamapps/common/StarRupture
   ```

2. Copy the downloaded DLL into:

   ```text
   StarRupture/StarRupture/Binaries/Win64/ModLoader/Plugins/RuptureGodMode.dll
   ```

   From a terminal:

   ```bash
   cp ~/Downloads/RuptureGodMode.dll \
     "/path/to/StarRupture/StarRupture/Binaries/Win64/ModLoader/Plugins/"
   ```

3. Make sure the ModLoader launch option required by your Proton setup is configured in Steam. The usual option is:

   ```text
   WINEDLLOVERRIDES="dwmapi=n,b" %command%
   ```

For a locally compiled DLL, the repository includes a validated installer:

```bash
scripts/install_local.sh "/path/to/StarRupture"
```

Override the DLL location with `RGM_DLL=/path/to/RuptureGodMode.dll` if needed.

### Windows

1. In Steam, right-click **StarRupture**, then select **Manage > Browse local files**.
2. Open `StarRupture\Binaries\Win64\ModLoader\Plugins` inside that directory.
3. Copy the downloaded file there as `RuptureGodMode.dll`.
4. If Windows blocked the downloaded DLL, open PowerShell and run:

   ```powershell
   Unblock-File "C:\path\to\StarRupture\StarRupture\Binaries\Win64\ModLoader\Plugins\RuptureGodMode.dll"
   ```

For a locally compiled DLL, use the included PowerShell installer:

```powershell
.\scripts\install_local.ps1 -GameRoot "C:\Program Files (x86)\Steam\steamapps\common\StarRupture"
```

Use `-DllPath "C:\path\to\RuptureGodMode.dll"` to install a DLL from another location.

### Verify the installation

1. Start StarRupture.
2. Open `StarRupture\Binaries\Win64\ModLoader\Logs\ModLoader.log` and look for `Plugin initialized: RuptureGodMode`.
3. Load a save and press `F8` to toggle God Mode.
4. After the first launch, settings are available in `ModLoader\Plugins\config\RuptureGodMode.ini`.

To uninstall the plugin, close the game and delete `ModLoader\Plugins\RuptureGodMode.dll`. The generated INI file can be kept or removed.

Compatibility: ModLoader plugin interface 60 and StarRupture Game SDK build CL121391.
