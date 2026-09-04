# Roadmap

## Windows-only foundation

- [x] Rename the fork default branch to `main` without changing upstream or Transifex resource names.
- [x] Repair empty browser native-messaging registrations when automatic extension setup is enabled.
- [x] Reject non-Windows, non-MSVC, and non-x64 configurations at CMake configure time.
- [x] Add pinned root dependency, native build, and Squirrel.Windows installer entry points.
- [x] Validate `RELEASES`, hashes, package paths, required payload, and unsigned setup state.
- [x] Remove audited macOS source, bundle, auto-type, quick-unlock, icon, compiler-probe, and Unix manpage paths.
- [ ] Remove WiX/CPack only after the verified real Squirrel package also installs and launches successfully.

## Material UI rewrite

- [x] Repair the new-entry crash and keep TOTP changes on the editor-owned attributes working copy.
- [x] Keep screen-capture permission changes from minimizing or hiding the application.
- [x] Make settings wheel scrolling work over content and contain the scrollbar at narrow sizes.
- [x] Add searchable passkey-import entry selection for groups with large entry counts.
- [x] Make Windows Hello quick unlock an explicit action instead of an automatic file-open prompt.
- [x] Create an encrypted local Git repository per database and restore deleted entries from saved KDBX snapshots.
- [x] Add direct TOTP setup to the entry editor and preserve TOTP alongside passkey credentials.
- [x] Remove the development-snapshot startup notification and its dead suppression control.
- [ ] Complete Settings destination parity; the current checkpoint adds real provenance/search/persistence foundations but still lacks focused compiled verification and the full canonical tab/anatomy migration.
- [x] Add the five responsive window-size classes and exact boundary tests.
- [x] Integrate responsive navigation, searchable compact bottom navigation, group-scope fallback, and an accessible narrow-layout detail sheet.
- [ ] Complete search registry, tab overflow, regex safety, appearance overrides, generated voice strings, and external-editor integration.
- [x] Add shared regex limits, high-risk shape refusal, zero-width safety, sample/match caps, and explicit result states.
- [x] Add stable hashed tab persistence identities, descriptor reconciliation, preferred order keys, and pin-state foundations.
- [x] Add explicit pin/unpin controls with persistent file-backed pins and session-only unsaved pins.
- [x] Replace the transient hidden-tab menu with a registered searchable all-tab material overlay.
- [x] Add stable-ID move commands that reorder the authoritative database tab widget and persist preferred order.
- [x] Centralize existing material search ownership and remove private Vault/History regex builders.
- [x] Register command-palette and notification-history searches and make both consumers mode/flag aware.
- [ ] Migrate every remaining dialog and auxiliary surface to the shared component system.
- [x] Add the first native History parity batch without changing append-only restore semantics.
- [x] Add the first native Changelog parity batch with bundled release coverage, composed date/regex filtering, rendered Markdown, and truthful commit provenance.
- [x] Bind every released Changelog entry to its exact tag commit and guard the catalog against missing or stale provenance.
- [x] Replace the plaintext History save log with an isolated local Git ledger and atomic redacted fingerprint transactions.
- [x] Serialize local History writers and preserve validated encrypted KDBX snapshots in the same append-only commits.
- [x] Add the first native Reports parity batch with truthful states, real category data, selection/export, regex filtering, accessibility, and responsive reflow.
- [x] Add the first native Appearance parity batch with typography persistence, element overrides, regex filtering, keyboard controls, and narrow reflow.
- [x] Capture every design reference and the built application at identical tuples, compare them, and hand-audit each row against Material Design 3 (`design/parity`).
- [x] List every entry recursively when the database root is selected, as the design does, with live add and remove.
- [x] Rename the Reports tiles and cards to the design's vocabulary (Health score, Breached, Needs work, Healthy).
- [x] Repair the remaining parity defects recorded in `design/parity/audits/*.json` (regex workbench, reports finding rows, OS caption bar, settings pages) and recapture; every audit lists zero open defects at `693367d1`.
- [x] Replace every stock combo box, slider and date field with searchable Material selects, Material 3 sliders and Material date fields with a calendar picker.
- [x] Draw a frameless Material title bar with its own window controls.
- [x] Add the per-element appearance editor with typography, colour, shape and preset tabs, the infinite colour picker with translator, contrast and rainbow, and preset export/import.
- [x] Give the vault its tag chips and detail card, History its detail card and CREATE badge, Reports its finding rows and exports, the regex builder its workbench and token blocks.
- [x] Consume the personal vocabulary file at the translation boundary in every language mode, accepting the canonical `entries` member.
- [x] Hide the legacy status bar under the Material shell and route progress through the notification host.
- [x] Show the running version, revision and exact updated-at time of that revision on the welcome screen, with an honest unavailable state.
- [x] Wrap the shared screen header onto extra rows when its actions and search bar would overflow.
- [x] Make the dim sum surprise a ten percent draw with no opt-out and retire the old toggle.
- [x] Bind the command palette to `Ctrl+Shift+F`.
- [ ] Give command palette results rich inline controls and exact-element teleport.
- [x] Capture a quick clipping matrix with the application's own widget probe; repair its findings (Reports header, segmented control) and rerun.
- [x] Run the full clipping matrix across six widths, three languages, two themes and four display scales, and repair every finding (three named records: widths 50 tuples, languages and themes 60 tuples at the expanded width, scales 30 tuples; 20 findings repaired; all three at 0 at `693367d1`).
- [ ] Turn every row of the fail-closed feature inventory (`docs/features/inventory.json`) green; `scripts/check-feature-inventory.mjs` currently reports 0/172.
- [x] Make the feature-inventory guard reject duplicate canonical rows and malformed row values instead of silently accepting or crashing on them.

## Installer and updater

- [x] Prevent Squirrel from launching helper executables and the bundled Visual C++ redistributable as install hooks.
- [x] Deduplicate repeated background update-failure notifications until state changes or the user retries.
- [x] Build and byte-verify a real unsigned `Setup.exe`, `RELEASES`, and full `.nupkg` from the native staged tree.
- [ ] Prove clean Squirrel installation and launch in an isolated Windows account or virtual machine.
- [x] Handle Squirrel install, updated, uninstall, obsolete, and first-run process arguments before ordinary UI startup.
- [x] Extend lifecycle handling beyond shortcuts to install-owned file associations, URI handling, and browser registration refresh.
- [ ] Replace the existing update checker with staged Squirrel states and user-controlled restart.
- [x] Wire non-blocking download/apply progress, persistent ready actions, deferred restart, and Squirrel process-start relaunch.
- [x] Remove the superseded modal update dialog after the non-blocking flow passed the full local suite.
- [x] Replace upstream release discovery with a bounded, versioned fork-owned Squirrel manifest and typed state/failure model.
- [x] Stream full packages with storage preflight, atomic finalization, SHA-256/SHA-1 validation, and bounded NuGet structure checks.
- [x] Generate the versioned update manifest from verified release bytes and apply only through a verified local Squirrel feed.
- [ ] Prove update, deferred restart, invalid package rejection, rollback, and uninstall.
- [x] Publish a CI-measured line count, workflow timing and a dim sum code name with the public photo in every release.
- [x] Fetch tags in the CodeQL checkout so the changelog provenance guard configures.
- [x] Commit `social-preview.png` at the repository root, add Open Graph and Twitter card tags to the site, publish `site/` through a Pages workflow, and point the repository homepage at the Pages URL.
- [ ] Upload `social-preview.png` in the repository's Settings → General → Social preview (manual; GitHub exposes no API for it).
