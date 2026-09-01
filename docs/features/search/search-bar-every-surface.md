# Search bars and the search registry

Feature id: `search-bar-every-surface` · Category: Search and regex

## Behaviour

`Material::SearchRegistry` (`src/gui/material/MaterialSearchRegistry.h`) owns every registered `Material::SearchBar`: it records the current field, routes builder requests to the window, restores focus when the builder closes and keeps each field's query, pattern, flags and mode independent. Vault, Reports, History, Changelog, Settings pages, the sheets, the notification centre and the command palette register their bars; compact More navigation and the group fallback own bounded builders of their own.

## Configuration

`GUI/SearchWaitForEnter` keeps the legacy behaviour of searching on Enter for the vault field.

## Failure modes

Legacy dialogs and menus that are not yet Material have no search bar or builder; they are open inventory rows (`dropdown-search`, `context-menu-search`).

## Security considerations

Search text never leaves the process.

## Verification

`testmaterialsearchregistry` (four cases).

## Suggested articles

- [Regex builder](../search/regex-builder.md)
- settings-search (not implemented yet; see `docs/features/inventory.json`)
- [Browser-style tabs](../navigation/tabs.md)
