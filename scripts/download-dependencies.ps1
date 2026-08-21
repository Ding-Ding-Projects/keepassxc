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

function Get-HashHex([string]$Path, [string]$Algorithm = 'SHA256') {
    $stream = [IO.File]::OpenRead($Path)
    try {
        $hasher = [Security.Cryptography.HashAlgorithm]::Create($Algorithm)
        try { return ([BitConverter]::ToString($hasher.ComputeHash($stream))).Replace('-', '').ToLowerInvariant() }
        finally { $hasher.Dispose() }
    } finally { $stream.Dispose() }
}

function Write-Phase([string]$Message) { if (-not $Silent) { Write-Host "[dependencies] $Message" } }
function Require-Command([string]$Name, [string]$WingetId) {
    if (Get-Command $Name -ErrorAction SilentlyContinue) { Write-Phase "$Name is already available."; return }
    if ($Name -eq 'ruby.exe' -and (Get-ChildItem 'C:\Ruby*\bin\ruby.exe' -ErrorAction SilentlyContinue | Select-Object -First 1)) {
        Write-Phase 'ruby.exe is already available through RubyInstaller.'
        return
    }
    $winget = Get-Command winget.exe -ErrorAction SilentlyContinue
    if (-not $winget) { throw "$Name is missing and winget.exe is unavailable." }
    & $winget.Source install --id $WingetId --exact --silent --accept-package-agreements --accept-source-agreements
    if ($LASTEXITCODE -ne 0) { throw "winget failed to install $WingetId (exit $LASTEXITCODE)." }
}

New-Item -ItemType Directory -Force -Path $toolRoot, $nugetRoot, $squirrelRoot | Out-Null
Require-Command 'git.exe' 'Git.Git'
Require-Command 'cmake.exe' 'Kitware.CMake'
Require-Command 'ninja.exe' 'Ninja-build.Ninja'
Require-Command 'ruby.exe' 'RubyInstallerTeam.RubyWithDevKit.3.3'
$rubyCommand = Get-Command ruby.exe -ErrorAction SilentlyContinue
if (-not $rubyCommand) { $rubyCommand = Get-ChildItem 'C:\Ruby*\bin\ruby.exe' -ErrorAction SilentlyContinue | Sort-Object FullName -Descending | Select-Object -First 1 }
if (-not $rubyCommand) { throw 'Ruby was installed but ruby.exe could not be resolved.' }
$rubyPath = if ($rubyCommand -is [IO.FileInfo]) { $rubyCommand.FullName } else { $rubyCommand.Source }

$gemCommand = Get-Command gem.cmd -ErrorAction SilentlyContinue
if (-not $gemCommand) {
    $gemCommand = Get-ChildItem (Join-Path $env:LOCALAPPDATA 'Microsoft\WinGet\Packages') -Recurse -Filter gem.cmd -ErrorAction SilentlyContinue | Select-Object -First 1
}
if (-not $gemCommand) {
    $gemCommand = Get-ChildItem 'C:\Ruby*\bin\gem.cmd' -ErrorAction SilentlyContinue | Sort-Object FullName -Descending | Select-Object -First 1
}
if (-not $gemCommand) { throw 'Ruby was installed but gem.cmd could not be resolved.' }
$gemPath = if ($gemCommand -is [IO.FileInfo]) { $gemCommand.FullName } else { $gemCommand.Source }
$asciidoctor = Get-Command asciidoctor.bat -ErrorAction SilentlyContinue
if (-not $asciidoctor) {
    Write-Phase 'Installing Asciidoctor from RubyGems for the current user.'
    & $gemPath install asciidoctor --no-document --user-install
    if ($LASTEXITCODE -ne 0) { throw "RubyGems failed to install Asciidoctor (exit $LASTEXITCODE)." }
    $asciidoctor = Get-ChildItem (Join-Path $env:USERPROFILE '.local\share\gem'), (Join-Path $env:APPDATA 'gem') -Recurse -Filter asciidoctor.bat -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $asciidoctor) {
        $asciidoctor = Get-ChildItem 'C:\Ruby*\bin\asciidoctor.bat' -ErrorAction SilentlyContinue | Sort-Object FullName -Descending | Select-Object -First 1
    }
}
if (-not $asciidoctor) { throw 'Asciidoctor was installed but its executable could not be resolved.' }
$asciidoctorPath = if ($asciidoctor -is [IO.FileInfo]) { $asciidoctor.FullName } else { $asciidoctor.Source }

if (-not (Test-Path -LiteralPath $nugetExe)) {
    Write-Phase "Downloading NuGet $($manifest.nuget.version) from dist.nuget.org."
    Invoke-WebRequest -UseBasicParsing -Uri $manifest.nuget.url -OutFile $nugetExe
}
$nugetHash = Get-HashHex $nugetExe
if ($nugetHash -ne $manifest.nuget.sha256) { throw "NuGet digest mismatch: expected $($manifest.nuget.sha256), got $nugetHash." }
if (-not (Test-Path -LiteralPath $squirrelExe)) {
    Write-Phase "Restoring squirrel.windows $($manifest.squirrelWindows.version) from NuGet.org."
    & $nugetExe install $manifest.squirrelWindows.packageId -Version $manifest.squirrelWindows.version -Source $manifest.squirrelWindows.source -OutputDirectory $squirrelRoot -ExcludeVersion -NonInteractive
    if ($LASTEXITCODE -ne 0) { throw "NuGet failed to restore squirrel.windows (exit $LASTEXITCODE)." }
}
if (-not (Test-Path -LiteralPath $squirrelExe)) { throw "Squirrel.exe is missing at $squirrelExe." }
$squirrelHash = Get-HashHex $squirrelExe
if ($squirrelHash -ne $manifest.squirrelWindows.squirrelExeSha256) { throw "Squirrel.exe digest mismatch: expected $($manifest.squirrelWindows.squirrelExeSha256), got $squirrelHash." }

$receipt = [ordered]@{
    schemaVersion = 1
    generatedAtUtc = [DateTime]::UtcNow.ToString('o')
    architecture = $manifest.architecture
    nuget = @{ version = $manifest.nuget.version; path = $nugetExe; sha256 = $nugetHash }
    squirrelWindows = @{ version = $manifest.squirrelWindows.version; path = $squirrelExe; sha256 = $squirrelHash; maintenanceStatus = $manifest.squirrelWindows.maintenanceStatus }
    asciidoctor = @{ path = $asciidoctorPath }
    ruby = @{ path = $rubyPath }
}
$receiptPath = Join-Path $toolRoot 'dependency-receipt.json'
$receipt | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $receiptPath -Encoding UTF8
Write-Phase "Dependency receipt: $receiptPath"
Write-Output $receiptPath
