# Shared, side-effect-free path and candidate checks for the Windows builders.
function Get-KpxcHash([string]$Path) {
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256 -ErrorAction Stop).Hash.ToLowerInvariant()
}

function Resolve-KpxcDirectory([string]$Root, [string]$Path) {
    if ([string]::IsNullOrWhiteSpace($Path)) { throw 'An output directory is required.' }
    if ([IO.Path]::IsPathRooted($Path) -and $Path -notmatch '^[A-Za-z]:[\\/]') { throw 'Use a fully qualified local drive path, not a device, network, or drive-relative path.' }
    if (-not [IO.Path]::IsPathRooted($Path)) { $Path = Join-Path $Root $Path }
    foreach ($segment in $Path.Substring(3).Split([char[]]'\/')) {
        if ($segment -notin @('.','..') -and $segment -match '[. ]$|~[0-9]+(?:[.].*)?$|^(?i:con|prn|aux|nul|com[1-9]|lpt[1-9])(?:[.]|$)') { throw 'Ambiguous Windows path aliases and reserved names are not build directories.' }
    }
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

function Get-KpxcTreeManifest([string]$Directory) {
    $entries = @()
    $queue = [Collections.Generic.Queue[string]]::new()
    Assert-KpxcNoLinks $Directory
    $queue.Enqueue($Directory)
    while ($queue.Count) {
        foreach ($item in Get-ChildItem -LiteralPath $queue.Dequeue() -Force -ErrorAction Stop) {
            Assert-KpxcNoLinks $item.FullName
            $relative = $item.FullName.Substring($Directory.Length + 1).Replace('\','/')
            if ($item.PSIsContainer) {
                $entries += @{path=$relative;type='directory';sha256=''}
                $queue.Enqueue($item.FullName)
            } else { $entries += @{path=$relative;type='file';sha256=(Get-KpxcHash $item.FullName)} }
        }
    }
    return $entries
}

function Test-KpxcTreeManifest([string]$Directory, $Entries) {
    if (-not (Test-Path -LiteralPath $Directory -PathType Container)) { return $false }
    $actual = @(Get-KpxcTreeManifest $Directory)
    if ($actual.Count -ne @($Entries).Count) { return $false }
    $expected = @{}
    foreach ($entry in $Entries) {
        if (-not $entry.path -or $entry.type -notin @('file','directory') -or $expected.ContainsKey($entry.path)) { throw 'Invalid transaction inventory.' }
        $expected[$entry.path] = $entry.type + '|' + $entry.sha256
    }
    foreach ($entry in $actual) {
        if (-not $expected.ContainsKey($entry.path) -or $expected[$entry.path] -cne ($entry.type + '|' + $entry.sha256)) { return $false }
    }
    return $true
}

function Get-KpxcTransactionPath([string]$Directory) {
    return Join-Path (Split-Path -Parent $Directory) ('.' + (Split-Path -Leaf $Directory) + '.keepassxc-transaction.json')
}

function New-KpxcDirectoryCandidate([string]$Root, [string]$Directory) {
    $Directory = Resolve-KpxcDirectory $Root $Directory
    Assert-KpxcBuildPaths $Root @($Directory)
    $path = Join-Path (Split-Path -Parent $Directory) ('.keepassxc-candidate-' + (Split-Path -Leaf $Directory) + '-' + [Guid]::NewGuid().ToString('N'))
    Assert-KpxcNoLinks $path
    New-Item -ItemType Directory -Path $path -ErrorAction Stop | Out-Null
    return $path
}

function Move-KpxcTransactionDirectory([string]$Source, [string]$Destination, [string]$Parent) {
    $Source = [IO.Path]::GetFullPath($Source)
    $Destination = [IO.Path]::GetFullPath($Destination)
    if ((Split-Path -Parent $Source) -ine $Parent -or (Split-Path -Parent $Destination) -ine $Parent -or $Source -ieq $Destination) { throw 'Transaction rename escaped its verified parent.' }
    Assert-KpxcNoLinks $Source
    Assert-KpxcNoLinks $Destination
    [IO.Directory]::Move($Source,$Destination)
}

function Repair-KpxcDirectoryPublicationCore([string]$Root, [string]$Directory) {
    $journalPath = Get-KpxcTransactionPath $Directory
    if (-not (Test-Path -LiteralPath $journalPath)) { return }
    Assert-KpxcNoLinks $journalPath
    $journal = Get-Content -Raw -LiteralPath $journalPath | ConvertFrom-Json
    $parent = Split-Path -Parent $Directory
    $leaf = Split-Path -Leaf $Directory
    if ($journal.schemaVersion -ne 1 -or $journal.ownerKey -cne (Get-KpxcOwnerKey $Root $Directory) -or $journal.directory -ine $Directory -or $journal.id -notmatch '^[0-9a-f]{32}$') { throw 'Transaction journal does not belong to this directory.' }
    $candidate = Join-Path $parent ('.keepassxc-candidate-' + $leaf + '-' + $journal.id)
    $backup = Join-Path $parent ('.keepassxc-previous-' + $leaf + '-' + $journal.id)
    Assert-KpxcNoLinks $candidate
    Assert-KpxcNoLinks $backup
    $oldAtDestination = $journal.hadOld -and (Test-KpxcTreeManifest $Directory $journal.oldEntries)
    $newAtCandidate = Test-KpxcTreeManifest $candidate $journal.newEntries
    if ($oldAtDestination -and $newAtCandidate -and -not (Test-Path -LiteralPath $backup)) {
        Move-KpxcTransactionDirectory $Directory $backup $parent
    }
    if (-not (Test-Path -LiteralPath $Directory) -and $newAtCandidate -and
        (-not $journal.hadOld -or (Test-KpxcTreeManifest $backup $journal.oldEntries))) {
        Move-KpxcTransactionDirectory $candidate $Directory $parent
    }
    if (Test-KpxcTreeManifest $Directory $journal.newEntries) {
        if ($journal.hadOld -and -not (Test-KpxcTreeManifest $backup $journal.oldEntries)) { throw 'Previous generation is not preserved; transaction was retained.' }
        Remove-Item -LiteralPath $journalPath -Force -ErrorAction Stop
        return
    }
    # If preparation was lost, restore only a fully verified previous generation.
    if (-not (Test-Path -LiteralPath $Directory) -and $journal.hadOld -and (Test-KpxcTreeManifest $backup $journal.oldEntries)) {
        Move-KpxcTransactionDirectory $backup $Directory $parent
        Remove-Item -LiteralPath $journalPath -Force -ErrorAction Stop
        return
    }
    if ($oldAtDestination -and -not (Test-Path -LiteralPath $backup)) {
        Remove-Item -LiteralPath $journalPath -Force -ErrorAction Stop
        return
    }
    throw 'Transaction contents do not match the recorded generations; all paths were preserved.'
}

function Repair-KpxcDirectoryPublication([string]$Root, [string]$Directory) {
    $Directory = Resolve-KpxcDirectory $Root $Directory
    Assert-KpxcBuildPaths $Root @($Directory)
    $mutex = [Threading.Mutex]::new($false,('Local\KeePassXC.BuildDirectory.' + (Get-KpxcOwnerKey $Directory $Directory)))
    $held = $false
    try {
        try { $held=$mutex.WaitOne(0) } catch [Threading.AbandonedMutexException] { $held=$true }
        if (-not $held) { throw 'Another publication owns this directory.' }
        Repair-KpxcDirectoryPublicationCore $Root $Directory
    } finally { if ($held) { $mutex.ReleaseMutex() }; $mutex.Dispose() }
}

function Publish-KpxcDirectory([string]$Root, [string]$Candidate, [string]$Directory, [scriptblock]$ValidateExisting, [scriptblock]$AfterFirstRename) {
    $Directory = Resolve-KpxcDirectory $Root $Directory
    $Candidate = Resolve-KpxcDirectory $Root $Candidate
    Assert-KpxcBuildPaths $Root @($Candidate,$Directory)
    $parent = Split-Path -Parent $Directory
    $leaf = Split-Path -Leaf $Directory
    if ((Split-Path -Parent $Candidate) -ine $parent -or (Split-Path -Leaf $Candidate) -notmatch ('^[.]keepassxc-candidate-' + [regex]::Escape($leaf) + '-([0-9a-f]{32})$')) { throw 'Candidate must be a generated same-volume sibling.' }
    $id=$Matches[1]
    $backup=Join-Path $parent ('.keepassxc-previous-' + $leaf + '-' + $id)
    $journalPath=Get-KpxcTransactionPath $Directory
    $mutex=[Threading.Mutex]::new($false,('Local\KeePassXC.BuildDirectory.' + (Get-KpxcOwnerKey $Directory $Directory)))
    $held=$false
    $journal=$null
    $journalPublished=$false
    try {
        try { $held=$mutex.WaitOne(0) } catch [Threading.AbandonedMutexException] { $held=$true }
        if (-not $held) { throw 'Another publication owns this directory.' }
        Repair-KpxcDirectoryPublicationCore $Root $Directory
        & $ValidateExisting
        $hadOld=Test-Path -LiteralPath $Directory
        $oldEntries=@(); if($hadOld){$oldEntries=@(Get-KpxcTreeManifest $Directory)}
        $journal=@{schemaVersion=1;id=$id;directory=$Directory;ownerKey=(Get-KpxcOwnerKey $Root $Directory);hadOld=$hadOld;oldEntries=$oldEntries;newEntries=@(Get-KpxcTreeManifest $Candidate)}
        Assert-KpxcNoLinks $journalPath
        Assert-KpxcNoLinks $backup
        $bytes=[Text.Encoding]::UTF8.GetBytes(($journal | ConvertTo-Json -Depth 8))
        $temporaryJournal=$journalPath + '.tmp.' + $id
        Assert-KpxcNoLinks $temporaryJournal
        $stream=[IO.FileStream]::new($temporaryJournal,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read)
        try { $stream.Write($bytes,0,$bytes.Length); $stream.Flush($true) } finally { $stream.Dispose() }
        [IO.File]::Move($temporaryJournal,$journalPath)
        $journalPublished=$true
        if($hadOld){Move-KpxcTransactionDirectory $Directory $backup $parent}
        if($AfterFirstRename){& $AfterFirstRename}
        Move-KpxcTransactionDirectory $Candidate $Directory $parent
        if(-not (Test-KpxcTreeManifest $Directory $journal.newEntries)){throw 'Published generation hash verification failed.'}
        Remove-Item -LiteralPath $journalPath -Force -ErrorAction Stop
    } catch {
        $originalError=$_
        if($journalPublished -and (Test-Path -LiteralPath $journalPath)) {
            if(Test-KpxcTreeManifest $Directory $journal.newEntries) {
                if(Test-Path -LiteralPath $Candidate){throw 'Rollback candidate already exists; transaction was retained.'}
                Move-KpxcTransactionDirectory $Directory $Candidate $parent
            }
            if($journal.hadOld -and -not (Test-Path -LiteralPath $Directory) -and (Test-KpxcTreeManifest $backup $journal.oldEntries)) {
                Move-KpxcTransactionDirectory $backup $Directory $parent
            }
            if(($journal.hadOld -and (Test-KpxcTreeManifest $Directory $journal.oldEntries)) -or (-not $journal.hadOld -and -not (Test-Path -LiteralPath $Directory))) {
                Remove-Item -LiteralPath $journalPath -Force -ErrorAction Stop
            }
        }
        throw $originalError
    } finally { if($held){$mutex.ReleaseMutex()};$mutex.Dispose() }
}

function Publish-KpxcOutput([string]$Root, [string]$Candidate, [string]$Directory, [scriptblock]$AfterFirstRename) {
    $Candidate=Resolve-KpxcDirectory $Root $Candidate
    $Directory=Resolve-KpxcDirectory $Root $Directory
    Assert-KpxcBuildPaths $Root @($Candidate,$Directory)
    Repair-KpxcDirectoryPublication $Root $Directory
    Assert-KpxcOutputOwnership $Root $Directory
    $files=@(Get-KpxcDirectoryFiles $Candidate)
    if(-not $files.Count -or @(Get-ChildItem -LiteralPath $Candidate -Directory -Force).Count -or '.keepassxc-output-owner.json' -in $files.Name){throw 'Release candidate must contain regular payload files only.'}
    $prepared=New-KpxcDirectoryCandidate $Root $Directory
    foreach($file in $files){
        $target=Join-Path $prepared $file.Name
        Copy-Item -LiteralPath $file.FullName -Destination $target -ErrorAction Stop
        if((Get-KpxcHash $target) -cne (Get-KpxcHash $file.FullName)){throw 'Prepared publication bytes differ from the verified candidate.'}
    }
    $owner=@{schemaVersion=1;ownerKey=(Get-KpxcOwnerKey $Root $Directory);files=@($files | ForEach-Object {@{name=$_.Name;sha256=(Get-KpxcHash $_.FullName)}})}
    $owner | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $prepared '.keepassxc-output-owner.json') -Encoding UTF8
    Publish-KpxcDirectory $Root $prepared $Directory {Assert-KpxcOutputOwnership $Root $Directory} $AfterFirstRename
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

function Test-KpxcMsvcEnvironment([string]$CompilerPath) {
    if (-not $env:INCLUDE -or -not $env:LIB -or -not $env:VCToolsRedistDir -or -not $env:VCToolsInstallDir -or -not $env:WindowsSdkDir -or -not $env:WindowsSDKVersion) { return $false }
    if ($CompilerPath -notmatch '^(.*)[\\/]VC[\\/]Tools[\\/]MSVC[\\/]([0-9.]+)[\\/]bin[\\/]Hostx64[\\/]x64[\\/]cl[.]exe$') { return $false }
    $installation=$Matches[1]
    $toolsetVersion=$Matches[2]
    $toolset=Join-Path $installation ('VC\Tools\MSVC\' + $toolsetVersion)
    try {
        if ([IO.Path]::GetFullPath($env:VCToolsInstallDir).TrimEnd([char[]]'\/') -ine $toolset) { return $false }
        if (-not (Test-KpxcContains (Join-Path $installation 'VC\Redist\MSVC') $env:VCToolsRedistDir)) { return $false }
        $runtime=Join-Path $env:VCToolsRedistDir 'x64\Microsoft.VC143.CRT\msvcp140.dll'
        Assert-KpxcNoLinks $runtime
        if(-not (Test-Path -LiteralPath $runtime -PathType Leaf)){return $false}
        $runtimeVersion=[Diagnostics.FileVersionInfo]::GetVersionInfo($runtime)
        if($runtimeVersion.FileMajorPart -ne 14 -or $runtimeVersion.FileMinorPart -ne [int]$toolsetVersion.Split('.')[1]){return $false}
        $includes=@($env:INCLUDE.Split(';') | Where-Object { $_ -match '[\\/]VC[\\/]Tools[\\/]MSVC[\\/]' })
        $libraries=@($env:LIB.Split(';') | Where-Object { $_ -match '[\\/]VC[\\/]Tools[\\/]MSVC[\\/]' })
        if (-not $includes.Count -or -not $libraries.Count) { return $false }
        foreach($path in @($includes)+@($libraries)){if(-not (Test-KpxcContains $toolset $path)){return $false}}
        $normalizedIncludes=@($includes | ForEach-Object {[IO.Path]::GetFullPath($_).TrimEnd([char[]]'\/')})
        $normalizedLibraries=@($libraries | ForEach-Object {[IO.Path]::GetFullPath($_).TrimEnd([char[]]'\/')})
        if((Join-Path $toolset 'include') -inotIn $normalizedIncludes -or (Join-Path $toolset 'lib\x64') -inotIn $normalizedLibraries){return $false}
        $sdkVersion=$env:WindowsSDKVersion.TrimEnd([char[]]'\/')
        if($sdkVersion -notmatch '^[0-9]{1,5}[.][0-9]{1,5}[.][0-9]{1,5}[.][0-9]{1,5}$' -or -not [IO.Path]::IsPathRooted($env:WindowsSdkDir)){return $false}
        $sdkRoot=[IO.Path]::GetFullPath($env:WindowsSdkDir).TrimEnd([char[]]'\/')
        Assert-KpxcNoLinks $sdkRoot
        $allIncludes=@($env:INCLUDE.Split(';') | Where-Object {$_} | ForEach-Object {[IO.Path]::GetFullPath($_).TrimEnd([char[]]'\/')})
        $allLibraries=@($env:LIB.Split(';') | Where-Object {$_} | ForEach-Object {[IO.Path]::GetFullPath($_).TrimEnd([char[]]'\/')})
        foreach($component in @('ucrt','shared','um','winrt')){
            $path=Join-Path $sdkRoot ('Include\'+$sdkVersion+'\'+$component)
            if($path -inotIn $allIncludes -or -not (Test-Path -LiteralPath $path -PathType Container)){return $false}
        }
        foreach($component in @('ucrt','um')){
            $path=Join-Path $sdkRoot ('Lib\'+$sdkVersion+'\'+$component+'\x64')
            if($path -inotIn $allLibraries -or -not (Test-Path -LiteralPath $path -PathType Container)){return $false}
        }
        foreach($file in @("Include\$sdkVersion\um\windows.h","Include\$sdkVersion\ucrt\stdio.h","Lib\$sdkVersion\um\x64\kernel32.lib","Lib\$sdkVersion\ucrt\x64\ucrt.lib")){
            if(-not (Test-Path -LiteralPath (Join-Path $sdkRoot $file) -PathType Leaf)){return $false}
        }
        return $true
    } catch { return $false }
}

function Initialize-KpxcMsvcEnvironment {
    $command=Get-Command cl.exe -ErrorAction SilentlyContinue
    $expectedCompiler=$null
    $versionArgument=''
    $vcvars=$null
    if($command){
        $expectedCompiler=$command.Source
        if($expectedCompiler -notmatch '^(.*)[\\/]VC[\\/]Tools[\\/]MSVC[\\/]([0-9.]+)[\\/]bin[\\/]Hostx64[\\/]x64[\\/]cl[.]exe$'){throw 'The discovered cl.exe is not a supported MSVC x64 compiler.'}
        $vcvars=Join-Path $Matches[1] 'VC\Auxiliary\Build\vcvars64.bat'
        $versionArgument=' -vcvars_ver=' + $Matches[2]
    } else {
        $vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
        if(Test-Path -LiteralPath $vswhere){
            $installation=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
            if($installation){$vcvars=Join-Path $installation 'VC\Auxiliary\Build\vcvars64.bat'}
        }
        if(-not $vcvars -or -not (Test-Path -LiteralPath $vcvars)){
            $vcvars=@(
                (Join-Path $env:LOCALAPPDATA 'KeePassXCMaterial\toolchain\BuildTools\VC\Auxiliary\Build\vcvars64.bat'),
                (Join-Path $env:LOCALAPPDATA 'material-virtualbox-toolchain\BuildTools\VC\Auxiliary\Build\vcvars64.bat')
            ) | Where-Object {Test-Path -LiteralPath $_ -PathType Leaf} | Select-Object -First 1
        }
    }
    if(-not $vcvars -or -not (Test-Path -LiteralPath $vcvars -PathType Leaf)){throw 'The matching MSVC x64 environment initializer is unavailable.'}
    Assert-KpxcNoLinks $vcvars
    # Always activate the selected toolset in a fresh child. Reset its inherited
    # initialization markers only there; never modify user or machine settings.
    $reset='set INCLUDE=&set LIB=&set LIBPATH=&set VSCMD_VER=&set __VSCMD_PREINIT_PATH=&set VCToolsInstallDir=&set VCToolsVersion=&set VCToolsRedistDir=&set WindowsSdkDir=&set WindowsSDKVersion=&set UniversalCRTSdkDir=&set UCRTVersion=&'
    cmd.exe /d /s /c "$reset call `"$vcvars`"$versionArgument >nul && set" | ForEach-Object {
        if($_ -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}
    }
    if($LASTEXITCODE -ne 0){throw 'The matching MSVC environment initializer failed.'}
    $resolved=(Get-Command cl.exe -ErrorAction Stop).Source
    if($expectedCompiler -and $resolved -ine $expectedCompiler){throw 'MSVC initialization changed the selected compiler instead of repairing its environment.'}
    if(-not (Test-KpxcMsvcEnvironment $resolved)){throw 'MSVC headers, libraries, or runtime source remain inconsistent after initialization.'}
    return $resolved
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

function Assert-KpxcStageReceipt([string]$Stage, [string]$ReceiptPath, [string]$ExpectedCommit, [string]$Version, [string]$RecordedStageDirectory = $Stage) {
    Assert-KpxcNoLinks $Stage
    Assert-KpxcNoLinks $ReceiptPath
    $receipt = Get-Content -Raw -LiteralPath $ReceiptPath -ErrorAction Stop | ConvertFrom-Json
    if ($receipt.schemaVersion -ne 1 -or $receipt.sourceCommit -cne $ExpectedCommit -or $receipt.version -cne $Version -or
        $receipt.architecture -ne 'x64' -or $receipt.stageDirectory -ine $RecordedStageDirectory) { throw 'Stage provenance does not match the requested source, version, architecture, and directory.' }
    $exe = Join-Path $Stage 'KeePassXC.exe'
    Assert-KpxcPeX64 $exe
    if ((Get-KpxcHash $exe) -cne $receipt.executableSha256) { throw 'Staged executable differs from its build receipt.' }
    if ($receipt.compiledHead -notmatch '^[0-9a-f]{7,40}$' -or -not $ExpectedCommit.StartsWith($receipt.compiledHead, [StringComparison]::Ordinal) -or
        -not ([Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($exe))).Contains($receipt.compiledHead)) { throw 'Executable source identity does not match the stage receipt.' }
    $info = [Diagnostics.FileVersionInfo]::GetVersionInfo($exe)
    Assert-KpxcExecutableVersion -FileVersion $info.FileVersion -ProductVersion $info.ProductVersion -ExpectedVersion $Version
    $files = @(Get-KpxcDirectoryFiles $Stage | Where-Object { $_.FullName -ine (Join-Path $Stage '.keepassxc-stage-provenance.json') })
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

function Assert-KpxcStageOwnership([string]$Root, [string]$Stage, [string]$LegacyReceiptPath) {
    if (-not (Test-Path -LiteralPath $Stage) -or @(Get-ChildItem -LiteralPath $Stage -Force).Count -eq 0) { return }
    $receiptPath=Join-Path $Stage '.keepassxc-stage-provenance.json'
    if(-not (Test-Path -LiteralPath $receiptPath) -and $LegacyReceiptPath){$receiptPath=$LegacyReceiptPath}
    Assert-KpxcNoLinks $receiptPath
    $previous=Get-Content -Raw -LiteralPath $receiptPath -ErrorAction Stop | ConvertFrom-Json
    Assert-KpxcStageReceipt $Stage $receiptPath $previous.sourceCommit $previous.version | Out-Null
    if($previous.ownerKey -cne (Get-KpxcOwnerKey $Root $Stage)){throw 'Existing stage is not owned by this checkout.'}
}

function Publish-KpxcStageGeneration([string]$Root, [string]$Stage, [scriptblock]$Install, [scriptblock]$Runtime, [scriptblock]$Receipt, [scriptblock]$Validate, [scriptblock]$ValidateExisting, [scriptblock]$AfterFirstRename) {
    Repair-KpxcDirectoryPublication $Root $Stage
    & $ValidateExisting
    $candidate=New-KpxcDirectoryCandidate $Root $Stage
    & $Install $candidate
    & $Runtime $candidate
    & $Receipt $candidate
    & $Validate $candidate
    Publish-KpxcDirectory $Root $candidate $Stage $ValidateExisting $AfterFirstRename
}

function Assert-KpxcPackagedPayload($Archive, $Provenance) {
    if(@($Archive.Entries | Where-Object { [IO.Path]::GetFileName($_.FullName.Replace('/','\')) -eq '.keepassxc-stage-provenance.json' }).Count){throw 'Private stage provenance must not enter the package.'}
    $expected = @{'lib/net45/KeePassXC.exe'=$Provenance.stagedExecutable.sha256}
    foreach ($required in @('msvcp140.dll','msvcp140_1.dll','msvcp140_2.dll','msvcp140_atomic_wait.dll','msvcp140_codecvt_ids.dll','vcruntime140.dll','vcruntime140_1.dll','concrt140.dll')) {
        if ($required -notin $Provenance.msvcRuntime.name) { throw "Package runtime provenance is incomplete: $required" }
    }
    foreach ($runtime in $Provenance.msvcRuntime) {
        if (-not $runtime.name -or [IO.Path]::GetFileName($runtime.name) -cne $runtime.name) { throw 'Invalid packaged runtime filename.' }
        $name = 'lib/net45/' + $runtime.name
        if ($expected.ContainsKey($name)) { throw 'Duplicate packaged runtime provenance.' }
        $expected[$name] = $runtime.sha256
    }
    foreach ($name in $expected.Keys) {
        if ($expected[$name] -notmatch '^[0-9a-fA-F]{64}$') { throw 'Package payload SHA-256 provenance is missing or malformed.' }
        $entries = @($Archive.Entries | Where-Object { $_.FullName.Replace('\','/').Equals($name, [StringComparison]::OrdinalIgnoreCase) })
        if ($entries.Count -ne 1) { throw "Required package payload is missing or duplicated: $name" }
        $input = $entries[0].Open()
        $hash = [Security.Cryptography.SHA256]::Create()
        try { $actual = ([BitConverter]::ToString($hash.ComputeHash($input))).Replace('-','').ToLowerInvariant() }
        finally { $hash.Dispose(); $input.Dispose() }
        if ($actual -ine $expected[$name]) { throw "Packaged bytes differ from the verified stage: $name" }
    }
}
