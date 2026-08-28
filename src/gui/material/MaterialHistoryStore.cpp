/* Copyright (C) 2026 KeePassXC Team <team@keepassxc.org>
 * SPDX-License-Identifier: GPL-2.0-or-later OR GPL-3.0-only */

#include "MaterialHistoryStore.h"
#include "core/Database.h"
#include "core/Entry.h"
#include "core/Group.h"
#include "format/KeePass2.h"
#include "format/KeePass2Reader.h"

#include <QCryptographicHash>
#include <QBuffer>
#include <QDir>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QProcess>
#include <QStandardPaths>
#include <QSaveFile>
#include <QThread>
#include <QtEndian>
#include <QUuid>

#include <algorithm>

namespace Material
{
    namespace
    {
        constexpr int DeadlineMs = 10000;
        constexpr int ReplaceAttempts = 7;
        constexpr int MaximumPageSize = 500;
        const QString StateName = QStringLiteral("revisions.json");
        const QString FingerprintsName = QStringLiteral("fingerprints");
        const QString SnapshotsName = QStringLiteral("snapshots");
        constexpr int LockTimeoutMs = 3000;

        struct ProcessResult { bool started = false; bool timedOut = false; int exitCode = -1; QByteArray out; QByteArray err; bool ok() const { return started && !timedOut && exitCode == 0; } };

        ProcessResult git(const QString& executable, const QString& repo, const QStringList& arguments)
        {
            ProcessResult result;
            if (executable.isEmpty()) return result;
            QProcess process;
            auto environment = QProcessEnvironment::systemEnvironment();
#ifdef Q_OS_WIN
            environment.insert(QStringLiteral("GIT_CONFIG_GLOBAL"), QStringLiteral("NUL"));
            environment.insert(QStringLiteral("GIT_CONFIG_SYSTEM"), QStringLiteral("NUL"));
#else
            environment.insert(QStringLiteral("GIT_CONFIG_GLOBAL"), QStringLiteral("/dev/null"));
            environment.insert(QStringLiteral("GIT_CONFIG_SYSTEM"), QStringLiteral("/dev/null"));
#endif
            environment.insert(QStringLiteral("GIT_AUTHOR_NAME"), QStringLiteral("KeePassXC History"));
            environment.insert(QStringLiteral("GIT_AUTHOR_EMAIL"), QStringLiteral("history@localhost"));
            environment.insert(QStringLiteral("GIT_COMMITTER_NAME"), QStringLiteral("KeePassXC History"));
            environment.insert(QStringLiteral("GIT_COMMITTER_EMAIL"), QStringLiteral("history@localhost"));
            process.setProcessEnvironment(environment);
            QStringList args;
            if (!repo.isEmpty()) args << QStringLiteral("-C") << repo;
            args << arguments;
            process.start(executable, args, QIODevice::ReadOnly);
            result.started = process.waitForStarted(DeadlineMs);
            if (!result.started) return result;
            if (!process.waitForFinished(DeadlineMs)) { result.timedOut = true; process.kill(); process.waitForFinished(); }
            result.exitCode = process.exitCode();
            result.out = process.readAllStandardOutput();
            result.err = process.readAllStandardError();
            return result;
        }

        QString digest(const QString& value, int characters = 64)
        {
            return QString::fromLatin1(QCryptographicHash::hash(value.toUtf8(), QCryptographicHash::Sha256).toHex().left(characters));
        }

        QString byteDigest(const QByteArray& value)
        {
            return QString::fromLatin1(QCryptographicHash::hash(value, QCryptographicHash::Sha256).toHex());
        }

        QString databaseId(const QString& path)
        {
            return digest(QDir::cleanPath(QDir::fromNativeSeparators(path)));
        }

        bool isKdbx(const QByteArray& bytes)
        {
            if (bytes.size() < 12) return false;
            const auto* data = reinterpret_cast<const uchar*>(bytes.constData());
            const quint32 first = qFromLittleEndian<quint32>(data);
            const quint32 second = qFromLittleEndian<quint32>(data + 4);
            return first == KeePass2::SIGNATURE_1 && second == KeePass2::SIGNATURE_2;
        }

