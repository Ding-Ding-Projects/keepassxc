/*
 *  Copyright (C) 2025 KeePassXC Team <team@keepassxc.org>
 *  Copyright (C) 2010 Felix Geyer <debfx@fobos.de>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 or (at your option)
 *  version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "MainWindow.h"
#include "ui_MainWindow.h"

#include <QCloseEvent>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QFontDialog>
#include <QFontInfo>
#include <QList>
#include <QLineEdit>
#include <QLocale>
#include <QMimeData>
#include <QShortcut>
#include <QSet>
#include <QStatusBar>
#include <QSysInfo>
#include <QTimer>
#include <QTabBar>
#include <QToolButton>
#include <QWindow>

#include <algorithm>

#include "config-keepassx.h"

#include "Application.h"
#include "Clipboard.h"
#include "autotype/AutoType.h"
#include "core/Database.h"
#include "core/Entry.h"
#include "core/EntryAttachments.h"
#include "core/Group.h"
#include "core/InactivityTimer.h"
#include "core/Metadata.h"
#include "core/Resources.h"
#include "core/Tools.h"
#include "crypto/kdf/Kdf.h"
#include "format/KeePass2.h"
#include "gui/AboutDialog.h"
#include "gui/ActionCollection.h"
#include "gui/Icons.h"
#include "gui/MessageBox.h"
#include "gui/SearchWidget.h"
#include "gui/ShortcutSettingsPage.h"
#include "gui/entry/EntryView.h"
#include "gui/material/MaterialChangelogFeed.h"
#include "gui/material/MaterialChangelogScreen.h"
#include "gui/material/MaterialCommandPalette.h"
#include "gui/material/MaterialGeneratorSheet.h"
#include "gui/material/MaterialHistoryFeed.h"
#include "gui/material/MaterialHistoryScreen.h"
#include "gui/material/MaterialHistoryStore.h"
#include "gui/material/MaterialNavigationRail.h"
#include "gui/material/MaterialNotificationCentre.h"
#include "gui/material/MaterialNotifier.h"
#include "gui/material/MaterialCaptureRoute.h"
#include "gui/material/MaterialRegexBuilder.h"
#include "gui/material/MaterialReportsFeed.h"
#include "gui/material/MaterialReportsScreen.h"
#include "gui/material/MaterialSearchBar.h"
#include "gui/material/MaterialSearchRegistry.h"
#include "gui/material/MaterialSettingsHub.h"
#include "gui/material/MaterialSettingsScreen.h"
#include "gui/material/MaterialSheetCatalogue.h"
#include "gui/material/MaterialShell.h"
#include "gui/material/MaterialSpecSheet.h"
#include "gui/material/MaterialTabStrip.h"
#include "gui/material/MaterialTheme.h"
#include "gui/material/MaterialTopAppBar.h"
#include "gui/material/MaterialVaultScreen.h"
#include "gui/material/MaterialVoice.h"
#ifdef Q_OS_WIN
#include "gui/material/MaterialTitleBar.h"
#include "gui/material/MaterialWindowChrome.h"
#endif
#include "gui/osutils/OSUtils.h"
#include "gui/remote/RemoteSettings.h"
#include "keeshare/KeeShare.h"
#include "keeshare/SettingsPageKeeShare.h"
#include "keys/drivers/YubiKey.h"

#ifdef KPXC_FEATURE_UPDATES
#include "networking/UpdateChecker.h"
#endif

#ifdef KPXC_FEATURE_SSHAGENT
#include "sshagent/AgentSettingsPage.h"
#include "sshagent/SSHAgent.h"
#endif

#ifdef KPXC_FEATURE_BROWSER
#include "browser/BrowserService.h"
#endif

const QString MainWindow::BaseWindowTitle = "KeePassXC";

MainWindow* g_MainWindow = nullptr;
MainWindow* getMainWindow()
{
    return g_MainWindow;
}

namespace
{
    /** The Material shell that owns the window interior, or nullptr before it exists. */
    Material::Shell* shell()
    {
        return Material::Shell::instance();
    }

    /** Runtime tab id for one live database widget. Persistence uses a separate path digest. */
    QString tabIdFor(const DatabaseWidget* dbWidget)
    {
        return QStringLiteral("db-%1").arg(reinterpret_cast<quintptr>(dbWidget));
    }

    /**
     * The app bar's second line for a database: the design's
     * "KDBX 4.1 · AES-256 · Argon2id · <path>".
     *
     * A locked database has not read its header yet, so format, cipher and KDF
     * are unknown and the line falls back to the path alone rather than
     * reporting defaults that may not be what is on disk.
     */
    QString databaseSubtitle(DatabaseWidget* dbWidget)
    {
        const auto db = dbWidget ? dbWidget->database() : QSharedPointer<Database>();
        if (!db) {
            return {};
        }
        const QString path = QDir::toNativeSeparators(db->filePath());
        if (dbWidget->isLocked()) {
            return path;
        }

        const quint32 version = db->formatVersion();
        QStringList parts{QStringLiteral("KDBX %1.%2").arg(version >> 16).arg(version & 0xffff),
                          KeePass2::cipherToString(db->cipher())};
        if (const auto kdf = db->kdf()) {
            parts << KeePass2::kdfToString(kdf->uuid());
        }
        parts << path;
        return parts.join(QStringLiteral(" · "));
    }

    /** The database behind a widget, or a null pointer when it is locked or absent. */
    QSharedPointer<Database> unlockedDatabase(DatabaseWidget* dbWidget)
    {
        if (!dbWidget || dbWidget->isLocked() || !dbWidget->database() || !dbWidget->database()->rootGroup()) {
            return {};
        }
        return dbWidget->database();
    }
} // namespace

MainWindow::MainWindow()
    : m_ui(new Ui::MainWindow())
{
    g_MainWindow = this;

    m_ui->setupUi(this);

#ifdef Q_OS_MACOS
    macUtils()->configureWindowAndHelpMenus(this, m_ui->menuHelp);
#endif

    setAcceptDrops(true);

    if (config()->get(Config::GUI_CompactMode).toBool()) {
        m_ui->toolBar->setIconSize({20, 20});
    }

    // Setup the search widget in the toolbar
    m_searchWidget = new SearchWidget();
    m_searchWidget->connectSignals(m_actionMultiplexer);
    m_searchWidgetAction = m_ui->toolBar->addWidget(m_searchWidget);
    m_searchWidgetAction->setEnabled(false);

    new QShortcut(QKeySequence::Find, this, SLOT(focusSearchWidget()));

    // The tool bar is only ever raised to carry the search field, so it goes
    // straight back down when the search is done with it.
    connect(m_searchWidget, &SearchWidget::searchCanceled, this, [this] {
        m_ui->toolBar->setExpanded(false);
        m_ui->toolBar->setVisible(false);
    });
    connect(m_searchWidget, &SearchWidget::lostFocus, this, [this] {
        m_ui->toolBar->setExpanded(false);
        m_ui->toolBar->setVisible(false);
    });

    m_countDefaultAttributes = m_ui->menuEntryCopyAttribute->actions().size();

    m_entryContextMenu = new QMenu(this);
    m_entryContextMenu->setSeparatorsCollapsible(true);
    m_entryContextMenu->addAction(m_ui->actionEntryRestore);
    m_entryContextMenu->addSeparator();
    m_entryContextMenu->addAction(m_ui->actionEntryCopyUsername);
    m_entryContextMenu->addAction(m_ui->actionEntryCopyPassword);
    m_entryContextMenu->addAction(m_ui->actionEntryCopyURL);
    m_entryContextMenu->addAction(m_ui->menuEntryCopyAttribute->menuAction());
    m_entryContextMenu->addAction(m_ui->menuEntryTotp->menuAction());
    m_entryContextMenu->addAction(m_ui->menuTags->menuAction());
    m_entryContextMenu->addSeparator();
    m_entryContextMenu->addAction(m_ui->actionEntryAutoType);
    m_entryContextMenu->addSeparator();
#ifdef KPXC_FEATURE_BROWSER
    m_entryContextMenu->addAction(m_ui->actionEntryImportPasskey);
    m_entryContextMenu->addAction(m_ui->actionEntryRemovePasskey);
    m_entryContextMenu->addSeparator();
#endif
    m_entryContextMenu->addAction(m_ui->actionEntryEdit);
    m_entryContextMenu->addAction(m_ui->actionEntryExpire);
    m_entryContextMenu->addAction(m_ui->actionEntryClone);
    m_entryContextMenu->addAction(m_ui->actionEntryDelete);
    m_entryContextMenu->addAction(m_ui->actionEntryNew);
    m_entryContextMenu->addSeparator();
    m_entryContextMenu->addAction(m_ui->actionEntryMoveUp);
    m_entryContextMenu->addAction(m_ui->actionEntryMoveDown);
    m_entryContextMenu->addSeparator();
    m_entryContextMenu->addAction(m_ui->actionEntryOpenUrl);
    m_entryContextMenu->addAction(m_ui->actionEntryDownloadIcon);
    m_entryContextMenu->addSeparator();
    m_entryContextMenu->addAction(m_ui->actionEntryAddToAgent);
    m_entryContextMenu->addAction(m_ui->actionEntryRemoveFromAgent);

    m_entryNewContextMenu = new QMenu(this);
    m_entryNewContextMenu->addAction(m_ui->actionEntryNew);

    connect(m_ui->menuRemoteSync, &QMenu::aboutToShow, this, &MainWindow::updateRemoteSyncMenuEntries);

    // Build Entry Level Auto-Type menu
    auto autotypeMenu = new QMenu({}, this);
    autotypeMenu->addAction(m_ui->actionEntryAutoTypeSequence);
    autotypeMenu->addSeparator();
    autotypeMenu->addAction(m_ui->actionEntryAutoTypeUsername);
    autotypeMenu->addAction(m_ui->actionEntryAutoTypeUsernameEnter);
    autotypeMenu->addAction(m_ui->actionEntryAutoTypePassword);
    autotypeMenu->addAction(m_ui->actionEntryAutoTypePasswordEnter);
    autotypeMenu->addAction(m_ui->actionEntryAutoTypeTOTP);
    autotypeMenu->addAction(m_ui->actionEntryAutoTypeURL);
    autotypeMenu->addAction(m_ui->actionEntryAutoTypeURLEnter);
    m_ui->actionEntryAutoType->setMenu(autotypeMenu);
    auto autoTypeButton = qobject_cast<QToolButton*>(m_ui->toolBar->widgetForAction(m_ui->actionEntryAutoType));
    if (autoTypeButton) {
        autoTypeButton->setPopupMode(QToolButton::MenuButtonPopup);
    }

    auto databaseLockMenu = new QMenu({}, this);
    databaseLockMenu->addAction(m_ui->actionLockAllDatabases);
    m_ui->actionLockDatabaseToolbar->setMenu(databaseLockMenu);
    auto databaseLockButton =
        qobject_cast<QToolButton*>(m_ui->toolBar->widgetForAction(m_ui->actionLockDatabaseToolbar));
    if (databaseLockButton) {
        databaseLockButton->setPopupMode(QToolButton::MenuButtonPopup);
    }

    connect(m_ui->tabWidget, &DatabaseTabWidget::databaseLocked, this, &MainWindow::databaseLocked);
    connect(m_ui->tabWidget, &DatabaseTabWidget::databaseUnlocked, this, &MainWindow::databaseUnlocked);
    connect(m_ui->tabWidget, &DatabaseTabWidget::activeDatabaseChanged, this, &MainWindow::activeDatabaseChanged);
    connect(m_ui->tabWidget,
            &DatabaseTabWidget::databaseUnlockDialogFinished,
            this,
            &MainWindow::databaseUnlockDialogFinished);

    initViewMenu();
    initActionCollection();

    m_ui->settingsWidget->addSettingsPage(new ShortcutSettingsPage());

#ifdef KPXC_FEATURE_BROWSER
    connect(
        browserService(), &BrowserService::requestUnlock, m_ui->tabWidget, &DatabaseTabWidget::performBrowserUnlock);
#endif

#ifdef KPXC_FEATURE_SSHAGENT
    connect(sshAgent(), SIGNAL(error(QString)), this, SLOT(showErrorMessage(QString)));
    connect(sshAgent(), SIGNAL(enabledChanged(bool)), this, SLOT(agentEnabled(bool)));
    connect(m_ui->actionClearSSHAgent, SIGNAL(triggered()), SLOT(clearSSHAgent()));
    m_ui->settingsWidget->addSettingsPage(new AgentSettingsPage());
#else
    agentEnabled(false);
#endif

    KeeShare::init(this);
    m_ui->settingsWidget->addSettingsPage(new SettingsPageKeeShare(m_ui->tabWidget));
    connect(KeeShare::instance(),
            SIGNAL(sharingMessage(QString, MessageWidget::MessageType)),
            SLOT(displayGlobalMessage(QString, MessageWidget::MessageType)));

    connect(YubiKey::instance(), SIGNAL(userInteractionRequest()), SLOT(showYubiKeyPopup()), Qt::QueuedConnection);
    connect(YubiKey::instance(), SIGNAL(challengeCompleted()), SLOT(hideYubiKeyPopup()), Qt::QueuedConnection);

    setWindowIcon(icons()->applicationIcon());
    m_ui->globalMessageWidget->hideMessage();
    connect(m_ui->globalMessageWidget, &MessageWidget::linkActivated, &MessageWidget::openHttpUrl);

    m_clearHistoryAction = new QAction(tr("Clear history"), m_ui->menuFile);
    m_lastDatabasesActions = new QActionGroup(m_ui->menuRecentDatabases);
    connect(m_clearHistoryAction, SIGNAL(triggered()), this, SLOT(clearLastDatabases()));
    connect(m_lastDatabasesActions, SIGNAL(triggered(QAction*)), this, SLOT(openRecentDatabase(QAction*)));
    connect(m_ui->menuRecentDatabases, SIGNAL(aboutToShow()), this, SLOT(updateLastDatabasesMenu()));

    m_copyAdditionalAttributeActions = new QActionGroup(m_ui->menuEntryCopyAttribute);
    m_actionMultiplexer.connect(
        m_copyAdditionalAttributeActions, SIGNAL(triggered(QAction*)), SLOT(copyAttribute(QAction*)));
    connect(m_ui->menuEntryCopyAttribute, SIGNAL(aboutToShow()), this, SLOT(updateCopyAttributesMenu()));

    m_setTagsMenuActions = new QActionGroup(m_ui->menuTags);
    m_setTagsMenuActions->setExclusive(false);
    m_actionMultiplexer.connect(m_setTagsMenuActions, SIGNAL(triggered(QAction*)), SLOT(setTag(QAction*)));
    connect(m_ui->menuTags, &QMenu::aboutToShow, this, &MainWindow::updateSetTagsMenu);

    Qt::Key globalAutoTypeKey = static_cast<Qt::Key>(config()->get(Config::GlobalAutoTypeKey).toInt());
    Qt::KeyboardModifiers globalAutoTypeModifiers =
        static_cast<Qt::KeyboardModifiers>(config()->get(Config::GlobalAutoTypeModifiers).toInt());
    if (globalAutoTypeKey > 0 && globalAutoTypeModifiers > 0) {
        autoType()->registerGlobalShortcut(globalAutoTypeKey, globalAutoTypeModifiers);
    }

    // The shell draws its own dividers, so the tool bar separator stays down.
    m_ui->toolbarSeparator->setVisible(false);
    m_showToolbarSeparator = false;

    m_ui->actionEntryAutoType->setVisible(autoType()->isAvailable());
    m_ui->actionAllowScreenCapture->setVisible(osUtils->canPreventScreenCapture());

    m_inactivityTimer = new InactivityTimer(this);
    connect(m_inactivityTimer, SIGNAL(inactivityDetected()), this, SLOT(lockAllDatabases()));
    applySettingsChanges();

    connect(m_ui->menuEntries, SIGNAL(aboutToShow()), SLOT(obtainContextFocusLock()));
    connect(m_ui->menuEntries, SIGNAL(aboutToHide()), SLOT(releaseContextFocusLock()));
    connect(m_entryContextMenu, SIGNAL(aboutToShow()), SLOT(obtainContextFocusLock()));
    connect(m_entryContextMenu, SIGNAL(aboutToHide()), SLOT(releaseContextFocusLock()));
    connect(m_entryNewContextMenu, SIGNAL(aboutToShow()), SLOT(obtainContextFocusLock()));
    connect(m_entryNewContextMenu, SIGNAL(aboutToHide()), SLOT(releaseContextFocusLock()));
    connect(m_ui->menuGroups, SIGNAL(aboutToShow()), SLOT(obtainContextFocusLock()));
    connect(m_ui->menuGroups, SIGNAL(aboutToHide()), SLOT(releaseContextFocusLock()));

    // Control window state
    new QShortcut(Qt::CTRL | Qt::Key_M, this, SLOT(minimizeOrHide()));
    new QShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_M, this, SLOT(hideWindow()));
    // Control database tabs
    // Ctrl+Tab is broken on Mac, so use Alt (i.e. the Option key) - https://bugreports.qt.io/browse/QTBUG-8596
    auto dbTabModifier2 = Qt::CTRL;
#ifdef Q_OS_MACOS
    dbTabModifier2 = Qt::ALT;
