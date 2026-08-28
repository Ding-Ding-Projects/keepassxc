[CmdletBinding()]
param([switch]$SelfTest)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

function Read-Source([string]$Path) {
    return [IO.File]::ReadAllText((Join-Path $root $Path))
}

function Assert-RepairContracts([hashtable]$Sources) {
    if ($Sources.EntryEditor.Contains('m_attributesModel->setEntryAttributes(m_entry->attributes())')) {
        throw 'The entry editor rebinds its attributes model to the transient entry.'
    }
    if (-not $Sources.EntryEditor.Contains('m_entryAttributes->set(key, source->value(key), source->isProtected(key))')) {
        throw 'The entry editor does not synchronize TOTP into its owned working attributes.'
    }
    if (-not $Sources.VersionResource.Contains('VALUE "SquirrelAwareVersion", "1"')) {
        throw 'The main executable is not marked Squirrel-aware.'
    }
    if ($Sources.OpenDialog.Contains('m_view->triggerQuickUnlock();')) {
        throw 'Opening a database dialog still starts Quick Unlock automatically.'
    }
    if (-not $Sources.MainWindow.Contains('m_updateFailureNotified')) {
        throw 'Repeated update-failure notifications are not deduplicated.'
    }
    if (-not $Sources.MainWindow.Contains('const Qt::WindowStates originalState = windowState();')) {
        throw 'Screen-capture affinity changes do not preserve window state.'
    }
    if (-not $Sources.Settings.Contains('bar->setValue(bar->value() - delta);')) {
        throw 'Settings wheel gestures are not forwarded from content.'
    }
    if (-not $Sources.Passkeys.Contains('passkeyEntrySearch')) {
        throw 'Passkey entry selection has no dedicated search field.'
    }
    if (-not $Sources.History.Contains('restoreDeletedEntries')) {
        throw 'Deleted-entry restoration is missing from local history.'
    }
    if (-not $Sources.History.Contains('databases/%1/repository')) {
        throw 'Per-database local Git repositories are missing.'
    }
}

$sources = @{
    EntryEditor = Read-Source 'src/gui/entry/EditEntryWidget.cpp'
    VersionResource = Read-Source 'cmake/VersionResource.rc'
    OpenDialog = Read-Source 'src/gui/DatabaseOpenDialog.cpp'
    MainWindow = Read-Source 'src/gui/MainWindow.cpp'
    Settings = Read-Source 'src/gui/material/MaterialSpecSheet.cpp'
    Passkeys = Read-Source 'src/gui/passkeys/PasskeyImportDialog.cpp'
    History = Read-Source 'src/gui/material/MaterialHistoryStore.cpp'
}

Assert-RepairContracts $sources

if ($SelfTest) {
    $breaks = @(
        @{ Key = 'EntryEditor'; Value = $sources.EntryEditor + "`nm_attributesModel->setEntryAttributes(m_entry->attributes());" },
        @{ Key = 'VersionResource'; Value = $sources.VersionResource.Replace('VALUE "SquirrelAwareVersion", "1"', '') },
        @{ Key = 'OpenDialog'; Value = $sources.OpenDialog + "`nm_view->triggerQuickUnlock();" },
        @{ Key = 'MainWindow'; Value = $sources.MainWindow.Replace('m_updateFailureNotified', 'updateFailureFlagRemoved') },
        @{ Key = 'Settings'; Value = $sources.Settings.Replace('bar->setValue(bar->value() - delta);', '') },
        @{ Key = 'Passkeys'; Value = $sources.Passkeys.Replace('passkeyEntrySearch', 'entrySearchRemoved') },
        @{ Key = 'History'; Value = $sources.History.Replace('restoreDeletedEntries', 'restoreRemoved') },
        @{ Key = 'History'; Value = $sources.History.Replace('databases/%1/repository', 'repository') }
    )
    foreach ($break in $breaks) {
        $probe = @{} + $sources
        $probe[$break.Key] = $break.Value
        $failed = $false
        try { Assert-RepairContracts $probe } catch { $failed = $true }
        if (-not $failed) { throw "Negative regression stayed green for $($break.Key)." }
    }
}

Write-Host 'PASS: repair contracts are present and negative regressions turn red.'