        QHash<QString, QString> fingerprintOf(const QSharedPointer<Database>& db, int* entries, int* groups)
        {
            QHash<QString, QString> result;
            *entries = 0; *groups = 0;
            for (const Group* group : db->rootGroup()->groupsRecursive(true)) {
                if (group->isRecycled()) continue;
                ++*groups;
                for (const Entry* entry : group->entries()) {
                    if (entry->isRecycled()) continue;
                    ++*entries;
                    const QString uuid = entry->uuidToHex();
                    const QString state = QStringLiteral("%1|%2|%3").arg(uuid).arg(entry->timeInfo().lastModificationTime().toMSecsSinceEpoch()).arg(group->uuidToHex());
                    result.insert(digest(uuid, 24), digest(state, 24));
                }
            }
            return result;
        }

        QByteArray fingerprintJson(const QHash<QString, QString>& fingerprint)
        {
            QJsonArray array;
            QStringList keys = fingerprint.keys(); keys.sort();
            for (const auto& key : keys) array.append(QStringLiteral("%1:%2").arg(key, fingerprint.value(key)));
            return QJsonDocument(array).toJson(QJsonDocument::Compact);
        }

        QHash<QString, QString> readFingerprint(const QString& path)
        {
            QHash<QString, QString> result; QFile file(path);
            if (!file.open(QIODevice::ReadOnly)) return result;
            for (const auto& value : QJsonDocument::fromJson(file.readAll()).array()) {
                const QString pair = value.toString(); const int colon = pair.indexOf(QLatin1Char(':'));
                if (colon > 0) result.insert(pair.left(colon), pair.mid(colon + 1));
            }
            return result;
        }

        QString kindToken(RevisionKind kind) { return kind == RevisionKind::Entry ? QStringLiteral("entry") : kind == RevisionKind::Group ? QStringLiteral("group") : QStringLiteral("settings"); }
        RevisionKind kindFromToken(const QString& token) { return token == QLatin1String("entry") ? RevisionKind::Entry : token == QLatin1String("group") ? RevisionKind::Group : RevisionKind::Settings; }

        QJsonObject toJson(const HistoryRevision& v)
        {
            QJsonObject object{{QStringLiteral("id"), v.id}, {QStringLiteral("time"), v.timestamp.toUTC().toString(Qt::ISODateWithMs)},
                               {QStringLiteral("databaseId"), v.databasePath}, {QStringLiteral("name"), v.databaseName},
                               {QStringLiteral("label"), v.label}, {QStringLiteral("kind"), kindToken(v.kind)},
                               {QStringLiteral("entries"), v.entryCount}, {QStringLiteral("groups"), v.groupCount},
                               {QStringLiteral("added"), v.added}, {QStringLiteral("removed"), v.removed}, {QStringLiteral("edited"), v.edited}};
            if (!v.snapshotPath.isEmpty()) { object.insert(QStringLiteral("snapshot"), v.snapshotPath); object.insert(QStringLiteral("snapshotSha256"), v.snapshotSha256); }
            return object;
        }

        HistoryRevision fromJson(const QJsonObject& o)
        {
            HistoryRevision v; v.id = o.value(QStringLiteral("id")).toString();
            v.timestamp = QDateTime::fromString(o.value(QStringLiteral("time")).toString(), Qt::ISODateWithMs);
            v.databasePath = o.value(QStringLiteral("databaseId")).toString(); v.databaseName = o.value(QStringLiteral("name")).toString();
            v.label = o.value(QStringLiteral("label")).toString(); v.kind = kindFromToken(o.value(QStringLiteral("kind")).toString());
            v.entryCount = o.value(QStringLiteral("entries")).toInt(); v.groupCount = o.value(QStringLiteral("groups")).toInt();
            v.added = o.value(QStringLiteral("added")).toInt(); v.removed = o.value(QStringLiteral("removed")).toInt(); v.edited = o.value(QStringLiteral("edited")).toInt();
            v.snapshotPath = o.value(QStringLiteral("snapshot")).toString(); v.snapshotSha256 = o.value(QStringLiteral("snapshotSha256")).toString();
            if (!v.timestamp.isValid() || v.databasePath.size() != 64) v.id.clear(); return v;
        }

        QByteArray revisionsJson(const QVector<HistoryRevision>& values)
        {
            QJsonArray array; for (const auto& value : values) array.append(toJson(value));
            return QJsonDocument(array).toJson(QJsonDocument::Compact);
        }