#endif
    new QShortcut(dbTabModifier2 | Qt::Key_Tab, this, SLOT(selectNextDatabaseTab()));
    new QShortcut(Qt::CTRL | Qt::Key_PageDown, this, SLOT(selectNextDatabaseTab()));
    new QShortcut(dbTabModifier2 | Qt::SHIFT | Qt::Key_Tab, this, SLOT(selectPreviousDatabaseTab()));
    new QShortcut(Qt::CTRL | Qt::Key_PageUp, this, SLOT(selectPreviousDatabaseTab()));

    // Tab selection by number: Windows uses Ctrl, macOS uses Command
    auto dbTabModifier = Qt::CTRL;
    auto shortcut = new QShortcut(dbTabModifier | Qt::Key_1, this);
    connect(shortcut, &QShortcut::activated, [this]() { selectDatabaseTab(0); });
    shortcut = new QShortcut(dbTabModifier | Qt::Key_2, this);
    connect(shortcut, &QShortcut::activated, [this]() { selectDatabaseTab(1); });
    shortcut = new QShortcut(dbTabModifier | Qt::Key_3, this);
    connect(shortcut, &QShortcut::activated, [this]() { selectDatabaseTab(2); });
    shortcut = new QShortcut(dbTabModifier | Qt::Key_4, this);
    connect(shortcut, &QShortcut::activated, [this]() { selectDatabaseTab(3); });
    shortcut = new QShortcut(dbTabModifier | Qt::Key_5, this);
    connect(shortcut, &QShortcut::activated, [this]() { selectDatabaseTab(4); });
    shortcut = new QShortcut(dbTabModifier | Qt::Key_6, this);
    connect(shortcut, &QShortcut::activated, [this]() { selectDatabaseTab(5); });
    shortcut = new QShortcut(dbTabModifier | Qt::Key_7, this);
    connect(shortcut, &QShortcut::activated, [this]() { selectDatabaseTab(6); });
    shortcut = new QShortcut(dbTabModifier | Qt::Key_8, this);
    connect(shortcut, &QShortcut::activated, [this]() { selectDatabaseTab(7); });
    shortcut = new QShortcut(dbTabModifier | Qt::Key_9, this);
    connect(shortcut, &QShortcut::activated, [this]() { selectDatabaseTab(m_ui->tabWidget->count() - 1); });

    m_ui->actionDatabaseNew->setIcon(icons()->icon("document-new"));
    m_ui->actionDatabaseOpen->setIcon(icons()->icon("document-open"));
    m_ui->menuRecentDatabases->setIcon(icons()->icon("document-open-recent"));
    m_ui->actionDatabaseSave->setIcon(icons()->icon("document-save"));
    m_ui->actionDatabaseSaveAs->setIcon(icons()->icon("document-save-as"));
    m_ui->actionDatabaseSaveBackup->setIcon(icons()->icon("document-save-copy"));
    m_ui->actionDatabaseClose->setIcon(icons()->icon("document-close"));
    m_ui->actionReports->setIcon(icons()->icon("reports"));
    m_ui->actionDatabaseSettings->setIcon(icons()->icon("database-settings"));
    m_ui->actionDatabaseSecurity->setIcon(icons()->icon("database-change-key"));
    m_ui->actionPasskeys->setIcon(icons()->icon("passkey"));
    m_ui->actionImportPasskey->setIcon(icons()->icon("document-import"));
    m_ui->actionLockDatabase->setIcon(icons()->icon("database-lock"));
    m_ui->actionLockDatabaseToolbar->setIcon(icons()->icon("database-lock"));
    m_ui->actionLockAllDatabases->setIcon(icons()->icon("database-lock-all"));
    m_ui->actionQuit->setIcon(icons()->icon("application-exit"));
    m_ui->actionDatabaseMerge->setIcon(icons()->icon("database-merge"));
    m_ui->menuRemoteSync->setIcon(icons()->icon("remote-sync"));
    m_ui->actionImport->setIcon(icons()->icon("document-import"));
    m_ui->menuExport->setIcon(icons()->icon("document-export"));

#ifndef KPXC_FEATURE_BROWSER
    m_ui->actionPasskeys->setVisible(false);
    m_ui->actionImportPasskey->setVisible(false);
    m_ui->actionEntryImportPasskey->setVisible(false);
#endif

    m_ui->actionEntryNew->setIcon(icons()->icon("entry-new"));
    m_ui->actionEntryClone->setIcon(icons()->icon("entry-clone"));
    m_ui->actionEntryEdit->setIcon(icons()->icon("entry-edit"));
    m_ui->actionEntryExpire->setIcon(icons()->icon("entry-expire"));
    m_ui->actionEntryDelete->setIcon(icons()->icon("entry-delete"));
    m_ui->actionEntryRestore->setIcon(icons()->icon("entry-restore"));
    m_ui->actionEntryAutoType->setIcon(icons()->icon("auto-type"));
    m_ui->actionEntryAutoTypeSequence->setIcon(icons()->icon("auto-type"));
    m_ui->actionEntryAutoTypeUsername->setIcon(icons()->icon("auto-type"));
    m_ui->actionEntryAutoTypeUsernameEnter->setIcon(icons()->icon("auto-type"));
    m_ui->actionEntryAutoTypePassword->setIcon(icons()->icon("auto-type"));
    m_ui->actionEntryAutoTypePasswordEnter->setIcon(icons()->icon("auto-type"));
    m_ui->actionEntryAutoTypeTOTP->setIcon(icons()->icon("auto-type"));
    m_ui->actionEntryAutoTypeURL->setIcon(icons()->icon("auto-type"));
    m_ui->actionEntryAutoTypeURLEnter->setIcon(icons()->icon("auto-type"));
    m_ui->actionEntryMoveUp->setIcon(icons()->icon("move-up"));
    m_ui->actionEntryMoveDown->setIcon(icons()->icon("move-down"));
    m_ui->actionEntryCopyUsername->setIcon(icons()->icon("username-copy"));
    m_ui->actionEntryCopyPassword->setIcon(icons()->icon("password-copy"));
    m_ui->actionEntryCopyURL->setIcon(icons()->icon("url-copy"));
    m_ui->menuEntryCopyAttribute->setIcon(icons()->icon("attributes-copy"));
    m_ui->menuEntryTotp->setIcon(icons()->icon("totp"));
    m_ui->actionEntryTotp->setIcon(icons()->icon("totp"));
    m_ui->actionEntryCopyTotp->setIcon(icons()->icon("totp-copy"));
    m_ui->actionEntryCopyPasswordTotp->setIcon(icons()->icon("totp-copy-password"));
    m_ui->actionEntryTotpQRCode->setIcon(icons()->icon("qrcode"));
    m_ui->actionEntrySetupTotp->setIcon(icons()->icon("totp-edit"));
    m_ui->actionEntryImportPasskey->setIcon(icons()->icon("document-import"));
    m_ui->actionEntryAddToAgent->setIcon(icons()->icon("utilities-terminal"));
    m_ui->actionEntryRemoveFromAgent->setIcon(icons()->icon("utilities-terminal"));
    m_ui->menuTags->setIcon(icons()->icon("tag-multiple"));
    m_ui->actionEntryDownloadIcon->setIcon(icons()->icon("favicon-download"));
    m_ui->actionGroupSortAsc->setIcon(icons()->icon("sort-alphabetical-ascending"));
    m_ui->actionGroupSortDesc->setIcon(icons()->icon("sort-alphabetical-descending"));

    m_ui->actionGroupNew->setIcon(icons()->icon("group-new"));
    m_ui->actionGroupEdit->setIcon(icons()->icon("group-edit"));
    m_ui->actionGroupClone->setIcon(icons()->icon("group-clone"));
    m_ui->actionGroupDelete->setIcon(icons()->icon("group-delete"));
    m_ui->actionGroupEmptyRecycleBin->setIcon(icons()->icon("group-empty-trash"));
    m_ui->actionEntryOpenUrl->setIcon(icons()->icon("web"));
    m_ui->actionGroupDownloadFavicons->setIcon(icons()->icon("favicon-download"));

    m_ui->actionSettings->setIcon(icons()->icon("configure"));
    m_ui->actionPasswordGenerator->setIcon(icons()->icon("password-generator"));
    m_ui->actionClearSSHAgent->setIcon(icons()->icon("utilities-terminal"));

    m_ui->actionAbout->setIcon(icons()->icon("help-about"));
    m_ui->actionDonate->setIcon(icons()->icon("donate"));
    m_ui->actionBugReport->setIcon(icons()->icon("bugreport"));
    m_ui->actionGettingStarted->setIcon(icons()->icon("getting-started"));
    m_ui->actionUserGuide->setIcon(icons()->icon("user-guide"));
    m_ui->actionOnlineHelp->setIcon(icons()->icon("system-help"));
    m_ui->actionKeyboardShortcuts->setIcon(icons()->icon("keyboard-shortcuts"));
    m_ui->actionCheckForUpdates->setIcon(icons()->icon("system-software-update"));

#ifdef KPXC_FEATURE_BROWSER
    m_ui->actionPasskeys->setIcon(icons()->icon("passkey"));
    m_ui->actionImportPasskey->setIcon(icons()->icon("document-import"));
    m_ui->actionEntryImportPasskey->setIcon(icons()->icon("document-import"));
    m_ui->actionEntryRemovePasskey->setIcon(icons()->icon("document-close"));
#endif

    m_actionMultiplexer.connect(SIGNAL(currentModeChanged(DatabaseWidget::Mode)), this, SLOT(updateMenuActionState()));
    m_actionMultiplexer.connect(SIGNAL(groupChanged()), this, SLOT(updateMenuActionState()));
    m_actionMultiplexer.connect(SIGNAL(entrySelectionChanged()), this, SLOT(updateMenuActionState()));
    m_actionMultiplexer.connect(SIGNAL(databaseNonDataChanged()), this, SLOT(updateMenuActionState()));
    m_actionMultiplexer.connect(SIGNAL(groupContextMenuRequested(QPoint)), this, SLOT(showGroupContextMenu(QPoint)));
    m_actionMultiplexer.connect(SIGNAL(entryContextMenuRequested(QPoint)), this, SLOT(showEntryContextMenu(QPoint)));
    m_actionMultiplexer.connect(SIGNAL(groupChanged()), this, SLOT(updateEntryCountLabel()));
    m_actionMultiplexer.connect(SIGNAL(databaseUnlocked()), this, SLOT(updateEntryCountLabel()));
    m_actionMultiplexer.connect(SIGNAL(databaseModified()), this, SLOT(updateEntryCountLabel()));
    m_actionMultiplexer.connect(SIGNAL(searchModeActivated()), this, SLOT(updateEntryCountLabel()));
    m_actionMultiplexer.connect(SIGNAL(listModeActivated()), this, SLOT(updateEntryCountLabel()));

    // Notify search when the active database changes or gets locked
    connect(m_ui->tabWidget,
            SIGNAL(activeDatabaseChanged(DatabaseWidget*)),
            m_searchWidget,
            SLOT(databaseChanged(DatabaseWidget*)));
    connect(m_ui->tabWidget, SIGNAL(databaseLocked(DatabaseWidget*)), m_searchWidget, SLOT(databaseChanged()));

    connect(m_ui->tabWidget, SIGNAL(tabNameChanged()), SLOT(updateWindowTitle()));
    connect(m_ui->tabWidget, SIGNAL(currentChanged(int)), SLOT(updateWindowTitle()));
    connect(m_ui->tabWidget, SIGNAL(currentChanged(int)), SLOT(databaseTabChanged(int)));
    connect(m_ui->tabWidget, SIGNAL(currentChanged(int)), SLOT(updateMenuActionState()));
    connect(m_ui->tabWidget, SIGNAL(databaseLocked(DatabaseWidget*)), SLOT(databaseStatusChanged(DatabaseWidget*)));
    connect(m_ui->tabWidget, SIGNAL(databaseUnlocked(DatabaseWidget*)), SLOT(databaseStatusChanged(DatabaseWidget*)));
    connect(m_ui->tabWidget, SIGNAL(tabVisibilityChanged(bool)), SLOT(updateToolbarSeparatorVisibility()));
    connect(m_ui->stackedWidget, SIGNAL(currentChanged(int)), SLOT(updateMenuActionState()));
    connect(m_ui->stackedWidget, SIGNAL(currentChanged(int)), SLOT(updateWindowTitle()));
    connect(m_ui->stackedWidget, SIGNAL(currentChanged(int)), SLOT(updateToolbarSeparatorVisibility()));
    connect(m_ui->settingsWidget, SIGNAL(accepted()), SLOT(applySettingsChanges()));
    connect(m_ui->settingsWidget, SIGNAL(settingsReset()), SLOT(applySettingsChanges()));
    connect(m_ui->settingsWidget, SIGNAL(accepted()), SLOT(switchToDatabases()));
    connect(m_ui->settingsWidget, SIGNAL(rejected()), SLOT(switchToDatabases()));

    connect(m_ui->actionDatabaseNew, SIGNAL(triggered()), m_ui->tabWidget, SLOT(newDatabase()));
    connect(m_ui->actionDatabaseOpen, SIGNAL(triggered()), m_ui->tabWidget, SLOT(openDatabase()));
    connect(m_ui->actionDatabaseSave, SIGNAL(triggered()), m_ui->tabWidget, SLOT(saveDatabase()));
    connect(m_ui->actionDatabaseSaveAs, SIGNAL(triggered()), m_ui->tabWidget, SLOT(saveDatabaseAs()));
    connect(m_ui->actionDatabaseSaveBackup, SIGNAL(triggered()), m_ui->tabWidget, SLOT(saveDatabaseBackup()));
    connect(m_ui->actionDatabaseClose, SIGNAL(triggered()), m_ui->tabWidget, SLOT(closeCurrentDatabaseTab()));
    connect(m_ui->actionDatabaseMerge, SIGNAL(triggered()), m_ui->tabWidget, SLOT(mergeDatabase()));
    connect(m_ui->actionDatabaseSettings, SIGNAL(toggled(bool)), m_ui->tabWidget, SLOT(showDatabaseSettings(bool)));
    connect(m_ui->actionDatabaseSecurity, SIGNAL(triggered()), m_ui->tabWidget, SLOT(showDatabaseSecurity()));
    connect(m_ui->actionReports, SIGNAL(toggled(bool)), m_ui->tabWidget, SLOT(showDatabaseReports(bool)));
#ifdef KPXC_FEATURE_BROWSER
    connect(m_ui->actionPasskeys, SIGNAL(triggered()), m_ui->tabWidget, SLOT(showPasskeys()));
    connect(m_ui->actionImportPasskey, SIGNAL(triggered()), m_ui->tabWidget, SLOT(importPasskey()));
    connect(m_ui->actionEntryImportPasskey, SIGNAL(triggered()), m_ui->tabWidget, SLOT(importPasskeyToEntry()));
    connect(m_ui->actionEntryRemovePasskey, SIGNAL(triggered()), m_ui->tabWidget, SLOT(removePasskeyFromEntry()));
#endif
    connect(m_ui->actionImport, SIGNAL(triggered()), m_ui->tabWidget, SLOT(importFile()));
    connect(m_ui->actionExportCsv, SIGNAL(triggered()), m_ui->tabWidget, SLOT(exportToCsv()));
    connect(m_ui->actionExportHtml, SIGNAL(triggered()), m_ui->tabWidget, SLOT(exportToHtml()));
    connect(m_ui->actionExportXML, SIGNAL(triggered()), m_ui->tabWidget, SLOT(exportToXML()));
    connect(
        m_ui->actionLockDatabase, SIGNAL(triggered()), m_ui->tabWidget, SLOT(lockAndSwitchToFirstUnlockedDatabase()));
    connect(m_ui->actionLockDatabaseToolbar, SIGNAL(triggered()), m_ui->actionLockDatabase, SIGNAL(triggered()));
    connect(m_ui->actionLockAllDatabases, SIGNAL(triggered()), m_ui->tabWidget, SLOT(lockDatabases()));
    connect(m_ui->actionQuit, SIGNAL(triggered()), SLOT(appExit()));

    m_actionMultiplexer.connect(m_ui->actionEntryNew, SIGNAL(triggered()), SLOT(createEntry()));
    m_actionMultiplexer.connect(m_ui->actionEntryEdit, SIGNAL(triggered()), SLOT(switchToEntryEdit()));
    m_actionMultiplexer.connect(m_ui->actionEntryExpire, SIGNAL(triggered()), SLOT(expireSelectedEntries()));
    m_actionMultiplexer.connect(m_ui->actionEntryClone, SIGNAL(triggered()), SLOT(cloneEntry()));
    m_actionMultiplexer.connect(m_ui->actionEntryDelete, SIGNAL(triggered()), SLOT(deleteSelectedEntries()));
    m_actionMultiplexer.connect(m_ui->actionEntryRestore, SIGNAL(triggered()), SLOT(restoreSelectedEntries()));

    m_actionMultiplexer.connect(m_ui->actionEntryTotp, SIGNAL(triggered()), SLOT(showTotp()));
    m_actionMultiplexer.connect(m_ui->actionEntrySetupTotp, SIGNAL(triggered()), SLOT(setupTotp()));

    m_actionMultiplexer.connect(m_ui->actionEntryCopyTotp, SIGNAL(triggered()), SLOT(copyTotp()));
    m_actionMultiplexer.connect(m_ui->actionEntryCopyPasswordTotp, SIGNAL(triggered()), SLOT(copyPasswordTotp()));
    m_actionMultiplexer.connect(m_ui->actionEntryTotpQRCode, SIGNAL(triggered()), SLOT(showTotpKeyQrCode()));
    m_actionMultiplexer.connect(m_ui->actionEntryCopyTitle, SIGNAL(triggered()), SLOT(copyTitle()));
    m_actionMultiplexer.connect(m_ui->actionEntryMoveUp, SIGNAL(triggered()), SLOT(moveEntryUp()));
    m_actionMultiplexer.connect(m_ui->actionEntryMoveDown, SIGNAL(triggered()), SLOT(moveEntryDown()));
    m_actionMultiplexer.connect(m_ui->actionEntryCopyUsername, SIGNAL(triggered()), SLOT(copyUsername()));
    m_actionMultiplexer.connect(m_ui->actionEntryCopyPassword, SIGNAL(triggered()), SLOT(copyPassword()));
    m_actionMultiplexer.connect(m_ui->actionEntryCopyURL, SIGNAL(triggered()), SLOT(copyURL()));
    m_actionMultiplexer.connect(m_ui->actionEntryCopyNotes, SIGNAL(triggered()), SLOT(copyNotes()));
    m_actionMultiplexer.connect(m_ui->actionEntryAutoType, SIGNAL(triggered()), SLOT(performAutoType()));
    m_actionMultiplexer.connect(m_ui->actionEntryAutoTypeSequence, SIGNAL(triggered()), SLOT(performAutoType()));
    m_actionMultiplexer.connect(
        m_ui->actionEntryAutoTypeUsername, SIGNAL(triggered()), SLOT(performAutoTypeUsername()));
    m_actionMultiplexer.connect(
        m_ui->actionEntryAutoTypeUsernameEnter, SIGNAL(triggered()), SLOT(performAutoTypeUsernameEnter()));
    m_actionMultiplexer.connect(
        m_ui->actionEntryAutoTypePassword, SIGNAL(triggered()), SLOT(performAutoTypePassword()));
    m_actionMultiplexer.connect(
        m_ui->actionEntryAutoTypePasswordEnter, SIGNAL(triggered()), SLOT(performAutoTypePasswordEnter()));
    m_actionMultiplexer.connect(m_ui->actionEntryAutoTypeTOTP, SIGNAL(triggered()), SLOT(performAutoTypeTOTP()));
    m_actionMultiplexer.connect(m_ui->actionEntryAutoTypeURL, SIGNAL(triggered()), SLOT(performAutoTypeURL()));
    m_actionMultiplexer.connect(
        m_ui->actionEntryAutoTypeURLEnter, SIGNAL(triggered()), SLOT(performAutoTypeURLEnter()));
    m_actionMultiplexer.connect(m_ui->actionEntryOpenUrl, SIGNAL(triggered()), SLOT(openUrl()));
    m_actionMultiplexer.connect(m_ui->actionEntryDownloadIcon, SIGNAL(triggered()), SLOT(downloadSelectedFavicons()));
