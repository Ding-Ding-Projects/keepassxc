[CmdletBinding()]
param([switch]$SelfTest)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$cmake = Get-Content -Raw -LiteralPath (Join-Path $root 'CMakeLists.txt')
$required = @('if(NOT WIN32)', 'if(NOT MSVC)', 'CMAKE_SIZEOF_VOID_P EQUAL 8')
$forbidden = @('option(WITH_APP_BUNDLE', 'CPACK_GENERATOR "ZIP;WIX"')

function Test-Scope([string]$Source) {
    foreach ($needle in $required) { if (-not $Source.Contains($needle)) { throw "Missing Windows-only build boundary: $needle" } }
    foreach ($needle in $forbidden) { if ($Source.Contains($needle)) { throw "Unsupported platform or package boundary remains: $needle" } }
}

Test-Scope $cmake
if ($SelfTest) {
    $turnedRed = $false
    try { Test-Scope ($cmake + "`noption(WITH_APP_BUNDLE `"break test`" ON)`n") } catch { $turnedRed = $true }
    if (-not $turnedRed) { throw 'Negative platform-scope regression did not turn red.' }
    Test-Scope $cmake
    Write-Host 'Negative platform-scope regression: red, then restored green.'
}
Write-Host 'Windows-only platform scope verified.'
