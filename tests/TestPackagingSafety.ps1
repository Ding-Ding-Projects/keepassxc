[CmdletBinding()]
param([string]$CompilerPath, [string]$RedistDirectory, [string]$StageExePath, [string]$StageCommit)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\scripts\PackagingSafety.ps1')
. (Join-Path $PSScriptRoot '..\scripts\ExecutableVersionContract.ps1')
$testRoot = Join-Path ([IO.Path]::GetTempPath()) ('keepassxc-packaging-tests-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $testRoot | Out-Null
$repo = Join-Path $testRoot 'source'
$candidate = Join-Path $testRoot 'candidate'
$output = Join-Path $testRoot 'external-output'
New-Item -ItemType Directory -Path $repo,$candidate,$output | Out-Null
$script:passed = 0
function Check([string]$Name, [scriptblock]$Body) {
    & $Body
    ++$script:passed
    Write-Host "PASS $Name"
}
function Reject([scriptblock]$Body) {
    $rejected = $false
    try { & $Body | Out-Null } catch { $rejected = $true }
    if (-not $rejected) { throw 'Expected rejection did not occur.' }
}
function Require([bool]$Condition) { if (-not $Condition) { throw 'Assertion failed.' } }
$wrapperRoot=Join-Path $testRoot 'wrapper-source'
$wrapperScripts=Join-Path $wrapperRoot 'scripts'
New-Item -ItemType Directory -Path $wrapperScripts | Out-Null
foreach ($scriptName in @('build-squirrel.ps1','PackagingSafety.ps1','ExecutableVersionContract.ps1')) {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot "..\scripts\$scriptName") -Destination (Join-Path $wrapperScripts $scriptName)
}
[IO.File]::WriteAllText((Join-Path $wrapperScripts 'build-windows.ps1'),"throw 'BUILD_MUST_NOT_RUN'")
$wrapperOutput=Join-Path $testRoot 'wrapper-unowned-output'
New-Item -ItemType Directory -Path $wrapperOutput | Out-Null
[IO.File]::WriteAllText((Join-Path $wrapperOutput 'sentinel.txt'),'not owned by packaging')
Check 'real wrapper rejects unowned external output before build invocation' {
    $message=''
    try { & (Join-Path $wrapperScripts 'build-squirrel.ps1') -Silent -ArtifactDirectory $wrapperOutput } catch { $message=$_.Exception.Message }
    Require ($message -match 'Non-empty release output has no ownership receipt')
    Require ([IO.File]::ReadAllText((Join-Path $wrapperOutput 'sentinel.txt')) -eq 'not owned by packaging')
}
Check 'real wrapper rejects overlapping stage and output before build invocation' {
    $message=''
    try { & (Join-Path $wrapperScripts 'build-squirrel.ps1') -Silent -StageDirectory $wrapperOutput -ArtifactDirectory (Join-Path $wrapperOutput 'nested') } catch { $message=$_.Exception.Message }
    Require ($message -match 'must not overlap')
}
Check 'absolute external output is resolved literally' { Require ((Resolve-KpxcDirectory $repo $output) -eq $output) }
Check 'relative defaults remain checkout-relative' { Require ((Resolve-KpxcDirectory $repo 'dist\squirrel-windows') -eq (Join-Path $repo 'dist\squirrel-windows')) }
Check 'filesystem root is rejected' { Reject { Resolve-KpxcDirectory $repo ([IO.Path]::GetPathRoot($repo)) } }
Check 'source checkout cannot be output' { Reject { Assert-KpxcBuildPaths $repo @($repo) } }
Check 'source ancestor cannot be output' { Reject { Assert-KpxcBuildPaths $repo @($testRoot) } }
Check 'overlapping stage and output are rejected' { Reject { Assert-KpxcBuildPaths $repo @($output,(Join-Path $output 'nested')) } }
Check 'prefix siblings remain distinct' { Assert-KpxcBuildPaths $repo @($output,($output + '-other')) }
Check 'device paths are rejected' { Reject { Resolve-KpxcDirectory $repo '\\?\C:\unsafe' } }
Check 'drive-relative paths are rejected' { Reject { Resolve-KpxcDirectory $repo 'C:unsafe' } }
Check 'alternate streams are rejected' { Reject { Resolve-KpxcDirectory $repo ($output + ':stream') } }
$sentinel = Join-Path $output 'unrelated.txt'
[IO.File]::WriteAllText($sentinel,'preserve this unrelated output')
$sentinelHash = Get-KpxcHash $sentinel
Check 'unowned non-empty output is retained' { Reject { Assert-KpxcOutputOwnership $repo $output }; Require ((Get-KpxcHash $sentinel) -eq $sentinelHash) }
Remove-Item -LiteralPath $sentinel
[IO.File]::WriteAllText((Join-Path $candidate 'Setup.exe'),'old setup fixture')
[IO.File]::WriteAllText((Join-Path $candidate 'RELEASES'),'old feed fixture')
Check 'empty output receives an owned publication' { Publish-KpxcOutput $repo $candidate $output; Assert-KpxcOutputOwnership $repo $output }
$original = Get-KpxcHash (Join-Path $output 'Setup.exe')
[IO.File]::AppendAllText((Join-Path $output 'Setup.exe'),'user changed it')
Check 'modified owned content is retained' { $changed=Get-KpxcHash (Join-Path $output 'Setup.exe'); Reject { Publish-KpxcOutput $repo $candidate $output }; Require ((Get-KpxcHash (Join-Path $output 'Setup.exe')) -eq $changed) }
Copy-Item -LiteralPath (Join-Path $candidate 'Setup.exe') -Destination (Join-Path $output 'Setup.exe') -Force
Check 'injected preparation copy failure preserves previous output' {
    function Copy-Item { throw 'Injected copy failure.' }
    try { Reject { Publish-KpxcOutput $repo $candidate $output } } finally { Remove-Item Function:Copy-Item }
    Require ((Get-KpxcHash (Join-Path $output 'Setup.exe')) -eq $original)
    Assert-KpxcOutputOwnership $repo $output
}
[IO.File]::WriteAllText((Join-Path $candidate 'Setup.exe'),'new setup fixture')
Check 'injected publication move failure restores previous output' {
    function Move-Item {
        param([string]$LiteralPath,[string]$Destination,[string]$ErrorAction)
        if ([IO.Path]::GetFileName($LiteralPath) -eq 'Setup.exe') { throw 'Injected publication failure.' }
        Microsoft.PowerShell.Management\Move-Item @PSBoundParameters
    }
    try { Reject { Publish-KpxcOutput $repo $candidate $output } } finally { Remove-Item Function:Move-Item }
    Require ((Get-KpxcHash (Join-Path $output 'Setup.exe')) -eq $original)
    Assert-KpxcOutputOwnership $repo $output
}
Check 'verified owned output can be replaced' { Publish-KpxcOutput $repo $candidate $output; Assert-KpxcOutputOwnership $repo $output; Require ((Get-KpxcHash (Join-Path $output 'Setup.exe')) -eq (Get-KpxcHash (Join-Path $candidate 'Setup.exe'))) }
$preserved = Get-KpxcHash (Join-Path $output 'Setup.exe')
Check 'silent copy corruption is rejected before replacing prior output' {
    function Copy-Item {
        param([string]$LiteralPath,[string]$Destination,[string]$ErrorAction)
        [IO.File]::WriteAllText($Destination,'injected corrupt bytes')
    }
    try { Reject { Publish-KpxcOutput $repo $candidate $output } } finally { Remove-Item Function:Copy-Item }
    Require ((Get-KpxcHash (Join-Path $output 'Setup.exe')) -eq $preserved)
    Assert-KpxcOutputOwnership $repo $output
}
[IO.File]::WriteAllText((Join-Path $candidate '.keepassxc-output-owner.json'),'untrusted ownership')
Check 'candidate cannot supply its own ownership marker' { Reject { Publish-KpxcOutput $repo $candidate $output }; Require ((Get-KpxcHash (Join-Path $output 'Setup.exe')) -eq $preserved) }
Remove-Item -LiteralPath (Join-Path $candidate '.keepassxc-output-owner.json')
New-Item -ItemType HardLink -Path (Join-Path $testRoot 'linked-owner.json') -Target (Join-Path $output '.keepassxc-output-owner.json') | Out-Null
Check 'hard-linked ownership records are rejected before publication' { Reject { Assert-KpxcOutputOwnership $repo $output }; Require ((Get-KpxcHash (Join-Path $output 'Setup.exe')) -eq $preserved) }
$external = Join-Path $testRoot 'external-sentinel'
$linked = Join-Path $testRoot 'linked'
New-Item -ItemType Directory -Path $external | Out-Null
[IO.File]::WriteAllText((Join-Path $external 'untouched.txt'),'outside the selected output')
$externalHash=Get-KpxcHash (Join-Path $external 'untouched.txt')
New-Item -ItemType Junction -Path $linked -Target $external | Out-Null
Check 'junction target and children are rejected without touching target' {
    Reject { Resolve-KpxcDirectory $repo (Join-Path $linked 'child') }
    Require ((Get-KpxcHash (Join-Path $external 'untouched.txt')) -eq $externalHash)
}
$build = Join-Path $testRoot 'build'
New-Item -ItemType Directory -Path $build | Out-Null
[IO.File]::WriteAllText((Join-Path $build 'unrelated.txt'),'preserve')
Check 'unowned warm build directory is rejected' { Reject { Assert-KpxcBuildCache $repo $build } }
[IO.File]::WriteAllText((Join-Path $build 'CMakeCache.txt'),"CMAKE_HOME_DIRECTORY:INTERNAL=$external`n")
Check 'another checkout build cache is rejected' { Reject { Assert-KpxcBuildCache $repo $build } }
[IO.File]::WriteAllText((Join-Path $build 'CMakeCache.txt'),"CMAKE_HOME_DIRECTORY:INTERNAL=$repo`n")
Check 'matching warm build cache is accepted' { Assert-KpxcBuildCache $repo $build }
$pe = Join-Path $testRoot 'header-fixture.bin'
$bytes = New-Object byte[] 128
$bytes[0]=0x4d;$bytes[1]=0x5a;$bytes[0x3c]=64;$bytes[64]=0x50;$bytes[65]=0x45;$bytes[68]=0x64;$bytes[69]=0x86;$bytes[88]=0x0b;$bytes[89]=0x02
[IO.File]::WriteAllBytes($pe,$bytes)
Check 'x64 PE32+ architecture header is accepted' { Assert-KpxcPeX64 $pe }
$bytes[68]=0x4c;$bytes[69]=0x01;[IO.File]::WriteAllBytes($pe,$bytes)
Check 'x86 architecture header is rejected' { Reject { Assert-KpxcPeX64 $pe } }
[IO.File]::WriteAllText($pe,'not an executable')
Check 'malformed PE header is rejected' { Reject { Assert-KpxcPeX64 $pe } }
if ($CompilerPath -and $RedistDirectory) {
    $runtimeStage=Join-Path $testRoot 'runtime-stage'
    New-Item -ItemType Directory -Path $runtimeStage | Out-Null
    Check 'real pinned x64 runtime DLLs are copied and hash verified' { $script:runtime=@(Copy-KpxcMsvcRuntime $runtimeStage $CompilerPath $RedistDirectory); Require ($script:runtime.Count -ge 8) }
    Check 'runtime from outside compiler installation is rejected' { Reject { Copy-KpxcMsvcRuntime $runtimeStage $CompilerPath $external } }
    Check 'a fabricated compiler path cannot authorize runtime staging' { Reject { Copy-KpxcMsvcRuntime $runtimeStage (Join-Path $testRoot 'VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\cl.exe') $RedistDirectory } }
    if ($StageExePath -and $StageCommit) {
        Copy-Item -LiteralPath $StageExePath -Destination (Join-Path $runtimeStage 'KeePassXC.exe')
        $receipt=@{schemaVersion=1;sourceCommit=$StageCommit;compiledHead=$StageCommit.Substring(0,7);version='2.8.0';architecture='x64';stageDirectory=$runtimeStage;executableSha256=(Get-KpxcHash (Join-Path $runtimeStage 'KeePassXC.exe'));msvcRuntime=$script:runtime;files=@(Get-KpxcDirectoryFiles $runtimeStage | ForEach-Object { @{path=$_.Name;sha256=(Get-KpxcHash $_.FullName)} })}
        $receiptPath=Join-Path $testRoot 'stage-fixture-receipt.json'
        $receipt | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $receiptPath
        Check 'real executable version and source identity match fixture receipt' { Assert-KpxcStageReceipt $runtimeStage $receiptPath $StageCommit '2.8.0' | Out-Null }
        Check 'another requested version is rejected before packaging' { Reject { Assert-KpxcStageReceipt $runtimeStage $receiptPath $StageCommit '2.8.1' } }
        Check 'another source commit is rejected before packaging' { Reject { Assert-KpxcStageReceipt $runtimeStage $receiptPath ('0' * 40) '2.8.0' } }
        [IO.File]::WriteAllText((Join-Path $runtimeStage 'unrelated.txt'),'unexpected stage content')
        Check 'unreceipted stage content is rejected' { Reject { Assert-KpxcStageReceipt $runtimeStage $receiptPath $StageCommit '2.8.0' } }
        Remove-Item -LiteralPath (Join-Path $runtimeStage 'unrelated.txt')
        $receipt.files[0].path='../external-sentinel/untouched.txt'
        $receipt | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $receiptPath
        Check 'traversing receipt path is rejected without touching outside content' { Reject { Assert-KpxcStageReceipt $runtimeStage $receiptPath $StageCommit '2.8.0' }; Require ((Get-KpxcHash (Join-Path $external 'untouched.txt')) -eq $externalHash) }
    }
}
Write-Host "Totals: $script:passed passed, 0 failed. Owned test data retained at $testRoot"
