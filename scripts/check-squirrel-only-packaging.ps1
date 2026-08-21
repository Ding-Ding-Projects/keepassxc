[CmdletBinding()]
param([switch]$ProbeLegacyInstaller)

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
    'build-installer.bat' = 'scripts\build-squirrel.ps1'
    '.github\workflows\material-release.yml' = 'build-installer.bat /s -Version ${{ steps.package_version.outputs.value }}'
    'scripts\build-squirrel.ps1' = "build-windows.ps1') -Silent -Version `$Version"
    'scripts\build-windows.ps1' = '"-DOVERRIDE_VERSION=$Version"'
    'release-tool.py' = "'/s', '-Version', version"
    'scripts\verify-squirrel-artifacts.ps1' = 'Setup.exe must be unsigned'
}
foreach ($relative in $requiredText.Keys) {
    $text = Get-Content -Raw -LiteralPath (Join-Path $root $relative)
    if (-not $text.Contains($requiredText[$relative])) {
        throw "Squirrel-only contract is missing '$($requiredText[$relative])' from $relative"
    }
}

$workflow = Get-Content -Raw -LiteralPath (Join-Path $root '.github\workflows\material-release.yml')
$workflowVersionContract = @(
    '$packageVersion = "2.8.$patch"',
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

if ($ProbeLegacyInstaller) {
    throw 'Squirrel-only packaging negative-regression probe'
}
Write-Host 'PASS: Squirrel.Windows is the only registered Windows packaging path.'
