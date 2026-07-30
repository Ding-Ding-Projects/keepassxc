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

#ifndef KEEPASSXC_MATERIALREPORTSFEED_H
#define KEEPASSXC_MATERIALREPORTSFEED_H

#include <QObject>
#include <QPair>
#include <QSharedPointer>
#include <QString>
#include <QVector>

class Database;

namespace Material
{
    class ReportsScreen;

    /**
     * What fills the reports destination.
     *
     * Runs the same password health check and the same DatabaseStats pass the
     * existing report widgets run, off the interface thread, and pushes the
     * result into a ReportsScreen: four summary tiles, the health findings and
     * the statistics table.
     *
     * Every number here comes out of the database. Breach exposure is the one
     * figure the design asks for that cannot be answered offline - it needs a
     * Have I Been Pwned lookup - so that tile is left out rather than filled
     * with a guess, and the screen's supporting line says why.
     *
     * The screen's search box filters the findings and the statistics; the
     * export button writes exactly what is on screen, filter included.
     */
    class ReportsFeed : public QObject
    {
        Q_OBJECT

    public:
        explicit ReportsFeed(ReportsScreen* screen, QObject* parent = nullptr);
        ~ReportsFeed() override;

        /** Point the feed at a database. A null pointer empties the screen. */
        void setDatabase(const QSharedPointer<Database>& db);

        /** Recompute from the current database and repaint the screen. */
        void refresh();

        /**
         * How many health findings the last pass produced. The rail reports
         * this as the Reports destination's sublabel.
         */
        int findingCount() const;

    signals:
        /** Emitted after every pass, with the new findingCount(). */
        void findingCountChanged(int count);

        /** A Fix button asked for the entry with @p uuidHex to be opened for editing. */
        void entryEditRequested(const QString& uuidHex);
        /** The header button asked for the full report widgets. */
        void detailedReportsRequested();

    private:
        /** One password health finding, in the order the health check ranks them. */
        struct Finding
        {
            QString uuid;
            QString title;
            QString path;
            QString reason;
            QString quality;
            int score = 0;
            bool bad = false;
            bool excluded = false;
            bool expired = false;
        };

        /** Everything one pass over the database answers. */
        struct Snapshot
        {
            bool valid = false;
            QString databaseName;
            QString databasePath;
            int entries = 0;
            int groups = 0;
            int healthy = 0;
            int shortPasswords = 0;
            int passkeys = 0;
            QVector<Finding> findings;
            QVector<QPair<QString, QString>> statistics;
        };

        static Snapshot* compute(QSharedPointer<Database> db);

        QVector<Finding> filteredFindings() const;
        QVector<QPair<QString, QString>> filteredStatistics() const;
        void apply();
        QString markdown() const;
        void exportMarkdown();

        ReportsScreen* m_screen = nullptr;
        QSharedPointer<Database> m_db;
        Snapshot m_snapshot;
        QString m_query;
        bool m_busy = false;
    };

} // namespace Material

#endif // KEEPASSXC_MATERIALREPORTSFEED_H
