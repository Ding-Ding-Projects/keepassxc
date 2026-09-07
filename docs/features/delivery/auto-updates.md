# Automatic updates

Feature id: `auto-updates` · Category: Build, install and update

## Behaviour

`UpdateChecker` (`src/networking/UpdateChecker.h`) reads the fork-owned versioned Squirrel manifest, streams full packages with storage preflight, atomic finalisation and SHA-256 and SHA-1 validation, stages them through a verified local Squirrel feed, and shows persistent non-blocking ready actions with deferral, unsaved-database protection and a `Update.exe --processStart` relaunch. A new check is ignored while the manifest request, package transfer, or updater process is active, so an overlapping timer or manual request cannot replace a live update state. Repeated background failures raise one notification until the state changes or the user retries.

## Configuration

`GUI_CheckForUpdatesIncludeBetas` defaults to false. Stable checks request `https://github.com/Ding-Ding-Projects/keepassxc/releases/latest/download/update-manifest-v1.json`. When beta inclusion is enabled, the updater first requests the public GitHub REST release index for this repository with `per_page=20`, then selects the highest compatible numeric version from that bounded response, considering both stable and prerelease entries. A newer stable release therefore outranks an older prerelease regardless of index order. This is deliberately a bounded recent-release window, not an exhaustive scan of historical releases.

Eligible releases are non-draft entries tagged `vMAJOR.MINOR.PATCH`, with canonical decimal components from 0 through 65535 and an exact repository/tag-bound `update-manifest-v1.json` asset. Leading zeros and suffixes such as `-beta1` are excluded because the current Squirrel packaging and executable-version contracts use three numeric components. GitHub's `prerelease` flag can mark a release with a compatible numeric tag. A valid index containing no eligible manifest falls back to the stable endpoint. Malformed, oversized, redirected, unavailable, or rate-limited index responses fail visibly instead of silently falling back. The feed remains unsigned.

## Failure modes

An isolated N to N+1 install, defer, restart and rollback proof is still pending. Index and manifest requests require HTTP 200, have 30-second inactivity and absolute deadlines, and retain at most 256 KiB and 64 KiB respectively, plus one overflow sentinel byte. HTTP 403/429 rate limits and other non-200 responses use the connection-failure state and retain the existing next-check schedule. JSON schema and size errors report their own failure states. Only a validated manifest advances the next successful-check date by seven days.

Destroyed managers or active replies fail the current check once. Finished replies are disconnected before replacement, and generation checks ignore stale callbacks. Immediate retries from state-change notifications retain their own version selection and manual-request context. A package redirect outside the GitHub HTTPS allowlist is rejected and reported as a redirect refusal rather than as an offline connection failure.

## Security considerations

HTTPS transport and package hashes provide integrity; no signature is claimed because code signing is permanently disabled. Index requests carry an explicit updater User-Agent and GitHub JSON Accept header. Manifest requests use JSON Accept. The fixed index endpoint accepts no redirects. Manifest redirects require approval before Qt follows them, allowing this repository's release download paths and the exact GitHub asset-storage hosts `objects.githubusercontent.com` and `release-assets.githubusercontent.com`, over HTTPS without credentials, fragments, or nonstandard ports.

An index-selected manifest must report that exact numeric version. Every manifest, including stable fallback, binds its notes URL, full-package filename, and package URL to this repository's canonical `vVERSION` release paths. An allowlisted hostname alone cannot authorize a different repository, tag, or package.

## Verification

`testupdatecheck` contains controlled-network regressions for stable selection, numeric prereleases, stable-versus-prerelease ranking, unsupported-tag and invalid-asset exclusion, headers, HTTP and network failures, malformed/oversized responses, redirects, exact manifest/package identity, reply destruction, stale callbacks, and reentrant retries. It also retains package, restart-command, and download-lifecycle coverage. Tests use an isolated temporary configuration and injected replies without live network traffic. The newly expanded index matrix requires a native suite run before a passing result can be claimed. `testsquirrellifecycle` separately covers installer lifecycle behavior.

## Suggested articles

- [Unsigned Squirrel.Windows installer](../delivery/squirrel-installer.md)
- [One-click build and installer scripts](../delivery/build-scripts.md)
