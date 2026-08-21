# handoff/

The Qt drop-in package for the Material 3 Expressive rewrite.

**Read `HANDOFF.md` first** — §0 has the exact copy commands and the three
things that must happen before anything compiles.

| File | What it answers |
| --- | --- |
| `HANDOFF.md` | How to land it, what changed, what is not proven |
| `MANIFEST.md` | Which files to create, which to modify, and why |
| `ACCEPTANCE.md` | Every shared UI requirement, where to see it, and what must be true in Qt |
| `TOKENS.md` | The `MaterialTheme.cpp` diff and all 29 new Config keys |
| `sheets-additions.json` | New pages, in `utils/design/sheets.json` format |
| `src/gui/material/` | Ten new classes: documented headers, `.cpp` skeletons |

These files were written into the design project, **not** into the KeePassXC
working copy — the repository was mounted read-only. No branch, no commit, no
change to `develop`. Landing them is a deliberate act; see §0.

The headers are complete and are meant to be read. The `.cpp` files are
skeletons: real signatures, real constants, real error paths, with `TODO`
markers where the body is the implementer's work. Where a body was short enough
to be unambiguous — `MaterialBreakpoints`, `riskReport()`, `runBounded()`,
`ExternalEditor::open()` — it is written out rather than described.

Nothing here has been compiled.
