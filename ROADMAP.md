# Roadmap

## Windows-only foundation

- [x] Reject non-Windows, non-MSVC, and non-x64 configurations at CMake configure time.
- [x] Add pinned root dependency, native build, and Squirrel.Windows installer entry points.
- [x] Validate `RELEASES`, hashes, package paths, required payload, and unsigned setup state.
- [x] Remove audited macOS source, bundle, auto-type, quick-unlock, icon, compiler-probe, and Unix manpage paths.
- [ ] Remove WiX/CPack only after the verified real Squirrel package also installs and launches successfully.

## Material UI rewrite

- [x] Add the five responsive window-size classes and exact boundary tests.
- [ ] Integrate responsive navigation, compact bottom navigation, and pane alternatives.
- [ ] Complete search registry, tab overflow, regex safety, appearance overrides, generated voice strings, and external-editor integration.
- [x] Centralize existing material search ownership and remove private Vault/History regex builders.
- [x] Register command-palette and notification-history searches and make both consumers mode/flag aware.
- [ ] Migrate every remaining dialog and auxiliary surface to the shared component system.
- [ ] Complete deterministic reference-versus-built parity evidence for every design surface.

## Installer and updater

- [x] Build and byte-verify a real unsigned `Setup.exe`, `RELEASES`, and full `.nupkg` from the native staged tree.
- [ ] Prove clean Squirrel installation and launch in an isolated Windows account or virtual machine.
- [x] Handle Squirrel install, updated, uninstall, obsolete, and first-run process arguments before ordinary UI startup.
- [ ] Extend lifecycle handling beyond shortcuts to file associations and browser registration refresh.
- [ ] Replace the existing update checker with staged Squirrel states and user-controlled restart.
- [x] Replace upstream release discovery with a bounded, versioned fork-owned Squirrel manifest and typed state/failure model.
- [x] Stream full packages with storage preflight, atomic finalization, SHA-256/SHA-1 validation, and bounded NuGet structure checks.
- [x] Generate the versioned update manifest from verified release bytes and apply only through a verified local Squirrel feed.
- [ ] Prove update, deferred restart, invalid package rejection, rollback, and uninstall.
