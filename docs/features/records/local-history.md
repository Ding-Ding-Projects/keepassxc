# Local Git-backed version history

Feature id: `local-history` · Category: Records and history

## Behaviour

Every database gets an isolated, encrypted local Git repository under application data (`Material::HistoryStore`, `src/gui/material/MaterialHistoryStore.h`). Saves commit redacted metadata, opaque ids, fingerprints and a validated encrypted KDBX snapshot in one transaction; bounded locks serialise writers; retrieval re-validates containment, signatures and SHA-256 and never overwrites a live database. Restoring is itself a new revision, so history is append-only.

## Configuration

Retention and pruning controls live in Settings › History; the store path is stable per database.

## Failure modes

A failed history write never fails the user's save; it is logged and surfaced as a notification.

## Security considerations

Snapshots keep the database's own encryption; the repository never sits inside the user's folder.

## Verification

`testmaterialhistory` (seven cases, real Git and KDBX round trips, concurrent writers, deleted-entry restore).

## Suggested articles

- [History panel filters](../records/history-panel-filters.md)
- [Changelog viewer](../records/changelog-viewer.md)
