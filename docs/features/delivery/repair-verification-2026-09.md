# September 2026 repair verification

The native and website repairs are integrated into `main` at `95e580d6d3ef83007dbeb2935e5b7495516790ca`. This record distinguishes source integration, local verification, publication, and installed behavior.

## Native checks

At `5c8066aec083cdc28f373faf2cb977afaed0fe78`, the production application, CLI and proxy compiled with MSVC x64 and Qt 6.8.3. The focused results were:

| Suite | Passed | Failed | Skipped |
| --- | ---: | ---: | ---: |
| testupdatecheck | 12 | 0 | 0 |
| testmaterialtitlebar | 8 | 0 | 0 |
| testmaterialtabs | 6 | 0 | 0 |
| Selected testgui cases | 14 | 0 | 0 |

Counts include suite initialization and cleanup. The selected GUI process exited with code zero. They cover delayed TOTP ownership, timer rollover, database replacement and teardown, native title-bar hit testing, tab event ownership, and update request lifecycle. They are not the complete project test suite and do not establish actual drag gestures or installed updates.

The reviewed candidate `4530d54111251b522c6df9d054a2cef026ecc6d7` then passed the focused repairs in a fresh verification checkout: `testmaterialreports` passed 5/0/0 in 20 ms, `testupdatecheck` passed 101/0/0 in 47 ms, `testmaterialtitlebar` passed 8/0/0 in 98 ms, and `testmaterialtabs` passed 6/0/0 in 8 ms. Production linking exited 0 with executable SHA-256 `1F23A6697F08C07E6CB5EECF0D916D1E1F1B5C226C76109191318BFA57E864D1`. These checks remain bounded component and controller evidence. They do not establish end-to-end native window movement, full MainWindow content ownership, installation, or update execution.

## Local package

The combined candidate `877f4434eea7d8411837b24ede6aaf6126fa4d07` produced genuine unsigned Squirrel.Windows version `2.8.13202` locally. The committed package verifier checked version identity, `RELEASES`, package structure, executable bytes, and ten declared MSVC runtime DLLs.

| Output | Bytes | SHA-256 |
| --- | ---: | --- |
| Setup.exe | 74414080 | 942C63D9033F89A49B850543793BCD85358027EAA6210F89CBE0F6DC57C937A2 |
| Full package | 73580518 | C3A8B0D6CE5B32094287AAD8EC205AD3DEFA02F5F8187CEFD40D829DE5C32570 |

The setup signature verdict was `NotSigned`. No delta was generated for this fresh output directory. This package was not installed during these checks. Its version is a local candidate version, not a claim that the final published release uses that number.

Packaging safety passed 52 focused checks, including interrupted-process recovery. An actual warm-cache compiler-path casing change caused CMake to reset without retaining the toolchain configuration. A complete fresh configuration recovered the build. The dedicated correction at `cbbd85b9a8d8f0110a16028f2fca8a4102ba9e36` retains equivalent cached compiler spellings only after Win32 file-identity proof. All 55 focused checks passed. A real same-cache configure retained the requested version and toolchain, exited zero, and did not reset the cache.

## Website

The public deployment of `7bf379fd` succeeded in run [34154701931](https://github.com/Ding-Ding-Projects/keepassxc/actions/runs/34154701931). The final Squirrel delivery run [34154701913](https://github.com/Ding-Ding-Projects/keepassxc/actions/runs/34154701913) also completed successfully.

The release projector requires exact release-tag, build and installer-receipt source identity; a `NotSigned` installer verdict; matching package hashes and byte counts; and exact project-owned URLs. Nineteen invalid-input mutations and missing-receipt rejection passed. Headless interaction confirmed language/theme persistence, narrow layout observations, explicit navigation focus transfer and invalid regex clearing. The regex worker terminates after 250 ms. These are focused observations, not a full accessibility, language, display-scale or canonical-feature acceptance matrix.

## Final published release

Release [`v2.8.22101`](https://github.com/Ding-Ding-Projects/keepassxc/releases/tag/v2.8.22101) is non-draft, stable, and targets `95e580d6d3ef83007dbeb2935e5b7495516790ca`. It contains `Setup.exe`, `RELEASES`, the full package, `artifact-receipt.json`, `build-provenance.json`, `update-manifest-v1.json`, and the required dim-sum photo. The receipt reports `NotSigned`, `Setup.exe` is 73,753,600 bytes with SHA-256 `bf600765a8517e002b42571f141673caf23f5778bf3aa36fe47924d605710af9`, and the full package is 72,919,158 bytes with SHA-256 `75c83803828ec6af2600114f71a9746898f613f90f89a5f3a4db0fbb734dc821`. The verified workflow timing is 00:38:38, from `2026-09-07T22:02:59Z` through `2026-09-07T22:41:37Z`. The finalizer run [34167536082](https://github.com/Ding-Ding-Projects/keepassxc/actions/runs/34167536082) marked this numeric release as `Latest`, and Pages run [34167536072](https://github.com/Ding-Ding-Projects/keepassxc/actions/runs/34167536072) succeeded.

## Remaining acceptance

- Actual title-bar, restored-window, tab, entry and group drag gestures.
- Older-to-newer update download, staging, explicit restart and unsaved-work protection after the normal-user package install proof.
- Full applicable local suites, per-click evidence and complete per-surface feature inventory. The current inventory remains **1 of 172 rows green**.
- Installed-user behavior and older-to-newer update execution.
- End-to-end native title-bar and content-drag proof through the complete MainWindow.
- Backup-verified, ancestry-proven cleanup after active work is complete.

Windowless Windows Sandbox system execution was available, and a disposable normal-user session later installed `v2.8.21901` with exit code 0, reporting the expected version and ten app-local MSVC runtime DLLs. The newer `v2.8.22101` package carries the exact `95e580d6` provenance above. A background input probe used prohibited cursor APIs and was stopped; its effect on the visible cursor was not established. Those probes are not accepted as drag evidence. The minimize regression at `6d7f344f` is preserved on its branch, but its fresh compile stopped in the Botan dependency before the new assertions were compiled.
