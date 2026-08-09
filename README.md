# Rupture God Mode

StarRupture ModLoader plugin that protects the local player's survival state. It starts enabled by default and can be toggled with `F8`, without adding another in-game window. The toggle state persists across world travel for the current game session.

## Protected values

While enabled, the plugin enables StarRupture's native immortality mode to stop lethal damage such as falls and solar storms. It also keeps health, energy, shield, oxygen, hydration, calories, med-tool charge, and grenade charge full. Toxicity, radiation, heat, drain, corrosion, and infection are held at their minimum, while temperature is frozen at the safe value captured for the pawn. Native StarRupture cheats also provide unlimited ammo and prevent weapon heat.

Multiplayer is supported too. A listen-server host protects every connected player by default, including with native immortality. A remote client maintains its local predicted attributes; the host must also run the plugin for reliable server-authoritative protection from lethal damage.

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
3. Download the package for your platform from the latest GitHub release.

Do not install the plugin while the game is running. The plugin is loaded only when StarRupture starts.

### Automatic updates

Release packages install `RuptureGodMode.json` next to the DLL. On every game startup, ModLoader uses this sidecar to check the latest compatible GitHub release and atomically replace the DLL before loading it. Automatic updates require `[AutoUpdate] Enabled=1` in `ModLoader/modloader.ini`; the Windows installer enables it automatically.

Users upgrading from v0.1.1 or an older DLL-only installation must install a packaged release once. Future releases are then handled by ModLoader.

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

2. Download `RuptureGodMode-Client-vX.Y.Z.zip` and extract its `Plugins` directory into:

   ```text
   StarRupture/StarRupture/Binaries/Win64/ModLoader
   ```

   From a terminal:

   ```bash
   unzip ~/Downloads/RuptureGodMode-Client-vX.Y.Z.zip \
     -d "/path/to/StarRupture/StarRupture/Binaries/Win64/ModLoader"
   ```

3. Make sure the ModLoader launch option required by your Proton setup is configured in Steam. The usual option is:

   ```text
   WINEDLLOVERRIDES="dwmapi=n,b" %command%
   ```

For a locally compiled DLL, the repository includes a validated installer that also installs the auto-update sidecar:

```bash
scripts/install_local.sh "/path/to/StarRupture"
```

Override the DLL location with `RGM_DLL=/path/to/RuptureGodMode.dll` if needed.

### Windows

1. Download `RuptureGodMode-Windows-vX.Y.Z.zip` from the latest release.
2. Extract the complete ZIP.
3. Right-click `Install-RuptureGodMode.ps1` and select **Run with PowerShell**. If script execution is blocked, open PowerShell in the extracted directory and run:

   ```powershell
   powershell -ExecutionPolicy Bypass -File .\Install-RuptureGodMode.ps1
   ```

The installer reads Steam's `libraryfolders.vdf` and searches all filesystem drives, so libraries on another SSD are detected automatically. It refuses to modify files while StarRupture is running, replaces the previous God Mode DLL, removes legacy duplicate filenames, installs the sidecar, and enables ModLoader auto-update.

If automatic detection fails, provide the folder shown by **Steam > Manage > Browse local files**:

```powershell
powershell -ExecutionPolicy Bypass -File .\Install-RuptureGodMode.ps1 `
  -GameRoot "D:\SteamLibrary\steamapps\common\StarRupture"
```

For a locally compiled DLL, run `.\scripts\install_local.ps1`; it uses the same automatic game detection. Use `-DllPath "C:\path\to\RuptureGodMode.dll"` to install a DLL from another location.

### Verify the installation

1. Start StarRupture.
2. Open `StarRupture\Binaries\Win64\ModLoader\Logs\ModLoader.log` and look for `Plugin initialized: RuptureGodMode`.
3. Load a save and press `F8` to toggle God Mode.
4. After the first launch, settings are available in `ModLoader\Plugins\config\RuptureGodMode.ini`.

To uninstall the plugin, close the game and delete both `ModLoader\Plugins\RuptureGodMode.dll` and `ModLoader\Plugins\RuptureGodMode.json`. Removing the sidecar prevents ModLoader from downloading the DLL again. The generated INI file can be kept or removed.

Compatibility: ModLoader plugin interface 60 and StarRupture Game SDK build CL121391.
