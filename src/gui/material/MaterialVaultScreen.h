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

#ifndef KEEPASSXC_MATERIALVAULTSCREEN_H
#define KEEPASSXC_MATERIALVAULTSCREEN_H

#include "MaterialTheme.h"
#include "MaterialBreakpoints.h"

#include <QHash>
#include <QIdentityProxyModel>
#include <QList>
#include <QPointer>
#include <QSharedPointer>
#include <QSortFilterProxyModel>
#include <QWidget>

class Database;
class DatabaseTabWidget;
class DatabaseWidget;
class Entry;
class EntryModel;
class Group;
class QLabel;
class QListView;
class QStackedWidget;
class QTimer;
class QToolButton;
class QMenu;
class QAction;
class QWidgetAction;

namespace Material
{
    class EntryDelegate;
    class EntryDetail;
    class ExtendedFab;
    class Overlay;
    class SearchBar;
    class SegmentedButton;
    class VaultSidebar;

    /**
     * The entry list model the Material row delegate reads.
     *
     * KeePassXC's EntryModel is a seventeen column table; EntryDelegate draws
     * one row at a time out of custom roles. This proxy keeps a single column -
     * the title - and answers the delegate's roles from the sibling columns of
     * the same source row, so no entry loading, no search and no icon lookup is
     * duplicated. Selection, drag data and the entries themselves all still
     * belong to the EntryModel underneath.
     *
     * The sort key is the segmented control above the list rather than a header
     * section, so lessThan() is keyed on it instead of on a column.
     */
    class EntryListModel : public QSortFilterProxyModel
    {
        Q_OBJECT

    public:
        enum class SortKey
        {
            Title,
            Modified,
            Health
        };

        explicit EntryListModel(QObject* parent = nullptr);
        ~EntryListModel() override;

        SortKey sortKey() const;
        void setSortKey(SortKey key);

        /**
         * The database the rows belong to. Password re-use can only be seen
         * across the whole of it, so the health verdict needs it as well as the
         * row's own entry.
         */
        void setDatabase(const QSharedPointer<Database>& db);
        /** Drop the cached re-use index; the next verdict rebuilds it. */
        void invalidateHealth();
        /**
         * The design's four health states for one entry: Breached, Weak, Reused
         * or Ok. An entry excluded from the reports or without a password cannot
         * be judged and answers Unknown, which the row leaves blank.
         */
        Health healthOf(Entry* entry) const;

        /** The entry behind a proxy row, or nullptr when the row is stale. */
        Entry* entryFromIndex(const QModelIndex& index) const;
        QModelIndex indexFromEntry(Entry* entry) const;

        QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    protected:
        bool filterAcceptsColumn(int sourceColumn, const QModelIndex& sourceParent) const override;
        bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

    private:
        EntryModel* entryModel() const;

        SortKey m_sortKey = SortKey::Title;
        QSharedPointer<Database> m_database;
        // Rebuilt on demand: one pass over the database answers every row.
        mutable QHash<QString, int> m_reuse;
        mutable bool m_reuseDirty = true;
    };

    /**
     * The group tree model the Material pill delegate reads.
     *
     * GroupModel already supplies the name and the icon; the sidebar rows also
     * carry the number of entries in the group, which is what this adds. The
     * tree structure, the drops and the groups themselves are untouched.
     */
    class GroupTreeModel : public QIdentityProxyModel
    {
        Q_OBJECT

    public:
        explicit GroupTreeModel(QObject* parent = nullptr);
        ~GroupTreeModel() override;

        /** The group behind a proxy index, or nullptr when it is not a group. */
        Group* groupFromIndex(const QModelIndex& index) const;
        QModelIndex indexFromGroup(Group* group) const;

        QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    };

    /**
     * The vault destination: the three panes of the design, bound to the open
     * database.
     *
     *     [ VaultSidebar 250 | search / list | EntryDetail 392 ]
     *
     * Nothing here owns database state. The sidebar draws the DatabaseWidget's
     * own GroupModel and selecting a row drives the real GroupView; the list
     * draws the real EntryModel and selecting a row drives the real EntryView,
     * so search, filtering, the context menus and every entry action behave
     * exactly as they did before. The detail pane is filled from the selected
     * entry and its buttons call the DatabaseWidget slots that already
     * implement them.
     *
     * The panes can only show an unlocked database in view mode. Everything
     * else the stock stack still owns - the welcome screen, the unlock dialog,
     * the entry and group editors, the reports and database settings pages, the
     * password generator - is kept alive underneath and shown in their place,
     * which is why the whole database lifecycle keeps working.
     */
    class VaultScreen : public QWidget
    {
        Q_OBJECT

