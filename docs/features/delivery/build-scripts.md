# One-click build and installer scripts

Feature id: `build-scripts` · Category: Build, install and update

## Behaviour

`build.bat` (silent with `/s`) resolves the pinned toolchain through `download-dependencies.bat`, exports the MSVC x64 environment itself, configures with CMake and Ninja, builds, installs to `stage\app` and reports the executable hash. `build-installer.bat` produces the verified Squirrel package from that stage.

## Configuration

The documentation website records its own build-start timestamp once in `build-provenance.json`, together with its exact source and a version-plus-source build identity. The front screen displays that identity and build update time. Product release provenance and installer downloads retain their separate recorded release time. Both producer and consumer validate the website record; a release-origin timestamp is not accepted as website build provenance. `scripts/test-site-build-provenance.mjs` rejects mismatched identity, invalid dates, wrong provenance origin and every missing field. Native application provenance has a separate acceptance boundary.

`QT_ROOT_DIR` and `KPXC_VCPKG_ROOT` override the user-scoped toolchain locations.

`build.bat /s -WithGuiTests` includes the native GUI regression executables through
the same root build route. The switch enables CMake `WITH_GUI_TESTS` only while
`WithTests` is enabled. Ordinary builds explicitly disable GUI tests, including
when a reused cache previously enabled them. Building these targets does not run
them, drive a real interface or establish native acceptance.

`QT_DEPLOY_ROOT_DIR` optionally selects a separate Qt deployment installation while compilation retains `QT_ROOT_DIR`. Before executing its tools, the builder compares every relative file path and SHA-256 in `bin`, `plugins`, `qml` and `translations`, including hidden files, against the compilation installation. Linked/reparse paths are rejected and Qt versions must match. The selected `qtpaths.exe` is placed first on the child PATH, and the selected `windeployqt.exe` is passed explicitly to every CMake configure so a stale cache cannot select another deployment tool. These path checks do not eliminate a hostile concurrent filesystem rename race.

This explicit override can recover a host-specific short-name alias collision without moving a shared installation. First preserve the original installation, create a task-owned full copy in a new path, compare the complete file inventory and hashes, and verify each destination path survives the Win32 short-name/long-name round trip unchanged. Set the override only for that validated copy. The builder does not automatically diagnose collisions or provision this recovery copy.

## Website-only build

Run `build.bat /s -WebsiteOnly` to install the locked website dependencies and
produce `site/dist` through the real root entrypoint. This mode branches before
native toolchain preparation, does not build or install the desktop application,
and does not launch a browser. Native build options do not change this mode.

The product-owned `scripts/website-node.manifest.json` pins the official Node.js
24.19.0 Windows x64 portable archive and executable SHA-256. Missing Node is
downloaded over HTTPS from the exact official URL, checked before extraction,
and installed in the current user's local application-data toolchain directory
without administrator rights. Valid caches are reused. An invalid existing cache
is retained and causes failure. `KPXC_WEBSITE_NODE_ROOT` can select an existing
portable installation, but its executable size, executable hash and bundled npm
version must match the manifest. This cache check does not independently hash
every npm file or eliminate concurrent filesystem replacement races.

The mode requires Git and a clean committed source tree. It invokes the pinned
Node executable directly for `npm ci --no-audit --no-fund` and `build.mjs`, and
propagates the exact nonzero child exit through `build.bat`. Existing release
metadata validation in the website builder still requires its supported GitHub
CLI access. Linked output, receipt and selected tool paths are rejected. The
website builder retains its own release and source-manifest validation.

Each execution writes an ignored `artifacts/website-build/execution-<id>.json`
receipt when its receipt directory can be established safely. It records start
and end times, source revisions and clean-state observations, package-lock and
tool hashes, the actual child exit, and every generated output file's relative
path, byte count and SHA-256. Successful output must carry matching
`build-provenance.json` source identity. Failures retain a failed receipt rather
than advertising stale output as successful. Download staging and disposable
fixture directories are retained for diagnosis.

`scripts/test-website-build.ps1` exercises real child exit propagation, rejected
alternate-stream and junction paths, dirty and changed source, failure receipts
and exact output hashes with simulated bootstrap/build transport. These focused
fixtures do not constitute an actual website build, fresh-environment bootstrap,
browser acceptance or deployment proof. Those receipts remain separate.

## Failure modes

Historical failures included missing MSVC environment headers and a readline
pkg-config include path containing spaces. The current root build passed from a
checkout path containing spaces; those older failures are retained as history,
not a current prohibition on such paths. Toolchain, configure, compile, deployment
and provenance failures propagate through the root entrypoint.

## Security considerations

Never installs secrets or a signing certificate; per-process execution policy bypass only.

## Verification

Run `build.bat /s` from a fresh shell and confirm `Built application:` and its SHA-256 are printed.

The exact root `build.bat /s` passed at
`cdb8862bd2637b30464abae8ad13888885def592`, with independently validated staged
provenance and executable SHA-256
`d57e0e69e805d59596254d69443cb88e1e0a87abc84c0925e537ec7220b4c0c6`.
It used the validated Qt deployment-copy override. Its GUI test option was off.
The new `-WithGuiTests` source path is independently reviewed and PowerShell syntax
validation passes, but its exact root run and capture-language fixture remain pending.
Native tests, real interface behavior and fresh-machine acceptance are separate verdicts.

## Suggested articles

- [Unsigned Squirrel.Windows installer](../delivery/squirrel-installer.md)
- [Design-reference parity](../design/design-parity.md)
