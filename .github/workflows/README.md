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

The release job uses the first available token in this order: `RELEASE_TOKEN`, `ORG_TOKEN`, then the built-in workflow token. No token is printed. Release tags use the monotonic `v0.0.<run_number>.<run_attempt>` form and are never recycled.
