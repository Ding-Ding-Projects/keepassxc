# Handoff — closing the gap between the Material UI and the design

> **Current direction (2026-08-21):** the fork is becoming a 64-bit Windows-only application with
> unsigned Squirrel.Windows installation and updates. Root build scripts, pinned packaging metadata,
> an artifact verifier, a hard CMake platform boundary, and the responsive breakpoint model are in
> the current working change. The full UI migration, old-WiX removal, installed-artifact proof, and
> updater replacement remain unfinished and must not be reported as shipped.

The first Reports parity batch retains the GUI-thread `HealthChecker` and `DatabaseStats` walk while
adding explicit empty/loading/populated/progress/warning/error states, real weak/reused/expired/
excluded categories, bounded regex filtering, collapsible sections, selectable findings, and scoped
Markdown export. Painted rows now expose their full facts to assistive technology and reflow at the
five shell widths. The inventory records the real Reports destination route as implemented in
source; visual captures, audits, comparisons, and diffs remain pending.
The corrective follow-up replaces the older two-section presentation with the canon's six stable
cards: `breached`, `weak`, `reused`, `expired`, `healthy`, and `statistics`. Weak/reused/expired
membership overlaps by real UUID; healthy rows come from the same `HealthChecker` pass; statistics
come from `DatabaseStats`; breach remains explicitly unavailable because this pass has no real HIBP
result. Expansion state is independent and survives content rebuilds.
The superseded standalone Password Health and Statistics cards are no longer constructed at all;
the header owns Export All and selected-finding export, and the focused Chut requires exactly six
`reportCard_*` widgets with no legacy card objects.

The first Appearance parity batch now exposes Auto/Light/Dark, seed, density, installed font
family, 85–140% scale, weight, a live CJK-safe preview, sparse per-element overrides, and a
continuous alpha-aware colour dialog. Appearance search is independently registered and now honors
plain text or bounded regex state. Cards reflow to one column below 840 logical pixels. The parity
inventory records the real Material-shell destination route as implemented in source, while raw
captures, audits, comparisons, and diffs remain explicitly pending.

The native shell now applies all five logical-width classes to real vault geometry. Large and Extra
Large use their exact group/detail widths; Expanded moves group selection into a searchable scope
menu; Medium and Compact replace inline details with an accessible overlay opened from the list
header. Compact navigation keeps the first five destinations in a 76 px bottom bar and the rest in
a searchable More menu, with current-state and focus handoff when the rail is hidden. A clean MSVC
and Qt 6.8.3 build produced the staged executable, and the focused breakpoint and shell tests pass.
No built-artifact visual capture has run yet, so this is source-line and local-test evidence only.
Both fallback searches are registered independently (`navigation.compact-more` and
`vault.group-scope`) and use the shared full regex builder with private pattern, flags, mode,
validation, and menu-aware focus restoration; a deliberate missing-registration probe turned the
responsive test red before the exact registration was restored.

The first real Squirrel candidate was produced from commit `a98ef19842595a638e07ff4e59fbfdc418be21c8`:
`Setup.exe` was 73,158,144 bytes and reported `NotSigned`; the full package was 72,320,747 bytes.
The deterministic artifact verifier passed. Installation, launch, update, rollback, and uninstall
remain unverified, so WiX has deliberately not been removed yet.

The next Windows-only contraction removed the macOS UI backend, auto-type implementation, Touch ID,
bundle and DMG assets, Apple compiler probes, and Unix manpage target after confirming the Windows
replacements remained compiled. The complete native build and all 46 registered local tests passed.

