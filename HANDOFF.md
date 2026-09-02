# Handoff — Windows-native Material rewrite and Squirrel distribution

Last verified: 2026-09-02 on the `codex/parity-capture` lane; `main` was fast-forwarded to it at the end of the pass (see the final section). Every claim names the commit it was measured at.

## What this pass established

### Design parity: nine rows, evidence complete, no open audit defect

- `design/parity/inventory.json` names the nine checked-in references exactly once. Every row carries a reference capture, a built capture from the application on an off-screen desktop, a labelled comparison, `diff.json`, a hand-reviewed Material Design 3 audit, the source commit and the capture tool. `node design/parity/check-parity.mjs --require-evidence` is green and `node design/parity/test-parity-guard.mjs` proves every structural and evidence probe red then green.
- Every audit under `design/parity/audits/` now lists **zero open defects**. Remaining differences are recorded deviations with an approval: the current Material 3 slider anatomy (thicker track, handle bar, stop dot) and the searchable Material selects for font family and weight, both because the owner asked for every control to be pure Material Design 3 and every dropdown to be a list box with a search bar; vault group sub-labels; the Reports header.
- Mismatch at `693367d1`: settings 5.93 %, sheet-editor 4.82 %, changelog 8.24 %, vault 8.42 %, history 9.16 %, appearance 9.82 %, shell 10.4 %, reports 11.23 %, regex-builder 20.8 %. Earlier baseline (`8022ef16`): regex-builder 32.5 %, appearance 11.2 %.
- Routes: `KeePassXC.exe --capture-route "kpxc://capture/<screen>?state=<state>&width=W&height=H&theme=light&lang=bilingual[&target=page][&probe=1]" --capture-receipt <path> --keyfile design/parity/fixtures/parity.keyx design/parity/fixtures/parity.kdbx`. Screens: `shell`, `vault`, `welcome`, `reports`, `history`, `changelog`, `settings`, `appearance`, `editor`/`sheet-editor`, `database`, `tools`, `help`, `regex-builder`. States: `default`; `appearance-editor` (opens the per-element editor on the interface font select); `personal-vocabulary` (Settings › Interface with the upload row in view). Spec-sheet rows are named `specRow_<symbol>`; a `probe=1` receipt lists every visible widget's name, rect and text.
- Harness: `design/parity/capture.mjs --app <exe> [--rows …] [--side built|reference|both]`, `compare.mjs`, `promote-evidence.mjs` (after editing an audit), `check-parity.mjs --require-evidence`, `test-parity-guard.mjs`.

### Clipping matrix

`node design/parity/clipping-matrix.mjs --app <exe> [--quick] [--screens] [--widths] [--languages] [--themes] [--scales] [--desktop] [--scratch]` writes `design/parity/evidence/clipping/matrix.json`; named records are kept beside it:

| Record | Tuples | Findings | Commit |
| --- | --- | --- | --- |
| `matrix-scales.json` (1.25 / 1.5 / 2.0) | 30 | 0 | `69c8c914` |
| `matrix-lang.json` (English / Cantonese / bilingual × light / dark, expanded) | 60 | 0 | `c165ec83` |
| `matrix-widths-before.json` (minimum … extra-large, bilingual, light) | 50 | 20 | `4577d73f` |
| `matrix-widths.json` (same tuples after the repairs) | 50 | see the final section | `693367d1` |

The twenty findings had three causes, all repaired in `d14e6b79` and `693367d1`: the spec-sheet sidebar squeezed its overline in a 640 px window (it scrolls now); the vault search placeholder overflowed at the medium width (every search bar elides its own placeholder and keeps the full text as the accessible description); nine Security & behaviour sub-labels were cut to one line in the three-column Appearance layout (a `WrapLabel` raises its minimum height to its height-for-width; the minimum is only ever raised, because lowering it made the scroll area oscillate and the capture route never settled).

The probe flags a wrapping label whose `heightForWidth(width())` exceeds its height, measured at the widget's own width; a squeezed control is one narrower or shorter than its minimum hint; hidden widgets are skipped.

### What landed on the lane (all pushed)

