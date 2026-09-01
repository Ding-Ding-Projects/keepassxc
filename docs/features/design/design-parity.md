# Design-reference parity

Feature id: `design-parity` · Category: Design and appearance

## Behaviour

The nine checked-in references under `design/*.dc.html` are the visual source of truth. `design/parity/inventory.json` names every reference exactly once with its capture tuple (screen, state, theme, viewport, scale, locale). The application exposes `--capture-route kpxc://capture/<screen>?...` which opens the fixture database, forces theme and language, sizes the shell or the destination page to the tuple, navigates, and writes a readiness receipt. `design/parity/capture.mjs` captures the reference (Edge headless against vendored, hash-pinned assets) and the built window (by handle on an off-screen desktop), `compare.mjs` writes the labelled side-by-side comparison and `diff.json`, `promote-evidence.mjs` writes fresh hashes into the inventory, and `audits/<row>.json` holds the hand-written Material Design 3 primitive review with open defects.

## Configuration

Rows are hand-written in `design/parity/inventory.json`. The fixture database is regenerated with `design/parity/fixtures/build-fixture.ps1`; vendored assets with `design/parity/vendor-reference-assets.mjs`.

## Failure modes

A missing capture, a stale hash, a pending audit or a pending commit makes `node design/parity/check-parity.mjs --require-evidence` red. Captures taken before a tall window painted contain black rows; the harness recaptures up to six times and then fails.

## Security considerations

The fixture database contains placeholder data from the design's demo vault and is protected by a generated key file; nothing in it is a real credential. The harness passes an isolated local configuration so a capture never reads the user's real last-opened databases.

## Verification

`node design/parity/check-parity.mjs --require-evidence` and `node design/parity/test-parity-guard.mjs` (ten evidence probes red then green). Comparison images live under `design/parity/evidence/<row>/comparison.png`.

## Suggested articles

- [Material Design 3 appearance](../design/material-3-appearance.md)
- [Clipping matrix](../design/clipping-matrix.md)
- [One-click build and installer scripts](../delivery/build-scripts.md)
