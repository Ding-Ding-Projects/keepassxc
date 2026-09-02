# Material Design 3 appearance

Feature id: `material-3-appearance` · Category: Design and appearance

## Behaviour

Every widget resolves colour, typography, shape, elevation and motion through `Material::Theme` (`src/gui/material/MaterialTheme.h`). The theme owns the active scheme (seed and light/dark mode), density and type scale, applies them to the application palette and stylesheet, and emits a change signal so the live interface restyles without a restart. Seed palettes are KeePassXC blue, baseline purple, vault green and signal amber; densities are compact, comfortable and spacious with 40, 52 and 64 px rows.

## Configuration

`GUI/ApplicationTheme` (auto, light, dark), `GUI/MaterialSeed`, `GUI/MaterialDensity`, `GUI/MaterialBackdrop`, `GUI/FontFamily`, `GUI/FontScale`, `GUI/FontWeight`, `GUI/ElementOverrides`, all persisted in the roaming configuration.

## Failure modes

The parity audits list the remaining primitive differences per screen. Boolean settings rows render a real Material switch that answers for the row: one click, one activation, and the owner re-syncs the switch after applying the value. The element-override sliders on the Appearance page carry a caption with the property name and its live pixel value, so a reader knows which property the thumb moves. Every slider is a `Material::Slider` (`src/gui/material/MaterialSlider.h`): the Material Design 3 anatomy with a 16 px track split by a 4 px handle bar that thins while pressed, a stop indicator at the far end, a value label above the handle during a drag, a keyboard focus ring, and a press that jumps straight to the pointer. Every dropdown is a `Material::Select` list box with its own search bar (see [dropdown-select](../search/dropdown-select.md)). No stock combo box or slider remains on a Material surface; the date fields on History and Changelog are the last stock controls and are recorded as open parity defects.

## Security considerations

No network access; fonts come from the installed system faces with a CJK-safe fallback chain.

## Verification

`testmaterialappearance` (five cases, including the captioned override sliders), `testmaterialslider`, `testmaterialselect` and `testmaterialshellresponsive::settingsSwitchRowsToggleAndStayInStep` and the parity rows `appearance-default` and `shell-default`.

## Suggested articles

- [Design-reference parity](../design/design-parity.md)
- font-customization (not implemented yet; see `docs/features/inventory.json`)
- per-element-appearance-editor (not implemented yet; see `docs/features/inventory.json`)
