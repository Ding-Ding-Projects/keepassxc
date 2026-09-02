# Clipping matrix

Feature id: `clipping-matrix` · Category: Design and appearance

## Behaviour

`design/parity/clipping-matrix.mjs` drives every shell destination across the width classes (minimum 480, compact 600, medium 840, expanded 1200, large 1600, extra-large 1920), the three language modes, both themes and the 100, 125, 150 and 200 percent display scales on an off-screen desktop. Each tuple keeps a capture and the application's own widget probe (`&probe=1` on the capture route): geometry in window coordinates, size hint versus actual size, whether a plain-text label or button is wider than its box, scroll ranges, and whether a widget outside a scroll area lies partly off screen.

## Configuration

`--quick` runs a two-width, bilingual, light, 100 percent subset. `--capture-scale` on the application sets `QT_SCALE_FACTOR` before Qt initialises.

## Failure modes

A finding is a widget whose plain text overflows (sideways for a one-line text; downwards for a wrapping label whose height-for-width at its actual width exceeds the height it was given), a leaf control squeezed below its own minimum size hint, or a non-scrollable widget partly off screen. Findings are the work list; the matrix does not fail a build by itself.

## Security considerations

Same isolation as parity captures: throwaway configuration, key-file fixture, off-screen desktop.

## Verification

`design/parity/evidence/clipping/matrix.json` records every tuple with its capture hash and findings.

## Suggested articles

- [Design-reference parity](../design/design-parity.md)
- responsive-sizing (not implemented yet; see `docs/features/inventory.json`)
- accessibility (not implemented yet; see `docs/features/inventory.json`)
