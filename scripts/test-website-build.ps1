$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'build-website.ps1') -FunctionsOnly
function Assert-True($Value, [string]$Label) { if (-not $Value) { throw $Label } }
function Expect-Rejection([scriptblock]$Action, [string]$Code) {
    try { & $Action | Out-Null } catch { if ($_.Exception.Message -eq $Code) { return }; throw }
    throw "Expected rejection: $Code"
}
$root = Join-Path ([IO.Path]::GetTempPath()) ('kpxc-website-fixture-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $root | Out-Null
Write-Host "Retained disposable fixture: $root"
$powershell = (Get-Command powershell.exe -ErrorAction Stop).Source
Assert-True ((Invoke-KpxcWebsiteChild $powershell @('-NoProfile','-NonInteractive','-Command','exit 17') $root) -eq 17) 'Real child exit 17 was changed'
Assert-True ((Invoke-KpxcWebsiteChild $powershell @('-NoProfile','-NonInteractive','-Command','exit 0') $root) -eq 0) 'Real child exit 0 was changed'
Expect-Rejection { Assert-KpxcWebsitePath (Join-Path $root 'file:stream') } 'WEBSITE_UNSAFE_PATH'
New-Item -ItemType Directory -Path (Join-Path $root 'site/dist') -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $root 'artifacts/outside') -Force | Out-Null
[IO.File]::WriteAllText((Join-Path $root '.gitignore'), "artifacts/`nsite/dist/`n")
[IO.File]::WriteAllText((Join-Path $root 'site/package-lock.json'), '{}')
$linked = Join-Path $root 'artifacts/linked'
New-Item -ItemType Junction -Path $linked -Target (Join-Path $root 'artifacts/outside') | Out-Null
Expect-Rejection { Assert-KpxcWebsitePath (Join-Path $linked 'child') } 'WEBSITE_LINKED_PATH'
& git -C $root init --quiet
if ($LASTEXITCODE -ne 0) { throw 'Fixture git init failed' }
& git -C $root add -- .gitignore site/package-lock.json
& git -C $root -c user.name='Claude Fable 5.1' -c user.email='noreply@anthropic.com' commit --quiet -m 'Synthetic website fixture'
if ($LASTEXITCODE -ne 0) { throw 'Fixture commit failed' }
$source = Get-KpxcWebsiteSource $root
[IO.File]::WriteAllText((Join-Path $root 'untracked.txt'), 'synthetic')
Expect-Rejection { Get-KpxcWebsiteSource $root } 'WEBSITE_SOURCE_NOT_CLEAN'
& git -C $root add -- untracked.txt
& git -C $root -c user.name='Claude Fable 5.1' -c user.email='noreply@anthropic.com' commit --quiet -m 'Synthetic source fixture'
$source = Get-KpxcWebsiteSource $root
# Only transport and bootstrap are simulated below. No product build or download runs.
function Get-KpxcWebsiteNode([string]$Root) { return 'C:\synthetic-node' }
function Get-KpxcWebsiteHash([string]$Path) {
    if ($Path -like '*node.exe' -or $Path -like '*npm-cli.js') { return ('a' * 64) }
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}
$script:childExit = 17
$script:mutateSource = $false
function Invoke-KpxcWebsiteChild([string]$File, [string[]]$Arguments, [string]$WorkingDirectory) {
    if ($script:childExit) { return $script:childExit }
    if ($Arguments[0] -eq 'build.mjs') {
        $head = (& git -C (Split-Path -Parent $WorkingDirectory) rev-parse HEAD).Trim()
        [IO.File]::WriteAllText((Join-Path $WorkingDirectory 'dist/build-provenance.json'), ('{"sourceCommit":"' + $head + '"}'))
        [IO.File]::WriteAllText((Join-Path $WorkingDirectory 'dist/index.html'), '<p>synthetic</p>')
        if ($script:mutateSource) { [IO.File]::WriteAllText((Join-Path $WorkingDirectory 'package-lock.json'), '{"changed":true}') }
    }
    return 0
}
Assert-True ((Invoke-KpxcWebsiteBuild $root -Silent) -eq 17) 'Workflow changed child exit 17'
$failed = Get-ChildItem -LiteralPath (Join-Path $root 'artifacts/website-build') -Filter '*.json' | ForEach-Object { Get-Content -Raw -LiteralPath $_.FullName | ConvertFrom-Json }
Assert-True ($failed.exitCode -eq 17 -and $failed.outcome -eq 'failed' -and @($failed.files).Count -eq 0) 'Failure receipt advertised output success'
Assert-True ($failed.sourceCommit -ceq $source -and $failed.sourceUnchanged) 'Failure receipt lost source binding'
$script:childExit = 0
Assert-True ((Invoke-KpxcWebsiteBuild $root -Silent) -eq 0) 'Synthetic success failed'
$passed = @(Get-ChildItem -LiteralPath (Join-Path $root 'artifacts/website-build') -Filter '*.json' | ForEach-Object { Get-Content -Raw -LiteralPath $_.FullName | ConvertFrom-Json } | Where-Object outcome -eq 'passed')
Assert-True ($passed.Count -eq 1 -and @($passed[0].files).Count -eq 2 -and $passed[0].sourceUnchanged) 'Success receipt lacks exact source/output inventory'
foreach ($file in $passed[0].files) { Assert-True ($file.sha256 -ceq (Get-FileHash -LiteralPath (Join-Path $root ('site/dist/' + $file.path)) -Algorithm SHA256).Hash.ToLowerInvariant()) 'Output hash mismatch' }
$script:mutateSource = $true
Assert-True ((Invoke-KpxcWebsiteBuild $root -Silent) -ne 0) 'Changed source accepted'
$sourceFailures = @(Get-ChildItem -LiteralPath (Join-Path $root 'artifacts/website-build') -Filter '*.json' | ForEach-Object { Get-Content -Raw -LiteralPath $_.FullName | ConvertFrom-Json } | Where-Object { $_.outcome -eq 'failed' -and $_.exitCode -eq 1 })
Assert-True ($sourceFailures.Count -eq 1 -and -not $sourceFailures[0].sourceUnchanged) 'Source mutation failure missing'
$entry = Get-Content -Raw -LiteralPath (Join-Path $PSScriptRoot 'build-windows.ps1')
Assert-True ($entry.IndexOf('if ($WebsiteOnly)') -lt $entry.IndexOf('$build = Resolve-KpxcDirectory')) 'Website branch entered native preparation'
Write-Host 'PASS website child exit, unsafe/linked paths, dirty and changed source, failure receipts and hashed output fixtures'
