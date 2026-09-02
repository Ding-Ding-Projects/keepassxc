# Search bars and the search registry

Feature id: `search-bar-every-surface` · Category: Search and regex

## Behaviour

`Material::SearchRegistry` (`src/gui/material/MaterialSearchRegistry.h`) owns every registered `Material::SearchBar`: it records the current field, routes builder requests to the window, restores focus when the builder closes and keeps each field's query, pattern, flags and mode independent. Vault, Reports, History, Changelog, Settings pages, the sheets, the notification centre and the command palette register their bars; compact More navigation and the group fallback own bounded builders of their own.

The vault carries three: the entry search hosted in the app bar (`vault.entries`), the group filter above the group tree (`vault.groups`, plain text by default, regex on request, a match keeps the group's ancestors visible and expands them), and the group-scope search inside the scope menu (`vault.group-scope`). Beside the entry search the health chips (Breached, Weak, Reused, Healthy) filter the list by verdict and carry live counts; Breached shows no count because breach exposure needs an online check the vault does not make.

When a bar asks for the builder the registry pins that bar as the builder's owner. Focus moves while the builder is open (its own fields, then whatever the window hands focus to when the overlay closes), and every move can make another bar current; restoring focus returns to the bar that asked, not to whichever bar focus wandered through.

## Configuration

`GUI/SearchWaitForEnter` keeps the legacy behaviour of searching on Enter for the vault field.

## Failure modes

Legacy dialogs and menus that are not yet Material have no search bar or builder; they are open inventory rows (`dropdown-search`, `context-menu-search`).

## Security considerations

Search text never leaves the process.

## Verification

`testmaterialsearchregistry` (four cases); `testmaterialvaultfilters` (group filter with ancestors, regex opt-in and unparsable patterns; health chips present, checkable and accessible); `testmaterialshellresponsive` (focus restored to the builder's owner).

## Suggested articles

- [Regex builder](../search/regex-builder.md)
- settings-search (not implemented yet; see `docs/features/inventory.json`)
- [Browser-style tabs](../navigation/tabs.md)
