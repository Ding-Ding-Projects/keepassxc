param(
    [switch]$NativeProcessChild,
    [string]$FixtureRoot
)

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$source = Get-Content -Raw -LiteralPath (Join-Path $PSScriptRoot 'build-squirrel.ps1')
$start = $source.IndexOf('# BEGIN clean-source diagnostics', [StringComparison]::Ordinal)
$end = $source.IndexOf('# END clean-source diagnostics', [StringComparison]::Ordinal)
if ($start -lt 0 -or $end -le $start) { throw 'The clean-source diagnostic seam is missing or malformed.' }
$functions = $source.Substring($start, $end - $start)
Invoke-Expression $functions

if ($NativeProcessChild) {
    if (-not $FixtureRoot) { throw 'The native-process fixture root is required.' }
    $nativeHead = Assert-KpxcCleanSourceCheckout $FixtureRoot
    if ($nativeHead -notmatch '^[0-9a-fA-F]{40,64}$') { throw 'The native process returned an invalid HEAD object id.' }
    Write-Output "PASS:NATIVE:HEAD:${nativeHead}:STATUS:ZERO_CHANGE"
    exit 0
}

$script:responses = @()
$script:callIndex = 0
$script:argumentsSeen = @()
$runner = {
    param($checkoutRoot, $arguments)
    $script:argumentsSeen += ,@($checkoutRoot, @($arguments))
    if ($script:callIndex -ge $script:responses.Count) { throw 'Unexpected source command call.' }
    $response = $script:responses[$script:callIndex]
    ++$script:callIndex
    if ($response.Throw) { throw 'secret-looking launch detail must not be surfaced.' }
    return $response
}

function Set-Responses([object[]]$Responses) {
    $script:responses = @($Responses)
    $script:callIndex = 0
    $script:argumentsSeen = @()
}

function Assert-ThrowsLike([scriptblock]$Action, [string]$Pattern, [string]$Case) {
    try { & $Action; throw "$Case unexpectedly passed." }
    catch {
        if ($_.Exception.Message -notmatch $Pattern) { throw "$Case returned an unexpected diagnostic: $($_.Exception.Message)" }
        return $_.Exception.Message
    }
}

function Assert-Equal($Actual, $Expected, [string]$Case) {
    if ($Actual -cne $Expected) { throw "$Case expected '$Expected' but received '$Actual'." }
}

function Invoke-FixtureGit([string[]]$Arguments, [string]$Case) {
    $output = @(& git -C $fixtureRoot @Arguments 2>$null)
    $exitCode = $LASTEXITCODE
    if ($exitCode -ne 0) { throw "$Case failed with exit code $exitCode." }
    return ,$output
}

