Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$TestRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("rupture-god-mode-installer-" + [guid]::NewGuid())
$OriginalSteamRoot = $env:RGM_STEAM_ROOT
$TestDriveName = "RGM" + ([guid]::NewGuid().ToString("N").Substring(0, 8))
$TestDriveCreated = $false

try {
    $SteamRoot = Join-Path $TestRoot "Steam"
    $SecondaryLibrary = Join-Path $TestRoot "GamesSSD"
    $GameRoot = Join-Path $SecondaryLibrary "steamapps\common\StarRupture"
    $BinaryDirectory = Join-Path $GameRoot "StarRupture\Binaries\Win64"
    $PluginDirectory = Join-Path $BinaryDirectory "ModLoader\Plugins"
    $PackageRoot = Join-Path $TestRoot "package"
    $PayloadRoot = Join-Path $PackageRoot "payload"

    New-Item -ItemType Directory -Path (Join-Path $SteamRoot "steamapps") -Force | Out-Null
    New-Item -ItemType Directory -Path $PluginDirectory -Force | Out-Null
    New-Item -ItemType Directory -Path $PayloadRoot -Force | Out-Null

    $VdfLibrary = $SecondaryLibrary -replace '\\', '\\'
    @"
"libraryfolders"
{
    "1"
    {
        "path" "$VdfLibrary"
    }
}
"@ | Set-Content -LiteralPath (Join-Path $SteamRoot "steamapps\libraryfolders.vdf") -Encoding UTF8

    New-Item -ItemType File -Path (Join-Path $BinaryDirectory "StarRuptureGameSteam-Win64-Shipping.exe") -Force | Out-Null
    New-Item -ItemType File -Path (Join-Path $BinaryDirectory "dwmapi.dll") -Force | Out-Null
    @"
[Logging]
Level=INFO
[AutoUpdate]
Enabled=0
[UI]
Enabled=1
"@ | Set-Content -LiteralPath (Join-Path $BinaryDirectory "ModLoader\modloader.ini") -Encoding UTF8

    [System.IO.File]::WriteAllBytes(
        (Join-Path $PluginDirectory "RuptureGodMode.dll"),
        [byte[]](0x4D, 0x5A, 0x6F, 0x6C, 0x64)
    )
    New-Item -ItemType File -Path (Join-Path $PluginDirectory "RuptureGodMode-Client.dll") -Force | Out-Null
    New-Item -ItemType File -Path (Join-Path $PluginDirectory "RuptureGodMode-Client.json") -Force | Out-Null

    $ExpectedPlugin = [byte[]](0x4D, 0x5A, 0x6E, 0x65, 0x77)
    [System.IO.File]::WriteAllBytes((Join-Path $PayloadRoot "RuptureGodMode.dll"), $ExpectedPlugin)
    @{
        manifest_url = "https://github.com/Lucenx9/rupturegodmode/releases/latest/download/RuptureGodMode-client-manifest.json"
    } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $PayloadRoot "RuptureGodMode.json") -Encoding UTF8
    Copy-Item -LiteralPath (Join-Path $ProjectRoot "windows-package\Install-RuptureGodMode.ps1") -Destination $PackageRoot

    New-PSDrive -Name $TestDriveName -PSProvider FileSystem -Root $TestRoot | Out-Null
    $TestDriveCreated = $true
    $env:RGM_STEAM_ROOT = $SteamRoot
    & (Join-Path $PackageRoot "Install-RuptureGodMode.ps1") -NonInteractive

    $InstalledPlugin = [System.IO.File]::ReadAllBytes((Join-Path $PluginDirectory "RuptureGodMode.dll"))
    if ([Convert]::ToBase64String($InstalledPlugin) -ne [Convert]::ToBase64String($ExpectedPlugin)) {
        throw "The installer did not replace the old plugin DLL"
    }
    if (-not (Test-Path -LiteralPath (Join-Path $PluginDirectory "RuptureGodMode.json"))) {
        throw "The installer did not install the auto-update sidecar"
    }
    if (Test-Path -LiteralPath (Join-Path $PluginDirectory "RuptureGodMode-Client.dll")) {
        throw "The installer left a legacy plugin DLL behind"
    }
    if (Test-Path -LiteralPath (Join-Path $PluginDirectory "RuptureGodMode-Client.json")) {
        throw "The installer left a legacy plugin sidecar behind"
    }
    $Config = Get-Content -LiteralPath (Join-Path $BinaryDirectory "ModLoader\modloader.ini") -Raw
    if ($Config -notmatch '(?ms)^\[AutoUpdate\]\s*.*?^Enabled=1\s*$') {
        throw "The installer did not enable ModLoader auto-update"
    }

    Remove-Item -LiteralPath (Join-Path $SteamRoot "steamapps\libraryfolders.vdf") -Force
    $env:RGM_STEAM_ROOT = Join-Path $TestRoot "MissingSteam"
    [System.IO.File]::WriteAllBytes(
        (Join-Path $PluginDirectory "RuptureGodMode.dll"),
        [byte[]](0x4D, 0x5A, 0x6F, 0x6C, 0x64)
    )
    & (Join-Path $PackageRoot "Install-RuptureGodMode.ps1") -NonInteractive
    $FallbackInstalledPlugin = [System.IO.File]::ReadAllBytes(
        (Join-Path $PluginDirectory "RuptureGodMode.dll")
    )
    if ([Convert]::ToBase64String($FallbackInstalledPlugin) -ne [Convert]::ToBase64String($ExpectedPlugin)) {
        throw "The fallback drive scan did not find StarRupture in a custom SSD directory"
    }

    $PreviousPlugin = [byte[]](0x4D, 0x5A, 0x70, 0x72, 0x65, 0x76)
    [System.IO.File]::WriteAllBytes((Join-Path $PluginDirectory "RuptureGodMode.dll"), $PreviousPlugin)
    '{"previous":true}' | Set-Content -LiteralPath (Join-Path $PluginDirectory "RuptureGodMode.json") -Encoding UTF8
    "[AutoUpdate]`nEnabled=0" | Set-Content -LiteralPath (Join-Path $BinaryDirectory "ModLoader\modloader.ini") -Encoding UTF8
    $LockedLegacy = Join-Path $PluginDirectory "RuptureGodMode-Client.dll"
    [System.IO.File]::WriteAllBytes($LockedLegacy, [byte[]](0x4D, 0x5A, 0x6C, 0x6F, 0x63, 0x6B))
    $Lock = [System.IO.File]::Open($LockedLegacy, "Open", "Read", "None")
    try {
        $FailedAsExpected = $false
        try {
            & (Join-Path $PackageRoot "Install-RuptureGodMode.ps1") -GameRoot $GameRoot -NonInteractive
        } catch {
            $FailedAsExpected = $true
        }
        if (-not $FailedAsExpected) { throw "A locked legacy DLL did not fail the installation" }
    } finally {
        $Lock.Dispose()
    }
    $RolledBackPlugin = [System.IO.File]::ReadAllBytes((Join-Path $PluginDirectory "RuptureGodMode.dll"))
    if ([Convert]::ToBase64String($RolledBackPlugin) -ne [Convert]::ToBase64String($PreviousPlugin)) {
        throw "A failed legacy DLL removal did not restore the previous plugin"
    }
    $RolledBackConfig = Get-Content -LiteralPath (Join-Path $BinaryDirectory "ModLoader\modloader.ini") -Raw
    if ($RolledBackConfig -notmatch 'Enabled=0') {
        throw "A failed legacy DLL removal did not restore modloader.ini"
    }
    $RolledBackSidecar = Get-Content -LiteralPath (Join-Path $PluginDirectory "RuptureGodMode.json") -Raw
    if ($RolledBackSidecar -notmatch 'previous') {
        throw "A failed legacy DLL removal did not restore the previous sidecar"
    }
} finally {
    $env:RGM_STEAM_ROOT = $OriginalSteamRoot
    if ($TestDriveCreated) {
        Remove-PSDrive -Name $TestDriveName -Force -ErrorAction SilentlyContinue
    }
    Remove-Item -LiteralPath $TestRoot -Recurse -Force -ErrorAction SilentlyContinue
}

Write-Host "windows_installer_tests: PASS"
