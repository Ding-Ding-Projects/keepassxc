# Handoff — Windows-native Material rewrite and Squirrel distribution

Last verified: 2026-09-01 against `main` at `e09d6e285993` plus the `codex/parity-capture` lane commits described below. Every claim names the commit it was measured at.

## What this pass established

### Design parity is evidence now, not a status word

- `design/parity/inventory.json` still names the nine checked-in references exactly once, but every row now carries a real reference capture, a real built capture from the application on an off-screen desktop, a labelled comparison, a `diff.json`, a hand-reviewed Material Design 3 audit, the source commit and the capture tool provenance. `node design/parity/check-parity.mjs --require-evidence` is green; `node design/parity/test-parity-guard.mjs` proves ten evidence probes red then green.
- Routes: `KeePassXC.exe --capture-route kpxc://capture/<screen>?state=default&width=W&height=H&theme=light&lang=bilingual[&target=page][&probe=1] --capture-receipt <path> --keyfile design/parity/fixtures/parity.keyx design/parity/fixtures/parity.kdbx`. Screens: `shell`, `vault`, `welcome` (no database), `reports`, `history`, `changelog`, `settings`, `appearance`, `editor`/`sheet-editor`, `database`, `tools`, `help`, `regex-builder`. `--capture-scale <factor>` sets the display scale before Qt starts.
- Harness: `node design/parity/capture.mjs --app <exe>` (both sides), `compare.mjs`, `promote-evidence.mjs`, `clipping-matrix.mjs [--quick]`. Reference assets (React, ReactDOM, Babel, Roboto, Roboto Mono, Noto Sans HK, Material Symbols) are vendored and hash-pinned under `design/lib/vendor/`; the reference server rewrites the public URLs to them at serve time.
- Mismatch at `8022ef16` (later rows recaptured on the lane): settings 5.6 %, sheet-editor 4.8 %, history 7.9 %, changelog 8.3 %, vault 8.6 %, shell 8.6 %, reports 10.4 %, appearance 11.2 %, regex-builder 32.5 %. The audits under `design/parity/audits/` list the open defects per row; `status: complete` there means the review happened, not that the row conforms.

### Open parity defects, in priority order

1. Regex builder: compact dialog instead of the design's workbench (Matches, Explain, Replace, Export, Cheat sheet tabs, dialect switch, pattern library, token blocks).
2. Reports: summary tile set and finding-row anatomy (entry rows with URL and Fix action) differ.
3. Vault: root group selection lists no entries; the reference lists them recursively. Health chips and the group filter field are missing; the detail pane lacks the card anatomy.
4. Shell: OS caption bar instead of a frameless Material title bar (the legacy status bar is already gone).
5. Settings: 8 pages versus 11 in the reference; History timeline badges; Changelog category chips and code names; Appearance section layout.

### Repairs that landed

- Legacy status bar hidden (`e09d6e28`); both whole-shell rows match the reference height.
- Shared screen header wraps: trailing actions drop to a second row and the search bar to a third full-width row when a row would overflow (lane, `3e1f8e2b`). Before this the Reports search bar sat off the right edge at 1200 px and the headline was squeezed to 106 px at 600 px.
- Segmented control honours a pinned chip height; the layout probe treats height-for-width buttons as wrapping (lane).
- Welcome screen shows version, revision and the exact updated-at time of that revision from the built commit's committer date, or says the date is unavailable (`testwelcomeprovenance`, lane).
- Dim sum surprise: ten percent draw, no opt-out, retired key ignored (`testdimsum`, lane).
- Command palette on `Ctrl+Shift+F` (lane).
- `build-windows.ps1` exports the MSVC environment itself; CodeQL checkout fetches tags so the changelog provenance guard can configure (lane, `cd7d2de6`).

### Fail-closed feature inventory

`docs/features/inventory.json` names 86 canonical features on two surfaces (`app`, `site`); `node scripts/check-feature-inventory.mjs` is red on any row that is not `implemented` with every link resolving, and `scripts/test-feature-inventory-guard.mjs` proves seven single-item removals red. Current verdict: **0 of 172 rows green**. Rows marked `partial` name what exists and what is missing; rows marked `missing` are not built at all: School mode, narrator and voice pickers, scheduled and external settings sources, per-element appearance editors and toy locks, Support Tickets, unlock ladder, authenticator and QR pairing, tab docking/groups/searches/bulk-close, super confirmation, bulk actions, export-everything and archives, offline docs browser, the documentation site, social preview, frameless title bar, ADHD modes, app-logo customization, file converter, Ollama manager, Status Hub row, browser-extension download dialogs, and the release-evidence rows that need captures.

Feature articles live under `docs/features/<category>/` with a category index each; 25 articles exist.

### Release evidence

- `scripts/count-lines.mjs`: project total 234,081 lines at `e09d6e28` (agents 39.1 %, people 60.9 %), self-consistent; excluded rows visible.
- `scripts/select-dim-sum.mjs`: next unused dish from the public catalog with a published photo; the first pick is `hk-dish-0001` Classic Har Gow.
- The publish job (lane, `cd7d2de6`) checks out the released commit, runs both scripts, measures workflow timing from the first job's real start, and writes timing, code name, photo asset and the line-count table into the release. This has not yet run on `main`; the first release after the lane merges is the first with these fields.

## Verification inventory

| Check | Scope | Last result |
| --- | --- | --- |
| `ctest -R 'testmaterial|testdimsum|testwelcomeprovenance|testrepaircontracts|testupdatecheck|testsquirrellifecycle|testpasskeys|testdesignparityguard|testfeatureinventoryguard'` | lane build `build-lane-parity` | 18/18 passed at lane tip (dim sum test rerun green after its rewrite) |
| `node design/parity/check-parity.mjs --require-evidence` | parity inventory | PASS, 9 rows |
| `node design/parity/test-parity-guard.mjs` | negative probes | 10 structural + 8 evidence probes red then green |
| `node scripts/check-feature-inventory.mjs` | feature inventory | 0/172 green (honest red) |
| `node scripts/test-feature-inventory-guard.mjs` | negative probes | 7 probes red, baseline unchanged |
| `node design/parity/clipping-matrix.mjs --quick` | 20 tuples | 13 findings before the header wrap; recapture pending at lane tip |
| Remote CI for `e09d6e28` | Material Squirrel Build and Release, CodeQL | in progress at handoff time; no verdict claimed |

## Known dead ends (do not repeat)

- Git Bash rewrites `/c` and `/s`; run `build.bat /s` from PowerShell.
- A build directory under a path with spaces breaks the readline pkg-config include; build under `%LOCALAPPDATA%\KeePassXCMaterial\build-lane-<jer>`.
- The local config option is `--localconfig`; the wrong spelling shows a native modal and the app never reaches its window. Always pass an existing isolated local config or captures read the real user's last databases.
- `PrintWindow` returns black rows on a tall window captured too early; the harness recaptures up to six times.
- `-DOVERRIDE_VERSION=2.8.x` unquoted in PowerShell can reach the cache as `2`; quote it.

## Next safe actions

1. Merge `codex/parity-capture` into `main` once its recapture evidence is promoted, then read the first release with timing, code name and line count back from GitHub.
2. Repair the parity defects in the order above, recapturing after each.
3. Work the inventory from `partial` rows to `implemented` (each needs a focused test, an interaction record and a capture), then the `missing` rows in the order the roadmap lists.
4. Run the full clipping matrix (six widths, three languages, two themes, four scales) and repair every finding.
5. Prove the installer in an isolated account: silent install, launch, update, defer, restart, uninstall.
