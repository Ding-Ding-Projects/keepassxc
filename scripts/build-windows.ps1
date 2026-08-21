[CmdletBinding()]
param(
    [Alias('s')][switch]$Silent,
    [string]$BuildDirectory = 'build-windows',
    [string]$InstallDirectory = 'stage\app'
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$build = [IO.Path]::GetFullPath((Join-Path $root $BuildDirectory))
$stage = [IO.Path]::GetFullPath((Join-Path $root $InstallDirectory))
$started = Get-Date
function Phase([string]$Message) { if (-not $Silent) { Write-Host "[build] $Message" } }
function Invoke-Native([string]$File, [string[]]$Arguments) { & $File @Arguments; if ($LASTEXITCODE -ne 0) { throw "$File exited with $LASTEXITCODE." } }

if (-not [Environment]::Is64BitOperatingSystem) { throw 'A 64-bit Windows installation is required.' }
& (Join-Path $PSScriptRoot 'download-dependencies.ps1') -Silent:$Silent | Out-Null
$vcpkgRoot = if ($env:VCPKG_ROOT) { $env:VCPKG_ROOT } else { Join-Path $env:LOCALAPPDATA 'KeePassXCMaterial\toolchain\vcpkg' }
$vcpkgManifest = Get-Content -Raw -LiteralPath (Join-Path $root 'vcpkg.json') | ConvertFrom-Json
$baseline = $vcpkgManifest.'builtin-baseline'
if (-not $baseline) { throw 'vcpkg.json does not declare builtin-baseline.' }
if (-not (Test-Path (Join-Path $vcpkgRoot '.git'))) {
    New-Item -ItemType Directory -Force -Path $vcpkgRoot | Out-Null
    Invoke-Native git @('init','--quiet',$vcpkgRoot)
    Invoke-Native git @('-C',$vcpkgRoot,'remote','add','origin','https://github.com/microsoft/vcpkg.git')
}
& git -C $vcpkgRoot cat-file -e "$baseline^{commit}" 2>$null
if ($LASTEXITCODE -ne 0) { Invoke-Native git @('-C',$vcpkgRoot,'fetch','--depth','1','origin',$baseline) }
Invoke-Native git @('-C',$vcpkgRoot,'checkout','--force',$baseline)
if (-not (Test-Path (Join-Path $vcpkgRoot 'vcpkg.exe'))) { Invoke-Native (Join-Path $vcpkgRoot 'bootstrap-vcpkg.bat') @('-disableMetrics') }

$qtRoot = $env:QT_ROOT_DIR
if (-not $qtRoot) { throw 'QT_ROOT_DIR must identify the pinned Qt 6.8.3 MSVC x64 installation.' }
$toolchain = Join-Path $vcpkgRoot 'scripts\buildsystems\vcpkg.cmake'
Phase "Configuring $build."
Invoke-Native cmake @('-S',$root,'-B',$build,'-G','Ninja','-DCMAKE_BUILD_TYPE=Release','-DWITH_TESTS=ON','-DKPXC_FEATURE_DOCS=ON',"-DCMAKE_TOOLCHAIN_FILE=$toolchain",'-DVCPKG_TARGET_TRIPLET=x64-windows','-DX_VCPKG_APPLOCAL_DEPS_INSTALL=ON',"-DCMAKE_PREFIX_PATH=$qtRoot")
Phase 'Building the native application.'
Invoke-Native cmake @('--build',$build,'--parallel')
Phase "Installing to $stage."
New-Item -ItemType Directory -Force -Path $stage | Out-Null
Invoke-Native cmake @('--install',$build,'--prefix',$stage)
$exe = Join-Path $stage 'KeePassXC.exe'
if (-not (Test-Path -LiteralPath $exe)) { throw "The staged application is missing $exe." }
Write-Host "Built application: $exe"
Write-Host "SHA-256: $((Get-FileHash $exe -Algorithm SHA256).Hash)"
Write-Host "Elapsed: $([int]((Get-Date)-$started).TotalSeconds)s"
if (-not $Silent -and (Read-Host 'Run KeePassXC now? [y/N]') -match '^(y|yes)$') { Start-Process -FilePath $exe }
