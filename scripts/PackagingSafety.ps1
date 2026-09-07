# Shared, side-effect-free path and candidate checks for the Windows builders.
function Get-KpxcHash([string]$Path) {
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256 -ErrorAction Stop).Hash.ToLowerInvariant()
}

function Resolve-KpxcDirectory([string]$Root, [string]$Path) {
    if ([string]::IsNullOrWhiteSpace($Path)) { throw 'An output directory is required.' }
    if ([IO.Path]::IsPathRooted($Path) -and $Path -notmatch '^[A-Za-z]:[\\/]') { throw 'Use a fully qualified local drive path, not a device, network, or drive-relative path.' }
    if (-not [IO.Path]::IsPathRooted($Path)) { $Path = Join-Path $Root $Path }
    $resolved = [IO.Path]::GetFullPath($Path)
    if ($resolved.Substring(2).Contains(':')) { throw 'Alternate data streams are not build directories.' }
    if ($resolved -eq [IO.Path]::GetPathRoot($resolved)) { throw 'A filesystem root is not a build directory.' }
    $resolved = $resolved.TrimEnd([char[]]'\/')
    Assert-KpxcNoLinks $resolved
    if ((Test-Path -LiteralPath $resolved) -and -not (Test-Path -LiteralPath $resolved -PathType Container)) {
        throw 'A build directory cannot be an existing file.'
    }
    return $resolved
}

function Assert-KpxcNoLinks([string]$Path) {
    $cursor = [IO.Path]::GetFullPath($Path)
    while ($cursor) {
        $item = Get-Item -LiteralPath $cursor -Force -ErrorAction SilentlyContinue
        if ($item -and ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw "Reparse points are not allowed in a build path: $cursor"
        }
        if ($item -and $item.LinkType -eq 'HardLink') { throw "Hard-linked build content is forbidden: $cursor" }
        $parent = [IO.Path]::GetDirectoryName($cursor)
        if ($parent -eq $cursor) { break }
        $cursor = $parent
    }
}

function Test-KpxcContains([string]$Parent, [string]$Child) {
    $parentPath = [IO.Path]::GetFullPath($Parent).TrimEnd([char[]]'\/')
    $childPath = [IO.Path]::GetFullPath($Child).TrimEnd([char[]]'\/')
    return $childPath.Equals($parentPath, [StringComparison]::OrdinalIgnoreCase) -or
        $childPath.StartsWith($parentPath + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)
}

function Assert-KpxcBuildPaths([string]$Root, [string[]]$Directories) {
    $rootPath = [IO.Path]::GetFullPath($Root)
    foreach ($directory in $Directories) {
        if (Test-KpxcContains $directory $rootPath) { throw 'A build directory cannot contain the source checkout.' }
        if (Test-KpxcContains (Join-Path $rootPath '.git') $directory) { throw 'Git administration is not a build directory.' }
    }
    for ($i = 0; $i -lt $Directories.Count; ++$i) {
        for ($j = $i + 1; $j -lt $Directories.Count; ++$j) {
            if ((Test-KpxcContains $Directories[$i] $Directories[$j]) -or
                (Test-KpxcContains $Directories[$j] $Directories[$i])) {
                throw 'Build, staging, scratch, and release directories must not overlap.'
            }
        }
    }
}

function Get-KpxcDirectoryFiles([string]$Directory) {
    Assert-KpxcNoLinks $Directory
    if (-not (Test-Path -LiteralPath $Directory)) { return @() }
    $pending = [Collections.Generic.Queue[string]]::new()
    $pending.Enqueue($Directory)
    $files = @()
    while ($pending.Count) {
        $current = $pending.Dequeue()
        foreach ($item in Get-ChildItem -LiteralPath $current -Force -ErrorAction Stop) {
            if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -or $item.LinkType -eq 'HardLink') { throw "Linked build content is forbidden: $($item.FullName)" }
            if ($item.PSIsContainer) { $pending.Enqueue($item.FullName) } else { $files += $item }
        }
    }
    return $files
}

