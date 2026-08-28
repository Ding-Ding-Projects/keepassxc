[CmdletBinding()]
param(
    [Alias('s')][switch]$Silent,
    [ValidatePattern('^\d+\.\d+\.\d+$')][string]$Version = '2.8.0',
    [bool]$WithTests = $true,
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
function Get-Sha256([string]$Path) { $s=[IO.File]::OpenRead($Path); try {$h=[Security.Cryptography.SHA256]::Create(); try {return ([BitConverter]::ToString($h.ComputeHash($s))).Replace('-','')} finally {$h.Dispose()}} finally {$s.Dispose()} }

if (-not [Environment]::Is64BitOperatingSystem) { throw 'A 64-bit Windows installation is required.' }
& (Join-Path $PSScriptRoot 'download-dependencies.ps1') -Silent:$Silent | Out-Null
$dependencyReceipt = Get-Content -Raw -LiteralPath (Join-Path $env:LOCALAPPDATA 'KeePassXCMaterial\toolchain\dependency-receipt.json') | ConvertFrom-Json
$asciidoctorExe = $dependencyReceipt.asciidoctor.path
if (-not $asciidoctorExe -or -not (Test-Path $asciidoctorExe)) { throw 'The dependency receipt does not contain a working Asciidoctor executable.' }
$rubyExe = $dependencyReceipt.ruby.path
if (-not $rubyExe -or -not (Test-Path $rubyExe)) { throw 'The dependency receipt does not contain a working Ruby executable.' }
$env:PATH = "$(Split-Path -Parent $rubyExe);$env:PATH"
$vcpkgRoot = if ($env:KPXC_VCPKG_ROOT) { $env:KPXC_VCPKG_ROOT } else { Join-Path $env:LOCALAPPDATA 'KeePassXCMaterial\toolchain\vcpkg' }
$vcpkgManifest = Get-Content -Raw -LiteralPath (Join-Path $root 'vcpkg.json') | ConvertFrom-Json
$baseline = $vcpkgManifest.'builtin-baseline'
if (-not $baseline) { throw 'vcpkg.json does not declare builtin-baseline.' }
if (-not (Test-Path (Join-Path $vcpkgRoot '.git'))) {
    New-Item -ItemType Directory -Force -Path $vcpkgRoot | Out-Null
    Invoke-Native git @('init','--quiet',$vcpkgRoot)
    Invoke-Native git @('-C',$vcpkgRoot,'remote','add','origin','https://github.com/microsoft/vcpkg.git')
}
$savedErrorAction = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
try {
    & git -C $vcpkgRoot cat-file -e "$baseline^{commit}" 2>$null
    $baselinePresent = $LASTEXITCODE -eq 0
} finally {
    $ErrorActionPreference = $savedErrorAction
}
if (-not $baselinePresent) { Invoke-Native git @('-C',$vcpkgRoot,'fetch','--depth','1','origin',$baseline) }
Invoke-Native git @('-C',$vcpkgRoot,'checkout','--force',$baseline)
if (-not (Test-Path (Join-Path $vcpkgRoot 'vcpkg.exe'))) { Invoke-Native (Join-Path $vcpkgRoot 'bootstrap-vcpkg.bat') @('-disableMetrics') }

$qtRoot = $env:QT_ROOT_DIR
if (-not $qtRoot) {
    $qtCandidates = @(
        (Join-Path $env:LOCALAPPDATA 'KeePassXCMaterial\toolchain\Qt\6.8.3\msvc2022_64'),
        (Join-Path $env:LOCALAPPDATA 'material-virtualbox-toolchain\Qt\6.8.3\msvc2022_64'),
        'C:\Qt\6.8.3\msvc2022_64'
    )
    $qtRoot = $qtCandidates | Where-Object {
        Test-Path -LiteralPath (Join-Path $_ 'bin\qmake.exe') -PathType Leaf
    } | Select-Object -First 1
}
if (-not $qtRoot) { throw 'The pinned Qt 6.8.3 MSVC x64 installation could not be found in QT_ROOT_DIR or a supported user-scoped toolchain location.' }
$qtRoot = [IO.Path]::GetFullPath($qtRoot)
$qtVersion = (& (Join-Path $qtRoot 'bin\qmake.exe') -query QT_VERSION).Trim()
if ($LASTEXITCODE -ne 0 -or $qtVersion -ne '6.8.3') { throw "Expected Qt 6.8.3 at $qtRoot; qmake reported '$qtVersion'." }
$toolchain = Join-Path $vcpkgRoot 'scripts\buildsystems\vcpkg.cmake'
$testsOption = if ($WithTests) { 'ON' } else { 'OFF' }
Phase "Configuring $build."
Invoke-Native cmake @('-S',$root,'-B',$build,'-G','Ninja','-DCMAKE_BUILD_TYPE=Release',"-DOVERRIDE_VERSION=$Version","-DWITH_TESTS=$testsOption",'-DKPXC_FEATURE_DOCS=ON',"-DASCIIDOCTOR_EXE=$asciidoctorExe","-DCMAKE_TOOLCHAIN_FILE=$toolchain",'-DVCPKG_TARGET_TRIPLET=x64-windows','-DX_VCPKG_APPLOCAL_DEPS_INSTALL=ON',"-DCMAKE_PREFIX_PATH=$qtRoot")
if ($WithTests) {
    Phase 'Building the native application and local test targets.'
    Invoke-Native cmake @('--build',$build,'--parallel')
} else {
    Phase 'Building production targets only.'
    Invoke-Native cmake @('--build',$build,'--parallel','--target','KeePassXC','keepassxc-cli','keepassxc-proxy','docs')
}
Phase "Installing to $stage."
New-Item -ItemType Directory -Force -Path $stage | Out-Null
Invoke-Native cmake @('--install',$build,'--prefix',$stage)
$exe = Join-Path $stage 'KeePassXC.exe'
if (-not (Test-Path -LiteralPath $exe)) { throw "The staged application is missing $exe." }
Write-Host "Built application: $exe"
Write-Host "SHA-256: $(Get-Sha256 $exe)"
Write-Host "Elapsed: $([int]((Get-Date)-$started).TotalSeconds)s"
if (-not $Silent -and (Read-Host 'Run KeePassXC now? [y/N]') -match '^(y|yes)$') { Start-Process -FilePath $exe }
