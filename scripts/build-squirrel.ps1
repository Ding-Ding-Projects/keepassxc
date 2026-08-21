[CmdletBinding()]
param(
    [Alias('s')][switch]$Silent,
    [ValidatePattern('^\d+\.\d+\.\d+$')][string]$Version = '2.8.0',
    [string]$StageDirectory = 'stage\app',
    [string]$ArtifactDirectory = 'dist\squirrel-windows'
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$stage = [IO.Path]::GetFullPath((Join-Path $root $StageDirectory))
$output = [IO.Path]::GetFullPath((Join-Path $root $ArtifactDirectory))
$scratch = [IO.Path]::GetFullPath((Join-Path $root 'stage\squirrel'))
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
$commit = (& git -C $root rev-parse HEAD).Trim()
$provenance = [ordered]@{ schemaVersion=1; sourceCommit=$commit; version=$Version; architecture='x64'; packageId='KeePassXC.Material'; packagingTool=@{name='squirrel.windows';version=$manifest.squirrelWindows.version;maintenanceStatus=$manifest.squirrelWindows.maintenanceStatus}; stagedExecutable=@{path=$appExe;sha256=(Get-FileHash $appExe -Algorithm SHA256).Hash.ToLowerInvariant()}; generatedAtUtc=[DateTime]::UtcNow.ToString('o') }
$provenancePath = Join-Path $output 'build-provenance.json'
$provenance | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $provenancePath -Encoding UTF8
& (Join-Path $PSScriptRoot 'verify-squirrel-artifacts.ps1') -ArtifactDirectory $output -ProvenancePath $provenancePath -ExpectedCommit $commit -ExpectedVersion $Version -ExpectedPackageId 'KeePassXC.Material' -ExpectedArchitecture x64 -RequiredPackageEntry 'lib/net45/KeePassXC.exe' -OutputPath (Join-Path $output 'artifact-receipt.json')
Write-Host 'Unsigned Squirrel.Windows artifacts were built successfully.'
Write-Host 'They may trigger Unknown Publisher or SmartScreen warnings.'
Get-ChildItem $output -File | Select-Object Name,Length,@{Name='SHA256';Expression={(Get-FileHash $_.FullName -Algorithm SHA256).Hash}}
