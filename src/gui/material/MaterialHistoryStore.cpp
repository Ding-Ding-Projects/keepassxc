/* Copyright (C) 2026 KeePassXC Team <team@keepassxc.org>
 * SPDX-License-Identifier: GPL-2.0-or-later OR GPL-3.0-only */

#include "MaterialHistoryStore.h"
#include "core/Database.h"
#include "core/Entry.h"
#include "core/Group.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>
#include <QThread>
#include <QUuid>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

namespace Material
{
    namespace
    {
        constexpr int DeadlineMs = 10000;
        constexpr int ReplaceAttempts = 7;
        constexpr int MaximumPageSize = 500;
        const QString StateName = QStringLiteral("revisions.json");
        const QString FingerprintsName = QStringLiteral("fingerprints");

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

        QString databaseId(const QString& path)
        {
            return digest(QDir::cleanPath(QDir::fromNativeSeparators(path)));
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
            return {{QStringLiteral("id"), v.id}, {QStringLiteral("time"), v.timestamp.toUTC().toString(Qt::ISODateWithMs)},
                    {QStringLiteral("databaseId"), v.databasePath}, {QStringLiteral("name"), v.databaseName},
                    {QStringLiteral("label"), v.label}, {QStringLiteral("kind"), kindToken(v.kind)},
                    {QStringLiteral("entries"), v.entryCount}, {QStringLiteral("groups"), v.groupCount},
                    {QStringLiteral("added"), v.added}, {QStringLiteral("removed"), v.removed}, {QStringLiteral("edited"), v.edited}};
        }

        HistoryRevision fromJson(const QJsonObject& o)
        {
            HistoryRevision v; v.id = o.value(QStringLiteral("id")).toString();
            v.timestamp = QDateTime::fromString(o.value(QStringLiteral("time")).toString(), Qt::ISODateWithMs);
            v.databasePath = o.value(QStringLiteral("databaseId")).toString(); v.databaseName = o.value(QStringLiteral("name")).toString();
            v.label = o.value(QStringLiteral("label")).toString(); v.kind = kindFromToken(o.value(QStringLiteral("kind")).toString());
            v.entryCount = o.value(QStringLiteral("entries")).toInt(); v.groupCount = o.value(QStringLiteral("groups")).toInt();
            v.added = o.value(QStringLiteral("added")).toInt(); v.removed = o.value(QStringLiteral("removed")).toInt(); v.edited = o.value(QStringLiteral("edited")).toInt();
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
            const QString temporary = target + QStringLiteral(".%1.tmp").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
            QFile file(temporary);
            if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.flush()) { file.close(); QFile::remove(temporary); return false; }
            file.close();
            for (int attempt = 0; attempt < ReplaceAttempts; ++attempt) {
#ifdef Q_OS_WIN
                if (MoveFileExW(reinterpret_cast<LPCWSTR>(temporary.utf16()), reinterpret_cast<LPCWSTR>(target.utf16()), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return true;
                const DWORD error = GetLastError();
                if (error != ERROR_SHARING_VIOLATION && error != ERROR_ACCESS_DENIED && error != ERROR_LOCK_VIOLATION) break;
#else
                if (QFile::rename(temporary, target)) return true;
#endif
                QThread::msleep(static_cast<unsigned long>(10 * (attempt + 1)));
            }
            QFile::remove(temporary); return false;
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

    bool HistoryStore::ensureRepository()
    {
        const QString repo = repositoryPath();
        if (repo.isEmpty() || !QDir().mkpath(repo) || m_gitExecutable.isEmpty()) return false;
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

    bool HistoryStore::commitTransaction(const HistoryRevision& revision, const QByteArray& fingerprint)
    {
        if (!ensureRepository()) return false;
        QVector<HistoryRevision> next = m_revisions; next.append(revision);
        const QString statePath = QDir(repositoryPath()).filePath(StateName);
        const QString fpPath = fingerprintPath(revision.databasePath);
        const auto rollback = [this, statePath, fpPath] {
            if (git(m_gitExecutable, repositoryPath(), {QStringLiteral("rev-parse"), QStringLiteral("--verify"), QStringLiteral("HEAD")}).ok()) {
                git(m_gitExecutable, repositoryPath(), {QStringLiteral("reset"), QStringLiteral("--hard"), QStringLiteral("HEAD")});
            } else {
                QFile::remove(statePath);
                QFile::remove(fpPath);
            }
        };
        if (!atomicReplace(statePath, revisionsJson(next)) || !atomicReplace(fpPath, fingerprint)) { rollback(); return false; }
        const auto add = git(m_gitExecutable, repositoryPath(), {QStringLiteral("add"), QStringLiteral("--"), StateName, FingerprintsName});
        const auto commit = add.ok() ? git(m_gitExecutable, repositoryPath(), {QStringLiteral("commit"), QStringLiteral("--quiet"), QStringLiteral("-m"), QStringLiteral("Record history event %1").arg(revision.id)}) : ProcessResult{};
        if (!commit.ok()) { rollback(); return false; }
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
        if (!commitTransaction(r, fingerprintJson(current))) { emit writeFailed(tr("Local history could not be recorded. The database save completed; retry after checking Git and application-data storage.")); return false; }
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
}
