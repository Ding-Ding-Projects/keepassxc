# Command palette

Feature id: `command-palette` · Category: Navigation

## Behaviour

`Material::CommandPalette` (`src/gui/material/MaterialCommandPalette.h`) lists every `QAction` in the window with its menu path and shortcut plus the shell's destinations, with its own search bar wired to the builder.

## Configuration

Opened from the app bar bolt button and `Ctrl+Shift+F`.

## Failure modes

The canonical shortcut is `Ctrl+Shift+F`, results must render rich inline controls, and selecting a result must teleport to the exact element; all three are open findings.

## Security considerations

None.

## Verification

Registered with the search registry tests; a palette-specific suite is pending.

## Suggested articles

- [Search bars and the search registry](../search/search-bar-every-surface.md)
- [Browser-style tabs](../navigation/tabs.md)
