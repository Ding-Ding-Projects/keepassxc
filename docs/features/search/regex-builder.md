# Regex builder

Feature id: `regex-builder` · Category: Search and regex

## Behaviour

Under the pattern field sit the design's **token blocks**: one coloured mono chip per token (literals, classes, groups, lookarounds, quantifiers, anchors, alternation and escapes each in their own tone) with a drag handle. Clicking a block removes that token from the pattern; dragging a block onto another moves it there; with the keyboard, <kbd>Delete</kbd> removes the focused block and <kbd>Ctrl</kbd>+<kbd>Left</kbd> / <kbd>Ctrl</kbd>+<kbd>Right</kbd> move it. The strip is a dashed drop zone that states plainly when the pattern is empty.

Every Material search bar carries a `.*` chip that switches the query to regex mode and a builder affordance that opens the `Material::RegexBuilder` overlay (`src/gui/material/MaterialRegexBuilder.h`) for that exact field. The overlay is a workbench, not a compact dialog:

- **Token palette and pattern library.** Guided token chips (character classes, anchors, quantifiers, groups, alternation, lookaround, backreferences) insert at the caret. Beneath them the pattern library lists the ten shipped presets with their own search field; choosing one loads its pattern, flags and sample.
- **Matches.** The sample text area and the live match and capture-group list, recomputed with `QRegularExpression` on every keystroke, bounded by `Material::RegexSafety`.
- **Explain.** One row per token from `Material::RegexLab::tokenize`, in English and Cantonese, with PCRE2-only constructs marked; clicking a row selects that span in the pattern field.
- **Replace.** A replacement template in `$1`, `$<name>` and `$&` form, translated through `RegexLab::qtReplacement` into the `\1` form `QString::replace` takes, with a live preview against the sample.
- **Export.** The pattern and flags written for `QRegularExpression` C++, JavaScript, Python `re` and `grep -P`, each with a copy action, filtered by the selected dialect.
- **Cheat sheet.** Every construct the builder knows, each row inserting its token and stating whether both engines or only PCRE2 support it.
- **Dialects.** The dialect switch (ECMAScript, QRegularExpression, Both) describes both engines' flags and differences; the subtitle says plainly that QRegularExpression (PCRE2) is the engine that runs, the same one behind every search bar.

Apply writes the pattern and flags back into the search bar that asked; Copy puts the pattern on the clipboard. The logic behind the workbench lives widget-free in `src/gui/material/MaterialRegexLab.h` so its rules are pinned by `testmaterialregexlab`.

## Configuration

Plain-text search is the default; regex is an explicit opt-in per field. Flags are a subset of `gimsu`.

## Failure modes

Evaluation is bounded by `Material::RegexSafety`; a pattern that exceeds the limits is refused with an explicit state rather than run. A named reference in the Replace template that names no group is left as typed rather than silently dropped. Only QRegularExpression executes: the ECMAScript dialect is described and exported, never run.

## Security considerations

Patterns and samples are evaluated locally and never transmitted or persisted.

## Verification

`testmaterialregexlab` (tokenizer spans, dialect exports, replacement translation, presets, cheat sheet), `testmaterialregexsafety` and `testmaterialsearchregistry`; parity row `regex-builder-default`.

## Suggested articles

- [Search bars and the search registry](../search/search-bar-every-surface.md)
- settings-search (not implemented yet; see `docs/features/inventory.json`)
- [Command palette](../navigation/command-palette.md)
