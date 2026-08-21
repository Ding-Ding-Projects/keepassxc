# Shared UI acceptance checklist

One row per rule. "Where" is where to look in the prototype; "Qt" is what has to
be true in the build. A row is not done because the prototype shows it.

## Material Design and appearance

| Rule | Where | Qt |
| --- | --- | --- |
| Full M3 (Expressive) — tokens, type, shape, elevation, motion, anatomy; zero legacy elements | Every surface | No widget hard-codes a colour; every value resolves through `Material::Theme` |
| Functional data colours exempt as data | Health dots, report severities, entropy meter | Keep them out of the seed remap |
| Persisted theme, density, seed | Appearance › Theme | `GUI_ApplicationTheme`, `GUI_MaterialSeed`, `GUI_MaterialDensity` |
| Full font customization: family, size scale, weight, live preview, CJK-safe fallback | Appearance › Typography | `GUI_FontFamily`, `GUI_FontScale`, `GUI_FontWeight`; fallback chain always ends in a CJK face |
| Changes apply to the live UI, not only after restart | Any change in Appearance | `Theme::reload()` + `Theme::changed()` |
| Per-element editors for font, colour, size, radius, spacing; persisted; resettable | Appearance › Element overrides | `MaterialElementOverrides`, `GUI_ElementOverrides` |

## Interface quality

| Rule | Where | Qt |
| --- | --- | --- |
| Keyboard reachability and visible focus | Tab through any destination | 3px ring, offset 2, on every interactive widget |
| Correct roles, names, states | Tab strip `tablist/tab`, switches `role=switch`, live regions | `QAccessible` roles to match |
| Contrast | Both modes, four seeds | **Not yet measured.** Run a checker over `lib/kpxc.css` pairs |
| Reduced motion respected | Appearance › Motion, and the OS preference | Durations to 0, end states still applied |
| No clipping at any size, scale, density or language | Five breakpoints × three densities × three languages | Validate narrow widths and the longest localized strings |
| Controls sized to spec, adequate hit targets, 100/125/150/200% | Rail 66×56, buttons 40, chips 32, dismiss ≥44 | Logical px throughout |

## Tabs

| Rule | Where | Qt |
| --- | --- | --- |
| Browser-style tabs, not one scrolling surface | Tab strip | — |
| Overflow surface, never silently clipped | The `+N` chevron | `MaterialTabOverflow` |
| Reordering | Drag a tab | `tabMoved(from,to)` |
| Pinning | Tab list › pin | `GUI_PinnedTabs` |
| Searchable tab list wired to the builder | Tab list › `.*` and the builder button | Bar registers with `SearchRegistry` |
| Order and grouping persist across restarts | — | `GUI_TabOrder` |
| Keyboard and screen-reader operable, roving focus | — | `aria-controls` equivalent via `QAccessible` |

## Regex builder

| Rule | Where | Qt |
| --- | --- | --- |
| A usable builder in the primary interface | App bar › builder button | `MaterialRegexBuilder` |
| Guided construction: literals, classes, anchors, groups, alternation, quantifiers | Token blocks + insert row | `MaterialRegexTokens` |
| Raw pattern editor, flags, sample text, syntax feedback, live matches, capture groups | Builder, all five tabs | — |
| Copy / export | Export tab: Qt, JS, Python, grep | — |
| Engine, dialect, flags and escaping identified | Dialects tab | Says PCRE2, and names every divergence |
| Every search bar reaches the builder | Twelve surfaces | `SearchRegistry` |
| Plain text default; regex opt-in; bidirectional sync | Every `.*` chip | `GUI_SearchRegexDefault` |
| Every settings/properties surface has its own bar, including every tab | Settings 11 pages, Sheets 27 pages | — |
| Says plainly when a match sits on a different tab | Settings, with a query that matches elsewhere | — |
| Bounded evaluation, zero-width safety, backtracking protection | Builder status line and the risk banner | `MaterialRegexSafety` |

## Notifications

