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
#include "MaterialRegexBuilder.h"
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
#include <QPainter>
#include <QResizeEvent>
#include <QScopedValueRollback>
#include <QStackedWidget>
#include <QUrl>
#include <QVBoxLayout>

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
        constexpr float EmptyGlyphOpacity = 0.5f;
        /** Below this the row sheds its url, health and modified columns. */
        constexpr int CompactListWidth = 620;

        /**
         * The health of one entry, straight out of the password health the rest
         * of the application already uses.
         *
         * Re-use and breach detection need a whole-database pass that is not
         * reachable from a single entry, so those two states are never claimed
         * here: an entry that cannot be judged answers Unknown rather than a
         * made up verdict.
         */
        Health healthOfEntry(Entry* entry)
        {
            if (!entry || entry->excludeFromReports() || entry->password().isEmpty()) {
                return Health::Unknown;
            }
            switch (entry->passwordHealth()->quality()) {
            case PasswordHealth::Quality::Bad:
            case PasswordHealth::Quality::Poor:
            case PasswordHealth::Quality::Weak:
                return Health::Weak;
            case PasswordHealth::Quality::Good:
            case PasswordHealth::Quality::Excellent:
                return Health::Ok;
            }
            return Health::Unknown;
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

        QString qualityLabel(PasswordHealth::Quality quality)
        {
            switch (quality) {
            case PasswordHealth::Quality::Bad:
                return VaultScreen::tr("Bad");
            case PasswordHealth::Quality::Poor:
                return VaultScreen::tr("Poor");
            case PasswordHealth::Quality::Weak:
                return VaultScreen::tr("Weak");
            case PasswordHealth::Quality::Good:
                return VaultScreen::tr("Good");
            case PasswordHealth::Quality::Excellent:
                return VaultScreen::tr("Excellent");
            }
            return {};
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
        case EntryDelegate::ModifiedRole:
            return column(EntryModel::Modified, Qt::DisplayRole);
        case EntryDelegate::TotpRole:
            return column(EntryModel::Totp, Qt::UserRole);
        case EntryDelegate::HealthRole:
            return QVariant::fromValue(healthOfEntry(model->entryFromIndex(source)));
        case EntryDelegate::SymbolRole:
            // No Material Symbols name: the delegate falls back to the entry's
            // own icon, which is what the decoration of the title column is.
            return {};
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
            const int leftRank = healthRank(healthOfEntry(leftEntry));
            const int rightRank = healthRank(healthOfEntry(rightEntry));
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

        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(0);

        m_stack = new QStackedWidget(this);
        root->addWidget(m_stack, 1);

        m_panes = buildPanes();
        m_stack->addWidget(m_panes);

        m_regexBuilder = new RegexBuilder(this);
        connect(m_regexBuilder, &RegexBuilder::patternApplied, this, [this](const QString& pattern) {
            m_searchBar->setRegexEnabled(true);
            m_searchBar->setText(pattern);
            runSearch();
        });
        connect(m_regexBuilder, &RegexBuilder::patternCopied, this, [](const QString& pattern) {
            clipboard()->setText(pattern);
        });

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

        connect(m_detail, &EntryDetail::copyRequested, this, &VaultScreen::copyField);
        connect(m_detail, &EntryDetail::autoTypeRequested, this, [this] {
            if (m_dbWidget) {
                m_dbWidget->performAutoType();
            }
        });
        connect(m_detail, &EntryDetail::editRequested, this, [this] {
            if (m_dbWidget) {
                m_dbWidget->switchToEntryEdit();
            }
        });
        connect(m_detail, &EntryDetail::deleteRequested, this, [this] {
            if (m_dbWidget) {
                m_dbWidget->deleteSelectedEntries();
            }
        });
        connect(m_detail, &EntryDetail::historyRequested, this, [this] {
            openEntryEditor(m_dbWidget, EditEntryWidget::Page::History);
        });
        connect(m_detail, &EntryDetail::attachmentActivated, this, [this](const QString&) {
            openEntryEditor(m_dbWidget, EditEntryWidget::Page::Advanced);
        });
        connect(m_detail, &EntryDetail::favouriteToggled, this, &VaultScreen::toggleFavourite);

        return panes;
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
        m_searchBar->setPlaceholder(tr("Search entries"));
        headerLayout->addWidget(m_searchBar);

        auto* summaryRow = new QWidget(header);
        auto* summaryLayout = new QHBoxLayout(summaryRow);
        summaryLayout->setContentsMargins(0, 0, 0, 0);
        summaryLayout->setSpacing(8);

        m_resultLabel = new QLabel(summaryRow);
        summaryLayout->addWidget(m_resultLabel, 1);

        m_sortControl = new SegmentedButton(summaryRow);
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
        emptyLayout->addStretch(1);
        m_emptyGlyph = new QLabel(m_emptyState);
        m_emptyGlyph->setAlignment(Qt::AlignCenter);
        emptyLayout->addWidget(m_emptyGlyph, 0);
        m_emptyLabel = new QLabel(m_emptyState);
        m_emptyLabel->setAlignment(Qt::AlignCenter);
        m_emptyLabel->setWordWrap(true);
        emptyLayout->addWidget(m_emptyLabel, 0);
        emptyLayout->addStretch(2);
        m_listStack->addWidget(m_emptyState);

        layout->addWidget(m_listStack, 1);

        m_fab = new ExtendedFab(QStringLiteral("add"), tr("New Entry"), centre);
        m_fab->raise();

        connect(m_searchBar, &SearchBar::textChanged, this, [this] { runSearch(); });
        connect(m_searchBar, &SearchBar::regexToggled, this, [this] { runSearch(); });
        connect(m_searchBar, &SearchBar::returnPressed, this, [this] { runSearch(); });
        connect(m_searchBar, &SearchBar::builderRequested, this, [this] {
            m_regexBuilder->setPattern(m_searchBar->text());
            m_regexBuilder->openOverlay();
        });

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
            m_groupModel->setSourceModel(nullptr);
            m_sidebar->setTags({});
            m_detail->clear();
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
            updateTags();
            syncGroupFromDatabase();
            syncSelectionFromDatabase();
            updateVisiblePage();
        });
        m_databaseConnections << connect(dbWidget, &DatabaseWidget::databaseLocked, this, [this] {
            m_detail->clear();
            updateVisiblePage();
        });
        m_databaseConnections << connect(dbWidget, &DatabaseWidget::databaseModified, this, [this] {
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
        if (!text.isEmpty()) {
            terms << (m_searchBar->isRegexEnabled() ? regexTerm(text) : text);
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

        QString line = tr("%n entry(s)", "number of entries in the list", rows);
        if (searching && m_searchBar->isRegexEnabled() && !m_searchBar->text().trimmed().isEmpty()) {
            line = tr("%1 · regex /%2/i").arg(line, m_searchBar->text().trimmed());
        }
        m_resultLabel->setText(line);

        m_emptyLabel->setText(searching ? tr("No entry matches this search.") : tr("This group has no entries."));
        m_listStack->setCurrentWidget(rows > 0 ? static_cast<QWidget*>(m_entryList) : m_emptyState);
    }

    void VaultScreen::updateDetail()
    {
        Entry* entry = m_dbWidget && !m_dbWidget->isLocked() ? m_dbWidget->currentSelectedEntry() : nullptr;
        if (!entry) {
            m_detail->clear();
            return;
        }

        EntryDetailData data;
        data.title = entry->resolveMultiplePlaceholders(entry->title());
        data.url = entry->resolveMultiplePlaceholders(entry->displayUrl());
        data.username = entry->resolveMultiplePlaceholders(entry->username());
        data.password = entry->resolveMultiplePlaceholders(entry->password());
        data.notes = entry->notes();
        data.health = healthOfEntry(entry);
        data.favourite = entry->tags().split(QLatin1Char(','), Qt::SkipEmptyParts).contains(QStringLiteral("Favorite"));

        if (!entry->password().isEmpty()) {
            const auto health = entry->passwordHealth();
            data.strengthPercent = qBound(0, health->score(), 100);
            data.strengthLabel = qualityLabel(health->quality());
        }

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

        static const QString favouriteTag = QStringLiteral("Favorite");
        QStringList tags = entry->tags().split(QLatin1Char(','), Qt::SkipEmptyParts);
        for (QString& tag : tags) {
            tag = tag.trimmed();
        }
        const bool has = tags.contains(favouriteTag);
        if (favourite == has) {
            return;
        }
        if (favourite) {
            tags.append(favouriteTag);
        } else {
            tags.removeAll(favouriteTag);
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
        m_resultLabel->setStyleSheet(metaStyle);

        m_emptyLabel->setFont(theme()->font(TypeRole::TitleSmall));
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