function Get-KpxcOwnerKey([string]$Root, [string]$Directory) {
    $bytes = [Text.Encoding]::UTF8.GetBytes(([IO.Path]::GetFullPath($Root) + '|' + [IO.Path]::GetFullPath($Directory)).ToLowerInvariant())
    $hash = [Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($hash.ComputeHash($bytes))).Replace('-', '').ToLowerInvariant() }
    finally { $hash.Dispose() }
}

function Assert-KpxcOutputOwnership([string]$Root, [string]$Directory) {
    $files = @(Get-KpxcDirectoryFiles $Directory)
    if (-not $files.Count -and (-not (Test-Path -LiteralPath $Directory) -or
        @(Get-ChildItem -LiteralPath $Directory -Force).Count -eq 0)) { return }
    $marker = Join-Path $Directory '.keepassxc-output-owner.json'
    Assert-KpxcNoLinks $marker
    if (-not (Test-Path -LiteralPath $marker -PathType Leaf)) { throw 'Non-empty release output has no ownership receipt; its contents were preserved.' }
    $owner = Get-Content -Raw -LiteralPath $marker | ConvertFrom-Json
    if ($owner.schemaVersion -ne 1 -or $owner.ownerKey -cne (Get-KpxcOwnerKey $Root $Directory)) { throw 'Release output ownership does not match this checkout and directory.' }
    if (@(Get-ChildItem -LiteralPath $Directory -Directory -Force).Count) { throw 'Release output contains an unexpected directory.' }
    $expected = @{}
    foreach ($file in $owner.files) {
        if (-not $file.name -or [IO.Path]::GetFileName($file.name) -cne $file.name -or
            $file.name -eq '.keepassxc-output-owner.json' -or $expected.ContainsKey($file.name)) { throw 'Malformed output ownership entry.' }
        $expected[$file.name] = $file.sha256
    }
    foreach ($file in $files) {
        if ($file.FullName -eq $marker) { continue }
        if (-not $expected.ContainsKey($file.Name) -or (Get-KpxcHash $file.FullName) -cne $expected[$file.Name]) {
            throw 'Release output contains unowned or changed content; every file was preserved.'
        }
        $expected.Remove($file.Name)
    }
    if ($expected.Count) { throw 'Owned release output is incomplete; it was preserved.' }
}

