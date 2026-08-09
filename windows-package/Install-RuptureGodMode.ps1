param(
    [string]$GameRoot = "",
    [string]$PayloadRoot = "",
    [switch]$NonInteractive
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$PluginName = "RuptureGodMode"
$ExpectedManifestUrl = "https://github.com/Lucenx9/rupturegodmode/releases/latest/download/RuptureGodMode-client-manifest.json"
$Utf8NoBom = [System.Text.UTF8Encoding]::new($false)

function Test-GameRoot([string]$Candidate) {
    if ([string]::IsNullOrWhiteSpace($Candidate)) { return $false }
    $Executable = Join-Path $Candidate "StarRupture\Binaries\Win64\StarRuptureGameSteam-Win64-Shipping.exe"
    return Test-Path -LiteralPath $Executable -PathType Leaf
}

function Get-SteamRoots {
    $Roots = [System.Collections.Generic.List[string]]::new()
    if (-not [string]::IsNullOrWhiteSpace($env:RGM_STEAM_ROOT)) {
        [void]$Roots.Add($env:RGM_STEAM_ROOT)
    }
    foreach ($RegistryKey in @(
        "HKCU:\Software\Valve\Steam",
        "HKLM:\SOFTWARE\WOW6432Node\Valve\Steam"
    )) {
        if (-not (Test-Path -LiteralPath $RegistryKey)) { continue }
        $Steam = Get-ItemProperty -LiteralPath $RegistryKey
        if ($Steam.PSObject.Properties.Name -contains "SteamPath") {
            [void]$Roots.Add([string]$Steam.SteamPath)
        }
        if ($Steam.PSObject.Properties.Name -contains "InstallPath") {
            [void]$Roots.Add([string]$Steam.InstallPath)
        }
    }
    if (-not [string]::IsNullOrWhiteSpace(${env:ProgramFiles(x86)})) {
        [void]$Roots.Add((Join-Path ${env:ProgramFiles(x86)} "Steam"))
    }
    return @($Roots | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | Sort-Object -Unique)
}

function Find-GameRoot {
    $Libraries = [System.Collections.Generic.List[string]]::new()
    foreach ($SteamRoot in Get-SteamRoots) {
        [void]$Libraries.Add($SteamRoot)
        $LibraryFile = Join-Path $SteamRoot "steamapps\libraryfolders.vdf"
        if (-not (Test-Path -LiteralPath $LibraryFile -PathType Leaf)) { continue }
        $Contents = [System.IO.File]::ReadAllText($LibraryFile)
        foreach ($Match in [regex]::Matches($Contents, '"path"\s+"([^"]+)"')) {
            [void]$Libraries.Add(($Match.Groups[1].Value -replace '\\\\', '\'))
        }
    }

    foreach ($Library in $Libraries | Sort-Object -Unique) {
        $Candidate = Join-Path $Library "steamapps\common\StarRupture"
        if (Test-GameRoot $Candidate) { return $Candidate }
    }

    foreach ($Drive in Get-PSDrive -PSProvider FileSystem) {
        foreach ($RelativePath in @(
            "steamapps\common\StarRupture",
            "SteamLibrary\steamapps\common\StarRupture",
            "Steam\steamapps\common\StarRupture"
        )) {
            $Candidate = Join-Path $Drive.Root $RelativePath
            if (Test-GameRoot $Candidate) { return $Candidate }
        }
        try {
            foreach ($TopLevelDirectory in Get-ChildItem -LiteralPath $Drive.Root -Directory -ErrorAction Stop) {
                $Candidate = Join-Path $TopLevelDirectory.FullName "steamapps\common\StarRupture"
                if (Test-GameRoot $Candidate) { return $Candidate }
            }
        } catch {
            Write-Verbose "Could not scan drive $($Drive.Name): $($_.Exception.Message)"
        }
    }
    return $null
}

function Write-StagedModLoaderConfig([string]$ConfigFile, [string]$StagedConfig) {
    if (Test-Path -LiteralPath $ConfigFile -PathType Leaf) {
        $Lines = [System.Collections.Generic.List[string]]::new()
        foreach ($Line in [System.IO.File]::ReadAllLines($ConfigFile)) {
            [void]$Lines.Add($Line)
        }
    } else {
        $Lines = [System.Collections.Generic.List[string]]::new()
    }

    $SectionIndex = -1
    $SectionEnd = $Lines.Count
    $EnabledIndex = -1
    for ($Index = 0; $Index -lt $Lines.Count; $Index++) {
        if ($Lines[$Index] -match '^\s*\[([^]]+)\]\s*$') {
            if ($SectionIndex -ge 0) {
                $SectionEnd = $Index
                break
            }
            if ($Matches[1] -ieq "AutoUpdate") { $SectionIndex = $Index }
        } elseif ($SectionIndex -ge 0 -and $Lines[$Index] -match '^\s*Enabled\s*=') {
            $EnabledIndex = $Index
        }
    }

    if ($SectionIndex -lt 0) {
        if ($Lines.Count -gt 0 -and $Lines[$Lines.Count - 1] -ne "") { [void]$Lines.Add("") }
        [void]$Lines.Add("[AutoUpdate]")
        [void]$Lines.Add("Enabled=1")
    } elseif ($EnabledIndex -ge 0) {
        $Lines[$EnabledIndex] = "Enabled=1"
    } else {
        $Lines.Insert($SectionEnd, "Enabled=1")
    }
    [System.IO.File]::WriteAllLines($StagedConfig, $Lines, $Utf8NoBom)
}

Write-Host "Rupture God Mode - Windows installer" -ForegroundColor Cyan

$RunningGame = Get-Process -ErrorAction SilentlyContinue |
    Where-Object { $_.ProcessName -like "StarRuptureGame*" }
if ($RunningGame) {
    throw "StarRupture is running. Close the game before installing the plugin."
}

if (-not (Test-GameRoot $GameRoot)) { $GameRoot = Find-GameRoot }
if (-not (Test-GameRoot $GameRoot) -and -not $NonInteractive) {
    Write-Host "StarRupture was not found automatically." -ForegroundColor Yellow
    $GameRoot = (Read-Host "Paste the folder shown by Steam > Manage > Browse local files").Trim().Trim('"')
}
if (-not (Test-GameRoot $GameRoot)) {
    throw "StarRupture could not be found. Pass -GameRoot with the installation directory."
}

if ([string]::IsNullOrWhiteSpace($PayloadRoot)) {
    $PayloadRoot = Join-Path $PSScriptRoot "payload"
}
$PayloadDll = Join-Path $PayloadRoot "$PluginName.dll"
$PayloadSidecar = Join-Path $PayloadRoot "$PluginName.json"
if (-not (Test-Path -LiteralPath $PayloadDll -PathType Leaf)) {
    throw "The package payload is missing: $PayloadDll"
}
if (-not (Test-Path -LiteralPath $PayloadSidecar -PathType Leaf)) {
    throw "The package payload is missing: $PayloadSidecar"
}
$DllHeader = [System.IO.File]::ReadAllBytes($PayloadDll)
if ($DllHeader.Length -lt 2 -or $DllHeader[0] -ne 0x4D -or $DllHeader[1] -ne 0x5A) {
    throw "The packaged plugin is not a Windows DLL. Download the release again."
}
$Sidecar = Get-Content -LiteralPath $PayloadSidecar -Raw | ConvertFrom-Json
if ($Sidecar.manifest_url -ne $ExpectedManifestUrl) {
    throw "The packaged auto-update sidecar is invalid. Download the release again."
}

$BinaryDirectory = Join-Path $GameRoot "StarRupture\Binaries\Win64"
$ModLoaderDirectory = Join-Path $BinaryDirectory "ModLoader"
$PluginDirectory = Join-Path $ModLoaderDirectory "Plugins"
if (-not (Test-Path -LiteralPath (Join-Path $BinaryDirectory "dwmapi.dll") -PathType Leaf) -or
    -not (Test-Path -LiteralPath $ModLoaderDirectory -PathType Container)) {
    throw "StarRupture ModLoader is not installed in: $BinaryDirectory"
}
New-Item -ItemType Directory -Path $PluginDirectory -Force | Out-Null

$DestinationDll = Join-Path $PluginDirectory "$PluginName.dll"
$DestinationSidecar = Join-Path $PluginDirectory "$PluginName.json"
$ConfigFile = Join-Path $ModLoaderDirectory "modloader.ini"
$LegacyDll = Join-Path $PluginDirectory "RuptureGodMode-Client.dll"
$LegacySidecar = Join-Path $PluginDirectory "RuptureGodMode-Client.json"
$StagedDll = Join-Path $PluginDirectory (".$PluginName.dll.install." + [guid]::NewGuid())
$StagedSidecar = Join-Path $PluginDirectory (".$PluginName.json.install." + [guid]::NewGuid())
$StagedConfig = Join-Path $ModLoaderDirectory ".modloader.ini.install.$([guid]::NewGuid())"
$RollbackDll = Join-Path $PluginDirectory (".$PluginName.dll.rollback." + [guid]::NewGuid())
$RollbackSidecar = Join-Path $PluginDirectory (".$PluginName.json.rollback." + [guid]::NewGuid())
$RollbackConfig = Join-Path $ModLoaderDirectory ".modloader.ini.rollback.$([guid]::NewGuid())"
$RollbackLegacyDll = Join-Path $PluginDirectory ".RuptureGodMode-Client.dll.rollback.$([guid]::NewGuid())"
$RollbackLegacySidecar = Join-Path $PluginDirectory ".RuptureGodMode-Client.json.rollback.$([guid]::NewGuid())"
$HadDll = Test-Path -LiteralPath $DestinationDll -PathType Leaf
$HadSidecar = Test-Path -LiteralPath $DestinationSidecar -PathType Leaf
$HadConfig = Test-Path -LiteralPath $ConfigFile -PathType Leaf
$HadLegacyDll = Test-Path -LiteralPath $LegacyDll -PathType Leaf
$HadLegacySidecar = Test-Path -LiteralPath $LegacySidecar -PathType Leaf
$InstalledDll = $false
$InstalledSidecar = $false
$InstalledConfig = $false
$CommitSucceeded = $false
$RollbackSucceeded = $false

try {
    Copy-Item -LiteralPath $PayloadDll -Destination $StagedDll
    Copy-Item -LiteralPath $PayloadSidecar -Destination $StagedSidecar
    Write-StagedModLoaderConfig $ConfigFile $StagedConfig
    if ($HadDll) { Move-Item -LiteralPath $DestinationDll -Destination $RollbackDll -Force }
    if ($HadSidecar) {
        Move-Item -LiteralPath $DestinationSidecar -Destination $RollbackSidecar -Force
    }
    if ($HadConfig) { Move-Item -LiteralPath $ConfigFile -Destination $RollbackConfig -Force }
    Move-Item -LiteralPath $StagedDll -Destination $DestinationDll -Force
    $InstalledDll = $true
    Move-Item -LiteralPath $StagedSidecar -Destination $DestinationSidecar -Force
    $InstalledSidecar = $true
    Move-Item -LiteralPath $StagedConfig -Destination $ConfigFile -Force
    $InstalledConfig = $true
    if ($HadLegacyDll) {
        Move-Item -LiteralPath $LegacyDll -Destination $RollbackLegacyDll -Force
    }
    if ($HadLegacySidecar) {
        Move-Item -LiteralPath $LegacySidecar -Destination $RollbackLegacySidecar -Force
    }
    $CommitSucceeded = $true
} catch {
    if ($InstalledDll -and (Test-Path -LiteralPath $DestinationDll)) {
        Remove-Item -LiteralPath $DestinationDll -Force -ErrorAction Stop
    }
    if ($InstalledSidecar -and (Test-Path -LiteralPath $DestinationSidecar)) {
        Remove-Item -LiteralPath $DestinationSidecar -Force -ErrorAction Stop
    }
    if ($InstalledConfig -and (Test-Path -LiteralPath $ConfigFile)) {
        Remove-Item -LiteralPath $ConfigFile -Force -ErrorAction Stop
    }
    if (Test-Path -LiteralPath $RollbackDll) {
        Move-Item -LiteralPath $RollbackDll -Destination $DestinationDll -Force
    }
    if (Test-Path -LiteralPath $RollbackSidecar) {
        Move-Item -LiteralPath $RollbackSidecar -Destination $DestinationSidecar -Force
    }
    if (Test-Path -LiteralPath $RollbackConfig) {
        Move-Item -LiteralPath $RollbackConfig -Destination $ConfigFile -Force
    }
    if (Test-Path -LiteralPath $RollbackLegacyDll) {
        Move-Item -LiteralPath $RollbackLegacyDll -Destination $LegacyDll -Force
    }
    if (Test-Path -LiteralPath $RollbackLegacySidecar) {
        Move-Item -LiteralPath $RollbackLegacySidecar -Destination $LegacySidecar -Force
    }
    $RollbackSucceeded = $true
    throw
} finally {
    foreach ($TemporaryFile in @(
        $StagedDll,
        $StagedSidecar,
        $StagedConfig
    )) {
        Remove-Item -LiteralPath $TemporaryFile -Force -ErrorAction SilentlyContinue
    }
    $RollbackFiles = @(
        $RollbackDll,
        $RollbackSidecar,
        $RollbackConfig,
        $RollbackLegacyDll,
        $RollbackLegacySidecar
    )
    if ($CommitSucceeded -or $RollbackSucceeded) {
        foreach ($RollbackFile in $RollbackFiles) {
            Remove-Item -LiteralPath $RollbackFile -Force -ErrorAction SilentlyContinue
        }
    } else {
        $PreservedRollbackFiles = @(
            $RollbackFiles | Where-Object { Test-Path -LiteralPath $_ }
        )
        if ($PreservedRollbackFiles.Count -gt 0) {
            Write-Warning (
                "Automatic rollback was incomplete. Recovery files were preserved:`n  " +
                ($PreservedRollbackFiles -join "`n  ")
            )
        }
    }
}

Unblock-File -LiteralPath $DestinationDll -ErrorAction SilentlyContinue

Write-Host ""
Write-Host "Installation completed." -ForegroundColor Green
Write-Host "Game: $GameRoot"
Write-Host "Plugin: $DestinationDll"
Write-Host "The previous Rupture God Mode DLL was replaced and ModLoader auto-update is enabled."
