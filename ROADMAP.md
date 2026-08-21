# Roadmap

## Windows-only foundation

- [x] Reject non-Windows, non-MSVC, and non-x64 configurations at CMake configure time.
- [x] Add pinned root dependency, native build, and Squirrel.Windows installer entry points.
- [x] Validate `RELEASES`, hashes, package paths, required payload, and unsigned setup state.
- [ ] Remove remaining macOS and Unix-only registrations after their Windows consumers are audited.
- [ ] Remove WiX/CPack only after a real Squirrel package installs and launches successfully.

## Material UI rewrite

- [x] Add the five responsive window-size classes and exact boundary tests.
- [ ] Integrate responsive navigation, compact bottom navigation, and pane alternatives.
- [ ] Complete search registry, tab overflow, regex safety, appearance overrides, generated voice strings, and external-editor integration.
- [ ] Migrate every remaining dialog and auxiliary surface to the shared component system.
- [ ] Complete deterministic reference-versus-built parity evidence for every design surface.

## Installer and updater

- [ ] Prove clean Squirrel installation and launch in an isolated Windows account or virtual machine.
- [ ] Add idempotent lifecycle handling for shortcuts, file associations, and browser registration.
- [ ] Replace the existing update checker with staged Squirrel states and user-controlled restart.
- [ ] Prove update, deferred restart, invalid package rejection, rollback, and uninstall.
