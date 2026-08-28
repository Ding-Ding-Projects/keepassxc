# Handoff — Windows-native Material rewrite and Squirrel distribution

Last verified: 2026-08-28

## Current task update

- New-entry TOTP now stays synchronized with the editor-owned attributes working copy. The attributes model is never rebound to the transient unsaved entry, and an open TOTP dialog closes when that entry is destroyed.
- Squirrel packaging marks only the main GUI and its generated execution stub as aware. The CLI, browser proxy, and bundled Visual C++ redistributable are no longer launched as installation hooks.
- Repeated automatic update failures produce one notification until the state changes or the user explicitly retries.
- Settings wheel gestures scroll from the content area, the scrollbar remains inside the page, screen-capture affinity changes preserve window state, and Windows Hello quick unlock is user-triggered rather than automatic on file open.
- Passkey import now provides a model-backed searchable entry picker suitable for groups containing at least 1,000 entries.
- Every database gets an isolated encrypted local Git repository under application data. A saved deletion can restore missing entries from the preceding encrypted KDBX snapshot without overwriting current entries; the restore is appended as another history event.
- The fork default branch is `main`. Upstream and Transifex resources that are genuinely named `develop` remain unchanged.
- The entry editor now exposes direct TOTP setup only while creating an unsaved entry; the existing TOTP dialog validates and stores the secret on that disposable in-memory entry before its final save. Existing-entry edit sessions keep the established selected-entry action so editor Cancel semantics are unchanged.
- TOTP and passkey attributes coexist on one entry and survive a real KDBX round trip.
- Browser startup repairs the otherwise unusable state where browser integration and automatic extension setup are enabled but Chrome-family, Firefox-family, and Edge native-messaging registrations are all absent.
- The development-snapshot startup warning and its obsolete settings control/config key are removed. Other actionable error and user-triggered completion notifications remain.
- Tagged CMake configuration now accepts an exact semantic version with one optional leading `v` and normalizes it before the strict version check.
- Focused verification at the task tree: `testmaterialhistory`, `testmaterialshellresponsive`, `testpasskeys`, and `testupdatecheck` passed together in 3.71 seconds. The new GUI cases compiled, but the isolated `testAddEntry` process crashed before entering the test body with the pre-existing access violation in `AutoTypeExecutor::~AutoTypeExecutor`; they are not reported as runtime passes. A real unsigned Squirrel package was rebuilt and its verifier proved that only `KeePassXC.exe` and its exact generated execution stub are Squirrel-aware. The cheap Lowlevel persistent HTTP endpoint was unavailable, so no installed-artifact screenshot claim is made.

## Closeout handoff — 2026-08-21

