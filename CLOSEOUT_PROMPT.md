# KeePassXC installer and window verification continuation

Updated: 2026-10-07

## Objective and latest user direction

Repair the Squirrel setup lifecycle so a successful installation creates the application and its Desktop and Start Menu shortcuts. Restore actual pointer resizing for the main window. Verify the updater from the published `v2.8.32701` release to a new successful `main` release, and keep issue #12 open during this scoped work.

The latest user direction explicitly requested immediate preservation and a cleanup pass. This continuation preserves the incomplete work and does not claim the automatic usage threshold was reached.

## Source state

- Baseline `main`: `3356cdfece47a33fca6fb02db4dcd82f8c576e12`.
- Baseline successful GitHub Actions run: [37170195681](https://github.com/Ding-Ding-Projects/keepassxc/actions/runs/37170195681).
- Published baseline: [v2.8.32701](https://github.com/Ding-Ding-Projects/keepassxc/releases/tag/v2.8.32701).
- Task branch: `codex/installer-resize-update-20261006`.
- Source checkpoint: `d44209a3e37013d16e65b492cb875f634940447e`.
- Prior task checkpoints: `414ef883c15e24381a8cb4af82a205dfae6cdd22` and `cf1638b3009d68b8919a0ebcf4094ac9886f7ea8`.
- `d44209a3` adds direct per-user Desktop and Start Menu shortcut creation through Windows Shell Link, plus an ownership receipt containing path, target, arguments, and SHA-256. Uninstall removes only an unchanged receipt match. The source and its focused tests remain unverified.
- At the time of this record, the task branch has not yet been published. No integration into `main` has occurred.

## Build and verification

- The exact root `build.bat /s` entrypoint first stopped because the selected Ruby installation could not resolve `ruby_builtin_dlls`.
- A second attempt stopped because the entrypoint selected an incomplete MSVC environment.
- With the verified user-scoped Ruby and MSVC paths selected for that process, configuration succeeded and Ninja built most targets, including `SquirrelLifecycle.cpp`, `TestSquirrelLifecycle.cpp`, `testmaterialtitlebar.exe`, and `testsquirrellifecycle.exe`. The overall entrypoint still exited 1 with `ninja: build stopped: subcommand failed`. The specific failing subcommand diagnostic was not retained, so no passing build is claimed.
- `testsquirrellifecycle`, `testmaterialtitlebar`, and `testupdatecheck` were not run against this source checkpoint.
- `build-installer.bat /s`, package receipt verification, disposable-profile setup execution, shortcut launch, and owned-shortcut uninstall verification remain unrun.
- The live `v2.8.32701` setup and full package hashes were independently checked earlier. No installed update from that release to a new `main` release has been performed.
- Baseline native inspection found `WS_THICKFRAME` enabled and all eight `WM_NCHITTEST` edge and corner probes returned the expected resize codes at 96 DPI. These probes do not establish pointer dragging. No real edge or corner drag has been verified, and this continuation contains no window-resizing source change.
- A disposable Windows profile is available, but no Setup installation or update was completed in it. The everyday profile has not been used.

## Records, issue, and external status

- Issue #12 remains open, and its body and checklist were not edited. No progress or completion comment was posted because the new visible behavior has no genuine built-artifact capture yet.
- README, ROADMAP.md, HANDOFF.md, the wiki, and the project documentation site have not yet been updated for this task.
- The Status Hub read path could not be reached because SSH host-key verification failed. No host-key trust setting was changed and no status update was sent.
- No new task release or exact-source GitHub Actions run exists.

## Preservation and next safe actions

- Publish the task branch for preservation, verify the resulting ref, and keep this work unmerged while its required build, native tests, installer run, updater sequence, and real pointer-drag proof are incomplete.
- The task branch is not merged into `main`, so neither it nor its linked checkout is eligible for removal.
- Recover the exact Ninja subcommand diagnostic from retained local build evidence before any same-source retry. Retry only after a verdict-relevant repair.
- After a successful exact root build, run `testsquirrellifecycle`, `testmaterialtitlebar`, and `testupdatecheck`; then use the exact `build-installer.bat /s` entrypoint and verify the package receipt.
- Continue installation and updater checks only in the disposable profile. Prove version, executable hash, both shortcuts, shortcut launch, owned-shortcut removal, package hashes, updater states, restart, and resulting version.
- Verify real pointer drags on every edge and corner at the requested sizes, languages, themes, and display scales. Keep resizing unverified until genuine drag evidence exists.
- Update the directly related documentation and handoff records, keep issue #12 open, then integrate and publish only after the required local and hosted evidence is complete.
