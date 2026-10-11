$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'PackagingSafety.ps1')
$root = [IO.Path]::GetFullPath((Join-Path ([IO.Path]::GetTempPath()) 'keepassxc-runtime-layout-test'))
foreach ($case in @(@(30, 'Microsoft.VC143.CRT'), @(44, 'Microsoft.VC143.CRT'), @(50, 'Microsoft.VC145.CRT'), @(51, 'Microsoft.VC145.CRT'))) {
    $actual = Get-KpxcMsvcCrtDirectory $root $case[0]
    $expected = Join-Path $root ('x64\' + $case[1])
    if ($actual -cne $expected) { throw 'Runtime layout did not match the selected toolset.' }
}
foreach ($minor in @(29, 60)) {
    $rejected = $false
    try { Get-KpxcMsvcCrtDirectory $root $minor | Out-Null } catch { $rejected = $true }
    if (-not $rejected) { throw 'Unsupported runtime layout was accepted.' }
}
Write-Output 'PASS: 4 supported runtime layouts and 2 unsupported layouts.'