#ifdef KPXC_FEATURE_SSHAGENT
    m_actionMultiplexer.connect(m_ui->actionEntryAddToAgent, SIGNAL(triggered()), SLOT(addToAgent()));
    m_actionMultiplexer.connect(m_ui->actionEntryRemoveFromAgent, SIGNAL(triggered()), SLOT(removeFromAgent()));
#endif

    m_actionMultiplexer.connect(m_ui->actionGroupNew, SIGNAL(triggered()), SLOT(createGroup()));
    m_actionMultiplexer.connect(m_ui->actionGroupEdit, SIGNAL(triggered()), SLOT(switchToGroupEdit()));
    m_actionMultiplexer.connect(m_ui->actionGroupClone, SIGNAL(triggered()), SLOT(cloneGroup()));
    m_actionMultiplexer.connect(m_ui->actionGroupDelete, SIGNAL(triggered()), SLOT(deleteGroup()));
    m_actionMultiplexer.connect(m_ui->actionGroupEmptyRecycleBin, SIGNAL(triggered()), SLOT(emptyRecycleBin()));
    m_actionMultiplexer.connect(m_ui->actionGroupSortAsc, SIGNAL(triggered()), SLOT(sortGroupsAsc()));
    m_actionMultiplexer.connect(m_ui->actionGroupSortDesc, SIGNAL(triggered()), SLOT(sortGroupsDesc()));
    m_actionMultiplexer.connect(m_ui->actionGroupDownloadFavicons, SIGNAL(triggered()), SLOT(downloadAllFavicons()));

    connect(m_ui->actionSettings, SIGNAL(toggled(bool)), SLOT(switchToSettings(bool)));
    connect(m_ui->actionPasswordGenerator, SIGNAL(toggled(bool)), SLOT(togglePasswordGenerator(bool)));
    connect(m_ui->passwordGeneratorWidget, &PasswordGeneratorWidget::closed, this, [this] {
        togglePasswordGenerator(false);
    });
    m_ui->passwordGeneratorWidget->setStandaloneMode(true);

    connect(m_ui->welcomeWidget, SIGNAL(newDatabase()), SLOT(switchToNewDatabase()));
    connect(m_ui->welcomeWidget, SIGNAL(openDatabase()), SLOT(switchToOpenDatabase()));
    connect(m_ui->welcomeWidget, SIGNAL(openDatabaseFile(QString)), SLOT(switchToDatabaseFile(QString)));
    connect(m_ui->welcomeWidget, SIGNAL(importFile()), m_ui->tabWidget, SLOT(importFile()));

    connect(m_ui->actionAbout, SIGNAL(triggered()), SLOT(showAboutDialog()));
    connect(m_ui->actionDonate, SIGNAL(triggered()), SLOT(openDonateUrl()));
    connect(m_ui->actionBugReport, SIGNAL(triggered()), SLOT(openBugReportUrl()));
    connect(m_ui->actionGettingStarted, SIGNAL(triggered()), SLOT(openGettingStartedGuide()));
    connect(m_ui->actionUserGuide, SIGNAL(triggered()), SLOT(openUserGuide()));
    connect(m_ui->actionOnlineHelp, SIGNAL(triggered()), SLOT(openOnlineHelp()));
    connect(m_ui->actionKeyboardShortcuts, SIGNAL(triggered()), SLOT(openKeyboardShortcuts()));
    connect(m_ui->actionAllowScreenCapture, &QAction::toggled, this, &MainWindow::setAllowScreenCapture);

    connect(osUtils, &OSUtilsBase::statusbarThemeChanged, this, &MainWindow::updateTrayIcon);

    // Install event filter for empty-area drag and menubar toggle
    auto* eventFilter = new MainWindowEventFilter(this);
    m_ui->menubar->installEventFilter(eventFilter);
    m_ui->toolBar->installEventFilter(eventFilter);
    m_ui->tabWidget->tabBar()->installEventFilter(eventFilter);
    installEventFilter(eventFilter);

#ifdef Q_OS_MACOS
    setUnifiedTitleAndToolBarOnMac(true);
#endif

#ifdef KPXC_FEATURE_UPDATES
    connect(m_ui->actionCheckForUpdates, SIGNAL(triggered()), SLOT(showUpdateCheckDialog()));
    connect(updateCheck(), &UpdateChecker::stateChanged, this, [this](UpdateChecker::State state, UpdateChecker::Failure) {
        switch (state) {
        case UpdateChecker::State::Checking:
            Material::Notify::progress(QStringLiteral("squirrel-update"), tr("Checking for updates…"), -1);
            break;
        case UpdateChecker::State::Available:
            m_updateFailureNotified = false;
            updateCheck()->downloadAvailableUpdate();
            break;
        case UpdateChecker::State::NoUpdate:
            m_updateFailureNotified = false;
            Material::Notify::endProgress(QStringLiteral("squirrel-update"));
            break;
        case UpdateChecker::State::Failed:
            Material::Notify::endProgress(QStringLiteral("squirrel-update"));
            if (!m_updateFailureNotified) {
                m_updateFailureNotified = true;
                Material::Notify::error(
                    tr("Update failed"),
                    tr("The update could not be completed. Open the notification history for the recorded state."));
            }
            break;
        default:
            break;
        }
    });
    connect(updateCheck(), &UpdateChecker::downloadProgress, this, [](quint64 received, quint64 total) {
        const int percent = total == 0 ? 0 : int(qMin<quint64>(100, received * 100 / total));
        Material::Notify::progress(QStringLiteral("squirrel-update"),
                                   tr("Downloading update: %1 of %2 MiB")
                                       .arg(received / (1024 * 1024))
                                       .arg(total / (1024 * 1024)),
                                   percent);
    });
    connect(updateCheck(), &UpdateChecker::updatePackageReady, this, [](const QString& path) {
        Material::Notify::progress(QStringLiteral("squirrel-update"), tr("Verifying and staging the update…"), 100);
        updateCheck()->applyVerifiedUpdate(path);
    });
    connect(updateCheck(), &UpdateChecker::updateReadyToRestart, this, [this](const QString& version) {
        Q_UNUSED(version)
        Material::Notify::endProgress(QStringLiteral("squirrel-update"));
        showUpdateReadyNotification();
    });
    // Setup an update check every hour (checked only occur every 7 days)
    connect(&m_updateCheckTimer, &QTimer::timeout, this, &MainWindow::performUpdateCheck);
    m_updateCheckTimer.start(3.6e6);
    // Perform the startup update check after 500 ms
    QTimer::singleShot(500, this, SLOT(performUpdateCheck()));
#else
    m_ui->actionCheckForUpdates->setVisible(false);
#endif

#ifndef KPXC_FEATURE_NETWORK
    m_ui->actionGroupDownloadFavicons->setVisible(false);
    m_ui->actionEntryDownloadIcon->setVisible(false);
#endif
#ifndef KPXC_FEATURE_DOCS
    m_ui->actionGettingStarted->setVisible(false);
    m_ui->actionUserGuide->setVisible(false);
    m_ui->actionKeyboardShortcuts->setVisible(false);
#endif

    // clang-format off
    connect(m_ui->tabWidget, SIGNAL(messageGlobal(QString,MessageWidget::MessageType)),
        SLOT(displayGlobalMessage(QString,MessageWidget::MessageType)));
    // clang-format on

    connect(m_ui->tabWidget, SIGNAL(messageDismissGlobal()), this, SLOT(hideGlobalMessage()));

#ifndef Q_OS_HAIKU
    m_screenLockListener = new ScreenLockListener(this);
    connect(m_screenLockListener, SIGNAL(screenLocked()), SLOT(handleScreenLock()));
