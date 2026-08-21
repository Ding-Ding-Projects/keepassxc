[CmdletBinding()]
param(
    [switch]$ProbeLegacyInstaller,
    [switch]$ProbeExecutableVersionMismatch,
    [switch]$ProbeQtBootstrapMissing,
    [switch]$ProbeReleaseVersionEnvMissing,
    [switch]$ProbeProductionBuildScopeMissing,
    [switch]$ProbeSharedReleaseConcurrency,
    [switch]$ProbeChangelogTagCheckoutMissing
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$forbiddenPaths = @(
    'cmake\WindowsCodesign.cmake.in',
    'share\windows\KPXC_ExitDlg.wxs',
    'share\windows\KPXC_InstallDir.wxs',
    'share\windows\KPXC_InstallDirDlg.wxs',
    'share\windows\installer-banner.png',
    'share\windows\installer-banner.psd',
    'share\windows\installer-wizard.png',
    'share\windows\installer-wizard.psd',
    'share\windows\installer-wizard.zip',
    'share\windows\wix-patch.xml',
    'share\windows\wix-template.xml'
)
foreach ($relative in $forbiddenPaths) {
    if (Test-Path -LiteralPath (Join-Path $root $relative)) {
        throw "Legacy installer path remains: $relative"
    }
}

$sources = @('src\CMakeLists.txt', 'release-tool.py', '.github\workflows\material-release.yml')
$forbiddenText = @('CPACK_WIX', 'CPACK_NSIS', 'ZIP;WIX', 'candle.exe', 'light.exe', 'heat.exe', '*.msi')
foreach ($relative in $sources) {
    $text = Get-Content -Raw -LiteralPath (Join-Path $root $relative)
    foreach ($needle in $forbiddenText) {
        if ($text.Contains($needle)) {
            throw "Legacy installer registration '$needle' remains in $relative"
        }
    }
}

$requiredText = @{
    'build-installer.bat' = @('scripts\build-squirrel.ps1')
    '.github\workflows\material-release.yml' = @('build-installer.bat /s -Version ${{ steps.package_version.outputs.value }}')
    'scripts\build-squirrel.ps1' = @("build-windows.ps1') -Silent -Version `$Version")
    'scripts\build-windows.ps1' = @('"-DOVERRIDE_VERSION=$Version"')
    'release-tool.py' = @("'/s', '-Version', version")
    'scripts\verify-squirrel-artifacts.ps1' = @('Setup.exe must be unsigned', 'Assert-KpxcExecutableVersion')
}
foreach ($relative in $requiredText.Keys) {
    $text = Get-Content -Raw -LiteralPath (Join-Path $root $relative)
    foreach ($needle in $requiredText[$relative]) {
        if (-not $text.Contains($needle)) {
            throw "Squirrel-only contract is missing '$needle' from $relative"
        }
    }
}

$workflow = Get-Content -Raw -LiteralPath (Join-Path $root '.github\workflows\material-release.yml')
if ($ProbeSharedReleaseConcurrency) {
    $sharedConcurrency = @'
concurrency:
  group: material-squirrel-release-${{ github.ref }}
  cancel-in-progress: false

'@
    $workflow = $sharedConcurrency + $workflow
}
$cancelEnabled = [regex]::IsMatch($workflow, '(?mi)^\s*cancel-in-progress:\s*true\s*$')
if ($cancelEnabled) {
    throw 'Release workflow must never cancel an earlier run in progress.'
}
$concurrencyGroups = [regex]::Matches($workflow, '(?mi)^\s*group:\s*(?<value>.+?)\s*$')
foreach ($group in $concurrencyGroups) {
    $value = $group.Groups['value'].Value
    if ($value.Contains('github.ref') -or -not $value.Contains('github.run_id')) {
        throw "Release workflow concurrency can replace pending runs unless it is unique per run id: $value"
    }
}
$checkoutMatch = [regex]::Match(
    $workflow,
    '(?ms)^\s{6}- name: Checkout\r?\n(?<step>.*?)(?=^\s{6}- name:|\z)')
if (-not $checkoutMatch.Success) { throw 'Release checkout step cannot be located for provenance validation.' }
$checkoutStep = $checkoutMatch.Groups['step'].Value
if ($ProbeChangelogTagCheckoutMissing) {
    $checkoutStep = $checkoutStep.Replace('fetch-depth: 0', 'fetch-depth: 1')
}
foreach ($needle in @('uses: actions/checkout@v7', 'fetch-depth: 0', 'fetch-tags: true')) {
    if (-not $checkoutStep.Contains($needle)) {
        throw "Changelog provenance checkout is incomplete: $needle"
    }
}
$tagVerificationContract = @(
    '- name: Verify Changelog provenance history',
    'git rev-parse --is-shallow-repository',
    'git tag --list',
    'git rev-parse --verify "$tag^{commit}"',
    'git cat-file -e "$commit^{commit}"'
)
foreach ($needle in $tagVerificationContract) {
    if (-not $workflow.Contains($needle)) {
        throw "Changelog provenance history verification is missing: $needle"
    }
}
$workflowVersionContract = @(
    '$packageVersion = "$major.$minor.$patch"',
    'package-version: ${{ steps.package_version.outputs.value }}',
    'PACKAGE_VERSION: ${{ needs.package-windows.outputs.package-version }}',
    'RELEASE_TAG: v${{ needs.package-windows.outputs.package-version }}',
    '--title "KeePassXC Material ${PACKAGE_VERSION}"'
)
foreach ($needle in $workflowVersionContract) {
    if (-not $workflow.Contains($needle)) {
        throw "Monotonic package version contract is missing from the workflow: $needle"
    }
}

$createReleaseMatch = [regex]::Match(
    $workflow,
    '(?ms)^\s{6}- name: Create the GitHub Release\r?\n(?<step>.*?)(?=^\s{6}- name:|\z)')
if (-not $createReleaseMatch.Success) {
    throw 'Create the GitHub Release step cannot be located for environment validation.'
}
$createReleaseStep = $createReleaseMatch.Groups['step'].Value
if ($ProbeReleaseVersionEnvMissing) {
    $createReleaseStep = $createReleaseStep.Replace(
        'PACKAGE_VERSION: ${{ needs.package-windows.outputs.package-version }}',
        'PACKAGE_VERSION: missing')
}
$releaseStepVersionContract = @(
    'PACKAGE_VERSION: ${{ needs.package-windows.outputs.package-version }}',
    'RELEASE_TAG: v${{ needs.package-windows.outputs.package-version }}',
    'gh release create "${RELEASE_TAG}"',
    '--title "KeePassXC Material ${PACKAGE_VERSION}"'
)
foreach ($needle in $releaseStepVersionContract) {
    if (-not $createReleaseStep.Contains($needle)) {
        throw "Create Release step is missing its local version environment boundary: $needle"
    }
}

$qtBootstrapContract = @(
    'uses: jurplel/install-qt-action@48d3ad6db93f3627c8ee7a0454bc6f3744f7e730',
    'version: 6.8.3',
    'arch: win64_msvc2022_64',
    '"QT_ROOT_DIR=$qtRoot" | Out-File -FilePath $env:GITHUB_ENV',
    "VC\Auxiliary\Build\vcvars64.bat",
    'build-installer.bat /s -Version ${{ steps.package_version.outputs.value }}'
)
$qtWorkflow = if ($ProbeQtBootstrapMissing) {
    $workflow.Replace($qtBootstrapContract[0], 'uses: missing-qt-bootstrap')
} else {
    $workflow
}
foreach ($needle in $qtBootstrapContract) {
    if (-not $qtWorkflow.Contains($needle)) {
        throw "Pinned Qt/MSVC bootstrap contract is missing from the workflow: $needle"
    }
}

$cmake = Get-Content -Raw -LiteralPath (Join-Path $root 'CMakeLists.txt')
foreach ($needle in @('set(KEEPASSXC_VERSION_MAJOR "${CMAKE_MATCH_1}")',
                       'set(KEEPASSXC_VERSION_MINOR "${CMAKE_MATCH_2}")',
                       'set(KEEPASSXC_VERSION_PATCH "${CMAKE_MATCH_3}")')) {
    if (-not $cmake.Contains($needle)) { throw "Native executable version propagation is missing: $needle" }
}

if ($ProbeExecutableVersionMismatch) {
    . (Join-Path $PSScriptRoot 'ExecutableVersionContract.ps1')
    Assert-KpxcExecutableVersion -FileVersion '2.8.0.0' -ProductVersion '2.8.0.0' -ExpectedVersion '2.8.1'
}

$squirrelBuild = Get-Content -Raw -LiteralPath (Join-Path $root 'scripts\build-squirrel.ps1')
$nativeBuild = Get-Content -Raw -LiteralPath (Join-Path $root 'scripts\build-windows.ps1')
if ($ProbeProductionBuildScopeMissing) {
    $squirrelBuild = $squirrelBuild.Replace('-WithTests:$false', '-WithTests:$true')
}
$productionBuildContract = @(
    @{ Text=$squirrelBuild; Needle='-WithTests:$false'; Source='scripts\build-squirrel.ps1' },
    @{ Text=$nativeBuild; Needle='[bool]$WithTests = $true'; Source='scripts\build-windows.ps1' },
    @{ Text=$nativeBuild; Needle='$testsOption = if ($WithTests)'; Source='scripts\build-windows.ps1' },
    @{ Text=$nativeBuild; Needle='"-DWITH_TESTS=$testsOption"'; Source='scripts\build-windows.ps1' },
    @{ Text=$nativeBuild; Needle="'--target','KeePassXC','keepassxc-cli','keepassxc-proxy','docs'"; Source='scripts\build-windows.ps1' }
)
foreach ($item in $productionBuildContract) {
    if (-not $item.Text.Contains($item.Needle)) {
        throw "Production-only installer build contract is missing '$($item.Needle)' from $($item.Source)"
    }
}

if ($ProbeLegacyInstaller) {
    throw 'Squirrel-only packaging negative-regression probe'
}
Write-Host 'PASS: Squirrel.Windows is the only registered Windows packaging path.'