function Publish-KpxcOutput([string]$Root, [string]$Candidate, [string]$Directory) {
    $Candidate = Resolve-KpxcDirectory $Root $Candidate
    $Directory = Resolve-KpxcDirectory $Root $Directory
    Assert-KpxcBuildPaths $Root @($Candidate, $Directory)
    Assert-KpxcOutputOwnership $Root $Directory
    $newFiles = @(Get-KpxcDirectoryFiles $Candidate)
    if (-not $newFiles.Count -or @(Get-ChildItem -LiteralPath $Candidate -Directory -Force).Count) { throw 'Release candidate must contain regular files only.' }
    if ('.keepassxc-output-owner.json' -in $newFiles.Name) { throw 'Candidate cannot provide its own output ownership marker.' }
    New-Item -ItemType Directory -Force -Path $Directory | Out-Null
    $oldFiles = @(Get-ChildItem -LiteralPath $Directory -File -Force)
    $oldHashes = @{}
    foreach ($file in $oldFiles) { $oldHashes[$file.Name] = Get-KpxcHash $file.FullName }
    $owner = @{schemaVersion=1;ownerKey=(Get-KpxcOwnerKey $Root $Directory);files=@($newFiles | ForEach-Object { @{name=$_.Name;sha256=(Get-KpxcHash $_.FullName)} })}
    $prepared = Join-Path $Directory ('.publish-' + [Guid]::NewGuid().ToString('N'))
    $backup = Join-Path (Split-Path -Parent $Candidate) ('previous-output-' + [Guid]::NewGuid().ToString('N'))
    Assert-KpxcNoLinks $backup
    $newNames = @($newFiles.Name) + @('.keepassxc-output-owner.json')
    $startedReplacement = $false
    try {
        New-Item -ItemType Directory -Path $prepared, $backup -ErrorAction Stop | Out-Null
        foreach ($file in $newFiles) { Copy-Item -LiteralPath $file.FullName -Destination (Join-Path $prepared $file.Name) -ErrorAction Stop }
        foreach ($file in $newFiles) {
            if ((Get-KpxcHash (Join-Path $prepared $file.Name)) -cne (Get-KpxcHash $file.FullName)) { throw 'Prepared publication bytes differ from the verified candidate.' }
        }
        $owner | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $prepared '.keepassxc-output-owner.json') -Encoding UTF8
        foreach ($file in $oldFiles) { Copy-Item -LiteralPath $file.FullName -Destination (Join-Path $backup $file.Name) -ErrorAction Stop }
        if (@(Get-ChildItem -LiteralPath $Directory -Force | Where-Object { $_.FullName -ne $prepared }).Count -ne $oldFiles.Count) { throw 'Release output changed during preparation.' }
        foreach ($file in $oldFiles) {
            Assert-KpxcNoLinks $file.FullName
            if ((Get-KpxcHash $file.FullName) -cne $oldHashes[$file.Name]) { throw 'Release output changed during preparation.' }
            if ((Get-KpxcHash (Join-Path $backup $file.Name)) -cne $oldHashes[$file.Name]) { throw 'Previous output backup did not preserve the original bytes.' }
        }
        $startedReplacement = $true
        # No recursive deletion. Every old file was copied and matched by hash.
        foreach ($file in $oldFiles) { Assert-KpxcNoLinks $file.FullName; Remove-Item -LiteralPath $file.FullName -Force -ErrorAction Stop }
        foreach ($name in $newNames) {
            $source = [IO.Path]::GetFullPath((Join-Path $prepared $name))
            $target = [IO.Path]::GetFullPath((Join-Path $Directory $name))
            if (-not (Test-KpxcContains $prepared $source) -or -not (Test-KpxcContains $Directory $target)) { throw 'Publication escaped its verified directories.' }
            Assert-KpxcNoLinks $source
            Assert-KpxcNoLinks $target
            Move-Item -LiteralPath $source -Destination $target -ErrorAction Stop
        }
    } catch {
        if ($startedReplacement) {
            foreach ($name in $newNames) {
                $target = Join-Path $Directory $name
                Assert-KpxcNoLinks $target
                if (Test-Path -LiteralPath $target -PathType Leaf) { Remove-Item -LiteralPath $target -Force -ErrorAction Stop }
            }
            foreach ($file in $oldFiles) {
                Assert-KpxcNoLinks $file.FullName
                Assert-KpxcNoLinks (Join-Path $backup $file.Name)
                Copy-Item -LiteralPath (Join-Path $backup $file.Name) -Destination $file.FullName -Force -ErrorAction Stop
            }
        }
        throw
    } finally {
        # Remove only filenames created by this attempt, never unknown contents.
        foreach ($name in $newNames) {
            $temporary = Join-Path $prepared $name
            Assert-KpxcNoLinks $temporary
            if (Test-Path -LiteralPath $temporary -PathType Leaf) { Remove-Item -LiteralPath $temporary -Force -ErrorAction Stop }
        }
        if ((Test-Path -LiteralPath $prepared) -and @(Get-ChildItem -LiteralPath $prepared -Force).Count -eq 0) {
            Remove-Item -LiteralPath $prepared -Force -ErrorAction Stop
        }
    }
}

function Assert-KpxcBuildCache([string]$Root, [string]$Directory) {
    if (-not (Test-Path -LiteralPath $Directory) -or @(Get-ChildItem -LiteralPath $Directory -Force).Count -eq 0) { return }
    $cache = Join-Path $Directory 'CMakeCache.txt'
    if (-not (Test-Path -LiteralPath $cache -PathType Leaf)) { throw 'Non-empty build directory has no CMake ownership record.' }
    $text = Get-Content -Raw -LiteralPath $cache
    if ($text -notmatch '(?m)^CMAKE_HOME_DIRECTORY:INTERNAL=(.+)\r?$' -or
        [IO.Path]::GetFullPath($Matches[1].Trim()).TrimEnd([char[]]'\/') -ine [IO.Path]::GetFullPath($Root).TrimEnd([char[]]'\/')) { throw 'Build cache belongs to another source checkout.' }
}

