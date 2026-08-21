[CmdletBinding()]
param([switch]$SelfTest)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$cmake = Get-Content -Raw -LiteralPath (Join-Path $root 'CMakeLists.txt')
$platformSources = $cmake + "`n" + (Get-Content -Raw -LiteralPath (Join-Path $root 'src\CMakeLists.txt')) + "`n" + (Get-Content -Raw -LiteralPath (Join-Path $root 'src\autotype\CMakeLists.txt')) + "`n" + (Get-Content -Raw -LiteralPath (Join-Path $root 'share\CMakeLists.txt')) + "`n" + (Get-Content -Raw -LiteralPath (Join-Path $root 'docs\CMakeLists.txt'))
$required = @('if(NOT WIN32)', 'if(NOT MSVC)', 'CMAKE_SIZEOF_VOID_P EQUAL 8')
$forbidden = @('option(WITH_APP_BUNDLE', 'if(APPLE)', 'elseif(APPLE', 'WITH_APP_BUNDLE', 'DragNDrop', 'macdeployqt', 'AutoTypeMac.cpp', 'TouchID.mm', 'if(UNIX)')
$forbiddenPaths = @('src\gui\osutils\macutils', 'src\autotype\mac', 'src\quickunlock\TouchID.h', 'src\quickunlock\TouchID.mm', 'share\macosx', 'cmake\compiler-checks\macos', 'cmake\MacOSCodesign.cmake.in', 'cmake\KPXCMacDeployHelpers.cmake')

function Test-Scope([string]$Source) {
    foreach ($needle in $required) { if (-not $Source.Contains($needle)) { throw "Missing Windows-only build boundary: $needle" } }
    foreach ($needle in $forbidden) { if ($Source.Contains($needle)) { throw "Unsupported platform or package boundary remains: $needle" } }
    foreach ($relative in $forbiddenPaths) { if (Test-Path -LiteralPath (Join-Path $root $relative)) { throw "Unsupported platform path remains: $relative" } }
}

Test-Scope $platformSources
if ($SelfTest) {
    $turnedRed = $false
    try { Test-Scope ($platformSources + "`noption(WITH_APP_BUNDLE `"break test`" ON)`n") } catch { $turnedRed = $true }
    if (-not $turnedRed) { throw 'Negative platform-scope regression did not turn red.' }
    Test-Scope $platformSources
    Write-Host 'Negative platform-scope regression: red, then restored green.'
}
Write-Host 'Windows-only platform scope verified.'
