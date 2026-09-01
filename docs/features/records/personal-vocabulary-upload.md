# Personal vocabulary upload

Feature id: `personal-vocabulary-upload` · Category: Records and history

## Behaviour

Settings carries a visible local JSON upload control with no-file, loaded, invalid, replace and clear states. A valid file is validated and cached in `GUI/PersonalVocabularyCache`; clearing purges the cache and restores original wording. Handling is local-only.

## Configuration

Settings › Personal vocabulary; the cache key is local configuration.

## Failure modes

Application-wide consumption of the cache, bounds tests and history events are still pending, so the row stays partial.

## Security considerations

The vocabulary payload never enters logs, exports, captures or public records.

## Verification

Pending focused tests; the Settings parity row exercises the surface.

## Suggested articles

- settings-search (not implemented yet; see `docs/features/inventory.json`)
- [Local Git-backed version history](../records/local-history.md)
