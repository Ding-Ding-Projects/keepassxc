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
#include <QPointer>
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
         *
         * The reference kept is weak: a feed must never be the reason an
         * unlocked database stays in memory.
         */
        void setDatabase(const QSharedPointer<Database>& db);

        /**
         * Re-read both sources and repaint the screen. Comparing revisions
         * touches every attachment of every entry, so this is driven by the
         * events that can actually change them, not by every keystroke.
         */
        void rebuild();

        /** Re-apply the search box and the filter chips to what was collected. */
        void refresh();

    private:
        /**
         * Where a row came from, so an action can find its subject again.
         *
         * Rows are looked up by id instead of holding a raw Entry*: the list is
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
            /**
             * The revision itself, guarded.
             *
             * A position in historyItems() cannot name a revision: the list is
             * oldest first and Entry::truncateHistory() drops from that end, so
             * every surviving position shifts and an index taken before a
             * truncation would resolve to a different revision afterwards -
             * restoring data the user never looked at. A QPointer names one
             * revision for as long as it exists and nothing once it is gone,
             * which is the only answer that cannot be wrong.
             */
            QPointer<Entry> revision;
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
         * A restore this feed performed on the database now in front.
         *
         * Neither the database nor the save log marks a revision as a restore,
         * so these are the only restores that can be listed truthfully. The
         * title is a decrypted value, so the list is dropped the moment that
         * database is locked, closed or swapped for another - it is not kept
         * for the life of the window.
         */
        struct Restored
        {
            QString id;
            QDateTime when;
            QString entryTitle;
            QDateTime revisionTime;
        };

        QVector<Change> entryRevisions() const;
        QVector<Change> savedRevisions() const;
        QVector<Change> sessionRestores() const;

        /**
         * Let go of everything read from the database.
         *
         * Locking hands the database widget a fresh locked database and calls
         * Database::releaseData() on the old one, which deletes the decrypted
         * tree. Nothing announces that as a lock - releaseData() silences
         * Database::modified() before it gets there - but the root group is
         * destroyed, and that is the moment the entries and their titles stop
         * existing. Following it is what empties this surface on lock. A reload
         * or a merge releases the old database the same way, so the feed then
         * lists only the save log until the window points it at the new one.
         */
        void forgetDatabase();

        void showDiff(const QString& id);
        void restoreRevision(const QString& id);

        void showEntryDiff(const Origin& origin);
        void showSaveDiff(const QString& logId);
        void applyRestore(const Origin& origin);

        /**
         * The revision @p origin names, or nullptr once it is gone.
         *
         * A revision counts as gone unless it is still one of @p owner's own
         * history items: a guarded pointer says the object is alive, not whose
         * history it is in.
         */
        Entry* revisionAt(const Origin& origin, Entry** owner) const;

        HistoryScreen* m_screen = nullptr;
        /** Weak, so a closed or locked database is released on the spot. */
        QWeakPointer<Database> m_database;
        /**
         * Live while a database is in front: its edits, its renames and the
         * teardown that follows a lock all have to reach the list, and they are
         * dropped together when another database takes its place.
         */
        QVector<QMetaObject::Connection> m_databaseWatch;
        QString m_databasePath;
        QString m_query;
        /** Everything both sources hold, newest first, before any filtering. */
        QVector<Change> m_changes;
        QHash<QString, Origin> m_origins;
        QVector<Restored> m_restores;
    };

} // namespace Material

#endif // KEEPASSXC_MATERIALHISTORYFEED_H
