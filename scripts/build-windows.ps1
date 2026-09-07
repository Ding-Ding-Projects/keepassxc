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
. (Join-Path $PSScriptRoot 'PackagingSafety.ps1')
. (Join-Path $PSScriptRoot 'ExecutableVersionContract.ps1')
$build = Resolve-KpxcDirectory $root $BuildDirectory
$stage = Resolve-KpxcDirectory $root $InstallDirectory
Assert-KpxcBuildPaths $root @($build, $stage)
Assert-KpxcBuildCache $root $build
$sourceCommit = (& git -C $root rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or (& git -C $root status --porcelain)) { throw 'Build provenance requires a clean, committed source checkout.' }
$stageReceiptPath = Join-Path $build 'stage-provenance.json'
if ((Test-Path -LiteralPath $stage) -and @(Get-ChildItem -LiteralPath $stage -Force).Count) {
    $previous = Assert-KpxcStageReceipt $stage $stageReceiptPath $sourceCommit $Version
    if ($previous.ownerKey -cne (Get-KpxcOwnerKey $root $stage)) { throw 'Existing stage is not owned by this build. Choose an empty InstallDirectory.' }
}
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
# Export the MSVC x64 environment when cl.exe is not already reachable. The
# release workflow does this with vswhere; a plain local shell has no INCLUDE
# or LIB at all, and the first compile then fails on <assert.h>.
if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    $vcvars = $null
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere -PathType Leaf) {
        $vsRoot = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($vsRoot) { $vcvars = Join-Path $vsRoot 'VC\Auxiliary\Build\vcvars64.bat' }
    }
    if (-not $vcvars -or -not (Test-Path -LiteralPath $vcvars -PathType Leaf)) {
        $vcvars = @(
            (Join-Path $env:LOCALAPPDATA 'KeePassXCMaterial\toolchain\BuildTools\VC\Auxiliary\Build\vcvars64.bat'),
            (Join-Path $env:LOCALAPPDATA 'material-virtualbox-toolchain\BuildTools\VC\Auxiliary\Build\vcvars64.bat')
        ) | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
    }
    if (-not $vcvars) { throw 'No MSVC x64 toolchain was found: vswhere reported no VC.Tools.x86.x64 installation and no user-scoped Build Tools vcvars64.bat exists.' }
    Phase "Exporting the MSVC x64 environment from $vcvars."
    cmd.exe /d /s /c "call `"$vcvars`" >nul && set" | ForEach-Object {
        if ($_ -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($matches[1], $matches[2], 'Process') }
    }
    if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) { throw "vcvars64.bat did not put cl.exe on PATH: $vcvars" }
}
$toolchain = Join-Path $vcpkgRoot 'scripts\buildsystems\vcpkg.cmake'
$compilerPath = (Get-Command cl.exe -ErrorAction Stop).Source
$testsOption = if ($WithTests) { 'ON' } else { 'OFF' }
Phase "Configuring $build."
Invoke-Native cmake @('-S',$root,'-B',$build,'-G','Ninja','-DCMAKE_BUILD_TYPE=Release',"-DOVERRIDE_VERSION=$Version","-DWITH_TESTS=$testsOption",'-DKPXC_FEATURE_DOCS=ON',"-DASCIIDOCTOR_EXE=$asciidoctorExe","-DCMAKE_TOOLCHAIN_FILE=$toolchain",'-DVCPKG_TARGET_TRIPLET=x64-windows','-DX_VCPKG_APPLOCAL_DEPS_INSTALL=ON',"-DCMAKE_PREFIX_PATH=$qtRoot","-DCMAKE_C_COMPILER=$compilerPath","-DCMAKE_CXX_COMPILER=$compilerPath")
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
$runtime = @(Copy-KpxcMsvcRuntime $stage $compilerPath $env:VCToolsRedistDir)
Assert-KpxcPeX64 $exe
$versionInfo = [Diagnostics.FileVersionInfo]::GetVersionInfo($exe)
Assert-KpxcExecutableVersion -FileVersion $versionInfo.FileVersion -ProductVersion $versionInfo.ProductVersion -ExpectedVersion $Version
$builtExe = Join-Path $build 'src\KeePassXC.exe'
if ((Get-KpxcHash $exe) -cne (Get-KpxcHash $builtExe)) { throw 'Installed executable differs from the current build output.' }
$generatedConfig = Get-Content -Raw -LiteralPath (Join-Path $build 'src\config-keepassx.h')
if ($generatedConfig -notmatch '(?m)^#define KEEPASSXC_GIT_HEAD "([0-9a-f]+)"\r?$' -or
    $Matches[1].Length -lt 7 -or -not $sourceCommit.StartsWith($Matches[1], [StringComparison]::Ordinal)) { throw 'CMake provenance does not name the current source commit.' }
$compiledHead = $Matches[1]
$binaryText = [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($exe))
if (-not $binaryText.Contains($compiledHead)) { throw 'The staged executable does not contain its configured source identifier.' }
if ((& git -C $root rev-parse HEAD).Trim() -cne $sourceCommit -or (& git -C $root status --porcelain)) { throw 'Source changed during the build; no stage receipt was issued.' }
$receipt = @{schemaVersion=1;sourceCommit=$sourceCommit;compiledHead=$compiledHead;version=$Version;architecture='x64';stageDirectory=$stage;ownerKey=(Get-KpxcOwnerKey $root $stage);executableSha256=(Get-KpxcHash $exe);msvcRuntime=$runtime;files=@(Get-KpxcDirectoryFiles $stage | ForEach-Object { @{path=$_.FullName.Substring($stage.Length + 1).Replace('\','/');sha256=(Get-KpxcHash $_.FullName)} })}
$receipt | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $stageReceiptPath -Encoding UTF8
Assert-KpxcStageReceipt $stage $stageReceiptPath $sourceCommit $Version | Out-Null
Write-Host "Stage provenance: $stageReceiptPath"
Write-Host "Built application: $exe"
Write-Host "SHA-256: $(Get-Sha256 $exe)"
Write-Host "Elapsed: $([int]((Get-Date)-$started).TotalSeconds)s"
if (-not $Silent -and (Read-Host 'Run KeePassXC now? [y/N]') -match '^(y|yes)$') { Start-Process -FilePath $exe }
