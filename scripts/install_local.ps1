param(
    [string]$GameRoot = "C:\Program Files (x86)\Steam\steamapps\common\StarRupture",
    [string]$DllPath = ""
)

$ErrorActionPreference = "Stop"

$ProjectRoot = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($DllPath)) {
    $DllPath = Join-Path $ProjectRoot "build\Client Release\Plugins\RuptureGodMode.dll"
    if (-not (Test-Path -LiteralPath $DllPath -PathType Leaf)) {
        $DllPath = Join-Path $ProjectRoot "build\RuptureGodMode.dll"
    }
}

$BinaryDirectory = Join-Path $GameRoot "StarRupture\Binaries\Win64"
$GameExecutable = Join-Path $BinaryDirectory "StarRuptureGameSteam-Win64-Shipping.exe"
$ModLoaderProxy = Join-Path $BinaryDirectory "dwmapi.dll"
$PluginDirectory = Join-Path $BinaryDirectory "ModLoader\Plugins"
$Destination = Join-Path $PluginDirectory "RuptureGodMode.dll"

if (-not (Test-Path -LiteralPath $GameExecutable -PathType Leaf)) {
    throw "StarRupture was not found under: $GameRoot"
}
if (-not (Test-Path -LiteralPath $ModLoaderProxy -PathType Leaf) -or
    -not (Test-Path -LiteralPath $PluginDirectory -PathType Container)) {
    throw "StarRupture ModLoader is not installed under: $BinaryDirectory"
}
if (-not (Test-Path -LiteralPath $DllPath -PathType Leaf)) {
    throw "Plugin DLL was not found: $DllPath"
}

$Header = [System.IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $DllPath))
if ($Header.Length -lt 2 -or $Header[0] -ne 0x4D -or $Header[1] -ne 0x5A) {
    throw "Build output is not a Windows DLL: $DllPath"
}

Copy-Item -LiteralPath $DllPath -Destination $Destination -Force
Unblock-File -LiteralPath $Destination -ErrorAction SilentlyContinue
Write-Host "Installed RuptureGodMode.dll in $PluginDirectory"
