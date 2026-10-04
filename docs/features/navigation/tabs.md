# Browser-style tabs

Feature id: `tabs` · Category: Navigation

## Behaviour

Databases open as browser-style tabs in `Material::TabStrip` (`src/gui/material/MaterialTabStrip.h`) with stable runtime ids, keyboard navigation and reordering, pointer drag with insertion markers, pin partition, a registered searchable overflow surface and persisted order. A tab pointer drag emits a tab move request only after it crosses the drag threshold and lands in the same pin partition; it never starts a window move.

## Configuration

### Documentation website navigation

The website retains its five Material primary tabs and the existing overflow,
search, reorder and pin/group controls. Tab hosts now use their complete intrinsic
label width rather than dividing narrow available width below the labels' size.
The tab strip keeps horizontal scrolling and the component's existing focused-tab
scroll behavior. No navigation text is removed, abbreviated or hidden by this repair.

The captured baseline at `edffb9db8be840e7a6db685d57990a1a57f6119c` showed
overlapping labels at 320 CSS pixels. The source fix at
`0404ed3f3cc1592bbcebab869abd7d4b7f728042` passed the focused built-output
CSS contract and navigation-preservation checks. The later built output at
`1f3ca0526e3657be5308da2763287913d3fd6ac6` passed the scoped 48-tuple navigation
capture: 320/1440 CSS-pixel widths, English/Cantonese/bilingual, light/dark and
1/1.25/1.5/2 browser device scale factors. All 960 label measurements stayed in
their targets and no body-overflow state was found. Scroll/overflow, native
background keyboard focus and activation, and a fresh favicon HTTP 200 response
were exercised. Device scale emulation does not establish operating-system
scaling acceptance. Original captures and interaction receipts remain external;
no evidence promotion or publication is claimed.

The aggregate audit remains failed because the browser profile was retained after
deletion was rejected. Owned processes, ports and the named desktop were released.
The retained profile and earlier profiles remain untouched. Successful scoped
navigation checks do not complete that aggregate lifecycle audit.

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
