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
        /** Opaque SHA-256 identity; never a filesystem path. */
        QString databasePath;
        QString label;
        RevisionKind kind = RevisionKind::Settings;
        int entryCount = 0;
        int groupCount = 0;
        int added = 0;
        int removed = 0;
        int edited = 0;
        QString snapshotPath;
        QString snapshotSha256;

        bool isValid() const
        {
            return !id.isEmpty();
        }
    };

    /**
     * The application's own record of the databases it saves.
     *
     * An isolated local Git repository under QStandardPaths::AppDataLocation,
     * never inside a user database folder. Each event is one commit and no
     * remote is configured. Restore records append like every other event;
     * existing commits are never rewritten.
     *
     * What is recorded is deliberately thin: when a save happened, an opaque
     * database digest, how many entries and groups it then held, and how many entries
     * were added, removed or edited since the previous save. No titles, no
     * URLs, no passwords, no attachments. The per-entry fingerprints needed to
     * work out those three numbers live in a side cache of truncated hashes
     * that are committed atomically with the revision state.
     *
     * That thinness is the point rather than a gap to be closed later. This
     * repository sits in the clear on disk while the database it describes is
     * encrypted, so nothing that is worth encrypting may be written into it -
     * not an entry's contents, not a password, not a byte of an attachment.
     *
     * Each save also keeps the encrypted KDBX bytes in that database's own
     * isolated local repository. History can therefore restore entries that
     * disappeared between two saves without exposing plaintext or overwriting
     * entries that still exist. Per-entry revisions remain the finer-grained
     * route for restoring field edits.
     */
    class HistoryStore : public QObject
    {
        Q_OBJECT

    public:
        static HistoryStore* instance();

        /** Test/diagnostic constructor. Production uses instance(). */
        explicit HistoryStore(const QString& storageRoot = {}, const QString& gitExecutable = {});

        /** Absolute path of the read-only legacy JSONL source, if present. */
        QString logPath() const;

        /**
         * Append a revision for @p db, describing what changed since the last
         * one recorded for the same file. Answers false when the database has
         * no root group or the log could not be written.
         */
        bool recordSave(const QSharedPointer<Database>& db);
        /** Record an already-completed redacted event such as restore/import/bulk. */
        bool recordEvent(const QSharedPointer<Database>& db, const QString& redactedLabel, RevisionKind kind);
        bool recordSettingsEvent(const QString& redactedLabel);

        /** Every recorded revision, newest first. */
        QVector<HistoryRevision> revisions() const;
        /** Bounded newest-first page. */
        QVector<HistoryRevision> revisions(int offset, int limit) const;
        /** The revisions of one database file, newest first. */
        QVector<HistoryRevision> revisionsFor(const QString& databasePath) const;

        HistoryRevision revision(const QString& id) const;
        /** The revision recorded just before @p id for the same file, if any. */
        HistoryRevision predecessor(const QString& id) const;
        /** Validated encrypted KDBX snapshot bytes, or empty with an error. */
        QByteArray snapshot(const QString& revisionId, QString* error = nullptr) const;

        /**
         * Restore entries that disappeared in @p revisionId from its previous
         * encrypted KDBX snapshot. Existing entries are never overwritten.
         */
        int restoreDeletedEntries(const QString& revisionId,
                                  const QSharedPointer<Database>& database,
                                  QString* error = nullptr);

        /** Isolated local Git repository for one database identity. */
        QString databaseRepositoryPath(const QString& databasePath) const;

    signals:
        /**
         * A revision was appended. The navigation rail counts revisions from
         * this, and HistoryFeed rebuilds its list, so it has to keep firing on
         * every successful recordSave().
         */
        void revisionsChanged();
        /** Persistence failed; the database operation itself must continue. */
        void writeFailed(const QString& recoveryMessage);

    private:
        QString historyDirectory() const;
        QString repositoryPath() const;
        QString fingerprintPath(const QString& databasePath) const;

        void load();
        bool ensureRepository();
        bool commitTransaction(const HistoryRevision& revision,
                               const QByteArray& fingerprint,
                               const QByteArray& encryptedSnapshot = {});
        bool migrateLegacy();
        bool commitDatabaseRepository(const HistoryRevision& revision, const QByteArray& encryptedSnapshot);

        /** Oldest first, which is the order the log is written in. */
        QVector<HistoryRevision> m_revisions;
        bool m_loaded = false;
        QString m_storageRoot;
        QString m_gitExecutable;
    };

} // namespace Material

#endif // KEEPASSXC_MATERIALHISTORYSTORE_H