| Rule | Where | Qt |
| --- | --- | --- |
| Info/success/progress/non-decision errors are corner notifications, never modal | Copy a password; lock | `SnackbarHost` |
| Auto-dismiss on a sensible timeout; errors and warnings persist | 4200 ms; errors do not | `GUI_NotificationTimeout` |
| Stack without overlapping; title, body, optional actions | Trigger several | — |
| Modals reserved for decisions | Delete confirmation only | — |
| Notification centre keeps dismissed ones | Bell icon | `GUI_NotificationHistoryLimit` |
| Focusable, announced, contrasted, adequate dismiss target | Centre rows | `aria-live`, 44px targets |

## Language

| Rule | Where | Qt |
| --- | --- | --- |
| English, HK Cantonese, bilingual — persisted | Appearance › Language | `GUI_Language` |
| Two independent 1–5 sliders, wired to rendered copy, persisted | Appearance › Voice, live sample below | `GUI_FunnyLevelEn`, `GUI_FunnyLevelYue` |
| Applies to every category, no exemptions | `saveError`, `deleteConfirm`, `breachedWarn` at level 1 and 5 | — |
| Voice changes, facts never | Same file, account, error text at every level | Generator fails on placeholder mismatch |
| Disclosed at first run and in the setting | Note under the sliders | — |
| Cantonese respectful at every level | — | Review with a native reader before ship |
| Bilingual mode without crowding | Rail, app bar, snackbars | Primary prominent, secondary compact |
| Optional narrator, off by default, serialized, yields to a screen reader | Appearance › Voice | `GUI_NarratorEnabled` |

## Local version control

| Rule | Where | Qt |
| --- | --- | --- |
| Local Git-backed history in an isolated repo beside the app data directory | History | Never a `.git` inside the user's folder |
| Browse, diff, restore, label | History detail pane | — |
| Retention, pruning, export | Settings › History & backups | `History_RetentionDays`, `History_MaxRevisions` |
| Every user-managed record, settings included | History kind filter › Settings | — |
| **Restore is a new revision, never a rewrite** | Banner above the timeline; revision r011 | Append-only |
| Snapshots preserve encryption; AAD bound to a stable id | — | **Not visible in the prototype.** A restored row gets a fresh id; AAD bound to an autoincrement id makes the data undecryptable in a way that looks exactly like corruption |
| Labels say what changed | Every revision label | — |
| A failed history write never fails the user's operation | — | — |

## Changelog

| Rule | Where | Qt |
| --- | --- | --- |
| Every released version, not just the newest | Changelog, six versions | — |
| Version, date, categorized changes | Each card | — |
| Date filter with presets, typed locale and ISO dates | The from/to field | — |
| Invalid input reported inline, typed text kept | Type `19-8` | — |
| Regex search wired to the builder | `.*` chip | — |
| Search and date filter compose | Use both | — |
| Honest empty state | A query that matches nothing | — |
| Export honouring filter and search, range stated in the file | Export button label changes | — |
| Never invent entries; a version with no changes says so | 2.7.7 | — |

## External editor

| Rule | Where | Qt |
| --- | --- | --- |
| Detect installed editors | Tools › External editor | `MaterialExternalEditor::detect()` |
| Let the user add or choose one | Add an editor… | `GUI_ExternalEditorPath` |
| Open the project folder or a file | Palette › Open database folder | `open()` |
| Persist the choice | — | — |
| Degrade with a clear message when none is found | Sublime Text row, shown as not found | `failed()` emits both languages |

## Dim sum

| Rule | Where | Qt |
| --- | --- | --- |
| 1% chance at startup, drawn fresh, never twice per launch | Real in the prototype; Appearance has a force button | — |
| Name in both languages, honouring language mode | Card | Correct at every funny level |
| Non-blocking, auto-dismissing, never gates startup or steals focus | 6 s auto-dismiss | Suppressed on first run, error paths, updates, mid-task |
| Bundled local assets, no network, no CDN | Placeholder names `share/dimsum/<id>.png` | — |
| Meaningful alt text; reduced motion and quiet honoured | — | — |
| Persisted off switch, honoured absolutely | Card › Turn off; Appearance › Motion | `GUI_DimSumEnabled` |