#endif

    // Tray Icon setup
    connect(Application::instance(), SIGNAL(focusWindowChanged(QWindow*)), SLOT(focusWindowChanged(QWindow*)));
    m_trayIconTriggerReason = QSystemTrayIcon::Unknown;
    m_trayIconTriggerTimer.setSingleShot(true);
    connect(&m_trayIconTriggerTimer, SIGNAL(timeout()), SLOT(processTrayIconTrigger()));

    if (config()->hasAccessError()) {
        m_ui->globalMessageWidget->showMessage(tr("Access error for config file %1").arg(config()->getFileName()),
                                               MessageWidget::Error);
    }

    // Properly shutdown on logoff, restart, and shutdown
    connect(qApp, &QGuiApplication::commitDataRequest, this, [this] { m_appExitCalled = true; });

    connect(qApp, SIGNAL(anotherInstanceStarted()), this, SLOT(bringToFront()));
    connect(qApp, SIGNAL(applicationActivated()), this, SLOT(bringToFront()));
    connect(qApp, SIGNAL(openFile(QString)), this, SLOT(openDatabase(QString)));
    connect(qApp, SIGNAL(quitSignalReceived()), this, SLOT(appExit()), Qt::DirectConnection);

    // Setup the status bar
    statusBar()->setFixedHeight(24);
    m_progressBarLabel = new QLabel(statusBar());
    m_progressBarLabel->setVisible(false);
    statusBar()->addPermanentWidget(m_progressBarLabel);
    m_progressBar = new QProgressBar(statusBar());
    m_progressBar->setVisible(false);
    m_progressBar->setTextVisible(false);
    m_progressBar->setMaximumWidth(100);
    m_progressBar->setFixedHeight(15);
    m_progressBar->setMaximum(100);
    statusBar()->addPermanentWidget(m_progressBar);
    connect(clipboard(), &Clipboard::updateCountdown, this, &MainWindow::updateProgressBar);
    m_actionMultiplexer.connect(SIGNAL(updateSyncProgress(int, QString)), this, SLOT(updateProgressBar(int, QString)));
    m_actionMultiplexer.connect(SIGNAL(databaseSyncInProgress()), this, SLOT(disableMenuAndToolbar()));
    m_actionMultiplexer.connect(SIGNAL(databaseSyncCompleted(QString)), this, SLOT(enableMenuAndToolbar()));
    m_actionMultiplexer.connect(SIGNAL(databaseSyncFailed(QString, const QString)), this, SLOT(enableMenuAndToolbar()));
    m_statusBarLabel = new QLabel(statusBar());
    m_statusBarLabel->setObjectName("statusBarLabel");
    statusBar()->addPermanentWidget(m_statusBarLabel);
    // The stock status bar is legacy chrome under a Material shell. Its entry
    // count already lives on the rail's Vault sublabel and its progress bar is
    // reported through the notification host, so the bar itself stays hidden.
    statusBar()->hide();

    // ------------------------------------------------------------------
    // The Material shell takes over the window interior.
    //
    // The stock menu bar and tool bar are hidden rather than destroyed: every
    // QAction wired above keeps its shortcut, its enable/disable rule and its
    // place in the entry and group context menus. The command palette below
    // lists all of them by name so nothing became unreachable.
    // ------------------------------------------------------------------
    auto* materialShell = new Material::Shell;

    // The caption asks; the window answers. The subtitle follows the window
    // title so the bar reads "KeePassXC  Personal.kdbx" like the reference.
    if (auto* titleBar = materialShell->titleBar()) {
        connect(titleBar, &Material::TitleBar::minimizeRequested, this, &MainWindow::showMinimized);
        connect(titleBar, &Material::TitleBar::maximizeRequested, this, [this] {
            if (isMaximized()) {
                showNormal();
            } else {
                showMaximized();
            }
        });
        connect(titleBar, &Material::TitleBar::closeRequested, this, &MainWindow::close);
        connect(this, &QWidget::windowTitleChanged, titleBar, [titleBar](const QString& title) {
            const QString app = QApplication::applicationDisplayName();
            QString subtitle = title;
            const QStringList separators{QStringLiteral(" - "), QString::fromUtf8(" \xe2\x80\x93 "), QString::fromUtf8(" \xe2\x80\x94 ")};
            for (const QString& separator : separators) {
                const int at = subtitle.lastIndexOf(separator);
                if (at >= 0 && subtitle.mid(at + separator.size()).trimmed() == app) {
                    subtitle = subtitle.left(at);
                    break;
                }
            }
            titleBar->setSubtitle(subtitle.trimmed() == app ? QString() : subtitle.trimmed());
        });
    }

    // The pages are moved out of their .ui layouts into the destinations. The
    // stacked widget stays whole - its indices still drive updateMenuActionState()
    // and updateWindowTitle() - and travels into the vault destination with the
    // welcome screen, the database tabs and the password generator inside it.
    m_ui->verticalLayout->removeWidget(m_ui->stackedWidget);
    m_ui->verticalLayout_3->removeWidget(m_ui->settingsWidget);

    auto* reportsScreen = new Material::ReportsScreen;
    auto* historyScreen = new Material::HistoryScreen;
    auto* changelogScreen = new Material::ChangelogScreen;

    // The vault destination is the three panes of the design in front of the
    // stock stack. The stack itself keeps the whole database lifecycle - the
    // welcome screen, the unlock dialog, the entry and group editors, the
    // reports and settings pages, the generator - and the panes take over
    // whenever an unlocked database is being browsed.
    auto* vaultScreen = new Material::VaultScreen;
    vaultScreen->setHostWidget(m_ui->stackedWidget, m_ui->tabWidget);
    vaultScreen->setBreakpoint(materialShell->breakpoint());
    connect(materialShell, &Material::Shell::breakpointChanged, vaultScreen, &Material::VaultScreen::setBreakpoint);

    // The settings destination is the Material hub: the spec sheets whose rows
    // are bound to the real Config keys, plus the stock
    // ApplicationSettingsWidget adopted as the classic editor so no option
    // becomes unreachable. The Appearance overview is hoisted out of the hub
    // because the design's rail gives it a destination of its own.
    auto* settingsHub = new Material::SettingsHub(Material::SettingsHub::Overview::Hosted, nullptr);
    m_settingsHub = settingsHub;
    settingsHub->setClassicEditor(m_ui->settingsWidget);
    auto* appearanceScreen = new Material::SettingsScreen;

    // The four reference sheets. Database settings, the entry editor, the
    // tools and the help pages all live in dialogs and menus elsewhere; these
    // describe them in the design's own words so the rail is complete.
    auto* editorSheet = Material::SheetCatalogue::create(QStringLiteral("editor"));
    auto* databaseSheet = Material::SheetCatalogue::create(QStringLiteral("database"));
    auto* toolsSheet = Material::SheetCatalogue::create(QStringLiteral("tools"));
    auto* helpSheet = Material::SheetCatalogue::create(QStringLiteral("help"));

    // The rail's ten destinations, in the design's order. Sublabels that count
    // something in the open database are refreshed by updateRailSublabels();
    // a sheet just reports how many pages it ended up with.
    const auto pages = [](Material::SpecSheet* sheet) {
        return sheet ? tr("%n page(s)", "", sheet->pageCount()) : QString();
    };

    materialShell->addDestination(
        QStringLiteral("vault"), vaultScreen, QStringLiteral("key_vertical"), tr("Vault"), QString());
    materialShell->addDestination(
        QStringLiteral("reports"), reportsScreen, QStringLiteral("health_metrics"), tr("Reports"), QString());
    materialShell->addDestination(
        QStringLiteral("editor"), editorSheet, QStringLiteral("edit_note"), tr("Entry"), pages(editorSheet));
    materialShell->addDestination(
        QStringLiteral("database"), databaseSheet, QStringLiteral("database"), tr("Database"), pages(databaseSheet));
    materialShell->addDestination(
        QStringLiteral("tools"), toolsSheet, QStringLiteral("construction"), tr("Tools"), pages(toolsSheet));
    materialShell->addDestination(
        QStringLiteral("history"), historyScreen, QStringLiteral("history"), tr("History"), QString());
    materialShell->addDestination(QStringLiteral("changelog"),
                                  changelogScreen,
                                  QStringLiteral("receipt_long"),
                                  tr("Changelog"),
                                  QString::fromLatin1(KEEPASSXC_VERSION));
    materialShell->addDestination(
        QStringLiteral("settings"), settingsHub, QStringLiteral("tune"), tr("Settings"), pages(settingsHub->specSheet()));
    materialShell->addDestination(
        QStringLiteral("appearance"), appearanceScreen, QStringLiteral("palette"), tr("Appearance"), QString());
    materialShell->addDestination(
        QStringLiteral("help"), helpSheet, QStringLiteral("help"), tr("Help"), pages(helpSheet));
    m_ui->verticalLayout->addWidget(materialShell, 1);

    // The three data-driven destinations. Each feed owns the reading, the
    // filtering and the Markdown export of its own screen; the window only
    // tells them which database is in front and acts on what they ask for.
    auto* reportsFeed = new Material::ReportsFeed(reportsScreen, this);
    auto* historyFeed = new Material::HistoryFeed(historyScreen, this);
    new Material::ChangelogFeed(changelogScreen, this);

    // The stock tab bar is replaced by the Material tab strip. toggleTabbar()
    // shows it again whenever a database opens, so it is put back down here.
    m_ui->tabWidget->tabBar()->hide();
    connect(
        m_ui->tabWidget, &DatabaseTabWidget::tabVisibilityChanged, this, [this] { m_ui->tabWidget->tabBar()->hide(); });

    auto syncTabStrip = [this] {
        auto* strip = shell() ? shell()->tabs() : nullptr;
        if (!strip) {
            return;
        }
        QList<Material::TabDescriptor> descriptors;
        const QStringList preferredOrder = config()->get(Config::GUI_TabOrder).toStringList();
        const QStringList pinnedList = config()->get(Config::GUI_PinnedTabs).toStringList();
        const QSet<QString> pinned(pinnedList.cbegin(), pinnedList.cend());
        QString currentId;
        for (int i = 0; i < m_ui->tabWidget->count(); ++i) {
            auto* dbWidget = m_ui->tabWidget->databaseWidgetFromIndex(i);
            if (!dbWidget) {
                continue;
            }
            const QString id = tabIdFor(dbWidget);
            const auto database = dbWidget->database();
            const QString persistenceKey = database ? Material::tabPersistenceKeyForPath(database->filePath()) : QString();
            descriptors.append({id,
                                persistenceKey,
                                dbWidget->isLocked() ? QStringLiteral("lock") : QStringLiteral("database"),
                                m_ui->tabWidget->tabName(i),
                                persistenceKey.isEmpty() ? m_sessionPinnedTabs.contains(id)
                                                         : pinned.contains(persistenceKey),
                                !persistenceKey.isEmpty()});
            if (i == m_ui->tabWidget->currentIndex()) {
                currentId = id;
            }
        }
        std::stable_sort(descriptors.begin(), descriptors.end(), [&preferredOrder](const auto& left, const auto& right) {
            if (left.pinned != right.pinned) return left.pinned;
            const int leftIndex = preferredOrder.indexOf(left.persistenceKey);
            const int rightIndex = preferredOrder.indexOf(right.persistenceKey);
            if (leftIndex < 0 && rightIndex < 0) return false;
            if (leftIndex < 0) return false;
            if (rightIndex < 0) return true;
            return leftIndex < rightIndex;
        });
        strip->setTabs(descriptors, currentId);
    };
    // The rail's counts follow the same events as the tab strip, plus the two
    // that produce the numbers themselves.
    connect(reportsFeed, &Material::ReportsFeed::findingCountChanged, this, [this](int count) {
        m_reportFindings = count;
        updateRailSublabels();
    });
    if (auto* store = Material::HistoryStore::instance()) {
        connect(store, &Material::HistoryStore::revisionsChanged, this, &MainWindow::updateRailSublabels);
    }
    connect(m_ui->tabWidget, &DatabaseTabWidget::currentChanged, this, &MainWindow::updateRailSublabels);
    connect(m_ui->tabWidget, &DatabaseTabWidget::databaseOpened, this, &MainWindow::updateRailSublabels);
    connect(m_ui->tabWidget, &DatabaseTabWidget::databaseClosed, this, &MainWindow::updateRailSublabels);
    connect(m_ui->tabWidget, &DatabaseTabWidget::databaseLocked, this, &MainWindow::updateRailSublabels);
    connect(m_ui->tabWidget, &DatabaseTabWidget::databaseUnlocked, this, &MainWindow::updateRailSublabels);

    connect(m_ui->tabWidget, &DatabaseTabWidget::currentChanged, this, syncTabStrip);
    connect(m_ui->tabWidget, &DatabaseTabWidget::tabNameChanged, this, syncTabStrip);
    connect(m_ui->tabWidget, &DatabaseTabWidget::databaseOpened, this, syncTabStrip);
    connect(m_ui->tabWidget, &DatabaseTabWidget::databaseClosed, this, syncTabStrip);
    connect(m_ui->tabWidget, &DatabaseTabWidget::databaseLocked, this, syncTabStrip);
    connect(m_ui->tabWidget, &DatabaseTabWidget::databaseUnlocked, this, syncTabStrip);
    connect(m_ui->tabWidget, &DatabaseTabWidget::tabVisibilityChanged, this, syncTabStrip);

    auto* tabStrip = materialShell->tabs();
    connect(tabStrip, &Material::TabStrip::tabSelected, this, [this](const QString& id) {
        for (int i = 0; i < m_ui->tabWidget->count(); ++i) {
            if (tabIdFor(m_ui->tabWidget->databaseWidgetFromIndex(i)) == id) {
                m_ui->tabWidget->setCurrentIndex(i);
                if (shell()) {
                    shell()->setCurrentDestination(QStringLiteral("vault"));
                }
                break;
            }
        }
    });
    connect(tabStrip, &Material::TabStrip::tabCloseRequested, this, [this](const QString& id) {
        for (int i = 0; i < m_ui->tabWidget->count(); ++i) {
            if (tabIdFor(m_ui->tabWidget->databaseWidgetFromIndex(i)) == id) {
                m_ui->tabWidget->closeDatabaseTab(i);
                break;
            }
        }
    });
    connect(tabStrip, &Material::TabStrip::tabPinRequested, this, [this, syncTabStrip](const QString& id, bool pinned) {
        for (int i = 0; i < m_ui->tabWidget->count(); ++i) {
            auto* widget = m_ui->tabWidget->databaseWidgetFromIndex(i);
            if (tabIdFor(widget) != id) continue;
            const auto database = widget ? widget->database() : QSharedPointer<Database>();
            const QString key = database ? Material::tabPersistenceKeyForPath(database->filePath()) : QString();
            if (key.isEmpty()) {
                if (pinned) {
                    m_sessionPinnedTabs.insert(id);
                } else {
                    m_sessionPinnedTabs.remove(id);
                }
            } else {
                QStringList values = config()->get(Config::GUI_PinnedTabs).toStringList();
                values.removeAll(key);
                if (pinned) values.append(key);
                config()->set(Config::GUI_PinnedTabs, values);
            }
            syncTabStrip();
            break;
        }
    });
    connect(tabStrip, &Material::TabStrip::tabMoveRequested, this, [this, tabStrip, syncTabStrip](const QString& id, const QString& beforeId) {
        auto desired = tabStrip->tabs();
        int from = -1;
        for (int i = 0; i < desired.size(); ++i) if (desired.at(i).runtimeId == id) { from = i; break; }
        if (from < 0) return;
        const auto moved = desired.takeAt(from);
        int destination = desired.size();
        if (!beforeId.isEmpty()) {
            for (int i = 0; i < desired.size(); ++i) if (desired.at(i).runtimeId == beforeId) { destination = i; break; }
        }
        desired.insert(destination, moved);

        for (int target = 0; target < desired.size(); ++target) {
            int current = -1;
            for (int i = 0; i < m_ui->tabWidget->count(); ++i) {
                if (tabIdFor(m_ui->tabWidget->databaseWidgetFromIndex(i)) == desired.at(target).runtimeId) {
                    current = i;
                    break;
                }
            }
            if (current >= 0 && current != target) {
                m_ui->tabWidget->tabBar()->moveTab(current, target);
            }
        }

        QStringList persistentOrder;
        for (const auto& descriptor : desired) {
            if (descriptor.persistable && !persistentOrder.contains(descriptor.persistenceKey)) {
                persistentOrder.append(descriptor.persistenceKey);
            }
        }
        config()->set(Config::GUI_TabOrder, persistentOrder);
        syncTabStrip();
    });
    connect(tabStrip, &Material::TabStrip::newTabRequested, m_ui->actionDatabaseOpen, &QAction::trigger);

    auto* appBar = materialShell->appBar();
    connect(appBar, &Material::TopAppBar::saveRequested, m_ui->actionDatabaseSave, &QAction::trigger);
    // The casino button raises the design's generator sheet. The stock
    // generator page keeps its menu entry and its place in the palette, so
    // nothing is lost - only the app bar affordance moves.
    auto* generatorSheet = new Material::GeneratorSheet(this);
    connect(appBar, &Material::TopAppBar::generatorRequested, generatorSheet, &Material::Overlay::openOverlay);
    connect(generatorSheet, &Material::GeneratorSheet::passwordCopied, this, [](const QString& password) {
        clipboard()->setText(password);
        Material::Notify::success(tr("Copied to the clipboard. It clears in 10 seconds."));
    });

    // The bolt button and Ctrl+Shift+F are what the hidden menu bar became.
    auto* commandPalette = new Material::CommandPalette(this);
    connect(appBar, &Material::TopAppBar::paletteRequested, commandPalette, &Material::Overlay::openOverlay);
    new QShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_F, this, [commandPalette] { commandPalette->openOverlay(); });

    auto* regexBuilder = new Material::RegexBuilder(this);
    m_regexBuilder = regexBuilder;
    auto openBuilderFor = [regexBuilder](Material::SearchBar* bar) {
        Material::SearchRegistry::instance()->setCurrent(bar);
        if (bar) {
            regexBuilder->setPattern(bar->text());
            regexBuilder->setFlags(bar->regexFlags());
        }
        regexBuilder->openOverlay();
    };
    connect(appBar, &Material::TopAppBar::regexRequested, this, [openBuilderFor] { openBuilderFor(nullptr); });
    connect(Material::SearchRegistry::instance(),
            &Material::SearchRegistry::builderRequested,
            this,
            openBuilderFor);
    connect(regexBuilder, &Material::RegexBuilder::patternApplied, this, [regexBuilder](const QString& pattern) {
        auto* target = Material::SearchRegistry::instance()->current();
        if (!target) {
            Material::Notify::warning(tr("No search field is selected. The pattern was not applied."));
            return;
        }
        target->setRegexFlags(regexBuilder->flags());
        target->setRegexEnabled(true);
        target->setText(pattern);
    });
    connect(regexBuilder, &Material::Overlay::closed, this, [] {
        Material::SearchRegistry::instance()->restoreCurrentFocus();
    });
    connect(regexBuilder, &Material::RegexBuilder::patternCopied, this, [](const QString& pattern) {
        clipboard()->setText(pattern);
        // Every copy in the design says the same thing about the clipboard.
        Material::Notify::success(tr("Copied to the clipboard. It clears in 10 seconds."));
    });

    // The settings hub asks the window for the three things it cannot do
    // itself: the font chooser, the real integration pages and the builder.
    // The Appearance destination is the same screen the hub used to embed, so
    // it asks the window for the same three things.
    connect(appearanceScreen, &Material::SettingsScreen::interfaceFontRequested, this, &MainWindow::chooseInterfaceFont);
    connect(settingsHub, &Material::SettingsHub::interfaceFontRequested, this, &MainWindow::chooseInterfaceFont);

    auto showIntegration = [this, settingsHub, vaultScreen](const QString& id) {
        // The External tools page's command row acts rather than navigates.
        if (id == QLatin1String("external-editor")) {
            vaultScreen->openDatabaseFolderExternally();
            return;
        }
        // Each integration row in the overview lands on the spec sheet that
        // owns it; anything without one falls back to the classic editor.
        static const QHash<QString, QString> pages{{QStringLiteral("browser"), QStringLiteral("browser")},
                                                   {QStringLiteral("ssh-agent"), QStringLiteral("sshagent")},
                                                   {QStringLiteral("yubikey"), QStringLiteral("security")},
                                                   {QStringLiteral("keeshare"), QStringLiteral("keeshare")},
                                                   // Passkeys are a browser-integration setting in the
                                                   // design, and the hub folded them onto that page.
                                                   {QStringLiteral("passkeys"), QStringLiteral("browser")}};
        const QString page = pages.value(id);
        if (page.isEmpty()) {
            settingsHub->showClassicEditor();
        } else {
            settingsHub->setCurrentPage(page);
        }
        // Whichever page it landed on, it is the settings destination that
        // shows it.
        if (shell()) {
            shell()->setCurrentDestination(QStringLiteral("settings"));
        }
    };
    connect(appearanceScreen, &Material::SettingsScreen::integrationActivated, this, showIntegration);
    connect(settingsHub, &Material::SettingsHub::integrationActivated, this, showIntegration);

    if (auto* notifications = Material::NotificationCentre::centreFor(this)) {
        notifications->attachAppBar(appBar);
    }
    Material::Notify::setHost(this);

    connect(materialShell->rail(), &Material::NavigationRail::themeToggleRequested, this, [this] {
        const bool wasDark = theme()->isDark();
        // Theme::setMode() writes Config::GUI_ApplicationTheme itself, so the
        // choice outlives the session without going through the View ▸ Theme
        // action group. Triggering that group instead would persist the same
        // value and then re-apply the whole theme - style, palette, sheet and
        // font - for what is only a change of mode.
        theme()->setMode(wasDark ? Material::Mode::Light : Material::Mode::Dark);
        // setChecked() does not emit QActionGroup::triggered, which is what
        // makes it the right call here: it moves the radio to the mode that is
        // now stored without writing it a second time.
        (wasDark ? m_ui->actionThemeLight : m_ui->actionThemeDark)->setChecked(true);
    });
    connect(materialShell->rail(),
            &Material::NavigationRail::lockRequested,
            m_ui->actionLockAllDatabases,
            &QAction::trigger);

    // A finding names an entry by UUID; the Fix button opens it for editing.
    connect(reportsFeed, &Material::ReportsFeed::entryEditRequested, this, [this](const QString& uuidHex) {
        auto* dbWidget = m_ui->tabWidget->currentDatabaseWidget();
        const auto db = unlockedDatabase(dbWidget);
        if (!db) {
            return;
        }
        for (Entry* entry : db->rootGroup()->entriesRecursive()) {
            if (entry->uuidToHex() == uuidHex) {
                if (shell()) {
                    shell()->setCurrentDestination(QStringLiteral("vault"));
                }
                // Reveal it rather than opening the editor: switchToEntryEdit(Entry*) is private,
                // and a report row saying "look at this entry" should show it in context, not
                // drop the user straight into an edit form they did not ask for.
                if (auto* view = dbWidget->entryView()) {
                    view->setCurrentEntry(entry);
                    view->setFocus();
                }
                break;
            }
        }
    });

    // Nothing the Material reports screen shows replaces the full report tabs -
    // the HIBP check, the passkey list and the browser statistics still live
    // there, so the header button takes the user straight to them.
    connect(reportsFeed, &Material::ReportsFeed::detailedReportsRequested, this, [this] {
        if (shell()) {
            shell()->setCurrentDestination(QStringLiteral("vault"));
        }
        m_ui->actionReports->setChecked(true);
    });

    // Every save is a revision. The signal lives on the database widget, so it
    // is picked up as each one appears.
    auto watchSaves = [this](DatabaseWidget* dbWidget) {
        // Both databaseOpened() and databaseUnlocked() land here for the same
        // widget, so the connection is made once. Qt::UniqueConnection cannot
        // do that job - it asserts on a functor - so the widget is marked.
        static const char* const watchedProperty = "materialSaveWatcher";
        if (!dbWidget || dbWidget->property(watchedProperty).toBool()) {
            return;
        }
        dbWidget->setProperty(watchedProperty, true);
        const auto database = dbWidget->database();
        if (database && !database->filePath().isEmpty()
            && Material::HistoryStore::instance()->revisionsFor(database->filePath()).isEmpty()) {
            // Capture the encrypted file before the first edit in this session,
            // so a delete followed by Save always has a recoverable predecessor.
            Material::HistoryStore::instance()->recordSave(database);
        }
        connect(dbWidget, &DatabaseWidget::databaseSaved, this, [dbWidget] {
            Material::HistoryStore::instance()->recordSave(dbWidget->database());
        });
    };
    connect(m_ui->tabWidget, &DatabaseTabWidget::databaseOpened, this, watchSaves);
    connect(m_ui->tabWidget, &DatabaseTabWidget::databaseUnlocked, this, watchSaves);

    auto pointFeedsAtCurrentDatabase = [this, reportsFeed, historyFeed] {
        const auto db = unlockedDatabase(m_ui->tabWidget->currentDatabaseWidget());
        reportsFeed->setDatabase(db);
        historyFeed->setDatabase(db);
    };

    connect(materialShell, &Material::Shell::destinationChanged, this, [this, reportsFeed, historyFeed](const QString& id) {
        const bool vault = (id == QLatin1String("vault"));
        const bool settings = (id == QLatin1String("settings"));

        if (m_ui->actionSettings->isChecked() != settings) {
            m_ui->actionSettings->setChecked(settings);
        }
        if (!vault && m_ui->actionPasswordGenerator->isChecked()) {
            m_ui->actionPasswordGenerator->setChecked(false);
        }
        if (vault && !m_ui->actionPasswordGenerator->isChecked() && !m_ui->actionSettings->isChecked()) {
            switchToDatabases();
        }

        // The health check is the expensive part, so a destination only pays
        // for its own data when it is the one being looked at.
        const auto db = unlockedDatabase(m_ui->tabWidget->currentDatabaseWidget());
        if (id == QLatin1String("reports")) {
            reportsFeed->setDatabase(db);
        } else if (id == QLatin1String("history")) {
            historyFeed->setDatabase(db);
        }
        updateWindowTitle();
    });

    connect(m_ui->tabWidget, &DatabaseTabWidget::activeDatabaseChanged, this, [pointFeedsAtCurrentDatabase] {
        if (shell()
            && (shell()->currentDestination() == QLatin1String("reports")
                || shell()->currentDestination() == QLatin1String("history"))) {
            pointFeedsAtCurrentDatabase();
        }
    });

    syncTabStrip();

    restoreConfigState();
    updateMenuActionState();
    updateWindowTitle();
}

MainWindow::~MainWindow()
{
#ifdef KPXC_FEATURE_SSHAGENT
    sshAgent()->removeAllIdentities();
#endif
}

/**
 * Restore the main window's state after launch
 */
void MainWindow::restoreConfigState()
{
    if (config()->get(Config::OpenPreviousDatabasesOnStartup).toBool()) {
        const QStringList fileNames = config()->get(Config::LastOpenedDatabases).toStringList();
        for (const QString& filename : fileNames) {
            if (!filename.isEmpty() && QFile::exists(filename)) {
                openDatabase(filename);
            }
        }
        auto lastActiveFile = config()->get(Config::LastActiveDatabase).toString();
        if (!lastActiveFile.isEmpty()) {
            openDatabase(lastActiveFile);
        }
    }
}