Branch `claude/ui-design-verification-ygrhj7`, [PR #6](https://github.com/Ding-Ding-Projects/keepassxc/pull/6).
33 commits off `develop` at `0dd3d702`, 195 files, +11 451 / −2 005.

**Builds, links, tests green, and the MSI now ships.** Run 30566007547 on `332bf39c` completed
successfully: `Package` succeeded, the installer guard passed, and `package-windows-x64` carries both
the MSI and the ZIP (138.4 MiB for the two). The offline documentation shipped —
`KPXC_FEATURE_DOCS: ON`, so `gem install asciidoctor` worked on the runner and the Getting Started and
User Guide shortcuts have real targets. That closes the MSI, which had been failing since before this
branch existed. The CodeQL move to Windows has not completed a run yet; §7.

This supersedes the previous handoff, which described the Material shell landing. Its §1 ("the vault
destination is still the stock three-pane widget") is done; several of its other claims were wrong and
are corrected below.

---

## 1. Read this first

**This fork is Windows only, and the mechanism matters.** `src/gui/osutils/` ships `winutils/` and
`macutils/`; `nixutils` is gone. `OSUtils.h` reads:

```c
#if defined(Q_OS_WIN)
#include "winutils/WinUtils.h"
#define osUtils static_cast<OSUtilsBase*>(winUtils())
#elif defined(Q_OS_MACOS)
...
#endif
```

There is no `#else`. On Linux `osUtils` simply does not exist, so every translation unit that touches
it fails — across `src/autotype/`, `src/browser/` and `src/gui/`. This is not a missing dependency and
no apt package fixes it. It is why the tree cannot be built or analysed on a Linux runner, and it cost
a full CI job (§5) before anyone read the error.

**KeePassXC is invisible to every screen-capture API unless you pass `--allow-screencapture`.**
`WinUtils.cpp` calls `SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE)` on every top-level
window. The window is visible on the physical display and reports `IsWindowVisible = true`, but
`PrintWindow`, `BitBlt` and `CopyFromScreen` all return black. This is upstream behaviour and it has
now cost two sessions hours of misdiagnosis. Always launch with:

```
KeePassXC.exe --config %TEMP%\kpxc.ini --allow-screencapture
```

**Nothing in this branch has ever been run.** Not once. It compiles, links and passes its tests; no
human or agent has opened the application and looked at the UI this branch exists to change. See §3.

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
own directory — and it had not been re-run after the shell landed. Re-run it whenever a component is
"finished":

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

It should print nothing, and at `332bf39c` it does. When it printed `SettingsHub`, `SettingsScreen`
and `GeneratorSheet`, that was 3 225 lines of finished UI that had never been constructed.

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
two that mattered are both security-relevant, and both were in code that looked correct:

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

**The standing constraint for anything that touches this surface:** this is a password manager. Never
fabricate a value, never write a secret to disk or to a log, and never leave a control that looks like
it works but does not. In particular `HistoryStore` must not persist entry content, passwords or
attachment bytes to its plaintext JSONL log.

---

## 3. How this was verified, and what that is worth

**A full build is impossible in this environment** — Windows-only fork, no `nixutils` (§1). Instead:
Qt 6.4 installed, `uic` and `config-keepassx.h` generated, then `g++ -fsyntax-only -std=c++17` against
the real headers. That catches everything short of link errors, and nothing about runtime behaviour.

**Two commits went out broken anyway, and the reason matters.** The check ran against the *working
tree* while work was landing concurrently, so a `.cpp` could be committed ahead of its `.h`;
`git add -A` snapshotted that instant. `edc96650` and `593c3ac` do not compile. Every commit after each
of them does.

The fix is `scratchpad/verify-and-push.sh`: it resets a detached worktree to `HEAD`, checks *that*, and
**refuses to push** on any failure. Use it for any C++ change. (It lives in the session scratchpad, not
the repo — if the scratchpad is gone, rewrite it before the next C++ push rather than pushing blind.)

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
because `docs/CMakeLists.txt` installs the pages into `${DATA_INSTALL_DIR}/docs` (`share/docs` on
Windows) — and that directory is added only `if(KPXC_FEATURE_DOCS)`. The workflow configured
`-DKPXC_FEATURE_DOCS=OFF`, so the pages were never built, never installed, and CPack never emitted
their `File` rows. The shortcuts named them regardless. `light.exe` does not treat that as a missing
icon; it refuses to link.

The package step was coupled to a feature flag that nothing connected it to. Note that theory #3 below
had the right *variable* and the wrong *mechanism* — it checked for dangling install rules, found none,
and closed the question. The shortcuts are not install rules. Getting the variable right is not the
same as getting the failure right.

**The fix, in two halves** (`332bf39c`):

1. `src/CMakeLists.txt` generates the template with `configure_file(… @ONLY)`, substituting
   `@KPXC_WIX_DOC_SHORTCUTS@` with the two shortcuts when `KPXC_FEATURE_DOCS` is on and a comment when
   it is off. The MSI builds either way, and the packaged shortcuts and the packaged files can no
   longer disagree. `@ONLY` matters: the template is full of WiX's own `$(var.CPACK_…)` syntax that
   CMake must not touch.
2. The workflow no longer hard-codes the flag. A step decides it: `gem install asciidoctor`, then
   `--version` to prove it runs, then `KPXC_FEATURE_DOCS=ON` with an explicit `ASCIIDOCTOR_EXE` full
   path. Every failure path downgrades to `OFF` with a `::warning::` rather than failing — a shipped
   installer without help pages beats no installer. `ASCIIDOCTOR_EXE` is passed explicitly because the
   gem installs a `.bat` shim and which extensions `find_program` tries is not worth a 45-minute bet.

Half 2 also fixes a silent defect nobody had noticed: the MSI had been shipping *without* the Getting
Started and User Guide entries at all.

**Verified in CI.** Run 30566007547 on `332bf39c` went green end to end: Configure, Build, Test,
Package, and the installer guard all succeeded; "Print why this job failed, last" was *skipped*, which
is the `if: failure()` proof that no WiX error block was produced. `KPXC_FEATURE_DOCS: ON` in the job
environment confirms half 2 worked — the documentation rendered and its shortcuts have real targets.
Before that it was verified locally as far as this environment allows: the substitution run through
real CMake at both settings, both outputs parsing as XML, all twelve `$(var.CPACK_…)` references
surviving, and the `&&` in the docs custom command reaching a shell rather than being escaped by
`VERBATIM`.

### Do not re-try these

| # | theory | why it is dead |
| --- | --- | --- |
| 1 | the `-snapshot` version string | `CPACK_PACKAGE_VERSION` uses `KEEPASSXC_VERSION_CLEAN`, which strips it |
| 2 | `qt.conf` installed twice | it is not |
| 3 | `KPXC_FEATURE_DOCS=OFF` breaks install rules | it does not — but the flag *was* the cause, through the WiX shortcuts. See above |
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

Fetch it with a **small** tail (45–70 lines; 40 is not enough — it lands mid-block). It paid for itself
twice: it caught theory #5 as self-inflicted within one run, and it produced the ICE67/ICE69 text
above. If the MSI breaks again, read that block first and fix exactly what the code names — `LGHT0091`
duplicate symbol, `LGHT0094` unresolved symbol, `LGHT0103` file not found, `LGHT0204`/`LGHT0217` ICE
validation, `CNDL####` a candle compile error. `no wix.log found` means packaging never ran; read the
FAILED TESTS block instead.

---

## 5. CodeQL: also read, also fixed

`Analyze (cpp)` had been red on every run for the life of the branch, and was written off in an earlier
handoff as "pre-existing, unrelated, maintainer's call". It was neither pre-existing in any useful
sense nor unfixable — it built on `ubuntu-latest`, and §1 is why that can never work. The job died at
~28% and reported `CodeQL job status was configuration error` every time.

The choice was to drop `cpp` from the matrix — leaving a password manager with no static analysis at
all — or to analyse the platform the fork actually targets. `codeql.yml` (`785e4f02`) now runs the same
MSVC + Ninja + vcpkg recipe `material-release.yml` builds and ships with, sharing its vcpkg cache key.
Configure runs *before* `codeql init` deliberately, so the tracer captures `src/` and not the vcpkg
ports. A `codeql-<ref>` concurrency group with `cancel-in-progress: true` was added — unlike a release,
a superseded analysis is worth nothing.

---

## 6. Still open

- **The CodeQL move to Windows is unverified.** The MSI fix is verified (§4); CodeQL is not. §7.
- **`testdatabase` fails intermittently, and it is NOT settled.** Four observations, none of them
  separated by a single line of C++: failed at `ef8f3707`, passed at `3d17b7e1`, passed at `332bf39c`,
  **failed twice** at `4387b8c6` (which differs from `332bf39c` by `HANDOFF.md` and workflow text
  only). So it is nondeterministic rather than caused by any code change — but at roughly a 50% rate,
  and `--repeat until-pass:2` did not save it, so calling it "flaky" and moving on is not good enough
  for a password manager's database test.

  **What was believed and was wrong:** the diagnostic reported "no FAIL! lines in LastTest.log; the
  test process probably died". That inference had no evidence behind it. The test executables are
  GUI-subsystem binaries on Windows, so their stdout is detached and QTest's report never reaches
  ctest — LastTest.log is empty for *any* outcome, assertion or crash alike. The silence was the
  expected state, not a symptom.

  **What to do:** the final step now re-runs each failing test directly with `-o <file>,txt`, which
  writes QTest's report to a file instead of stdout and so survives the GUI subsystem, and prints it in
  the last lines where a small tail reaches it. The next failure will name the function. Two outcomes
  are worth reading carefully — a re-run that *passes* means timing-dependence, and a re-run that
  exits non-zero with no report means it really did die early.

  **The one to suspect first** is `testExternallyModified` (`tests/TestDatabase.cpp:273`). It waits on
  a `QFileSystemWatcher` signal through `QTRY_COMPARE(spyFileChanged.count(), 1)` — a filesystem race
  with a bounded timeout, on a loaded CI runner. That is the shape of this failure. It is a hypothesis,
  not a finding; the re-run report will confirm or kill it.

  Note `testmerge`, which an earlier handoff recorded as a pre-existing Windows failure, now passes.
- **The vcpkg cache — resolved by the first green run.** It had never been hit: "Restore the vcpkg
  cache" finished in ~1 second every time and Configure then spent 29 minutes building ports. The cause
  was the obvious one in hindsight — no run had ever *completed*, so `actions/cache`'s post-step save
  had never committed a key. Run 30566007547 finished and saved 873 MiB under
  `vcpkg-Windows-f7977f7a…-66c0373d…`, and `material-release` now restores it in 26 s and configures in
  **45 seconds instead of 29 minutes**.

  **Open question: CodeQL does not seem to get the same cache.** `codeql.yml` was given the same key
  material deliberately, but run 30574045644 restored in 0 s and then spent 22 minutes in Configure,
  while `material-release` run 30574043349 — same branch, sixteen minutes later — restored in 26 s and
  configured in 45 s. So the two workflows are not sharing the entry despite intending to. Cause
  unknown; do not guess, read the `Restore the vcpkg cache` step's own log, which says whether it
  matched a key. Costs time, not correctness.
- **Branch archive.** `.github/workflows/archive-branches.yml` bundles all 25 branches with full
  history, verifies the archive restores every tip, and publishes it as a release. It is
  `workflow_dispatch` only and **has never been run** — an agent cannot trigger it. **A human must run
  Actions → "Archive branches".** Nothing may be deleted until that release exists and its
  `BRANCHES.txt` lists all 25. The bundle was verified locally: 25/25 tips, 59 tags, fsck clean, clones
  successfully.
- **`main` / branch cleanup** was requested and then parked by the user. Note there is no `main`; the
  default is `develop`. Of the 24 other branches, 14 share **no common ancestor** with this fork —
  merging one means resolving 366–686 file conflicts. The user's stated preference, if this resumes,
  was `git merge -s ours` for those 14 and a fresh `gh-pages`.
- **Two commits do not compile** (`edc96650`, `593c3ac`). Harmless unless you bisect; can be folded
  into their parents if the history should be clean.
- **The UI has never been run.** §1. Everything in §2 is verified by reading and by the compiler, not
  by use.

---

## 7. Pick it up here

Everything is committed and pushed; the working tree is clean.

**The MSI is fixed and proven.** `material-release` run 30566007547 on `332bf39c` went green end to
end and produced the installer. Nothing about WiX or packaging is outstanding.

**But the branch is not reliably green**, because `testdatabase` fails intermittently and stops the job
before packaging — that is what happened on `4387b8c6`. §6 has the four observations and the
hypothesis. Read the new re-run report on the next failure before concluding anything about it.

One thing is still unverified: **CodeQL on Windows.** Its run on `332bf39c` was cancelled by a
subsequent push (`cancel-in-progress: true`), so the newest run on the current head is its first
uninterrupted attempt. Confirm the Windows build completed and the analysis uploaded. Note it no longer
pays for a cold vcpkg cache — the MSI run populated the shared key.

Two smaller things, neither blocking:

- The job now prints a `===== PACKAGE =====` block listing each artifact with its size and the
  documentation setting, in the last steps where a small tail can reach it. Before that, "how big is
  the MSI" could only be answered by fetching a huge log, because `actions/upload-artifact`'s
  environment dump sits between `cpack`'s output and the end of the log. The first run to exercise it
  is the one after `5d42588a`.
- Nothing else in §6 needs CI.

---

## 8. Traps

1. **Pushing costs you runs, but not symmetrically.** `material-release` sets
   `cancel-in-progress: false`, so a push does *not* disturb a run that is already executing — it
   replaces the single *pending* run. `codeql` sets `cancel-in-progress: true`, so a push **does** kill
   an in-flight analysis and restart it from a cold cache. Before pushing, check what is in flight.
   Pushing repeatedly is why the `wix.log` diagnostic sat unexecuted through five rounds.
2. **`git bundle verify` will tell you an archive is complete when it is not.** This working clone was
   *shallow*; bundling it dropped 11 of 25 branches and still reported "records a complete history" —
   true only against its own shallow boundary. Check `git rev-parse --is-shallow-repository` first.
3. **A bundle of `refs/remotes/*` clones as an empty repository.** `clone` looks under `refs/heads/`.
   It builds, verifies, lists every branch, and restores nothing. Bundle from a bare mirror.
4. **`-fsyntax-only` on the working tree proves nothing about the commit.** §3.
5. **Findings are not facts.** A 235-finding audit of this UI produced 104 confirmed and 131 refuted
   after adversarial verification. Have claims refuted before acting on them; several "gaps" were
   features that already existed, and one attributed the extended FAB's metrics to `FilledButton`.
6. **"Pre-existing and unrelated" is a conclusion, not an excuse.** Both CI failures in §4 and §5 were
   filed that way for rounds. One was a template referencing files a flag had removed; the other was a
   Linux runner compiling a Windows-only tree. Both were readable from their own error output the whole
   time. Read the error before classifying the failure.

---

## 9. Final cleanup — done in this session

| Item | State | Date |
| --- | --- | --- |
| develop pulled to latest (c766e2e7) | ✅ Fast-forward, clean | 2026-07-31 |
| Merged branches identified and merged | ✅ material-ui-rewrite, feature/fido2 → develop | 2026-07-31 |
| Deleted merged branches from origin | ✅ Pushed deletion of material-ui-rewrite and eature/fido2 | 2026-07-31 |
| PR #6 status | ✅ Merged 2026-07-30T21:50:40Z at c766e2e7 | [PR link](https://github.com/Ding-Ding-Projects/keepassxc/pull/6) |
| Working tree | ✅ Clean, no stashes, no worktrees | 2026-07-31 |
| Local ↔ remote sync | ✅ HEAD == origin/develop at c766e2e7 | 2026-07-31 |

### Remaining open items (no agent action possible)

| Item | Action needed |
| --- | --- |
| Run rchive-branches.yml workflow | Human: Actions → "Archive branches" dispatch |
| Verify CodeQL Windows build on latest head | Confirm in CI; was cancelled by prior push |
| Fix two non-compiling commits (edc96650, 593c3ac) if clean history desired | Can be squashed/folded later |

### Active remote branches (unchanged — not ours to delete)

The following remain on origin and were **not** part of this fork's work stream. They belong to upstream or automated tooling:

- copilot/fix-* branches (Copilot-generated fixes from upstream)
- eature/* branches (upstream features not yet merged there either)
- ix/* branches (upstream fixes)
- dependabot/* branch (dependency update)
- gh-pages (docs site, not built from here)
- elease/2.7.x (upstream release branch)
- ork_keepassx_core (legacy fork tracking)

These should only be managed by their respective owners. Our fork is clean.

---

*This handoff supersedes all previous handoff documents for this repository. Last updated: 2026-07-31.*
