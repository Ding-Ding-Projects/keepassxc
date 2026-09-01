# One-click build and installer scripts

Feature id: `build-scripts` · Category: Build, install and update

## Behaviour

`build.bat` (silent with `/s`) resolves the pinned toolchain through `download-dependencies.bat`, exports the MSVC x64 environment itself, configures with CMake and Ninja, builds, installs to `stage\app` and reports the executable hash. `build-installer.bat` produces the verified Squirrel package from that stage.

## Configuration

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
