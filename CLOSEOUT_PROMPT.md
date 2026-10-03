# KeePassXC lifecycle and interface continuation

The active objective includes the complete synthetic database lifecycle, full Material Design 3 conversion, motion across the interface, searchable context menus with a local regular-expression builder, and every required feature on the native application and documentation website. The objective is incomplete. Do not equate implementation, a schema check, an offscreen test or a package download with native acceptance.

## Integrated menu unit

- Source candidate: `69f4ae67fe58b4e4ad4aeb43dbff1cc4f7f34cda`. Its task branch was pushed and independently read back. This continuation update follows its local fast-forward integration into main and does not change tested source.
- Central interception adds a local search and inline builder to QMenu surfaces, including editor context menus, while preserving original action identity, ownership and callbacks. Existing Select searches are reused.
- Bounded filtering handles syntax and resource limits without leaving stale activatable results. Query, mode and flags remain local to the menu. Queued work cannot refilter shared actions after closure.
- The actual Select builder-button path expands its popup within available screen bounds and restores the original width and hidden container on closure.
- First tests-only regression: `5c33af6b74c18bb43d0f0c932e9a38fd71e06c16`, 15 passed and 8 failed. The first repairs reached 23 passes.
- Second tests-only regression: `270eb663e6303166839ac1ef3f3c5b2e11e71656`, 23 passed and 8 failed. Final candidate: 31 passed, 0 failed, 0 skipped, exit 0, no timeout, 544 ms. Totals include setup and cleanup.
- Final test executable SHA-256: `7DDF2A8B8D9F784AA389D32CE9813CACA9E108DB011B0F7E41741D023E7AE378`. Independent final source review found no further confirmed finding within its bounded correctness, lifetime, accessibility and privacy lenses.
- Native rendering, keyboard grabs, screen-reader behavior and the complete scale/language/viewport matrix remain unverified. Non-QMenu adapters, calendar-grid filtering, per-element appearance and locks, saved snippets and a visible query-history picker remain unfinished.
- Procedure and bounds: [native menu search](docs/features/search/context-menu-search.md). Route inventory: [context-menu inventory](design/context-menu-inventory.md).

## Other active isolated units

- Motion candidate `3003180d830e398814ca70443a793f1d8278d6fa` remains unintegrated. Shared motion has 33 passing Qt entries, appearance 6 and tabs 6. An independent deliberate mutation made two reduced-motion veto cases fail; exact restoration made both pass. Its responsive-shell suite still has 8 passes and one pane-sizing failure.
- A separate task branch owns the independently supported first-show pane-restoration repair, with tests first and preserved user widths. The motion source and its verification tree remain frozen.
- Inventory candidate `ee21ce8f5e97ebae880b06494a34d1068d60de8d` remains unintegrated. Its full documentation bundle built successfully. Independent review found omitted nested destinations, evidence-tuple validation gaps and contradictory startup-surprise documentation. A separate reviewer is repairing these gaps and will repin the immutable article manifest. No completeness count is accepted until this repair and follow-up review finish.
- The current explicit startup-surprise requirement is a fresh 1% launch chance with a persisted off setting, at most once per launch, excluding first-run, update, error and mid-task flows. Existing runtime behavior is not yet compliant.
- All new delegated work uses the user-selected `gpt-6-astra`. Every lane has explicit isolated ownership.

## Original lifecycle evidence and published baseline

- Lifecycle source: `97825b47b2916ced881e82763e1a49d23ed98313`. Implemented actual Save As return results, an owning startup filename string, history key-readiness checks, hydration after unlock with one save listener, isolated Git configuration and command-local nested-file long-path support.
- Focused lifecycle checks: 28 isolated processes, 99 Qt passes, zero failures and zero timeouts. GUI: 15 processes / 50 passes. History: 12 / 37. Database: 1 / 12. Counts include setup and cleanup. These are source-bound historical checks, not a test of the later menu integration.
- [Published v2.8.29801](https://github.com/Ding-Ding-Projects/keepassxc/releases/tag/v2.8.29801) targets `07ce4dc158bcd7eb735cc309f009c531731f3133`. All seven published assets were downloaded and validated for bytes, provenance, package contents and update manifest.
- Published executable SHA-256: `ff0e81e698691806436df3d6a5499c51f9d2e7ed1eafeb0d3fa79d65fb1dbee6`. [Build 37147954035](https://github.com/Ding-Ding-Projects/keepassxc/actions/runs/37147954035) and [documentation run 37147954125](https://github.com/Ding-Ding-Projects/keepassxc/actions/runs/37147954125) succeeded. This release does not contain the new menu or motion candidates.
- Wiki master `03b3e478d2bb07ae520343085143ea9f38ff96cf` and Home readback were verified.

## Required remaining acceptance and constraints

- The approved isolated native-control route remains unavailable after three unchanged attempts. No ten-entry native creation, full Quit/relaunch, positional filename with spaces and Cantonese characters, exact field readback or new genuine screenshot is claimed.
- Preserve personal databases and unrelated processes. Establish isolated profile and data/history paths before process construction; Windows known-folder behavior makes APPDATA/LOCALAPPDATA overrides alone insufficient.
- Do not substitute visible control, install or reconfigure the native-control route, print synthetic credential values or cause a host power/login action.
- History initialization in an initial directory of 285 characters remains an explicitly unverified diagnostic limitation. Three unchanged launch comparisons are exhausted. Preserve the separate reproduction branch at `6c1c3accdc86890053a99d5ae79ca909a2cb0c64`; do not call it fixed or integrate its incomplete test.
- Compiler installation was approved and completed with restart disabled. MSVC, SDK and Qt 6.8.3 are available. The local production documentation build has a separate Ruby assembly-resolution limitation; the hosted production route has succeeded.
- Keep the goal active while these independent authorized units can progress. Native-route unavailability does not establish a whole-goal impasse.
- Preserve every active or unmerged branch and working tree. No cleanup has occurred. Archive, ownership, preservation and remote ancestry proof remain prerequisites to any cleanup.

Public tracking: [feature issue 12](https://github.com/Ding-Ding-Projects/keepassxc/issues/12#issuecomment-5973196402), [lifecycle issue 17](https://github.com/Ding-Ding-Projects/keepassxc/issues/17#issuecomment-5972987265), [rolling discussion 18](https://github.com/Ding-Ding-Projects/keepassxc/discussions/18), and [release discussion 19](https://github.com/Ding-Ding-Projects/keepassxc/discussions/19).

Next: push and verify this completed menu source unit on main, observe its exact hosted build separately, finish independent inventory and pane repairs, and continue the remaining feature work. Native acceptance can resume only after the approved route becomes available. No whole-product completion is claimed.
