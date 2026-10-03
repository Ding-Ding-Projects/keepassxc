# Database creation, saving and reopening

## October 2026 repair

The investigation starts from `8301d0364a4e0753f9f0ae5c700174cd3dd1e353`, also the source of installed version `2.8.29201`. It follows a new database through entry creation, saving, complete application exit, a new process, and reopening the saved file.

The requested local lifecycle checks exposed additional defects in the history integration:

- Opening a database emits `databaseOpened` before unlocking it. The history callback attempted a snapshot while the database had a path and root group but no composite key. A symbolized run reproduced an access violation through `HistoryStore::serializeDatabaseWithoutEmbeddedHistory()`, `Database::writeDatabase()` and `CompositeKey::isEmpty()`. Correction `178d3e720261f6f52af06ca856fb49d5d8c53e92` registers save notifications once while allowing ready-state history initialization after unlock. It also rejects a missing or untransformed key before serialization or history mutation without excluding a legitimate transformed empty composite key. The keyless regression subsequently passed at `e2f26c784f3a78bdaab5ab38ba2ce5b2104d18e8`; packaged native confirmation remains pending.
- The Windows history helper supplied `NUL` for `GIT_CONFIG_GLOBAL` and `GIT_CONFIG_SYSTEM`. The installed Git rejected that configuration path with exit 128. An independent isolated probe confirmed that the documented `/dev/null` overrides permit initialization and a synthetic commit while excluding system and global configuration. Commit `24e05bf8d945b38c3251f80ee7c14ee3650e3e37` makes those two overrides portable; hook handling is unchanged.
- With host configuration deliberately excluded, deeply nested encrypted snapshots exceeded Git for Windows' default path limit. The GUI log explicitly reported `Filename too long`. An isolated comparison failed to add a 305-character snapshot path with `core.longpaths=false`, while a 304-character path completed initialization, add, commit and revision lookup with command-local `core.longpaths=true`. A production-helper regression and correction are in progress. No host configuration was changed. See the [vendor's long-path documentation](https://gitforwindows.org/git-cannot-create-a-file-or-directory-with-a-long-path.html).

Initial source inspection also found these two narrow issues:

- `DatabaseWidget::saveAs()` initialized its result to false but did not assign the result of `performSave()`. It therefore reported failure after a successful write. A new database's ordinary Save also uses this path, and close/lock decisions consume the returned result.
- The Windows positional-filename path obtained a UTF-16 pointer from a temporary `QString` before a later `FindFirstFileW()` call. The repaired path holds an owning string until the native call completes. Whether the previous code causes an observable crash depends on the allocation lifetime in the running build; it is not established as the reported crash's root cause.

Successful Save As now returns true. Cancellation and unsuccessful writes remain false. The change preserves the file format, encryption, save strategy, and embedded-history design.

## Focused verification

The original regression-only candidate is `d5be645278834bb015f37fcacbd3972c740853b8`. The source repair is `61ee4d40638ca65fdce7cebe715b48b3c5b705e4`; `5c5f9745dcf9273171ca7714c460b696a2cfdc87` makes recent-file expectations use native path separators and asserts nonempty lists before reading their first element. Test-isolation change `3a1bb24b601ab57cd42a9f6d14efb4ef2bc5256a` was applied separately to the regression-only candidate, producing `7972fe0372abee1444b0a62a34a072c732a537f7`. That baseline still lacks the production repairs.

| Check | Intended observation | Current result |
| --- | --- | --- |
| `skipsHistoryBeforeDatabaseUnlock` | Reject a keyless snapshot without mutation or crash, then allow later unlock/editing | `0xC0000005` at regression-only `89791fd7`; 3 passed, 0 failed at repaired `e2f26c7` |
| `recordsReadyDatabaseSnapshots` | Preserve password-key and transformed empty-key snapshots and editable source state | Both rows pass at `d08b434`; 4 passed, 0 failed including setup and cleanup |
| `testHistoryInitializationAfterUnlock` | Create the unlock baseline and register only one save listener | 3 passed, 0 failed at `de8d133` and `d08b434` |
| `testSaveAs` | True result, written file, cleared modification state, recent destination | Intended false-result assertion red at `e2f26c7`; 3 passed, 0 failed at `d8282cf4` |
| `testSaveAsCanceled` | False result, original path and unsaved editable metadata preserved | 3 passed, 0 failed at `e2f26c7` and `d8282cf4` |
| `testSaveAsFailed` | False result for an existing-directory destination; data remains editable | 3 passed, 0 failed at `e2f26c7` and `d8282cf4` |
| `testFirstSaveOnClose` | First save completes, tab closes, saved database reopens | Intended false-result assertion red at `e2f26c7`; 3 passed, 0 failed at `d8282cf4` |
| `testdatabase` | Existing database behavior remains intact | 12 passed, 0 failed at `7972fe0372abee1444b0a62a34a072c732a537f7` |
| Ten selected temporary-root history cases | Storage and embedded-history round trips | Each case passes 3/3 at `d08b434`, including KDBX3/KDBX4 embedded-history round trips |
| Related GUI checks | Creation, entries, saves, backup and locking | Creation, edit, search-edit, delete, clone, save, backup and locking each pass 3/3 at `d08b434`; add-entry and three backup-path rows remain under investigation |
| `recordsSnapshotsBelowDeepHistoryRoot` | Persist and reconstruct nested encrypted history under a long root | Red at `c684f987`; still red at `d08b434`, where `recordSave()` returns false without a preceding diagnostic |
| Packaged application's fresh-process lifecycle | Ten synthetic entries survive and match after reopening | Not run yet |

The new GUI cases explicitly enable atomic saves because the general GUI-test setup disables them. The focused native targets compiled successfully in Release and the GUI target was rebuilt with `RelWithDebInfo` for diagnosis. Only the later intended boolean assertions are accepted as the Save As red regression. The `d8282cf4ff4a47a7de1958c7ba862060b445ccdc` GUI run completed 14 separate processes, nine green and five red, without a timeout or access violation. Its executable SHA-256 is `92c4789c7602853bc88f63cc00d6a1ac88db5c4b07b51d34194b695465ea24ca`. Counts include Qt setup and cleanup. These are isolated offscreen tests, not native packaged acceptance.

The subsequent short-temporary-root run at `d08b434fc734c2b35e15e86a8d3349fa0dd4675f` completed 27 isolated processes: 24 passed and three failed, with no crash or timeout. Its GUI executable SHA-256 is `ce4383136377ff36b7c74b4a46d5656cc602601b395bec61267fd550e41b992d`; the history executable SHA-256 is `30f894ad05351e778da0de299ebeeb48a7d2db3fbcd56896dae2dda4340881df`. The remaining add-entry assertion expected zero history records after configuring TOTP, although TOTP setup deliberately records the previous entry state. Commit `d7575293747631bd42349c14579ebc28656b62ff` asserts that exact pre-TOTP state and its preservation instead; execution is pending. The backup-path failures begin at the modified-state precondition before saving. Neither missing backup output nor the quiet deep-root failure establishes a further production defect yet.

## Native acceptance procedure

1. Record source identity, executable SHA-256, version and package provenance. Establish an isolated configuration, application-data/history root and temporary root before constructing the process. The single-instance check runs before `--config` is parsed. Use the existing `KPXC_CONFIG` and `KPXC_CONFIG_LOCAL` environment overrides for early configuration isolation and a unique temporary directory and username namespace for the single-instance lock and socket. Qt 6.8.3 resolves Windows application-data locations through `SHGetKnownFolderPath`, so changing `APPDATA` or `LOCALAPPDATA` alone does not isolate the default history store. Native acceptance needs a genuinely isolated operating-system profile or an independently verified equivalent. The GUI test executable uses Qt test mode and a unique organization namespace before constructing its application; this test-only mechanism is not native-package evidence.
2. Create a synthetic KDBX4 database through the real interface. Add ten reproducible random entries across two groups, including varied titles, usernames, reserved example URLs, multiline notes, Cantonese text and generated test values.
3. Save explicitly and verify that the modified indicator clears. Use Quit and confirm the original process exits; a window hidden to the tray is not a completed exit.
4. Launch a distinct process with the same isolated profile. Open and unlock the saved database through the interface, and compare all synthetic fields locally.
5. Quit again and launch with the database filename as a positional argument. Use a path containing spaces and Cantonese characters, then repeat unlocking and exact readback.
6. Retain real captures and an interaction record tied to source identity, executable hash, process identities, exit results and comparison verdicts.

Use only disposable synthetic data. Never read or modify a personal database, terminate an unrelated process, or place credential values in screenshots, logs or published evidence. Keep password fields masked. Configuration and data isolation must not rely only on command-line configuration paths.

## Current environment limitations

The configured isolated-desktop service cannot currently be reached, so no native interaction or screenshot is claimed. The compiler and SDK were absent at the start of the task; the approved signature-verified Visual Studio 2022 Build Tools installation completed with setup exit 0 and restart disabled. Qt 6.8.3 and the complete compiler/SDK environment are verified. Packaging remains pending because canonical Ruby documentation tooling cannot resolve its original `ruby_builtin_dlls` assembly. Manifest identities, paths and referenced files agree; no speculative vendor patch is being used.

A successful local test or package build will not close the native acceptance item. Completion requires the real packaged lifecycle and exact readback. Track current evidence in [issue #17](https://github.com/Ding-Ding-Projects/keepassxc/issues/17) and [progress discussion #18](https://github.com/Ding-Ding-Projects/keepassxc/discussions/18).
