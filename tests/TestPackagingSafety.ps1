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
Check 'short-name aliases cannot hide a protected ancestor' { Reject { Resolve-KpxcDirectory $repo 'C:\SOURCE~1\output' } }
Check 'trailing-dot and reserved-name aliases are rejected' { Reject { Resolve-KpxcDirectory $repo ($output + '.') }; Reject { Resolve-KpxcDirectory $repo (Join-Path $testRoot 'NUL') } }
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
    Reject { Publish-KpxcOutput $repo $candidate $output {throw 'Injected failure after preserving the old generation.'} }
    Require ((Get-KpxcHash (Join-Path $output 'Setup.exe')) -eq $original)
    Assert-KpxcOutputOwnership $repo $output
}
Check 'verified owned output can be replaced' { Publish-KpxcOutput $repo $candidate $output; Assert-KpxcOutputOwnership $repo $output; Require ((Get-KpxcHash (Join-Path $output 'Setup.exe')) -eq (Get-KpxcHash (Join-Path $candidate 'Setup.exe'))) }
$previousOutputHash=Get-KpxcHash (Join-Path $output 'Setup.exe')
[IO.File]::WriteAllText((Join-Path $candidate 'Setup.exe'),'interrupted publication fixture')
$childScript=Join-Path $testRoot 'interrupt-publication.ps1'
@'
param($Helper,$Root,$Candidate,$Output)
$ErrorActionPreference='Stop'
. $Helper
Publish-KpxcOutput $Root $Candidate $Output {[Environment]::Exit(86)}
'@ | Set-Content -LiteralPath $childScript
Check 'abrupt process exit is recovered from the durable release journal' {
    & (Join-Path $PSHOME 'pwsh.exe') -NoProfile -File $childScript (Join-Path $PSScriptRoot '..\scripts\PackagingSafety.ps1') $repo $candidate $output
    Require ($LASTEXITCODE -eq 86)
    $journalPath=Get-KpxcTransactionPath $output
    $journal=Get-Content -Raw -LiteralPath $journalPath | ConvertFrom-Json
    $backup=Join-Path (Split-Path -Parent $output) ('.keepassxc-previous-' + (Split-Path -Leaf $output) + '-' + $journal.id)
    Require ((Get-KpxcHash (Join-Path $backup 'Setup.exe')) -eq $previousOutputHash)
    Repair-KpxcDirectoryPublication $repo $output
    Assert-KpxcOutputOwnership $repo $output
    Require ((Get-KpxcHash (Join-Path $output 'Setup.exe')) -eq (Get-KpxcHash (Join-Path $candidate 'Setup.exe')))
    Require (-not (Test-Path -LiteralPath $journalPath))
    Require ((Get-KpxcHash (Join-Path $backup 'Setup.exe')) -eq $previousOutputHash)
}
Check 'recovery restores the verified previous output when the new generation is unavailable' {
    $before=Get-KpxcHash (Join-Path $output 'Setup.exe')
    [IO.File]::WriteAllText((Join-Path $candidate 'Setup.exe'),'unavailable new generation')
    & (Join-Path $PSHOME 'pwsh.exe') -NoProfile -File $childScript (Join-Path $PSScriptRoot '..\scripts\PackagingSafety.ps1') $repo $candidate $output
    Require ($LASTEXITCODE -eq 86)
    $journalPath=Get-KpxcTransactionPath $output
    $journal=Get-Content -Raw -LiteralPath $journalPath | ConvertFrom-Json
    $prepared=Join-Path $testRoot ('.keepassxc-candidate-' + (Split-Path -Leaf $output) + '-' + $journal.id)
    $retained=Join-Path $testRoot ('retained-unavailable-' + $journal.id)
    Require ((Test-KpxcContains $testRoot $prepared) -and (Test-KpxcContains $testRoot $retained))
    [IO.Directory]::Move($prepared,$retained)
    Repair-KpxcDirectoryPublication $repo $output
    Assert-KpxcOutputOwnership $repo $output
    Require ((Get-KpxcHash (Join-Path $output 'Setup.exe')) -eq $before)
    Require (-not (Test-Path -LiteralPath $journalPath))
}
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
$transactionStage=Join-Path $testRoot 'transaction-stage'
New-Item -ItemType Directory -Path $transactionStage | Out-Null
[IO.File]::WriteAllText((Join-Path $transactionStage 'application.bin'),'previous application')
[IO.File]::WriteAllText((Join-Path $transactionStage '.keepassxc-stage-provenance.json'),'previous receipt')
$originalStage=@(Get-KpxcTreeManifest $transactionStage)
foreach($failurePhase in @('install','runtime','receipt')) {
    Check "failed stage $failurePhase preserves the original application and receipt" {
        Reject {
            Publish-KpxcStageGeneration -Root $repo -Stage $transactionStage -ValidateExisting {} -Validate {} -Install {param($path)
                [IO.File]::WriteAllText((Join-Path $path 'application.bin'),'candidate application')
                if($failurePhase -eq 'install'){throw 'Injected install failure.'}
            } -Runtime {param($path)
                [IO.File]::WriteAllText((Join-Path $path 'runtime.bin'),'candidate runtime')
                if($failurePhase -eq 'runtime'){throw 'Injected runtime failure.'}
            } -Receipt {param($path)
                [IO.File]::WriteAllText((Join-Path $path '.keepassxc-stage-provenance.json'),'candidate receipt')
                if($failurePhase -eq 'receipt'){throw 'Injected receipt failure.'}
            }
        }
        Require (Test-KpxcTreeManifest $transactionStage $originalStage)
    }
}
$stageChild=Join-Path $testRoot 'interrupt-stage.ps1'
@'
param($Helper,$Root,$Stage)
$ErrorActionPreference='Stop'
. $Helper
Publish-KpxcStageGeneration -Root $Root -Stage $Stage -ValidateExisting {} -Validate {} -Install {param($p)
    [IO.File]::WriteAllText((Join-Path $p 'application.bin'),'complete new application')
} -Runtime {param($p)
    New-Item -ItemType Directory -Path (Join-Path $p 'plugins') | Out-Null
    [IO.File]::WriteAllText((Join-Path $p 'plugins/runtime.bin'),'complete new runtime')
} -Receipt {param($p)
    [IO.File]::WriteAllText((Join-Path $p '.keepassxc-stage-provenance.json'),'complete new receipt')
} -AfterFirstRename {[Environment]::Exit(87)}
'@ | Set-Content -LiteralPath $stageChild
Check 'abrupt stage publication recovers application and receipt as one generation' {
    & (Join-Path $PSHOME 'pwsh.exe') -NoProfile -File $stageChild (Join-Path $PSScriptRoot '..\scripts\PackagingSafety.ps1') $repo $transactionStage
    Require ($LASTEXITCODE -eq 87)
    Repair-KpxcDirectoryPublication $repo $transactionStage
    Require ([IO.File]::ReadAllText((Join-Path $transactionStage 'application.bin')) -eq 'complete new application')
    Require ([IO.File]::ReadAllText((Join-Path $transactionStage '.keepassxc-stage-provenance.json')) -eq 'complete new receipt')
    Require (Test-Path -LiteralPath (Join-Path $transactionStage 'plugins/runtime.bin'))
    Require (-not (Test-Path -LiteralPath (Get-KpxcTransactionPath $transactionStage)))
}
if ($CompilerPath -and $RedistDirectory) {
    $environmentChild=Join-Path $testRoot 'repair-msvc-environment.ps1'
@'
param($Helper,$Compiler,[switch]$Inconsistent,[switch]$MissingSdk)
$ErrorActionPreference='Stop'
. $Helper
$env:PATH=(Split-Path -Parent $Compiler)+';'+$env:PATH
$env:INCLUDE=$null
$env:LIB=$null
$env:VCToolsRedistDir=$null
if($MissingSdk){
    Initialize-KpxcMsvcEnvironment | Out-Null
    $env:INCLUDE=Join-Path $env:VCToolsInstallDir 'include'
    $env:LIB=Join-Path $env:VCToolsInstallDir 'lib\x64'
}
if($Inconsistent){
    $env:INCLUDE='C:\inconsistent\VC\Tools\MSVC\14.00.00000\include'
    $env:LIB='C:\inconsistent\VC\Tools\MSVC\14.00.00000\lib\x64'
    $env:VCToolsRedistDir='C:\inconsistent\VC\Redist\MSVC\14.00.00000'
    $env:VCToolsInstallDir='C:\inconsistent\VC\Tools\MSVC\14.00.00000'
}
if(-not (Get-Command cl.exe -ErrorAction SilentlyContinue)){throw 'Fixture must retain cl.exe on PATH.'}
if(Test-KpxcMsvcEnvironment $Compiler){throw 'Fixture unexpectedly has a complete environment.'}
$resolved=Initialize-KpxcMsvcEnvironment
if([IO.Path]::GetFullPath($resolved) -ine [IO.Path]::GetFullPath($Compiler) -or -not (Test-KpxcMsvcEnvironment $resolved)){throw 'Selected compiler environment was not repaired.'}
'@ | Set-Content -LiteralPath $environmentChild
    Check 'wrapper initializer repairs missing MSVC environment with cl already on PATH' {
        & (Join-Path $PSHOME 'pwsh.exe') -NoProfile -File $environmentChild (Join-Path $PSScriptRoot '..\scripts\PackagingSafety.ps1') $CompilerPath
        Require ($LASTEXITCODE -eq 0)
    }
    Check 'wrapper initializer repairs inconsistent MSVC environment without changing compiler' {
        & (Join-Path $PSHOME 'pwsh.exe') -NoProfile -File $environmentChild (Join-Path $PSScriptRoot '..\scripts\PackagingSafety.ps1') $CompilerPath -Inconsistent
        Require ($LASTEXITCODE -eq 0)
    }
    Check 'wrapper initializer repairs a valid VC environment missing Windows SDK paths' {
        & (Join-Path $PSHOME 'pwsh.exe') -NoProfile -File $environmentChild (Join-Path $PSScriptRoot '..\scripts\PackagingSafety.ps1') $CompilerPath -MissingSdk
        Require ($LASTEXITCODE -eq 0)
    }
    $runtimeStage=Join-Path $testRoot 'runtime-stage'
    New-Item -ItemType Directory -Path $runtimeStage | Out-Null
    Check 'real pinned x64 runtime DLLs are copied and hash verified' { $script:runtime=@(Copy-KpxcMsvcRuntime $runtimeStage $CompilerPath $RedistDirectory); Require ($script:runtime.Count -ge 8) }
    Check 'runtime from outside compiler installation is rejected' { Reject { Copy-KpxcMsvcRuntime $runtimeStage $CompilerPath $external } }
    Check 'a fabricated compiler path cannot authorize runtime staging' { Reject { Copy-KpxcMsvcRuntime $runtimeStage (Join-Path $testRoot 'VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\cl.exe') $RedistDirectory } }
    if ($StageExePath -and $StageCommit) {
        Copy-Item -LiteralPath $StageExePath -Destination (Join-Path $runtimeStage 'KeePassXC.exe')
        $receipt=@{schemaVersion=1;sourceCommit=$StageCommit;compiledHead=$StageCommit.Substring(0,7);version='2.8.0';architecture='x64';stageDirectory=$runtimeStage;ownerKey=(Get-KpxcOwnerKey $repo $runtimeStage);executableSha256=(Get-KpxcHash (Join-Path $runtimeStage 'KeePassXC.exe'));msvcRuntime=$script:runtime;files=@(Get-KpxcDirectoryFiles $runtimeStage | ForEach-Object { @{path=$_.Name;sha256=(Get-KpxcHash $_.FullName)} })}
        $receiptPath=Join-Path $testRoot 'stage-fixture-receipt.json'
        $receipt | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $receiptPath
        Check 'real executable version and source identity match fixture receipt' { Assert-KpxcStageReceipt $runtimeStage $receiptPath $StageCommit '2.8.0' | Out-Null }
        $canonicalReceipt=Join-Path $runtimeStage '.keepassxc-stage-provenance.json'
        $receipt | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $canonicalReceipt
        Check 'canonical stage receipt is validated with the application generation' { Assert-KpxcStageOwnership $repo $runtimeStage $null }
        Check 'another requested version is rejected before packaging' { Reject { Assert-KpxcStageReceipt $runtimeStage $receiptPath $StageCommit '2.8.1' } }
        Check 'another source commit is rejected before packaging' { Reject { Assert-KpxcStageReceipt $runtimeStage $receiptPath ('0' * 40) '2.8.0' } }
        [IO.File]::WriteAllText((Join-Path $runtimeStage 'unrelated.txt'),'unexpected stage content')
        Check 'unreceipted stage content is rejected' { Reject { Assert-KpxcStageReceipt $runtimeStage $receiptPath $StageCommit '2.8.0' } }
        Remove-Item -LiteralPath (Join-Path $runtimeStage 'unrelated.txt')
        $receipt.files[0].path='../external-sentinel/untouched.txt'
        $receipt | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $receiptPath
        Check 'traversing receipt path is rejected without touching outside content' { Reject { Assert-KpxcStageReceipt $runtimeStage $receiptPath $StageCommit '2.8.0' }; Require ((Get-KpxcHash (Join-Path $external 'untouched.txt')) -eq $externalHash) }
        Add-Type -AssemblyName System.IO.Compression.FileSystem
        $package=Join-Path $testRoot 'payload-fixture.zip'
        $archive=[IO.Compression.ZipFile]::Open($package,[IO.Compression.ZipArchiveMode]::Create)
        try {
            foreach ($file in Get-ChildItem -LiteralPath $runtimeStage -File | Where-Object { $_.Name -ne '.keepassxc-stage-provenance.json' }) {
                [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive,$file.FullName,('lib/net45/'+$file.Name)) | Out-Null
            }
        } finally { $archive.Dispose() }
        $payload=@{stagedExecutable=@{sha256=(Get-KpxcHash (Join-Path $runtimeStage 'KeePassXC.exe'))};msvcRuntime=$script:runtime}
        Check 'packaged executable and runtime bytes match the verified stage' {
            $archive=[IO.Compression.ZipFile]::OpenRead($package)
            try { Assert-KpxcPackagedPayload $archive $payload } finally { $archive.Dispose() }
        }
        Check 'different executable bytes are rejected despite matching version metadata' {
            $wrong=@{stagedExecutable=@{sha256=('0'*64)};msvcRuntime=$script:runtime}
            $archive=[IO.Compression.ZipFile]::OpenRead($package)
            try { Reject { Assert-KpxcPackagedPayload $archive $wrong } } finally { $archive.Dispose() }
        }
        $archive=[IO.Compression.ZipFile]::Open($package,[IO.Compression.ZipArchiveMode]::Update)
        try { [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive,$canonicalReceipt,'lib/net45/.keepassxc-stage-provenance.json') | Out-Null } finally { $archive.Dispose() }
        Check 'private stage provenance is rejected if accidentally packaged' {
            $archive=[IO.Compression.ZipFile]::OpenRead($package)
            try { Reject { Assert-KpxcPackagedPayload $archive $payload } } finally { $archive.Dispose() }
        }
        $archive=[IO.Compression.ZipFile]::Open($package,[IO.Compression.ZipArchiveMode]::Update)
        try { $archive.GetEntry('lib/net45/.keepassxc-stage-provenance.json').Delete(); $archive.GetEntry('lib/net45/msvcp140.dll').Delete() } finally { $archive.Dispose() }
        Check 'a missing packaged compiler runtime is rejected' {
            $archive=[IO.Compression.ZipFile]::OpenRead($package)
            try { Reject { Assert-KpxcPackagedPayload $archive $payload } } finally { $archive.Dispose() }
        }
    }
}
Write-Host "Totals: $script:passed passed, 0 failed. Owned test data retained at $testRoot"
