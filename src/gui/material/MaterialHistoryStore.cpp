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

#include "MaterialHistoryStore.h"

#include "core/Database.h"
#include "core/Entry.h"
#include "core/Group.h"
#include "core/Metadata.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QStringList>
#include <QUuid>

namespace Material
{
    namespace
    {
        constexpr int HashCharacters = 12;
        const QString LogFileName = QStringLiteral("revisions.jsonl");
        const QString FingerprintDirectory = QStringLiteral("fingerprints");

        /** A short, stable, one way digest. Used so no UUID is written in the clear. */
        QString shortHash(const QString& value)
        {
            const QByteArray digest =
                QCryptographicHash::hash(value.toUtf8(), QCryptographicHash::Sha256).toHex();
            return QString::fromLatin1(digest.left(HashCharacters));
        }

        /**
         * The per-entry fingerprint of a database: one "entryHash:stateHash"
         * pair for every entry outside the recycle bin. Comparing two of these
         * answers how many entries were added, removed and edited without
         * anything of the entries themselves being kept.
         */
        QHash<QString, QString> fingerprintOf(const QSharedPointer<Database>& db, int* entryCount, int* groupCount)
        {
            QHash<QString, QString> fingerprint;
            int entries = 0;
            int groups = 0;

            for (const Group* group : db->rootGroup()->groupsRecursive(true)) {
                if (group->isRecycled()) {
                    continue;
                }
                ++groups;
                for (const Entry* entry : group->entries()) {
                    if (entry->isRecycled()) {
                        continue;
                    }
                    ++entries;
                    const QString uuid = entry->uuidToHex();
                    const QString state = QStringLiteral("%1|%2|%3")
                                              .arg(uuid)
                                              .arg(entry->timeInfo().lastModificationTime().toMSecsSinceEpoch())
                                              .arg(group->uuidToHex());
                    fingerprint.insert(shortHash(uuid), shortHash(state));
                }
            }

            // groupsRecursive(true) includes the root group, which is how the
            // statistics report counts too, so the two agree.
            *entryCount = entries;
            *groupCount = groups;
            return fingerprint;
        }

