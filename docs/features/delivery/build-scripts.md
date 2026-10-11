# One-click build and installer scripts

Feature id: `build-scripts` · Category: Build, install and update

## Behaviour

`build.bat` (silent with `/s`) resolves the pinned toolchain through `download-dependencies.bat`, exports the MSVC x64 environment itself, configures with CMake and Ninja, builds, installs to `stage\app` and reports the executable hash. `build-installer.bat` produces the verified Squirrel package from that stage.

## Configuration

The documentation website records its own build-start timestamp once in `build-provenance.json`, together with its exact source and a version-plus-source build identity. The front screen displays that identity and build update time. Product release provenance and installer downloads retain their separate recorded release time. Both producer and consumer validate the website record; a release-origin timestamp is not accepted as website build provenance. `scripts/test-site-build-provenance.mjs` rejects mismatched identity, invalid dates, wrong provenance origin and every missing field. Native application provenance has a separate acceptance boundary.

`QT_ROOT_DIR` and `KPXC_VCPKG_ROOT` override the user-scoped toolchain locations.

## Failure modes

From a plain shell the first compile used to fail on `<assert.h>` because no script exported the MSVC environment; fixed. A build directory whose path contains spaces breaks the readline pkg-config include path; build under a space-free directory.

## Security considerations

Never installs secrets or a signing certificate; per-process execution policy bypass only.

## Verification

Run `build.bat /s` from a fresh shell and confirm `Built application:` and its SHA-256 are printed.

## Suggested articles

- [Unsigned Squirrel.Windows installer](../delivery/squirrel-installer.md)
- [Design-reference parity](../design/design-parity.md)
