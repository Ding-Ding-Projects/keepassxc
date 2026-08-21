# <img src="https://keepassxc.org/assets/img/keepassxc.svg" width="40" height="40"/> KeePassXC — Material

> **Installer transition:** this fork now targets unsigned Squirrel.Windows packages as its sole
> supported installation and update format. The generated executables are intentionally unsigned
> and may display Unknown Publisher or SmartScreen warnings.

Run `build.bat` for the native application, `build-installer.bat` for verified `Setup.exe`,
`RELEASES`, and full-package output, or `download-dependencies.bat` to prepare the pinned user-scoped
toolchain. Each accepts `/s` or `--silent`.

A **Windows-only** fork of [KeePassXC](https://keepassxc.org) whose interface is being rebuilt in
**Material Design 3**.

The cryptography, the KDBX format handling, the browser and SSH integrations and the command line
tool are upstream KeePassXC and are deliberately untouched. What changed is what you look at: the
stock Qt styling — `BaseStyle` (4 860 lines), `LightStyle`, `DarkStyle`, `phantomcolor` and all
four `.qss` sheets — has been deleted and replaced by one token-driven design system.

> This is a personal fork and it is unfinished — see [Status](#status) before you rely on it. For
> the official, supported password manager, use
> [keepassxreboot/keepassxc](https://github.com/keepassxreboot/keepassxc).

## Windows only

Linux support has been removed from this fork, not merely disabled. Gone from the tree: the
freedesktop.org Secret Service server, the XDG desktop portals, D-Bus, the X11 and Wayland
auto-type backends, PolKit quick unlock, `nixutils`, and Snap / Flatpak / AppImage packaging. The
CI builds and tests Windows only. macOS sources are still present but are neither built nor tested
here.

If you need Linux or macOS, use upstream.

## The interface

The window is a Material 3 shell: an 88 px navigation rail, a 64 px top app bar, a database tab
strip, and a five-destination stack. `Ctrl+Shift+P` opens a command palette listing all 218
`QAction`s with their menu path and shortcut.

| Destination | State |
| --- | --- |
| **Vault** | **Still the stock three-pane widget.** Restyled by the Material stylesheet, but the group tree / entry table / preview layout is upstream's. The Material vault screen is written and not yet wired — see [Status](#status). |
| **Reports** | Material screen — password health, breach and reuse findings, database statistics as stat cards |
| **History** | Material screen — local Git-backed revision history, with diff and restore |
| **Changelog** | Material screen — every released version, searchable and date-filterable, exportable to Markdown |
| **Settings** | Material screen — appearance, language, behaviour and integrations, plus spec sheets for individual settings |

### Appearance is a runtime setting, not a build flag

Everything the interface draws resolves through one design system, so the whole application
restyles live:

- **Theme** — light or dark, or follow Windows.
- **Seed colour** — KeePassXC blue, baseline purple, vault green or signal amber. The seed drives
  the primary and container roles; surfaces, outlines and status colours flip with the light/dark
  family.
- **Density** — compact, comfortable or spacious, changing every list, tree and table row height
  (40 / 52 / 64 px).
- **Interface font** — family, size scale and weight, with a CJK-safe fallback.

No widget in `src/` hard-codes a colour. They ask the theme for a semantic role, so a seed or
density change repaints the application without a restart.

### Cross-cutting behaviour

- **Regex builder** — a guided builder (character classes, anchors, quantifiers, groups, flags,
  sample text, live matches and captures) reachable from *every* search bar. Plain-text search
  stays the default; regex is an explicit opt-in, and query, pattern, flags and mode stay in sync
  both ways.
- **Non-blocking notifications** — informational, success and progress messages are snackbars
  anchored bottom-centre, never modal dialogs. Modals are reserved for decisions you must make:
  confirmations, destructive gates and credential steps. Dismissed notifications stay reviewable in
  the notification centre.
- **Language modes** — English, playful Hong Kong Cantonese, or bilingual, with an independent 1–5
  humour slider per language. The humour styles the *voice* only: what happened, what is affected
  and what is irreversible stay exact at every level.
- **Passkey clipboard transfer** — passkeys can be copied and pasted as
  `keepassxc-passkey:v1:<base64>`. Copying a private key raises a warning that cannot be suppressed
  and defaults to Cancel, because base64 is encoding, not encryption: anything you paste that
  string into can read the key.

### Accessibility

Keyboard reachability, visible focus, correct roles and names, contrast and reduced-motion respect
are treated as defects when broken, not as polish. So is visual clipping and mis-sizing at
100/125/150/200 % display scale and in the longest localized strings.

## Password manager features

Inherited from upstream KeePassXC and intact:

- Create, open and save KDBX 3 and KDBX 4 databases; everything is encrypted at rest
- Entries organised into groups, with tags, attachments, custom attributes and field references
- Password and passphrase generator
- TOTP storage and generation
- YubiKey / OnlyKey challenge-response
- Auto-Type into any application
- Browser integration for Chrome, Firefox, Edge, Chromium, Vivaldi, Brave and Tor Browser,
  including passkeys — extensions register themselves per-user on first run
- Windows Hello quick unlock
- Import from CSV, 1Password, Bitwarden, Proton Pass and KeePass1
- Export to CSV, XML and HTML
- SSH Agent integration
- Command line interface (`keepassxc-cli`)
- Additional ciphers: Twofish and ChaCha20

Removed with Linux: freedesktop.org Secret Service.

See [CHANGELOG.md](CHANGELOG.md) for release history and
[docs/topics/KeyboardShortcuts.adoc](./docs/topics/KeyboardShortcuts.adoc) for shortcuts.

## Building

Requires **Visual Studio 2022+** with the C++ workload, **Qt 6.8 msvc2022_64**, **CMake 3.16+** and
**Ninja**. Native dependencies (Botan 3, minizip, libqrencode, zlib, readline) are declared in
[`vcpkg.json`](vcpkg.json) and resolved by vcpkg.

From an **x64 Native Tools Command Prompt**:

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/msvc2022_64 -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake
```

```bash
cmake --build build
```

Longer instructions, including the test invocations, live in [INSTALL.md](./INSTALL.md).

### Screenshots come out black — this is not a bug in the app

KeePassXC calls `SetWindowDisplayAffinity(WDA_EXCLUDEFROMCAPTURE)` on its top-level windows. The
window is plainly visible on your monitor and fully hit-testable, but `PrintWindow`, `BitBlt` and
`CopyFromScreen` all return black, and a desktop grab of its rectangle returns *the windows behind
it*. Pass `--allow-screencapture` when you need to record or screenshot it. This is upstream
behaviour and it cost this project several hours of misdiagnosis.

## Layout of the UI code

```
src/gui/material/
  MaterialTheme.*        colour roles, seeds, density, type scale, palette
  MaterialStyleSheet.*   the single generated stylesheet for stock Qt widgets
  MaterialStyle.*        QProxyStyle for what a stylesheet cannot express
  MaterialIcons.*        Material Symbols names resolved to bundled SVGs
  MaterialShell.*        the window interior: rail, app bar, tab strip, stack
  Material<Component>.*  buttons, chips, switches, cards, search bars,
                         snackbars, overlays, item delegates, screens and
                         spec sheets
```

Adding a surface means composing these components and asking the theme for roles — not writing
another stylesheet.

## Status

Honest state of the rewrite:

- **Done** — stock styling deleted; theme and token engine; the shell and command palette; 57 of 73
  `.ui` surfaces on Material widgets; Reports, History, Changelog and Settings screens; passkey save
  path verified against a real KDBX round trip.
- **Not done** — the vault destination is still upstream's three-pane widget. `MaterialVaultScreen`
  exists in the tree but is not compiled and not wired to `GroupModel` / `EntryModel`. This is the
  largest remaining gap.
- **Drafts** — the Reports, History and Changelog feeds landed but were never finished or reviewed.

[HANDOFF.md](./HANDOFF.md) carries the full picture, including known defects and two things that
were reported as fact during this work and turned out to be wrong.

## Contributing

Bug reports and ideas for the *upstream* password manager belong in the
[KeePassXC issue tracker](https://github.com/keepassxreboot/keepassxc/issues). Issues specific to
this fork's interface belong here.

Contributors adhere to the project's [Code of Conduct](CODE-OF-CONDUCT.md). See
[CONTRIBUTING](.github/CONTRIBUTING.md) for the upstream workflow.

## Generative AI

Substantial parts of this fork's Material interface were written with agent-assisted development.
All submissions go through review regardless of workflow.

## License

KeePassXC code is licensed under GPL-2 or GPL-3. The Material interface layer added by this fork
carries the same licence. Third-party file licensing is detailed in [COPYING](./COPYING).
