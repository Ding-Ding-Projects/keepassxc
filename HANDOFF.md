# Handoff — Material Design 3 interface rewrite

Branch `material-ui-rewrite`, pushed to `origin`.

Tracked in [issue #1](https://github.com/Ding-Ding-Projects/keepassxc/issues/1),
progress thread [discussion #2](https://github.com/Ding-Ding-Projects/keepassxc/discussions/2).

## This fork is Windows only

Linux is not supported and the code that supported it is gone, not disabled:

| Removed | What it was |
| --- | --- |
| `src/fdosecrets/` | freedesktop.org Secret Service server (376 K) |
| `src/gui/osutils/nixutils/` | `NixUtils`, XDG portals, D-Bus screen lock, libusb listener (160 K) |
| `src/autotype/xcb/`, `src/autotype/wayland/` | X11 and portal auto-type backends |
| `src/quickunlock/Polkit*`, `src/quickunlock/dbus/` | PolKit quick unlock |
| `src/gui/org.keepassxc.KeePassXC.MainWindow.xml` | the D-Bus adaptor interface |
| `snap/`, `share/linux/` | Snapcraft, AppImage runner, `.desktop`, polkit policy, appstream |
| Snap / Flatpak / AppImage code paths | `KEEPASSXC_DIST_*` and every branch behind it |
| `WITH_X11`, `KPXC_FEATURE_FDOSECRETS`, `Qt6::DBus`, libusb, keyutils | build options and link dependencies |
| `release-tool.py` `build_linux` / `_build_linux_appimage` / appstream check | Linux release plumbing |
| The Linux CI job | only Windows gates the release now |

macOS sources are still in the tree but are neither built nor tested here.

---

## Read this first: how to run and screenshot it

**KeePassXC is invisible to every screen-capture API unless you pass `--allow-screencapture`.**
`WinUtils.cpp:64` calls `SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE)` on every
top-level window. The window stays visible on the physical display, stays hit-testable, is not
cloaked and reports `IsWindowVisible = true` — but `PrintWindow`, `BitBlt` and `CopyFromScreen`
all return black, and a desktop grab of its rectangle returns *the windows behind it*. This is
upstream behaviour, present in the pre-rewrite baseline, and it cost this project several hours
of misdiagnosis. Always launch with:

```
KeePassXC.exe --config %TEMP%\kpxc.ini --allow-screencapture
```

Capture with `scratchpad/capture.ps1` under **Windows PowerShell 5.1** (not pwsh 7 — `System.Drawing`
is not in its default assemblies). The script is DPI-aware, checks `GetWindowDisplayAffinity` and
fails loudly on the exclusion case, and refuses to capture a window that does not own the pixels at
its own centre.

Headless works fine and keeps the app off the real desktop:
`launch_on_headless_desktop` + `screenshot(hwnd=...)`, again with `--allow-screencapture`.

### Build

```
scratchpad/configure.cmd      # Qt 6.8.3 msvc2022_64, Ninja, vcpkg, docs off
scratchpad/build.cmd
```
Kill any running `KeePassXC.exe` / `test*.exe` first or the link fails with `LNK1168`.

---

## What is done

| Area | State |
| --- | --- |
| Stock styling deleted | `BaseStyle` (4 860 lines), `LightStyle`, `DarkStyle`, `phantomcolor`, all four `.qss`, `styles.qrc` — gone |
| Design system | `MaterialTheme` — colour roles, 4 seeds, light/dark, density (40/52/64), type scale, `QPalette` |
| Stylesheet | `MaterialStyleSheet` — one generated sheet covering every stock Qt widget class |
| Style | `MaterialStyle` — `QProxyStyle` over Fusion for what a sheet cannot express |
| Icons | `MaterialIcons` — Material Symbols names → bundled SVGs, tinted and cached |
| **Window shell** | **`Material::Shell` owns the interior: 88px rail, 64px app bar, 48px tab strip, 5-destination stack** |
| Command palette | `Ctrl+Shift+P`, lists all 218 `QAction`s with menu path + shortcut |
| `.ui` sweep | 57 of 73 surfaces on Material widgets; 203 instances; no hard-coded colours left in `src/` |
| Passkeys | Save path verified — attributes stored, 3 secrets protected, survives a real KDBX round trip |
| Passkey clipboard | `keepassxc-passkey:v1:<base64>`, non-suppressible warning, 5 review findings fixed |
| Browser extensions | Per-user registration (HKCU / External Extensions JSON), store fallback |
| Language + humour | 3 modes, two 1–5 sliders, disclosure on first run |
| Notifications | Snackbars + notification centre |
| Dim sum | 1% at startup, disable-able |
| Docs | README, wiki (4 pages), Pages site at https://ding-ding-projects.github.io/keepassxc/ |
| CI | `.github/workflows/material-release.yml` — **works**: builds Windows + Linux, runs 45 tests, gates the release. Currently **red for a real reason**, see below |

---

## What is NOT done

### 1. The vault destination is still the stock three-pane widget

This is the biggest remaining gap. The shell is real, but inside the **vault** destination sits
`m_ui->stackedWidget` — the original welcome screen / `DatabaseTabWidget` / group tree / entry
table / preview pane. The design's own vault layout is **written but not wired**:

- `MaterialVaultSidebar` — 250px groups + tag chips
- `MaterialEntryDelegate` / `MaterialGroupDelegate` — rounded-16 rows, pill group rows
- `MaterialEntryDetail` — 392px detail pane, strength meter, TOTP ring

The agent that was to build `MaterialVaultScreen` and bind these to the real `GroupModel` /
`EntryModel` / `DatabaseWidget` **never ran** (session limit). Next step is exactly that task:
compose the three panes, install a proxy mapping `EntryModel` columns onto the delegate's
`TitleRole`/`UsernameRole`/`UrlRole`/`HealthRole`/`TotpRole`/`ModifiedRole`/`SymbolRole`, and
connect `EntryDetail`'s signals to the existing `DatabaseWidget` slots. **Do not reimplement entry
loading** — reuse the existing models.

### 2. Destination feeds are partial

`MaterialReportsFeed`, `MaterialHistoryFeed`, `MaterialChangelogFeed`, `MaterialSettingsHub` and
`MaterialHistoryStore` landed but their agents died mid-task. They compile and are wired enough to
populate Reports and History with real database facts, but they were never finished or reviewed.
Treat them as drafts.

Reports deliberately omits password-strength scoring — scoring every entry blocks the UI. The
existing Reports action still opens the real health check.

### 3. Known defects

| # | Defect | Notes |
| --- | --- | --- |
| 21 | `testdimsum` fails, runs **2539s** | Four separate causes, all now addressed — **verify before closing**. (1) `DimSum::shouldShow()` called `canShow()→isQuiet()` on every invocation, an out-of-process `SHQueryUserNotificationState` round trip, 20 000 times; the whole decision is latched now. (2) The `m_animation` `finished` lambda was connected *before* `m_holdTimer` existed — this is the Linux `SIGSEGV ... address 0xe0`; the timer is now built first. (3) `dismiss()` called `m_animation->stop()` while the animation sat exactly on its end value, which re-emits `finished()` and re-enters the handler that deletes the card; wrapped in a `QSignalBlocker`. (4) All five test functions shared process-wide latch state, so each read the previous one's leftovers; `DimSum::resetLaunchState()` now runs in `init()`. |
| 20 | `build/tests` missing Qt DLLs | `Qt6Testd.dll`, `Qt6Concurrentd.dll`. Any test run without Qt on `PATH` pops a **system-modal** error dialog that steals focus. Patched locally with `windeployqt`; needs a CMake post-build deploy step for the test targets. |
| — | Rail footer clipped | With the pre-release banner on a 900px window, the theme toggle and lock are pushed below the fold. `MaterialNavigationRail.cpp` lays the footer out at a fixed offset and does not compress. |
| — | `MaterialCard` unusable from `.ui` | Its constructor creates its own `QVBoxLayout`, so `uic`'s `new QVBoxLayout(card)` is rejected and children end up unparented. Make `m_rootLayout` lazy and every group box can be promoted mechanically. |
| — | 8 checkboxes left as `QCheckBox` | `tests/gui/TestGui.cpp` does `findChild<QCheckBox*>` on them; `Material::Switch` derives from `QAbstractButton`. Converting needs a two-line test change. `WITH_GUI_TESTS` is OFF so this is latent. |

### 4. CI status

`Material CI and Release` runs on every push. There is one test job — **Test (Windows x64)** — and
the release job is gated behind it with an explicit `success()`, so a failed or cancelled test job
cannot produce a release. The release job itself runs on an Ubuntu runner, but only to download the
Windows artifact and call the GitHub API; nothing Linux is built.

The Linux test job is **gone**, not skipped. Its two failures went with it:

- `testdimsum` segfaulted on Linux (`SIGSEGV ... for address 0xe0` in `testFiresOnlyOncePerLaunch`).
  The null dereference it was reporting was real and has since been fixed in shared code — see the
  defect table above — so removing the job did not bury it.
- `testcli` also failed on Linux. Never investigated; passes standalone on Windows at 83/0.

One cross-platform datum worth keeping now that it can no longer be re-measured: **`testmerge`
passed on Linux** (0.23 s). The two `testmerge` failures below are Windows-only, which is worth
knowing before anyone chases them as logic bugs.

### 5. Test status (local, Windows)

**41 of 43 pass.** Qt must be on `PATH` (`C:\Qt\6.8.3\msvc2022_64\bin`) or tests pop modal dialogs.

- `testmerge` — `testResolveConflictEntry_Synchronize` / `_KeepNewer`. **Pre-existing**, proven by
  building clean `develop` in a worktree and reproducing identically. Not a regression.
- `testdimsum` — ours, see above.
- `testcli` — flakes under parallel `ctest` (clipboard timing), passes standalone at 83/0.

`testpasskeys`: **35 passed, 0 failed.**

---

## Corrections worth carrying forward

Two things were reported as fact during this work and were wrong. Both are fixed, but the pattern
is worth knowing:

1. **"The main window renders nothing — confirmed bug in the rewrite."** False. It was
   `WDA_EXCLUDEFROMCAPTURE`. The control experiment (build stock upstream and see if *it* renders)
   was skipped; when finally run it answered in minutes.
2. **"The Material shell has landed."** False. The 59-file component library was dead code —
   `grep` for `NavigationRail|TopAppBar|TabStrip` outside `src/gui/material/` returned zero. "It
   compiles" was mistaken for "it works". That grep is the objective check; run it, not the vibe.

---

## Suggested next steps, in order

1. Build `MaterialVaultScreen` and wire the three panes to the real models (§1). This is the
   remaining substance of the rewrite.
2. Run `testdimsum` alone and find the real failure (§3, task 21).
3. Add the CMake deploy step for test targets (§3, task 20).
4. Finish the settings spec sheets against real `Config` keys, and the Reports/History/Changelog
   feeds (§2).
5. Capture all five destinations headless with `--allow-screencapture` and post them to issue #1 —
   the project's own rule is that a fix with a visible surface gets a capture from the real build.
