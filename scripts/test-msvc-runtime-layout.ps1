$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'PackagingSafety.ps1')
$hashFixture = Join-Path ([IO.Path]::GetTempPath()) ('keepassxc-hash-' + [Guid]::NewGuid().ToString('N'))
try {
    function Get-FileHash { throw 'The packaging hash must not depend on command autoload.' }
    [IO.File]::WriteAllBytes($hashFixture, [byte[]]@())
    if ((Get-KpxcHash $hashFixture) -cne 'e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855') {
        throw 'Empty-file SHA-256 mismatch.'
    }
    [IO.File]::WriteAllBytes($hashFixture, [Text.Encoding]::ASCII.GetBytes('abc'))
    if ((Get-KpxcHash $hashFixture) -cne 'ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad') {
        throw 'Known-vector SHA-256 mismatch.'
    }
    $handle = [IO.File]::Open($hashFixture, [IO.FileMode]::Open, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
    $handle.Dispose()
    $missingRejected = $false
    try { Get-KpxcHash ($hashFixture + '.missing') | Out-Null } catch { $missingRejected = $true }
    if (-not $missingRejected) { throw 'Missing hash input was accepted.' }
} finally {
    if ([IO.File]::Exists($hashFixture)) { [IO.File]::Delete($hashFixture) }
    Remove-Item Function:\Get-FileHash -ErrorAction SilentlyContinue
}
Write-Output 'PASS: streamed SHA-256 vectors, closed file handle, missing input and no command autoload.'
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
