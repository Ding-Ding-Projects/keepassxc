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
#include <QString>
#include <QVector>

class QGridLayout;
class QVBoxLayout;

namespace Material
{
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
        explicit ReportsScreen(QWidget* parent = nullptr);
        ~ReportsScreen() override;

        void setStatCards(const QVector<StatCard>& cards);
        void setHealthRows(const QVector<HealthRow>& rows);
        void setStatistics(const QVector<QPair<QString, QString>>& statistics);

    signals:
        /** The Fix button of the health row carrying @p id was pressed. */
        void fixRequested(const QString& id);
        void exportRequested();

    private:
        void rebuild();
        void rebuildStatCards();
        void rebuildHealthRows();
        void rebuildStatistics();

        QGridLayout* m_statGrid = nullptr;
        QVBoxLayout* m_healthLayout = nullptr;
        QVBoxLayout* m_statisticsLayout = nullptr;
        Card* m_healthCard = nullptr;
        Card* m_statisticsCard = nullptr;
        QVector<StatCard> m_statCards;
        QVector<HealthRow> m_healthRows;
        QVector<QPair<QString, QString>> m_statistics;
    };

} // namespace Material

#endif // KEEPASSXC_MATERIALREPORTSSCREEN_H
