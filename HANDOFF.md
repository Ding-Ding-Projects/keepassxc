# Handoff — closing the gap between the Material UI and the design

Branch `claude/ui-design-verification-ygrhj7`, [PR #6](https://github.com/Ding-Ding-Projects/keepassxc/pull/6), 29 commits,
192 files, +10 928 / −1 813.

**Builds and links on MSVC. 43 of 43 tests pass. The MSI still does not build**, for a reason that
predates this branch and has not yet been read. That is the only open item, and §4 is about it.

This supersedes the previous handoff, which described the Material shell landing. Its §1 ("the vault
destination is still the stock three-pane widget") is done; several of its other claims were wrong and
are corrected below.

---

## 1. Read this first

**This fork is Windows only.** `src/gui/osutils/nixutils` is gone, so the tree does not configure on
Linux at all. That is by design, not breakage.

**KeePassXC is invisible to every screen-capture API unless you pass `--allow-screencapture`.**
`WinUtils.cpp` calls `SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE)` on every top-level
window. The window is visible on the physical display and reports `IsWindowVisible = true`, but
`PrintWindow`, `BitBlt` and `CopyFromScreen` all return black. This is upstream behaviour and it has
now cost two sessions hours of misdiagnosis. Always launch with:

```
KeePassXC.exe --config %TEMP%\kpxc.ini --allow-screencapture
```

**Nothing in this branch has been run.** See §3.

---

## 2. What this session did

The design is `design.zip` → `KeePassXC Material.dc.html`, a 1440×920 mockup in which every element
carries exact inline CSS. It was extracted into a diffable form (per-section templates, the token
tables, and `SHEETS` / `PALETTE` as JSON) and compared element by element against `src/gui/material/`.

### The four structural gaps

| | design | before |
| --- | --- | --- |
| Rail destinations | 10 | 5 |
| Spec sheets | 5 sheets · 35 pages · 397 rows | 1 sheet · 6 pages · 155 rows |
| Design icons that resolve | 202 | 113 |
| Settings destination | the Material hub | the **stock** `ApplicationSettingsWidget` |

**Three finished surfaces were dead code** — compiled on every build, never constructed:

| | lines |
| --- | --- |
| `Material::SettingsHub` | 1 706 |
| `Material::SettingsScreen` | 999 |
| `Material::GeneratorSheet` | 520 |

The previous handoff prescribes the objective check for exactly this — grep for the type outside its
own directory — and it had not been re-run after the shell landed. It is worth re-running whenever a
component is "finished":

```
for f in src/gui/material/Material*.h; do
  c=${f##*/Material}; c=${c%.h}
  # Not every header declares a class of its own name: MaterialElevation and
  # MaterialStyleSheet declare free functions (paintSurface, buildStyleSheet) and
  # MaterialWidgets is an umbrella include. Those three are expected here and are
  # not dead; check any OTHER name it prints.
  case $c in Elevation|StyleSheet|Widgets) continue;; esac
  n=$(grep -rl "\b$c\b" --include=*.cpp src/ | grep -v "material/Material$c" | wc -l)
  [ "$n" = 0 ] && echo "UNREFERENCED: $c"
done
```

It should print nothing. When it printed `SettingsHub`, `SettingsScreen` and `GeneratorSheet`, that was
3 225 lines of finished UI that had never been constructed.

### What changed

- **Ten destinations.** Entry, Database, Tools, Appearance and Help now exist. Appearance is
  `SettingsScreen`; Settings is `SettingsHub`, which adopts the stock widget as its classic editor so
  no option became unreachable. Rail sublabels are live counts, not empty strings.
- **The four reference sheets are generated, not transcribed.** `utils/generate_sheet_catalogue.mjs`
  emits `MaterialSheetCatalogue.{h,cpp}` from `utils/design/sheets.json` — 33 pages, 71 sections,
  389 rows of the design's own wording. Re-run it to diff the transcription rather than trust it.
- **Auto-Type, Password Generator defaults and Shortcuts** are real settings pages bound to real
  `Config` keys. Rows were *moved* off General and Security, not copied, so nothing is bound twice.
  Shortcuts reads `ActionCollection`, so a rebound key shows up and a new action cannot go missing.
- **110 icons drawn.** `Icons::symbol()` returns an empty `QIcon` for an unknown name, so 89 of the
  design's 202 symbols were blank space — including six the rail itself asks for.
- **The generator sheet, command palette, notification centre and snackbar** now match the design, and
  the regex builder returns its pattern to the field that opened it rather than always the vault.

### The defects worth knowing about

Adversarial review of the finished work found 18 problems in code that had already passed review. The
two that mattered:

- **History held a strong `QSharedPointer<Database>`.** Locking neither cleared the surface nor
  released the database, so the decrypted database stayed reachable for the life of the window. It
  holds a `QWeakPointer` now and follows the root group's destruction.
- **A restore could apply a revision the user never selected.** Rows were identified by a *position*
  in `Entry::historyItems()`, but `truncateHistory()` drops from the oldest end, so every surviving
  index shifts. A row clicked after a truncation resolved to different data than it displayed. Rows
  hold a `QPointer` to the revision itself now, so a stale row declines instead.

Also fixed: a restore was invisible in the vault (`copyDataFrom()` raises nothing the model listens
for), the preserved last-access time was overwritten one line later by `addHistoryItem()`, and Reports
walked the live entry tree from a worker thread while a nested event loop ran on the GUI thread.

---

## 3. How this was verified, and what that is worth

**A full build is impossible in this environment** — Windows-only fork, no `nixutils`. Instead: Qt 6.4
installed, `uic` and `config-keepassx.h` generated, then `g++ -fsyntax-only -std=c++17` against the
real headers. That catches everything short of link errors.

**Two commits went out broken anyway, and the reason matters.** The check ran against the *working
tree* while work was landing concurrently, so a `.cpp` could be committed ahead of its `.h`;
`git add -A` snapshotted that instant. `edc96650` and `593c3ac` do not compile. Every commit after each
of them does.

The fix is `scratchpad/verify-and-push.sh`: it resets a detached worktree to `HEAD`, checks *that*, and
**refuses to push** on any failure. Use it. Do not push otherwise.

A green syntax check is not a build. It was wrong about this branch twice.

---

## 4. The MSI: six dead theories and a working diagnostic

`Test (Windows x64)` compiles, links, tests, produces a 63.4 MiB ZIP, and then fails:

```
CPack Error: Problem running WiX. Please check '.../wix.log' for errors.
CPack Error: Fatal WiX Generator Error
```

**This is not from the Material work.** `develop` at `0dd3d702` fails identically, with the same error
and the same ZIP size.

### Do not re-try these

| # | theory | why it is dead |
| --- | --- | --- |
| 1 | the `-snapshot` version string | `CPACK_PACKAGE_VERSION` uses `KEEPASSXC_VERSION_CLEAN`, which strips it |
| 2 | `qt.conf` installed twice | it is not |
| 3 | `KPXC_FEATURE_DOCS=OFF` | that skips `add_subdirectory(docs)` entirely; no dangling rules |
| 4 | `cpack --config … -B artifacts` | changed to `cd build && cpack`; no effect. Kept, as it matches the workflow that last shipped an MSI |
| 5 | missing `WixUIExtension.dll` | adding it **caused** `LGHT0091 Duplicate symbol`: CPack already passes it whenever `CPACK_WIX_UI_REF` is set |
| 6 | the duplicate from #5 | reverted in `f6f9d8c4` |

### Why five rounds were wasted

`wix.log` could never be read. `actions/upload-artifact` dumps several hundred lines of runner
environment after every upload, and the job log is reachable only by tail, so the real error was always
out of reach — and every round reasoned from the *configuration* instead of the *error*. That is
guessing with extra steps. The visibility should have been fixed after the second attempt, not the
fifth.

### What to do now

The final step of the job is **"Print why this job failed, last"**. It prints two blocks in the last
~30 lines:

```
===== FAILED TESTS ... END FAILED TESTS =====   LastTestsFailed.log + FAIL!/QFATAL/ASSERT
===== WIX ERRORS  ... END WIX ERRORS  =====     LGHT####/CNDL#### codes, then 40 lines of wix.log
```

Fetch the log with a **small** tail (45–70 lines). Then fix exactly what the code names:

- `LGHT0091` duplicate symbol → something defined or loaded twice
- `LGHT0094` unresolved symbol → a missing extension or source
- `LGHT0103` file not found → a `.wxs` references a path not on disk
- `LGHT0204` / `LGHT0217` ICE validation → may need that ICE suppressed
- `CNDL####` → a compile problem, which would be new; candle currently succeeds

`no wix.log found` means packaging never ran — read the FAILED TESTS block instead.

The diagnostic has already earned its keep: it caught theory #5 as self-inflicted within one run.

---

## 5. Still open

- **The MSI.** §4.
- **`testdatabase` is flaky.** It failed and then passed on *identical binaries* (`ef8f3707..3d17b7e1`
  differ by workflow text alone). `ctest --repeat until-pass:2` bounds it: retries are printed, and a
  test failing twice still fails the job. Note `testmerge`, which the previous handoff recorded as a
  pre-existing Windows failure, now passes.
- **`Analyze (cpp)` — fixed, pending its first Windows run.** CodeQL built on `ubuntu-latest` and died
  at ~28% every time, because `src/gui/osutils/OSUtils.h` defines `osUtils` under `Q_OS_WIN` and
  `Q_OS_MACOS` and has **no `#else`** — on Linux the macro does not exist, so every unit that touches
  it fails. No apt package would have fixed that. `codeql.yml` now runs the same MSVC + Ninja + vcpkg
  recipe `material-release.yml` ships with, sharing its vcpkg cache key. Configure runs *before*
  `codeql init` so the tracer sees `src/` and not the vcpkg ports. Unverified: it has not completed a
  run yet, and the first one pays for a cold cache.
- **Branch archive.** `.github/workflows/archive-branches.yml` bundles all 25 branches with full
  history, verifies the archive restores every tip, and publishes it as a release. It is
  `workflow_dispatch` only and **has never been run**. Nothing may be deleted until it has.
- **`main` / branch cleanup** was requested and then parked. Note there is no `main`; the default is
  `develop`. Of the 24 other branches, 14 share **no common ancestor** with this fork — merging one
  means resolving 366–686 file conflicts.
- **Two commits do not compile** (`edc96650`, `593c3ac`). Harmless unless you bisect; can be folded
  into their parents if the history should be clean.

---

## 6. Traps

1. **Every push cancels the pending Windows run.** The workflow serialises per branch and only one run
   may wait. Pushing repeatedly means no run ever completes — which is why the `wix.log` diagnostic sat
   unexecuted through five pushes. Push, then wait.
2. **`git bundle verify` will tell you an archive is complete when it is not.** This working clone was
   *shallow*; bundling it dropped 11 of 25 branches and still reported "records a complete history" —
   true only against its own shallow boundary. Check `git rev-parse --is-shallow-repository` first.
3. **A bundle of `refs/remotes/*` clones as an empty repository.** `clone` looks under `refs/heads/`.
   It builds, verifies, lists every branch, and restores nothing. Bundle from a bare mirror.
4. **`-fsyntax-only` on the working tree proves nothing about the commit.** §3.
5. **Findings are not facts.** A 235-finding audit of this UI produced 104 confirmed and 131 refuted
   after adversarial verification. Have claims refuted before acting on them; several "gaps" were
   features that already existed, and one attributed the extended FAB's metrics to `FilledButton`.