        bool atomicReplace(const QString& target, const QByteArray& bytes)
        {
            QDir().mkpath(QFileInfo(target).absolutePath());
            for (int attempt = 0; attempt < ReplaceAttempts; ++attempt) {
                QSaveFile file(target);
                file.setDirectWriteFallback(false);
                if (file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit()) return true;
                file.cancelWriting();
                QThread::msleep(static_cast<unsigned long>(10 * (attempt + 1)));
            }
            return false;
        }
    }

    HistoryStore::HistoryStore(const QString& storageRoot, const QString& gitExecutable)
        : m_storageRoot(storageRoot), m_gitExecutable(gitExecutable.isEmpty() ? QStandardPaths::findExecutable(QStringLiteral("git")) : gitExecutable) {}

    HistoryStore* HistoryStore::instance() { static HistoryStore store; return &store; }

    QString HistoryStore::historyDirectory() const
    {
        const QString base = m_storageRoot.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) : m_storageRoot;
        return base.isEmpty() ? QString() : QDir(base).filePath(QStringLiteral("history"));
    }
    QString HistoryStore::repositoryPath() const { return QDir(historyDirectory()).filePath(QStringLiteral("repository")); }
    QString HistoryStore::logPath() const { return QDir(historyDirectory()).filePath(QStringLiteral("revisions.jsonl")); }
    QString HistoryStore::fingerprintPath(const QString& path) const { const QString id = path.size() == 64 ? path : databaseId(path); return QDir(repositoryPath()).filePath(QDir(FingerprintsName).filePath(id + QStringLiteral(".json"))); }
    QString HistoryStore::databaseRepositoryPath(const QString& path) const
    {
        const QString id = path.size() == 64 ? path : databaseId(path);
        return QDir(historyDirectory()).filePath(QStringLiteral("databases/%1/repository").arg(id));
    }

    bool HistoryStore::commitDatabaseRepository(const HistoryRevision& revision, const QByteArray& encryptedSnapshot)
    {
        if (encryptedSnapshot.isEmpty() || revision.databasePath.size() != 64) return false;
        const QString repo = databaseRepositoryPath(revision.databasePath);
        if (!QDir().mkpath(repo) || m_gitExecutable.isEmpty()) return false;
        QLockFile lock(QDir(repo).filePath(QStringLiteral("repository.lock")));
        lock.setStaleLockTime(0);
        if (!lock.tryLock(LockTimeoutMs)) return false;
        if (!QFileInfo::exists(QDir(repo).filePath(QStringLiteral(".git")))) {
            if (!git(m_gitExecutable, repo, {QStringLiteral("init"), QStringLiteral("--quiet")}).ok()) return false;
            if (!git(m_gitExecutable, repo, {QStringLiteral("config"), QStringLiteral("user.name"), QStringLiteral("KeePassXC History")}).ok()) return false;
            if (!git(m_gitExecutable, repo, {QStringLiteral("config"), QStringLiteral("user.email"), QStringLiteral("history@localhost")}).ok()) return false;
        }
        const QString snapshotName = QStringLiteral("snapshots/%1.kdbx").arg(revision.id);
        const QString metadataName = QStringLiteral("revisions/%1.json").arg(revision.id);
        if (!atomicReplace(QDir(repo).filePath(snapshotName), encryptedSnapshot)
            || !atomicReplace(QDir(repo).filePath(metadataName), QJsonDocument(toJson(revision)).toJson(QJsonDocument::Compact))) {
            return false;
        }
        const auto add = git(m_gitExecutable,
                             repo,
                             {QStringLiteral("add"), QStringLiteral("--"), snapshotName, metadataName});
        const auto commit = add.ok()
                                ? git(m_gitExecutable,
                                      repo,
                                      {QStringLiteral("commit"),
                                       QStringLiteral("--quiet"),
                                       QStringLiteral("-m"),
                                       QStringLiteral("Record encrypted database revision %1").arg(revision.id)})
                                : ProcessResult{};
        return commit.ok();
    }

    bool HistoryStore::ensureRepository()
    {
        const QString repo = repositoryPath();
        if (repo.isEmpty() || !QDir().mkpath(repo) || m_gitExecutable.isEmpty()) return false;
        QLockFile initializationLock(QDir(repo).filePath(QStringLiteral("initialization.lock")));
        initializationLock.setStaleLockTime(0);
        if (!initializationLock.tryLock(LockTimeoutMs)) return false;
        if (!QFileInfo::exists(QDir(repo).filePath(QStringLiteral(".git")))) {
            if (!git(m_gitExecutable, repo, {QStringLiteral("init"), QStringLiteral("--quiet")}).ok()) return false;
            if (!git(m_gitExecutable, repo, {QStringLiteral("config"), QStringLiteral("user.name"), QStringLiteral("KeePassXC History")}).ok()) return false;
            if (!git(m_gitExecutable, repo, {QStringLiteral("config"), QStringLiteral("user.email"), QStringLiteral("history@localhost")}).ok()) return false;
        }
        return true;
    }

    bool HistoryStore::migrateLegacy()
    {
        if (!QFileInfo::exists(logPath()) || QFileInfo::exists(QDir(repositoryPath()).filePath(StateName))) return true;
        QLockFile lock(QDir(repositoryPath()).filePath(QStringLiteral("ledger.lock")));
        lock.setStaleLockTime(0);
        if (!lock.tryLock(LockTimeoutMs)) return false;
        QFile source(logPath()); if (!source.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
        QVector<HistoryRevision> imported;
        while (!source.atEnd()) {
            const auto o = QJsonDocument::fromJson(source.readLine().trimmed()).object(); if (o.isEmpty()) continue;
            HistoryRevision v; v.id = o.value(QStringLiteral("id")).toString(); v.timestamp = QDateTime::fromString(o.value(QStringLiteral("time")).toString(), Qt::ISODateWithMs);
            const QString oldPath = o.value(QStringLiteral("path")).toString(); v.databasePath = databaseId(oldPath); v.databaseName = QStringLiteral("Database %1").arg(v.databasePath.left(8));
            v.label = o.value(QStringLiteral("label")).toString(); v.kind = kindFromToken(o.value(QStringLiteral("kind")).toString());
            v.entryCount = o.value(QStringLiteral("entries")).toInt(); v.groupCount = o.value(QStringLiteral("groups")).toInt(); v.added = o.value(QStringLiteral("added")).toInt(); v.removed = o.value(QStringLiteral("removed")).toInt(); v.edited = o.value(QStringLiteral("edited")).toInt();
            if (!v.id.isEmpty() && v.timestamp.isValid()) imported.append(v);
        }
        if (imported.isEmpty()) return true;
        m_revisions = imported;
        if (!atomicReplace(QDir(repositoryPath()).filePath(StateName), revisionsJson(m_revisions))
            || !git(m_gitExecutable, repositoryPath(), {QStringLiteral("add"), QStringLiteral("--"), StateName}).ok()
            || !git(m_gitExecutable, repositoryPath(), {QStringLiteral("commit"), QStringLiteral("--quiet"), QStringLiteral("-m"), QStringLiteral("Import legacy history")}).ok()) {
            m_revisions.clear(); git(m_gitExecutable, repositoryPath(), {QStringLiteral("reset"), QStringLiteral("--hard"), QStringLiteral("HEAD")}); return false;
        }
        return true;
    }

    void HistoryStore::load()
    {
        if (m_loaded) return; m_loaded = true;
        if (!ensureRepository() || !migrateLegacy()) return;
        m_revisions.clear();
        QFile file(QDir(repositoryPath()).filePath(StateName)); if (!file.open(QIODevice::ReadOnly)) return;
        for (const auto& value : QJsonDocument::fromJson(file.readAll()).array()) { const auto revision = fromJson(value.toObject()); if (revision.isValid()) m_revisions.append(revision); }
    }

    bool HistoryStore::commitTransaction(const HistoryRevision& revision,
                                         const QByteArray& fingerprint,
                                         const QByteArray& encryptedSnapshot)
    {
        if (!ensureRepository()) { qWarning() << "Local history repository initialization failed"; return false; }
        QLockFile lock(QDir(repositoryPath()).filePath(QStringLiteral("ledger.lock")));
        lock.setStaleLockTime(0);
        if (!lock.tryLock(LockTimeoutMs)) { qWarning() << "Local history lock acquisition failed" << lock.error(); return false; }
        QVector<HistoryRevision> committed;
        QFile committedState(QDir(repositoryPath()).filePath(StateName));
        if (committedState.open(QIODevice::ReadOnly)) {
            for (const auto& value : QJsonDocument::fromJson(committedState.readAll()).array()) {
                const auto existing = fromJson(value.toObject());
                if (existing.isValid()) committed.append(existing);
            }
            committedState.close();
        }
        QVector<HistoryRevision> next = committed; next.append(revision);
        const QString statePath = QDir(repositoryPath()).filePath(StateName);
        const QString fpPath = fingerprintPath(revision.databasePath);
        const QString snapshotAbsolute = revision.snapshotPath.isEmpty() ? QString() : QDir(repositoryPath()).filePath(revision.snapshotPath);
        const auto rollback = [this, statePath, fpPath, snapshotAbsolute] {
            if (git(m_gitExecutable, repositoryPath(), {QStringLiteral("rev-parse"), QStringLiteral("--verify"), QStringLiteral("HEAD")}).ok()) {
                git(m_gitExecutable, repositoryPath(), {QStringLiteral("reset"), QStringLiteral("--hard"), QStringLiteral("HEAD")});
            } else {
                QFile::remove(statePath);
                QFile::remove(fpPath);
            }
            if (!snapshotAbsolute.isEmpty()) QFile::remove(snapshotAbsolute);
        };
        QStringList staged{StateName, FingerprintsName};
        if (!atomicReplace(statePath, revisionsJson(next))) { qWarning() << "Local history revision transaction failed"; rollback(); return false; }
        if (!atomicReplace(fpPath, fingerprint)) { qWarning() << "Local history fingerprint transaction failed"; rollback(); return false; }
        if (!revision.snapshotPath.isEmpty()) {
            const QString snapshotPath = QDir(repositoryPath()).filePath(revision.snapshotPath);
            if (encryptedSnapshot.isEmpty() || !atomicReplace(snapshotPath, encryptedSnapshot)) { qWarning() << "Local history snapshot transaction failed" << snapshotPath << encryptedSnapshot.size(); rollback(); return false; }
            staged << SnapshotsName;
        }
        const auto add = git(m_gitExecutable, repositoryPath(), QStringList{QStringLiteral("add"), QStringLiteral("--")} + staged);
        const auto commit = add.ok() ? git(m_gitExecutable, repositoryPath(), {QStringLiteral("commit"), QStringLiteral("--quiet"), QStringLiteral("-m"), QStringLiteral("Record history event %1").arg(revision.id)}) : ProcessResult{};
        if (!commit.ok()) { qWarning() << "Local history Git transaction failed" << add.err << commit.err; rollback(); return false; }
        m_revisions = next; emit revisionsChanged(); return true;
    }

    bool HistoryStore::recordSave(const QSharedPointer<Database>& db)
    {
        if (!db || !db->rootGroup() || db->filePath().isEmpty()) return false;
        load(); const QString opaqueId = databaseId(db->filePath());
        int entryCount = 0, groupCount = 0; const auto current = fingerprintOf(db, &entryCount, &groupCount);
        const bool hadFingerprint = QFileInfo::exists(fingerprintPath(opaqueId)); const auto previous = readFingerprint(fingerprintPath(opaqueId));
        HistoryRevision r; r.id = QUuid::createUuid().toString(QUuid::WithoutBraces); r.timestamp = QDateTime::currentDateTimeUtc(); r.databasePath = opaqueId;
        r.databaseName = QStringLiteral("Database %1").arg(opaqueId.left(8)); r.entryCount = entryCount; r.groupCount = groupCount;
        const auto last = revisionsFor(db->filePath()).value(0);
        if (!hadFingerprint || !last.isValid()) { r.kind = RevisionKind::Entry; r.label = tr("First recorded save of this database - %n entry(s)", "", entryCount); }
        else {
            for (auto it = current.cbegin(); it != current.cend(); ++it) { const auto old = previous.constFind(it.key()); if (old == previous.cend()) ++r.added; else if (old.value() != it.value()) ++r.edited; }
            for (auto it = previous.cbegin(); it != previous.cend(); ++it) if (!current.contains(it.key())) ++r.removed;
            const int groupDelta = groupCount - last.groupCount; QStringList parts;
            if (r.added) parts << tr("%n entry(s) added", "", r.added); if (r.removed) parts << tr("%n entry(s) removed", "", r.removed); if (r.edited) parts << tr("%n entry(s) edited", "", r.edited);
            if (groupDelta > 0) parts << tr("%n group(s) added", "", groupDelta); if (groupDelta < 0) parts << tr("%n group(s) removed", "", -groupDelta);
            r.label = parts.isEmpty() ? tr("Saved with no entry or group changes") : parts.join(tr(", "));
            r.kind = r.added + r.removed + r.edited > 0 ? RevisionKind::Entry : groupDelta != 0 ? RevisionKind::Group : RevisionKind::Settings;
        }
        QByteArray encryptedSnapshot;
        QFile databaseFile(db->filePath());
        if (databaseFile.open(QIODevice::ReadOnly)) {
            encryptedSnapshot = databaseFile.readAll();
            if (isKdbx(encryptedSnapshot)) {
                r.snapshotPath = QStringLiteral("%1/%2/%3.kdbx").arg(SnapshotsName, opaqueId, r.id);
                r.snapshotSha256 = byteDigest(encryptedSnapshot);
            } else {
                encryptedSnapshot.clear();
            }
        }
        if (!commitTransaction(r, fingerprintJson(current), encryptedSnapshot)
            || !commitDatabaseRepository(r, encryptedSnapshot)) {
            emit writeFailed(tr("Local history could not be recorded. The database save completed; retry after checking Git and application-data storage."));
            return false;
        }
        return true;
    }

    bool HistoryStore::recordEvent(const QSharedPointer<Database>& db, const QString& redactedLabel, RevisionKind kind)
    {
        if (!db || !db->rootGroup() || db->filePath().isEmpty() || redactedLabel.trimmed().isEmpty()) return false;
        load();
        int entries = 0, groups = 0;
        const auto fingerprint = fingerprintOf(db, &entries, &groups);
        HistoryRevision revision;
        revision.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        revision.timestamp = QDateTime::currentDateTimeUtc();
        revision.databasePath = databaseId(db->filePath());
        revision.databaseName = QStringLiteral("Database %1").arg(revision.databasePath.left(8));
        revision.label = redactedLabel;
        revision.kind = kind;
        revision.entryCount = entries;
        revision.groupCount = groups;
        if (!commitTransaction(revision, fingerprintJson(fingerprint))) {
            emit writeFailed(tr("Local history could not be recorded. The database operation completed; retry after checking Git and application-data storage."));
            return false;
        }
        return true;
    }

    bool HistoryStore::recordSettingsEvent(const QString& redactedLabel)
    {
        if (redactedLabel.trimmed().isEmpty()) return false;
        load();
        HistoryRevision revision;
        revision.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        revision.timestamp = QDateTime::currentDateTimeUtc();
        revision.databasePath = QCryptographicHash::hash(QByteArrayLiteral("keepassxc-application-settings"), QCryptographicHash::Sha256).toHex();
        revision.databaseName = tr("Application settings");
        revision.label = redactedLabel;
        revision.kind = RevisionKind::Settings;
        if (!commitTransaction(revision, QByteArrayLiteral("[]"))) {
            emit writeFailed(tr("The settings change completed, but its local history record could not be written."));
            return false;
        }
        return true;
    }

    QVector<HistoryRevision> HistoryStore::revisions() const { return revisions(0, MaximumPageSize); }
    QVector<HistoryRevision> HistoryStore::revisions(int offset, int limit) const
    {
        const_cast<HistoryStore*>(this)->load(); QVector<HistoryRevision> page; if (offset < 0 || limit <= 0) return page; limit = qMin(limit, MaximumPageSize);
        for (int i = m_revisions.size() - 1 - offset; i >= 0 && page.size() < limit; --i) page.append(m_revisions.at(i)); return page;
    }
    QVector<HistoryRevision> HistoryStore::revisionsFor(const QString& path) const
    {
        const_cast<HistoryStore*>(this)->load(); const QString wanted = path.size() == 64 ? path : databaseId(path); QVector<HistoryRevision> result;
        for (int i = m_revisions.size() - 1; i >= 0; --i) if (m_revisions.at(i).databasePath == wanted) result.append(m_revisions.at(i)); return result;
    }
    HistoryRevision HistoryStore::revision(const QString& id) const { const_cast<HistoryStore*>(this)->load(); for (const auto& v : m_revisions) if (v.id == id) return v; return {}; }
    HistoryRevision HistoryStore::predecessor(const QString& id) const
    {
        const auto target = revision(id); HistoryRevision previous; for (const auto& v : m_revisions) { if (v.id == id) return previous; if (v.databasePath == target.databasePath) previous = v; } return {};
    }

    QByteArray HistoryStore::snapshot(const QString& revisionId, QString* error) const
    {
        const auto fail = [error](const QString& message) { if (error) *error = message; return QByteArray(); };
        const HistoryRevision value = revision(revisionId);
        if (!value.isValid() || value.snapshotPath.isEmpty()) return fail(tr("No encrypted snapshot is available for this revision."));
        const QString normalized = QDir::cleanPath(value.snapshotPath);
        const QString expectedPrefix = QStringLiteral("%1/%2/").arg(SnapshotsName, value.databasePath);
        if (QDir::isAbsolutePath(normalized) || normalized.startsWith(QStringLiteral("../")) || !normalized.startsWith(expectedPrefix)) {
            return fail(tr("The encrypted snapshot path is outside the local history repository."));
        }
        QFile file(QDir(repositoryPath()).filePath(normalized));
        if (!file.open(QIODevice::ReadOnly)) return fail(tr("The encrypted snapshot file is missing."));
        const QByteArray bytes = file.readAll();
        if (!isKdbx(bytes)) return fail(tr("The snapshot is not a valid KDBX container."));
        if (byteDigest(bytes) != value.snapshotSha256) return fail(tr("The encrypted snapshot hash does not match its revision."));
        if (error) error->clear();
        return bytes;
    }

    int HistoryStore::restoreDeletedEntries(const QString& revisionId,
                                            const QSharedPointer<Database>& database,
                                            QString* error)
    {
        const auto fail = [error](const QString& message) {
            if (error) *error = message;
            return 0;
        };
        if (!database || !database->rootGroup() || !database->key()) {
            return fail(tr("The current database is not unlocked."));
        }
        const HistoryRevision previous = predecessor(revisionId);
        if (!previous.isValid()) return fail(tr("No earlier encrypted snapshot is available."));
        QString snapshotError;
        const QByteArray bytes = snapshot(previous.id, &snapshotError);
        if (bytes.isEmpty()) return fail(snapshotError);

        QBuffer buffer;
        buffer.setData(bytes);
        if (!buffer.open(QIODevice::ReadOnly)) return fail(tr("The encrypted snapshot could not be opened."));
        auto snapshotDatabase = QSharedPointer<Database>::create();
        KeePass2Reader reader;
        reader.readDatabase(&buffer, database->key(), snapshotDatabase.data());
        if (reader.hasError()) {
            return fail(tr("The encrypted snapshot could not be unlocked with the current database key."));
        }

        int restored = 0;
        QList<DeletedObject> deletedObjects = database->deletedObjects();
        for (Entry* oldEntry : snapshotDatabase->rootGroup()->entriesRecursive(false)) {
            if (!oldEntry || oldEntry->isRecycled() || database->rootGroup()->findEntryByUuid(oldEntry->uuid())) continue;
            Group* target = oldEntry->group()
                                ? database->rootGroup()->findGroupByUuid(oldEntry->group()->uuid())
                                : nullptr;
            if (!target || target->isRecycled()) target = database->rootGroup();
            oldEntry->clone(Entry::CloneIncludeHistory)->setGroup(target);
            deletedObjects.erase(std::remove_if(deletedObjects.begin(),
                                                deletedObjects.end(),
                                                [oldEntry](const DeletedObject& value) {
                                                    return value.uuid == oldEntry->uuid();
                                                }),
                                 deletedObjects.end());
            ++restored;
        }
        database->setDeletedObjects(deletedObjects);
        if (restored == 0) return fail(tr("No deleted entries were found in the previous snapshot."));
        recordEvent(database, tr("Restored deleted entries from encrypted history"), RevisionKind::Entry);
        if (error) error->clear();
        return restored;
    }
}
