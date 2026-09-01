# Material Design 3 appearance

Feature id: `material-3-appearance` · Category: Design and appearance

## Behaviour

Every widget resolves colour, typography, shape, elevation and motion through `Material::Theme` (`src/gui/material/MaterialTheme.h`). The theme owns the active scheme (seed and light/dark mode), density and type scale, applies them to the application palette and stylesheet, and emits a change signal so the live interface restyles without a restart. Seed palettes are KeePassXC blue, baseline purple, vault green and signal amber; densities are compact, comfortable and spacious with 40, 52 and 64 px rows.

## Configuration

`GUI/ApplicationTheme` (auto, light, dark), `GUI/MaterialSeed`, `GUI/MaterialDensity`, `GUI/MaterialBackdrop`, `GUI/FontFamily`, `GUI/FontScale`, `GUI/FontWeight`, `GUI/ElementOverrides`, all persisted in the roaming configuration.

## Failure modes

The window still uses the operating-system caption bar with DWM attributes; the frameless Material title bar is an open inventory row. The parity audits list the remaining primitive differences per screen.

## Security considerations

No network access; fonts come from the installed system faces with a CJK-safe fallback chain.

## Verification

`testmaterialappearance` (five cases) and the parity rows `appearance-default` and `shell-default`.

## Suggested articles

- [Design-reference parity](../design/design-parity.md)
- font-customization (not implemented yet; see `docs/features/inventory.json`)
- per-element-appearance-editor (not implemented yet; see `docs/features/inventory.json`)