QList<DatabaseWidget*> MainWindow::getOpenDatabases()
{
    QList<DatabaseWidget*> dbWidgets;
    for (int i = 0; i < m_ui->tabWidget->count(); ++i) {
        dbWidgets << m_ui->tabWidget->databaseWidgetFromIndex(i);
    }
    return dbWidgets;
}

void MainWindow::showErrorMessage(const QString& message)
{
    m_ui->globalMessageWidget->showMessage(message, MessageWidget::Error);
}

void MainWindow::appExit()
{
    m_appExitCalled = true;
    close();
}

/**
 * Returns if application was built with hardware key support.
 * Intended to be used by 3rd-party applications using DBus.
 *
 * @return True if built with hardware key support, false otherwise
 */
bool MainWindow::isHardwareKeySupported()
{
    return true;
}

/**
 * Refreshes list of hardware keys known.
 * Triggers the DatabaseOpenWidget to automatically select the key last used for a database if found.
 * Intended to be used by 3rd-party applications using DBus.
 *
 * @return True if any key was found, false otherwise or if application lacks hardware key support
 */
bool MainWindow::refreshHardwareKeys()
{
    auto yk = YubiKey::instance();
    // find keys sync to allow returning if any key was found
    bool found = yk->findValidKeys();
    // emit signal so DatabaseOpenWidget can select last used key
    // emit here manually because sync findValidKeys() cannot do that properly
    emit yk->detectComplete(found);
    return found;
}

void MainWindow::updateLastDatabasesMenu()
{
    m_ui->menuRecentDatabases->clear();

    const QStringList lastDatabases = config()->get(Config::LastDatabases).toStringList();
    for (const QString& database : lastDatabases) {
        QAction* action = m_ui->menuRecentDatabases->addAction(Tools::escapeAccelerators(database));
        action->setData(database);
        m_lastDatabasesActions->addAction(action);
    }
    m_ui->menuRecentDatabases->addSeparator();
    m_ui->menuRecentDatabases->addAction(m_clearHistoryAction);
}

void MainWindow::updateCopyAttributesMenu()
{
    DatabaseWidget* dbWidget = m_ui->tabWidget->currentDatabaseWidget();
    if (!dbWidget) {
        return;
    }

    if (dbWidget->numberOfSelectedEntries() != 1) {
        return;
    }

    QList<QAction*> actions = m_ui->menuEntryCopyAttribute->actions();
    for (int i = m_countDefaultAttributes; i < actions.size(); i++) {
        delete actions[i];
    }

    const QStringList customEntryAttributes = dbWidget->customEntryAttributes();
    for (const QString& key : customEntryAttributes) {
        QAction* action = m_ui->menuEntryCopyAttribute->addAction(key);
        action->setData(QVariant(key));
        m_copyAdditionalAttributeActions->addAction(action);
    }
}

void MainWindow::updateSetTagsMenu()
{
    m_ui->menuTags->setTearOffEnabled(true);

    auto dbWidget = m_ui->tabWidget->currentDatabaseWidget();
    if (dbWidget) {
        // Enumerate tags applied to the selected entries
        QSet<QString> selectedTags;
        for (const auto entry : dbWidget->entryView()->selectedEntries()) {
            for (const auto& tag : entry->tagList()) {
                selectedTags.insert(tag);
            }
        }

        // Remove missing tags
        const auto tagList = dbWidget->database()->tagList();
        for (const auto action : m_ui->menuTags->actions()) {
            if (!tagList.contains(action->text()) || !action->isEnabled()) {
                delete action;
            }
        }

        // Add known database tags as actions and set checked if
        // a selected entry has that tag
        QList<QAction*> actionList = m_ui->menuTags->actions();
        for (const auto& tag : tagList) {
            auto actionForTag = std::find_if(actionList.cbegin(),
                                             actionList.cend(),
                                             [&tag](const QAction* action) -> bool { return action->text() == tag; });
            QAction* action = actionForTag == actionList.cend() ? nullptr : *actionForTag;
            if (!action) {
                action = m_ui->menuTags->addAction(icons()->icon("tag"), tag);
                action->setCheckable(true);
                m_setTagsMenuActions->addAction(action);
            }
            action->setChecked(selectedTags.contains(tag));
        }
    }

    // If no tags exist in the database then show a tip to the user
    if (m_ui->menuTags->isEmpty()) {
        m_ui->menuTags->setTearOffEnabled(false);
        auto action = m_ui->menuTags->addAction(tr("No Tags"));
        action->setEnabled(false);
    }
}

void MainWindow::openRecentDatabase(QAction* action)
{
    openDatabase(action->data().toString());
}

void MainWindow::clearLastDatabases()
{
    config()->remove(Config::LastDatabases);
    bool inWelcomeWidget = (m_ui->stackedWidget->currentIndex() == 2);

    if (inWelcomeWidget) {
        m_ui->welcomeWidget->refreshLastDatabases();
    }
}

void MainWindow::openDatabase(const QString& filePath, const QString& password, const QString& keyfile)
{
    m_ui->tabWidget->addDatabaseTab(filePath, false, password, keyfile);
}

void MainWindow::updateMenuActionState()
{
    // MainWindow State
    int currentIndex = m_ui->stackedWidget->currentIndex();
    bool hasLockableDatabase = m_ui->tabWidget->hasLockableDatabases();
    bool inAppSettings = (currentIndex == SettingsScreen);
    bool inPasswordGenerator = (currentIndex == PasswordGeneratorScreen);

    auto dbWidget = (currentIndex == DatabaseTabScreen ? m_ui->tabWidget->currentDatabaseWidget() : nullptr);
    auto dbMode = (dbWidget ? dbWidget->currentMode() : DatabaseWidget::Mode::None);

    // Database State
    bool databaseUnlocked = (dbWidget && !dbWidget->isLocked());
    bool inDatabase = (dbMode == DatabaseWidget::Mode::ViewMode);
    bool inDatabaseSettings = (dbMode == DatabaseWidget::Mode::DatabaseSettingsMode);
    bool inReports = (dbMode == DatabaseWidget::Mode::ReportsMode);
    bool editingEntry = (dbMode == DatabaseWidget::Mode::EditEntryMode);

    // Synchronize toggle buttons
    m_ui->actionDatabaseSettings->blockSignals(true);
    m_ui->actionPasswordGenerator->blockSignals(true);
    m_ui->actionReports->blockSignals(true);
    m_ui->actionSettings->blockSignals(true);

    m_ui->actionDatabaseSettings->setChecked(inDatabaseSettings);
    m_ui->actionPasswordGenerator->setChecked(inPasswordGenerator);
    m_ui->actionReports->setChecked(inReports);
    m_ui->actionSettings->setChecked(inAppSettings);

    m_ui->actionDatabaseSettings->blockSignals(false);
    m_ui->actionPasswordGenerator->blockSignals(false);
    m_ui->actionReports->blockSignals(false);
    m_ui->actionSettings->blockSignals(false);

    // Entry State
    bool singleEntrySelected = (inDatabase && dbWidget->numberOfSelectedEntries() == 1);
    bool singleEntryOrEditing = (singleEntrySelected || editingEntry);
    bool multiEntrySelected = (inDatabase && dbWidget->numberOfSelectedEntries() > 0);

    // Group State
    bool groupSelected = (inDatabase && dbWidget->isGroupSelected());
    bool groupHasChildren = (groupSelected && dbWidget->currentGroup()->hasChildren());
    bool groupHasEntries = (groupSelected && !dbWidget->currentGroup()->entries().isEmpty());
    bool inRecycleBin = (inDatabase && dbWidget->isRecycleBinSelected());

    bool entryViewSorted = (inDatabase && dbWidget->isSorted());
    bool entryViewAtTop = (inDatabase && dbWidget->currentEntryIndex() == 0);
    bool entryViewAtBottom =
        (groupSelected && dbWidget->currentEntryIndex() == dbWidget->currentGroup()->entries().size() - 1);

    m_ui->actionEntryNew->setEnabled(inDatabase && !inRecycleBin);
    m_ui->actionEntryClone->setEnabled(singleEntrySelected && !inRecycleBin);
    m_ui->actionEntryEdit->setEnabled(singleEntrySelected);
    m_ui->actionEntryExpire->setEnabled(multiEntrySelected);
    m_ui->actionEntryDelete->setEnabled(multiEntrySelected);
    if (dbWidget) {
        if (dbWidget->database()->metadata()->recycleBinEnabled() && !inRecycleBin) {
            m_ui->actionEntryDelete->setToolTip(
                tr("Move selected entry(s) to the recycle bin", "", dbWidget->numberOfSelectedEntries()));
        } else {
            m_ui->actionEntryDelete->setToolTip(
                tr("Permanently delete the selected entry(s)", "", dbWidget->numberOfSelectedEntries()));
        }
    } else {
        m_ui->actionEntryDelete->setToolTip(tr("Delete Entry"));
    }
    bool hasRecycledEntries = (inDatabase && dbWidget && dbWidget->hasRecycledSelectedEntries());
    m_ui->actionEntryRestore->setVisible(multiEntrySelected && hasRecycledEntries);
    m_ui->actionEntryRestore->setEnabled(multiEntrySelected && hasRecycledEntries);
    if (dbWidget) {
        m_ui->actionEntryRestore->setText(tr("Restore Entry(s)", "", dbWidget->numberOfSelectedEntries()));
        m_ui->actionEntryRestore->setToolTip(tr("Restore Entry(s)", "", dbWidget->numberOfSelectedEntries()));
    }
    m_ui->actionEntryMoveUp->setVisible(inDatabase && !entryViewSorted);
    m_ui->actionEntryMoveDown->setVisible(inDatabase && !entryViewSorted);
    m_ui->actionEntryMoveUp->setEnabled(singleEntrySelected && !entryViewSorted && !entryViewAtTop);
    m_ui->actionEntryMoveDown->setEnabled(singleEntrySelected && !entryViewSorted && !entryViewAtBottom);
    m_ui->actionEntryCopyTitle->setEnabled(singleEntryOrEditing && dbWidget->currentEntryHasTitle());
    m_ui->actionEntryCopyUsername->setEnabled(singleEntryOrEditing && dbWidget->currentEntryHasUsername());
    // NOTE: Copy password is enabled even if the selected entry's password is blank to prevent Ctrl+C
    // from copying information from the currently selected cell in the entry view table.
    m_ui->actionEntryCopyPassword->setEnabled(singleEntryOrEditing);
    m_ui->actionEntryCopyURL->setEnabled(singleEntryOrEditing && dbWidget->currentEntryHasUrl());
    m_ui->actionEntryCopyNotes->setEnabled(singleEntryOrEditing && dbWidget->currentEntryHasNotes());
    m_ui->menuEntryCopyAttribute->setEnabled(singleEntryOrEditing);
    m_ui->menuEntryTotp->setEnabled(singleEntrySelected);
    m_ui->menuTags->setEnabled(multiEntrySelected);
    // Handle tear-off tags menu
    if (m_ui->menuTags->isTearOffMenuVisible()) {
        if (!databaseUnlocked) {
            m_ui->menuTags->hideTearOffMenu();
        } else {
            updateSetTagsMenu();
        }
    }
    m_ui->actionEntryAutoType->setEnabled(singleEntrySelected && dbWidget->currentEntryHasAutoTypeEnabled());
    m_ui->actionEntryAutoType->menu()->setEnabled(singleEntrySelected && dbWidget->currentEntryHasAutoTypeEnabled());
    m_ui->actionEntryAutoTypeSequence->setText(singleEntrySelected
                                                   ? dbWidget->currentSelectedEntry()->effectiveAutoTypeSequence()
                                                   : Group::RootAutoTypeSequence);
    m_ui->actionEntryAutoTypeSequence->setEnabled(singleEntrySelected);
    m_ui->actionEntryAutoTypeUsername->setEnabled(singleEntrySelected && dbWidget->currentEntryHasUsername());
    m_ui->actionEntryAutoTypeUsernameEnter->setEnabled(singleEntrySelected && dbWidget->currentEntryHasUsername());
    m_ui->actionEntryAutoTypePassword->setEnabled(singleEntrySelected && dbWidget->currentEntryHasPassword());
    m_ui->actionEntryAutoTypePasswordEnter->setEnabled(singleEntrySelected && dbWidget->currentEntryHasPassword());
    m_ui->actionEntryAutoTypeTOTP->setEnabled(singleEntrySelected && dbWidget->currentEntryHasTotp());
    m_ui->actionEntryAutoTypeURL->setEnabled(singleEntrySelected && dbWidget->currentEntryHasUrl());
    m_ui->actionEntryAutoTypeURLEnter->setEnabled(singleEntrySelected && dbWidget->currentEntryHasUrl());
    m_ui->actionEntryAutoTypeTOTP->setVisible(singleEntrySelected && dbWidget->currentEntryHasTotp());
    m_ui->actionEntryOpenUrl->setEnabled(singleEntryOrEditing && dbWidget->currentEntryHasUrl());
    m_ui->actionEntryTotp->setEnabled(singleEntrySelected && dbWidget->currentEntryHasTotp());
    m_ui->actionEntryCopyTotp->setEnabled(singleEntrySelected);
    m_ui->actionEntryCopyPasswordTotp->setEnabled(singleEntrySelected && dbWidget->currentEntryHasTotp());
    m_ui->actionEntrySetupTotp->setEnabled(singleEntrySelected);
    m_ui->actionEntryTotpQRCode->setEnabled(singleEntrySelected && dbWidget->currentEntryHasTotp());
    m_ui->actionEntryDownloadIcon->setEnabled((multiEntrySelected && !singleEntrySelected)
                                              || (singleEntrySelected && dbWidget->currentEntryHasUrl()));
#ifdef KPXC_FEATURE_BROWSER
    m_ui->actionEntryImportPasskey->setVisible(singleEntrySelected);
    m_ui->actionEntryImportPasskey->setEnabled(singleEntrySelected);
    m_ui->actionEntryRemovePasskey->setVisible(singleEntrySelected && dbWidget->currentEntryHasPasskey());
    m_ui->actionEntryRemovePasskey->setEnabled(singleEntrySelected && dbWidget->currentEntryHasPasskey());
#endif
#ifdef KPXC_FEATURE_SSHAGENT
    bool hasSSHKey = singleEntrySelected && sshAgent()->isEnabled() && dbWidget->currentEntryHasSshKey();
    m_ui->actionEntryAddToAgent->setVisible(hasSSHKey);
    m_ui->actionEntryAddToAgent->setEnabled(hasSSHKey);
    m_ui->actionEntryRemoveFromAgent->setVisible(hasSSHKey);
    m_ui->actionEntryRemoveFromAgent->setEnabled(hasSSHKey);
    m_ui->actionClearSSHAgent->setVisible(sshAgent()->isEnabled());
    m_ui->actionClearSSHAgent->setEnabled(sshAgent()->isEnabled());
#endif

    m_ui->actionGroupNew->setEnabled(groupSelected && !inRecycleBin);
    m_ui->actionGroupEdit->setEnabled(groupSelected);
    m_ui->actionGroupClone->setEnabled(groupSelected && dbWidget->canCloneCurrentGroup());
    m_ui->actionGroupDelete->setEnabled(groupSelected && dbWidget->canDeleteCurrentGroup());
    m_ui->actionGroupSortAsc->setVisible(groupHasChildren);
    m_ui->actionGroupSortAsc->setEnabled(groupHasChildren);
    m_ui->actionGroupSortDesc->setVisible(groupHasChildren);
    m_ui->actionGroupSortDesc->setEnabled(groupHasChildren);
    m_ui->actionGroupEmptyRecycleBin->setVisible(inRecycleBin);
    m_ui->actionGroupEmptyRecycleBin->setEnabled(inRecycleBin);
#ifdef KPXC_FEATURE_NETWORK
    m_ui->actionGroupDownloadFavicons->setVisible(!inRecycleBin);
#endif
    m_ui->actionGroupDownloadFavicons->setEnabled(groupSelected && groupHasEntries && !inRecycleBin);

    // Database Menu
    m_ui->actionDatabaseSave->setEnabled(databaseUnlocked && m_ui->tabWidget->canSave());
    m_ui->actionDatabaseSaveAs->setEnabled(databaseUnlocked);
    m_ui->actionDatabaseSaveBackup->setEnabled(databaseUnlocked);
    m_ui->actionDatabaseClose->setEnabled(dbWidget);
    m_ui->actionLockDatabase->setEnabled(databaseUnlocked);
    m_ui->actionLockAllDatabases->setEnabled(hasLockableDatabase);
    m_ui->actionLockDatabaseToolbar->setEnabled(hasLockableDatabase);
    m_ui->actionDatabaseSettings->setEnabled(inDatabase || inDatabaseSettings);
    m_ui->actionDatabaseSecurity->setEnabled(inDatabase || inDatabaseSettings);
    m_ui->actionReports->setEnabled(inDatabase || inReports);
    m_ui->menuRemoteSync->setEnabled(inDatabase || inDatabaseSettings);
    m_ui->menuExport->setEnabled(inDatabase);
    m_ui->actionDatabaseMerge->setEnabled(inDatabase);
#ifdef KPXC_FEATURE_BROWSER
    m_ui->actionPasskeys->setEnabled(inDatabase || inReports);
    m_ui->actionImportPasskey->setEnabled(inDatabase);
#endif

    m_searchWidgetAction->setEnabled(inDatabase);

    // The Save pill mirrors the Save action it triggers.
    if (auto* materialShell = shell()) {
        materialShell->appBar()->setSaveEnabled(m_ui->actionDatabaseSave->isEnabled());
    }
}

void MainWindow::updateToolbarSeparatorVisibility()
{
    if (!m_showToolbarSeparator) {
        m_ui->toolbarSeparator->setVisible(false);
        return;
    }

    switch (m_ui->stackedWidget->currentIndex()) {
    case DatabaseTabScreen:
        m_ui->toolbarSeparator->setVisible(!m_ui->tabWidget->tabBar()->isVisible()
                                           && m_ui->tabWidget->tabBar()->count() == 1);
        break;
    case SettingsScreen:
        m_ui->toolbarSeparator->setVisible(true);
        break;
    default:
        m_ui->toolbarSeparator->setVisible(false);
    }
}