        QHash<QString, QString> readFingerprint(const QString& path)
        {
            QHash<QString, QString> fingerprint;
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly)) {
                return fingerprint;
            }
            const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
            file.close();
            if (!document.isArray()) {
                return fingerprint;
            }
            for (const QJsonValue& value : document.array()) {
                const QString pair = value.toString();
                const int separator = pair.indexOf(QLatin1Char(':'));
                if (separator > 0) {
                    fingerprint.insert(pair.left(separator), pair.mid(separator + 1));
                }
            }
            return fingerprint;
        }

        bool writeFingerprint(const QString& path, const QHash<QString, QString>& fingerprint)
        {
            QJsonArray array;
            for (auto it = fingerprint.constBegin(); it != fingerprint.constEnd(); ++it) {
                array.append(QStringLiteral("%1:%2").arg(it.key(), it.value()));
            }

            QSaveFile file(path);
            if (!file.open(QIODevice::WriteOnly)) {
                return false;
            }
            file.write(QJsonDocument(array).toJson(QJsonDocument::Compact));
            return file.commit();
        }

        /**
         * The kind token as it is written to the log. Kept out of tr() because
         * the log is data, not display: a line written in one language has to
         * still classify in another.
         */
        QString kindToken(RevisionKind kind)
        {
            switch (kind) {
            case RevisionKind::Entry:
                return QStringLiteral("entry");
            case RevisionKind::Group:
                return QStringLiteral("group");
            case RevisionKind::Settings:
                break;
            }
            return QStringLiteral("settings");
        }

        RevisionKind kindFromToken(const QString& token, const HistoryRevision& revision)
        {
            if (token == QLatin1String("entry")) {
                return RevisionKind::Entry;
            }
            if (token == QLatin1String("group")) {
                return RevisionKind::Group;
            }
            if (token == QLatin1String("settings")) {
                return RevisionKind::Settings;
            }
            // Lines written before the kind was recorded still carry their
            // entry deltas, so an entry revision can be recognised; a group
            // revision cannot, because no group delta was ever stored.
            return (revision.added + revision.removed + revision.edited) > 0 ? RevisionKind::Entry
                                                                            : RevisionKind::Settings;
        }

        HistoryRevision fromJson(const QJsonObject& object)
        {
            HistoryRevision revision;
            revision.id = object.value(QStringLiteral("id")).toString();
            revision.timestamp =
                QDateTime::fromString(object.value(QStringLiteral("time")).toString(), Qt::ISODateWithMs);
            revision.databaseName = object.value(QStringLiteral("name")).toString();
            revision.databasePath = object.value(QStringLiteral("path")).toString();
            revision.label = object.value(QStringLiteral("label")).toString();
            revision.entryCount = object.value(QStringLiteral("entries")).toInt();
            revision.groupCount = object.value(QStringLiteral("groups")).toInt();
            revision.added = object.value(QStringLiteral("added")).toInt();
            revision.removed = object.value(QStringLiteral("removed")).toInt();
            revision.edited = object.value(QStringLiteral("edited")).toInt();
            revision.kind = kindFromToken(object.value(QStringLiteral("kind")).toString(), revision);
            if (!revision.timestamp.isValid()) {
                revision.id.clear();
            }
            return revision;
        }

        QJsonObject toJson(const HistoryRevision& revision)
        {
            QJsonObject object;
            object.insert(QStringLiteral("id"), revision.id);
            object.insert(QStringLiteral("time"), revision.timestamp.toString(Qt::ISODateWithMs));
            object.insert(QStringLiteral("name"), revision.databaseName);
            object.insert(QStringLiteral("path"), revision.databasePath);
            object.insert(QStringLiteral("label"), revision.label);
            object.insert(QStringLiteral("entries"), revision.entryCount);
            object.insert(QStringLiteral("groups"), revision.groupCount);
            object.insert(QStringLiteral("added"), revision.added);
            object.insert(QStringLiteral("removed"), revision.removed);
            object.insert(QStringLiteral("edited"), revision.edited);
            object.insert(QStringLiteral("kind"), kindToken(revision.kind));
            return object;
        }
    } // namespace

    HistoryStore::HistoryStore() = default;

    HistoryStore* HistoryStore::instance()
    {
        static HistoryStore store;
        return &store;
    }

    QString HistoryStore::historyDirectory() const
    {
        const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        if (base.isEmpty()) {
            return {};
        }
        return QDir(base).filePath(QStringLiteral("history"));
    }

    QString HistoryStore::logPath() const
    {
        const QString directory = historyDirectory();
        if (directory.isEmpty()) {
            return {};
        }
        return QDir(directory).filePath(LogFileName);
    }

    QString HistoryStore::fingerprintPath(const QString& databasePath) const
    {
        const QString directory = historyDirectory();
        if (directory.isEmpty()) {
            return {};
        }
        const QString name = QStringLiteral("%1.json").arg(shortHash(QDir::fromNativeSeparators(databasePath)));
        return QDir(directory).filePath(QDir(FingerprintDirectory).filePath(name));
    }

    void HistoryStore::load()
    {
        if (m_loaded) {
            return;
        }
        m_loaded = true;

        const QString path = logPath();
        if (path.isEmpty()) {
            return;
        }

        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return;
        }
        while (!file.atEnd()) {
            const QByteArray line = file.readLine().trimmed();
            if (line.isEmpty()) {
                continue;
            }
            const QJsonDocument document = QJsonDocument::fromJson(line);
            if (!document.isObject()) {
                continue;
            }
            const HistoryRevision revision = fromJson(document.object());
            if (revision.isValid()) {
                m_revisions.append(revision);
            }
        }
    }

    bool HistoryStore::append(const HistoryRevision& revision)
    {
        const QString directory = historyDirectory();
        if (directory.isEmpty() || !QDir().mkpath(directory)) {
            return false;
        }

        QFile file(logPath());
        if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            return false;
        }
        file.write(QJsonDocument(toJson(revision)).toJson(QJsonDocument::Compact));
        file.write("\n");
        const bool written = file.flush();
        file.close();
        if (!written) {
            return false;
        }

        m_revisions.append(revision);
        emit revisionsChanged();
        return true;
    }

    bool HistoryStore::recordSave(const QSharedPointer<Database>& db)
    {
        if (!db || !db->rootGroup()) {
            return false;
        }
        load();

        const QString path = QDir::fromNativeSeparators(db->filePath());
        if (path.isEmpty()) {
            return false;
        }

        int entryCount = 0;
        int groupCount = 0;
        const QHash<QString, QString> current = fingerprintOf(db, &entryCount, &groupCount);

        const QString cachePath = fingerprintPath(path);
        const bool hadFingerprint = !cachePath.isEmpty() && QFileInfo::exists(cachePath);
        const QHash<QString, QString> previous = hadFingerprint ? readFingerprint(cachePath) : QHash<QString, QString>();

        HistoryRevision revision;
        revision.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        revision.timestamp = QDateTime::currentDateTime();
        revision.databasePath = path;
        revision.databaseName =
            db->metadata()->name().isEmpty() ? QFileInfo(path).completeBaseName() : db->metadata()->name();
        revision.entryCount = entryCount;
        revision.groupCount = groupCount;

        const HistoryRevision last = revisionsFor(path).value(0);

        if (!hadFingerprint || !last.isValid()) {
            revision.label = tr("First recorded save of this database - %n entry(s)", "", entryCount);
            // The first line establishes the entry set, so that is what it is about.
            revision.kind = RevisionKind::Entry;
        } else {
            for (auto it = current.constBegin(); it != current.constEnd(); ++it) {
                const auto match = previous.constFind(it.key());
                if (match == previous.constEnd()) {
                    ++revision.added;
                } else if (match.value() != it.value()) {
                    ++revision.edited;
                }
            }
            for (auto it = previous.constBegin(); it != previous.constEnd(); ++it) {
                if (!current.contains(it.key())) {
                    ++revision.removed;
                }
            }

            QStringList parts;
            if (revision.added > 0) {
                parts << tr("%n entry(s) added", "", revision.added);
            }
            if (revision.removed > 0) {
                parts << tr("%n entry(s) removed", "", revision.removed);
            }
            if (revision.edited > 0) {
                parts << tr("%n entry(s) edited", "", revision.edited);
            }

            const int groupDelta = groupCount - last.groupCount;
            if (groupDelta > 0) {
                parts << tr("%n group(s) added", "", groupDelta);
            } else if (groupDelta < 0) {
                parts << tr("%n group(s) removed", "", -groupDelta);
            }

            revision.label = parts.isEmpty() ? tr("Saved with no entry or group changes") : parts.join(tr(", "));

            // Entries win over groups: a save that moved an entry into a new
            // group is about the entry, not about the group that received it.
            if (revision.added + revision.removed + revision.edited > 0) {
                revision.kind = RevisionKind::Entry;
            } else if (groupDelta != 0) {
                revision.kind = RevisionKind::Group;
            } else {
                revision.kind = RevisionKind::Settings;
            }
        }

        if (!cachePath.isEmpty()) {
            QDir().mkpath(QFileInfo(cachePath).absolutePath());
            writeFingerprint(cachePath, current);
        }

        return append(revision);
    }

    QVector<HistoryRevision> HistoryStore::revisions() const
    {
        const_cast<HistoryStore*>(this)->load();

        QVector<HistoryRevision> newestFirst;
        newestFirst.reserve(m_revisions.size());
        for (int i = m_revisions.size() - 1; i >= 0; --i) {
            newestFirst.append(m_revisions.at(i));
        }
        return newestFirst;
    }

    QVector<HistoryRevision> HistoryStore::revisionsFor(const QString& databasePath) const
    {
        const QString wanted = QDir::fromNativeSeparators(databasePath);
        QVector<HistoryRevision> matching;
        for (const HistoryRevision& revision : revisions()) {
            if (revision.databasePath == wanted) {
                matching.append(revision);
            }
        }
        return matching;
    }

    HistoryRevision HistoryStore::revision(const QString& id) const
    {
        const_cast<HistoryStore*>(this)->load();
        for (const HistoryRevision& revision : m_revisions) {
            if (revision.id == id) {
                return revision;
            }
        }
        return {};
    }

    HistoryRevision HistoryStore::predecessor(const QString& id) const
    {
        const HistoryRevision target = revision(id);
        if (!target.isValid()) {
            return {};
        }

        HistoryRevision previous;
        for (const HistoryRevision& candidate : m_revisions) {
            if (candidate.id == id) {
                return previous;
            }
            if (candidate.databasePath == target.databasePath) {
                previous = candidate;
            }
        }
        return {};
    }

} // namespace Material
