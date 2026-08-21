# GitHub Actions workflows

## Material Squirrel Build and Release

`material-release.yml` is the Windows-only build, packaging, and publication workflow. It runs on every branch push and by manual dispatch. Default-branch and manual runs publish one unique, non-draft release.

The workflow calls the repository's supported installer entry point directly:

```bat
build-installer.bat /s
```

That script bootstraps the pinned toolchain, builds and stages the native Qt application through `cmake --install`, deploys the Qt runtime with `windeployqt`, creates the Squirrel.Windows package, and verifies the result. The workflow does not duplicate those commands.

Required assets are `Setup.exe`, `RELEASES`, exactly one full `.nupkg`, any compatible generated delta package, `artifact-receipt.json`, `build-provenance.json`, and `update-manifest-v1.json`.

Squirrel.Windows is the sole installer and update format. CPack, WiX, NSIS, MSI, and portable ZIP release routes are absent and protected by `scripts/check-squirrel-only-packaging.ps1`.

Code signing is permanently disabled. The verifier requires `Setup.exe` to report `NotSigned`, and release notes state that Windows may show Unknown Publisher or SmartScreen warnings.

GitHub Actions performs building, packaging, evidence collection, and publication only. It runs no tests or lint and makes no quality verdict; local verification is reported separately.

The package job derives one semantic version from `run_number * 100 + run_attempt`. It starts in the `2.8` series and carries overflow from the patch into minor and then major, keeping every Windows PE component at or below 65535. That exact value is passed through `build-installer.bat` and the native CMake `OVERRIDE_VERSION`, then reused by the executable PE FileVersion/ProductVersion, full package, `RELEASES`, update manifest, release tag, and title. The arithmetic is monotonic across runs and reruns, and every value remains a three-part semantic version.

The release job uses the first available token in this order: `RELEASE_TOKEN`, `ORG_TOKEN`, then the built-in workflow token. No token is printed. Release tags use `v<package-version>` and are never recycled.
