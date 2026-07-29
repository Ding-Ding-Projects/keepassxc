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

#ifndef KEEPASSXC_MATERIALHISTORYFEED_H
#define KEEPASSXC_MATERIALHISTORYFEED_H

#include <QObject>
#include <QSharedPointer>
#include <QString>

class Database;

namespace Material
{
    class HistoryScreen;

    /**
     * What fills the version history destination.
     *
     * Reads the append-only revision log kept by HistoryStore and turns it into
     * revision rows, newest first, scoped to the database in front when there
     * is one. The screen's search box filters them.
     *
     * Diff opens what the log actually recorded for a revision next to the one
     * before it. Restore says plainly that putting contents back is not wired,
     * because the log deliberately keeps no copy of the database - a feed that
     * quietly did nothing would be worse than one that explains itself.
     */
    class HistoryFeed : public QObject
    {
        Q_OBJECT

    public:
        explicit HistoryFeed(HistoryScreen* screen, QObject* parent = nullptr);
        ~HistoryFeed() override;

        /** Scope the list to one database. A null pointer shows every database. */
        void setDatabase(const QSharedPointer<Database>& db);

        /** Re-read the log and repaint the screen. */
        void refresh();

    private:
        void showDiff(const QString& id);
        void explainRestore(const QString& id);

        HistoryScreen* m_screen = nullptr;
        QString m_databasePath;
        QString m_query;
    };

} // namespace Material

#endif // KEEPASSXC_MATERIALHISTORYFEED_H
