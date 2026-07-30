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

#include "MaterialHistoryScreen.h"

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QSharedPointer>
#include <QString>
#include <QUuid>
#include <QVector>

class Database;
class Entry;

namespace Material
{
    /**
     * What fills the version history destination.
     *
     * Two sources are merged into one time-ordered list, because neither alone
     * is the history of a database. The database's own per-entry revisions -
     * Entry::historyItems(), the same ones the entry editor's History tab shows
     * - carry the previous values in full, so their rows can be compared with
     * the entry as it stands and put back. HistoryStore's append-only save log
     * carries what those cannot: that a save happened at all, that entries were
     * added or deleted, that a group appeared, that a save changed nothing an
     * entry would know about.
     *
     * The rows a source cannot back up are drawn without the action in
     * question rather than with one that explains itself: a save record offers
     * Diff and no Restore, and the lines shown when there is nothing to list
     * offer neither.
     */
    class HistoryFeed : public QObject
    {
        Q_OBJECT

    public:
        explicit HistoryFeed(HistoryScreen* screen, QObject* parent = nullptr);
        ~HistoryFeed() override;

        /**
         * Scope the list to one database, which is also where entry revisions
         * are read from. A null pointer leaves only the save log, which is all
         * that survives a database being closed or locked.
         */
        void setDatabase(const QSharedPointer<Database>& db);

        /** Re-read both sources and repaint the screen. */
        void refresh();

    private:
        /**
         * Where a row came from, so an action can find its subject again.
         *
         * Rows are looked up by id instead of holding an Entry*: the list is
         * rebuilt whenever the database changes, and a pointer kept across that
         * could outlive the entry it named.
         */
        struct Origin
        {
            enum class Kind
            {
                SaveLog, // a database-level record from HistoryStore
                EntryRevision, // one item of Entry::historyItems()
                SessionRestore // a restore this feed performed, held in memory
            };

            Kind kind = Kind::SaveLog;
            QString logId;
            QUuid entryUuid;
            int historyIndex = -1;
        };

        /** One change before the screen's filters have had a look at it. */
        struct Change
        {
            QDateTime when;
            /**
             * Whether the Entries chip keeps this row. Entry revisions,
             * attachment changes and restores are about one entry; save records
             * about groups or settings are about the file.
             */
            bool entryScoped = false;
            Revision row;
            Origin origin;
        };

        /**
         * A restore this feed performed since the window opened.
         *
         * Neither the database nor the save log marks a revision as a restore,
         * so these are the only restores that can be listed truthfully, and
         * only for as long as the window lives.
         */
        struct Restored
        {
            QString id;
            QDateTime when;
            QString databasePath;
            QString entryTitle;
            QDateTime revisionTime;
        };

        QVector<Change> entryRevisions() const;
        QVector<Change> savedRevisions() const;
        QVector<Change> sessionRestores() const;

        void showDiff(const QString& id);
        void restoreRevision(const QString& id);

        void showEntryDiff(const Origin& origin);
        void showSaveDiff(const QString& logId);
        void applyRestore(const QUuid& entryUuid, int historyIndex);

        /** The revision @p origin names, or nullptr once it is gone. */
        Entry* revisionAt(const Origin& origin, Entry** owner) const;

        HistoryScreen* m_screen = nullptr;
        QSharedPointer<Database> m_database;
        QString m_databasePath;
        QString m_query;
        QHash<QString, Origin> m_origins;
        QVector<Restored> m_restores;
    };

} // namespace Material

#endif // KEEPASSXC_MATERIALHISTORYFEED_H