function Assert-KpxcPeX64([string]$Path) {
    $stream = [IO.File]::OpenRead($Path)
    $reader = [IO.BinaryReader]::new($stream)
    try {
        if ($stream.Length -lt 64 -or $reader.ReadUInt16() -ne 0x5A4D) { throw 'Missing DOS executable header.' }
        $stream.Position = 0x3c
        $offset = $reader.ReadInt32()
        if ($offset -lt 64 -or $offset -gt $stream.Length - 26) { throw 'Invalid PE header offset.' }
        $stream.Position = $offset
        if ($reader.ReadUInt32() -ne 0x00004550 -or $reader.ReadUInt16() -ne 0x8664) { throw 'An x64 PE executable is required.' }
        $stream.Position = $offset + 24
        if ($reader.ReadUInt16() -ne 0x20b) { throw 'PE32+ is required.' }
    } finally { $reader.Dispose(); $stream.Dispose() }
}

function Copy-KpxcMsvcRuntime([string]$Stage, [string]$CompilerPath, [string]$RedistDirectory) {
    if ($CompilerPath -notmatch '^(.*)[\\/]VC[\\/]Tools[\\/]MSVC[\\/](14[.][0-9]+)[.][^\\/]+[\\/]bin[\\/]Hostx64[\\/]x64[\\/]cl[.]exe$') {
        throw 'The runtime source must be bound to the selected MSVC x64 compiler.'
    }
    $installation = $Matches[1]
    $family = $Matches[2]
    Assert-KpxcNoLinks $CompilerPath
    if (-not (Test-Path -LiteralPath $CompilerPath -PathType Leaf)) { throw 'The selected MSVC compiler executable is missing.' }
    Assert-KpxcPeX64 $CompilerPath
    $compilerVersion = [Diagnostics.FileVersionInfo]::GetVersionInfo($CompilerPath)
    if ($compilerVersion.FileMajorPart -ne 19 -or $compilerVersion.FileMinorPart -ne [int]$family.Split('.')[1]) { throw 'The compiler executable does not match its selected MSVC toolset family.' }
    $allowed = Join-Path $installation 'VC\Redist\MSVC'
    $redist = Resolve-KpxcDirectory $allowed $RedistDirectory
    if (-not (Test-KpxcContains $allowed $redist)) { throw 'VCToolsRedistDir is outside the selected MSVC installation.' }
    $crt = Join-Path $redist 'x64\Microsoft.VC143.CRT'
    Assert-KpxcNoLinks $crt
    $files = @(Get-ChildItem -LiteralPath $crt -Filter '*.dll' -File -ErrorAction Stop)
    foreach ($required in @('msvcp140.dll','msvcp140_1.dll','msvcp140_2.dll','msvcp140_atomic_wait.dll','msvcp140_codecvt_ids.dll','vcruntime140.dll','vcruntime140_1.dll','concrt140.dll')) {
        if ($required -notin $files.Name) { throw "The selected MSVC runtime is incomplete: $required" }
    }
    foreach ($file in $files) {
        Assert-KpxcNoLinks $file.FullName
        Assert-KpxcPeX64 $file.FullName
        $version = [Diagnostics.FileVersionInfo]::GetVersionInfo($file.FullName)
        if ("$($version.FileMajorPart).$($version.FileMinorPart)" -ne $family) { throw 'MSVC runtime and compiler families differ.' }
        if ($file.Name -notmatch '^(msvcp140(?:_[a-z0-9_]+)?|vcruntime140(?:_[a-z0-9_]+)?|concrt140|vccorlib140)[.]dll$') { throw 'Unexpected DLL in the compiler runtime directory.' }
    }
    Assert-KpxcNoLinks $Stage
    foreach ($file in $files) {
        $target = Join-Path $Stage $file.Name
        Assert-KpxcNoLinks $target
        Copy-Item -LiteralPath $file.FullName -Destination $target -Force -ErrorAction Stop
        if ((Get-KpxcHash $target) -cne (Get-KpxcHash $file.FullName)) { throw 'MSVC runtime copy hash mismatch.' }
    }
    return @($files | ForEach-Object { @{name=$_.Name;sha256=(Get-KpxcHash $_.FullName);version=$_.VersionInfo.FileVersion;architecture='x64'} })
}

