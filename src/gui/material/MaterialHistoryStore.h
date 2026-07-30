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

#ifndef KEEPASSXC_MATERIALHISTORYSTORE_H
#define KEEPASSXC_MATERIALHISTORYSTORE_H

#include <QDateTime>
#include <QObject>
#include <QSharedPointer>
#include <QString>
#include <QVector>

class Database;

namespace Material
{
    /**
     * What a revision turned out to be about.
     *
     * Derived from what the save actually changed, never guessed: a save that
     * touched entries is an Entry revision, one that only changed the number of
     * groups is a Group revision, and a save that changed neither is what is
     * left - the database's own settings and metadata.
     */
    enum class RevisionKind
    {
        Entry,
        Group,
        Settings
    };

    /**
     * One recorded revision of a database.
     *
     * The counts are the state at the moment of the save; the three deltas are
     * how that state differs from the previously recorded revision of the same
     * file. @p label is the sentence built from those deltas when the revision
     * was written, kept verbatim so an old line still reads as it did.
     */
    struct HistoryRevision
    {
        QString id;
        QDateTime timestamp;
        QString databaseName;
        QString databasePath;
        QString label;
        RevisionKind kind = RevisionKind::Settings;
        int entryCount = 0;
        int groupCount = 0;
        int added = 0;
        int removed = 0;
        int edited = 0;

        bool isValid() const
        {
            return !id.isEmpty();
        }
    };

    /**
     * The application's own version history of the databases it saves.
     *
     * An append-only log of one JSON object per line, kept under
     * QStandardPaths::AppDataLocation - never inside the folder the user keeps
     * their database in, and never touching a repository of theirs. Nothing is
     * ever rewritten or deleted: correcting the record means appending to it.
     *
     * What is recorded is deliberately thin: when a save happened, which file
     * it was, how many entries and groups it then held, and how many entries
     * were added, removed or edited since the previous save. No titles, no
     * URLs, no passwords, no attachments. The per-entry fingerprints needed to
     * work out those three numbers live in a side cache of truncated hashes
     * that is rewritten on every save and is not part of the log.
     *
     * The store keeps no copy of the database contents, so it can describe a
     * revision but cannot put one back; callers must say so rather than
     * pretending otherwise.
     */
    class HistoryStore : public QObject
    {
        Q_OBJECT

    public:
        static HistoryStore* instance();

        /** Absolute path of the append-only log file. */
        QString logPath() const;

        /**
         * Append a revision for @p db, describing what changed since the last
         * one recorded for the same file. Answers false when the database has
         * no root group or the log could not be written.
         */
        bool recordSave(const QSharedPointer<Database>& db);

        /** Every recorded revision, newest first. */
        QVector<HistoryRevision> revisions() const;
        /** The revisions of one database file, newest first. */
        QVector<HistoryRevision> revisionsFor(const QString& databasePath) const;

        HistoryRevision revision(const QString& id) const;
        /** The revision recorded just before @p id for the same file, if any. */
        HistoryRevision predecessor(const QString& id) const;

    signals:
        /** A revision was appended. */
        void revisionsChanged();

    private:
        HistoryStore();

        QString historyDirectory() const;
        QString fingerprintPath(const QString& databasePath) const;

        void load();
        bool append(const HistoryRevision& revision);

        /** Oldest first, which is the order the log is written in. */
        QVector<HistoryRevision> m_revisions;
        bool m_loaded = false;
    };

} // namespace Material

#endif // KEEPASSXC_MATERIALHISTORYSTORE_H