function Invoke-NativeCleanSourceChild([string]$Root) {
    $command = Get-Command pwsh -ErrorAction Stop
    $startInfo = [Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $command.Source
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    [void]$startInfo.ArgumentList.Add('-NoProfile')
    [void]$startInfo.ArgumentList.Add('-File')
    [void]$startInfo.ArgumentList.Add($PSCommandPath)
    [void]$startInfo.ArgumentList.Add('-NativeProcessChild')
    [void]$startInfo.ArgumentList.Add('-FixtureRoot')
    [void]$startInfo.ArgumentList.Add($Root)
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $startInfo
    try {
        if (-not $process.Start()) { throw 'The native-process regression could not start PowerShell.' }
        $stdoutTask = $process.StandardOutput.ReadToEndAsync()
        $stderrTask = $process.StandardError.ReadToEndAsync()
        $process.WaitForExit()
        $stdout = $stdoutTask.GetAwaiter().GetResult().Trim()
        $null = $stderrTask.GetAwaiter().GetResult()
        if ($process.ExitCode -ne 0) { throw 'The native-process regression returned a nonzero exit code.' }
        return $stdout
    } finally {
        $process.Dispose()
    }
}

$head = 'f73d2dd5572b16bdeac4ad3159bcef1b248d9d03'
$rootPath = 'C:\fixture\checkout'
$calls = 0

# Exercise the production Git path in a fresh PowerShell process over a disposable fixture repository.
$fixtureRoot = Join-Path ([IO.Path]::GetTempPath()) ('kpxc-source-clean-' + [Guid]::NewGuid().ToString('N'))
try {
    New-Item -ItemType Directory -Path $fixtureRoot | Out-Null
    $null = Invoke-FixtureGit @('init', '--quiet') 'fixture init'
    $null = Invoke-FixtureGit @('config', 'user.name', 'Packaging Regression') 'fixture author name'
    $null = Invoke-FixtureGit @('config', 'user.email', 'packaging-regression@example.invalid') 'fixture author email'
    Set-Content -LiteralPath (Join-Path $fixtureRoot 'fixture.txt') -Value 'fixture' -NoNewline
    $null = Invoke-FixtureGit @('add', '--', 'fixture.txt') 'fixture stage'
    $null = Invoke-FixtureGit @('commit', '--quiet', '-m', 'Create installer regression fixture') 'fixture commit'
    $expectedNativeHead = (Invoke-FixtureGit @('rev-parse', 'HEAD') 'fixture HEAD lookup' | Select-Object -First 1).Trim()
    if ($expectedNativeHead -notmatch '^[0-9a-fA-F]{40,64}$') { throw 'The native-process fixture did not produce a valid HEAD.' }
    $nativeResult = Invoke-NativeCleanSourceChild $fixtureRoot
    if ($nativeResult -cnotmatch '^PASS:NATIVE:HEAD:([0-9a-fA-F]{40,64}):STATUS:ZERO_CHANGE$') { throw 'The native-process regression returned an invalid result marker.' }
    if ($Matches[1] -cne $expectedNativeHead) { throw 'The native-process regression returned a different HEAD.' }
} finally {
    if (Test-Path -LiteralPath $fixtureRoot) { Remove-Item -LiteralPath $fixtureRoot -Recurse -Force }
}

# Clean checkout: HEAD and status are both called in order and no entry is reported.
Set-Responses @(
    [pscustomobject]@{ ExitCode = 0; Output = @($head) },
    [pscustomobject]@{ ExitCode = 0; Output = @() }
)
$actualHead = Assert-KpxcCleanSourceCheckout $rootPath $runner
Assert-Equal $actualHead $head 'clean checkout HEAD'
Assert-Equal $script:callIndex 2 'clean checkout call count'
Assert-Equal (@($script:argumentsSeen[0][1]) -join ' ') 'rev-parse HEAD' 'HEAD command arguments'
Assert-Equal (@($script:argumentsSeen[1][1]) -join ' ') 'status --porcelain=v1 --untracked-files=all' 'status command arguments'

# HEAD command errors and malformed output are reported separately, without echoed command output.
Set-Responses @([pscustomobject]@{ ExitCode = 128; Output = @('secret-looking HEAD output') })
$message = Assert-ThrowsLike { Assert-KpxcCleanSourceCheckout $rootPath $runner } 'could not read HEAD \(git exit code 128\)' 'unreadable HEAD'
if ($message.Contains('secret-looking')) { throw 'HEAD command output leaked into diagnostics.' }
Assert-Equal $script:callIndex 1 'HEAD error call count'

Set-Responses @([pscustomobject]@{ Throw = $true })
$message = Assert-ThrowsLike { Assert-KpxcCleanSourceCheckout $rootPath $runner } 'could not start Git to read HEAD' 'HEAD command launch error'
if ($message.Contains('secret-looking')) { throw 'HEAD launch detail leaked into diagnostics.' }
Assert-Equal $script:callIndex 1 'HEAD launch error call count'

Set-Responses @([pscustomobject]@{ ExitCode = 0; Output = @('not-an-object-id') })
$message = Assert-ThrowsLike { Assert-KpxcCleanSourceCheckout $rootPath $runner } 'valid HEAD object id' 'malformed HEAD'
if ($message.Contains('not-an-object-id')) { throw 'Malformed HEAD output leaked into diagnostics.' }
Assert-Equal $script:callIndex 1 'malformed HEAD call count'

# A status-command error has its own bounded diagnostic and never echoes stderr-like output.
Set-Responses @(
    [pscustomobject]@{ ExitCode = 0; Output = @($head) },
    [pscustomobject]@{ ExitCode = 2; Output = @('credential=do-not-print') }
)
$message = Assert-ThrowsLike { Assert-KpxcCleanSourceCheckout $rootPath $runner } 'could not inspect source status \(git exit code 2\)' 'status command error'
if ($message.Contains('credential')) { throw 'Status command output leaked into diagnostics.' }

Set-Responses @(
    [pscustomobject]@{ ExitCode = 0; Output = @($head) },
    [pscustomobject]@{ Throw = $true }
)
$message = Assert-ThrowsLike { Assert-KpxcCleanSourceCheckout $rootPath $runner } 'could not start Git to inspect source status' 'status command launch error'
if ($message.Contains('secret-looking')) { throw 'Status launch detail leaked into diagnostics.' }

# Tracked, untracked, malformed and hostile-looking names are counted but never disclosed.
$sensitiveNames = @(
    ' M secret-looking-password=value.txt',
    'A  unicode-雪-and-quotes-"-file.txt',
    " M `"line`nfeed`r`ttab`0-control.txt`"",
    'T  type-change-private-name.txt',
    ('?? ' + ('very-long-private-name-' * 120)),
    '?? untracked-private-token=abc'
)
Set-Responses @(
    [pscustomobject]@{ ExitCode = 0; Output = @($head) },
    [pscustomobject]@{ ExitCode = 0; Output = @($sensitiveNames[0], $sensitiveNames[1], $sensitiveNames[2], $sensitiveNames[3], $sensitiveNames[4], $sensitiveNames[5]) }
)
$message = Assert-ThrowsLike { Assert-KpxcCleanSourceCheckout $rootPath $runner } 'tracked=4, untracked=2, unclassified=0' 'dirty checkout and sanitization'
foreach ($name in @('password=value', 'unicode-', 'line', 'type-change-private', 'very-long-private-name', 'private-token')) {
    if ($message.Contains($name)) { throw "A path fragment leaked into diagnostics: $name" }
}
if ($message.Length -gt 180) { throw 'Dirty checkout diagnostic exceeded its bounded length.' }

Set-Responses @(
    [pscustomobject]@{ ExitCode = 0; Output = @($head) },
    [pscustomobject]@{ ExitCode = 0; Output = @('x') }
)
$message = Assert-ThrowsLike { Assert-KpxcCleanSourceCheckout $rootPath $runner } 'tracked=0, untracked=0, unclassified=1' 'unclassified porcelain entry'
if ($message.Contains('x')) { throw 'Unclassified status data leaked into diagnostics.' }

Write-Output 'PASS: clean HEAD/status, native PowerShell process path, command failures, tracked/untracked changes, malformed status, and sanitized path edge cases.'