function Assert-KpxcStageReceipt([string]$Stage, [string]$ReceiptPath, [string]$ExpectedCommit, [string]$Version) {
    Assert-KpxcNoLinks $Stage
    Assert-KpxcNoLinks $ReceiptPath
    $receipt = Get-Content -Raw -LiteralPath $ReceiptPath -ErrorAction Stop | ConvertFrom-Json
    if ($receipt.schemaVersion -ne 1 -or $receipt.sourceCommit -cne $ExpectedCommit -or $receipt.version -cne $Version -or
        $receipt.architecture -ne 'x64' -or $receipt.stageDirectory -ine $Stage) { throw 'Stage provenance does not match the requested source, version, architecture, and directory.' }
    $exe = Join-Path $Stage 'KeePassXC.exe'
    Assert-KpxcPeX64 $exe
    if ((Get-KpxcHash $exe) -cne $receipt.executableSha256) { throw 'Staged executable differs from its build receipt.' }
    if ($receipt.compiledHead -notmatch '^[0-9a-f]{7,40}$' -or -not $ExpectedCommit.StartsWith($receipt.compiledHead, [StringComparison]::Ordinal) -or
        -not ([Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($exe))).Contains($receipt.compiledHead)) { throw 'Executable source identity does not match the stage receipt.' }
    $info = [Diagnostics.FileVersionInfo]::GetVersionInfo($exe)
    Assert-KpxcExecutableVersion -FileVersion $info.FileVersion -ProductVersion $info.ProductVersion -ExpectedVersion $Version
    $files = @(Get-KpxcDirectoryFiles $Stage)
    if ($files.Count -ne @($receipt.files).Count) { throw 'Stage contains missing or additional files.' }
    $seen = @{}
    foreach ($entry in $receipt.files) {
        $path = [IO.Path]::GetFullPath((Join-Path $Stage $entry.path))
        if (-not (Test-KpxcContains $Stage $path) -or $path -eq $Stage -or $seen.ContainsKey($path)) { throw 'Invalid stage receipt path.' }
        $seen[$path] = $true
        Assert-KpxcNoLinks $path
        if ((Get-KpxcHash $path) -cne $entry.sha256) { throw 'Stage content differs from the build receipt.' }
    }
    if (-not @($receipt.msvcRuntime).Count) { throw 'App-local MSVC runtime provenance is missing.' }
    foreach ($required in @('msvcp140.dll','msvcp140_1.dll','msvcp140_2.dll','msvcp140_atomic_wait.dll','msvcp140_codecvt_ids.dll','vcruntime140.dll','vcruntime140_1.dll','concrt140.dll')) {
        if ($required -notin $receipt.msvcRuntime.name) { throw "Staged runtime provenance is incomplete: $required" }
    }
    foreach ($runtime in $receipt.msvcRuntime) {
        if ([IO.Path]::GetFileName($runtime.name) -cne $runtime.name) { throw 'Invalid runtime receipt filename.' }
        $path = Join-Path $Stage $runtime.name
        Assert-KpxcPeX64 $path
        if ((Get-KpxcHash $path) -cne $runtime.sha256) { throw 'Staged runtime differs from the selected compiler runtime.' }
        if ([Diagnostics.FileVersionInfo]::GetVersionInfo($path).FileVersion -cne $runtime.version) { throw 'Runtime version differs from its receipt.' }
    }
    return $receipt
}
