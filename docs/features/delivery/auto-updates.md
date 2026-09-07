# Automatic updates

Feature id: `auto-updates` · Category: Build, install and update

## Behaviour

`UpdateChecker` (`src/networking/UpdateChecker.h`) reads the fork-owned versioned Squirrel manifest, streams full packages with storage preflight, atomic finalisation and SHA-256 and SHA-1 validation, stages them through a verified local Squirrel feed, and shows persistent non-blocking ready actions with deferral, unsaved-database protection and a `Update.exe --processStart` relaunch. A new check is ignored while the manifest request, package transfer, or updater process is active, so an overlapping timer or manual request cannot replace a live update state. Repeated background failures raise one notification until the state changes or the user retries.

## Configuration

Update checks and beta inclusion are configuration keys; stable checks use the latest stable manifest, while beta inclusion first reads the bounded project release index and then requests the selected prerelease manifest over HTTPS. The feed is unsigned by policy.

## Failure modes

An isolated N to N+1 install, defer, restart and rollback proof is still pending. A package redirect outside the GitHub HTTPS allowlist is rejected and reported as a redirect refusal rather than as an offline connection failure.

## Security considerations

HTTPS transport and package hashes provide integrity; no signature is claimed because code signing is permanently disabled.

## Verification

`testupdatecheck` (eight cases, including injected manifest and package reply lifecycles) and `testsquirrellifecycle` (eight cases).

## Suggested articles

- [Unsigned Squirrel.Windows installer](../delivery/squirrel-installer.md)
- [One-click build and installer scripts](../delivery/build-scripts.md)
