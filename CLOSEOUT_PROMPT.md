# Database lifecycle repair continuation

This is an in-progress handoff, not a completion report. The objective is to repair and verify database creation, ten synthetic entries, save, complete exit, relaunch and exact reopening, including a positional filename with spaces and Cantonese characters. Preserve personal databases and unrelated processes. Use the approved isolated-desktop route; do not install or reconfigure it merely because it is unavailable.

Baseline: `main` and release `v2.8.29201` source `8301d0364a4e0753f9f0ae5c700174cd3dd1e353`.

Implementation branch: `codex/database-lifecycle-20261003`. Current code commits:

- `d5be645278834bb015f37fcacbd3972c740853b8`: atomic Save As, cancellation, failed-write and first-close regressions.
- `61ee4d40638ca65fdce7cebe715b48b3c5b705e4`: Save As return result and owning startup filename string.
- `5c5f9745dcf9273171ca7714c460b696a2cfdc87`: native recent-file separators and nonempty list checks.
- `3a1bb24b601ab57cd42a9f6d14efb4ef2bc5256a`: isolate default GUI-test data paths before application construction.
- `24e05bf8d945b38c3251f80ee7c14ee3650e3e37`: portable Git configuration isolation for history.
- `740be16faf2623a69fb6e1486082a5098bd99ab1`: keyless-open, ready snapshot and unlock/listener regressions.
- `178d3e720261f6f52af06ca856fb49d5d8c53e92`: defer history snapshots until key readiness and allow initialization after unlock.
- `a7e4d392b119b40201f6dd24d3e2b992462afc88`, corrected through `44e844b98117b296b219b0d7dc3f076852740290` and `ab14108fb1490315e60d1ec06b74d268d2d17660`: deep-root snapshot regression through public accessors.
- `4c5c49bec69c2347754f16d6c5639052605009d1`: command-local Windows long-path support for history subprocesses.
- `0e3ad08de78764c4b79acabf1e3894fdfd0a7e70`: adapt five GUI cases to current vault controls and root recursive inventory.
- `d7575293747631bd42349c14579ebc28656b62ff`: assert the exact pre-TOTP history snapshot during entry creation; execution pending.

Verification branch: `codex/lifecycle-verification-20261003`. Preserve the exact source while a build or test reads it. Database checks pass 12/12 at `7972fe0372abee1444b0a62a34a072c732a537f7`. The keyless-opening regression is red at `89791fd7569d4147d0d5a61ab98d5bf1e18e9997` and green at `e2f26c784f3a78bdaab5ab38ba2ce5b2104d18e8`. Intended Save As/first-close assertions are red at `e2f26c7`; all four focused cases pass at `d8282cf4ff4a47a7de1958c7ba862060b445ccdc`. Unlock-baseline/single-listener coverage passes at `de8d1330f60e67a9f12aba4c62af708eb57b23c7`.

The short-temporary-root run at `d08b434fc734c2b35e15e86a8d3349fa0dd4675f` completed 27 isolated processes: 24 passed and three failed, with no crash or timeout. All ten selected history cases, ready-key rows, unlock baseline/listener and four focused atomic-save cases passed. Creation, edit, search-edit, delete, clone, save, backup and locking also passed. Remaining: deep-root recordSave returns false without a warning; add-entry expects zero history after TOTP setup; three backup-path rows fail the modified-state precondition before saving. The TOTP expectation correction is ready for execution. Diagnose the first operation for the other two before changing production code.

Qt redirects the original long runner paths through a package cache, causing the temporary working directory itself to exceed 260 characters. Keep result logs separate from a short owned TEMP/TMP root. The original nested-path behavior was observed red at `c684f9877d944a32b06a7952c913b58f712550bd` under that corrected environment. Do not run global history-feed cases without isolated default data paths.

Compiler installation was approved and completed with restart disabled. MSVC, SDK, Qt 6.8.3 and focused compilation are verified. Ruby documentation tooling remains blocked on original private-assembly resolution; a bounded elevated trace is awaiting separate authorization. No speculative vendor patch is authorized.

Native acceptance is unverified. The required isolated-desktop service is unavailable. Qt uses Windows known-folder locations, so APPDATA/LOCALAPPDATA environment changes alone cannot isolate production history. An isolated profile or verified equivalent is required before native launch. No actual ten-entry native sequence or screenshot is claimed.

Documentation is being updated in README, ROADMAP, HANDOFF, CHANGELOG, the categorized lifecycle article and wiki source. The website manifest must reference the committed article snapshot before publication. No branch push, main integration, release verification or cleanup is claimed by this snapshot. Retain all task-owned work and diagnostic evidence. Do not delete unrelated or unverified checkouts.

Public tracking: [issue #17](https://github.com/Ding-Ding-Projects/keepassxc/issues/17), [discussion #18](https://github.com/Ding-Ding-Projects/keepassxc/discussions/18). Other open issues are outside this request. The goal remains active; completion requires native acceptance and delivery. Do not infer three blocked goal turns from repeated tool calls.
