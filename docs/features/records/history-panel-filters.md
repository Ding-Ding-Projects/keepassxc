# History panel filters

Feature id: `history-panel-filters` · Category: Records and history

## Behaviour

The History destination (`Material::HistoryScreen`) filters revisions by kind chips (all, created, edited, deleted, restored, settings), by date range with two Material date fields (typed locale or ISO dates, and an anchored calendar picker whose month and year choosers are searchable selects), and by a bounded regex search, and offers selection, export and per-revision Diff and Restore controls. An append-only banner under the filter row states the one rule that makes restoring safe. Every row opens with its kind badge (EDIT, SETTINGS, RESTORE or DELETE, tinted like its glyph and included in the accessible name), keeps the label beside it, prints the short digest at the right edge and the meta line beneath.

## Configuration

No configuration beyond the store's retention.

## Failure modes

The reference timeline shows kind badges and a detail card; the built panel differs, recorded as parity defect `history-default-1`.

## Security considerations

Exports carry redacted metadata only.

## Verification

`testmaterialhistory` and parity row `history-default`.

## Suggested articles

- [Local Git-backed version history](../records/local-history.md)
- [Regex builder](../search/regex-builder.md)
