# Bind Qt deployment to the selected installation without altering that installation.
function Initialize-KpxcQtDeployment([string]$QtRoot, [string]$BuildDirectory) {
    $selectedRoot = [IO.Path]::GetFullPath($QtRoot).TrimEnd('\')
    $buildRoot = [IO.Path]::GetFullPath($BuildDirectory).TrimEnd('\')
    $alias = Join-Path $buildRoot 'qt-deployment-runtime'
    foreach ($tool in @('qmake.exe', 'qtpaths.exe', 'windeployqt.exe')) {
        if (-not (Test-Path -LiteralPath (Join-Path $selectedRoot "bin\$tool") -PathType Leaf)) {
            throw "Selected Qt installation is missing $tool."
        }
    }
    New-Item -ItemType Directory -Force -Path $buildRoot | Out-Null
    if (Test-Path -LiteralPath $alias) {
        $existing = Get-Item -LiteralPath $alias -Force
        if ($existing.LinkType -ne 'Junction' -or @($existing.Target).Count -ne 1 -or
            [IO.Path]::GetFullPath([string]$existing.Target).TrimEnd('\') -ine $selectedRoot) {
            throw 'Qt deployment alias already exists and does not belong to the selected installation.'
        }
    } else {
        # Qt 6.8 normalizes DLL paths through Windows short names. A relocated installation may
        # retain a stale short-name expansion; a build-owned junction supplies a current identity.
        New-Item -ItemType Junction -Path $alias -Target $selectedRoot | Out-Null
    }
    foreach ($tool in @('qmake.exe', 'qtpaths.exe', 'windeployqt.exe')) {
        if ((Get-FileHash -LiteralPath (Join-Path $alias "bin\$tool") -Algorithm SHA256).Hash -cne
            (Get-FileHash -LiteralPath (Join-Path $selectedRoot "bin\$tool") -Algorithm SHA256).Hash) {
            throw 'Qt deployment alias does not expose the selected tool binaries.'
        }
    }
    $qtpaths = Join-Path $alias 'bin\qtpaths.exe'
    $queriedBin = (& $qtpaths --query QT_INSTALL_BINS).Trim()
    if ($LASTEXITCODE -ne 0 -or [IO.Path]::GetFullPath($queriedBin).TrimEnd('\') -ine
        [IO.Path]::GetFullPath((Join-Path $alias 'bin')).TrimEnd('\')) {
        throw 'Selected Qt deployment tool does not resolve the build-owned runtime directory.'
    }
    return @{ root = $alias; qtpaths = $qtpaths; windeployqt = (Join-Path $alias 'bin\windeployqt.exe') }
}
