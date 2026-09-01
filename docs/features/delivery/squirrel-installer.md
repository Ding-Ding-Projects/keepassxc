# Unsigned Squirrel.Windows installer

Feature id: `squirrel-installer` · Category: Build, install and update

## Behaviour

`scripts/build-squirrel.ps1` packages the staged application through Squirrel.Windows and emits `Setup.exe`, `RELEASES`, one full `.nupkg`, optional deltas, build provenance, an artifact receipt and the update manifest. `scripts/verify-squirrel-artifacts.ps1` proves the setup executable is unsigned, the package contains the expected entries and the provenance names the intended commit.

## Configuration

Version comes from `OVERRIDE_VERSION`; only the main GUI and its generated stub are Squirrel-aware.

## Failure modes

Setup may trigger Unknown Publisher or SmartScreen warnings because code signing is permanently disabled.

## Security considerations

No signing certificate is ever requested or used.

## Verification

`testsquirrellifecycle`; release assets are verified again in the publish job.

## Suggested articles

- [Automatic updates](../delivery/auto-updates.md)
- [One-click build and installer scripts](../delivery/build-scripts.md)
- [Line count in every release](../delivery/line-count-release.md)