void MainWindow::updateWindowTitle()
{
    QString customWindowTitlePart;
    int stackedWidgetIndex = m_ui->stackedWidget->currentIndex();
    int tabWidgetIndex = m_ui->tabWidget->currentIndex();
    bool isModified = m_ui->tabWidget->isModified(tabWidgetIndex);

    if (stackedWidgetIndex == DatabaseTabScreen && tabWidgetIndex != -1) {
        customWindowTitlePart = m_ui->tabWidget->tabName(tabWidgetIndex);
        if (isModified && customWindowTitlePart.endsWith("*")) {
            customWindowTitlePart.remove(customWindowTitlePart.size() - 1, 1);
        }
        m_ui->actionDatabaseSave->setEnabled(m_ui->tabWidget->canSave(tabWidgetIndex));
    } else if (stackedWidgetIndex == StackedWidgetIndex::SettingsScreen) {
        customWindowTitlePart = tr("Settings");
    } else if (stackedWidgetIndex == StackedWidgetIndex::PasswordGeneratorScreen) {
        customWindowTitlePart = tr("Password Generator");
    }

    QString windowTitle;
    if (customWindowTitlePart.isEmpty()) {
        windowTitle = QString("%1[*]").arg(BaseWindowTitle);
    } else {
        windowTitle = QString("%1[*] - %2").arg(customWindowTitlePart, BaseWindowTitle);
    }

    setWindowTitle(windowTitle);
    setWindowModified(isModified);

    // The app bar says what the window title says, split into the name of the
    // thing on screen and the path or context underneath it.
    if (auto* materialShell = shell()) {
        const QString destination = materialShell->currentDestination();
        auto* dbWidget = m_ui->tabWidget->currentDatabaseWidget();
        const QString databaseName = (tabWidgetIndex != -1)
                                         ? m_ui->tabWidget->tabName(tabWidgetIndex).remove(QLatin1Char('*')).trimmed()
                                         : QString();

        // The rail already names the destination, so the bar names the thing
        // being looked at: a spec sheet by its own label, the changelog by the
        // version it ends at, and everything else by the open database.
        const QString sheetLabel = Material::SheetCatalogue::label(destination);

        QString barTitle;
        QString barSubtitle;
        if (!sheetLabel.isEmpty()) {
            barTitle = sheetLabel;
            barSubtitle = tr("Recreated from the KeePassXC sources · every option, page and action");
        } else if (destination == QLatin1String("changelog")) {
            barTitle = tr("KeePassXC %1").arg(QString::fromLatin1(KEEPASSXC_VERSION));
            barSubtitle = tr("Every released version");
        } else if (destination == QLatin1String("appearance")) {
            barTitle = tr("Appearance & language");
            barSubtitle = tr("Theme, density, language and the voice of the messages");
        } else if (stackedWidgetIndex == StackedWidgetIndex::PasswordGeneratorScreen) {
            barTitle = tr("Password Generator");
            barSubtitle = tr("Standalone generator");
        } else if (dbWidget) {
            barTitle = databaseName.isEmpty() ? BaseWindowTitle : databaseName;
            barSubtitle = databaseSubtitle(dbWidget);
        } else {
            barTitle = BaseWindowTitle;
            barSubtitle = tr("No database open");
        }

        materialShell->appBar()->setTitle(barTitle);
        materialShell->appBar()->setSubtitle(barSubtitle);
        materialShell->appBar()->setSaveEnabled(m_ui->actionDatabaseSave->isEnabled());
    }

    updateTrayIcon();
}

void MainWindow::showAboutDialog()
{
    auto* aboutDialog = new AboutDialog(this);
    // Auto close the about dialog before attempting database locks
    if (m_ui->tabWidget->currentDatabaseWidget()) {
        connect(m_ui->tabWidget->currentDatabaseWidget(),
                &DatabaseWidget::databaseLockRequested,
                aboutDialog,
                &AboutDialog::close);
    }
    aboutDialog->open();
}

void MainWindow::performUpdateCheck()
{
#ifdef KPXC_FEATURE_UPDATES
    if (config()->get(Config::GUI_CheckForUpdates).toBool()) {
        updateCheck()->checkForUpdates(false);
    }

#endif
}

void MainWindow::showUpdateCheckDialog()
{
#ifdef KPXC_FEATURE_UPDATES
    // A user-requested retry is a new attempt and may report its result once.
    m_updateFailureNotified = false;
    updateCheck()->checkForUpdates(true);
#endif
}

void MainWindow::restartForUpdate()
{
#ifdef KPXC_FEATURE_UPDATES
    if (m_squirrelRestartRequested) {
        return;
    }
    if (!updateCheck()->canRestartThroughSquirrel()) {
        Material::Notify::error(tr("Restart unavailable"), tr("The Squirrel update launcher is missing or is not in the expected installation location."));
        return;
    }
    m_squirrelRestartRequested = true;
    if (auto* centre = Material::NotificationCentre::centreFor(this); centre && m_updateNotificationId != 0) {
        centre->updateEntry(m_updateNotificationId,
                            tr("Restarting to install update"),
                            tr("KeePassXC is closing safely before the verified update starts."),
                            Material::SeverityLevel::Warning,
                            {});
    }
    m_appExitCalled = true;
    close();
#endif
}

void MainWindow::deferUpdateForLater()
{
#ifdef KPXC_FEATURE_UPDATES
    updateCheck()->deferUpdate();
    showUpdateReadyNotification(true);
#endif
}

void MainWindow::showUpdateReadyNotification(bool replaceExisting)
{
#ifdef KPXC_FEATURE_UPDATES
    const auto candidate = updateCheck()->candidate();
    const QString title = tr("Update %1 is ready").arg(candidate.version);
    const QString body = tr("Current version: %1. Available version: %2. The update is unsigned and may show an Unknown Publisher or SmartScreen warning. Restart happens only when you choose it.")
                             .arg(QStringLiteral(KEEPASSXC_VERSION), candidate.version);
    QList<Material::NotificationAction> actions{
        {tr("Restart to install update"), [this] { restartForUpdate(); }, this},
        {tr("Release notes"), [this, notes = candidate.notesUrl] { customOpenUrl(notes); }, this},
    };
    if (updateCheck()->state() == UpdateChecker::State::ReadyToRestart) {
        actions.append({tr("Later"), [this] { deferUpdateForLater(); }, this});
    }

    auto* centre = Material::NotificationCentre::centreFor(this);
    if (replaceExisting && centre && m_updateNotificationId != 0) {
        centre->updateEntry(m_updateNotificationId, title, body, Material::SeverityLevel::Warning, actions);
        return;
    }

    Material::Notify::warning(title, body, actions);
    if (centre) {
        const auto notifications = centre->notifications();
        if (!notifications.isEmpty()) {
            m_updateNotificationId = notifications.constFirst().id;
        }
    }
#endif
}

void MainWindow::customOpenUrl(QString url)
{
    QDesktopServices::openUrl(QUrl(url));
}

void MainWindow::openDonateUrl()
{
    customOpenUrl("https://keepassxc.org/donate");
}

void MainWindow::openBugReportUrl()
{
    customOpenUrl("https://github.com/keepassxreboot/keepassxc/issues");
}

void MainWindow::openGettingStartedGuide()
{
    customOpenUrl(QString("file:///%1").arg(resources()->dataPath("docs/KeePassXC_GettingStarted.html")));
}

void MainWindow::openUserGuide()
{
    customOpenUrl(QString("file:///%1").arg(resources()->dataPath("docs/KeePassXC_UserGuide.html")));
}

void MainWindow::openOnlineHelp()
{
    customOpenUrl("https://keepassxc.org/docs/");
}

void MainWindow::openKeyboardShortcuts()
{
    customOpenUrl(QString("file:///%1").arg(resources()->dataPath("docs/KeePassXC_KeyboardShortcuts.html")));
}

void MainWindow::switchToDatabases()
{
    if (m_ui->tabWidget->currentIndex() == -1) {
        m_ui->stackedWidget->setCurrentIndex(WelcomeScreen);
        statusBar()->setAutoFillBackground(false);
    } else {
        m_ui->stackedWidget->setCurrentIndex(DatabaseTabScreen);
        statusBar()->setAutoFillBackground(true);
    }

    // Leaving the settings widget means leaving the settings destination. Any
    // other destination was chosen deliberately and is left where it is.
    if (shell() && shell()->currentDestination() == QLatin1String("settings")) {
        shell()->setCurrentDestination(QStringLiteral("vault"));
    }
}

void MainWindow::updateRailSublabels()
{
    auto* rail = shell() ? shell()->rail() : nullptr;
    if (!rail) {
        return;
    }

    auto* dbWidget = m_ui->tabWidget->currentDatabaseWidget();
    const auto db = (dbWidget && !dbWidget->isLocked()) ? dbWidget->database() : QSharedPointer<Database>();

    // Vault counts what is actually browsable: a locked or absent database
    // has nothing to report, and the sublabel disappears rather than lying.
    QString vault;
    if (db && db->rootGroup()) {
        vault = QString::number(db->rootGroup()->entriesRecursive(false).size());
    }
    rail->setSublabel(QStringLiteral("vault"), vault);

    // The design puts a warning glyph next to the finding count.
    rail->setSublabel(QStringLiteral("reports"),
                      m_reportFindings > 0 ? tr("%1 ⚠").arg(m_reportFindings) : QString());

    const auto* store = Material::HistoryStore::instance();
    int revisions = 0;
    if (store) {
        revisions = db ? store->revisionsFor(db->filePath()).size() : store->revisions().size();
    }
    rail->setSublabel(QStringLiteral("history"), revisions > 0 ? QString::number(revisions) : QString());
}

void MainWindow::chooseInterfaceFont()
{
    bool accepted = false;
    const QFont chosen = QFontDialog::getFont(&accepted, QApplication::font(), this, tr("Interface font"));
    if (!accepted) {
        return;
    }

    // The size is kept as the offset the rest of the application already
    // understands, so the slider in settings and this dialog agree.
    config()->set(Config::GUI_FontFamily, chosen.family());
    config()->set(Config::GUI_FontWeight, static_cast<int>(chosen.weight()));
    const int original = QFontInfo(QApplication::font()).pointSize();
    config()->set(Config::GUI_FontSizeOffset,
                  qBound(-2, chosen.pointSize() - original + config()->get(Config::GUI_FontSizeOffset).toInt(), 4));
    Application::applyFontSize();
}

void MainWindow::switchToSettings(bool enabled)
{
    if (enabled) {
        m_ui->settingsWidget->loadSettings();
        m_ui->stackedWidget->setCurrentIndex(SettingsScreen);
        statusBar()->setAutoFillBackground(true);
        if (shell()) {
            shell()->setCurrentDestination(QStringLiteral("settings"));
        }
    } else {
        switchToDatabases();
    }
}

void MainWindow::togglePasswordGenerator(bool enabled)
{
    if (enabled) {
        m_ui->passwordGeneratorWidget->loadSettings();
        m_ui->passwordGeneratorWidget->regeneratePassword();
        m_ui->stackedWidget->setCurrentIndex(PasswordGeneratorScreen);
        statusBar()->setAutoFillBackground(false);
        // The generator page lives inside the vault destination's stack.
        if (shell()) {
            shell()->setCurrentDestination(QStringLiteral("vault"));
        }
    } else {
        m_ui->passwordGeneratorWidget->saveSettings();
        switchToDatabases();
    }
}

void MainWindow::switchToNewDatabase()
{
    m_ui->tabWidget->newDatabase();
    switchToDatabases();
}

void MainWindow::switchToOpenDatabase()
{
    m_ui->tabWidget->openDatabase();
    switchToDatabases();
}

void MainWindow::switchToDatabaseFile(const QString& file)
{
    m_ui->tabWidget->addDatabaseTab(file);
    switchToDatabases();
}

void MainWindow::updateRemoteSyncMenuEntries()
{
    m_ui->menuRemoteSync->clear();

    auto dbWidget = m_ui->tabWidget->currentDatabaseWidget();
    if (dbWidget) {
        // Setup sync shortcut
        auto action = m_ui->menuRemoteSync->addAction(tr("Setup Remote Sync…"));
        connect(action, &QAction::triggered, dbWidget, &DatabaseWidget::switchToRemoteSettings);

        m_ui->menuRemoteSync->addSeparator();

        // Build remote sync menu
        for (const auto params : dbWidget->getRemoteParams()) {
            auto* remoteSyncAction = new QAction(params->name, this);
            m_ui->menuRemoteSync->addAction(remoteSyncAction);
            connect(remoteSyncAction, &QAction::triggered, dbWidget, [=] { dbWidget->syncWithRemote(params); });
        }
    }
}

void MainWindow::databaseStatusChanged(DatabaseWidget* dbWidget)
{
    Q_UNUSED(dbWidget);
    updateTrayIcon();
}

/**
 * Select a database tab by its index. Stays bounded to first/last tab
 * on overflow unless wrap is true.
 *
 * @param tabIndex 0-based tab index selector
 * @param wrap if true wrap around to first/last tab
 */
void MainWindow::selectDatabaseTab(int tabIndex, bool wrap)
{
    if (m_ui->stackedWidget->currentIndex() == DatabaseTabScreen) {
        if (wrap) {
            if (tabIndex < 0) {
                tabIndex = m_ui->tabWidget->count() - 1;
            } else if (tabIndex >= m_ui->tabWidget->count()) {
                tabIndex = 0;
            }
        } else {
            tabIndex = qBound(0, tabIndex, m_ui->tabWidget->count() - 1);
        }

        m_ui->tabWidget->setCurrentIndex(tabIndex);
    }
}

void MainWindow::selectNextDatabaseTab()
{
    selectDatabaseTab(m_ui->tabWidget->currentIndex() + 1, true);
}

void MainWindow::selectPreviousDatabaseTab()
{
    selectDatabaseTab(m_ui->tabWidget->currentIndex() - 1, true);
}

void MainWindow::databaseTabChanged(int tabIndex)
{
    if (tabIndex != -1 && m_ui->stackedWidget->currentIndex() == WelcomeScreen) {
        m_ui->stackedWidget->setCurrentIndex(DatabaseTabScreen);
        statusBar()->setAutoFillBackground(true);
    } else if (tabIndex == -1 && m_ui->stackedWidget->currentIndex() == DatabaseTabScreen) {
        m_ui->stackedWidget->setCurrentIndex(WelcomeScreen);
        statusBar()->setAutoFillBackground(false);
    }

    m_actionMultiplexer.setCurrentObject(m_ui->tabWidget->currentDatabaseWidget());
    updateEntryCountLabel();

    // Clear the tags menu to prevent re-use between databases
    for (const auto action : m_ui->menuTags->actions()) {
        delete action;
    }
}

bool MainWindow::event(QEvent* event)
{
    if (event->type() == QEvent::ShortcutOverride) {
        const auto keyevent = static_cast<QKeyEvent*>(event);
        // Did we get a ShortcutOverride event with the same key sequence as the OS default
        // copy-to-clipboard shortcut?
        if (keyevent->matches(QKeySequence::Copy)) {
            // If so, we ask the database widget to check if any of its sub-widgets has
            // text selected, and to copy it to the clipboard if that is the case.
            // We do this because that is what the user likely expects to happen, yet Qt does not
            // behave like that (at least on some platforms).
            auto dbWidget = m_ui->tabWidget->currentDatabaseWidget();
            if (dbWidget && dbWidget->copyFocusedTextSelection()) {
                // Note: instead of actively copying the selected text to the clipboard
                // above, simply accepting the event would have a similar effect (Qt
                // would deliver it as a key press to the current widget, which would
                // trigger the built-in copy-to-clipboard behaviour). However, that
                // would not come with our special (configurable) behaviour of
                // clearing the clipboard after a certain time period.
                keyevent->accept();
                return true;
            }
        }
    }
    return QMainWindow::event(event);
}

void MainWindow::showEvent(QShowEvent* event)
{
    Q_UNUSED(event)
#ifdef Q_OS_WIN
    // Qt Hack - Prevent white flicker when showing window
    QTimer::singleShot(50, this, [=] { setProperty("windowOpacity", 1.0); });
#endif

    // Restore geometry and window state only on the first showEvent to prevent issues with minimized tray startup
    if (!m_windowInformationRestored) {
        restoreWindowInformation();
        m_windowInformationRestored = true;
    }

    // Dress the title bar in the application's own colours. The native handle
    // exists by the time a show event arrives, and install() is idempotent, so
    // a return from the tray only refreshes the attributes. Only Windows has a
    // caption an application is allowed to repaint, and the translation unit is
    // only in the build there, so the call is fenced off rather than left to
    // fail at link time on a platform that does not compile it.
#ifdef Q_OS_WIN
    Material::WindowChrome::install(this);
    // The application draws its own caption; the desktop keeps the frame.
    if (!QCoreApplication::arguments().contains(QStringLiteral("--native-caption"))) {
        Material::WindowChrome::installFrameless(this);
    }
#endif

    // State plainly, once, that the humour level styles warnings and errors too.
    if (Material::Voice::disclosurePending()) {
        QTimer::singleShot(0, this, [this] { Material::Voice::presentDisclosure(this); });
    }
}

void MainWindow::hideEvent(QHideEvent* event)
{
    Q_UNUSED(event)
#ifdef Q_OS_WIN
    // Qt Hack - Prevent white flicker when showing window
    setProperty("windowOpacity", 0.0);
#endif
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (m_appExiting) {
        event->accept();
        return;
    }

    // Ignore event and hide to tray if this is not an actual close
    // request by the system's session manager.
    if (config()->get(Config::GUI_MinimizeOnClose).toBool() && !m_appExitCalled && !isHidden()
        && !qApp->isSavingSession()) {
        event->ignore();
        hideWindow();
        return;
    }

    m_appExiting = saveLastDatabases();
    if (m_appExiting) {
        saveWindowInformation();
        event->accept();
        if (m_squirrelRestartRequested) {
#ifdef KPXC_FEATURE_UPDATES
            if (kpxcApp->restart([] { return updateCheck()->launchUpdatedVersion(); })) {
                return;
            }
            m_appExiting = false;
            m_appExitCalled = false;
            m_squirrelRestartRequested = false;
            event->ignore();
            Material::Notify::error(
                tr("Restart failed"),
                tr("The verified update remains installed, but the Squirrel launcher could not start it. KeePassXC is still open."));
            showUpdateReadyNotification(true);
            return;
#endif
        }
        m_restartRequested ? kpxcApp->restart() : QApplication::quit();
        return;
    }

    m_appExitCalled = false;
    m_restartRequested = false;
    if (m_squirrelRestartRequested) {
        m_squirrelRestartRequested = false;
        showUpdateReadyNotification(true);
    }
    event->ignore();
}

