# Material Design system

Every colour, radius, duration, and row height in the interface comes from `src/gui/material/MaterialTheme.h`. Widgets ask for a semantic role; they never hard-code a value. That is what makes theme, seed, and density switchable at runtime.

## Colour roles

Neutral and status roles flip with the light/dark family:

| Role | Light | Dark |
| --- | --- | --- |
| `Surface` | `#f8f9ff` | `#111417` |
| `SurfaceContainerLowest` | `#ffffff` | `#0c0f12` |
| `SurfaceContainerLow` | `#f2f3f9` | `#191c20` |
| `SurfaceContainer` | `#eceef4` | `#1d2024` |
| `SurfaceContainerHigh` | `#e6e8ee` | `#282a2e` |
| `SurfaceContainerHighest` | `#e0e2e8` | `#333539` |
| `OnSurface` | `#191c20` | `#e1e2e7` |
| `OnSurfaceVariant` | `#41474d` | `#c1c7ce` |
| `Outline` | `#71787e` | `#8b9198` |
| `OutlineVariant` | `#c1c7ce` | `#41474d` |
| `InverseSurface` | `#2e3135` | `#e1e2e7` |
| `InverseOnSurface` | `#eff1f7` | `#2e3135` |
| `Error` | `#ba1a1a` | `#ffb4ab` |
| `ErrorContainer` | `#ffdad6` | `#93000a` |
| `Green` (healthy) | `#1b7f37` | `#57ab5a` |
| `GreenContainer` | `#d2f2d8` | `#113a1b` |
| `Amber` (weak / reused) | `#9a6700` | `#d8a739` |
| `AmberContainer` | `#ffe9b8` | `#3d2e00` |

> The status families are **not** constant across themes. A light-mode green on a dark surface reads as a glowing sticker; each family has its own dark value.

## Seeds

The seed supplies the primary and container roles in both families:

| Seed | Swatch | Light primary | Dark primary |
| --- | --- | --- | --- |
| KeePassXC blue | `#006493` | `#006493` | `#8dcdff` |
| Baseline purple | `#6750a4` | `#6750a4` | `#cfbcff` |
| Vault green | `#1b7f37` | `#146c2e` | `#8bd996` |
| Signal amber | `#9a6700` | `#8a5200` | `#ffb95c` |

## Density

`theme()->rowHeight()` drives every list, tree, and table:

| Density | Row height |
| --- | --- |
| Compact | 40 px |
| Comfortable | 52 px |
| Spacious | 64 px |

## Shape

| Radius | Used by |
| --- | --- |
| `Full` (999) | Buttons, chips, search bars, group rows, nav destinations |
| `ExtraLarge` (28) | Cards, sheets, dialogs |
| `Rail` (18) | Rail tiles, FAB, field cards |
| `Row` (16) | List rows, note cards |
| `Large` (14) | Inputs, tiles |
| `Medium` (12) | Tabs (top corners only) |
| `Small` (8) | Small chips, pills |

## Motion

| Duration | Curve | Used by |
| --- | --- | --- |
| 140 ms | ease | Scrim fade in/out |
| 180 ms | `cubic-bezier(.2,0,0,1)` | Hover and background transitions |
| 240 ms | `cubic-bezier(.2,0,0,1)` | Sheet entry (translate 18 px, scale .98) |

## Layout constants

```
rail 88   app bar 64   tab strip 48   group pane 250
detail pane 392   sheet nav 266   search bar 52   FAB 56
```

## Source layout

```
src/gui/material/
  MaterialTheme.*        colour roles, seeds, density, type scale, QPalette
  MaterialStyleSheet.*   one generated stylesheet for stock Qt widgets
  MaterialStyle.*        QProxyStyle for what a stylesheet cannot express
  MaterialIcons.*        Material Symbols names resolved to bundled SVGs
  Material<Component>.*  rail, app bar, tab strip, buttons, chips, switches,
                         cards, search bars, snackbars, overlays, delegates,
                         screens, spec sheets
```

## Removed

The stock styling was deleted rather than themed over:

- `BaseStyle` (4 860 lines), `LightStyle`, `DarkStyle`, `phantomcolor`
- `basestyle.qss`, `classicstyle.qss`, `lightstyle.qss`, `darkstyle.qss`
- `styles.qrc` and the `classic` theme option
- Per-widget `setStyleSheet()` calls across `src/gui`

## Adding a surface

Compose the existing components and ask the theme for roles. If you find yourself writing a stylesheet, the component library is missing something — add it there instead.
