# Handoff — closing the gap between the Material UI and the design

Branch `claude/ui-design-verification-ygrhj7`, [PR #6](https://github.com/Ding-Ding-Projects/keepassxc/pull/6), 29 commits,
192 files, +10 928 / −1 813.

**Builds and links on MSVC. 43 of 43 tests pass.** The MSI failure that predates this branch has been
read and fixed — WiX Start Menu shortcuts pointing at documentation the build was configured not to
produce. §4. The fix has not completed a run yet.

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

## 4. The MSI: read, and fixed

`Test (Windows x64)` compiled, linked, tested, produced a 63.4 MiB ZIP, and then failed with
`CPack Error: Fatal WiX Generator Error`. **It was never from the Material work** — `develop` at
`0dd3d702` failed identically.

Six rounds guessed at it from the *configuration*. The seventh read the error:

```
main.wxs(75) : error LGHT0204 : ICE67: The shortcut 'GettingStartedShortcut' is a
    non-advertised shortcut with a file target, but the target file does not exist.
main.wxs(79) : error LGHT0204 : ICE67: The shortcut 'UserGuideShortcut' ...
main.wxs(75) : error LGHT0204 : ICE69: 'CM_FP_share.docs.KeePassXC_GettingStarted.html'
    references invalid file.
main.wxs(79) : error LGHT0204 : ICE69: 'CM_FP_share.docs.KeePassXC_UserGuide.html' ...
```

**The cause.** `share/windows/wix-template.xml` declared two Start Menu shortcuts targeting
`[#CM_FP_share.docs.KeePassXC_GettingStarted.html]` and `…_UserGuide.html`. Those file ids exist only
because `docs/CMakeLists.txt` installs the pages into `${DATA_INSTALL_DIR}/docs` — and that directory
is added only `if(KPXC_FEATURE_DOCS)`. The workflow configured `-DKPXC_FEATURE_DOCS=OFF`, so the pages
were never built, never installed, and CPack never emitted their `File` rows. The shortcuts named them
regardless. `light.exe` does not treat that as a missing icon; it refuses to link.

The package step was coupled to a feature flag that nothing connected it to. Note that theory #3 below
had the right *variable* and the wrong *mechanism* — it checked for dangling install rules, found none,
and closed the question. The shortcuts are not install rules.

**The fix, in two halves.**

1. `src/CMakeLists.txt` now generates the template with `configure_file(… @ONLY)`, substituting
   `@KPXC_WIX_DOC_SHORTCUTS@` with the two shortcuts when `KPXC_FEATURE_DOCS` is on and a comment when
   it is off. The MSI builds either way, and the packaged shortcuts and the packaged files can no
   longer disagree. `@ONLY` matters: the template is full of WiX's own `$(var.CPACK_…)` syntax.
2. The workflow no longer hard-codes the flag. A step decides it: `gem install asciidoctor`, then
   `--version` to prove it runs, then `KPXC_FEATURE_DOCS=ON` with an explicit `ASCIIDOCTOR_EXE` full
   path. Every failure path downgrades to `OFF` with a `::warning::` rather than failing — a shipped
   installer without help pages beats no installer. `ASCIIDOCTOR_EXE` is passed explicitly because the
   gem installs a `.bat` shim and which extensions `find_program` tries is not worth a 45-minute bet.

### Do not re-try these

| # | theory | why it is dead |
| --- | --- | --- |
| 1 | the `-snapshot` version string | `CPACK_PACKAGE_VERSION` uses `KEEPASSXC_VERSION_CLEAN`, which strips it |
| 2 | `qt.conf` installed twice | it is not |
| 3 | `KPXC_FEATURE_DOCS=OFF` breaks install rules | it does not — but see above, the flag *was* the cause, through the WiX shortcuts |
| 4 | `cpack --config … -B artifacts` | changed to `cd build && cpack`; no effect. Kept, as it matches the workflow that last shipped an MSI |
| 5 | missing `WixUIExtension.dll` | adding it **caused** `LGHT0091 Duplicate symbol`: CPack already passes it whenever `CPACK_WIX_UI_REF` is set |
| 6 | the duplicate from #5 | reverted in `f6f9d8c4` |

### Why six rounds were wasted, and the diagnostic that ended it

`wix.log` could never be read. `actions/upload-artifact` dumps several hundred lines of runner
environment after every upload and the job log is reachable only by tail, so the real error was always
out of reach — and every round reasoned from the configuration instead of the error. That is guessing
with extra steps. The visibility should have been fixed after the second attempt, not the sixth.

The last step of the job is now **"Print why this job failed, last"**, which prints two blocks in the
final ~30 lines:

```
===== FAILED TESTS ... END FAILED TESTS =====   LastTestsFailed.log + FAIL!/QFATAL/ASSERT
===== WIX ERRORS  ... END WIX ERRORS  =====     LGHT####/CNDL#### codes, then 40 lines of wix.log
```

Fetch it with a **small** tail (45–70 lines). It paid for itself twice: it caught theory #5 as
self-inflicted within one run, and it produced the ICE67/ICE69 text above. If the MSI ever breaks
again, read that block first and fix exactly what the code names — `LGHT0091` duplicate symbol,
`LGHT0094` unresolved symbol, `LGHT0103` file not found, `LGHT0204`/`LGHT0217` ICE validation,
`CNDL####` a candle compile error. `no wix.log found` means packaging never ran; read the FAILED TESTS
block instead.

**Unverified:** the fix has not completed a run yet.

---

## 5. Still open

- **The MSI fix is unverified.** §4 explains it; no run has completed against it yet.
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
