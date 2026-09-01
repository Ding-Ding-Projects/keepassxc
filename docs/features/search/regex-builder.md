# Regex builder

Feature id: `regex-builder` · Category: Search and regex

## Behaviour

Every Material search bar carries a `.*` chip that switches the query to regex mode and a builder affordance that opens the `Material::RegexBuilder` overlay (`src/gui/material/MaterialRegexBuilder.h`) for that exact field. The builder offers guided token chips (character classes, anchors, quantifiers, groups and alternation), a pattern field with flag chips, a sample text area and a live match and capture-group list recomputed with `QRegularExpression` on every keystroke. Apply writes the pattern and flags back into the search bar that asked; Copy puts the pattern on the clipboard.

## Configuration

Plain-text search is the default; regex is an explicit opt-in per field. Flags are a subset of `gimsu`.

## Failure modes

The reference design's workbench (Matches, Explain, Replace, Export and Cheat sheet tabs, dialect switch, pattern library, token blocks) is not built yet; that is parity defect `regex-builder-default-1` and an open inventory row. Evaluation is bounded by `Material::RegexSafety`; a pattern that exceeds the limits is refused with an explicit state rather than run.

## Security considerations

Patterns and samples are evaluated locally and never transmitted or persisted.

## Verification

`testmaterialregexsafety` and `testmaterialsearchregistry`; parity row `regex-builder-default`.

## Suggested articles

- [Search bars and the search registry](../search/search-bar-every-surface.md)
- settings-search (not implemented yet; see `docs/features/inventory.json`)
- [Command palette](../navigation/command-palette.md)
