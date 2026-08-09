Rupture God Mode - Windows installation
======================================

Requirements
------------
StarRupture ModLoader must already be installed.

Installation
------------
1. Close StarRupture.
2. Extract the complete ZIP. Do not move the installer away from the payload folder.
3. Right-click Install-RuptureGodMode.ps1 and choose "Run with PowerShell".

If Windows blocks script execution, open PowerShell in the extracted folder and run:

  powershell -ExecutionPolicy Bypass -File .\Install-RuptureGodMode.ps1

The installer reads Steam's libraryfolders.vdf, so it can find StarRupture in
the default Steam folder or in a Steam library on another SSD. If automatic
detection fails, pass the game directory explicitly:

  powershell -ExecutionPolicy Bypass -File .\Install-RuptureGodMode.ps1 `
    -GameRoot "D:\SteamLibrary\steamapps\common\StarRupture"

The installer replaces older Rupture God Mode DLLs, installs the auto-update
sidecar, and enables plugin updates in ModLoader\modloader.ini. Future releases
are downloaded by ModLoader the next time StarRupture starts.
