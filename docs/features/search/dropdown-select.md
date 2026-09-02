# Dropdowns are list boxes with a search bar

Feature id: `dropdown-search` · Category: Search

## Behaviour

Every dropdown in the Material interface is a `Material::Select` (`src/gui/material/MaterialSelect.h`), never a stock combo box. The closed control is an outlined field showing the chosen item with a trailing arrow. Activating it (click, Space, Return, Down or F4) opens an anchored list box whose head is the list's own search bar: plain text by default, with the same Regex chip and builder button as every other search surface. Typing filters the visible rows locally; the arrow keys and Page Up/Down move through what survives; Return chooses the highlighted row; Escape clears the filter first and closes on a second press; focus returns to the field either way. An unparsable regex leaves the list as it was rather than emptying it or searching literally. Reopening the list clears the previous filter and the regex opt-in. A wheel over a closed select changes nothing, so scrolling a page never swaps a setting under the pointer.

Rows may carry their own font, which is how the interface font family list shows every installed face in its own typeface.

## Where it is used

| Select | Object name | Search identity |
|---|---|---|
| Interface font family (Appearance) | `appearanceFontFamily` | `appearance.font-family` |
| Interface font weight (Appearance) | `appearanceFontWeight` | `appearance.font-weight` |
| Element to customize (Appearance) | `appearanceOverrideElement` | `appearance.override-element` |
| Report category (Reports) | `reportsCategory` | `reports.category` |
| History date preset (History) | `historyDatePreset` | `history.date-preset` |
| Settings page picker (narrow Settings hub) | `settingsPagePicker` | `settings.page-picker` |

Each search identity registers with the search registry, so the command palette and the settings search can reach the list's own filter, and each field owns its query, pattern, flags and mode; no two selects share state.

## Configuration

None. The control persists nothing of its own; the owning screen persists the chosen value exactly as it did before.

## Failure modes

An empty list does not open. A filter that matches nothing shows an honest "No choices match" accessible description and keeps the search bar operable. The Regex chip is an explicit opt-in; leaving it off keeps plain-text matching.

## Security considerations

Filtering is local and bounded by the search bar's own pattern limits; nothing is transmitted or persisted.

## Verification

`testmaterialselect` covers the combo-box slice of the API, filtering with plain text and regex including an invalid pattern, keyboard traversal and choice, Escape semantics, focus return, and a sweep asserting that every select on the Appearance, Reports and History screens carries an accessible name and a search identity. `testmaterialappearance` and `testmaterialreports` exercise the screens' selects through the same API they used with the stock control.

## Suggested articles

- [A search bar on every surface](search-bar-every-surface.md)
- [Regex builder](regex-builder.md)
- context-menu-search (not implemented yet; see `docs/features/inventory.json`)
