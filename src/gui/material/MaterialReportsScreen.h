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

#ifndef KEEPASSXC_MATERIALREPORTSSCREEN_H
#define KEEPASSXC_MATERIALREPORTSSCREEN_H

#include "MaterialScreen.h"
#include "MaterialTheme.h"

#include <QPair>
#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

class QGridLayout;
class QVBoxLayout;
class QLabel;
class QProgressBar;
class QToolButton;
class QResizeEvent;

namespace Material
{
    class Select;

    class Card;

    /**
     * One tile of the four column summary grid.
     *
     * The tile takes its background and foreground from the status family of
     * @p status; Health::Unknown is the informational tint, primaryContainer,
     * which is what the passkey counter uses in the design.
     */
    struct StatCard
    {
        QString label;
        QString value;
        QString sub;
        Health status = Health::Unknown;
    };

    /** One finding in the password health card. */
    struct HealthRow
    {
        QString id;
        QString symbol;
        QString title;
        QString reason;
        QString score;
        Health status = Health::Unknown;
    };

    struct ReportCard
    {
        QString id;
        QString title;
        QString blurb;
        QString count;
        QString symbol;
        Health status = Health::Unknown;
        QVector<HealthRow> rows;
        QVector<QPair<QString, QString>> statistics;
        bool unavailable = false;
    };

    /**
     * The reports destination.
     *
     * A grid of tinted stat cards over a 1.4 : 1 pair of cards: the password
     * health findings on the left, the database statistics and the export
     * action on the right. The screen holds no reporting logic of its own; a
     * caller pushes the three models in and listens for the two signals.
     */
    class ReportsScreen : public Screen
    {
        Q_OBJECT

    public:
        enum class State { Empty, Loading, Populated, Progress, Warning, Error };
        explicit ReportsScreen(QWidget* parent = nullptr);
        ~ReportsScreen() override;

        void setStatCards(const QVector<StatCard>& cards);
        void setReportCards(const QVector<ReportCard>& cards);
        QStringList reportCardIds() const;
        bool isCardExpanded(const QString& id) const;
        void setCardExpanded(const QString& id, bool expanded);
        void setState(State state, const QString& message = {}, int progress = -1);
        State state() const;
        QStringList selectedFindingIds() const;
        void setSearchValidation(bool valid, const QString& message = {});

    signals:
        /** The Fix button of the health row carrying @p id was pressed. */
        void fixRequested(const QString& id);
        void exportRequested();
        void bulkExportRequested(const QStringList& ids);
        void categoryChanged(const QString& category);

    protected:
        void resizeEvent(QResizeEvent* event) override;

    private:
        void rebuild();
        void rebuildStatCards();
        void rebuildReportCards();
        void applyResponsiveLayout();
        void updateBulkActions();

        QGridLayout* m_statGrid = nullptr;
        QGridLayout* m_reportCardsLayout = nullptr;
        QWidget* m_reportCardsHost = nullptr;
        QLabel* m_stateLabel = nullptr;
        QProgressBar* m_progress = nullptr;
        QToolButton* m_bulkExport = nullptr;
        Select* m_category = nullptr;
        State m_state = State::Empty;
        QSet<QString> m_selectedIds;
        QVector<ReportCard> m_reportCards;
        QHash<QString, bool> m_expandedCards;
        QVector<StatCard> m_statCards;
    };

} // namespace Material

#endif // KEEPASSXC_MATERIALREPORTSSCREEN_H
