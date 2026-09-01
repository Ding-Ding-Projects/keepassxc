# Browser-style tabs

Feature id: `tabs` · Category: Navigation

## Behaviour

Databases open as browser-style tabs in `Material::TabStrip` (`src/gui/material/MaterialTabStrip.h`) with stable runtime ids, keyboard navigation and reordering, pointer drag with insertion markers, pin partition, a registered searchable overflow surface and persisted order.

## Configuration

`GUI/TabOrder`, `GUI/PinnedTabs`, `GUI/TabOverflow`, `GUI/ShowTabStrip`.

## Failure modes

Docking to other edges, groups, the four tab-discovery searches and bulk-close are open inventory rows.

## Security considerations

None.

## Verification

`testmaterialtabs` (three cases).

## Suggested articles

- tab-overflow (not implemented yet; see `docs/features/inventory.json`)
- tab-pinning (not implemented yet; see `docs/features/inventory.json`)
- [Search bars and the search registry](../search/search-bar-every-surface.md)
