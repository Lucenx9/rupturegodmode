param(
    [string]$GameRoot = "",
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

if (-not (Test-Path -LiteralPath $DllPath -PathType Leaf)) {
    throw "Plugin DLL was not found: $DllPath"
}

$Header = [System.IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $DllPath))
if ($Header.Length -lt 2 -or $Header[0] -ne 0x4D -or $Header[1] -ne 0x5A) {
    throw "Build output is not a Windows DLL: $DllPath"
}

$TemporaryPackage = Join-Path ([System.IO.Path]::GetTempPath()) `
    ("rupture-god-mode-local-" + [guid]::NewGuid())
$Payload = Join-Path $TemporaryPackage "payload"
New-Item -ItemType Directory -Path $Payload -Force | Out-Null
try {
    Copy-Item -LiteralPath $DllPath -Destination (Join-Path $Payload "RuptureGodMode.dll")
    Copy-Item -LiteralPath (Join-Path $ProjectRoot "release\RuptureGodMode.json") `
        -Destination (Join-Path $Payload "RuptureGodMode.json")
    & (Join-Path $ProjectRoot "windows-package\Install-RuptureGodMode.ps1") `
        -GameRoot $GameRoot `
        -PayloadRoot $Payload
} finally {
    Remove-Item -LiteralPath $TemporaryPackage -Recurse -Force -ErrorAction SilentlyContinue
}
