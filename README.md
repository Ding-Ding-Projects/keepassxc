# <img src="https://keepassxc.org/assets/img/keepassxc.svg" width="40" height="40"/> KeePassXC — Material

A fork of [KeePassXC](https://keepassxc.org) whose interface has been rebuilt from the ground up in **Material Design 3**.

The cryptography, the KDBX format handling, the browser and SSH integrations, and the command line tool are upstream KeePassXC and are deliberately untouched. What changed is everything you look at: the stock Qt widget styling, the toolbar-and-tabs shell, and the per-widget stylesheets have been removed and replaced by a single token-driven design system.

> This is a personal fork. For the official, supported password manager, use [keepassxreboot/keepassxc](https://github.com/keepassxreboot/keepassxc).

## The interface

The window is a Material 3 shell: an 88 px navigation rail, a 64 px top app bar, a database tab strip, and one of five destinations.

| Destination | What it holds |
| --- | --- |
| **Vault** | Group sidebar with tag chips, a pill search bar, the entry list, and a detail pane with credentials, strength meter, and a live TOTP ring |
| **Reports** | Password health, breach and reuse findings, and database statistics as stat cards |
| **History** | Local, Git-backed revision history for the database, with diff and restore |
| **Changelog** | Every released version, searchable and date-filterable, exportable to Markdown |
| **Settings** | Appearance, language, behaviour, and integrations — plus full spec sheets for every application setting |

### Appearance is a runtime setting, not a build flag

Everything the interface draws resolves through one design system, so the whole application restyles live:

- **Theme** — light or dark, or follow the operating system.
- **Seed colour** — KeePassXC blue, baseline purple, vault green, or signal amber. The seed drives the primary and container roles; surfaces, outlines, and status colours flip with the light/dark family.
- **Density** — compact, comfortable, or spacious, changing every list, tree, and table row height (40 / 52 / 64 px).
- **Interface font** — family, size scale, and weight, with a CJK-safe fallback.

No widget hard-codes a colour. They ask the theme for a semantic role, so a seed or density change repaints the application without a restart.

### Cross-cutting behaviour

- **Regex builder** — a full guided builder (character classes, anchors, quantifiers, groups, flags, sample text, live matches and captures) reachable from *every* search bar. Plain-text search stays the default; regex is an explicit opt-in, and query, pattern, flags, and mode stay in sync both ways.
- **Non-blocking notifications** — informational, success, and progress messages are snackbars anchored bottom-centre, never modal dialogs. Modals are reserved for decisions you must make: confirmations, destructive gates, and credential steps. Dismissed notifications stay reviewable in the notification centre.
- **Language modes** — English, playful Hong Kong Cantonese, or bilingual, with an independent 1–5 humour slider per language. The humour styles the *voice* only: what happened, what is affected, and what is irreversible stay exact at every level.
- **External editor** — detect installed editors and open the database folder or a selected file in your choice of them.

### Accessibility

Keyboard reachability, visible focus, correct roles and names, contrast, and reduced-motion respect are treated as defects when broken, not as polish. So is visual clipping and mis-sizing at 100/125/150/200 % display scale and in the longest localized strings.

## Password manager features

Inherited from upstream KeePassXC and fully intact:

- Create, open, and save KDBX 3 and KDBX 4 databases; everything is encrypted at rest
- Entries organised into groups, with tags, attachments, custom attributes, and field references
- Password and passphrase generator
- TOTP storage and generation
- YubiKey / OnlyKey challenge-response
- Auto-Type into any application
- Browser integration for Chrome, Firefox, Edge, Chromium, Vivaldi, Brave, and Tor Browser, including passkeys
- Import from CSV, 1Password, Bitwarden, Proton Pass, and KeePass1
- Export to CSV, XML, and HTML
- SSH Agent integration and FreeDesktop.org Secret Service
- Command line interface (`keepassxc-cli`)
- Additional ciphers: Twofish and ChaCha20

See [CHANGELOG.md](CHANGELOG.md) for release history and [docs/topics/KeyboardShortcuts.adoc](./docs/topics/KeyboardShortcuts.adoc) for shortcuts.

## Building

Requires **Qt 6.6+**, **CMake 3.16+**, and a C++17 compiler. Native dependencies (Botan 3, minizip, libqrencode, zlib, readline) are declared in [`vcpkg.json`](vcpkg.json) and resolved by vcpkg.

```bash
git clone --depth 1 https://github.com/microsoft/vcpkg.git "$HOME/vcpkg"
"$HOME/vcpkg/bootstrap-vcpkg.sh" -disableMetrics
```

Then configure and build:

```bash
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE="$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake"
cmake --build build
```

On Windows use `bootstrap-vcpkg.bat` and point `CMAKE_PREFIX_PATH` at your Qt installation, for example `C:/Qt/6.8.3/msvc2022_64`.

Longer instructions live in [INSTALL.md](./INSTALL.md).

## Layout of the UI code

```
src/gui/material/
  MaterialTheme.*        colour roles, seeds, density, type scale, palette
  MaterialStyleSheet.*   the single generated stylesheet for stock Qt widgets
  MaterialStyle.*        QProxyStyle for what a stylesheet cannot express
  MaterialIcons.*        Material Symbols names resolved to bundled SVGs
  Material<Component>.*  navigation rail, app bar, tab strip, buttons, chips,
                         switches, cards, search bars, snackbars, overlays,
                         item delegates, screens, and spec sheets
```

Adding a surface means composing these components and asking the theme for roles — not writing another stylesheet.

## Contributing

Bug reports and ideas for the *upstream* password manager belong in the [KeePassXC issue tracker](https://github.com/keepassxreboot/keepassxc/issues). Issues specific to this fork's interface belong here.

Contributors adhere to the project's [Code of Conduct](CODE-OF-CONDUCT.md). See [CONTRIBUTING](.github/CONTRIBUTING.md) for the upstream workflow.

## Generative AI

Substantial parts of this fork's Material interface were written with agent-assisted development. All submissions go through review regardless of workflow.

## License

KeePassXC code is licensed under GPL-2 or GPL-3. The Material interface layer added by this fork carries the same licence. Third-party file licensing is detailed in [COPYING](./COPYING).
