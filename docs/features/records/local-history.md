# Local Git-backed version history

Feature id: `local-history` · Category: Records and history

## Behaviour

Every database gets an isolated local Git repository under application data (`Material::HistoryStore`, `src/gui/material/MaterialHistoryStore.h`). Each KDBX save carries a stable opaque database identity and a versioned Git bundle in encrypted custom data. The bundle contains redacted revision metadata, entry fingerprints and encrypted KDBX snapshots.

Before the primary file write, the application stages the new revision and bundle in a temporary repository. The KDBX contents and bundle are then written together through the existing atomic save path. Only after that write succeeds does the application merge the staged revision into its local history repository. A failed database write discards the staged repository and preserves the prior embedded bundle. A failure to update the separate application history index does not fail a successful database save; the KDBX bundle remains recoverable on the next open.

Database identity stays stable across filesystem moves and Save As. Backup files carry the history available at the time of the backup. Concurrent histories merge append-only when their revision files do not conflict. Restoring is itself a new revision, so history is never rewritten.

## Configuration

Retention and pruning controls live in Settings › History; the store path is stable per database.

## Failure modes

Embedded bundles are limited to 64 MiB, 4,096 commits, 100,000 objects and 512 MiB of expanded object data. When a limit or history-storage failure prevents bundle preparation, the database save continues with the prior embedded history and a non-blocking notification explains that the newest revision was not embedded. Malformed, oversized, foreign, hash-mismatched and locally stale bundles are rejected before import. Local application history remains available when the KDBX file is opened; a valid embedded bundle can restore it when the file moves to another device.

## Security considerations

Snapshots keep the database's own encryption, and the bundle is stored inside the encrypted KDBX payload. The per-database repository never sits inside the user's folder and has no remote. Bundle decoding checks identity, version, path inventory, commit and object counts, expanded object size, snapshot container signatures, SHA-256 digests and current database fingerprints before merging. A stale bundle is detected when the local repository already contains a descendant. A full rollback of both the KDBX file and its bundle cannot be distinguished on a new device without an external trusted record.

## Verification

The existing `testmaterialhistory` suite covers real Git and KDBX round trips, concurrent writers and deleted-entry restore. This change's local build and focused test status are recorded in `HANDOFF.md`; the embedded-bundle cases must cover successful transfer, failed-save rollback, Save As, backup, concurrent histories, replay, malformed input, size limits, foreign identity and digest mismatch.

## Suggested articles

- [History panel filters](../records/history-panel-filters.md)
- [Changelog viewer](../records/changelog-viewer.md)
