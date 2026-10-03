# Roadmap

## October 2026 database lifecycle repair

- [x] Repair the history snapshot attempted before database unlock and prove its focused regression red then green. Packaged native acceptance remains below.
- [x] Verify successful Save As returns true, cancellation and unsuccessful writes return false, and first save on close completes in isolated atomic-save regressions. Both success assertions were observed red before repair and green afterward.
- [x] Verify focused history initialization with portable configuration isolation. Ten selected history cases, both ready-key rows and unlock baseline/listener pass at `d08b434`; this does not include packaged native acceptance.
- [ ] Support per-database history directories beyond 260 characters. Nested-file long-path support is verified, but the initial directory change fails at 285 characters. Three bounded launch comparisons failed; the reproduction remains preserved outside completed-unit integration.
- [x] Complete the focused creation, entry, saving, backup and locking GUI coverage using the current interface. All 15 selected processes pass at `6c1c3ac`, with 50 Qt entries passing including setup/cleanup. Native packaged acceptance remains separate.
- [ ] Verify positional database opening with spaces and Cantonese characters after retaining the Windows filename string through its native call.
- [ ] Create ten synthetic entries through the packaged interface, save, fully exit, relaunch and prove exact contents after reopening.
- [ ] Verify the final package, remote integration and task-owned cleanup. Current evidence is recorded in [the lifecycle article](docs/features/delivery/database-lifecycle-repair.md).

## Embedded database history

- [ ] Stage a bounded per-database history bundle before the primary KDBX write and save the data and bundle atomically; current local code adds KDBX3/KDBX4 round trips, but the full production target has not been rebuilt after the latest edits.
- [ ] Merge staged history into application storage only after the KDBX write succeeds, including Save As and backup flows; focused Save As ancestry coverage passes, while backup and real runtime behavior remain unverified.
- [ ] Complete focused KDBX and Git coverage for transfer, rollback, concurrency, replay, malformed input and size limits; `testmaterialhistory` passed 1/1 in 40.57 seconds, but oversized-input execution, replay, all rollback paths, foreign identity and backup coverage remain open.
- [x] Complete the production-only x64 build and verify staged executable provenance at `c52d19a753323c6dea653fbfffb7d9078536c7a7`; KeePassXC 2.8.0 staged, SHA-256 `7C164611CC931F34F5515FB9AA61AE13871C660D44407BE50AA74FA9392E5959`. This verdict predates the current local edits and must not be applied to them.

## September 18, 2026 closeout

- [x] Inspect the primary checkout, all linked checkouts, local and remote branches, and artifact Tongs after fetching the Git remote.
- [x] Confirm every inspected checkout had zero uncommitted paths and zero unmerged index entries before integration.
- [x] Integrate the local `codex/*` branches into `main`, preserving both parent histories and recording automatic conflict choices in `HANDOFF.md`.
- [x] Confirm the integrated index is clean and no conflict markers remain in the inspected source and handoff files.
- [x] Create and verify the external repository archive before any cleanup removal.
- [x] push `main` and verify the Git remote ref with `git ls-remote`.
- [x] Prove ancestry for each cleanup candidate, remove only safe task-owned redundant items, and document every retained item.
- [ ] Re-scan open issues and external project surfaces after the closeout push.

## Repair and release verification

- [x] Separate package HEAD and source-status failures, sanitize dirty-state counts, and verify clean, tracked, untracked, malformed, and command-error states in the focused regression check.
- [ ] Run a new GitHub Actions package job after integration; the original `35372345117` path remains unavailable because that run emitted only the generic message.
- [x] Validate website release metadata against published package and build provenance, including BOM and malformed-input regressions.
- [x] Render verified downloads and provenance in the website and refresh them during publication (live main deployment `7bf379fd`, run `34154701931`).
- [x] Compile the repaired native targets and pass the focused update, title-bar, tab and selected GUI suites at `5c8066ae`.
- [x] Produce and byte-verify local unsigned Squirrel packages at `877f4434`; installed behavior remains separate.
- [x] Integrate and locally verify the reviewed updater and report repairs at `4530d541` (`testupdatecheck` 101 passed, `testmaterialreports` 5 passed).
- [x] Verify title-bar and tab ownership checks at `4530d541` (`testmaterialtitlebar` 8 passed, `testmaterialtabs` 6 passed).
- [x] Add post-publication release timing and numeric Latest reconciliation at `7dabb1c9`; finalizer run `34157090857` passed.
- [x] Publish and verify the final stable release `v2.8.22201` targeting `a30d1096`, including timing, hashes, package metadata and required photo asset.
- [ ] Verify the repaired Windows build, window/content dragging, and installed automatic update lifecycle. The normal-user install now proves the installed version and ten app-local MSVC runtime DLLs; GUI update execution remains unverified.
- [ ] Complete the per-surface inventory and real runtime evidence before declaring release-grade completion.

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
