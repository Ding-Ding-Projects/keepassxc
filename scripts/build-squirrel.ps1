[CmdletBinding()]
param(
    [Alias('s')][switch]$Silent,
    [ValidatePattern('^\d+\.\d+\.\d+$')][string]$Version = '2.8.0',
    [string]$StageDirectory = 'stage\app',
    [string]$ArtifactDirectory = 'dist\squirrel-windows',
    [string]$ReleaseBaseUrl
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$stage = [IO.Path]::GetFullPath((Join-Path $root $StageDirectory))
$output = [IO.Path]::GetFullPath((Join-Path $root $ArtifactDirectory))
$scratch = [IO.Path]::GetFullPath((Join-Path $root 'stage\squirrel'))
function Get-Sha256([string]$Path) { $s=[IO.File]::OpenRead($Path); try {$h=[Security.Cryptography.SHA256]::Create(); try {return ([BitConverter]::ToString($h.ComputeHash($s))).Replace('-','').ToLowerInvariant()} finally {$h.Dispose()}} finally {$s.Dispose()} }
& (Join-Path $PSScriptRoot 'build-windows.ps1') -Silent -InstallDirectory $StageDirectory
& (Join-Path $PSScriptRoot 'download-dependencies.ps1') -Silent | Out-Null
$manifest = Get-Content -Raw -LiteralPath (Join-Path $root 'packaging\squirrel\toolchain.json') | ConvertFrom-Json
$toolRoot = Join-Path $env:LOCALAPPDATA 'KeePassXCMaterial\toolchain'
$nugetExe = Join-Path $toolRoot "nuget-$($manifest.nuget.version)\nuget.exe"
$squirrelExe = Join-Path $toolRoot "squirrel-windows-$($manifest.squirrelWindows.version)\squirrel.windows\tools\Squirrel.exe"
$appExe = Join-Path $stage 'KeePassXC.exe'
if (-not (Test-Path $appExe)) { throw "Staged executable not found at $appExe." }
New-Item -ItemType Directory -Force -Path $scratch, $output | Out-Null
Get-ChildItem -LiteralPath $output -Force -ErrorAction SilentlyContinue | Remove-Item -Force -Recurse
$nuspec = Join-Path $scratch 'KeePassXC.Material.nuspec'
@"
<?xml version="1.0"?>
<package xmlns="http://schemas.microsoft.com/packaging/2010/07/nuspec.xsd">
  <metadata><id>KeePassXC.Material</id><version>$Version</version><title>KeePassXC Material</title><authors>KeePassXC Team</authors><owners>KeePassXC Team</owners><requireLicenseAcceptance>false</requireLicenseAcceptance><description>Windows-only KeePassXC Material desktop application.</description></metadata>
  <files><file src="$stage\**\*" target="lib\net45" /></files>
</package>
"@ | Set-Content -LiteralPath $nuspec -Encoding UTF8
& $nugetExe pack $nuspec -OutputDirectory $scratch -NoPackageAnalysis -NonInteractive
if ($LASTEXITCODE -ne 0) { throw "NuGet pack failed with exit $LASTEXITCODE." }
$package = Get-ChildItem $scratch -Filter "KeePassXC.Material.$Version.nupkg" | Select-Object -First 1
if (-not $package) { throw 'NuGet did not produce the expected application package.' }
& $squirrelExe --releasify $package.FullName --releaseDir $output --no-msi
if ($LASTEXITCODE -ne 0) { throw "Squirrel releasify failed with exit $LASTEXITCODE." }
$deadline = [DateTime]::UtcNow.AddMinutes(2)
$lastSizes = ''
$stableObservations = 0
do {
    $setupCandidate = Join-Path $output 'Setup.exe'
    $releasesCandidate = Join-Path $output 'RELEASES'
    $fullCandidate = @(Get-ChildItem -LiteralPath $output -Filter '*-full.nupkg' -ErrorAction SilentlyContinue)
    if ((Test-Path $setupCandidate) -and (Test-Path $releasesCandidate) -and $fullCandidate.Count -eq 1) {
        $sizes = "$(Get-Item $setupCandidate | Select-Object -ExpandProperty Length):$((Get-Item $releasesCandidate).Length):$($fullCandidate[0].Length)"
        if ($sizes -eq $lastSizes) { ++$stableObservations } else { $stableObservations = 0; $lastSizes = $sizes }
    }
    if ($stableObservations -lt 2) { Start-Sleep -Milliseconds 500 }
} while ($stableObservations -lt 2 -and [DateTime]::UtcNow -lt $deadline)
if ($stableObservations -lt 2) { throw 'Squirrel did not produce a size-stable Setup.exe, RELEASES, and full package within two minutes.' }
$commit = (& git -C $root rev-parse HEAD).Trim()
$provenance = [ordered]@{ schemaVersion=1; sourceCommit=$commit; version=$Version; architecture='x64'; packageId='KeePassXC.Material'; packagingTool=@{name='squirrel.windows';version=$manifest.squirrelWindows.version;maintenanceStatus=$manifest.squirrelWindows.maintenanceStatus}; stagedExecutable=@{path=$appExe;sha256=(Get-Sha256 $appExe)}; generatedAtUtc=[DateTime]::UtcNow.ToString('o') }
$provenancePath = Join-Path $output 'build-provenance.json'
$provenance | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $provenancePath -Encoding UTF8
& (Join-Path $PSScriptRoot 'verify-squirrel-artifacts.ps1') -ArtifactDirectory $output -ProvenancePath $provenancePath -ExpectedCommit $commit -ExpectedVersion $Version -ExpectedPackageId 'KeePassXC.Material' -ExpectedArchitecture x64 -RequiredPackageEntry 'lib/net45/KeePassXC.exe' -OutputPath (Join-Path $output 'artifact-receipt.json')
$receipt = Get-Content -Raw -LiteralPath (Join-Path $output 'artifact-receipt.json') | ConvertFrom-Json
$fullPackage = $receipt.fullPackages | Select-Object -First 1
$releaseRow = Get-Content -LiteralPath (Join-Path $output 'RELEASES') | Where-Object { $_ -match [regex]::Escape($fullPackage.name) } | Select-Object -First 1
if (-not $releaseRow -or $releaseRow -notmatch '^([0-9a-fA-F]{40})\s+') { throw 'The verified full package has no canonical RELEASES SHA-1 row.' }
if (-not $ReleaseBaseUrl) { $ReleaseBaseUrl = "https://github.com/Ding-Ding-Projects/keepassxc/releases/download/v$Version" }
$manifestOutput = [ordered]@{
    schemaVersion = 1
    packageId = 'KeePassXC.Material'
    architecture = 'x64'
    version = $Version
    notesUrl = "https://github.com/Ding-Ding-Projects/keepassxc/releases/tag/v$Version"
    packageUrl = "$($ReleaseBaseUrl.TrimEnd('/'))/$($fullPackage.name)"
    packageFile = $fullPackage.name
    bytes = [int64]$fullPackage.bytes
    sha256 = $fullPackage.sha256
    releasesSha1 = $Matches[1].ToLowerInvariant()
    executableSha256 = $provenance.stagedExecutable.sha256
}
$manifestOutput | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $output 'update-manifest-v1.json') -Encoding UTF8
Write-Host 'Unsigned Squirrel.Windows artifacts were built successfully.'
Write-Host 'They may trigger Unknown Publisher or SmartScreen warnings.'
Get-ChildItem $output -File | Select-Object Name,Length,@{Name='SHA256';Expression={Get-Sha256 $_.FullName}}
