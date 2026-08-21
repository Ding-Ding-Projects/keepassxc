# File manifest

`create` = the file does not exist in the fork today. `modify` = it does.

## New classes

| | Path | Purpose |
| --- | --- | --- |
| create | `src/gui/material/MaterialBreakpoints.{h,cpp}` | The five window size classes and the pane rules |
| create | `src/gui/material/MaterialShapeMorph.{h,cpp}` | Spring corner-radius interpolation for press states |
| create | `src/gui/material/MaterialSearchRegistry.{h,cpp}` | Every live SearchBar, so the builder knows its anchor |
| create | `src/gui/material/MaterialTabOverflow.{h,cpp}` | The searchable sheet tabs overflow into |
| create | `src/gui/material/MaterialFabMenu.{h,cpp}` | Staggered FAB menu |
| create | `src/gui/material/MaterialRegexTokens.{h,cpp}` | Flat token view for blocks and the explainer |
| create | `src/gui/material/MaterialRegexSafety.{h,cpp}` | Static risk shapes and the bounded evaluator |
| create | `src/gui/material/MaterialElementOverrides.{h,cpp}` | Per-element persisted appearance overrides |
| create | `src/gui/material/MaterialVoiceStrings.{h,cpp}` | Generated five-level bilingual string table |
| create | `src/gui/material/MaterialExternalEditor.{h,cpp}` | Detect, choose and launch an external editor |

## Modified

| | Path | Change |
| --- | --- | --- |
| modify | `src/gui/material/MaterialShell.cpp` | Breakpoint enum, rail ⇄ bottom bar reparenting, `breakpointChanged` |
| modify | `src/gui/material/MaterialNavigationRail.cpp` | Shape morph on press, 72px icons-only form, live sublabels |
| modify | `src/gui/material/MaterialTabStrip.cpp` | Overflow, drag reorder, pinning, persisted order |
| modify | `src/gui/material/MaterialSearchBar.cpp` | Register with SearchRegistry; per-bar mode and flags |
| modify | `src/gui/material/MaterialRegexBuilder.cpp` | Token blocks, explainer, replace preview, dialect tab, export |
| modify | `src/gui/material/MaterialEntryDetail.cpp` | Colour-flooded hero by health role |
| modify | `src/gui/material/MaterialEntryDelegate.cpp` | Health dot, avatar, tag chips, density row heights |
| modify | `src/gui/material/MaterialSettingsScreen.cpp` | Element overrides section, font family/scale/weight |
| modify | `src/gui/material/MaterialSettingsHub.cpp` | Per-page search bar wired to the builder; cross-page hit counts |
| modify | `src/gui/material/MaterialHistoryScreen.cpp` | Kind filter, per-revision diff, restore-as-new-revision banner |
| modify | `src/gui/material/MaterialChangelogScreen.cpp` | Date range with inline parse errors, presets, composed filters |
| modify | `src/gui/material/MaterialButtons.cpp` | ExtendedFab → ExpressiveFab; 56/28 metrics kept off FilledButton |
| modify | `src/gui/material/MaterialSnackbar.cpp` | Errors and warnings do not auto-dismiss; mirror into the centre |
| modify | `src/gui/material/MaterialNotificationCentre.cpp` | Own search bar, history limit |
| modify | `src/gui/material/MaterialDimSum.cpp` | Bilingual name at every funny level; suppression rules |
| modify | `src/gui/material/MaterialVoice.cpp` | Delegate to VoiceStrings; two persisted sliders |
| modify | `src/gui/material/MaterialTheme.cpp` | Font family/scale/weight; see TOKENS.md |
| modify | `src/gui/material/MaterialWindowChrome.cpp` | 44px custom frame, caption buttons, drag region |
| modify | `src/gui/MainWindow.cpp` | Wire the new signals; `addDestination()` calls unchanged |
| modify | `src/core/Config.cpp`, `Config.h` | 24 new keys — see ACCEPTANCE.md |
| modify | `utils/design/sheets.json` | New pages; see `sheets-additions.json` |
| modify | `utils/generate_sheet_catalogue.mjs` | Unchanged format; re-run after editing sheets.json |
| create | `utils/design/voice-strings.json` | Source for the generated string table |
| create | `utils/generate_voice_strings.mjs` | Generator, with the placeholder-parity check |
| modify | `src/gui/CMakeLists.txt` | The ten new source pairs |
