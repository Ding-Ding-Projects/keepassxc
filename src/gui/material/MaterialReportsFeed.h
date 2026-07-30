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
     * existing report widgets run, and pushes the result into a ReportsScreen:
     * four summary tiles, the health findings and the statistics table.
     *
     * The pass reads the live group and entry tree, which belongs to the
     * interface thread, so it runs on the interface thread. refresh() says why
     * a worker is the wrong shape for this particular walk.
     *
     * Every number here comes out of the database. Breach exposure is the one
     * figure the design asks for that cannot be answered offline - it needs a
     * Have I Been Pwned lookup - so that tile keeps its place in the grid but
     * says it has not been checked rather than showing a guess.
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

        /**
         * Recompute from the current database and repaint the screen.
         *
         * A request that arrives while a pass is running is coalesced onto the
         * end of that pass rather than dropped, so the screen never settles on
         * counts the database has already moved past.
         */
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
            int entropy = 0; // raw password entropy in bits, what the chip reports
            bool bad = false;
            bool excluded = false;
            bool expired = false;
            bool reused = false; // the password is on more than one entry
            bool tooShort = false;
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
            int weakOrShort = 0; // passwords below the design's entropy bar
            int passkeys = 0;
            int relyingParties = 0; // distinct relying parties across the passkeys
            QVector<Finding> findings;
            QVector<QPair<QString, QString>> statistics;
        };

        /**
         * One pass over @p db, on the interface thread and never off it.
         * A null or emptied database answers an invalid snapshot, which is
         * what empties the screen.
         */
        static Snapshot compute(const QSharedPointer<Database>& db);

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
        bool m_refreshPending = false; // a refresh asked for while m_busy, owed once the pass lands
    };

} // namespace Material

#endif // KEEPASSXC_MATERIALREPORTSFEED_H