bool MainWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result)
{
#ifdef Q_OS_WIN
    if (eventType == "windows_generic_MSG") {
        auto* shell = Material::Shell::instance();
        auto* titleBar = shell ? shell->titleBar() : nullptr;
        if (titleBar
            && Material::WindowChrome::handleNativeEvent(this, message, result, [this, titleBar](const QPoint& local) {
                   return titleBar->isCaptionArea(titleBar->mapFrom(this, local));
               })) {
            return true;
        }
    }
#endif
    return QMainWindow::nativeEvent(eventType, message, result);
}

void MainWindow::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::WindowStateChange) {
        if (auto* shell = Material::Shell::instance()) {
            if (shell->titleBar()) {
                shell->titleBar()->setMaximized(isMaximized());
            }
        }
    }
    if ((event->type() == QEvent::WindowStateChange) && isMinimized()) {
        if (isTrayIconEnabled() && config()->get(Config::GUI_MinimizeToTray).toBool()) {
            event->ignore();
            hide();
        }

        if (config()->get(Config::Security_LockDatabaseMinimize).toBool()) {
            m_ui->tabWidget->lockDatabasesDelayed();
        }
    } else {
        QMainWindow::changeEvent(event);
    }
}

void MainWindow::keyPressEvent(QKeyEvent* event)
{
    if (!event->modifiers()) {
        // Allow for direct focus of search, group view, and entry view
        auto dbWidget = m_ui->tabWidget->currentDatabaseWidget();
        if (dbWidget && dbWidget->isEntryViewActive()) {
            if (event->key() == Qt::Key_F1) {
                dbWidget->focusOnGroups(true);
                return;
            } else if (event->key() == Qt::Key_F2) {
                dbWidget->focusOnEntries(true);
                return;
            } else if (event->key() == Qt::Key_F3 || event->key() == Qt::Key_F6) {
                focusSearchWidget();
                return;
            } else if (event->key() == Qt::Key_Escape && dbWidget->isSearchActive()) {
                m_searchWidget->clearSearch();
                return;
            }
        }
    }

    QMainWindow::keyPressEvent(event);
}

bool MainWindow::focusNextPrevChild(bool next)
{
    // Only navigate around the main window if the database widget is showing the entry view
    auto dbWidget = m_ui->tabWidget->currentDatabaseWidget();
    if (dbWidget && dbWidget->isVisible() && dbWidget->isEntryViewActive()) {
        // Search Widget <-> Tab Widget <-> DbWidget
        if (next) {
            if (m_searchWidget->hasFocus()) {
                if (m_ui->tabWidget->count() > 1) {
                    m_ui->tabWidget->setFocus(Qt::TabFocusReason);
                } else {
                    dbWidget->setFocus(Qt::TabFocusReason);
                }
            } else if (m_ui->tabWidget->hasFocus()) {
                dbWidget->setFocus(Qt::TabFocusReason);
            } else {
                focusSearchWidget();
            }
        } else {
            if (m_searchWidget->hasFocus()) {
                dbWidget->setFocus(Qt::BacktabFocusReason);
            } else if (m_ui->tabWidget->hasFocus()) {
                focusSearchWidget();
            } else {
                if (m_ui->tabWidget->count() > 1) {
                    m_ui->tabWidget->setFocus(Qt::BacktabFocusReason);
                } else {
                    focusSearchWidget();
                }
            }
        }
        return true;
    }

    // Defer to Qt to make a decision, this maintains normal behavior
    return QMainWindow::focusNextPrevChild(next);
}

void MainWindow::focusSearchWidget()
{
    // The vault's own search pill is the search field now. It drives the same
    // DatabaseWidget::search() the tool bar field does, so Ctrl+F lands there
    // whenever a database is actually being browsed.
    if (shell()) {
        auto* vault = qobject_cast<Material::VaultScreen*>(shell()->destination(QStringLiteral("vault")));
        auto* dbWidget = vault ? vault->databaseWidget() : nullptr;
        if (dbWidget && !dbWidget->isLocked()) {
            shell()->setCurrentDestination(QStringLiteral("vault"));
            vault->focusSearch();
            return;
        }
    }

    if (m_searchWidgetAction->isEnabled()) {
        // The search field is still a tool bar widget, so the tool bar is
        // raised for exactly as long as the search needs it and drops again on
        // cancel or focus loss. It is the one piece of stock chrome that can
        // still appear, and only on demand.
        m_ui->toolBar->setVisible(true);
        m_ui->toolBar->setExpanded(true);
        m_searchWidget->focusSearch();
    }
}

void MainWindow::enableMenuAndToolbar()
{
    m_ui->toolBar->setDisabled(false);
    m_ui->menubar->setDisabled(false);
    if (auto* materialShell = shell()) {
        materialShell->rail()->setDisabled(false);
        materialShell->appBar()->setDisabled(false);
        materialShell->tabs()->setDisabled(false);
        // The shell's Go To commands and theme toggle are actions, not part of
        // the rail widget, so the palette keeps offering them unless they are
        // turned off by name.
        materialShell->setCommandsEnabled(true);
    }
}

void MainWindow::disableMenuAndToolbar()
{
    m_ui->toolBar->setDisabled(true);
    m_ui->menubar->setDisabled(true);
    if (auto* materialShell = shell()) {
        materialShell->rail()->setDisabled(true);
        materialShell->appBar()->setDisabled(true);
        materialShell->tabs()->setDisabled(true);
        materialShell->setCommandsEnabled(false);
    }
}

void MainWindow::clearSSHAgent()
{
#ifdef KPXC_FEATURE_SSHAGENT
    auto agent = SSHAgent::instance();
    auto ret = agent->clearAllAgentIdentities();
    displayGlobalMessage(agent->errorString(), ret ? MessageWidget::Positive : KMessageWidget::Error, false);
#endif
}

void MainWindow::saveWindowInformation()
{
    if (isVisible()) {
        config()->set(Config::GUI_MainWindowGeometry, saveGeometry());
        config()->set(Config::GUI_MainWindowState, saveState());
    }
}

void MainWindow::restoreWindowInformation()
{
    restoreGeometry(config()->get(Config::GUI_MainWindowGeometry).toByteArray());
    restoreState(config()->get(Config::GUI_MainWindowState).toByteArray());
    // A state saved before the shell existed can bring the tool bar back up.
    m_ui->toolBar->setHidden(true);
}

bool MainWindow::saveLastDatabases()
{
    if (config()->get(Config::OpenPreviousDatabasesOnStartup).toBool()) {
        auto currentDbWidget = m_ui->tabWidget->currentDatabaseWidget();
        if (currentDbWidget && !currentDbWidget->database()->isTemporaryDatabase()) {
            config()->set(Config::LastActiveDatabase, currentDbWidget->database()->filePath());
        } else {
            config()->remove(Config::LastActiveDatabase);
        }

        QStringList openDatabases;
        for (int i = 0; i < m_ui->tabWidget->count(); ++i) {
            auto dbWidget = m_ui->tabWidget->databaseWidgetFromIndex(i);
            if (!dbWidget->database()->isTemporaryDatabase()) {
                openDatabases.append(QDir::toNativeSeparators(dbWidget->database()->filePath()));
            }
        }

        config()->set(Config::LastOpenedDatabases, openDatabases);
    } else {
        config()->remove(Config::LastActiveDatabase);
        config()->remove(Config::LastOpenedDatabases);
    }

    return m_ui->tabWidget->closeAllDatabaseTabs();
}

void MainWindow::updateTrayIcon()
{
    if (config()->get(Config::GUI_ShowTrayIcon).toBool()) {
        if (!m_trayIcon) {
            m_trayIcon = new QSystemTrayIcon(this);
            auto* menu = new QMenu(this);

            auto* actionToggle = new QAction(tr("Toggle window"), menu);
            menu->addAction(actionToggle);
            actionToggle->setIcon(icons()->icon("keepassxc-monochrome-dark"));

            menu->addAction(m_ui->actionLockAllDatabases);

#ifdef Q_OS_MACOS
            auto actionQuit = new QAction(tr("Quit KeePassXC"), menu);
            connect(actionQuit, SIGNAL(triggered()), SLOT(appExit()));
            menu->addAction(actionQuit);
#else
            menu->addAction(m_ui->actionQuit);
#endif
            m_trayIcon->setContextMenu(menu);

            connect(m_trayIcon,
                    SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
                    SLOT(trayIconTriggered(QSystemTrayIcon::ActivationReason)));
            connect(actionToggle, SIGNAL(triggered()), SLOT(toggleWindow()));
        }

        bool showUnlocked = m_ui->tabWidget->hasLockableDatabases();
        m_trayIcon->setIcon(icons()->trayIcon(showUnlocked));
        m_trayIcon->setToolTip(windowTitle().replace("[*]", isWindowModified() ? "*" : ""));
        m_trayIcon->show();

        if (!isTrayIconEnabled() || !QSystemTrayIcon::isSystemTrayAvailable()) {
            // Try to show tray icon after 5 seconds, try 5 times
            // This can happen if KeePassXC starts before the system tray is available
            static int trayIconAttempts = 0;
            if (trayIconAttempts < 5) {
                QTimer::singleShot(5000, this, &MainWindow::updateTrayIcon);
                ++trayIconAttempts;
            }
        }
    } else {
        if (m_trayIcon) {
            m_trayIcon->hide();
            delete m_trayIcon;
        }
    }

    QApplication::setQuitOnLastWindowClosed(!isTrayIconEnabled());
}

void MainWindow::updateProgressBar(int percentage, QString message)
{
    // Long operations report through the corner notification host, which
    // keeps a single progress card per operation id and dismisses it when the
    // percentage drops below zero.
    static const QString progressId = QStringLiteral("main-window-operation");
    Material::Notify::progress(progressId, message, percentage);
}

void MainWindow::updateEntryCountLabel()
{
    auto dbWidget = m_ui->tabWidget->currentDatabaseWidget();
    QString vaultSublabel;
    if (dbWidget && dbWidget->currentMode() == DatabaseWidget::Mode::ViewMode) {
        int numEntries = dbWidget->entryView()->model()->rowCount();
        m_statusBarLabel->setText(tr("%1 Entry(s)", "", numEntries).arg(numEntries));
        vaultSublabel = QString::number(numEntries);
    } else {
        m_statusBarLabel->setText("");
    }

    // The rail's Vault tile carries the same count under its label.
    if (auto* materialShell = shell()) {
        materialShell->rail()->setSublabel(QStringLiteral("vault"), vaultSublabel);
    }
}

void MainWindow::obtainContextFocusLock()
{
    m_contextMenuFocusLock = true;
}

void MainWindow::releaseContextFocusLock()
{
    m_contextMenuFocusLock = false;
}

void MainWindow::agentEnabled(bool enabled)
{
    m_ui->actionEntryAddToAgent->setVisible(enabled);
    m_ui->actionEntryRemoveFromAgent->setVisible(enabled);
    m_ui->actionClearSSHAgent->setEnabled(enabled);
    m_ui->actionClearSSHAgent->setVisible(enabled);
}

void MainWindow::showEntryContextMenu(const QPoint& globalPos)
{
    bool entrySelected = false;
    auto dbWidget = m_ui->tabWidget->currentDatabaseWidget();
    if (dbWidget) {
        entrySelected = dbWidget->numberOfSelectedEntries() > 0;
    }

    if (entrySelected) {
        m_entryContextMenu->popup(globalPos);
    } else {
        m_entryNewContextMenu->popup(globalPos);
    }
}

void MainWindow::showGroupContextMenu(const QPoint& globalPos)
{
    m_ui->menuGroups->popup(globalPos);
}

void MainWindow::applySettingsChanges()
{
    if (config()->get(Config::Security_LockDatabaseIdle).toBool()) {
        auto timeout = config()->get(Config::Security_LockDatabaseIdleSeconds).toInt() * 1000;
        m_inactivityTimer->activate(timeout);
    } else {
        m_inactivityTimer->deactivate();
    }

    // The Material shell is the chrome now, so neither the menu bar nor the
    // tool bar is ever shown. The menu bar is squashed to zero height rather
    // than hidden because setHidden() disables the menu keyboard shortcuts on
    // Wayland, and those shortcuts are the whole reason the menus still exist.
    {
        // Syncing the two view toggles must not write the user's configuration
        // back, nor re-enter this function.
        const QSignalBlocker toolbarBlocker(m_ui->actionShowToolbar);
        const QSignalBlocker menubarBlocker(m_ui->actionShowMenubar);
        m_ui->actionShowToolbar->setChecked(false);
        m_ui->actionShowMenubar->setChecked(false);
    }

#ifndef Q_OS_MACOS
    m_ui->menubar->setMaximumHeight(0);
#endif

    m_ui->toolBar->setHidden(true);
    m_ui->toolBar->setMovable(false);
    addToolBar(Qt::TopToolBarArea, m_ui->toolBar);

    bool isOk = false;
    const auto toolButtonStyle =
        static_cast<Qt::ToolButtonStyle>(config()->get(Config::GUI_ToolButtonStyle).toInt(&isOk));
    if (isOk) {
        m_ui->toolBar->setToolButtonStyle(toolButtonStyle);
    }

    updateTrayIcon();

    kpxcApp->applyFontSize();
}

bool MainWindow::captureNavigate(const QString& screen, const QString& page)
{
    auto* materialShell = shell();
    const QString destination = Material::CaptureRoute::destinationFor(screen);
    if (!materialShell || destination.isEmpty() || !materialShell->destinations().contains(destination)) {
        return false;
    }
    materialShell->setCurrentDestination(destination);
    if (destination == QLatin1String("settings") && m_settingsHub && !page.isEmpty()) {
        m_settingsHub->setCurrentPage(page);
    } else if (!page.isEmpty()) {
        if (auto* sheet = qobject_cast<Material::SpecSheet*>(materialShell->destination(destination))) {
            sheet->setCurrentPage(page);
        }
    }
    if (screen == QLatin1String("regex-builder") && m_regexBuilder) {
        Material::SearchRegistry::instance()->setCurrent(nullptr);
        m_regexBuilder->openOverlay();
    }
    return true;
}

void MainWindow::setAllowScreenCapture(bool state)
{
    const Qt::WindowStates originalState = windowState();
    const bool wasVisible = isVisible();
    m_allowScreenCapture = state;
    for (auto window : qApp->topLevelWindows()) {
        if (window->isVisible() && (window->type() == Qt::Window || window->type() == Qt::Dialog)) {
            osUtils->setPreventScreenCapture(window, !m_allowScreenCapture);
        }
    }
    m_ui->actionAllowScreenCapture->blockSignals(true);
    m_ui->actionAllowScreenCapture->setChecked(m_allowScreenCapture);
    m_ui->actionAllowScreenCapture->blockSignals(false);

    // Changing capture affinity must never change application visibility or
    // window state. Restore defensively because platform capture APIs and
    // transient menu windows can otherwise provoke a state transition.
    if (windowState() != originalState) {
        setWindowState(originalState);
    }
    if (wasVisible && !isVisible()) {
        QMainWindow::show();
    } else if (!wasVisible && isVisible()) {
        QMainWindow::hide();
    }
}

void MainWindow::focusWindowChanged(QWindow* window)
{
    if (window != windowHandle()) {
        m_lastFocusOutTime = Clock::currentMilliSecondsSinceEpoch();
    }

    if (!osUtils->setPreventScreenCapture(window, !m_allowScreenCapture) && !m_allowScreenCapture) {
        displayGlobalMessage(QObject::tr("Warning: Failed to block screenshot capture on a top-level window."),
                             MessageWidget::Error);
    }
}

void MainWindow::trayIconTriggered(QSystemTrayIcon::ActivationReason reason)
{
    if (!m_trayIconTriggerTimer.isActive()) {
        m_trayIconTriggerTimer.start(150);
    }
    // Overcome Qt bug https://bugreports.qt.io/browse/QTBUG-69698
    // Store last issued tray icon activation reason to properly
    // capture doubleclick events
    m_trayIconTriggerReason = reason;
}

