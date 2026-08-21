[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$ArtifactDirectory,
    [Parameter(Mandatory)][string]$ProvenancePath,
    [Parameter(Mandatory)][string]$ExpectedCommit,
    [Parameter(Mandatory)][string]$ExpectedPackageId,
    [Parameter(Mandatory)][string]$ExpectedVersion,
    [Parameter(Mandatory)][ValidateSet('x64','arm64')][string]$ExpectedArchitecture,
    [Parameter(Mandatory)][string]$RequiredPackageEntry,
    [Parameter(Mandatory)][string]$OutputPath,
    [switch]$RequireDelta
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'ExecutableVersionContract.ps1')
$securityModule = Join-Path $PSHOME 'Modules\Microsoft.PowerShell.Security\Microsoft.PowerShell.Security.psd1'
if (Test-Path -LiteralPath $securityModule) { Import-Module $securityModule -ErrorAction Stop }
$dir = [IO.Path]::GetFullPath($ArtifactDirectory)
$setup = Join-Path $dir 'Setup.exe'
$releases = Join-Path $dir 'RELEASES'
$full = @(Get-ChildItem -LiteralPath $dir -Filter '*-full.nupkg')
$delta = @(Get-ChildItem -LiteralPath $dir -Filter '*-delta.nupkg')
function Get-HashHex([string]$Path, [string]$Algorithm) { $s=[IO.File]::OpenRead($Path); try {$h=[Security.Cryptography.HashAlgorithm]::Create($Algorithm); try {return ([BitConverter]::ToString($h.ComputeHash($s))).Replace('-','').ToLowerInvariant()} finally {$h.Dispose()}} finally {$s.Dispose()} }
if (-not (Test-Path $setup)) { throw 'Setup.exe is missing.' }
if (-not (Test-Path $releases)) { throw 'RELEASES is missing.' }
if ($full.Count -ne 1) { throw "Expected exactly one full package; found $($full.Count)." }
if ((Get-Item $setup).Length -lt $full[0].Length) { throw 'Setup.exe is smaller than the full package and appears to be an incomplete Squirrel stub.' }
if ($RequireDelta -and $delta.Count -eq 0) { throw 'A required delta package is missing.' }
$provenance = Get-Content -Raw -LiteralPath $ProvenancePath | ConvertFrom-Json
if ($provenance.sourceCommit -ne $ExpectedCommit -or $provenance.version -ne $ExpectedVersion -or $provenance.packageId -ne $ExpectedPackageId -or $provenance.architecture -ne $ExpectedArchitecture) { throw 'Build provenance does not match the expected candidate.' }

$indexed = @{}
foreach ($line in Get-Content -LiteralPath $releases) {
    if ($line -notmatch '^([0-9a-fA-F]{40})\s+([^\s]+)\s+(\d+)(?:\s+.*)?$') { throw "Malformed RELEASES row: $line" }
    $name = $Matches[2]
    if ([IO.Path]::GetFileName($name) -ne $name) { throw "Unsafe package path in RELEASES: $name" }
    $path = Join-Path $dir $name
    if (-not (Test-Path $path)) { throw "Indexed package is missing: $name" }
    if ((Get-Item $path).Length -ne [int64]$Matches[3]) { throw "Indexed size differs for $name" }
    if ((Get-HashHex $path 'SHA1') -ine $Matches[1]) { throw "Indexed SHA-1 differs for $name" }
    $indexed[$name] = $true
}
foreach ($pkg in @($full) + @($delta)) { if (-not $indexed.ContainsKey($pkg.Name)) { throw "Package is not indexed by RELEASES: $($pkg.Name)" } }

Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = [IO.Compression.ZipFile]::OpenRead($full[0].FullName)
$packagedExecutable = $null
$versionInfo = $null
try {
    $seen = @{}
    foreach ($item in $zip.Entries) {
        $normalized = $item.FullName.Replace('\','/').ToLowerInvariant()
        if ($normalized.StartsWith('/') -or $normalized -match '(^|/)\.\.(/|$)') { throw "Unsafe package entry: $($item.FullName)" }
        if ($seen.ContainsKey($normalized)) { throw "Duplicate package entry: $($item.FullName)" }
        $seen[$normalized] = $true
    }
    if (-not $seen.ContainsKey($RequiredPackageEntry.ToLowerInvariant())) { throw "Required package entry is missing: $RequiredPackageEntry" }
    $entry = $zip.Entries | Where-Object { $_.FullName.Replace('\','/').Equals($RequiredPackageEntry, [StringComparison]::OrdinalIgnoreCase) } | Select-Object -First 1
    if (-not $entry) { throw "Required package entry cannot be opened: $RequiredPackageEntry" }
    $packagedExecutable = Join-Path ([IO.Path]::GetTempPath()) ("keepassxc-version-{0}.exe" -f [Guid]::NewGuid().ToString('N'))
    $input = $entry.Open()
    $output = [IO.File]::Create($packagedExecutable)
    try { $input.CopyTo($output) } finally { $output.Dispose(); $input.Dispose() }
    $versionInfo = [Diagnostics.FileVersionInfo]::GetVersionInfo($packagedExecutable)
    Assert-KpxcExecutableVersion -FileVersion $versionInfo.FileVersion -ProductVersion $versionInfo.ProductVersion -ExpectedVersion $ExpectedVersion
} finally {
    $zip.Dispose()
    if ($packagedExecutable) { Remove-Item -LiteralPath $packagedExecutable -Force -ErrorAction SilentlyContinue }
}

$signature = Get-AuthenticodeSignature -FilePath $setup
if ($signature.Status -ne 'NotSigned') { throw "Setup.exe must be unsigned; status is $($signature.Status)." }
$receipt = [ordered]@{
    schemaVersion = 1; sourceCommit = $ExpectedCommit; packageId = $ExpectedPackageId; version = $ExpectedVersion; architecture = $ExpectedArchitecture
    setup = @{ name='Setup.exe'; bytes=(Get-Item $setup).Length; sha256=(Get-HashHex $setup 'SHA256'); signingStatus=$signature.Status.ToString() }
    releases = @{ sha256=(Get-HashHex $releases 'SHA256') }
    fullPackages = @($full | ForEach-Object { @{name=$_.Name;bytes=$_.Length;sha256=(Get-HashHex $_.FullName 'SHA256')} })
    deltaPackages = @($delta | ForEach-Object { @{name=$_.Name;bytes=$_.Length;sha256=(Get-HashHex $_.FullName 'SHA256')} })
    requiredPackageEntry = $RequiredPackageEntry
    packagedExecutable = @{ path=$RequiredPackageEntry; fileVersion=$versionInfo.FileVersion; productVersion=$versionInfo.ProductVersion }
}
$receipt | ConvertTo-Json -Depth 7 | Set-Content -LiteralPath $OutputPath -Encoding UTF8
Write-Host "Verified unsigned Squirrel artifacts. Receipt: $OutputPath"
