# Automatic updates

Feature id: `auto-updates` · Category: Build, install and update

## Behaviour

`UpdateChecker` (`src/networking/UpdateChecker.h`) reads the fork-owned versioned Squirrel manifest, streams full packages with storage preflight, atomic finalisation and SHA-256 and SHA-1 validation, stages them through a verified local Squirrel feed, and shows persistent non-blocking ready actions with deferral, unsaved-database protection and a `Update.exe --processStart` relaunch. Repeated background failures raise one notification until the state changes or the user retries.

## Configuration

Update checks and beta inclusion are configuration keys; the feed is unsigned by policy.

## Failure modes

An isolated N to N+1 install, defer, restart and rollback proof is still pending.

## Security considerations

HTTPS transport and package hashes provide integrity; no signature is claimed because code signing is permanently disabled.

## Verification

`testupdatecheck` (six cases) and `testsquirrellifecycle` (eight cases).

## Suggested articles

- [Unsigned Squirrel.Windows installer](../delivery/squirrel-installer.md)
- [One-click build and installer scripts](../delivery/build-scripts.md)