- The preserved Settings checkpoint from `f27d787934fd11667cb0cd017390c95531da27f6` is integrated during this closeout. Its source changes include truthful persisted/default provenance, independent bounded per-page search, keyboard-operable rows, compact page selection, a dialog-emoji preference, bounded local personal-vocabulary loading/reset, and redacted Settings history events.
- Focused Settings compilation, runtime interaction, design-parity evidence, and negative completeness tests were not completed during this closeout. The work remains explicitly incomplete rather than being treated as verified by the merge.
- Installer Visual C++ prerequisite handling from issue #8 is repaired by preventing Squirrel from executing the bundled redistributable as an install hook.
- The capture-related unsolicited main-window minimizing from issue #9 is repaired by preserving state around display-affinity changes and excluding transient popup windows.
- Unconditional Material Design 3 and legacy-style-route removal are tracked in [issue #10](https://github.com/Ding-Ding-Projects/keepassxc/issues/10).
- Per-database local Git history from issue #11 now has isolated repositories, encrypted snapshots, and deleted-entry restoration.
- The fail-closed per-surface feature inventory and remaining implementation are tracked in [issue #12](https://github.com/Ding-Ding-Projects/keepassxc/issues/12).

## Current repository state

This fork is a 64-bit Windows 10/11 native C++/Qt 6 application. CMake rejects non-Windows,
non-MSVC, and non-x64 configurations before dependency discovery. Squirrel.Windows is the only
supported installer and update format; CPack, WiX, MSI, NSIS, and portable-ZIP release paths are
removed. Code signing is permanently disabled, so setup and update executables must report
`NotSigned` and may trigger Unknown Publisher or SmartScreen warnings.

The default branch is `main`. Its exact pushed closeout commit and ancestry proof are recorded in
issue #7. The incomplete Settings checkpoint is no longer a branch-only change: its source commit
`f27d787934fd11667cb0cd017390c95531da27f6` is integrated while its missing verification remains
open in issue #12. No stash contains task work.

## Completed architecture and delivery work

- Enforced the Windows/MSVC/x64 build boundary and added a fail-closed platform-scope guard.
- Removed remaining macOS implementation/resources and obsolete Unix package helpers while
  preserving Windows Auto-Type, Windows Hello, device, screen-lock, window-chrome, browser, SSH,
  YubiKey, and runtime-deployment consumers.
- Added root build, installer, and dependency scripts with silent operation and pinned tools.
- Made Squirrel.Windows the sole installer/update route. Required outputs are `Setup.exe`,
  `RELEASES`, one full `.nupkg`, optional valid deltas, provenance, receipt, and update manifest.
- Propagated one strict three-part version through PE metadata, visible app version, package, feed,
  update manifest, release tag, and release title, with packaged-PE version verification.
- Made installer builds production-only while direct local builds remain test-capable.
- Added pinned Qt 6.8.3 MSVC x64 and `vcvars64.bat` bootstrap to the release workflow.
- Fixed step-local release version/tag environment and removed ref-shared concurrency after live
  evidence showed that GitHub replaces older pending runs. Every later push retains its own verdict.
- Hardened Squirrel lifecycle argument position/arity/version parsing, canonical app/updater layout,
  structured helper diagnostics, and install-owned browser/file/URI refresh/removal. Foreign records
  are preserved.
- Added verified package download/apply states, persistent unsigned ready actions, deferral,
  unsaved-database protection, and `Update.exe --processStart KeePassXC.exe` relaunch.

## Completed native Material surfaces

- Five responsive shell classes are wired into real Vault geometry: Compact, Medium, Expanded,
  Large, and Extra Large.
- Compact navigation has five direct destinations plus searchable More. Compact More and the Vault
  group fallback own independent bounded regex-builder state and focus restoration.
- Tabs use stable runtime IDs and support keyboard navigation/reordering, pointer drag,
  same-pin-partition movement, insertion markers, no-op suppression, and model-drift cancellation.
- Appearance includes Auto/Light/Dark, seed, density, installed font family, 85–140% scale, weight,
  live preview, responsive cards, bounded regex search, and initial real element overrides.
- Reports constructs exactly six canonical cards: `breached`, `weak`, `reused`, `expired`,
  `healthy`, and `statistics`. It has truthful breach-unavailable state, real report data, bounded
  regex filtering, UUID selection/export, accessibility facts, and responsive layouts. Legacy
  two-section widgets are removed.
- History has truthful states, locale/ISO dates, action filters, bounded regex search, selection,
  export, and revision-specific accessible Diff/Restore controls.
- History persistence is an isolated append-only local Git repository under stable app data. It
  commits redacted metadata, opaque IDs, fingerprints, and validated encrypted KDBX snapshots in
  one transaction. Bounded locks serialize writers; retrieval revalidates containment, signatures,
  and SHA-256 and never overwrites a live database.
- Changelog parses every bundled release, composes date and bounded-regex filters, renders safe
  interactive Markdown, supports copy/export, exposes explicit states, and reflows at five widths.
  A generated catalog maps 54 releases to exact tag commits and URLs; pending 2.8.0 is the sole
  exemption. Build and workflow guards require complete tags and commit objects.

## Design-reference state

`design/` is the visual source of truth. It has a deterministic localhost renderer and a hand-written
nine-row inventory covering Appearance, Changelog, History, main shell, Regex Builder, Reports,
Settings, Sheet/Tools, and Vault. Structural and negative guards are green.

Appearance, Reports, History, and Changelog have source routes marked
`implemented-source-unverified`. Raw reference captures, installed-artifact captures, labelled
comparisons, machine-readable diffs, per-primitive Material Design 3 audits, source receipts, and
capture-tool provenance remain pending. Source routes and unit tests are not visual evidence.

## Focused test inventory

These test files and QTest function counts exist at the handoff. Green results were produced by
focused MSVC/Qt 6.8.3 builds on the commits that introduced them; the full suite was not rerun at
`a3fe002b`.

| Test source | Functions | Last focused result |
| --- | ---: | --- |
| `tests/TestMaterialBreakpoints.cpp` | 2 | Green; breakpoint probe red then green |
| `tests/TestMaterialShellResponsive.cpp` | 5 | Green; includes settings content-wheel and scrollbar containment |
| `tests/TestMaterialSearchRegistry.cpp` | 4 | Green |
| `tests/TestMaterialRegexSafety.cpp` | 3 | Green |
| `tests/TestMaterialTabs.cpp` | 3 | Green |
| `tests/TestUpdateCheck.cpp` | 6 | Green; restart-command probe red then green |
| `tests/TestSquirrelLifecycle.cpp` | 8 | Green; spoof/ownership probes red then green |
| `tests/TestMaterialAppearance.cpp` | 5 | Green; Config-registration probe red then green |
| `tests/TestMaterialReports.cpp` | 2 | Green; card/legacy-widget probes red then green |
| `tests/TestMaterialHistory.cpp` | 7 | Green; real Git/KDBX/concurrent-writer and deleted-entry restore integration |
| `tests/TestMaterialChangelog.cpp` | 2 | Green; provenance/control probes red then green |

The latest local production-only Squirrel proof was version 2.8.5. `Setup.exe` SHA-256 is
`9e465ac16888d4e8985cc7cb6dd49cb134c9e16a9e501e25261cf605cd73c8b4`; full package SHA-256 is
`6de12ae1916ad004e11b825ec82dcda72b20f92d8c4a61cd0ad79a5bd88e9377`.

## Published baseline and external state

The latest manually verified Squirrel release is
[`v2.8.1`](https://github.com/Ding-Ding-Projects/keepassxc/releases/tag/v2.8.1), targeted at
`a4453c5e5847346679a219cb609b5a46d1d93239`. Its unsigned setup and full package include receipt,
provenance, and update metadata. No delta was published because no verified prior same-identity full
package was supplied.

Several newer `Material Squirrel Build and Release` runs are still in progress. This handoff does not
assert a green remote result. The next owner must inspect the newest run for current `main` and
verify its release target and attached assets.

## Preserved incomplete Settings checkpoint

`origin/codex/settings-parity` at `f27d787934fd11667cb0cd017390c95531da27f6` is intentionally not
merged. It contains coherent but unbuilt work:

- visible persisted-versus-compiled-default provenance on settings rows;
- independent bounded per-page searches;
- keyboard/focus/accessibility improvements;
- compact page picker and responsive sidebar widths;
- real persisted dialog-emoji toggle;
- visible bounded local-only personal-vocabulary JSON upload/clear controls;
- redacted Settings event hooks into local Git History.

Before merging, add a focused Settings target, explicit hand-written inventory, red/green
explanation/provenance/search guards, duplicate-key/depth/cache/no-network vocabulary tests,
dropdown-local regex builders, application-wide vocabulary consumption, emoji preference use in
every dialog, five-width geometry checks, and the Settings parity route.

## Remaining work

- Finish and verify Settings before merging its preserved branch.
- Migrate Vault secondary flows, Regex Builder, Sheet/Tools, Database, Editor, Help, and all remaining
  dialogs, wizards, menus, pickers, overlays, and state surfaces.
- Complete every-element Appearance coverage, full color translator, theme import/export, typography
  depth, and local-history wiring.
- Add encrypted-history snapshot export and safe user-directed restore; wire settings, discard,
  import, and bulk producers to the redacted event seam.
- Run installed Squirrel N→N+1 install/update/defer/restart/repair/uninstall/recovery tests in an
  isolated Windows account or VM, including native messaging, associations, URI handling, AppData
  stability, database preservation, and invalid-update recovery.
- Produce deterministic reference and installed-artifact captures for all nine rows across themes,
  languages, five widths, scales, densities, reduced motion, and declared states, then generate
  comparisons, diff JSON, and Material audits.
- Update README, categorized documentation, landing/download guidance, wiki/Pages, and final release
  records after the remaining behavior is implemented and captured.

## Next safe actions

1. Continue from `origin/codex/settings-parity`, reconcile with current `main`, add focused
   Settings coverage, and merge only after it passes.
2. Inspect the newest release workflow for `origin/main`; if terminal, record its exact conclusion,
   tag, target commit, artifact names, sizes, and hashes.
3. Continue one referenced destination at a time, keeping a parity row source-implemented but
   unverified until installed-artifact evidence is complete.
