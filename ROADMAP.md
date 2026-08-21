# Roadmap

## Windows-only foundation

- [x] Reject non-Windows, non-MSVC, and non-x64 configurations at CMake configure time.
- [x] Add pinned root dependency, native build, and Squirrel.Windows installer entry points.
- [x] Validate `RELEASES`, hashes, package paths, required payload, and unsigned setup state.
- [x] Remove audited macOS source, bundle, auto-type, quick-unlock, icon, compiler-probe, and Unix manpage paths.
- [ ] Remove WiX/CPack only after the verified real Squirrel package also installs and launches successfully.

## Material UI rewrite

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
- [x] Add the first native Reports parity batch with truthful states, real category data, selection/export, regex filtering, accessibility, and responsive reflow.
- [x] Add the first native Appearance parity batch with typography persistence, element overrides, regex filtering, keyboard controls, and narrow reflow.
- [ ] Complete deterministic reference-versus-built parity evidence for every design surface.

## Installer and updater

- [x] Build and byte-verify a real unsigned `Setup.exe`, `RELEASES`, and full `.nupkg` from the native staged tree.
- [ ] Prove clean Squirrel installation and launch in an isolated Windows account or virtual machine.
- [x] Handle Squirrel install, updated, uninstall, obsolete, and first-run process arguments before ordinary UI startup.
- [ ] Extend lifecycle handling beyond shortcuts to file associations and browser registration refresh.
- [ ] Replace the existing update checker with staged Squirrel states and user-controlled restart.
- [x] Wire non-blocking download/apply progress, persistent ready actions, deferred restart, and Squirrel process-start relaunch.
- [x] Remove the superseded modal update dialog after the non-blocking flow passed the full local suite.
- [x] Replace upstream release discovery with a bounded, versioned fork-owned Squirrel manifest and typed state/failure model.
- [x] Stream full packages with storage preflight, atomic finalization, SHA-256/SHA-1 validation, and bounded NuGet structure checks.
- [x] Generate the versioned update manifest from verified release bytes and apply only through a verified local Squirrel feed.
- [ ] Prove update, deferred restart, invalid package rejection, rollback, and uninstall.
