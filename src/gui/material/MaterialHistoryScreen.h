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

#ifndef KEEPASSXC_MATERIALHISTORYSCREEN_H
#define KEEPASSXC_MATERIALHISTORYSCREEN_H

#include "MaterialChip.h"
#include "MaterialScreen.h"

#include <QString>
#include <QStringList>
#include <QDate>
#include <QDateTime>
#include <QHash>
#include <QSet>
#include <QVector>

class QVBoxLayout;
class QDateEdit;
class QComboBox;
class QLabel;
class QProgressBar;
class QToolButton;
class QResizeEvent;

namespace Material
{
    /**
     * Which container family a revision's 40px glyph circle is drawn in.
     *
     * The design tints the circle by what the change was about, so a deletion
     * reads as an error and a restore as healthy without the screen knowing
     * anything about where a revision came from.
     */
    enum class RevisionTint
    {
        Neutral, // secondary container: the design's ordinary change
        Accent, // primary container: a secret of an entry changed
        Positive, // green container: something was put back
        Negative, // error container: something was removed
        Muted // surface container: the lines that are not changes at all
    };

    /**
     * One row of the version history.
     *
     * @p id is opaque to the screen: it is minted by the feed and handed back
     * with the two action signals, so the feed can find the row's subject again
     * without the screen holding a pointer into the database.
     *
     * The two action flags are what the row can honestly offer. A row that
     * describes a database-level save can be compared but not put back, and the
     * lines the screen shows when there is nothing to list can do neither, so
     * they are drawn with no buttons rather than with buttons that excuse
     * themselves.
     */
    struct Revision
    {
        QString id;
        QString symbol;
        QString label;
        QString meta;
        RevisionTint tint = RevisionTint::Neutral;
        bool canDiff = false;
        bool canRestore = false;
        QString action;
        QDateTime timestamp;
    };

    /** Which kind of revision the Entries / Settings chips leave showing. */
    enum class RevisionFilter
    {
        All,
        Entries,
        Settings
    };

    /**
     * The version history destination: a blurb, a search bar with filter chips
     * and a column of revision rows, each offering the actions it can keep.
     *
     * The chips only report what the user picked; the feed owns the filtering,
     * the same way the search box does.
     */
    class HistoryScreen : public Screen
    {
        Q_OBJECT

    public:
        enum class State { Empty, Loading, Populated, Progress, Warning, Error };
        explicit HistoryScreen(QWidget* parent = nullptr);
        ~HistoryScreen() override;

        void setRevisions(const QVector<Revision>& revisions);
        void setState(State state, const QString& message = {}, int progress = -1);
        State state() const;
        QDate fromDate() const;
        QDate toDate() const;
        QStringList actionFilters() const;
        void setActionCounts(const QHash<QString, int>& counts);
        QStringList selectedRevisionIds() const;

        /** Which of the two mutually exclusive kind chips is pressed. */
        RevisionFilter kindFilter() const;
        /** Whether the date chip is pressed, scoping the list to its window. */
        bool isRecentOnly() const;
        /** How far back the date chip reaches, in days. */
        static int recentDays();

    signals:
        void diffRequested(const QString& id);
        void restoreRequested(const QString& id);
        /** A filter chip was pressed or released. */
        void filterChanged();
        void exportRequested(const QStringList& ids);

    protected:
        void resizeEvent(QResizeEvent* event) override;

    private:
        void rebuild();
        void updateSelectionActions();
        void applyResponsiveLayout();

        QVBoxLayout* m_revisionLayout = nullptr;
        QVector<Revision> m_revisions;
        Chip* m_entriesChip = nullptr;
        Chip* m_settingsChip = nullptr;
        Chip* m_recentChip = nullptr;
        Chip* m_restoreChip = nullptr;
        QDateEdit* m_fromDate = nullptr;
        QDateEdit* m_toDate = nullptr;
        QComboBox* m_datePreset = nullptr;
        QLabel* m_stateLabel = nullptr;
        QProgressBar* m_progress = nullptr;
        QToolButton* m_exportSelected = nullptr;
        QToolButton* m_deleteUnavailable = nullptr;
        QWidget* m_filterPanel = nullptr;
        QSet<QString> m_selectedIds;
        State m_state = State::Empty;
    };

} // namespace Material

#endif // KEEPASSXC_MATERIALHISTORYSCREEN_H
