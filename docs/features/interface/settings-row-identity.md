# Settings row identity and live search

Settings Hub rows use a persistent identity built from the settings surface and the symbolic configuration key, for example `settings/general/config/SingleInstance`. The identity does not use the configuration enumeration's numeric position, section title, translated label, icon, or current value. Command rows carry explicit action identities such as `settings/general/command/reset-settings`.

The row's object name is `specRow_` followed by that identity. Its switch uses the corresponding `specSwitch_` identity. Rows sharing an icon therefore remain independently addressable by appearance editors, keyboard tooling, and later command-palette descriptors. Shortcut rows use the registered action's object name; an unnamed action remains visible with an explicitly `unidentified-` fallback and does not claim a durable action identity.

Spec-sheet callers may supply an explicit stable key. Existing callers without one retain their section/label lookup API, while their object names include the page to avoid cross-page collisions. Such legacy callers do not acquire localization-independent identities automatically.

## Existing appearance overrides

The Settings Hub inventories legacy icon-row and section/label-switch aliases after building its rows. A legacy override is copied to a stable identity only when that alias resolves to exactly one row or switch in the inventoried sheet and the stable identity has no newer override. Shared aliases are retained without guessing which row owns the customization. The original entries remain available for other surfaces and rollback. Migration neither deletes ambiguous entries nor replaces an existing stable override.

Each resolved target is recorded in the versioned local `GUI/ElementOverrideMigrations` state independently of its current style. Resetting that element therefore cannot reapply the retained legacy source when the hub reopens or the application restarts. Existing customized targets also consume their migration, so a later reset stays at the default. Element reset and appearance reset do not clear these markers; an explicit full configuration reset may clear them together with the original overrides. Invalid migration state disables migration rather than guessing, and ordinary appearance saves preserve that invalid state for recovery.

## Current values and search

Refreshing a setting publishes its displayed value, provenance, and full current-value search metadata as one update. Long paths remain searchable even when their display pill shortens them. Old user values disappear from the row's search metadata when replaced. Plain search is case insensitive; regular expressions operate on the actual case-preserving text with the selected flags. Each page immediately reapplies its active filter when its metadata changes, including when configuration changes outside that page.

The full search text also includes the option label, section, explanation, displayed value, and current provenance. A compiled default mentioned by the current provenance remains legitimate searchable information. Unchanged metadata does not repeatedly rerun the page filter.

## Verification and descriptor seam

`TestMaterialShellResponsive` contains focused regressions for same-icon uniqueness, localization-independent explicit keys, one-notification state publication, plain and regex searches after value replacement, unshortened live Config values, and non-conflating appearance migration. The migration regression resets the stable override, reconstructs the mapping, and starts a fresh process against the persisted synthetic configuration to verify that reset survives restart while the original legacy entry remains unchanged. Native execution remains required before reporting these regressions passed.

`SettingsHub::configurationRowIdentity`, `SpecSheetRow::key`, `currentValue`, and `searchableText` provide the next command-palette descriptor seam. A descriptor can retain the stable identity and original Config binding while reading live search metadata. This foundation does not itself implement a palette redesign or add missing settings workflows.
