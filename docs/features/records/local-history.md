# Local Git-backed version history

Feature id: `local-history` · Category: Records and history

## Behaviour

Every database gets an isolated local Git repository under application data (`Material::HistoryStore`, `src/gui/material/MaterialHistoryStore.h`). Each KDBX save carries a stable opaque database identity and a versioned Git bundle in encrypted custom data. The bundle contains redacted revision metadata, entry fingerprints and encrypted KDBX snapshots.

Before the primary file write, the application stages the new revision and bundle in a temporary repository. The KDBX contents and bundle are then written together through the existing atomic save path. Only after that write succeeds does the application merge the staged revision into its local history repository. A failed database write discards the staged repository and preserves the prior embedded bundle. A failure to update the separate application history index does not fail a successful database save; the KDBX bundle remains recoverable on the next open.

Database identity stays stable across filesystem moves. Save As creates a fresh identity for the destination while carrying forward the source history ancestry. Backup files carry the history available at the time of the backup without changing the source database identity. Concurrent histories merge append-only when their revision files do not conflict; database contents are never merged by this history operation. Restoring is itself a new revision, so history is never rewritten.

## Configuration

Retention and pruning controls live in Settings › History; the store path is stable per database.

## Failure modes

Embedded bundles are limited to 256 MiB packed data, 1 GiB of expanded object data, 1,000,000 Git objects, 100,000 history ancestors, and 256 nested tree levels. When a limit or history-storage failure prevents bundle preparation, the database save continues with the prior embedded history and a non-blocking notification explains that the newest revision was not embedded. Malformed, oversized, foreign, hash-mismatched and locally stale bundles are rejected before import. Local application history remains available when the KDBX file is opened; a valid embedded bundle can restore it when the file moves to another device.

## Security considerations

Snapshots keep the database's own encryption, and the bundle is stored inside the encrypted KDBX payload. The per-database repository never sits inside the user's folder and has no remote. Bundle decoding checks identity, version, path inventory, commit and object counts, expanded object size, snapshot container signatures, SHA-256 digests and current database fingerprints before merging. A stale bundle is detected when the local repository already contains a descendant. A full rollback of both the KDBX file and its bundle cannot be distinguished on a new device without an external trusted record.

## Verification

The existing `testmaterialhistory` suite covers real Git and KDBX round trips, concurrent writers and deleted-entry restore. The current local `testmaterialhistory` CTest run passed 1/1 after adding KDBX3/KDBX4 transfer, Save As ancestry, divergent history union, malformed digest and bundle rejection, and limit-helper coverage. This is source-level focused evidence only. Full production compilation after the latest edits, backup behavior, failed-save rollback, replay, oversized hostile-input execution, foreign identity, and packaged save/reopen interaction remain unverified; see `HANDOFF.md` for the exact candidate state.

## Suggested articles

- [History panel filters](../records/history-panel-filters.md)
- [Changelog viewer](../records/changelog-viewer.md)
