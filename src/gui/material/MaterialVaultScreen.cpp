/*
 *  Copyright (C) 2026 KeePassXC Team <team@keepassxc.org>
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

#include "MaterialVaultScreen.h"

#include "MaterialButtons.h"
#include "MaterialEntryDelegate.h"
#include "MaterialEntryDetail.h"
#include "MaterialGroupDelegate.h"
#include "MaterialIcons.h"
#include "MaterialNotifier.h"
#include "MaterialOverlay.h"
#include "MaterialRegexSafety.h"
#include "MaterialSearchBar.h"
#include "MaterialSegmentedButton.h"
#include "MaterialTheme.h"
#include "MaterialVaultSidebar.h"

#include "core/Clock.h"
#include "core/Database.h"
#include "core/Entry.h"
#include "core/EntryAttachments.h"
#include "core/Group.h"
#include "core/PasswordHealth.h"
#include "core/Totp.h"
#include "gui/Clipboard.h"
#include "gui/DatabaseTabWidget.h"
#include "gui/DatabaseWidget.h"
#include "gui/MainWindow.h"
#include "gui/MessageWidget.h"
#include "gui/entry/EditEntryWidget.h"
#include "gui/entry/EntryModel.h"
#include "gui/entry/EntryView.h"
#include "gui/group/GroupModel.h"
#include "gui/group/GroupView.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QPainter>
#include <QPersistentModelIndex>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QScopedValueRollback>
#include <QStackedWidget>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidgetAction>

#include <functional>

namespace Material
{
    namespace
    {
        constexpr int HeaderLeftPadding = 16;
        constexpr int HeaderTopPadding = 14;
        constexpr int HeaderBottomPadding = 10;
        constexpr int HeaderSpacing = 12;
        constexpr int ListPadding = 12;
        constexpr int ListBottomPadding = 96;
        constexpr int FabMargin = 24;
        constexpr int EmptyGlyphSize = 44;
        constexpr int EmptyGlyphGap = 10;
        constexpr int EmptyTopPadding = 56;
        /** Design size of the empty-state message; the type scale has no 16px role. */
        constexpr int EmptyLineSize = 16;
        constexpr float EmptyGlyphOpacity = 0.5f;
        /** Below this the row sheds its url, health and modified columns. */
        constexpr int CompactListWidth = 620;
        /** How often the detail pane's one-time password is checked for a roll-over. */
        constexpr int TotpWatchInterval = 1000;

        /** A type role rescaled to another design size; the scale is anchored at 14px. */
        QFont scaledFont(TypeRole role, int designPx)
        {
            QFont font = theme()->font(role);
            if (font.pointSizeF() > 0.0) {
                font.setPointSizeF(font.pointSizeF() * designPx / 14.0);
            } else {
                font.setPixelSize(qMax(1, qRound(font.pixelSize() * designPx / 14.0)));
            }
            return font;
        }

        /**
         * How many entries of a database share each password.
         *
         * Re-use is a property of the whole database, so it cannot be answered
         * from one entry. This is the pass HealthChecker makes for the reports,
         * kept here because the list needs a verdict for every row it paints;
         * recycled entries and references are left out, exactly as there.
         */
        QHash<QString, int> buildReuseIndex(const QSharedPointer<Database>& db)
        {
            QHash<QString, int> reuse;
            if (!db || !db->rootGroup()) {
                return reuse;
            }
            for (const Entry* entry : db->rootGroup()->entriesRecursive()) {
                if (entry->isRecycled() || entry->isAttributeReference(QStringLiteral("Password"))) {
                    continue;
                }
                ++reuse[entry->password()];
            }
            return reuse;
        }

        /**
         * A short relative age: the design's "2 h ago" / "6 d ago" / "3 mo ago"
         * rather than an absolute timestamp, which never fits the 80px column.
         */
        QString relativeAge(const QDateTime& when)
        {
            if (!when.isValid()) {
                return {};
            }
            const qint64 seconds = when.secsTo(QDateTime::currentDateTimeUtc());
            if (seconds < 60) {
                return VaultScreen::tr("now", "age of an entry that was just modified");
            }
            const qint64 minutes = seconds / 60;
            if (minutes < 60) {
                return VaultScreen::tr("%1 min ago").arg(minutes);
            }
            const qint64 hours = minutes / 60;
            if (hours < 24) {
                return VaultScreen::tr("%1 h ago").arg(hours);
            }
            const qint64 days = hours / 24;
            if (days < 7) {
                return VaultScreen::tr("%1 d ago").arg(days);
            }
            if (days < 30) {
                return VaultScreen::tr("%1 w ago").arg(days / 7);
            }
            if (days < 365) {
                return VaultScreen::tr("%1 mo ago").arg(days / 30);
            }
            return VaultScreen::tr("%1 y ago").arg(days / 365);
        }

        /** Worst first, so the health sort puts what needs attention on top. */
        int healthRank(Health health)
        {
            switch (health) {
            case Health::Breached:
                return 0;
            case Health::Weak:
                return 1;
            case Health::Reused:
                return 2;
            case Health::Ok:
                return 3;
            case Health::Unknown:
                break;
            }
            return 4;
        }

        /** The tag that marks an entry as a favourite, as the importers write it. */
        const QString& favouriteTag()
        {
            static const QString tag = QStringLiteral("Favorite");
            return tag;
        }

        /**
         * The one-time password step the current moment falls in.
         *
         * Steps are aligned to the epoch, so the code changes exactly when this
         * number does. It is what the pane draws its countdown ring from, and
         * what keeps the digits beside the ring in step with it here.
         */
        qint64 totpStep(int period)
        {
            const int step = period > 0 ? period : static_cast<int>(Totp::DEFAULT_STEP);
            return QDateTime::currentSecsSinceEpoch() / step;
        }

        QString formatSize(qint64 bytes)
        {
            static const char* units[] = {"B", "KiB", "MiB", "GiB"};
            double value = static_cast<double>(bytes);
            int unit = 0;
            while (value >= 1024.0 && unit < 3) {
                value /= 1024.0;
                ++unit;
            }
            return QStringLiteral("%1 %2").arg(QString::number(value, 'f', unit == 0 ? 0 : 1),
                                               QLatin1String(units[unit]));
        }

        /**
         * One search term that is matched as a raw regular expression.
         *
         * The search parser already understands this: the `*` modifier turns
         * wildcard conversion off, and the quotes keep a pattern containing
         * spaces in one piece.
         */
        QString regexTerm(const QString& pattern)
        {
            QString escaped = pattern;
            escaped.replace(QLatin1String("\""), QLatin1String("\\\""));
            return QStringLiteral("*\"%1\"").arg(escaped);
        }

        /** Say what a message banner would have said, as a toast. */
        void announce(const QString& text, KMessageWidget::MessageType type)
        {
            switch (type) {
            case KMessageWidget::Positive:
                Notify::success(text);
                return;
            case KMessageWidget::Warning:
                Notify::warning(text);
                return;
            case KMessageWidget::Error:
                Notify::error(text);
                return;
            case KMessageWidget::Information:
                break;
            }
            Notify::info(text);
        }

        /**
         * Open the selected entry in the stock editor on a given tab.
         *
         * switchToEntryEdit() is the public entry point and it makes the editor
         * the database widget's current page, so the widget it just raised is
         * the one to tell which tab to show. Nothing private is reached into.
         */
        void openEntryEditor(DatabaseWidget* dbWidget, EditEntryWidget::Page page)
        {
            if (!dbWidget || !dbWidget->currentSelectedEntry()) {
                return;
            }
            dbWidget->switchToEntryEdit();
            if (auto* editor = qobject_cast<EditEntryWidget*>(dbWidget->currentWidget())) {
                editor->switchToPage(page);
            }
        }

        /**
         * The entry list.
         *
         * Only exists so the design's padding can be put inside the scroll area
         * rather than around it - QAbstractScrollArea::setViewportMargins is
         * protected - which keeps the scroll bar on the outer edge and leaves
         * room under the last row for the floating action button.
         */
        class EntryListView : public QListView
        {
        public:
            explicit EntryListView(QWidget* parent = nullptr)
                : QListView(parent)
            {
            }

            void setPadding(int left, int top, int right, int bottom)
            {
                setViewportMargins(left, top, right, bottom);
            }
        };
    } // namespace

    // -------------------------------------------------------------- EntryListModel

    EntryListModel::EntryListModel(QObject* parent)
        : QSortFilterProxyModel(parent)
    {
        setDynamicSortFilter(true);
        sort(0, Qt::AscendingOrder);
    }

    EntryListModel::~EntryListModel() = default;

    EntryListModel::SortKey EntryListModel::sortKey() const
    {
        return m_sortKey;
    }

    void EntryListModel::setSortKey(SortKey key)
    {
        if (key == m_sortKey) {
            return;
        }
        m_sortKey = key;
        invalidate();
        sort(0, Qt::AscendingOrder);
    }

    void EntryListModel::setDatabase(const QSharedPointer<Database>& db)
    {
        m_database = db;
        invalidateHealth();
    }

    void EntryListModel::invalidateHealth()
    {
        m_reuseDirty = true;
    }

    Health EntryListModel::healthOf(Entry* entry) const
    {
        if (!entry || entry->excludeFromReports() || entry->password().isEmpty()) {
            return Health::Unknown;
        }

        // Worst verdict wins, in the order the design's health sort ranks them:
        // a critically bad password is reported as Breached the way the reports
        // feed already reports it, and re-use only surfaces once the password is
        // otherwise strong enough not to be called Weak.
        switch (entry->passwordHealth()->quality()) {
        case PasswordHealth::Quality::Bad:
            return Health::Breached;
        case PasswordHealth::Quality::Poor:
        case PasswordHealth::Quality::Weak:
            return Health::Weak;
        case PasswordHealth::Quality::Good:
        case PasswordHealth::Quality::Excellent:
            break;
        }

        if (m_reuseDirty) {
            m_reuse = buildReuseIndex(m_database);
            m_reuseDirty = false;
        }
        return m_reuse.value(entry->password()) > 1 ? Health::Reused : Health::Ok;
    }

    EntryModel* EntryListModel::entryModel() const
    {
        return qobject_cast<EntryModel*>(sourceModel());
    }

    Entry* EntryListModel::entryFromIndex(const QModelIndex& index) const
    {
        auto* model = entryModel();
        if (!model || !index.isValid()) {
            return nullptr;
        }
        const QModelIndex source = mapToSource(index);
        if (!source.isValid() || source.row() >= model->rowCount()) {
            return nullptr;
        }
        return model->entryFromIndex(source);
    }

    QModelIndex EntryListModel::indexFromEntry(Entry* entry) const
    {
        auto* model = entryModel();
        if (!model || !entry) {
            return {};
        }
        return mapFromSource(model->indexFromEntry(entry));
    }

    bool EntryListModel::filterAcceptsColumn(int sourceColumn, const QModelIndex& sourceParent) const
    {
        Q_UNUSED(sourceParent);
        // The row is one item; every other column is read through the roles below.
        return sourceColumn == EntryModel::Title;
    }

    QVariant EntryListModel::data(const QModelIndex& index, int role) const
    {
        auto* model = entryModel();
        if (!model || !index.isValid()) {
            return QSortFilterProxyModel::data(index, role);
        }

        const QModelIndex source = mapToSource(index);
        if (!source.isValid()) {
            return QSortFilterProxyModel::data(index, role);
        }

        auto column = [&source](EntryModel::ModelColumn column, int columnRole) {
            return source.sibling(source.row(), column).data(columnRole);
        };

        switch (role) {
        case EntryDelegate::TitleRole:
            return column(EntryModel::Title, Qt::DisplayRole);
        case EntryDelegate::UsernameRole:
            return column(EntryModel::Username, Qt::DisplayRole);
        case EntryDelegate::UrlRole:
            return column(EntryModel::Url, Qt::DisplayRole);
        case EntryDelegate::ModifiedRole: {
            // The design's column is 80px of relative age; the absolute time the
            // Modified column carries is kept as the row's tooltip below.
            Entry* entry = model->entryFromIndex(source);
            return entry ? relativeAge(entry->timeInfo().lastModificationTime().toLocalTime()) : QString();
        }
        case Qt::ToolTipRole:
            return tr("Modified %1").arg(column(EntryModel::Modified, Qt::DisplayRole).toString());
        case EntryDelegate::TotpRole:
            return column(EntryModel::Totp, Qt::UserRole);
        case EntryDelegate::HealthRole:
            return QVariant::fromValue(healthOf(model->entryFromIndex(source)));
        case EntryDelegate::SymbolRole:
            // The same helper the detail pane asks, so the row avatar and the
            // pane tile are one glyph. It answers a name for every entry, so
            // the delegate's decoration fallback is never reached here - a
            // custom icon is a picture the pane cannot name, and both surfaces
            // would rather show the neutral glyph than disagree.
            return Icons::entrySymbol(model->entryFromIndex(source));
        default:
            break;
        }

        return QSortFilterProxyModel::data(index, role);
    }

    bool EntryListModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
    {
        auto* model = entryModel();
        if (!model || !left.isValid() || !right.isValid()) {
            return QSortFilterProxyModel::lessThan(left, right);
        }

        Entry* leftEntry = model->entryFromIndex(left);
        Entry* rightEntry = model->entryFromIndex(right);
        if (!leftEntry || !rightEntry) {
            return QSortFilterProxyModel::lessThan(left, right);
        }

        const QString leftTitle = left.data(Qt::DisplayRole).toString();
        const QString rightTitle = right.data(Qt::DisplayRole).toString();

        switch (m_sortKey) {
        case SortKey::Modified: {
            const QDateTime leftTime = leftEntry->timeInfo().lastModificationTime();
            const QDateTime rightTime = rightEntry->timeInfo().lastModificationTime();
            if (leftTime != rightTime) {
                // Newest first: the most recently touched entry is the one wanted.
                return leftTime > rightTime;
            }
            break;
        }
        case SortKey::Health: {
            const int leftRank = healthRank(healthOf(leftEntry));
            const int rightRank = healthRank(healthOf(rightEntry));
            if (leftRank != rightRank) {
                return leftRank < rightRank;
            }
            break;
        }
        case SortKey::Title:
            break;
        }

        return QString::localeAwareCompare(leftTitle, rightTitle) < 0;
    }

    // -------------------------------------------------------------- GroupTreeModel

    GroupTreeModel::GroupTreeModel(QObject* parent)
        : QIdentityProxyModel(parent)
    {
    }

    GroupTreeModel::~GroupTreeModel() = default;

    Group* GroupTreeModel::groupFromIndex(const QModelIndex& index) const
    {
        auto* model = qobject_cast<GroupModel*>(sourceModel());
        if (!model || !index.isValid()) {
            return nullptr;
        }
        return model->groupFromIndex(mapToSource(index));
    }

    QModelIndex GroupTreeModel::indexFromGroup(Group* group) const
    {
        auto* model = qobject_cast<GroupModel*>(sourceModel());
        if (!model || !group) {
            return {};
        }
        return mapFromSource(model->index(group));
    }

    QVariant GroupTreeModel::data(const QModelIndex& index, int role) const
    {
        if (role == GroupDelegate::CountRole) {
            Group* group = groupFromIndex(index);
            return group ? group->entries().size() : -1;
        }
        return QIdentityProxyModel::data(index, role);
    }

    // ----------------------------------------------------------------- VaultScreen

    VaultScreen::VaultScreen(QWidget* parent)
        : QWidget(parent)
    {
        setObjectName(QStringLiteral("materialVaultScreen"));

        m_entryModel = new EntryListModel(this);
        m_groupModel = new GroupTreeModel(this);

        // The pane counts its ring down from the wall clock but is only ever
        // handed one code, so the step boundary is watched from here and a fresh
        // code pushed across it. A second is enough: the check is a division.
        m_totpTimer = new QTimer(this);
        m_totpTimer->setInterval(TotpWatchInterval);
        m_totpTimer->setTimerType(Qt::CoarseTimer);
        connect(m_totpTimer, &QTimer::timeout, this, &VaultScreen::refreshTotp);

        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(0);

        m_stack = new QStackedWidget(this);
        root->addWidget(m_stack, 1);

        m_panes = buildPanes();
        m_stack->addWidget(m_panes);
        applyBreakpoint();

        connect(theme(), &Theme::changed, this, &VaultScreen::applyTheme);
        applyTheme();
        updateResultLine();
    }

    VaultScreen::~VaultScreen() = default;

    QWidget* VaultScreen::buildPanes()
    {
        auto* panes = new QWidget(m_stack);
        auto* layout = new QHBoxLayout(panes);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        m_sidebar = new VaultSidebar(panes);
        m_sidebar->setGroupModel(m_groupModel);
        layout->addWidget(m_sidebar, 0);

        m_centre = buildCentreColumn();
        m_centre->setParent(panes);
        // The FAB is placed by hand over the centre column, and the row shape
        // sheds columns when the column narrows; both need its resizes.
        m_centre->installEventFilter(this);
        layout->addWidget(m_centre, 1);

        m_detail = new EntryDetail(panes);
        layout->addWidget(m_detail, 0);

        auto* tree = m_sidebar->groupView();
        tree->setContextMenuPolicy(Qt::CustomContextMenu);
        tree->setDragEnabled(false);
        tree->viewport()->setAcceptDrops(true);
        tree->setDropIndicatorShown(true);
        tree->setDefaultDropAction(Qt::MoveAction);
        connect(tree, &QWidget::customContextMenuRequested, this, [this, tree](const QPoint& pos) {
            const QModelIndex index = tree->indexAt(pos);
            if (index.isValid()) {
                tree->selectionModel()->setCurrentIndex(
                    index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
            }
            if (auto* window = getMainWindow()) {
                QMetaObject::invokeMethod(window,
                                          "showGroupContextMenu",
                                          Qt::QueuedConnection,
                                          Q_ARG(QPoint, tree->viewport()->mapToGlobal(pos)));
            }
        });

        connect(m_sidebar, &VaultSidebar::groupSelected, this, [this](const QModelIndex& index) {
            if (m_syncingGroup || !m_dbWidget) {
                return;
            }
            Group* group = m_groupModel->groupFromIndex(index);
            if (!group) {
                return;
            }
            QScopedValueRollback<bool> guard(m_syncingGroup, true);
            m_dbWidget->groupView()->setCurrentGroup(group);
        });
        connect(m_sidebar, &VaultSidebar::tagsChanged, this, [this] { runSearch(); });
        connect(m_sidebar, &VaultSidebar::externalEditorRequested, this, [this] {
            if (!m_dbWidget || !m_dbWidget->database()) {
                return;
            }
            const QString path = m_dbWidget->database()->filePath();
            if (path.isEmpty()) {
                return;
            }
            QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
        });

        connectDetailActions(m_detail);

        m_detailOverlay = new Overlay(this);
        m_detailOverlay->setObjectName(QStringLiteral("materialVaultDetailOverlay"));
        m_detailOverlay->setSheetWidth(520);
        auto* sheet = new QWidget;
        sheet->setObjectName(QStringLiteral("materialVaultDetailSheet"));
        sheet->setAccessibleName(tr("Entry details"));
        auto* sheetLayout = new QVBoxLayout(sheet);
        sheetLayout->setContentsMargins(12, 12, 12, 12);
        auto* close = new QToolButton(sheet);
        close->setText(tr("Close entry details"));
        close->setAccessibleName(close->text());
        connect(close, &QToolButton::clicked, m_detailOverlay, &Overlay::closeOverlay);
        sheetLayout->addWidget(close, 0, Qt::AlignRight);
        m_sheetDetail = new EntryDetail(sheet);
        m_sheetDetail->setMinimumWidth(0);
        m_sheetDetail->setMaximumWidth(QWIDGETSIZE_MAX);
        connectDetailActions(m_sheetDetail);
        sheetLayout->addWidget(m_sheetDetail, 1);
        m_detailOverlay->setSheetWidget(sheet);
        connect(m_detailOverlay, &Overlay::closed, this, [this] {
            if (m_detailSheetButton && m_detailSheetButton->isVisible()) {
                m_detailSheetButton->setFocus(Qt::PopupFocusReason);
            }
        });

        return panes;
    }

    void VaultScreen::connectDetailActions(EntryDetail* detail)
    {
        connect(detail, &EntryDetail::copyRequested, this, &VaultScreen::copyField);
        connect(detail, &EntryDetail::autoTypeRequested, this, [this] {
            if (m_dbWidget) {
                m_dbWidget->performAutoType();
            }
        });
        connect(detail, &EntryDetail::editRequested, this, [this] {
            if (m_dbWidget) {
                m_dbWidget->switchToEntryEdit();
            }
        });
        connect(detail, &EntryDetail::deleteRequested, this, [this] {
            if (m_dbWidget) {
                m_dbWidget->deleteSelectedEntries();
            }
        });
        connect(detail, &EntryDetail::historyRequested, this, [this] {
            openEntryEditor(m_dbWidget, EditEntryWidget::Page::History);
        });
        connect(detail, &EntryDetail::attachmentActivated, this, [this](const QString&) {
            openEntryEditor(m_dbWidget, EditEntryWidget::Page::Advanced);
        });
        connect(detail, &EntryDetail::favouriteToggled, this, &VaultScreen::toggleFavourite);
    }

    QWidget* VaultScreen::buildCentreColumn()
    {
        auto* centre = new QWidget;
        auto* layout = new QVBoxLayout(centre);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        auto* header = new QWidget(centre);
        auto* headerLayout = new QVBoxLayout(header);
        headerLayout->setContentsMargins(
            HeaderLeftPadding, HeaderTopPadding, HeaderLeftPadding, HeaderBottomPadding);
        headerLayout->setSpacing(HeaderSpacing);

        m_searchBar = new SearchBar(SearchBar::Variant::Prominent, header);
        m_searchBar->setPlaceholder(tr("Search entries — title, username, URL, notes"));
        m_searchBar->setIdentity(QStringLiteral("vault.entries"), tr("Vault entry search"));
        headerLayout->addWidget(m_searchBar);

        auto* summaryRow = new QWidget(header);
        auto* summaryLayout = new QHBoxLayout(summaryRow);
        summaryLayout->setContentsMargins(0, 0, 0, 0);
        summaryLayout->setSpacing(8);

        m_resultLabel = new QLabel(summaryRow);
        summaryLayout->addWidget(m_resultLabel, 1);

        m_groupScopeButton = new QToolButton(summaryRow);
        m_groupScopeButton->setObjectName(QStringLiteral("materialVaultGroupScope"));
        m_groupScopeButton->setText(tr("Groups"));
        m_groupScopeButton->setAccessibleName(tr("Choose a vault group"));
        m_groupScopeButton->setPopupMode(QToolButton::InstantPopup);
        m_groupScopeMenu = new QMenu(m_groupScopeButton);
        m_groupScopeMenu->setAccessibleName(tr("Vault groups"));
        m_groupScopeButton->setMenu(m_groupScopeMenu);
        m_groupScopeSearch = new SearchBar(SearchBar::Variant::Prominent, m_groupScopeMenu);
        m_groupScopeSearch->setObjectName(QStringLiteral("materialVaultGroupScopeSearch"));
        m_groupScopeSearch->setPlaceholder(tr("Search groups"));
        m_groupScopeSearch->setIdentity(QStringLiteral("vault.group-scope"), tr("Vault group scope search"));
        m_groupScopeSearch->lineEdit()->setAccessibleName(tr("Search vault groups"));
        connect(m_groupScopeSearch, &SearchBar::textChanged, this, &VaultScreen::filterGroupScopeMenu);
        connect(m_groupScopeSearch, &SearchBar::regexToggled, this, [this] {
            filterGroupScopeMenu(m_groupScopeSearch->text());
        });
        m_groupScopeSearchAction = new QWidgetAction(m_groupScopeMenu);
        m_groupScopeSearchAction->setDefaultWidget(m_groupScopeSearch);
        m_groupScopeMenu->addAction(m_groupScopeSearchAction);
        m_groupScopeMenu->addSeparator();
        connect(m_groupScopeMenu, &QMenu::aboutToShow, this, &VaultScreen::rebuildGroupScopeMenu);
        summaryLayout->addWidget(m_groupScopeButton, 0);

        m_detailSheetButton = new QToolButton(summaryRow);
        m_detailSheetButton->setObjectName(QStringLiteral("materialVaultDetailSheetButton"));
        m_detailSheetButton->setText(tr("Details"));
        m_detailSheetButton->setAccessibleName(tr("Open entry details"));
        connect(m_detailSheetButton, &QToolButton::clicked, this, &VaultScreen::openDetailSheet);
        summaryLayout->addWidget(m_detailSheetButton, 0);

        m_sortControl = new SegmentedButton(summaryRow);
        // The result row's sort strip is a chip-height control, not a button.
        m_sortControl->setFixedHeight(Layout::ChipHeight);
        m_sortControl->addSegment(QStringLiteral("title"), tr("Title"));
        m_sortControl->addSegment(QStringLiteral("modified"), tr("Modified"));
        m_sortControl->addSegment(QStringLiteral("health"), tr("Health"));
        summaryLayout->addWidget(m_sortControl, 0);
        headerLayout->addWidget(summaryRow);

        layout->addWidget(header, 0);

        m_listStack = new QStackedWidget(centre);
        m_listStack->setContentsMargins(0, 0, 0, 0);

        m_entryDelegate = new EntryDelegate(this);

        auto* entryList = new EntryListView(m_listStack);
        m_entryList = entryList;
        m_entryList->setModel(m_entryModel);
        m_entryList->setItemDelegate(m_entryDelegate);
        m_entryList->setFrameShape(QFrame::NoFrame);
        m_entryList->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_entryList->setSelectionMode(QAbstractItemView::ExtendedSelection);
        m_entryList->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_entryList->setUniformItemSizes(true);
        m_entryList->setResizeMode(QListView::Adjust);
        m_entryList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_entryList->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        m_entryList->setContextMenuPolicy(Qt::CustomContextMenu);
        m_entryList->setDragEnabled(true);
        m_entryList->setDragDropMode(QAbstractItemView::DragOnly);
        m_entryList->setDefaultDropAction(Qt::MoveAction);
        m_entryList->setMouseTracking(true);
        m_entryList->viewport()->setAttribute(Qt::WA_Hover, true);
        m_entryList->viewport()->setAutoFillBackground(false);
        // The bottom padding is what keeps the last row clear of the FAB.
        entryList->setPadding(ListPadding, 0, ListPadding, ListBottomPadding);
        // The delegate owns the whole row, so the application sheet's item
        // padding and its square selection fill are switched off here.
        m_entryList->setStyleSheet(
            QStringLiteral("QListView { background: transparent; border: none; }"
                           "QListView::item { background: transparent; border: none; padding: 0; margin: 0; }"
                           "QListView::item:hover, QListView::item:selected { background: transparent; }"));
        m_listStack->addWidget(m_entryList);

        m_emptyState = new QWidget(m_listStack);
        auto* emptyLayout = new QVBoxLayout(m_emptyState);
        emptyLayout->setContentsMargins(20, EmptyTopPadding, 20, 20);
        emptyLayout->setSpacing(EmptyGlyphGap);
        // The block is anchored by its 56px top padding, not centred in the pane.
        m_emptyGlyph = new QLabel(m_emptyState);
        m_emptyGlyph->setAlignment(Qt::AlignCenter);
        emptyLayout->addWidget(m_emptyGlyph, 0);
        m_emptyLabel = new QLabel(m_emptyState);
        m_emptyLabel->setAlignment(Qt::AlignCenter);
        m_emptyLabel->setWordWrap(true);
        emptyLayout->addWidget(m_emptyLabel, 0);
        emptyLayout->addStretch(1);
        m_listStack->addWidget(m_emptyState);

        layout->addWidget(m_listStack, 1);

        m_fab = new ExtendedFab(QStringLiteral("add"), tr("New entry"), centre);
        m_fab->raise();

        connect(m_searchBar, &SearchBar::textChanged, this, [this] { runSearch(); });
        connect(m_searchBar, &SearchBar::regexToggled, this, [this] { runSearch(); });
        connect(m_searchBar, &SearchBar::returnPressed, this, [this] { runSearch(); });
        connect(m_sortControl, &SegmentedButton::segmentSelected, this, [this](const QString& id) {
            if (id == QLatin1String("modified")) {
                m_entryModel->setSortKey(EntryListModel::SortKey::Modified);
            } else if (id == QLatin1String("health")) {
                m_entryModel->setSortKey(EntryListModel::SortKey::Health);
            } else {
                m_entryModel->setSortKey(EntryListModel::SortKey::Title);
            }
        });

        connect(m_fab, &QAbstractButton::clicked, this, [this] {
            if (m_dbWidget) {
                m_dbWidget->createEntry();
            }
        });

        connect(m_entryDelegate, &EntryDelegate::menuRequested, this, &VaultScreen::showEntryMenu);
        connect(m_entryDelegate, &EntryDelegate::totpRequested, this, [this](const QModelIndex& index) {
            if (!m_dbWidget) {
                return;
            }
            m_entryList->selectionModel()->setCurrentIndex(
                index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
            m_dbWidget->copyTotp();
        });
        connect(m_entryList, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
            showEntryMenu(m_entryList->indexAt(pos), m_entryList->viewport()->mapToGlobal(pos));
        });
        connect(m_entryList, &QAbstractItemView::doubleClicked, this, [this] {
            if (m_dbWidget && m_dbWidget->currentSelectedEntry()) {
                m_dbWidget->switchToEntryEdit();
            }
        });

        connect(m_entryModel, &QAbstractItemModel::modelReset, this, &VaultScreen::updateResultLine);
        connect(m_entryModel, &QAbstractItemModel::rowsInserted, this, &VaultScreen::updateResultLine);
        connect(m_entryModel, &QAbstractItemModel::rowsRemoved, this, &VaultScreen::updateResultLine);
        connect(m_entryModel, &QAbstractItemModel::layoutChanged, this, &VaultScreen::updateResultLine);

        // The view owns its selection model for good: the proxy above never
        // changes, only its source does, so this is connected once.
        connect(m_entryList->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this] {
            syncSelectionToDatabase();
        });
        connect(m_entryList->selectionModel(), &QItemSelectionModel::currentChanged, this, [this] {
            syncSelectionToDatabase();
        });

        return centre;
    }

    VaultSidebar* VaultScreen::sidebar() const
    {
        return m_sidebar;
    }

    EntryDetail* VaultScreen::detail() const
    {
        return m_detail;
    }

    SearchBar* VaultScreen::searchBar() const
    {
        return m_searchBar;
    }

    DatabaseWidget* VaultScreen::databaseWidget() const
    {
        return m_dbWidget;
    }

    Breakpoint VaultScreen::breakpoint() const
    {
        return m_breakpoint;
    }

    bool VaultScreen::groupPaneVisible() const
    {
        return m_sidebar && m_sidebar->isVisible();
    }

    bool VaultScreen::detailPaneInline() const
    {
        return m_detail && m_detail->isVisible();
    }

    QToolButton* VaultScreen::groupScopeButton() const
    {
        return m_groupScopeButton;
    }

    QToolButton* VaultScreen::detailSheetButton() const
    {
        return m_detailSheetButton;
    }

    void VaultScreen::setBreakpoint(Breakpoint breakpoint)
    {
        if (m_breakpoint == breakpoint) {
            return;
        }
        m_breakpoint = breakpoint;
        applyBreakpoint();
    }

    void VaultScreen::applyBreakpoint()
    {
        const bool showGroups = hasGroupPane(m_breakpoint);
        const bool inlineDetail = hasInlineDetail(m_breakpoint);
        m_sidebar->setVisible(showGroups);
        if (showGroups) {
            m_sidebar->setFixedWidth(m_breakpoint == Breakpoint::ExtraLarge ? 250 : 216);
        }
        m_groupScopeButton->setVisible(!showGroups);
        m_detail->setVisible(inlineDetail);
        if (inlineDetail) {
            m_detail->setFixedWidth(detailWidth(m_breakpoint));
            if (m_detailOverlay->isOpen()) {
                m_detailOverlay->closeOverlay();
            }
        }
        m_detailSheetButton->setVisible(!inlineDetail);
        m_detailSheetButton->setEnabled(m_dbWidget && m_dbWidget->currentSelectedEntry());
        updateFabGeometry();
    }

    void VaultScreen::openDetailSheet()
    {
        if (hasInlineDetail(m_breakpoint) || !m_dbWidget || !m_dbWidget->currentSelectedEntry()) {
            return;
        }
        updateDetail();
        m_detailOverlay->openOverlay();
    }

    void VaultScreen::rebuildGroupScopeMenu()
    {
        for (auto* action : m_groupScopeActions) {
            m_groupScopeMenu->removeAction(action);
            action->deleteLater();
        }
        m_groupScopeActions.clear();

        std::function<void(const QModelIndex&, int)> append = [&](const QModelIndex& parent, int depth) {
            for (int row = 0; row < m_groupModel->rowCount(parent); ++row) {
                const QModelIndex index = m_groupModel->index(row, 0, parent);
                Group* group = m_groupModel->groupFromIndex(index);
                if (!group) {
                    continue;
                }
                auto* action = m_groupScopeMenu->addAction(QString(depth * 2, QLatin1Char(' ')) + group->name());
                action->setCheckable(true);
                action->setChecked(m_dbWidget && m_dbWidget->currentGroup() == group);
                action->setStatusTip(action->isChecked() ? tr("Current group") : QString());
                const QPersistentModelIndex persistent(index);
                connect(action, &QAction::triggered, this, [this, persistent] {
                    if (!persistent.isValid() || !m_dbWidget) {
                        return;
                    }
                    Group* selected = m_groupModel->groupFromIndex(persistent);
                    if (selected) {
                        m_dbWidget->groupView()->setCurrentGroup(selected);
                        m_groupScopeButton->setText(selected->name());
                    }
                });
                m_groupScopeActions.append(action);
                append(index, depth + 1);
            }
        };
        append(QModelIndex(), 0);
        filterGroupScopeMenu(m_groupScopeSearch->text());
        m_groupScopeSearch->setFocus(Qt::PopupFocusReason);
    }

    void VaultScreen::filterGroupScopeMenu(const QString& query)
    {
        const QString needle = query.trimmed();
        bool valid = true;
        QString error;
        const bool regex = m_groupScopeSearch->isRegexEnabled() && !needle.isEmpty();
        if (regex) {
            const auto validation = runBounded(needle, optionsForFlags(m_groupScopeSearch->regexFlags()), QString());
            valid = validation.compiled && !validation.blocked && !validation.timedOut;
            error = validation.error;
        }
        for (auto* action : m_groupScopeActions) {
            const QString label = action->text().trimmed();
            bool match = needle.isEmpty() || label.contains(needle, Qt::CaseInsensitive);
            if (regex && valid) {
                const auto run = runBounded(needle, optionsForFlags(m_groupScopeSearch->regexFlags()), label);
                match = !run.matches.isEmpty();
            } else if (regex) {
                match = false;
            }
            action->setVisible(match);
        }
        m_groupScopeSearch->lineEdit()->setAccessibleDescription(valid ? tr("Valid group filter")
                                                                    : tr("Invalid regular expression: %1").arg(error));
        m_groupScopeSearch->setToolTip(valid ? QString() : tr("Invalid regular expression: %1").arg(error));
    }

    void VaultScreen::setHostWidget(QStackedWidget* host, DatabaseTabWidget* tabs)
    {
        if (m_host == host && m_tabs == tabs) {
            return;
        }

        if (m_host) {
            m_host->disconnect(this);
            m_stack->removeWidget(m_host);
        }
        if (m_tabs) {
            m_tabs->disconnect(this);
        }

        m_host = host;
        m_tabs = tabs;
        m_databasePage = nullptr;

        if (host) {
            // A widget moved out of another layout leaves a stale item behind
            // unless it is taken out explicitly; reparenting alone is not enough.
            if (QWidget* previousParent = host->parentWidget()) {
                if (QLayout* previousLayout = previousParent->layout()) {
                    previousLayout->removeWidget(host);
                }
            }
            m_stack->addWidget(host);
            connect(host, &QStackedWidget::currentChanged, this, [this] { updateVisiblePage(); });
        }

        if (tabs) {
            // The tab widget sits inside one page of the host stack; that page
            // being current is what "a database is on screen" means.
            for (QWidget* widget = tabs; widget; widget = widget->parentWidget()) {
                if (widget->parentWidget() == host) {
                    m_databasePage = widget;
                    break;
                }
            }
            connect(tabs, &QTabWidget::currentChanged, this, [this] {
                setDatabaseWidget(m_tabs ? m_tabs->currentDatabaseWidget() : nullptr);
            });
            connect(tabs, &DatabaseTabWidget::databaseUnlocked, this, [this] {
                setDatabaseWidget(m_tabs ? m_tabs->currentDatabaseWidget() : nullptr);
            });
            connect(tabs, &DatabaseTabWidget::databaseLocked, this, [this] { updateVisiblePage(); });
            connect(tabs, &DatabaseTabWidget::databaseOpened, this, [this] {
                setDatabaseWidget(m_tabs ? m_tabs->currentDatabaseWidget() : nullptr);
            });
            setDatabaseWidget(tabs->currentDatabaseWidget());
        } else {
            setDatabaseWidget(nullptr);
        }

        updateVisiblePage();
    }

    void VaultScreen::setDatabaseWidget(DatabaseWidget* dbWidget)
    {
        if (dbWidget == m_dbWidget && !m_databaseConnections.isEmpty()) {
            updateVisiblePage();
            return;
        }

        for (const auto& connection : m_databaseConnections) {
            disconnect(connection);
        }
        m_databaseConnections.clear();

        m_dbWidget = dbWidget;

        if (!dbWidget) {
            m_entryModel->setSourceModel(nullptr);
            m_entryModel->setDatabase(nullptr);
            m_groupModel->setSourceModel(nullptr);
            m_sidebar->setTags({});
            m_detail->clear();
            m_sheetDetail->clear();
            updateResultLine();
            updateVisiblePage();
            return;
        }

        auto* entryView = dbWidget->entryView();
        auto* groupView = dbWidget->groupView();

        QAbstractItemModel* entrySource = entryView ? entryView->model() : nullptr;
        if (auto* proxy = qobject_cast<QAbstractProxyModel*>(entrySource)) {
            entrySource = proxy->sourceModel();
        }
        m_entryModel->setSourceModel(entrySource);
        m_entryModel->setDatabase(dbWidget->database());
        m_groupModel->setSourceModel(groupView ? groupView->model() : nullptr);

        if (entryView) {
            m_databaseConnections << connect(
                entryView, &EntryView::entrySelectionChanged, this, [this] { syncSelectionFromDatabase(); });
        }
        if (groupView) {
            m_databaseConnections << connect(
                groupView, &GroupView::groupSelectionChanged, this, [this] { syncGroupFromDatabase(); });
        }

        m_databaseConnections << connect(dbWidget, &DatabaseWidget::currentModeChanged, this, [this] {
            updateVisiblePage();
            updateDetail();
        });
        m_databaseConnections << connect(dbWidget, &DatabaseWidget::databaseUnlocked, this, [this] {
            // Unlocking swaps in a new Database, so the re-use pass needs it.
            m_entryModel->setDatabase(m_dbWidget ? m_dbWidget->database() : nullptr);
            updateTags();
            syncGroupFromDatabase();
            syncSelectionFromDatabase();
            updateVisiblePage();
        });
        m_databaseConnections << connect(dbWidget, &DatabaseWidget::databaseLocked, this, [this] {
            m_detail->clear();
            m_sheetDetail->clear();
            updateVisiblePage();
        });
        m_databaseConnections << connect(dbWidget, &DatabaseWidget::databaseModified, this, [this] {
            // Any edit can create or resolve a re-used password.
            m_entryModel->invalidateHealth();
            updateTags();
            updateDetail();
            updateResultLine();
        });
        m_databaseConnections << connect(dbWidget, &DatabaseWidget::databaseSaved, this, [this] { updateTags(); });
        m_databaseConnections << connect(dbWidget, &DatabaseWidget::groupChanged, this, [this] {
            syncGroupFromDatabase();
            updateResultLine();
        });
        m_databaseConnections << connect(dbWidget, &DatabaseWidget::clearSearch, this, [this] {
            if (m_syncingSearch) {
                return;
            }
            QScopedValueRollback<bool> guard(m_syncingSearch, true);
            m_searchBar->clear();
            // There is no pattern left to be broken.
            m_regexInvalid = false;
            updateResultLine();
        });
        // The tool bar search field is still reachable; keep the pill honest
        // about what is actually being searched for.
        m_databaseConnections << connect(dbWidget, &DatabaseWidget::searchModeActivated, this, [this] {
            if (m_syncingSearch || !m_dbWidget) {
                return;
            }
            const QString text = m_dbWidget->getCurrentSearch();
            if (!text.isEmpty() && text != m_searchBar->text()) {
                QScopedValueRollback<bool> guard(m_syncingSearch, true);
                m_searchBar->setText(text);
            }
            updateResultLine();
        });
        m_databaseConnections << connect(
            dbWidget, &DatabaseWidget::listModeActivated, this, &VaultScreen::updateResultLine);

        // A message banner inside the database widget cannot be seen while the
        // panes are in front of it, so what it says is repeated as a toast. The
        // test is on the banner itself: whenever the stock stack is the visible
        // page the banner speaks for itself and nothing is said twice.
        for (MessageWidget* banner : dbWidget->findChildren<MessageWidget*>()) {
            m_databaseConnections << connect(banner, &MessageWidget::showAnimationStarted, this, [banner] {
                if (banner->isVisible() || banner->text().isEmpty()) {
                    return;
                }
                announce(banner->text(), banner->messageType());
            });
        }

        updateTags();
        syncGroupFromDatabase();
        syncSelectionFromDatabase();
        updateResultLine();
        updateVisiblePage();
    }

    void VaultScreen::focusSearch()
    {
        m_searchBar->setFocus(Qt::ShortcutFocusReason);
        m_searchBar->lineEdit()->selectAll();
    }

    void VaultScreen::runSearch()
    {
        if (!m_dbWidget || m_syncingSearch) {
            return;
        }

        QStringList terms;
        const QString text = m_searchBar->text().trimmed();
        const bool regex = m_searchBar->isRegexEnabled();

        // A pattern that does not compile shows nothing until it parses: the
        // search is not run at all, so the rows behind it are not left standing
        // as if they still matched.
        m_regexInvalid = regex && !text.isEmpty() && !QRegularExpression(text).isValid();
        if (m_regexInvalid) {
            updateResultLine();
            return;
        }

        if (!text.isEmpty()) {
            terms << (regex ? regexTerm(text) : text);
        }
        for (const QString& tag : m_sidebar->selectedTags()) {
            QString escaped = tag;
            escaped.replace(QLatin1String("\""), QLatin1String("\\\""));
            terms << QStringLiteral("tag:\"%1\"").arg(escaped);
        }

        QScopedValueRollback<bool> guard(m_syncingSearch, true);
        // An empty search string is how the existing entry point ends a search.
        m_dbWidget->search(terms.join(QLatin1Char(' ')));
        updateResultLine();
    }

    void VaultScreen::updateTags()
    {
        if (!m_dbWidget || !m_dbWidget->database() || m_dbWidget->isLocked()) {
            m_sidebar->setTags({});
            return;
        }
        m_sidebar->setTags(m_dbWidget->database()->tagList());
    }

    void VaultScreen::updateResultLine()
    {
        const int rows = m_entryModel->rowCount();
        const bool searching = m_dbWidget && m_dbWidget->isSearchActive();

        m_resultLabel->setStyleSheet(resultLineStyle());

        if (m_regexInvalid) {
            m_resultLabel->setText(tr("Invalid regular expression — showing nothing until it parses."));
            m_emptyLabel->setText(tr("No entry matches this search."));
            m_listStack->setCurrentWidget(m_emptyState);
            return;
        }

        QString line = tr("%n entry(s)", "number of entries in the list", rows);
        if (searching && m_searchBar->isRegexEnabled() && !m_searchBar->text().trimmed().isEmpty()) {
            line = tr("%1 · regex /%2/i").arg(line, m_searchBar->text().trimmed());
        }
        m_resultLabel->setText(line);

        m_emptyLabel->setText(searching ? tr("No entry matches this search.") : tr("This group has no entries."));
        m_listStack->setCurrentWidget(rows > 0 ? static_cast<QWidget*>(m_entryList) : m_emptyState);
    }

    QString VaultScreen::resultLineStyle() const
    {
        return QStringLiteral("color: %1; background: transparent;")
            .arg(theme()->hex(m_regexInvalid ? Role::Error : Role::OnSurfaceVariant));
    }

    void VaultScreen::updateDetail()
    {
        Entry* entry = m_dbWidget && !m_dbWidget->isLocked() ? m_dbWidget->currentSelectedEntry() : nullptr;
        if (!entry) {
            m_detail->clear();
            m_sheetDetail->clear();
            m_detailSheetButton->setEnabled(false);
            m_totpStep = -1;
            updateTotpTimer();
            return;
        }

        EntryDetailData data;
        data.title = entry->resolveMultiplePlaceholders(entry->title());
        data.url = entry->resolveMultiplePlaceholders(entry->displayUrl());
        data.symbol = Icons::entrySymbol(entry);
        data.username = entry->resolveMultiplePlaceholders(entry->username());
        data.password = entry->resolveMultiplePlaceholders(entry->password());
        data.notes = entry->notes();
        data.health = m_entryModel->healthOf(entry);
        // tagList() is the stored list: already trimmed, so a tag written with
        // spaces around it still registers.
        data.favourite = entry->tagList().contains(favouriteTag());

        if (!entry->password().isEmpty()) {
            data.strengthPercent = qBound(0, entry->passwordHealth()->score(), 100);
        }
        // The meter is labelled with the health state, not with the password
        // quality: Healthy / Weak / Reused / Breached, the one vocabulary the
        // row's health column and the meter's own tint already share. An entry
        // that cannot be judged is left unlabelled rather than read "Unknown".
        if (data.health != Health::Unknown) {
            data.strengthLabel = Theme::healthLabel(data.health);
        }

        m_totpStep = -1;
        if (entry->hasTotp()) {
            bool valid = false;
            const QString code = entry->totp(&valid);
            if (valid) {
                data.totpCode = code;
            }
            const auto settings = entry->totpSettings();
            if (settings && settings->step > 0) {
                data.totpPeriod = static_cast<int>(settings->step);
            }
            if (!data.totpCode.isEmpty()) {
                m_totpStep = totpStep(data.totpPeriod);
            }
        }

        const auto attachmentKeys = entry->attachments()->keys();
        for (const QString& key : attachmentKeys) {
            data.attachments.append({key, formatSize(entry->attachments()->value(key).size())});
        }

        const int revisions = entry->historyItems().size();
        data.historySummary = revisions > 0
                                  ? tr("%n previous version(s) · last change %1", "", revisions)
                                        .arg(Clock::toString(entry->timeInfo().lastModificationTime().toLocalTime()))
                                  : tr("No previous versions");

        m_detail->setEntryData(data);
        m_sheetDetail->setEntryData(data);
        m_detailSheetButton->setEnabled(true);
        updateTotpTimer();
    }

    void VaultScreen::refreshTotp()
    {
        if (m_totpStep < 0 || totpStep(m_detail->entryData().totpPeriod) == m_totpStep) {
            return;
        }
        // Same entry, only a newer code. The pane rebuilds itself from whatever
        // it is handed, so the reveal is put back the way the user left it -
        // this is not a new selection, where hiding the password is the point.
        const bool revealed = m_detail->isPasswordVisible();
        const bool sheetRevealed = m_sheetDetail->isPasswordVisible();
        updateDetail();
        if (m_totpStep >= 0) {
            m_detail->setPasswordVisible(revealed);
            m_sheetDetail->setPasswordVisible(sheetRevealed);
        }
    }

    void VaultScreen::updateTotpTimer()
    {
        const bool run = m_totpStep >= 0;
        if (run == m_totpTimer->isActive()) {
            return;
        }
        if (run) {
            m_totpTimer->start();
        } else {
            m_totpTimer->stop();
        }
    }

    void VaultScreen::copyField(const QString& field)
    {
        if (!m_dbWidget) {
            return;
        }
        if (field == QLatin1String("username")) {
            m_dbWidget->copyUsername();
        } else if (field == QLatin1String("password")) {
            m_dbWidget->copyPassword();
        } else if (field == QLatin1String("totp")) {
            m_dbWidget->copyTotp();
        }
    }

    void VaultScreen::toggleFavourite(bool favourite)
    {
        Entry* entry = m_dbWidget ? m_dbWidget->currentSelectedEntry() : nullptr;
        if (!entry) {
            return;
        }

        // The stored list, trimmed and de-duplicated by setTags() already, so the
        // test below and the one the detail pane is given cannot disagree.
        QStringList tags = entry->tagList();
        const bool has = tags.contains(favouriteTag());
        if (favourite == has) {
            return;
        }
        if (favourite) {
            tags.append(favouriteTag());
        } else {
            tags.removeAll(favouriteTag());
        }
        entry->setTags(tags.join(QLatin1Char(',')));
        updateTags();
    }

    void VaultScreen::showEntryMenu(const QModelIndex& index, const QPoint& globalPos)
    {
        if (index.isValid() && !m_entryList->selectionModel()->isSelected(index)) {
            m_entryList->selectionModel()->setCurrentIndex(
                index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        }
        auto* window = getMainWindow();
        if (!window) {
            return;
        }
        // Queued so the selection above has reached the real entry view - and
        // the actions their enabled state - before the menu pops up.
        QMetaObject::invokeMethod(window, "showEntryContextMenu", Qt::QueuedConnection, Q_ARG(QPoint, globalPos));
    }

    void VaultScreen::syncSelectionToDatabase()
    {
        if (m_syncingSelection || !m_dbWidget) {
            return;
        }
        auto* view = m_dbWidget->entryView();
        if (!view || !view->selectionModel()) {
            return;
        }

        QScopedValueRollback<bool> guard(m_syncingSelection, true);

        QItemSelection selection;
        const auto rows = m_entryList->selectionModel()->selectedIndexes();
        for (const QModelIndex& index : rows) {
            Entry* entry = m_entryModel->entryFromIndex(index);
            if (!entry) {
                continue;
            }
            const QModelIndex target = view->indexFromEntry(entry);
            if (target.isValid()) {
                selection.select(target, target);
            }
        }

        Entry* current = m_entryModel->entryFromIndex(m_entryList->currentIndex());
        const QModelIndex currentTarget = current ? view->indexFromEntry(current) : QModelIndex();

        auto* selectionModel = view->selectionModel();
        if (selection.isEmpty()) {
            selectionModel->clearSelection();
        } else {
            selectionModel->select(selection,
                                   QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        }
        if (currentTarget.isValid()) {
            selectionModel->setCurrentIndex(currentTarget, QItemSelectionModel::NoUpdate);
        }

        updateDetail();
    }

    void VaultScreen::syncSelectionFromDatabase()
    {
        if (m_syncingSelection || !m_dbWidget) {
            return;
        }

        QScopedValueRollback<bool> guard(m_syncingSelection, true);

        Entry* entry = m_dbWidget->currentSelectedEntry();
        const QModelIndex index = entry ? m_entryModel->indexFromEntry(entry) : QModelIndex();
        auto* selectionModel = m_entryList->selectionModel();
        if (index.isValid()) {
            selectionModel->setCurrentIndex(index,
                                            QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
            m_entryList->scrollTo(index, QAbstractItemView::EnsureVisible);
        } else {
            selectionModel->clearSelection();
        }

        updateDetail();
        updateResultLine();
    }

    void VaultScreen::syncGroupFromDatabase()
    {
        if (m_syncingGroup || !m_dbWidget) {
            return;
        }

        QScopedValueRollback<bool> guard(m_syncingGroup, true);

        Group* group = m_dbWidget->currentGroup();
        const QModelIndex index = group ? m_groupModel->indexFromGroup(group) : QModelIndex();
        auto* tree = m_sidebar->groupView();
        if (index.isValid() && tree->selectionModel()) {
            tree->selectionModel()->setCurrentIndex(index,
                                                    QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        }
    }

    void VaultScreen::updateVisiblePage()
    {
        const bool databaseOnScreen = m_databasePage && m_host && m_host->currentWidget() == m_databasePage;
        const bool viewMode = m_dbWidget && m_dbWidget->currentMode() == DatabaseWidget::Mode::ViewMode;

        QWidget* page = (databaseOnScreen && viewMode) ? m_panes : static_cast<QWidget*>(m_host.data());
        if (page && m_stack->currentWidget() != page) {
            m_stack->setCurrentWidget(page);
        }
        if (page == m_panes) {
            updateFabGeometry();
        }
    }

    void VaultScreen::applyTheme()
    {
        const QString metaStyle =
            QStringLiteral("color: %1; background: transparent;").arg(theme()->hex(Role::OnSurfaceVariant));
        m_resultLabel->setFont(theme()->font(TypeRole::BodySmall));
        m_resultLabel->setStyleSheet(resultLineStyle());

        m_emptyLabel->setFont(scaledFont(TypeRole::BodyMedium, EmptyLineSize));
        m_emptyLabel->setStyleSheet(metaStyle);

        QColor glyphTint = theme()->color(Role::OnSurfaceVariant);
        glyphTint.setAlphaF(EmptyGlyphOpacity);
        m_emptyGlyph->setPixmap(Icons::pixmap(QStringLiteral("search_off"), EmptyGlyphSize, glyphTint));

        m_entryList->setFont(theme()->font(TypeRole::BodyMedium));
        m_entryList->doItemsLayout();
        m_entryList->viewport()->update();
        update();
    }

    void VaultScreen::updateFabGeometry()
    {
        if (!m_fab || !m_centre) {
            return;
        }
        const QSize hint = m_fab->sizeHint();
        m_fab->setGeometry(m_centre->width() - hint.width() - FabMargin,
                           m_centre->height() - hint.height() - FabMargin,
                           hint.width(),
                           hint.height());
        m_fab->raise();
    }

    bool VaultScreen::eventFilter(QObject* watched, QEvent* event)
    {
        if (watched == m_centre && event->type() == QEvent::Resize) {
            updateFabGeometry();
            const bool compact = m_centre->width() < CompactListWidth;
            if (compact != m_entryDelegate->compactColumns()) {
                m_entryDelegate->setCompactColumns(compact);
                m_entryList->viewport()->update();
            }
        }
        return QWidget::eventFilter(watched, event);
    }

    void VaultScreen::paintEvent(QPaintEvent* event)
    {
        Q_UNUSED(event);
        QPainter painter(this);
        painter.fillRect(rect(), theme()->color(Role::Surface));
    }

    void VaultScreen::resizeEvent(QResizeEvent* event)
    {
        QWidget::resizeEvent(event);
        updateFabGeometry();
    }

} // namespace Material
