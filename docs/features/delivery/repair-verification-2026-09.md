# September 2026 repair verification

The native and website repairs were integrated into `main` at `614c58fca2cc0f86c4270d5ef83cbaedf1d9731b`. This record distinguishes source integration, local verification, publication, and installed behavior.

## Native checks

At `5c8066aec083cdc28f373faf2cb977afaed0fe78`, the production application, CLI and proxy compiled with MSVC x64 and Qt 6.8.3. The focused results were:

| Suite | Passed | Failed | Skipped |
| --- | ---: | ---: | ---: |
| testupdatecheck | 12 | 0 | 0 |
| testmaterialtitlebar | 8 | 0 | 0 |
| testmaterialtabs | 6 | 0 | 0 |
| Selected testgui cases | 14 | 0 | 0 |

Counts include suite initialization and cleanup. The selected GUI process exited with code zero. They cover delayed TOTP ownership, timer rollover, database replacement and teardown, native title-bar hit testing, tab event ownership, and update request lifecycle. They are not the complete project test suite and do not establish actual drag gestures or installed updates.

## Local package

The combined candidate `877f4434eea7d8411837b24ede6aaf6126fa4d07` produced genuine unsigned Squirrel.Windows version `2.8.13202` locally. The committed package verifier checked version identity, `RELEASES`, package structure, executable bytes, and ten declared MSVC runtime DLLs.

| Output | Bytes | SHA-256 |
| --- | ---: | --- |
| Setup.exe | 74414080 | 942C63D9033F89A49B850543793BCD85358027EAA6210F89CBE0F6DC57C937A2 |
| Full package | 73580518 | C3A8B0D6CE5B32094287AAD8EC205AD3DEFA02F5F8187CEFD40D829DE5C32570 |

The setup signature verdict was `NotSigned`. No delta was generated for this fresh output directory. This package was not installed during these checks. Its version is a local candidate version, not a claim that the final published release uses that number.

Packaging safety passed 52 focused checks, including interrupted-process recovery. An actual warm-cache compiler-path casing change caused CMake to reset without retaining the toolchain configuration. A complete fresh configuration recovered the build; a dedicated wrapper correction remains in progress.

## Website

The public deployment of `614c58fc` succeeded in run [34090392214](https://github.com/Ding-Ding-Projects/keepassxc/actions/runs/34090392214). Reading the live provenance returned that exact commit. At verification time, downloads still correctly referred to published `v2.8.13201` while the next installer delivery was running.

The release projector requires exact release-tag, build and installer-receipt source identity; a `NotSigned` installer verdict; matching package hashes and byte counts; and exact project-owned URLs. Nineteen invalid-input mutations and missing-receipt rejection passed. Headless interaction confirmed language/theme persistence, narrow layout observations, explicit navigation focus transfer and invalid regex clearing. The regex worker terminates after 250 ms. These are focused observations, not a full accessibility, language, display-scale or canonical-feature acceptance matrix.

## Remaining acceptance

- Actual title-bar, restored-window, tab, entry and group drag gestures.
- Installation in a disposable normal user session, followed by older-to-newer update download, staging, explicit restart and unsaved-work protection.
- Full applicable local suites, per-click evidence and complete per-surface feature inventory. The current inventory remains **1 of 172 rows green**.
- Final release asset, timing, tag, download and installed-behavior verification.
- Backup-verified, ancestry-proven cleanup after active work is complete.

Windowless Windows Sandbox system execution was available, but its normal-user execution returned `0x80070520` because no login session existed. No host user installation was substituted. A background input probe used prohibited cursor APIs and was stopped; its effect on the visible cursor was not established. Those probes are not accepted as drag evidence.
