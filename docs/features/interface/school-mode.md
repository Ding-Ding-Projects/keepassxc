# Shared presentation mode

The application now has a bounded first implementation of a shared, user-renamable presentation mode. Its local control appears above the appearance overview and in the settings hub. The shipped default name is School mode. After a rename, this control, its accessible name, status and local credential prompts use the exact chosen name. Renaming never changes the record location or application identity.

Set a local PIN or password before enabling the mode. Turning it off verifies the shared local credential. Renaming or replacing an established credential also requires that credential. Inputs remain in local password-mode controls; they are never requested in chat, stored in Config, exported, written to history or logged. This is a convenience lock, not a security boundary. Deleting the shared local record intentionally resets its state, name and credential. Passkeys are not implemented.

While the mode is active, Voice getters and explicit previews resolve to English at the neutral level. Stored language and tone choices remain unchanged and return after unlocking. A removable source-text translator handles subsequent Qt translation lookups, and the owned settings surfaces rebuild on restriction changes. Their language/tone controls and vocabulary rows are removed from the page and search inventories. The settings hub's classic route is omitted while active. Startup photo eligibility is blocked; an active shared-state change cancels pending photo requests and removes a visible photo card. The catalogue and display-name entry points return empty while restricted.

## Interoperable local record

All cooperating products use `QStandardPaths::GenericDataLocation`, independent of their organization/application name, with the relative path `material-shared/school-mode.json`. This is a new documented protocol, not a claim that other installed products already implement it.

The JSON object has schema version 1 and these fields:

| Field | Contract |
| --- | --- |
| `schemaVersion` | Exact numeric 1 |
| `name` | Nonempty, trimmed string, at most 80 UTF-16 code units; no ASCII control characters |
| `enabled` | Boolean |
| `revision` | Nonnegative integral JSON number, at most 9007199254740990 |
| `credential` | Optional object when inactive; required when active |
| `credential.algorithm` | Exact `argon2id-v1` |
| `credential.memoryKiB`, `iterations`, `parallelism` | Exact 16384, 2, 1 |
| `credential.salt` | Lowercase hexadecimal representation of 16 random bytes |
| `credential.verifier` | Lowercase hexadecimal representation of a 32-byte derived result |

The whole compact record is limited to 8192 bytes and JSON depth 8. Unknown bounded fields, including credential extension fields, survive updates. Cooperating writers acquire `<record>.lock` with QLockFile before rereading and changing the current record. Updates increment revision and use QSaveFile with atomic replacement and no direct-write fallback. Verifier work uses the application's existing Botan password-hash primitive. The chosen input accepts 4..1024 UTF-8 bytes. Comparison is constant time. Temporary byte buffers are scrubbed where owned; Qt input controls and implicit string copies do not provide a guarantee that every transient memory copy is erased.

The reader watches the containing directory, its parent anchor and the file when present, then re-registers watches after replacement. Missing files mean intentional reset. Malformed, unreadable, unsafe-link or unwatchable state is unavailable rather than silently inactive; effective restrictions remain active and the control explains the condition. Existing ancestor symlinks and Windows reparse points are rejected before access. These path checks and requested owner-only file permissions do not prove race-free handle containment or a complete Windows ACL policy. The mode is not a security boundary.

## Scope and remaining work

The [implementation handoff](../../../design/school-mode/README.md) maps consumers and incomplete work. Universal enforcement remains incomplete: other applications must adopt the schema; global palette and direct routes, all existing rendered surfaces, direct personal-vocabulary consumers, translator-priority changes, built-in food references elsewhere and complete settings export/import interactions still need work. Existing translated widgets outside the rebuilt settings do not gain verified English presentation merely because future lookups do. Raw duplicate-key rejection and complete filesystem/ACL hardening are also pending. No claim of universal completion or passkey support is made.

The 34 `school.control.*` voice catalogue entries are integrated in source, including English and Cantonese variants at all five tone levels. Rendered language acceptance remains pending. No credential values belong in documentation, screenshots, diagnostics, exports or source.

## Verification status

`TestMaterialSchoolMode` uses temporary records, temporary Config files, test-mode standard paths and an injected shared-service fixture. Source fixtures cover validation bounds, absent verifier, wrong credentials, replacement, rename, unknown-field preservation, live two-reader updates, deletion reset, corrupt/unwatchable states, effective language/tone restoration and physical language-card omission. They have not been compiled or executed. Native timing, real multi-process behavior, accessibility, full suppression, language switching, viewport and display-scale acceptance remain pending.

After a reviewed root build, the focused command is:

```powershell
ctest --test-dir build-windows --output-on-failure --timeout 180 -R "^testmaterialschoolmode$"
```
