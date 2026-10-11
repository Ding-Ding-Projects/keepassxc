[CmdletBinding()]
param([switch]$FunctionsOnly)

# This helper is entered only through the root website-only build mode.
function Get-KpxcWebsiteHash([string]$Path) {
    $stream = [IO.File]::OpenRead($Path)
    try {
        $hash = [Security.Cryptography.SHA256]::Create()
        try { return ([BitConverter]::ToString($hash.ComputeHash($stream))).Replace('-', '').ToLowerInvariant() }
        finally { $hash.Dispose() }
    } finally { $stream.Dispose() }
}
function Assert-KpxcWebsitePath([string]$Path) {
    try { $full = [IO.Path]::GetFullPath($Path) } catch { throw 'WEBSITE_UNSAFE_PATH' }
    if ($full -notmatch '^[A-Za-z]:[\\/]' -or $full.Substring(2).Contains(':')) { throw 'WEBSITE_UNSAFE_PATH' }
    $cursor = $full
    while ($cursor) {
        $item = Get-Item -LiteralPath $cursor -Force -ErrorAction SilentlyContinue
        if ($item -and (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -or $item.LinkType -eq 'HardLink')) { throw 'WEBSITE_LINKED_PATH' }
        $parent = Split-Path -Parent $cursor
        if ($parent -eq $cursor) { break }
        $cursor = $parent
    }
    return $full
}
function Get-KpxcWebsiteFiles([string]$Directory) {
    $directory = (Assert-KpxcWebsitePath $Directory).TrimEnd('\','/')
    $result = @()
    # Check each directory before descending, including hidden output files.
    foreach ($entry in @(Get-ChildItem -LiteralPath $directory -Force -ErrorAction Stop)) {
        Assert-KpxcWebsitePath $entry.FullName | Out-Null
        if ($entry.PSIsContainer) { $result += @(Get-KpxcWebsiteFiles $entry.FullName) }
        else { $result += $entry }
    }
    return $result
}
function Get-KpxcWebsiteSource([string]$Root) {
    $head = (& git -C $Root rev-parse HEAD 2>$null)
    if ($LASTEXITCODE -ne 0 -or [string]$head -notmatch '^[a-f0-9]{40}$') { throw 'WEBSITE_SOURCE_UNREADABLE' }
    $changes = @(& git -C $Root status --porcelain --untracked-files=normal)
    if ($LASTEXITCODE -ne 0 -or $changes.Count) { throw 'WEBSITE_SOURCE_NOT_CLEAN' }
    return ([string]$head).Trim()
}
function Test-KpxcWebsiteNode([string]$Directory, $Manifest) {
    Assert-KpxcWebsitePath $Directory | Out-Null
    $node = Join-Path $Directory 'node.exe'
    $npm = Join-Path $Directory 'node_modules/npm/bin/npm-cli.js'
    foreach ($path in @($node, $npm, (Join-Path $Directory 'node_modules/npm/package.json'))) {
        Assert-KpxcWebsitePath $path | Out-Null
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { return $false }
    }
    return ((Get-Item -LiteralPath $node).Length -eq $Manifest.nodeBytes -and
        (Get-KpxcWebsiteHash $node) -ceq $Manifest.nodeSha256 -and
        (Get-Content -Raw -LiteralPath (Join-Path $Directory 'node_modules/npm/package.json') | ConvertFrom-Json).version -ceq $Manifest.npmVersion)
}
function Get-KpxcWebsiteNode([string]$Root) {
    if (-not [Environment]::Is64BitOperatingSystem -or $env:OS -ne 'Windows_NT') { throw 'WEBSITE_WINDOWS_X64_REQUIRED' }
    $manifestPath = Assert-KpxcWebsitePath (Join-Path $Root 'scripts/website-node.manifest.json')
    $manifest = Get-Content -Raw -LiteralPath $manifestPath | ConvertFrom-Json
    if ($manifest.schemaVersion -ne 1 -or $manifest.version -ne '24.19.0' -or
        $manifest.url -cne 'https://nodejs.org/dist/v24.19.0/node-v24.19.0-win-x64.zip' -or
        $manifest.archiveRoot -cne 'node-v24.19.0-win-x64' -or $manifest.archiveSha256 -notmatch '^[a-f0-9]{64}$' -or
        $manifest.nodeSha256 -notmatch '^[a-f0-9]{64}$') { throw 'WEBSITE_NODE_MANIFEST_INVALID' }
    if ($env:KPXC_WEBSITE_NODE_ROOT) {
        $directory = Assert-KpxcWebsitePath $env:KPXC_WEBSITE_NODE_ROOT
        if (-not (Test-KpxcWebsiteNode $directory $manifest)) { throw 'WEBSITE_NODE_OVERRIDE_INVALID' }
        return $directory
    }
    $local = [Environment]::GetFolderPath([Environment+SpecialFolder]::LocalApplicationData)
    if (-not $local) { throw 'WEBSITE_LOCAL_DATA_UNAVAILABLE' }
    $parent = Assert-KpxcWebsitePath (Join-Path $local 'KeePassXCMaterial/toolchain/node')
    $directory = Join-Path $parent $manifest.version
    if (Test-KpxcWebsiteNode $directory $manifest) { return $directory }
    if (Test-Path -LiteralPath $directory) { throw 'WEBSITE_NODE_CACHE_INVALID_RETAINED' }
    New-Item -ItemType Directory -Force -Path $parent | Out-Null
    $staging = Join-Path $parent ('pending-' + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $staging | Out-Null
    # On failure the isolated staging directory is retained for diagnosis.
    $archive = Join-Path $staging 'node.zip'
    $previousProtocol = [Net.ServicePointManager]::SecurityProtocol
    try {
        [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
        Invoke-WebRequest -UseBasicParsing -Uri $manifest.url -MaximumRedirection 0 -TimeoutSec 120 -OutFile $archive
    } finally { [Net.ServicePointManager]::SecurityProtocol = $previousProtocol }
    if ((Get-KpxcWebsiteHash $archive) -cne $manifest.archiveSha256) { throw 'WEBSITE_NODE_ARCHIVE_HASH_MISMATCH' }
    Expand-Archive -LiteralPath $archive -DestinationPath $staging
    $extracted = Join-Path $staging $manifest.archiveRoot
    Get-KpxcWebsiteFiles $extracted | Out-Null
    if (-not (Test-KpxcWebsiteNode $extracted $manifest)) { throw 'WEBSITE_NODE_EXTRACTED_IDENTITY_INVALID' }
    Move-Item -LiteralPath $extracted -Destination $directory
    return $directory
}
function Invoke-KpxcWebsiteChild([string]$File, [string[]]$Arguments, [string]$WorkingDirectory) {
    Push-Location -LiteralPath $WorkingDirectory
    try {
        & $File @Arguments | Out-Host
        $code = $LASTEXITCODE
        if ($null -eq $code) { throw 'WEBSITE_CHILD_NO_EXIT_RESULT' }
        return [int]$code
    } finally { Pop-Location }
}
function Invoke-KpxcWebsiteBuild([string]$Root, [switch]$Silent) {
    $ErrorActionPreference = 'Stop'
    $started = [DateTime]::UtcNow.ToString('o')
    $code = 1
    $receipt = [ordered]@{ schemaVersion=1; mode='website-only'; command='build.bat /s -WebsiteOnly'; startedAtUtc=$started; endedAtUtc=$null; sourceCommit=$null; finalSourceCommit=$null; outcome='failed'; stage='preflight'; exitCode=1; files=@() }
    $receiptPath = $null
    if (-not $Silent) { $receipt.command = 'build.bat -WebsiteOnly' }
    $previousPath = $env:PATH
    try {
        $root = Assert-KpxcWebsitePath $Root
        $receiptDirectory = Assert-KpxcWebsitePath (Join-Path $root 'artifacts/website-build')
        New-Item -ItemType Directory -Force -Path $receiptDirectory | Out-Null
        & git -C $root check-ignore --quiet -- $receiptDirectory
        if ($LASTEXITCODE -ne 0) { throw 'WEBSITE_RECEIPT_DIRECTORY_NOT_IGNORED' }
        $receiptPath = Join-Path $receiptDirectory ('execution-' + [guid]::NewGuid().ToString('N') + '.json')
        $source = Get-KpxcWebsiteSource $root
        $receipt.sourceCommit = $source
        $site = Assert-KpxcWebsitePath (Join-Path $root 'site')
        $output = Assert-KpxcWebsitePath (Join-Path $site 'dist')
        if (Test-Path -LiteralPath $output) { Get-KpxcWebsiteFiles $output | Out-Null }
        foreach ($inputPath in @('package-lock.json', 'build.mjs')) { Assert-KpxcWebsitePath (Join-Path $site $inputPath) | Out-Null }
        $receipt.packageLockSha256 = Get-KpxcWebsiteHash (Join-Path $site 'package-lock.json')
        $receipt.stage = 'node-bootstrap'
        $nodeRoot = Get-KpxcWebsiteNode $root
        $node = Join-Path $nodeRoot 'node.exe'
        $npm = Join-Path $nodeRoot 'node_modules/npm/bin/npm-cli.js'
        $receipt.nodeSha256 = Get-KpxcWebsiteHash $node
        $receipt.npmCliSha256 = Get-KpxcWebsiteHash $npm
        $env:PATH = "$nodeRoot;$previousPath"
        $receipt.stage = 'locked-dependencies'
        $code = Invoke-KpxcWebsiteChild $node @($npm,'ci','--no-audit','--no-fund') $site
        if ($code -eq 0) {
            $receipt.stage = 'static-build'
            $code = Invoke-KpxcWebsiteChild $node @('build.mjs') $site
        }
        if ($code -ne 0) { $receipt.failureCode = 'WEBSITE_CHILD_FAILED' }
        else {
            $receipt.stage = 'output-verification'
            $receipt.finalSourceCommit = Get-KpxcWebsiteSource $root
            if ($receipt.finalSourceCommit -cne $source) { throw 'WEBSITE_SOURCE_CHANGED' }
            $provenance = Get-Content -Raw -LiteralPath (Join-Path $output 'build-provenance.json') | ConvertFrom-Json
            if ($provenance.sourceCommit -cne $source) { throw 'WEBSITE_OUTPUT_SOURCE_MISMATCH' }
            $files = @(Get-KpxcWebsiteFiles $output | Sort-Object FullName)
            if (-not $files.Count) { throw 'WEBSITE_OUTPUT_EMPTY' }
            $receipt.files = @($files | ForEach-Object { [ordered]@{path=$_.FullName.Substring($output.Length+1).Replace('\','/');bytes=$_.Length;sha256=(Get-KpxcWebsiteHash $_.FullName)} })
            $receipt.outcome = 'passed'
            $receipt.stage = 'complete'
        }
    } catch {
        if ($code -eq 0) { $code = 1 }
        $receipt.failureCode = if ($_.Exception.Message -match '^WEBSITE_[A-Z0-9_]+$') { $_.Exception.Message } else { 'WEBSITE_BUILD_EXCEPTION' }
        Write-Host "Website build failed: $($receipt.failureCode)"
    } finally {
        $env:PATH = $previousPath
        if ($receipt.sourceCommit) {
            try {
                $endHead = (& git -C $root rev-parse HEAD 2>$null)
                if ($LASTEXITCODE -eq 0) { $receipt.finalSourceCommit = ([string]$endHead).Trim() }
                $endChanges = @(& git -C $root status --porcelain --untracked-files=normal)
                $receipt.sourceCleanAtEnd = ($LASTEXITCODE -eq 0 -and $endChanges.Count -eq 0)
                $receipt.sourceUnchanged = ($receipt.finalSourceCommit -ceq $receipt.sourceCommit -and $receipt.sourceCleanAtEnd)
                if (-not $receipt.sourceUnchanged -and $code -eq 0) { $code = 1; $receipt.outcome = 'failed'; $receipt.failureCode = 'WEBSITE_SOURCE_CHANGED' }
            } catch { $receipt.sourceCleanAtEnd = $false; if ($code -eq 0) { $code = 1; $receipt.outcome = 'failed' } }
        }
        $receipt.endedAtUtc = [DateTime]::UtcNow.ToString('o')
        $receipt.exitCode = $code
        if ($receiptPath) {
            try {
                Assert-KpxcWebsitePath $receiptPath | Out-Null
                $temporaryReceipt = $receiptPath + '.tmp'
                Assert-KpxcWebsitePath $temporaryReceipt | Out-Null
                [IO.File]::WriteAllText($temporaryReceipt, ($receipt | ConvertTo-Json -Depth 8), [Text.UTF8Encoding]::new($false))
                Move-Item -LiteralPath $temporaryReceipt -Destination $receiptPath
                Write-Host "Website execution receipt: $receiptPath"
            } catch { Write-Host 'Website execution receipt could not be written.'; if ($code -eq 0) { $code = 1 } }
        }
    }
    return [int]$code
}
if (-not $FunctionsOnly) { throw 'Use the repository root build.bat /s -WebsiteOnly entrypoint.' }
