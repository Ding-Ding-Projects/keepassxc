<#
.SYNOPSIS
Builds the deterministic parity fixture database from the design's demo vault.

The groups, entries, URLs, notes and custom fields come from design/lib/vault-data.js,
the same data the checked-in references render, so the built application and the
reference show the same vault. Every password is a placeholder; nothing here is real.

.PARAMETER Cli
Path to keepassxc-cli.exe. Defaults to the staged application.

.PARAMETER Output
Where to write the fixture. Defaults to design/parity/fixtures/parity.kdbx.

.PARAMETER KeyFile
Where to write the generated key file that protects the fixture. Defaults to design/parity/fixtures/parity.keyx.
#>
[CmdletBinding()]
param(
    [string]$Cli = (Join-Path (Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $PSScriptRoot))) 'stage\app\keepassxc-cli.exe'),
    [string]$Output = (Join-Path $PSScriptRoot 'parity.kdbx'),
    [string]$KeyFile = (Join-Path $PSScriptRoot 'parity.keyx')
)
$ErrorActionPreference = 'Stop'
if (-not (Test-Path -LiteralPath $Cli -PathType Leaf)) { throw "keepassxc-cli.exe was not found at $Cli" }
$data = & node (Join-Path $PSScriptRoot 'dump-vault-data.mjs') | ConvertFrom-Json
if ($LASTEXITCODE -ne 0) { throw 'dump-vault-data.mjs failed.' }
if (Test-Path -LiteralPath $Output) { Remove-Item -LiteralPath $Output -Force }
if (Test-Path -LiteralPath $KeyFile) { Remove-Item -LiteralPath $KeyFile -Force }

function Invoke-Cli([string[]]$Arguments, [string[]]$Lines) {
    $stdin = ($Lines -join "`n") + "`n"
    $stdin | & $Cli @Arguments 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "keepassxc-cli $($Arguments -join ' ') exited with $LASTEXITCODE" }
}

# The fixture is protected by a generated key file and no password, so the built
# application can open it unattended from the capture harness without a stdin
# prompt. A fixed decryption time keeps the KDF cheap and stable.
Invoke-Cli @('db-create', '-q', '--set-key-file', $KeyFile, '-t', '100', $Output) @()
$open = @('--no-password', '-k', $KeyFile)

# Group paths mirror the design's tree: depth 1 under the root, depth 2 under its parent.
$groupPath = @{}
foreach ($group in $data.GROUPS) {
    if ($group.depth -eq 0) { continue }
    if ($group.id -eq 'recycle') { continue }  # the recycle bin is created by the application itself
    $parent = ''
    if ($group.depth -eq 2) {
        $parentGroup = $data.GROUPS | Where-Object { $_.depth -eq 1 -and $group.id.StartsWith($_.id + '-') } | Select-Object -First 1
        if ($parentGroup) { $parent = $groupPath[$parentGroup.id] + '/' }
    }
    $path = $parent + $group.name
    $groupPath[$group.id] = $path
    Invoke-Cli (@('mkdir', '-q') + $open + @($Output, $path)) @()
}

foreach ($entry in $data.ENTRIES) {
    $path = if ($groupPath.ContainsKey($entry.group)) { $groupPath[$entry.group] + '/' + $entry.title } else { $entry.title }
    $args = @('add', '-q', '-p') + $open + @('-u', $entry.username, '--url', $entry.url, $Output, $path)
    if ($entry.notes) { $args += @('--notes', $entry.notes) }
    # Placeholder passwords shaped to reproduce the design's authored health verdicts.
    $secret = switch ($entry.health) {
        'weak'     { 'password1' }
        'reused'   { 'Shared-Household-2024' }
        'breached' { 'Winter2019!' }
        default    { 'Placeholder-' + $entry.id + '-Xk9#vQ2mL7pR' }
    }
    Invoke-Cli $args @($secret)
}

$hash = (Get-FileHash -LiteralPath $Output -Algorithm SHA256).Hash.ToLowerInvariant()
Write-Host "Fixture: $Output"
Write-Host "Key file: $KeyFile SHA-256: $((Get-FileHash -LiteralPath $KeyFile -Algorithm SHA256).Hash.ToLowerInvariant())"
Write-Host "SHA-256: $hash"
Write-Host "Groups: $($groupPath.Count)  Entries: $($data.ENTRIES.Count)"
