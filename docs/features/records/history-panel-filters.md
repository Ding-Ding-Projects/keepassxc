# History panel filters

Feature id: `history-panel-filters` · Category: Records and history

## Behaviour

The History destination (`Material::HistoryScreen`) filters revisions by kind chips (all, created, edited, deleted, restored, settings), by date range with native date pickers, and by a bounded regex search, and offers selection, export and per-revision Diff and Restore controls.

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
