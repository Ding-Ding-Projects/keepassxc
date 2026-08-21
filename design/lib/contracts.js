// Per-widget Qt/C++ contracts, read by the inspector drawer.
//
// Every element in the prototype that maps onto a real widget carries
// data-contract="<id>"; the inspector walks up from the click to the nearest
// one and prints this row. "class" names the widget that owns the element,
// "replaces" names the class in src/gui/material/ this design supersedes when
// the Expressive rethink outgrew it, "files" lists create vs. modify.

export const CONTRACTS = {
  shell: {
    title: 'Application shell',
    class: 'Material::ExpressiveShell',
    replaces: 'Material::Shell (src/gui/material/MaterialShell.{h,cpp})',
    role: 'md.sys.color.surface — Role::Surface',
    metrics: 'Rail 88 · app bar 64 · tab strip 48 · rail item 66',
    config: [],
    signals: [
      'destinationChanged(const QString& id)',
      'breakpointChanged(Shell::Breakpoint bp)'
    ],
    files: [
      ['modify', 'src/gui/material/MaterialShell.h'],
      ['modify', 'src/gui/material/MaterialShell.cpp'],
      ['create', 'src/gui/material/MaterialBreakpoints.h'],
      ['modify', 'src/gui/MainWindow.cpp — addDestination() calls, line 777']
    ],
    notes: 'Shell gains a Breakpoint enum and re-parents the rail into a bottom bar below 600 logical px. addDestination() is unchanged; the rail asks the shell which form it is in.'
  },
  chrome: {
    title: 'Window chrome',
    class: 'Material::WindowChrome',
    replaces: null,
    role: 'Role::SurfaceContainerLow, 1dp elevation tint',
    metrics: 'Height 44 · caption buttons 46×44 · drag region excludes the tab strip',
    config: ['GUI_ApplicationTheme', 'GUI_CustomWindowChrome (new, bool, default true on Windows)'],
    signals: ['minimiseRequested()', 'maximiseRequested()', 'closeRequested()'],
    files: [
      ['modify', 'src/gui/material/MaterialWindowChrome.cpp'],
      ['modify', 'src/gui/osutils/winutils/WinUtils.cpp — WM_NCHITTEST forwarding']
    ],
    notes: 'WinUtils already applies WDA_EXCLUDEFROMCAPTURE to every top-level window; a custom frame does not change that. Launch with --allow-screencapture to capture this surface.'
  },
  rail: {
    title: 'Navigation rail',
    class: 'Material::NavigationRail',
    replaces: null,
    role: 'Role::SurfaceContainer · active indicator Role::SecondaryContainer',
    metrics: 'Width 88 · item 66×56 · indicator radius 18 → 28 on press (shape morph)',
    config: ['GUI_MaterialRailSublabels (new, bool, default true)'],
    signals: ['destinationActivated(const QString& id)'],
    files: [
      ['modify', 'src/gui/material/MaterialNavigationRail.cpp'],
      ['create', 'src/gui/material/MaterialShapeMorph.{h,cpp}']
    ],
    notes: 'The rail is painted, not built from widgets, so nothing on it is a QAction and none of it reaches the command palette on its own. Shell carries one QAction per destination for exactly this reason — keep that when adding a destination.'
  },
  appbar: {
    title: 'Top app bar',
    class: 'Material::TopAppBar',
    replaces: null,
    role: 'Role::Surface, no elevation until the content scrolls',
    metrics: 'Height 64 · title TitleLarge 22 · icon buttons 40',
    config: [],
    signals: ['searchSubmitted(const QString&, bool isRegex)'],
    files: [['modify', 'src/gui/material/MaterialTopAppBar.cpp']],
    notes: 'Title is bilingual when GUI_Language is "both": TitleLarge English over LabelSmall Cantonese, both from the same key so they can never disagree.'
  },
  search: {
    title: 'Search bar (+ regex)',
    class: 'Material::SearchBar',
    replaces: null,
    role: 'Role::SurfaceContainerHigh · focus ring Role::Primary 2px',
    metrics: 'Height 52 (44 on surfaces) · radius Full · leading icon 20',
    config: ['GUI_SearchRegexDefault (new, bool, default false)', 'GUI_SearchFlags (new, QString)'],
    signals: [
      'textChanged(const QString&)',
      'modeChanged(bool regex)',
      'builderRequested(Material::SearchBar* origin)'
    ],
    files: [
      ['modify', 'src/gui/material/MaterialSearchBar.cpp'],
      ['create', 'src/gui/material/MaterialSearchRegistry.{h,cpp}']
    ],
    notes: 'Plain text is the default mode and regex is opt-in. Query, pattern, flags, validation and mode synchronise bidirectionally — editing the pattern in the builder writes back to the bar that opened it, never to the vault bar unconditionally. SearchRegistry is new: it enumerates every live SearchBar so the builder can be anchored to whichever one has focus.'
  },
  tabstrip: {
    title: 'Database tab strip',
    class: 'Material::TabStrip',
    replaces: null,
    role: 'Role::SurfaceContainerLow · active tab Role::SecondaryContainer',
    metrics: 'Strip 48 · tab 38 · radius Full · overflow chevron at 40',
    config: ['GUI_TabOrder (new, QStringList)', 'GUI_PinnedTabs (new, QStringList)'],
    signals: ['tabActivated(int)', 'tabMoved(int from, int to)', 'tabPinned(int, bool)', 'tabListRequested()'],
    files: [
      ['modify', 'src/gui/material/MaterialTabStrip.cpp'],
      ['create', 'src/gui/material/MaterialTabOverflow.{h,cpp}']
    ],
    notes: 'Tabs never clip silently: past the available width they collapse into the overflow sheet, which carries its own search bar wired to the regex builder. Roles are tablist/tab/tabpanel with roving tabindex; order and pinning persist across restarts.'
  },
  entrylist: {
    title: 'Entry list',
    class: 'Material::EntryDelegate + EntryListModel',
    replaces: null,
    role: 'Row Role::SurfaceContainerLowest · selected Role::SecondaryContainer',
    metrics: 'Row height 40 / 52 / 64 by density · radius Row 16 · health dot 8',
    config: ['GUI_MaterialDensity', 'GUI_EntrySortKey (new, enum Title|Modified|Health)'],
    signals: ['entryActivated(Entry*)', 'sortKeyChanged(EntryListModel::SortKey)'],
    files: [
      ['modify', 'src/gui/material/MaterialEntryDelegate.cpp'],
      ['modify', 'src/gui/material/MaterialVaultScreen.cpp']
    ],
    notes: 'EntryListModel is a QSortFilterProxyModel over the 17-column EntryModel keeping only the title column and answering delegate roles from sibling columns. Health verdicts need the whole database — re-use cannot be judged per row — so setDatabase() must be called before healthOf().'
  },
  detail: {
    title: 'Entry detail pane',
    class: 'Material::EntryDetail',
    replaces: null,
    role: 'Role::SurfaceContainerLow · protected fields Role::SurfaceContainerHighest',
    metrics: 'Width 392 · radius ExtraLarge 28 · field row 56',
    config: ['Security_ClearClipboardTimeout', 'Security_HidePasswordPreviewPanel'],
    signals: ['copyRequested(const QString& field)', 'revealToggled(bool)', 'editRequested(Entry*)'],
    files: [['modify', 'src/gui/material/MaterialEntryDetail.cpp']],
    notes: 'Never render a password into a widget that survives a lock. Reveal state resets on databaseLocked(); the copy action arms the existing clipboard clear timer rather than a new one.'
  },
  fab: {
    title: 'Extended FAB + menu',
    class: 'Material::ExpressiveFab',
    replaces: 'Material::ExtendedFab (in MaterialButtons.cpp)',
    role: 'Role::PrimaryContainer · menu items Role::SurfaceContainerHigh',
    metrics: 'Height 56 · radius 28 → 16 on expand (shape morph) · menu stagger 40ms',
    config: [],
    signals: ['actionTriggered(const QString& id)', 'expandedChanged(bool)'],
    files: [
      ['modify', 'src/gui/material/MaterialButtons.cpp'],
      ['create', 'src/gui/material/MaterialFabMenu.{h,cpp}']
    ],
    notes: 'An earlier audit attributed the extended FAB metrics to FilledButton. They are separate: FilledButton is 40 high with radius Full, the FAB is 56 with radius 28.'
  },
  snackbar: {
    title: 'Snackbar host',
    class: 'Material::SnackbarHost',
    replaces: null,
    role: 'Role::InverseSurface / Role::InverseOnSurface · error Role::ErrorContainer',
    metrics: 'Width 344–512 · radius Small 8 · timeout 4200ms · stack gap 8',
    config: ['GUI_NotificationTimeout (new, int ms, default 4200)'],
    signals: ['actionTriggered(const QString& id)', 'dismissed(const QString& id)'],
    files: [
      ['modify', 'src/gui/material/MaterialSnackbar.cpp'],
      ['modify', 'src/gui/material/MaterialNotificationCentre.cpp']
    ],
    notes: 'Informational, success, progress and non-decision errors are snackbars, never modal. Errors and warnings do not auto-dismiss. Every snackbar is also appended to the notification centre so a dismissal is not a deletion.'
  },
  notifications: {
    title: 'Notification centre',
    class: 'Material::NotificationCentre',
    replaces: null,
    role: 'Role::SurfaceContainer · unread dot Role::Primary',
    metrics: 'Drawer 400 wide · row 72 · radius ExtraLarge 28 on the leading edge',
    config: ['GUI_NotificationHistoryLimit (new, int, default 200)'],
    signals: ['cleared()', 'itemActivated(const QString& id)'],
    files: [['modify', 'src/gui/material/MaterialNotificationCentre.cpp']],
    notes: 'Focusable rows, announced by the screen reader, dismiss target ≥ 44px. Carries its own search bar wired to the builder.'
  },
  palette: {
    title: 'Command palette',
    class: 'Material::CommandPalette',
    replaces: null,
    role: 'Role::SurfaceContainerHigh · scrim onSurface @ 32%',
    metrics: 'Width 640 · radius ExtraLarge 28 · row 48',
    config: [],
    signals: ['commandTriggered(QAction*)'],
    files: [['modify', 'src/gui/material/MaterialCommandPalette.cpp']],
    notes: 'Lists commands by walking the window action tree, so a rail destination only appears because Shell carries a QAction for it. Go To is a deliberate divergence from the design’s six groups: without it half the rail would have no keyboard route.'
  },
  regexbuilder: {
    title: 'Regex builder',
    class: 'Material::RegexBuilder',
    replaces: null,
    role: 'Role::SurfaceContainerLow · token chips Role::SecondaryContainer',
    metrics: 'Sheet 960×640 · token chip 32 · match table row 36',
    config: ['GUI_RegexPresets (new, QStringList of JSON)', 'GUI_RegexLastPattern (new, QString)'],
    signals: ['patternAccepted(const QString& pattern, QRegularExpression::PatternOptions)'],
    files: [
      ['modify', 'src/gui/material/MaterialRegexBuilder.cpp'],
      ['create', 'src/gui/material/MaterialRegexTokens.{h,cpp}'],
      ['create', 'src/gui/material/MaterialRegexSafety.{h,cpp}']
    ],
    notes: 'The engine is QRegularExpression (PCRE2), NOT ECMAScript — the dialect selector says so on screen. Evaluation is bounded: 512-char pattern, 20 000-char sample, 120 ms budget, 500 matches. Nested-quantifier shapes are flagged statically before the run. The accepted pattern returns to the search bar that opened the builder.'
  },
  appearance: {
    title: 'Appearance editor',
    class: 'Material::SettingsScreen',
    replaces: null,
    role: 'Role::Surface · preview card Role::SurfaceContainerLowest',
    metrics: 'Preview 320×200 · swatch 44 · slider track 4',
    config: [
      'GUI_MaterialSeed', 'GUI_MaterialDensity', 'GUI_ApplicationTheme',
      'GUI_FontFamily (new)', 'GUI_FontScale (new, double 0.85–1.4)', 'GUI_FontWeight (new)',
      'GUI_ElementOverrides (new, QJsonObject keyed by element id)'
    ],
    signals: ['Material::Theme::changed()'],
    files: [
      ['modify', 'src/gui/material/MaterialSettingsScreen.cpp'],
      ['create', 'src/gui/material/MaterialElementOverrides.{h,cpp}']
    ],
    notes: 'Per-element overrides persist by element id and reset to default individually. Theme::reload() restyles the whole application from the seed; widgets must never hard-code a colour, only ask for a Role.'
  },
  history: {
    title: 'History screen',
    class: 'Material::HistoryScreen + HistoryStore',
    replaces: null,
    role: 'Role::SurfaceContainerLow · restore chip Role::PrimaryContainer',
    metrics: 'Timeline gutter 40 · revision card radius Large 14',
    config: ['History_RetentionDays (new, int, default 365)', 'History_MaxRevisions (new, int)'],
    signals: ['restoreRequested(const QString& revisionId)', 'exportRequested()'],
    files: [
      ['modify', 'src/gui/material/MaterialHistoryStore.cpp'],
      ['modify', 'src/gui/material/MaterialHistoryScreen.cpp']
    ],
    notes: 'HistoryStore holds a QWeakPointer<Database> and follows the root group’s destruction — a strong pointer kept the decrypted database alive past lock. Rows hold a QPointer to the revision itself, never a position in Entry::historyItems(): truncateHistory() drops from the oldest end and every surviving index shifts. Never persist entry content, passwords or attachment bytes to the plaintext JSONL log.'
  },
  changelog: {
    title: 'Changelog viewer',
    class: 'Material::ChangelogScreen + ChangelogFeed',
    replaces: null,
    role: 'Role::Surface · version header Role::PrimaryContainer',
    metrics: 'Version card radius ExtraLarge 28 · date chip 32',
    config: ['GUI_ChangelogLastSeen (new, QString version)'],
    signals: ['filterChanged(QDate from, QDate to)', 'exportRequested(const QString& format)'],
    files: [['modify', 'src/gui/material/MaterialChangelogScreen.cpp']],
    notes: 'Covers every released version, not just the newest. Date filter and search compose rather than override. A version with no recorded changes says so — never invent entries to fill a gap.'
  },
  settings: {
    title: 'Settings hub',
    class: 'Material::SettingsHub',
    replaces: null,
    role: 'Role::Surface · nav Role::SurfaceContainerLow',
    metrics: 'Nav 266 · row 56 · section header LabelSmall 11 uppercase',
    config: ['every Config key the hub binds — one per row, listed on the row itself'],
    signals: ['pageChanged(const QString& id)'],
    files: [['modify', 'src/gui/material/MaterialSettingsHub.cpp']],
    notes: 'Adopts the stock ApplicationSettingsWidget as its classic editor so no option became unreachable during the migration. Rows were MOVED off General and Security into Auto-Type, Generator and Shortcuts, not copied — nothing is bound twice. Shortcuts reads ActionCollection, so a rebound key shows up and a new action cannot go missing.'
  },
  dimsum: {
    title: 'Dim sum surprise',
    class: 'Material::DimSum',
    replaces: null,
    role: 'Role::SurfaceContainerHigh · radius ExtraLarge 28',
    metrics: 'Card 320 wide · image 320×180 · auto-dismiss 6000ms',
    config: ['GUI_DimSumEnabled (new, bool, default true)'],
    signals: ['dismissed()'],
    files: [['modify', 'src/gui/material/MaterialDimSum.cpp']],
    notes: '1% chance per launch, drawn fresh, never twice in one launch. Suppressed on first run, on an error path, during an update, and whenever the user is mid-task. Images are bundled local assets — no network fetch, no CDN. Alt text names the dish; the dish name stays correct at every funny level.'
  },
  language: {
    title: 'Language + funny level',
    class: 'Material::VoiceSettings',
    replaces: 'Material::Voice (src/gui/material/MaterialVoice.{h,cpp})',
    role: 'Role::SurfaceContainer · slider active Role::Primary',
    metrics: 'Slider 5 discrete stops · row 72',
    config: [
      'GUI_Language (new, enum en|yue|both)',
      'GUI_FunnyLevelEn (new, int 1–5)',
      'GUI_FunnyLevelYue (new, int 1–5)',
      'GUI_NarratorEnabled (new, bool, default false)'
    ],
    signals: ['languageChanged(Voice::Lang)', 'funnyLevelChanged(Voice::Lang, int)'],
    files: [
      ['modify', 'src/gui/material/MaterialVoice.cpp'],
      ['create', 'src/gui/material/MaterialVoiceStrings.{h,cpp}']
    ],
    notes: 'Two independent sliders, actually wired to rendered copy, persisted. The level applies to every category with no exemptions — destructive, financial, security, accessibility and error copy included. It changes voice, never facts: which file, which account, what is irreversible and what the error was appear identically at level 1 and level 5. Disclosed at first run. The narrator is off by default, serialised so utterances never overlap, and yields to an active screen reader.'
  },
  editor: {
    title: 'External editor',
    class: 'Material::ExternalEditor',
    replaces: null,
    role: 'Role::SurfaceContainerLow',
    metrics: 'Row 64 · icon 24',
    config: ['GUI_ExternalEditorPath (new, QString)', 'GUI_ExternalEditorArgs (new, QString)'],
    signals: ['openRequested(const QString& path)'],
    files: [['create', 'src/gui/material/MaterialExternalEditor.{h,cpp}']],
    notes: 'Detects installed editors, lets the user add one, opens the database folder or a selected file. Degrades with a clear message when none is found — never a silent no-op.'
  },
  reports: {
    title: 'Reports',
    class: 'Material::ReportsScreen + ReportsFeed',
    replaces: null,
    role: 'Role::Surface · finding cards by severity container role',
    metrics: 'Stat value DisplaySmall 44 light · card radius ExtraLarge 28',
    config: ['Security_ExcludeFromReports (per entry)'],
    signals: ['findingActivated(const QString& reportId, Entry*)'],
    files: [['modify', 'src/gui/material/MaterialReportsScreen.cpp']],
    notes: 'ReportsFeed must not walk the live entry tree from a worker thread while a nested event loop runs on the GUI thread — that was a real crash. Snapshot on the GUI thread, analyse off it.'
  },
  sheet: {
    title: 'Spec sheet',
    class: 'Material::SpecSheet + SheetCatalogue',
    replaces: null,
    role: 'Role::Surface · nav Role::SurfaceContainerLow',
    metrics: 'Nav 266 · page padding by density · row 48',
    config: [],
    signals: ['pageChanged(int)'],
    files: [
      ['modify', 'src/gui/material/MaterialSpecSheet.cpp'],
      ['modify', 'utils/design/sheets.json'],
      ['modify', 'utils/generate_sheet_catalogue.mjs']
    ],
    notes: 'MaterialSheetCatalogue.{h,cpp} is GENERATED from utils/design/sheets.json — never hand-edited. Re-run utils/generate_sheet_catalogue.mjs to diff a transcription instead of trusting it.'
  }
};

