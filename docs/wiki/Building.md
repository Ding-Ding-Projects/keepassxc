# Building on Windows x64

The active build uses MSVC x64 and Qt 6.8.3. Other operating systems and compilers are outside the current delivery scope. Use the repository's committed wrappers; the pinned dependency inventory and bootstrap route are the source of truth.

```powershell
.\download-dependencies.bat /s
.\build.bat /s
.\build-installer.bat /s -Version 2.8.13202
```

The version above is an example local candidate. Choose the intended candidate version explicitly; never rename an old executable or package to claim a new version. These commands build and package, and do not authorize installation over an existing user installation.

For external owned output directories, use `scripts/build-windows.ps1` or `scripts/build-squirrel.ps1` with their documented `BuildDirectory`, `StageDirectory` and `ArtifactDirectory` parameters. The wrappers require committed source, validate path ownership, preserve previous generations, stage the selected compiler's MSVC runtime DLLs, and bind the application version and file hashes to a provenance receipt.

`-UseExistingStage` accepts only a stage whose recorded commit, version, inventory and executable match the requested candidate. It is not a way to relabel a previous build.

A case-only difference in a cached compiler path is retained only after filesystem identity proof. A genuinely different compiler requires a new build directory. When invoking CMake directly from PowerShell, quote the complete version argument, for example `"-DOVERRIDE_VERSION=2.8.13202"`, and reconfigure for each candidate commit so generated provenance remains current.

## Packaging and verification boundaries

The supported installer is genuine unsigned Squirrel.Windows: `Setup.exe`, `RELEASES`, a full `.nupkg`, and deltas when generated. Unknown Publisher or SmartScreen warnings may appear. Package hashes prove integrity agreement, not publisher authenticity.

Local package-byte validation and focused tests are separate from installation and update acceptance. Installation must use a disposable normal user or VM, with the supported headless route. Changing `LOCALAPPDATA` is not isolation, and a system-context install is not normal-user proof.

See the maintained [build scripts article](https://github.com/Ding-Ding-Projects/keepassxc/blob/main/docs/features/delivery/build-scripts.md), [packaging safety article](https://github.com/Ding-Ding-Projects/keepassxc/blob/main/docs/features/delivery/packaging-path-safety.md), and [current verification record](https://github.com/Ding-Ding-Projects/keepassxc/blob/main/docs/features/delivery/repair-verification-2026-09.md).