- Regex builder: workbench with Matches, Explain, Replace, Export, Cheat sheet and Dialect tabs, dialect switch, searchable pattern library, reorderable token blocks (`2c67f3e7`, `69c8c914`; `testmaterialregexbuilder`).
- Reports: finding rows with URL and Fix actions, one-column cards, Material header with Markdown export and bulk export (`fcee1b07`, `3051d2b4`).
- Frameless Material title bar with its own window controls (`WM_NCCALCSIZE` / `WM_NCHITTEST`, `76cbb796`).
- Vault: app bar content, health chips, group filter, recursive root listing, tag chips in entry rows, detail card with hero, health chip, field containers, attachment filter and footer actions (`c130ec80`, `981efe48`, `07c23549`, `a961def2`).
- History: badges, hash, append-only banner, detail card with diff lines, CREATE badge (`5c1beee2`, `3b6cb1e9`, `761bf198`).
- Changelog: category chips, date pills, rich rows (`2afa4bc7`).
- Controls: `Material::Select` (list box + search bar + regex builder) replaces every `QComboBox`; `Material::Slider` replaces every `QSlider`; `Material::DateField` with a calendar picker replaces every `QDateEdit` (`a16d064f`; `testmaterialselect`, `testmaterialslider`, `testmaterialdatefield`).
- Appearance editor: per-element editor on Shift+right-click or Ctrl+Shift+E with typography, colour, shape and preset tabs; infinite colour picker with notation translator, contrast readout and rainbow; presets export/import (`8c7bcc45`, `98d83824`; `testmaterialcolorpicker`, `testmaterialappearanceeditor`).
- Settings: Material switches on every row, captioned override sliders, three-column Appearance layout with the first card named Theme, scrolling spec-sheet sidebar, wrapping sub-labels (`41d900eb`, `30568449`, `d14e6b79`, `693367d1`).
- Personal vocabulary consumed at the translation boundary: `src/core/PersonalVocabulary.{h,cpp}` validates the bounded schema-1 contract (canonical `entries` member; `replacements` accepted and normalised), applies whole-word longest-first replacements, and a translator installed after the language translators wraps them so every language mode takes the user's terms; upload and clear reload the cache and re-translate open windows; a corrupt cache fails closed (`4577d73f`; `testpersonalvocabulary`).
- Screen base declares a 320 px minimum and re-wraps its header; probe rule for wrapped text (`3b6cb1e9`).
- Personal vocabulary proved on the built artifact through the real Settings row and the native file dialog on a hidden desktop (`design/parity/evidence/personal-vocabulary/`, drivers `design/parity/vocab-proof.mjs` and `design/parity/vocab-count.mjs`): upload, restart persistence with the wording applied, replace, invalid refusal with the cache kept, clear, original wording after restart. The canonical private file was exercised the same way with counts only retained (114 entries loaded and persisted, replace, invalid refusal, clear and restoration all recorded; on the Appearance page 1 of 60 plain labels changed once the cache was present and 0 before it; no capture, receipt or profile kept). Its three notifications are voiced in English and Cantonese through the catalogue. First inventory row green (`personal-vocabulary-upload`, 1/172).

### Fail-closed feature inventory

`docs/features/inventory.json` names 86 canonical features on two surfaces; `node scripts/check-feature-inventory.mjs` is red on any row that is not `implemented` with every link resolving, and `scripts/test-feature-inventory-guard.mjs` proves single-item removals red. Verdict at `693367d1`: **0 of 172 rows green** (honest red). Partial rows now carry implementation anchors, tests and captures for: dropdown search, per-element appearance editor, infinite colour picker, rainbow colour, personal vocabulary upload, regex builder, tab strip, title bar. Rows marked `missing` are not built: School mode, narrator and voice pickers, scheduled and external settings sources, toy locks, Support Tickets, unlock ladder, authenticator and QR pairing, tab docking/groups/searches/bulk-close, super confirmation, bulk actions, export-everything and archives, offline docs browser, the documentation site's feature set, ADHD modes, app-logo customization, file converter, Ollama manager, Status Hub row, browser-extension download dialogs, and the release-evidence rows that need captures.

### Release evidence

- `scripts/count-lines.mjs` prints the line-count table the release publishes; `scripts/select-dim-sum.mjs` picks the next unused dish with a published photo; the publish job writes timing, code name, photo asset and the table into every release (verified on `v2.8.9401` at `60f62c16`).
- The release workflow triggers on every push to every branch, so lane pushes publish releases too; the closeout release is the one produced by the final `main` push.

## Verification inventory

| Check | Scope | Last result |
| --- | --- | --- |
| `ctest -R 'testmaterial|testdimsum|testwelcomeprovenance|testrepaircontracts|testupdatecheck|testsquirrellifecycle|testpasskeys|testdesignparityguard|testfeatureinventoryguard|testentrymodel|testpersonalvocabulary'` | lane build | see the final section |
| `node design/parity/check-parity.mjs --require-evidence` | 9 rows | PASS at `693367d1` |
| `node design/parity/test-parity-guard.mjs` | negative probes | GREEN at `693367d1` |
| `node scripts/check-feature-inventory.mjs` | inventory | 0/172 green (honest red) |
| `node scripts/test-feature-inventory-guard.mjs` | negative probes | GREEN, baseline unchanged |
| Clipping records | see the table above | scales 0, languages 0, widths after repair: final section |

## Known dead ends (do not repeat)

- A `WrapLabel` that lowers its minimum height on a narrower pass makes a settings scroll area toggle its bar and the layout bounce forever; the capture route then times out at 45 s. Only ever raise the minimum.
- A `Qt::AlignTop` grid item is sized by its hint and drops height-for-width; a stretch row after the last card keeps cards top-packed without it.
- Two KeePassXC instances launched with different profiles still hand off to each other on launch; a driven proof and a clipping matrix must not share one executable at the same time. A hung capture process may refuse `Stop-Process`; `taskkill /F /PID` ends it.
- `clipping-matrix.mjs --help` is not an option: it launches the matrix with the next argument as the application.
- A commented-out or renamed wiring line still satisfies a substring guard; anchor guards to the start of a line and watch them fail.
- The Settings validator once demanded a `replacements` member while the canonical vocabulary file carries `entries`; the contract is `entries`, with the old name accepted.

## Final state of this pass

Filled in by the closeout commit: final `main` SHA, lane suite verdict, widths record after repair, release tag and CI verdicts, installer proof, Mat Day result.