export const UH_RULES = [
  { id: 'm3', rule: 'Full Material Design 3 (M3 Expressive) — tokens, type, shape, elevation, motion, anatomy. Zero legacy elements.', where: 'Every surface. Tokens in MaterialTheme.cpp; no widget hard-codes a colour.' },
  { id: 'appearance', rule: 'Persisted runtime theme, density, seed and full font customization with live preview and CJK-safe fallback.', where: 'Appearance destination' },
  { id: 'element-editor', rule: 'In-app per-element editors for font, colour, size, radius and spacing, persisted and resettable.', where: 'Appearance → Element overrides' },
  { id: 'a11y', rule: 'Keyboard reachability, visible focus, correct roles, contrast, reduced motion, screen-reader structure.', where: 'Shell-wide; focus rings on every interactive element' },
  { id: 'clipping', rule: 'No clipped, truncated, overlapping or off-screen text at any supported size, scale, density or language.', where: 'Five breakpoints × three densities × three languages' },
  { id: 'tabs', rule: 'Browser-style tabs with overflow surface, reordering, pinning, searchable tab list, persisted order.', where: 'Tab strip' },
  { id: 'regex', rule: 'A usable regex builder in the primary interface; every search bar reaches it; every settings surface has its own bar.', where: 'Regex builder + 12 search surfaces' },
  { id: 'notify', rule: 'Non-blocking corner notifications; modals only for decisions; notification centre keeps dismissed ones.', where: 'Snackbar host + notification centre' },
  { id: 'lang', rule: 'English / HK Cantonese / bilingual, plus two independent 1–5 funny sliders wired to real copy.', where: 'Appearance → Voice' },
  { id: 'vcs', rule: 'Local Git-backed history of every user-managed record including settings; restore is a new revision, never a rewrite.', where: 'History destination' },
  { id: 'changelog', rule: 'In-app changelog for every released version, with calendar date filter, regex search and export.', where: 'Changelog destination' },
  { id: 'editor', rule: 'Configurable open-in-external-editor with detection and graceful degradation.', where: 'Tools → External editor' },
  { id: 'dimsum', rule: '1% chance per launch of a dim sum dish, bilingual name, bundled asset, non-blocking, disableable.', where: 'Dim sum card' }
];