    public:
        explicit VaultScreen(QWidget* parent = nullptr);
        ~VaultScreen() override;

        /**
         * Adopt the stock stacked widget (welcome screen, database tabs,
         * password generator) and the tab widget inside it. The screen follows
         * the tab widget from here on and needs no further wiring.
         */
        void setHostWidget(QStackedWidget* host, DatabaseTabWidget* tabs);

        VaultSidebar* sidebar() const;
        EntryDetail* detail() const;
        SearchBar* searchBar() const;

        /** The database the panes are currently bound to; may be nullptr. */
        DatabaseWidget* databaseWidget() const;
        Breakpoint breakpoint() const;
        bool groupPaneVisible() const;
        bool detailPaneInline() const;
        QToolButton* groupScopeButton() const;
        QToolButton* detailSheetButton() const;

    public slots:

        /** Open the current database folder in the configured external editor, or the file manager. */

        void openDatabaseFolderExternally();
        void setDatabaseWidget(DatabaseWidget* dbWidget);
        void focusSearch();
        void setBreakpoint(Material::Breakpoint breakpoint);
        void openDetailSheet();

    protected:
        bool eventFilter(QObject* watched, QEvent* event) override;
        void paintEvent(QPaintEvent* event) override;
        void resizeEvent(QResizeEvent* event) override;

    private:
        QWidget* buildPanes();
        QWidget* buildCentreColumn();

        void applyTheme();
        /** Stylesheet of the result line; error red while the pattern is broken. */
        QString resultLineStyle() const;
        void updateFabGeometry();
        void applyBreakpoint();
        void rebuildGroupScopeMenu();
        void filterGroupScopeMenu(const QString& query);
        void connectDetailActions(EntryDetail* detail);
        void updateVisiblePage();
        void updateResultLine();
        void updateTags();
        void updateDetail();
        /**
         * Hand the detail pane a fresh one-time password when the step it was
         * generated in ends.
         *
         * The pane redraws its countdown ring from the wall clock, but the code
         * itself only ever arrives through setEntryData() and it has no signal
         * to ask for a newer one - so the step boundary is watched here instead
         * and the selected entry is read again across it.
         */
        void refreshTotp();
        /** Run that watch only while a code is actually on screen. */
        void updateTotpTimer();
        void runSearch();

        void syncSelectionToDatabase();
        void syncSelectionFromDatabase();
        void syncGroupFromDatabase();

        void showEntryMenu(const QModelIndex& index, const QPoint& globalPos);
        void copyField(const QString& field);
        void toggleFavourite(bool favourite);

        QWidget* m_panes = nullptr;
        QStackedWidget* m_stack = nullptr;
        QPointer<QStackedWidget> m_host;
        QPointer<QWidget> m_databasePage;
        QPointer<DatabaseTabWidget> m_tabs;
        QPointer<DatabaseWidget> m_dbWidget;

        VaultSidebar* m_sidebar = nullptr;
        EntryDetail* m_detail = nullptr;
        EntryDetail* m_sheetDetail = nullptr;
        Overlay* m_detailOverlay = nullptr;
        SearchBar* m_searchBar = nullptr;
        QToolButton* m_groupScopeButton = nullptr;
        QToolButton* m_detailSheetButton = nullptr;
        QMenu* m_groupScopeMenu = nullptr;
        SearchBar* m_groupScopeSearch = nullptr;
        QWidgetAction* m_groupScopeSearchAction = nullptr;
        QList<QAction*> m_groupScopeActions;
        SegmentedButton* m_sortControl = nullptr;
        QLabel* m_resultLabel = nullptr;
        QWidget* m_centre = nullptr;
        QStackedWidget* m_listStack = nullptr;
        QListView* m_entryList = nullptr;
        QWidget* m_emptyState = nullptr;
        QLabel* m_emptyGlyph = nullptr;
        QLabel* m_emptyLabel = nullptr;
        ExtendedFab* m_fab = nullptr;

        EntryListModel* m_entryModel = nullptr;
        GroupTreeModel* m_groupModel = nullptr;
        EntryDelegate* m_entryDelegate = nullptr;
        QTimer* m_totpTimer = nullptr;
        /** The step the code on screen belongs to; negative when there is none. */
        qint64 m_totpStep = -1;

        QList<QMetaObject::Connection> m_databaseConnections;
        bool m_syncingSelection = false;
        bool m_syncingGroup = false;
        bool m_syncingSearch = false;
        /** The regex chip is on and the pattern does not compile. */
        bool m_regexInvalid = false;
        Breakpoint m_breakpoint = Breakpoint::ExtraLarge;
    };

} // namespace Material

#endif // KEEPASSXC_MATERIALVAULTSCREEN_H
