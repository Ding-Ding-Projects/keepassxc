[CmdletBinding()]
param([Alias('s')][switch]$Silent)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$manifest = Get-Content -Raw -LiteralPath (Join-Path $root 'packaging\squirrel\toolchain.json') | ConvertFrom-Json
$toolRoot = Join-Path $env:LOCALAPPDATA 'KeePassXCMaterial\toolchain'
$nugetRoot = Join-Path $toolRoot "nuget-$($manifest.nuget.version)"
$nugetExe = Join-Path $nugetRoot 'nuget.exe'
$squirrelRoot = Join-Path $toolRoot "squirrel-windows-$($manifest.squirrelWindows.version)"
$squirrelExe = Join-Path $squirrelRoot 'squirrel.windows\tools\Squirrel.exe'

function Write-Phase([string]$Message) { if (-not $Silent) { Write-Host "[dependencies] $Message" } }
function Require-Command([string]$Name, [string]$WingetId) {
    if (Get-Command $Name -ErrorAction SilentlyContinue) { Write-Phase "$Name is already available."; return }
    $winget = Get-Command winget.exe -ErrorAction SilentlyContinue
    if (-not $winget) { throw "$Name is missing and winget.exe is unavailable." }
    & $winget.Source install --id $WingetId --exact --silent --accept-package-agreements --accept-source-agreements
    if ($LASTEXITCODE -ne 0) { throw "winget failed to install $WingetId (exit $LASTEXITCODE)." }
}

New-Item -ItemType Directory -Force -Path $toolRoot, $nugetRoot, $squirrelRoot | Out-Null
Require-Command 'git.exe' 'Git.Git'
Require-Command 'cmake.exe' 'Kitware.CMake'
Require-Command 'ninja.exe' 'Ninja-build.Ninja'

if (-not (Test-Path -LiteralPath $nugetExe)) {
    Write-Phase "Downloading NuGet $($manifest.nuget.version) from dist.nuget.org."
    Invoke-WebRequest -UseBasicParsing -Uri $manifest.nuget.url -OutFile $nugetExe
}
$nugetHash = (Get-FileHash $nugetExe -Algorithm SHA256).Hash.ToLowerInvariant()
if ($nugetHash -ne $manifest.nuget.sha256) { throw "NuGet digest mismatch: expected $($manifest.nuget.sha256), got $nugetHash." }
if (-not (Test-Path -LiteralPath $squirrelExe)) {
    Write-Phase "Restoring squirrel.windows $($manifest.squirrelWindows.version) from NuGet.org."
    & $nugetExe install $manifest.squirrelWindows.packageId -Version $manifest.squirrelWindows.version -Source $manifest.squirrelWindows.source -OutputDirectory $squirrelRoot -ExcludeVersion -NonInteractive
    if ($LASTEXITCODE -ne 0) { throw "NuGet failed to restore squirrel.windows (exit $LASTEXITCODE)." }
}
if (-not (Test-Path -LiteralPath $squirrelExe)) { throw "Squirrel.exe is missing at $squirrelExe." }
$squirrelHash = (Get-FileHash $squirrelExe -Algorithm SHA256).Hash.ToLowerInvariant()
if ($squirrelHash -ne $manifest.squirrelWindows.squirrelExeSha256) { throw "Squirrel.exe digest mismatch: expected $($manifest.squirrelWindows.squirrelExeSha256), got $squirrelHash." }

$receipt = [ordered]@{
    schemaVersion = 1
    generatedAtUtc = [DateTime]::UtcNow.ToString('o')
    architecture = $manifest.architecture
    nuget = @{ version = $manifest.nuget.version; path = $nugetExe; sha256 = $nugetHash }
    squirrelWindows = @{ version = $manifest.squirrelWindows.version; path = $squirrelExe; sha256 = $squirrelHash; maintenanceStatus = $manifest.squirrelWindows.maintenanceStatus }
}
$receiptPath = Join-Path $toolRoot 'dependency-receipt.json'
$receipt | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $receiptPath -Encoding UTF8
Write-Phase "Dependency receipt: $receiptPath"
Write-Output $receiptPath