void MainWindow::processTrayIconTrigger()
{
#ifdef Q_OS_MACOS
    // Do not toggle the window on macOS and just show the context menu instead.
    // Right click detection doesn't seem to be working anyway
    // and anything else will only trigger the context menu AND
    // toggle the window at the same time, which is confusing at best.
    // Showing only a context menu for tray icons seems to be best
    // practice on macOS anyway, so this is probably fine.
    return;
#endif

    if (m_trayIconTriggerReason == QSystemTrayIcon::DoubleClick) {
        // Always toggle window on double click
        toggleWindow();
    } else if (m_trayIconTriggerReason == QSystemTrayIcon::Trigger
               || m_trayIconTriggerReason == QSystemTrayIcon::MiddleClick) {
        // Toggle window if is not in front.
#ifdef Q_OS_WIN
        // If on Windows, check if focus switched within the 500 milliseconds since
        // clicking the tray icon removes focus from main window.
        if (isHidden() || (Clock::currentMilliSecondsSinceEpoch() - m_lastFocusOutTime) <= 500) {
#else
        // If on Linux, check if the window has focus.
        if (hasFocus() || isHidden() || windowHandle()->isActive()) {
#endif
            toggleWindow();
        } else {
            bringToFront();
        }
    }
}

void MainWindow::show()
{
#ifndef Q_OS_WIN
    m_lastShowTime = Clock::currentMilliSecondsSinceEpoch();
#endif
#ifdef Q_OS_MACOS
    // Unset minimize state to avoid weird fly-in effects
    setWindowState(windowState() & ~Qt::WindowMinimized);
    macUtils()->toggleForegroundApp(true);
#endif
    QMainWindow::show();
}

void MainWindow::hide()
{
#ifndef Q_OS_WIN
    qint64 current_time = Clock::currentMilliSecondsSinceEpoch();
    if (current_time - m_lastShowTime < 250) {
        return;
    }
#endif
    QMainWindow::hide();
#ifdef Q_OS_MACOS
    macUtils()->toggleForegroundApp(false);
#endif
}

void MainWindow::hideWindow()
{
    saveWindowInformation();

    // Only hide if tray icon is active, otherwise window will be gone forever
    if (isTrayIconEnabled()) {
        // On X11, the window should NOT be minimized and hidden at the same time. See issue #1595.
        // On macOS, we are skipping minimization as well to avoid playing the magic lamp animation.
        if (QGuiApplication::platformName() != "xcb" && QGuiApplication::platformName() != "cocoa") {
            setWindowState(windowState() | Qt::WindowMinimized);
        }
        hide();
    } else {
        showMinimized();
    }

    if (config()->get(Config::Security_LockDatabaseMinimize).toBool()) {
        m_ui->tabWidget->lockDatabasesDelayed();
    }
}

void MainWindow::minimizeOrHide()
{
    if (config()->get(Config::GUI_MinimizeToTray).toBool()) {
        hideWindow();
    } else {
        showMinimized();
    }
}

void MainWindow::toggleWindow()
{
    if (isVisible() && !isMinimized()) {
        hideWindow();
    } else {
        bringToFront();
    }
}

void MainWindow::closeModalWindow()
{
    if (qApp->modalWindow()) {
        qApp->modalWindow()->close();
    }
}

bool MainWindow::isTrayIconEnabled() const
{
    return m_trayIcon && m_trayIcon->isVisible();
}

void MainWindow::displayGlobalMessage(const QString& text,
                                      MessageWidget::MessageType type,
                                      bool showClosebutton,
                                      int autoHideTimeout)
{
    m_ui->globalMessageWidget->setCloseButtonVisible(showClosebutton);
    m_ui->globalMessageWidget->showMessage(text, type, autoHideTimeout);
}

void MainWindow::displayTabMessage(const QString& text,
                                   MessageWidget::MessageType type,
                                   bool showClosebutton,
                                   int autoHideTimeout)
{
    m_ui->tabWidget->currentDatabaseWidget()->showMessage(text, type, showClosebutton, autoHideTimeout);
}

void MainWindow::hideGlobalMessage()
{
    m_ui->globalMessageWidget->hideMessage();
}

void MainWindow::showYubiKeyPopup()
{
    displayGlobalMessage(tr("Please present or touch your YubiKey to continue…"),
                         MessageWidget::Information,
                         false,
                         MessageWidget::DisableAutoHide);
    setEnabled(false);
}

void MainWindow::hideYubiKeyPopup()
{
    hideGlobalMessage();
    setEnabled(true);
}

void MainWindow::bringToFront()
{
    ensurePolished();
    setWindowState((windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
    show();
    raise();
    activateWindow();
}

void MainWindow::handleScreenLock()
{
    if (config()->get(Config::Security_LockDatabaseScreenLock).toBool()) {
        lockAllDatabases();
    }
}

QStringList MainWindow::kdbxFilesFromUrls(const QList<QUrl>& urls)
{
    QStringList kdbxFiles;
    for (const QUrl& url : urls) {
        const QFileInfo fInfo(url.toLocalFile());
        const bool isKdbxFile = fInfo.isFile() && fInfo.suffix().toLower() == "kdbx";
        if (isKdbxFile) {
            kdbxFiles.append(fInfo.absoluteFilePath());
        }
    }

    return kdbxFiles;
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
    const QMimeData* mimeData = event->mimeData();
    if (mimeData->hasUrls()) {
        const QStringList kdbxFiles = kdbxFilesFromUrls(mimeData->urls());
        if (!kdbxFiles.isEmpty()) {
            event->acceptProposedAction();
        }
    }
}

void MainWindow::dropEvent(QDropEvent* event)
{
    const QMimeData* mimeData = event->mimeData();
    if (mimeData->hasUrls()) {
        const QStringList kdbxFiles = kdbxFilesFromUrls(mimeData->urls());
        if (!kdbxFiles.isEmpty()) {
            event->acceptProposedAction();
        }
        for (const QString& kdbxFile : kdbxFiles) {
            openDatabase(kdbxFile);
        }
    }
}

void MainWindow::closeAllDatabases()
{
    m_ui->tabWidget->closeAllDatabaseTabs();
}

void MainWindow::lockAllDatabases()
{
    m_ui->tabWidget->lockDatabases();
}

void MainWindow::displayDesktopNotification(const QString& msg, QString title, int msTimeoutHint)
{
    if (!m_trayIcon || !QSystemTrayIcon::supportsMessages()) {
        return;
    }

    if (title.isEmpty()) {
        title = BaseWindowTitle;
    }

    m_trayIcon->showMessage(title, msg, icons()->applicationIcon(), msTimeoutHint);
}

void MainWindow::restartApp(const QString& message)
{
    auto ans = MessageBox::question(
        this, tr("Restart Application?"), message, MessageBox::Yes | MessageBox::No, MessageBox::Yes);
    if (ans == MessageBox::Yes) {
        m_appExitCalled = true;
        m_restartRequested = true;
        close();
    } else {
        m_restartRequested = false;
    }
}

void MainWindow::initViewMenu()
{
    m_ui->actionThemeAuto->setData("auto");
    m_ui->actionThemeLight->setData("light");
    m_ui->actionThemeDark->setData("dark");

    auto themeActions = new QActionGroup(this);
    themeActions->addAction(m_ui->actionThemeAuto);
    themeActions->addAction(m_ui->actionThemeLight);
    themeActions->addAction(m_ui->actionThemeDark);

    auto appTheme = config()->get(Config::GUI_ApplicationTheme).toString();
    for (auto action : themeActions->actions()) {
        if (action->data() == appTheme) {
            action->setChecked(true);
            break;
        }
    }

    connect(themeActions, &QActionGroup::triggered, this, [](QAction* action) {
        config()->set(Config::GUI_ApplicationTheme, action->data());
        kpxcApp->applyTheme();
    });

    bool compact = config()->get(Config::GUI_CompactMode).toBool();
    m_ui->actionCompactMode->setChecked(compact);
    connect(m_ui->actionCompactMode, &QAction::toggled, this, [this, compact](bool checked) {
        config()->set(Config::GUI_CompactMode, checked);
        if (checked != compact) {
            restartApp(tr("You must restart the application to apply this setting. Would you like to restart now?"));
        }
    });

#ifdef Q_OS_MACOS
    m_ui->actionShowMenubar->setVisible(false);
#else
    m_ui->actionShowMenubar->setChecked(!config()->get(Config::GUI_HideMenubar).toBool());
    connect(m_ui->actionShowMenubar, &QAction::toggled, this, [this](bool checked) {
        config()->set(Config::GUI_HideMenubar, !checked);
        applySettingsChanges();
    });
#endif

    m_ui->actionShowToolbar->setChecked(!config()->get(Config::GUI_HideToolbar).toBool());
    connect(m_ui->actionShowToolbar, &QAction::toggled, this, [this](bool checked) {
        config()->set(Config::GUI_HideToolbar, !checked);
        applySettingsChanges();
    });

    m_ui->actionShowGroupPanel->setChecked(!config()->get(Config::GUI_HideGroupPanel).toBool());
    connect(m_ui->actionShowGroupPanel, &QAction::toggled, this, [](bool checked) {
        config()->set(Config::GUI_HideGroupPanel, !checked);
    });

    m_ui->actionShowPreviewPanel->setChecked(!config()->get(Config::GUI_HidePreviewPanel).toBool());
    connect(m_ui->actionShowPreviewPanel, &QAction::toggled, this, [](bool checked) {
        config()->set(Config::GUI_HidePreviewPanel, !checked);
    });

    connect(m_ui->actionAlwaysOnTop, &QAction::toggled, this, [this](bool checked) {
        config()->set(Config::GUI_AlwaysOnTop, checked);
        if (checked) {
            setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
        } else {
            setWindowFlags(windowFlags() & ~Qt::WindowStaysOnTopHint);
        }
        show();
    });
    // Set checked after connecting to act on a toggle in state (default state is unchecked)
    m_ui->actionAlwaysOnTop->setChecked(config()->get(Config::GUI_AlwaysOnTop).toBool());

    m_ui->actionHideUsernames->setChecked(config()->get(Config::GUI_HideUsernames).toBool());
    connect(m_ui->actionHideUsernames, &QAction::toggled, this, [](bool checked) {
        config()->set(Config::GUI_HideUsernames, checked);
    });

    m_ui->actionHidePasswords->setChecked(config()->get(Config::GUI_HidePasswords).toBool());
    connect(m_ui->actionHidePasswords, &QAction::toggled, this, [](bool checked) {
        config()->set(Config::GUI_HidePasswords, checked);
    });
}

void MainWindow::initActionCollection()
{
    auto ac = ActionCollection::instance();
    ac->addActions({// Database Menu
                    m_ui->actionDatabaseNew,
                    m_ui->actionDatabaseOpen,
                    m_ui->actionDatabaseSave,
                    m_ui->actionDatabaseSaveAs,
                    m_ui->actionDatabaseSaveBackup,
                    m_ui->actionDatabaseClose,
                    m_ui->actionLockDatabase,
                    m_ui->actionLockAllDatabases,
                    m_ui->actionDatabaseSettings,
                    m_ui->actionDatabaseSecurity,
                    m_ui->actionReports,
                    m_ui->actionPasskeys,
                    m_ui->actionDatabaseMerge,
                    m_ui->actionImportPasskey,
                    m_ui->actionImportCsv,
                    m_ui->actionImportOpVault,
                    m_ui->actionImportKeePass1,
                    m_ui->actionExportCsv,
                    m_ui->actionExportHtml,
                    m_ui->actionExportXML,
                    m_ui->actionQuit,
                    // Entry Menu
                    m_ui->actionEntryNew,
                    m_ui->actionEntryEdit,
                    m_ui->actionEntryClone,
                    m_ui->actionEntryDelete,
                    m_ui->actionEntryCopyUsername,
                    m_ui->actionEntryCopyPassword,
                    m_ui->actionEntryCopyURL,
                    m_ui->actionEntryCopyTitle,
                    m_ui->actionEntryCopyNotes,
                    m_ui->actionEntryTotp,
                    m_ui->actionEntryTotpQRCode,
                    m_ui->actionEntrySetupTotp,
                    m_ui->actionEntryCopyTotp,
                    m_ui->actionEntryCopyPasswordTotp,
                    m_ui->actionEntryAutoTypeSequence,
                    m_ui->actionEntryAutoTypeUsername,
                    m_ui->actionEntryAutoTypeUsernameEnter,
                    m_ui->actionEntryAutoTypePassword,
                    m_ui->actionEntryAutoTypePasswordEnter,
                    m_ui->actionEntryAutoTypeTOTP,
                    m_ui->actionEntryDownloadIcon,
                    m_ui->actionEntryOpenUrl,
                    m_ui->actionEntryMoveUp,
                    m_ui->actionEntryMoveDown,
                    m_ui->actionEntryAddToAgent,
                    m_ui->actionEntryRemoveFromAgent,
                    m_ui->actionEntryRestore,
                    // Group Menu
                    m_ui->actionGroupNew,
                    m_ui->actionGroupEdit,
                    m_ui->actionGroupClone,
                    m_ui->actionGroupDelete,
                    m_ui->actionGroupDownloadFavicons,
                    m_ui->actionGroupSortAsc,
                    m_ui->actionGroupSortDesc,
                    m_ui->actionGroupEmptyRecycleBin,
                    // Tools Menu
                    m_ui->actionPasswordGenerator,
                    m_ui->actionClearSSHAgent,
                    m_ui->actionSettings,
                    // View Menu
                    m_ui->actionThemeAuto,
                    m_ui->actionThemeLight,
                    m_ui->actionThemeDark,
                    m_ui->actionCompactMode,
#ifndef Q_OS_MACOS
                    m_ui->actionShowMenubar,
#endif
                    m_ui->actionShowToolbar,
                    m_ui->actionShowGroupPanel,
                    m_ui->actionShowPreviewPanel,
                    m_ui->actionAllowScreenCapture,
                    m_ui->actionAlwaysOnTop,
                    m_ui->actionHideUsernames,
                    m_ui->actionHidePasswords,
                    // Help Menu
                    m_ui->actionGettingStarted,
                    m_ui->actionUserGuide,
                    m_ui->actionKeyboardShortcuts,
                    m_ui->actionOnlineHelp,
                    m_ui->actionCheckForUpdates,
                    m_ui->actionDonate,
                    m_ui->actionBugReport,
                    m_ui->actionAbout});

    // Register as default any shortcuts that were set in the .ui file
    for (const auto action : ac->actions()) {
        if (!action->shortcut().isEmpty()) {
            ac->setDefaultShortcut(action, action->shortcut());
        }
    }

    // Actions with standard shortcuts (if no standard shortcut exists, leave the existing
    // shortcuts from the .ui file in place)
    ac->setDefaultShortcut(m_ui->actionDatabaseOpen, QKeySequence::Open);
    ac->setDefaultShortcut(m_ui->actionDatabaseSave, QKeySequence::Save);
    ac->setDefaultShortcut(m_ui->actionDatabaseSaveAs, QKeySequence::SaveAs);
    ac->setDefaultShortcut(m_ui->actionDatabaseClose, QKeySequence::Close);
    ac->setDefaultShortcut(m_ui->actionSettings, QKeySequence::Preferences);
    ac->setDefaultShortcut(m_ui->actionQuit, QKeySequence::Quit);
    ac->setDefaultShortcut(m_ui->actionEntryNew, QKeySequence::New);

    // Prevent conflicts with global Mac shortcuts (force Control on all platforms)
    // Note: Qt::META means Ctrl on Mac.
#ifdef Q_OS_MAC
    ac->setDefaultShortcut(m_ui->actionEntryAddToAgent, Qt::META | Qt::Key_H);
    ac->setDefaultShortcut(m_ui->actionEntryRemoveFromAgent, Qt::META | Qt::SHIFT | Qt::Key_H);
#endif

    QTimer::singleShot(1, ac, &ActionCollection::restoreShortcuts);
}

MainWindowEventFilter::MainWindowEventFilter(QObject* parent)
    : QObject(parent)
{
    m_altCoolDown.setInterval(250);
    m_altCoolDown.setSingleShot(true);

    m_menubarTimer.setInterval(250);
    m_menubarTimer.setSingleShot(false);
    connect(&m_menubarTimer, &QTimer::timeout, this, [this] {
        auto mainwindow = getMainWindow();
        if (mainwindow && mainwindow->m_ui->menubar->maximumHeight() > 0
            && config()->get(Config::GUI_HideMenubar).toBool()) {
            // If the menu bar is visible with no active menu, hide it
            if (!mainwindow->m_ui->menubar->activeAction()) {
                mainwindow->m_ui->menubar->setMaximumHeight(0);
                m_altCoolDown.start();
                m_menubarTimer.stop();
            }
            // Conditions to hide the menubar or stop the timer have not been met
            return;
        }
        // We no longer need the timer
        m_menubarTimer.stop();
    });
}

/**
 * MainWindow event filter to initiate empty-area drag on the toolbar, menubar, and tabbar.
 * Also shows menubar with Alt when menubar itself is hidden.
 */
bool MainWindowEventFilter::eventFilter(QObject* watched, QEvent* event)
{
    auto* mainWindow = getMainWindow();
    if (!mainWindow || !mainWindow->m_ui) {
        return QObject::eventFilter(watched, event);
    }

    auto eventType = event->type();
    if (eventType == QEvent::MouseButtonPress) {
        auto mouseEvent = dynamic_cast<QMouseEvent*>(event);
        if (watched == mainWindow->m_ui->menubar) {
            if (!mainWindow->m_ui->menubar->actionAt(mouseEvent->pos())) {
                mainWindow->windowHandle()->startSystemMove();
                return false;
            }
        } else if (watched == mainWindow->m_ui->toolBar) {
            if (!mainWindow->m_ui->toolBar->isMovable() || mainWindow->m_ui->toolBar->cursor() != Qt::SizeAllCursor) {
                mainWindow->windowHandle()->startSystemMove();
                return false;
            }
        } else if (watched == mainWindow->m_ui->tabWidget->tabBar()) {
            if (mainWindow->m_ui->tabWidget->tabBar()->tabAt(mouseEvent->pos()) == -1) {
                mainWindow->windowHandle()->startSystemMove();
                return true;
            }
        }
    } else if (eventType == QEvent::KeyRelease && watched == mainWindow) {
#ifdef Q_OS_MACOS
        // On macOS, the menubar is always visible, so no need to toggle it
        return false;
#endif
        auto keyEvent = dynamic_cast<QKeyEvent*>(event);
#ifdef Q_OS_WIN
        // Windows translates AltGr into CTRL + ALT, this breaks using AltGr when the menubar is hidden
        // Prevent this by activating the ALT cooldown to ignore the next key event which will be an ALT key
        if (keyEvent->key() == Qt::Key_Control && keyEvent->modifiers() == Qt::AltModifier
            && config()->get(Config::GUI_HideMenubar).toBool()) {
            m_altCoolDown.start();
            return false;
        }
#endif
        if (keyEvent->key() == Qt::Key_Alt && !keyEvent->modifiers() && config()->get(Config::GUI_HideMenubar).toBool()
            && !m_altCoolDown.isActive()) {
            auto menubar = mainWindow->m_ui->menubar;
            menubar->setMaximumHeight(menubar->maximumHeight() > 0 ? 0 : QWIDGETSIZE_MAX);
            if (menubar->maximumHeight() > 0) {
                QTimer::singleShot(0, [menubar, mainWindow] {
                    // Run this with a singleshot timer so it's after menubar->setMaximumHeight() has taken effect,
                    // otherwise it won't be selected and menubarTimer will hide the menubar instantly
                    menubar->setActiveAction(mainWindow->m_ui->menuFile->menuAction());
                });
                m_menubarTimer.start();
            } else {
                m_menubarTimer.stop();
            }
            return true;
        }
    }

    return QObject::eventFilter(watched, event);
}
