# Personal vocabulary upload

Feature id: `personal-vocabulary-upload` · Category: Records and history

## Behaviour

Settings › Interface › Language & voice carries a visible local JSON upload control with
no-file, loaded, invalid, replace and clear states. The control is always present, even before a
file exists; until a valid file is supplied every surface renders its original shipped wording.

A chosen file is read once, validated in full, and its canonical form is cached in the local
configuration key `GUI/PersonalVocabularyCache`. The cache is consumed at the user-facing text
boundary: a translator installed after the language translators asks them for the real string
(English, Cantonese or the bilingual pair) and then applies the vocabulary to that string, so the
same file works in every language mode and reaches accessible names built from translated text.
Commands, URLs, identifiers, file paths and factual external records are never translated and are
therefore never rewritten.

Replacements are whole-word and case-sensitive, applied longest key first, so `Database file`
wins over `Database` and `DatabaseWidget` is left alone. Loading a second file replaces the cache
entirely; **Clear personal vocabulary** purges the cache and restores the original wording
immediately. Open windows are re-translated on load and on clear; text that a screen composed
before the change is refreshed on its next rebuild or at the next launch.

The loaded, cleared and invalid notifications are voiced through the shared catalogue
(`share/voice/voice.json`: `vocabulary.loaded`, `vocabulary.cleared`, `vocabulary.invalid`) in
English and Cantonese at every humour level; the facts (entry count, "this computer only", the
schema bounds) are enforced at every level. Row labels follow the Settings hub's language mode.

## Contract

Schema version 1, one JSON object with exactly two members:

```json
{ "schemaVersion": 1, "entries": { "original wording": "replacement" } }
```

Bounds: file at most 64 KiB, at most 500 entries, keys 1 to 128 characters, string values of at
most 512 characters, no `__proto__`, `constructor` or `prototype` keys, no extra members. A file
written by an earlier build with the member named `replacements` is accepted and normalised to
`entries` in the cache. Anything outside the contract is refused as a whole; no partial application
ever happens.

## Configuration

Settings › Personal vocabulary JSON (`Choose file…`) and Clear personal vocabulary. The only stored
state is the validated cache in local configuration; the source path is never persisted.

## Failure modes

- Malformed JSON, a wrong schema version, an extra member, a non-string value, an unsafe key, an
  oversized file or too many entries: the file is refused with a notification and the previous
  cache (if any) stays active.
- A corrupt cache on disk fails closed: zero entries are active and original wording renders.
- The cache is revalidated on every reload, so a hand-edited configuration cannot apply partially.

## Security considerations

Handling is local-only: no network request, no telemetry, no log line carries a mapping. The
payload never enters exports, history snapshots, captures or public records; the Settings
notification reports only the entry count. The cache lives in the local (not roaming) configuration.

## Verification

- `testpersonalvocabulary` covers the schema and every bound, whole-word longest-first
  application, translator application through `QCoreApplication::translate`, replace, corrupt-cache
  fail-closed and clear restoring original wording.
- `node design/parity/vocab-proof.mjs <exe> <repo> <outDir> <json> neutral|private` drives the Settings upload control with a real file on the hidden
  desktop and records load, applied wording, restart persistence, replace, invalid-file refusal and
  clear, keeping the payload out of the retained captures.

## Suggested articles

- [A search bar on every surface](../search/search-bar-every-surface.md)
- [Local Git-backed version history](../records/local-history.md)
