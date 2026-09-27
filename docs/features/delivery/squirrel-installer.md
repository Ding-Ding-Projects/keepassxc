# Unsigned Squirrel.Windows installer

Feature id: `squirrel-installer` · Category: Build, install and update

## Behaviour

`scripts/build-squirrel.ps1` packages the staged application through Squirrel.Windows and emits `Setup.exe`, `RELEASES`, one full `.nupkg`, optional deltas, build provenance, an artifact receipt and the update manifest. `scripts/verify-squirrel-artifacts.ps1` proves the setup executable is unsigned, the package contains the expected entries and the provenance names the intended commit.

## Configuration

Version comes from `OVERRIDE_VERSION`; only the main GUI and its generated stub are Squirrel-aware.

## Failure modes

Setup may trigger Unknown Publisher or SmartScreen warnings because code signing is permanently disabled.

Packaging stops before building if `HEAD` cannot be read, Git cannot start or inspect source status,
or the checkout contains tracked or untracked changes. The diagnostic separates those cases and
reports only status counts; it never prints changed paths or command output.

## Security considerations

No signing certificate is ever requested or used.

## Verification

`testsquirrellifecycle` and `scripts/test-packaging-clean-source-check.ps1`; release assets are
verified again in the publish job. The clean-source regression check covers clean state, command failures,
tracked and untracked paths, malformed status records, and path-redaction cases.

## Suggested articles

- [Automatic updates](../delivery/auto-updates.md)
- [One-click build and installer scripts](../delivery/build-scripts.md)
- [Line count in every release](../delivery/line-count-release.md)
