# Native menu route inventory

All native routes below pass through `Application::bootstrap` installing
`Material::MenuSearch`. The application event filter observes `QEvent::Show`,
after dynamic population. Tests must prove this registration, not merely find
a `QMenu` spelling. Every listed command menu needs its own local search and
adjacent full builder. No native captures have been recorded for this change.

| Owner | Exact route | Strategy |
| --- | --- | --- |
| MainWindow.cpp | `m_entryContextMenu` | Automatic command search |
| MainWindow.cpp | `m_entryNewContextMenu` | Automatic command search |
| MainWindow.cpp | `autotypeMenu` | Automatic command search |
| MainWindow.cpp | `databaseLockMenu` | Automatic command search |
| MainWindow.cpp | dynamic URL menu | Automatic command search |
| MainWindow.ui | `menuFile`, `menuRecentDatabases`, `menuExport`, `menuRemoteSync` | Automatic, after population |
| MainWindow.ui | `menuHelp`, `menuEntries`, `menuEntryCopyAttribute`, `menuEntryTotp`, `menuTags` | Automatic, nested independently |
| MainWindow.ui | `menuGroups`, `menuTools`, `menuView`, `menuTheme` | Automatic command search |
| DatabaseWidget.cpp | group and entry context-menu requests | MainWindow-owned menus |
| MaterialVaultScreen.cpp | group and entry context-menu requests | Original native menu routes |
| MaterialVaultScreen.cpp | `m_groupScopeMenu` | Existing search reused; local builder |
| MaterialShell.cpp | `m_moreMenu` | Existing search reused; local builder |
| MaterialShell.cpp | `m_goToMenu`, `m_viewMenu` | Metadata menus, intercepted if shown |
| MaterialSelect.cpp | `m_popup` | Existing search reused; local builder |
| MaterialTopAppBar.cpp | `m_overflowMenu` | Automatic command search |
| MaterialTabStrip.cpp | stack-local `QMenu menu(this)` | Automatic; same-popup builder lifetime |
| EntryView.cpp | `m_headerMenu` | Automatic command search |
| MergeDialog.cpp | `m_headerContextMenu` and horizontal-header `Qt::ActionsContextMenu` | Automatic, including Qt-created menu |
| TagView.cpp | both stack-local menus | Automatic; same-popup builder lifetime |
| EditEntryWidget.cpp | `createPresetsMenu()` | Automatic command search |
| EditWidgetIcons.cpp | `createApplyIconToMenu()` | Automatic command search |
| EntryAttachmentsWidget.cpp | `addButtonMenu` | Automatic command search |
| ReportsWidgetPasskeys.cpp | context menu | Automatic command search |
| ReportsWidgetHibp.cpp | context menu | Automatic command search |
| ReportsWidgetHealthcheck.cpp | context menu | Automatic command search |
| ReportsWidgetBrowserStatistics.cpp | context menu | Automatic command search |
| SearchWidget.cpp | `m_searchMenu` | Automatic command search |
| QLineEdit | `createStandardContextMenu()` and default context events | Automatic, synthetic fixture |
| QPlainTextEdit / QTextEdit | Qt-generated standard menu | Automatic, synthetic fixture |
| QWidget | `Qt::ActionsContextMenu` | Automatic, generated at invocation |
| MaterialDateField.cpp | `m_picker` | Search row installed; custom day-grid filtering pending |

Completion popups, target-specific menus on elements
without a context menu, appearance editors and element locks remain separate
work. This inventory deliberately does not claim their implementation.

## Model-preserving combobox choice popups

These 45 existing widgets use `Material::ComboBox::showPopup()` and its separate
filtering proxy/view. Their `QComboBox` source models, editable fields, completers,
validators and insertion policies stay owned by their original controls.
The list below is hand-written from the registered production widgets. Native
interaction and capture evidence remain pending for each destination.

| Form | Exact widget names | Count |
| --- | --- | --- |
| `ApplicationSettingsWidgetGeneral.ui` | `alternativeSaveComboBox`, `urlDoubleClickComboBox`, `languageComboBox`, `fontSizeComboBox`, `trayIconAppearance` | 5 |
| `CsvImportWidget.ui` | `urlCombo`, `usernameCombo`, `lastModifiedCombo`, `iconCombo`, `totpCombo`, `titleCombo`, `notesCombo`, `createdCombo`, `passwordCombo`, `groupCombo`, `tagsCombo`, `comboBoxCodec`, `comboBoxTextQualifier`, `comboBoxFieldSeparator`, `comboBoxComment` | 15 |
| `DatabaseOpenWidget.ui` | `hardwareKeyCombo` | 1 |
| `YubiKeyEditWidget.ui` | `comboChallengeResponse` | 1 |
| `DatabaseSettingsWidgetEncryption.ui` | `compatibilitySelection`, `kdfComboBox`, `algorithmComboBox` | 3 |
| `ExportDialog.ui` | `sortingStrategy` | 1 |
| `ImageAttachmentsWidget.ui` | `zoomComboBox` | 1 |
| `EditGroupWidgetMain.ui` | `autotypeComboBox`, `searchComboBox` | 2 |
| `EditGroupWidgetBrowser.ui` | `browserIntegrationHideEntriesComboBox`, `browserIntegrationSkipAutoSubmitComboBox`, `browserIntegrationOnlyHttpAuthComboBox`, `browserIntegrationNotHttpAuthComboBox`, `browserIntegrationOmitWwwCombobox`, `browserIntegrationRestrictKeyCombobox` | 6 |
| `EditEntryWidgetSSHAgent.ui` | `attachmentComboBox` | 1 |
| `EditEntryWidgetMain.ui` | `usernameComboBox` | 1 |
| `TextAttachmentsPreviewWidget.ui` | `typeComboBox` | 1 |
| `PasswordGeneratorWidget.ui` | `comboBoxWordList`, `wordCaseComboBox` | 2 |
| `PasskeyImportDialog.ui` | `selectDatabaseCombobBox`, `selectGroupComboBox`, `selectEntryComboBox` | 3 |
| `TotpSetupDialog.ui` | `algorithmComboBox` | 1 |
| `ImportWizardPageSelect.ui` | `existingDatabaseChoice` | 1 |

The four plain `QComboBox` controls in `BrowserSettingsWidget.ui` (one),
`OpenSSHKeyGenDialog.ui` (two) and `EditGroupWidgetKeeShare.ui` (one) remain
excluded. `WindowSelectComboBox`, completion popups and calendar pickers are
also excluded. No production model is replaced by `Material::Select`.

See [Searchable combobox choices](../docs/features/search/combobox-choice-search.md)
for behavior, limits, privacy and the distinction between Qt verification and
missing native evidence.
