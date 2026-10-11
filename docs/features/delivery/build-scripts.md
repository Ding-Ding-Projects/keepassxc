# One-click build and installer scripts

Feature id: `build-scripts` · Category: Build, install and update

## Behaviour

`build.bat` (silent with `/s`) resolves the pinned toolchain through `download-dependencies.bat`, exports the MSVC x64 environment itself, configures with CMake and Ninja, builds, installs to `stage\app` and reports the executable hash. `build-installer.bat` produces the verified Squirrel package from that stage.

## Configuration

The documentation website records its own build-start timestamp once in `build-provenance.json`, together with its exact source and a version-plus-source build identity. The front screen displays that identity and build update time. Product release provenance and installer downloads retain their separate recorded release time. Both producer and consumer validate the website record; a release-origin timestamp is not accepted as website build provenance. `scripts/test-site-build-provenance.mjs` rejects mismatched identity, invalid dates, wrong provenance origin and every missing field. Native application provenance has a separate acceptance boundary.

`QT_ROOT_DIR` and `KPXC_VCPKG_ROOT` override the user-scoped toolchain locations.

`QT_DEPLOY_ROOT_DIR` optionally selects a separate Qt deployment installation while compilation retains `QT_ROOT_DIR`. Before executing its tools, the builder compares every relative file path and SHA-256 in `bin`, `plugins`, `qml` and `translations`, including hidden files, against the compilation installation. Linked/reparse paths are rejected and Qt versions must match. The selected `qtpaths.exe` is placed first on the child PATH, and the selected `windeployqt.exe` is passed explicitly to every CMake configure so a stale cache cannot select another deployment tool. These path checks do not eliminate a hostile concurrent filesystem rename race.

This explicit override can recover a host-specific short-name alias collision without moving a shared installation. First preserve the original installation, create a task-owned full copy in a new path, compare the complete file inventory and hashes, and verify each destination path survives the Win32 short-name/long-name round trip unchanged. Set the override only for that validated copy. The builder does not automatically diagnose collisions or provision this recovery copy.

## Failure modes

From a plain shell the first compile used to fail on `<assert.h>` because no script exported the MSVC environment; fixed. A build directory whose path contains spaces breaks the readline pkg-config include path; build under a space-free directory.

## Security considerations

Never installs secrets or a signing certificate; per-process execution policy bypass only.

## Verification

Run `build.bat /s` from a fresh shell and confirm `Built application:` and its SHA-256 are printed.

The October 11 source repair is awaiting an exact root-entrypoint run. A separately pinned root build cleared deployment using an externally verified byte-identical copy and generated-cache selection; that diagnostic recovery does not establish that this revised script has passed. Native checks and fresh-machine acceptance remain separate.

## Suggested articles

- [Unsigned Squirrel.Windows installer](../delivery/squirrel-installer.md)
- [Design-reference parity](../design/design-parity.md)
