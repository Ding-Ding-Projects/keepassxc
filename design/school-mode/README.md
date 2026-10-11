# Shared presentation mode implementation handoff

This is a target-native Qt source handoff for the shared local mode service and its owned settings consumers. It is not a rendered reference or proof of universal delivery. The existing settings widgets are reused. No screenshots, native interactions or runtime receipts have been produced for this candidate.

## State inventory

- Missing record: intentional reset, default name, inactive, no credential.
- Valid inactive record: stored preferences apply; enable, rename and credential controls are available.
- Valid active record: exact chosen name remains; effective Voice is neutral English; owned language/tone and personal-vocabulary settings are absent; startup photos are suppressed and pending requests are canceled.
- Renamed record: labels, accessible names and local prompts use the chosen name. Record identity and location never depend on that name.
- Invalid/unreadable record or failed watch: unavailable explanation remains on the discoverable mode control; effective presentation restrictions remain active. The last validated name is retained when available.
- Concurrent update: bounded lock wait, factual retry state, no partial write.
- Incorrect credential: no mode change; generic local diagnostic.
- File replacement/deletion: directory and file watches reload and re-register; deletion intentionally resets the record.

## Capability inventory

| Capability | Implemented source path | Remaining evidence or implementation |
| --- | --- | --- |
| Shared versioned record | MaterialSchoolMode, GenericDataLocation/material-shared/school-mode.json | Other products must adopt this exact schema and locking protocol |
| Atomic serialized updates | QLockFile, QSaveFile with direct-write fallback disabled | Native competing-process and crash behavior pending |
| Live rename/state/verifier propagation | QFileSystemWatcher on record, containing directory and parent anchor | Native watch-limit, deletion and replacement matrix pending |
| Local credential | Botan Argon2id, fixed validated parameters, random salt, constant-time comparison | Password/PIN supported; passkeys not implemented |
| Shared control | Settings overview and SettingsHub, local password-mode input dialogs | Full localization patch integration and native accessibility pending |
| Prior preferences | Effective getters and preview override; stored settings remain untouched | Cross-product and complete export/import/reset interaction matrix pending |
| English presentation | Effective Voice plus removable source-text translator; owned settings rebuilt | Already rendered external surfaces and translator-priority changes need full enforcement |
| Forbidden settings omission | Language/tone card destroyed and rebuilt; SettingsHub omits language/voice/vocabulary rows and classic route | Global palette, command routes, other dialogs and other surfaces remain incomplete |
| Photo suppression | Startup eligibility, shared-state change cancellation, card removal and empty catalogue/name exposure | Native in-flight network and visible-card cancellation evidence pending |
| Personal vocabulary | Owned controls omitted; source-text translator precedes existing translators when installed | Direct replacement consumers, translator reinstallation and complete cache suppression remain incomplete |
| Validation/privacy | 8192-byte serialized record, depth 8, bounded name/revision/verifier parameters; no Config storage | Raw duplicate-key rejection, complete ACL/handle-race hardening and cross-platform receipts remain incomplete |
| Isolated regression fixtures | TestMaterialSchoolMode | Written, not executed |

## Native acceptance queue

Run the exact root build with tests enabled after this candidate is independently reviewed. Then execute `testmaterialschoolmode` alongside the existing Voice, settings and DimSum suites. Exercise two cooperating processes, credential replacement, rename, intentionally deleted record, unreadable record, watch exhaustion, active search, opening/closing settings, classic-route removal, queued file changes and source-text translation priority. Prove every affected surface in English/Cantonese/bilingual modes before and after activation, without credential material or private user data in captures. Cover normal/minimum geometry and high display scales.

Full universal delivery remains incomplete until every consumer and route is enforced and real runtime evidence exists. A shared module alone does not make other products honor the mode.

## Website bounded continuation

The reader's marked settings omission is independent of the native shared-record protocol.
`site/app.js` snapshots and restores exact hidden/inert/ARIA attributes for the six wrappers
listed in the [feature article](../../docs/features/interface/school-mode.md#website-marked-control-omission).
Focus returns to the local mode control before an owning subtree is omitted. Vocabulary status
is omitted with its controls while stored replacement data and preferences remain intact.
The hand-maintained inventory and deliberate missing-marker negative live in
`site/test-restricted-controls.mjs`; isolated source fixtures passed. Real browser keyboard,
accessibility and rendered acceptance remain pending. Shared record, PIN/password unlock,
renaming, cross-application propagation and complete palette/direct-route coverage are not
implemented by this browser-local switch. No universal-delivery or security-boundary claim is made.
