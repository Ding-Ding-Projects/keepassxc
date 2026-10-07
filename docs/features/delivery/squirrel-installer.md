# Unsigned Squirrel.Windows installer

Feature id: `squirrel-installer` · Category: Build, install and update

## Behaviour

`scripts/build-squirrel.ps1` packages the staged application through Squirrel.Windows and emits `Setup.exe`, `RELEASES`, one full `.nupkg`, optional deltas, build provenance, an artifact receipt and the update manifest. `scripts/verify-squirrel-artifacts.ps1` proves the setup executable is unsigned, the package contains the expected entries and the provenance names the intended commit.

The NuGet package identity remains `KeePassXC.Material` because installed versions validate that exact identity while selecting a release package. Squirrel.Windows documents that shortcut creation can fail when a NuGet package ID contains a dot. The `--squirrel-install` and `--squirrel-updated` lifecycle handlers therefore create the per-user Desktop and Start Menu links directly. Each link starts the stable root `Update.exe` with `--processStart KeePassXC.exe`, so it resolves the latest installed version after an update. The Start Menu link is under the current user's Programs folder.

The lifecycle handler records each created link's exact path, target, arguments and SHA-256 in the current user's registry. On uninstall it deletes a link only when all recorded values still match. It preserves modified links, links without an ownership receipt, symbolic links and foreign files at the expected locations. The Squirrel `--removeShortcut` helper is not used because it deletes the computed link path without checking who owns its current contents.

This compatibility path follows the [Squirrel.Windows shortcut troubleshooting guidance](https://github.com/Squirrel/Squirrel.Windows/blob/develop/docs/faq.md#installing) and its [shortcut creation and removal implementation](https://github.com/Squirrel/Squirrel.Windows/blob/develop/src/Squirrel/UpdateManager.ApplyReleases.cs).

## Configuration

Version comes from `OVERRIDE_VERSION`; only the main GUI and its generated stub are Squirrel-aware.

## Failure modes

Setup may trigger Unknown Publisher or SmartScreen warnings because code signing is permanently disabled.

An unavailable Desktop or Start Menu path, a conflicting foreign link, a shell-link write failure or an unreadable ownership receipt makes the lifecycle hook return failure. Existing content is preserved. A user-modified shortcut is left in place and is not removed later.

Packaging stops before building if `HEAD` cannot be read, Git cannot start or inspect source status,
or the checkout contains tracked or untracked changes. The diagnostic separates those cases and
reports only status counts; it never prints changed paths or command output.

## Security considerations

No signing certificate is ever requested or used.

## Verification

`testsquirrellifecycle` and `scripts/test-packaging-clean-source-check.ps1`; release assets are
verified again in the publish job. The clean-source regression check covers clean state, command failures,
tracked and untracked paths, malformed status records, and path-redaction cases.
The disposable-profile Setup run, shortcut launch and uninstall checks are required to claim installed
behavior. A local `testsquirrellifecycle` result is required before its source-level lifecycle changes
are considered verified.

## Suggested articles

- [Automatic updates](../delivery/auto-updates.md)
- [One-click build and installer scripts](../delivery/build-scripts.md)
- [Line count in every release](../delivery/line-count-release.md)
