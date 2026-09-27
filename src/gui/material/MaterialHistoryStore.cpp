/* Copyright (C) 2026 KeePassXC Team <team@keepassxc.org>
 * SPDX-License-Identifier: GPL-2.0-or-later OR GPL-3.0-only */

#include "MaterialHistoryStore.h"
#include "core/Database.h"
#include "core/Entry.h"
#include "core/Group.h"
#include "core/Metadata.h"
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
#include <QRegularExpression>
#include <QStandardPaths>
#include <QSaveFile>
#include <QSet>
#include <QSignalBlocker>
#include <QTemporaryDir>
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
        const QString DatabaseIdentityKey = QStringLiteral("KeePassXC/History/DatabaseId");
        const QString EmbeddedHistoryKey = QStringLiteral("KeePassXC/History/BundleV1");
        const QString DatabaseHistoryManifestName = QStringLiteral("history.json");
        constexpr qsizetype MaximumEmbeddedBundleBytes = 64 * 1024 * 1024;
        constexpr int MaximumEmbeddedCommits = 4096;
        constexpr int MaximumEmbeddedObjects = 100000;
        constexpr qint64 MaximumEmbeddedObjectBytes = 512LL * 1024 * 1024;
        constexpr qsizetype MaximumRevisionMetadataBytes = 64 * 1024;
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

        QString fingerprintDigest(const QByteArray& fingerprint)
        {
            return byteDigest(fingerprint);
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
            if (!v.contentFingerprint.isEmpty()) object.insert(QStringLiteral("contentFingerprint"), v.contentFingerprint);
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
            v.contentFingerprint = o.value(QStringLiteral("contentFingerprint")).toString();
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

        bool isDatabaseIdentity(const QString& value)
        {
            static const QRegularExpression identityPattern(QStringLiteral("^[a-f0-9]{64}$"));
            return identityPattern.match(value).hasMatch();
        }

        QString randomDatabaseIdentity()
        {
            return QString::fromLatin1(QCryptographicHash::hash(QUuid::createUuid().toRfc4122(), QCryptographicHash::Sha256).toHex());
        }

        QJsonObject databaseHistoryManifest(const QString& identity)
        {
            return QJsonObject{{QStringLiteral("schemaVersion"), 1},
                               {QStringLiteral("databaseId"), identity},
                               {QStringLiteral("snapshotFormat"), QStringLiteral("encrypted-kdbx")}};
        }

        bool isRevisionId(const QString& value)
        {
            static const QRegularExpression idPattern(QStringLiteral("^[0-9a-fA-F-]{36}$"));
            return idPattern.match(value).hasMatch()
                && QUuid(value).toString(QUuid::WithoutBraces).compare(value, Qt::CaseInsensitive) == 0;
        }

        bool initializeDatabaseHistoryRepository(const QString& gitExecutable, const QString& repository)
        {
            if (!QDir().mkpath(repository)) return false;
            if (!QFileInfo::exists(QDir(repository).filePath(QStringLiteral(".git")))
                && !git(gitExecutable, repository, {QStringLiteral("init"), QStringLiteral("--quiet")}).ok()) {
                return false;
            }
            const auto head = git(gitExecutable, repository, {QStringLiteral("symbolic-ref"), QStringLiteral("--short"), QStringLiteral("HEAD")});
            if (head.ok() && head.out.trimmed() != QByteArrayLiteral("main")) {
                if (!git(gitExecutable, repository, {QStringLiteral("branch"), QStringLiteral("-m"), QStringLiteral("main")}).ok()) return false;
            } else if (!head.ok()) {
                if (!git(gitExecutable, repository, {QStringLiteral("symbolic-ref"), QStringLiteral("HEAD"), QStringLiteral("refs/heads/main")}).ok()) return false;
            }
            if (!git(gitExecutable, repository, {QStringLiteral("config"), QStringLiteral("user.name"), QStringLiteral("KeePassXC History")}).ok()
                || !git(gitExecutable, repository, {QStringLiteral("config"), QStringLiteral("user.email"), QStringLiteral("history@localhost")}).ok()
                || !git(gitExecutable, repository, {QStringLiteral("config"), QStringLiteral("commit.gpgsign"), QStringLiteral("false")}).ok()) {
                return false;
            }
#ifdef Q_OS_WIN
            const QString noHooks = QStringLiteral("NUL");
#else
            const QString noHooks = QStringLiteral("/dev/null");
#endif
            return git(gitExecutable, repository, {QStringLiteral("config"), QStringLiteral("core.hooksPath"), noHooks}).ok();
        }

        QString embeddedHistoryEnvelope(const QString& databaseIdentity, const QByteArray& bundle)
        {
            const QByteArray sha = QCryptographicHash::hash(bundle, QCryptographicHash::Sha256).toHex();
            return QStringLiteral("1:%1:%2:%3")
                .arg(databaseIdentity, QString::fromLatin1(sha), QString::fromLatin1(bundle.toBase64()));
        }

        bool decodeEmbeddedHistoryEnvelope(const QString& value,
                                           const QString& expectedIdentity,
                                           QByteArray* bundle,
                                           QString* error)
        {
            const auto fail = [error](const QString& reason) {
                if (error) *error = reason;
                return false;
            };
            if (value.size() > ((MaximumEmbeddedBundleBytes + 2) / 3) * 4 + 140) {
                return fail(QStringLiteral("The embedded history bundle exceeds its encoded size limit."));
            }
            const QStringList fields = value.split(QLatin1Char(':'));
            if (fields.size() != 4 || fields.at(0) != QLatin1String("1")
                || !isDatabaseIdentity(fields.at(1)) || fields.at(1) != expectedIdentity
                || !QRegularExpression(QStringLiteral("^[a-f0-9]{64}$")).match(fields.at(2)).hasMatch()) {
                return fail(QStringLiteral("The embedded history header or database identity is invalid."));
            }
            const QByteArray encoded = fields.at(3).toLatin1();
            const QByteArray decoded = QByteArray::fromBase64(encoded, QByteArray::AbortOnBase64DecodingErrors);
            if (decoded.isEmpty() || decoded.size() > MaximumEmbeddedBundleBytes || decoded.toBase64() != encoded) {
                return fail(QStringLiteral("The embedded history bundle is malformed or exceeds its size limit."));
            }
            if (QCryptographicHash::hash(decoded, QCryptographicHash::Sha256).toHex() != fields.at(2).toLatin1()) {
                return fail(QStringLiteral("The embedded history digest does not match its contents."));
            }
            if (bundle) *bundle = decoded;
            if (error) error->clear();
            return true;
        }
    }

    HistoryStore::HistoryStore(const QString& storageRoot, const QString& gitExecutable)
        : m_storageRoot(storageRoot), m_gitExecutable(gitExecutable.isEmpty() ? QStandardPaths::findExecutable(QStringLiteral("git")) : gitExecutable) {}

    HistoryStore* HistoryStore::instance() { static HistoryStore store; return &store; }

    QByteArray HistoryStore::serializeDatabaseWithoutEmbeddedHistory(const QSharedPointer<Database>& db, QString* error) const
    {
        if (!db || !db->metadata() || !db->metadata()->customData()) return {};
        auto* customData = db->metadata()->customData();
        const bool hadBundle = customData->contains(EmbeddedHistoryKey);
        const auto bundle = hadBundle ? customData->item(EmbeddedHistoryKey) : CustomData::CustomDataItem{};
        const bool wasModified = db->isModified();
        QSignalBlocker blocker(db.data());
        if (hadBundle) customData->remove(EmbeddedHistoryKey);

        QBuffer buffer;
        buffer.open(QIODevice::WriteOnly);
        const bool written = db->writeDatabase(&buffer, error);

        if (hadBundle) customData->set(EmbeddedHistoryKey, bundle);
        if (wasModified) db->markAsModified();
        else db->markAsClean();
        return written ? buffer.data() : QByteArray{};
    }

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

    QString HistoryStore::databaseIdentity(const QSharedPointer<Database>& db) const
    {
        if (!db || !db->metadata() || !db->metadata()->customData()) return {};
        const QString stored = db->metadata()->customData()->value(DatabaseIdentityKey);
        if (isDatabaseIdentity(stored)) return stored;
        return db->filePath().isEmpty() ? QString() : databaseId(db->filePath());
    }

    bool HistoryStore::beginDatabaseSave(const QSharedPointer<Database>& db, const QString& destinationPath)
    {
        if (!db || !db->rootGroup() || !db->metadata() || !db->metadata()->customData() || destinationPath.isEmpty()) return false;
        if (m_databaseSaves.contains(db.data())) cancelDatabaseSave(db);

        auto* customData = db->metadata()->customData();
        DatabaseSaveState state;
        state.hadIdentity = customData->contains(DatabaseIdentityKey);
        if (state.hadIdentity) state.identity = customData->item(DatabaseIdentityKey);
        state.hadBundle = customData->contains(EmbeddedHistoryKey);
        if (state.hadBundle) state.bundle = customData->item(EmbeddedHistoryKey);
        state.wasModified = db->isModified();
        const QString identity = state.hadIdentity && isDatabaseIdentity(state.identity.value)
                                     ? state.identity.value
                                     : (db->filePath().isEmpty() ? randomDatabaseIdentity() : databaseId(db->filePath()));
        if (!isDatabaseIdentity(identity)) return false;
        state.databaseIdentity = identity;
        if (!state.hadIdentity || state.identity.value != identity) {
            customData->set(DatabaseIdentityKey, identity);
        }
        state.hadSavedBundle = customData->contains(EmbeddedHistoryKey);
        if (state.hadSavedBundle) state.savedBundle = customData->item(EmbeddedHistoryKey);
        m_databaseSaves.insert(db.data(), state);
        return true;
    }

    void HistoryStore::cancelDatabaseSave(const QSharedPointer<Database>& db, bool restoreModifiedState)
    {
        if (!db || !db->metadata() || !db->metadata()->customData()) return;
        const auto found = m_databaseSaves.find(db.data());
        if (found == m_databaseSaves.end()) return;
        const DatabaseSaveState state = found.value();
        auto* customData = db->metadata()->customData();
        if (state.hadIdentity) customData->set(DatabaseIdentityKey, state.identity);
        else customData->remove(DatabaseIdentityKey);
        if (state.hadBundle) customData->set(EmbeddedHistoryKey, state.bundle);
        else customData->remove(EmbeddedHistoryKey);
        if (restoreModifiedState) {
            if (state.wasModified) db->markAsModified();
            else db->markAsClean();
        }
        m_databaseSaves.erase(found);
    }

    bool HistoryStore::embedLatestHistory(const QSharedPointer<Database>& db, QString* error)
    {
        const auto fail = [error](const QString& message) {
            if (error) *error = message;
            return false;
        };
        if (!db || !db->metadata() || !db->metadata()->customData() || !m_databaseSaves.contains(db.data())) {
            return fail(tr("No active database save is available for history embedding."));
        }
        const QString identity = databaseIdentity(db);
        QByteArray fingerprint;
        QByteArray encryptedSnapshot;
        HistoryRevision revision = createSaveRevision(db, &fingerprint, &encryptedSnapshot);
        if (!revision.isValid() || encryptedSnapshot.isEmpty()) return fail(tr("The encrypted database snapshot could not be prepared."));
        auto stagingDirectory = QSharedPointer<QTemporaryDir>::create();
        if (!stagingDirectory->isValid()) return fail(tr("A temporary directory for encrypted history staging could not be created."));
        const QString stagingRepository = QDir(stagingDirectory->path()).filePath(QStringLiteral("repository"));
        const QString localRepository = databaseRepositoryPath(identity);
        const bool localRepositoryExists = QFileInfo::exists(QDir(localRepository).filePath(QStringLiteral(".git")));
        if (localRepositoryExists) {
            QLockFile lock(localRepository + QStringLiteral(".lock"));
            lock.setStaleLockTime(0);
            if (!lock.tryLock(LockTimeoutMs)) return fail(tr("The per-database history repository is in use by another save."));
            QString repositoryError;
            if (!validateDatabaseRepository(localRepository, identity, &repositoryError)) return fail(repositoryError);
            const auto clone = git(m_gitExecutable,
                                   QString(),
                                   {QStringLiteral("clone"), QStringLiteral("--quiet"), QStringLiteral("--no-hardlinks"),
                                    localRepository, stagingRepository});
            if (!clone.ok()) return fail(tr("The current encrypted history could not be staged."));
            if (!git(m_gitExecutable, stagingRepository, {QStringLiteral("remote"), QStringLiteral("remove"), QStringLiteral("origin")}).ok()) {
                return fail(tr("The temporary history staging reference could not be removed."));
            }
        } else if (!initializeDatabaseHistoryRepository(m_gitExecutable, stagingRepository)) {
            return fail(tr("A staging history repository could not be initialized."));
        }
        if (!commitDatabaseRepositoryAt(stagingRepository, revision, encryptedSnapshot, fingerprint)) {
            return fail(tr("The new encrypted history revision could not be staged."));
        }
        QString validationError;
        if (!validateDatabaseRepository(stagingRepository, identity, &validationError)) return fail(validationError);

        const QString bundlePath = QDir(stagingDirectory->path()).filePath(QStringLiteral("history.bundle"));
        const auto created = git(m_gitExecutable,
                                 stagingRepository,
                                 {QStringLiteral("bundle"), QStringLiteral("create"), bundlePath, QStringLiteral("refs/heads/main")});
        if (!created.ok()) return fail(tr("The per-database history bundle could not be created."));
        const auto verified = git(m_gitExecutable, stagingRepository, {QStringLiteral("bundle"), QStringLiteral("verify"), bundlePath});
        const auto heads = git(m_gitExecutable, stagingRepository, {QStringLiteral("bundle"), QStringLiteral("list-heads"), bundlePath});
        const QList<QByteArray> headLines = heads.out.trimmed().split('\n');
        if (!verified.ok() || !heads.ok() || headLines.size() != 1
            || !headLines.constFirst().trimmed().endsWith(QByteArrayLiteral(" refs/heads/main"))) {
            return fail(tr("The per-database history bundle failed reference or object verification."));
        }
        QFile bundleFile(bundlePath);
        if (!bundleFile.open(QIODevice::ReadOnly) || bundleFile.size() <= 0 || bundleFile.size() > MaximumEmbeddedBundleBytes) {
            return fail(tr("The encrypted history bundle is empty or exceeds its 64 MiB limit."));
        }
        const QByteArray bundle = bundleFile.readAll();
        if (bundle.size() != bundleFile.size()) return fail(tr("The encrypted history bundle could not be read completely."));
        const QString envelope = embeddedHistoryEnvelope(identity, bundle);
        QByteArray checkedBundle;
        QString envelopeError;
        if (!decodeEmbeddedHistoryEnvelope(envelope, identity, &checkedBundle, &envelopeError) || checkedBundle != bundle) {
            return fail(tr("The encrypted history bundle failed its integrity check."));
        }
        auto state = m_databaseSaves.find(db.data());
        state->stagingDirectory = stagingDirectory;
        state->stagingRepository = stagingRepository;
        state->stagedRevision = revision;
        state->stagedSnapshot = encryptedSnapshot;
        state->stagedFingerprint = fingerprint;
        state->staged = true;
        db->metadata()->customData()->set(EmbeddedHistoryKey, envelope);
        if (error) error->clear();
        return true;
    }

    void HistoryStore::finishEmbeddedHistory(const QSharedPointer<Database>& db, bool persisted)
    {
        if (!db || !db->metadata() || !db->metadata()->customData()) return;
        const auto found = m_databaseSaves.find(db.data());
        if (found == m_databaseSaves.end()) return;
        if (persisted && !found->finalizationAttempted) recordSave(db);
        if (!persisted) {
            cancelDatabaseSave(db);
            return;
        }
        db->markAsClean();
        m_databaseSaves.erase(found);
    }

    bool HistoryStore::hydrateDatabase(const QSharedPointer<Database>& db, QString* error)
    {
        const auto fail = [error](const QString& message) {
            if (error) *error = message;
            return false;
        };
        if (!db || !db->rootGroup() || !db->metadata() || !db->metadata()->customData() || db->filePath().isEmpty()) {
            return fail(tr("The opened database is not ready for history recovery."));
        }
        const QString identity = databaseIdentity(db);
        if (!isDatabaseIdentity(identity) || m_gitExecutable.isEmpty()) return fail(tr("The database identity or local Git executable is unavailable."));
        QString localRepository = databaseRepositoryPath(identity);
        if (localRepository.isEmpty()) return fail(tr("The per-database history location is unavailable."));

        const auto currentFingerprint = [&db] {
            int entryCount = 0;
            int groupCount = 0;
            return fingerprintDigest(fingerprintJson(fingerprintOf(db, &entryCount, &groupCount)));
        }();
        const auto bundleMatchesDatabase = [this, &identity, &currentFingerprint](const QString& repo, QString* reason) {
            const QDir revisionsDirectory(QDir(repo).filePath(QStringLiteral("revisions")));
            const QStringList files = revisionsDirectory.entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
            bool hasFingerprint = false;
            for (const QString& filename : files) {
                QFile file(revisionsDirectory.filePath(filename));
                if (!file.open(QIODevice::ReadOnly) || file.size() > MaximumRevisionMetadataBytes) {
                    if (reason) *reason = tr("A history revision could not be checked against this database.");
                    return false;
                }
                const auto revision = fromJson(QJsonDocument::fromJson(file.readAll()).object());
                if (!revision.isValid() || revision.databasePath != identity) {
                    if (reason) *reason = tr("A history revision belongs to another database.");
                    return false;
                }
                if (!revision.contentFingerprint.isEmpty()) {
                    hasFingerprint = true;
                    if (revision.contentFingerprint == currentFingerprint) return true;
                }
            }
            if (hasFingerprint && reason) *reason = tr("The embedded history is stale or replayed for the current database contents.");
            return !hasFingerprint;
        };

        const QString encoded = db->metadata()->customData()->value(EmbeddedHistoryKey);
        QString candidateRepository;
        QTemporaryDir temporary;
        if (!encoded.isEmpty()) {
            const QString storedIdentity = db->metadata()->customData()->value(DatabaseIdentityKey);
            if (!isDatabaseIdentity(storedIdentity) || storedIdentity != identity) {
                return fail(tr("The embedded history has no matching encrypted database identity."));
            }
            QByteArray bundle;
            QString envelopeError;
            if (!decodeEmbeddedHistoryEnvelope(encoded, identity, &bundle, &envelopeError)) return fail(envelopeError);
            if (!temporary.isValid()) return fail(tr("A temporary directory for history validation could not be created."));
            const QString bundlePath = QDir(temporary.path()).filePath(QStringLiteral("incoming.bundle"));
            if (!atomicReplace(bundlePath, bundle)) return fail(tr("The embedded history bundle could not be staged for validation."));
            candidateRepository = QDir(temporary.path()).filePath(QStringLiteral("candidate"));
            if (!initializeDatabaseHistoryRepository(m_gitExecutable, candidateRepository)) {
                return fail(tr("A temporary history repository could not be initialized."));
            }
            const auto verified = git(m_gitExecutable, candidateRepository, {QStringLiteral("bundle"), QStringLiteral("verify"), bundlePath});
            const auto heads = git(m_gitExecutable, candidateRepository, {QStringLiteral("bundle"), QStringLiteral("list-heads"), bundlePath});
            const QList<QByteArray> headLines = heads.out.trimmed().split('\n');
            if (!verified.ok() || !heads.ok() || headLines.size() != 1
                || !headLines.constFirst().trimmed().endsWith(QByteArrayLiteral(" refs/heads/main"))) {
                return fail(tr("The embedded history bundle has invalid references or missing objects."));
            }
            const auto fetched = git(m_gitExecutable,
                                     candidateRepository,
                                     {QStringLiteral("fetch"), QStringLiteral("--no-tags"), bundlePath,
                                      QStringLiteral("refs/heads/main:refs/heads/main")});
            const auto reset = fetched.ok()
                                   ? git(m_gitExecutable,
                                         candidateRepository,
                                         {QStringLiteral("reset"), QStringLiteral("--hard"), QStringLiteral("refs/heads/main")})
                                   : ProcessResult{};
            if (!fetched.ok() || !reset.ok()) {
                return fail(tr("The embedded history bundle could not be imported into the validation repository."));
            }
            QString validationError;
            if (!validateDatabaseRepository(candidateRepository, identity, &validationError)) return fail(validationError);
            QString fingerprintError;
            if (!bundleMatchesDatabase(candidateRepository, &fingerprintError)) return fail(fingerprintError);
        }

        const bool localRepositoryHasHistory = QFileInfo::exists(QDir(localRepository).filePath(QStringLiteral(".git")))
            && git(m_gitExecutable, localRepository, {QStringLiteral("rev-parse"), QStringLiteral("--verify"), QStringLiteral("HEAD")}).ok();
        const bool localRepositoryExists = localRepositoryHasHistory;
        if (candidateRepository.isEmpty() && !localRepositoryExists) {
            if (error) error->clear();
            return true;
        }
        const QString lockPath = localRepository + QStringLiteral(".lock");
        QDir().mkpath(localRepository);
        QLockFile lock(lockPath);
        lock.setStaleLockTime(0);
        if (!lock.tryLock(LockTimeoutMs)) return fail(tr("The per-database history repository is in use by another save."));
        if (!initializeDatabaseHistoryRepository(m_gitExecutable, localRepository)) {
            return fail(tr("The per-database history repository could not be initialized."));
        }
        if (!localRepositoryExists && !candidateRepository.isEmpty()) {
            const auto status = git(m_gitExecutable, localRepository, {QStringLiteral("status"), QStringLiteral("--porcelain"), QStringLiteral("--untracked-files=all")});
            if (!status.ok() || !status.out.isEmpty()) return fail(tr("Unrecognized local history files were left untouched."));
            const auto fetch = git(m_gitExecutable,
                                   localRepository,
                                   {QStringLiteral("fetch"), QStringLiteral("--no-tags"), candidateRepository,
                                    QStringLiteral("refs/heads/main:refs/remotes/embedded/main")});
            const auto reset = fetch.ok()
                                   ? git(m_gitExecutable,
                                         localRepository,
                                         {QStringLiteral("reset"), QStringLiteral("--hard"), QStringLiteral("refs/remotes/embedded/main")})
                                   : ProcessResult{};
            if (!fetch.ok() || !reset.ok()) return fail(tr("The validated embedded history could not initialize local history."));
            QString validationError;
            if (!validateDatabaseRepository(localRepository, identity, &validationError)) return fail(validationError);
            if (!loadDatabaseRepositoryRevisions(identity, currentFingerprint, &validationError)) return fail(validationError);
            if (error) error->clear();
            return true;
        }
        const QString manifestPath = QDir(localRepository).filePath(DatabaseHistoryManifestName);
        if (!QFileInfo::exists(manifestPath)) {
            const auto status = git(m_gitExecutable, localRepository, {QStringLiteral("status"), QStringLiteral("--porcelain"), QStringLiteral("--untracked-files=all")});
            if (!status.ok() || !status.out.isEmpty()) return fail(tr("An unrecognized history repository has local changes and was left untouched."));
            if (!atomicReplace(manifestPath,
                               QJsonDocument(databaseHistoryManifest(identity)).toJson(QJsonDocument::Compact))) {
                return fail(tr("The per-database history manifest could not be migrated."));
            }
            const auto add = git(m_gitExecutable, localRepository, {QStringLiteral("add"), QStringLiteral("--"), DatabaseHistoryManifestName});
            const auto commit = add.ok()
                                    ? git(m_gitExecutable,
                                          localRepository,
                                          {QStringLiteral("commit"), QStringLiteral("--quiet"), QStringLiteral("-m"),
                                           QStringLiteral("Identify encrypted database history")})
                                    : ProcessResult{};
            if (!commit.ok()) return fail(tr("The legacy per-database history repository could not be identified."));
        }
        QString localValidationError;
        if (!validateDatabaseRepository(localRepository, identity, &localValidationError)) return fail(localValidationError);

        if (!candidateRepository.isEmpty()) {
            const auto fetch = git(m_gitExecutable,
                                   localRepository,
                                   {QStringLiteral("fetch"), QStringLiteral("--no-tags"), candidateRepository,
                                    QStringLiteral("refs/heads/main:refs/remotes/embedded/main")});
            if (!fetch.ok()) return fail(tr("The validated encrypted history could not be merged with local history."));
            const auto currentHead = git(m_gitExecutable, localRepository, {QStringLiteral("rev-parse"), QStringLiteral("HEAD")});
            const auto embeddedHead = git(m_gitExecutable, localRepository, {QStringLiteral("rev-parse"), QStringLiteral("refs/remotes/embedded/main")});
            if (!currentHead.ok() || !embeddedHead.ok()) return fail(tr("The history references could not be compared."));
            if (currentHead.out.trimmed() != embeddedHead.out.trimmed()) {
                const auto localBeforeIncoming = git(m_gitExecutable,
                                                     localRepository,
                                                     {QStringLiteral("merge-base"), QStringLiteral("--is-ancestor"), QStringLiteral("HEAD"),
                                                      QStringLiteral("refs/remotes/embedded/main")});
                if (localBeforeIncoming.ok()) {
                    if (!git(m_gitExecutable, localRepository, {QStringLiteral("merge"), QStringLiteral("--ff-only"), QStringLiteral("refs/remotes/embedded/main")}).ok()) {
                        return fail(tr("The validated history could not be advanced safely."));
                    }
                } else {
                    const auto incomingBeforeLocal = git(m_gitExecutable,
                                                         localRepository,
                                                         {QStringLiteral("merge-base"), QStringLiteral("--is-ancestor"),
                                                          QStringLiteral("refs/remotes/embedded/main"), QStringLiteral("HEAD")});
                    if (incomingBeforeLocal.ok()) {
                        QString loadError;
                        if (!loadDatabaseRepositoryRevisions(identity, currentFingerprint, &loadError)) return fail(loadError);
                        return fail(tr("The embedded history is older than local history and was refused as a replay."));
                    }
                    const auto merged = git(m_gitExecutable,
                                            localRepository,
                                            {QStringLiteral("merge"), QStringLiteral("--no-ff"), QStringLiteral("--no-edit"),
                                             QStringLiteral("-m"), QStringLiteral("Merge recovered encrypted history"),
                                             QStringLiteral("refs/remotes/embedded/main")});
                    if (!merged.ok()) {
                        git(m_gitExecutable, localRepository, {QStringLiteral("merge"), QStringLiteral("--abort")});
                        return fail(tr("Divergent local and embedded history could not be merged without a conflict."));
                    }
                }
            }
            QString mergedStateError;
            if (!bundleMatchesDatabase(localRepository, &mergedStateError)) return fail(mergedStateError);
        } else if (QFileInfo::exists(manifestPath)) {
            QString localStateError;
            if (!bundleMatchesDatabase(localRepository, &localStateError)) return fail(localStateError);
        }

        QString loadError;
        if (!loadDatabaseRepositoryRevisions(identity, currentFingerprint, &loadError)) return fail(loadError);
        if (error) error->clear();
        return true;
    }

    bool HistoryStore::commitDatabaseRepository(const HistoryRevision& revision,
                                                const QByteArray& encryptedSnapshot,
                                                const QByteArray& fingerprint)
    {
        const QString repo = databaseRepositoryPath(revision.databasePath);
        if (repo.isEmpty() || m_gitExecutable.isEmpty()) return false;
        if (!QDir().mkpath(repo)) return false;
        QLockFile lock(repo + QStringLiteral(".lock"));
        lock.setStaleLockTime(0);
        if (!lock.tryLock(LockTimeoutMs)) return false;
        return commitDatabaseRepositoryAt(repo, revision, encryptedSnapshot, fingerprint);
    }

    bool HistoryStore::commitDatabaseRepositoryAt(const QString& repo,
                                                  const HistoryRevision& revision,
                                                  const QByteArray& encryptedSnapshot,
                                                  const QByteArray& fingerprint)
    {
        if (encryptedSnapshot.isEmpty() || !isDatabaseIdentity(revision.databasePath) || !isRevisionId(revision.id)
            || !isKdbx(encryptedSnapshot) || byteDigest(encryptedSnapshot) != revision.snapshotSha256
            || fingerprint.isEmpty() || fingerprintDigest(fingerprint) != revision.contentFingerprint
            || m_gitExecutable.isEmpty() || !initializeDatabaseHistoryRepository(m_gitExecutable, repo)) return false;
        const auto status = git(m_gitExecutable, repo, {QStringLiteral("status"), QStringLiteral("--porcelain"), QStringLiteral("--untracked-files=all")});
        if (!status.ok() || !status.out.isEmpty()) return false;
        const QString manifestPath = QDir(repo).filePath(DatabaseHistoryManifestName);
        if (QFileInfo::exists(manifestPath)) {
            QFile manifest(manifestPath);
            if (!manifest.open(QIODevice::ReadOnly) || manifest.size() > 4096) return false;
            const auto current = QJsonDocument::fromJson(manifest.readAll()).object();
            if (current != databaseHistoryManifest(revision.databasePath)) return false;
        } else if (!atomicReplace(manifestPath,
                                  QJsonDocument(databaseHistoryManifest(revision.databasePath)).toJson(QJsonDocument::Compact))) {
            return false;
        }
        const QString snapshotName = QStringLiteral("snapshots/%1.kdbx").arg(revision.id);
        const QString metadataName = QStringLiteral("revisions/%1.json").arg(revision.id);
        const QString fingerprintName = QStringLiteral("fingerprints/%1.json").arg(revision.id);
        HistoryRevision storedRevision = revision;
        storedRevision.snapshotPath = snapshotName;
        if (!atomicReplace(QDir(repo).filePath(snapshotName), encryptedSnapshot)
            || !atomicReplace(QDir(repo).filePath(metadataName),
                              QJsonDocument(toJson(storedRevision)).toJson(QJsonDocument::Compact))
            || !atomicReplace(QDir(repo).filePath(fingerprintName), fingerprint)) {
            return false;
        }
        const auto add = git(m_gitExecutable,
                             repo,
                             {QStringLiteral("add"), QStringLiteral("--"), DatabaseHistoryManifestName, snapshotName, metadataName, fingerprintName});
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

    bool HistoryStore::validateDatabaseRepository(const QString& repo, const QString& identity, QString* error) const
    {
        const auto fail = [error](const QString& message) {
            if (error) *error = message;
            return false;
        };
        if (!isDatabaseIdentity(identity) || m_gitExecutable.isEmpty()
            || !QFileInfo::exists(QDir(repo).filePath(QStringLiteral(".git")))) {
            return fail(tr("The per-database history repository is not initialized."));
        }
        const auto branch = git(m_gitExecutable, repo, {QStringLiteral("symbolic-ref"), QStringLiteral("--short"), QStringLiteral("HEAD")});
        if (!branch.ok() || branch.out.trimmed() != QByteArrayLiteral("main")) {
            return fail(tr("The per-database history repository has an unexpected reference."));
        }
        const auto status = git(m_gitExecutable, repo, {QStringLiteral("status"), QStringLiteral("--porcelain"), QStringLiteral("--untracked-files=all")});
        if (!status.ok() || !status.out.isEmpty()) {
            return fail(tr("The per-database history repository has unrecorded changes."));
        }

        QFile manifest(QDir(repo).filePath(DatabaseHistoryManifestName));
        if (!manifest.open(QIODevice::ReadOnly) || manifest.size() > 4096
            || QJsonDocument::fromJson(manifest.readAll()).object() != databaseHistoryManifest(identity)) {
            return fail(tr("The per-database history manifest is malformed or belongs to another database."));
        }

        const auto commitCount = git(m_gitExecutable, repo, {QStringLiteral("rev-list"), QStringLiteral("--count"), QStringLiteral("HEAD")});
        bool commitCountOk = false;
        const int commits = commitCount.out.trimmed().toInt(&commitCountOk);
        if (!commitCount.ok() || !commitCountOk || commits <= 0 || commits > MaximumEmbeddedCommits) {
            return fail(tr("The per-database history commit count is invalid or exceeds its limit."));
        }
        const auto objects = git(m_gitExecutable, repo, {QStringLiteral("rev-list"), QStringLiteral("--objects"), QStringLiteral("--all")});
        if (!objects.ok()) return fail(tr("The per-database history object list could not be read."));
        const QList<QByteArray> objectLines = objects.out.split('\n');
        if (objectLines.size() > MaximumEmbeddedObjects + 1) {
            return fail(tr("The per-database history object count exceeds its limit."));
        }
        const auto objectSizes = git(m_gitExecutable,
                                     repo,
                                     {QStringLiteral("cat-file"), QStringLiteral("--batch-all-objects"), QStringLiteral("--batch-check=%(objectsize)")});
        if (!objectSizes.ok()) return fail(tr("The per-database history object sizes could not be checked."));
        qint64 totalObjectBytes = 0;
        const QList<QByteArray> sizeLines = objectSizes.out.split('\n');
        if (sizeLines.size() > MaximumEmbeddedObjects + 1) {
            return fail(tr("The per-database history object count exceeds its limit."));
        }
        for (const QByteArray& line : sizeLines) {
            if (line.isEmpty()) continue;
            bool sizeOk = false;
            const qint64 size = line.trimmed().toLongLong(&sizeOk);
            if (!sizeOk || size < 0 || size > MaximumEmbeddedObjectBytes - totalObjectBytes) {
                return fail(tr("The per-database history expanded object data exceeds its limit."));
            }
            totalObjectBytes += size;
        }

        const auto tree = git(m_gitExecutable, repo, {QStringLiteral("ls-tree"), QStringLiteral("-r"), QStringLiteral("--full-tree"), QStringLiteral("HEAD")});
        if (!tree.ok()) return fail(tr("The per-database history tree could not be read."));
        QSet<QString> treePaths;
        for (const QByteArray& line : tree.out.split('\n')) {
            if (line.isEmpty()) continue;
            const int tab = line.indexOf('\t');
            if (tab < 0 || !line.startsWith("100644 blob ")) return fail(tr("The per-database history contains an unsupported file entry."));
            const QString path = QString::fromUtf8(line.mid(tab + 1));
            if (path != DatabaseHistoryManifestName
                && !QRegularExpression(QStringLiteral("^revisions/[0-9a-fA-F-]{36}\\.json$")).match(path).hasMatch()
                && !QRegularExpression(QStringLiteral("^snapshots/[0-9a-fA-F-]{36}\\.kdbx$")).match(path).hasMatch()
                && !QRegularExpression(QStringLiteral("^fingerprints/[0-9a-fA-F-]{36}\\.json$")).match(path).hasMatch()) {
                return fail(tr("The per-database history contains an unexpected path."));
            }
            treePaths.insert(path);
        }
        if (treePaths.size() < 3 || treePaths.size() > MaximumEmbeddedCommits * 3 + 1) {
            return fail(tr("The per-database history file count is invalid."));
        }

        const QDir revisionsDirectory(QDir(repo).filePath(QStringLiteral("revisions")));
        const QStringList revisionFiles = revisionsDirectory.entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
        if (revisionFiles.isEmpty() || revisionFiles.size() > MaximumEmbeddedCommits
            || treePaths.size() < revisionFiles.size() * 2 + 1) {
            return fail(tr("The per-database history revision inventory is invalid."));
        }
        const QDir snapshotsDirectory(QDir(repo).filePath(QStringLiteral("snapshots")));
        const QStringList snapshotFiles = snapshotsDirectory.entryList({QStringLiteral("*.kdbx")}, QDir::Files, QDir::Name);
        const QDir fingerprintsDirectory(QDir(repo).filePath(QStringLiteral("fingerprints")));
        const QStringList fingerprintFiles = fingerprintsDirectory.entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
        if (fingerprintFiles.size() > revisionFiles.size()) return fail(tr("The per-database fingerprint inventory is invalid."));

        int expectedFingerprintCount = 0;
        int expectedSnapshotCount = 0;
        for (const QString& filename : revisionFiles) {
            const QString id = filename.left(filename.size() - QStringLiteral(".json").size());
            if (!isRevisionId(id) || !treePaths.contains(QStringLiteral("revisions/%1").arg(filename))) {
                return fail(tr("The per-database history revision path is invalid."));
            }
            QFile metadata(revisionsDirectory.filePath(filename));
            if (!metadata.open(QIODevice::ReadOnly) || metadata.size() > MaximumRevisionMetadataBytes) {
                return fail(tr("The per-database history revision metadata exceeds its limit."));
            }
            QJsonParseError parseError;
            const auto document = QJsonDocument::fromJson(metadata.readAll(), &parseError);
            const HistoryRevision revision = fromJson(document.object());
            static const QRegularExpression digestPattern(QStringLiteral("^[a-f0-9]{64}$"));
            if (parseError.error != QJsonParseError::NoError || !revision.isValid() || revision.id != id
                || revision.databasePath != identity
                || (!revision.snapshotPath.isEmpty() && !digestPattern.match(revision.snapshotSha256).hasMatch())
                || (revision.snapshotPath.isEmpty() && !revision.snapshotSha256.isEmpty())
                || (!revision.contentFingerprint.isEmpty() && !digestPattern.match(revision.contentFingerprint).hasMatch())) {
                return fail(tr("The per-database history revision metadata is malformed or foreign."));
            }
            const QString snapshotRelativePath = QStringLiteral("snapshots/%1.kdbx").arg(id);
            if (!revision.snapshotPath.isEmpty()) {
                ++expectedSnapshotCount;
                if (!digestPattern.match(revision.snapshotSha256).hasMatch() || !treePaths.contains(snapshotRelativePath)) {
                    return fail(tr("A saved revision is missing its encrypted snapshot."));
                }
            } else if (!revision.snapshotSha256.isEmpty() || treePaths.contains(snapshotRelativePath)) {
                return fail(tr("A history event has inconsistent snapshot metadata."));
            }
            const QString fingerprintRelativePath = QStringLiteral("fingerprints/%1.json").arg(id);
            if (!revision.contentFingerprint.isEmpty()) {
                ++expectedFingerprintCount;
                if (!treePaths.contains(fingerprintRelativePath)) return fail(tr("A revision is missing its content fingerprint."));
                QFile fingerprint(QDir(repo).filePath(fingerprintRelativePath));
                if (!fingerprint.open(QIODevice::ReadOnly) || fingerprint.size() > 16 * 1024 * 1024) {
                    return fail(tr("A revision fingerprint could not be read within its size limit."));
                }
                const QByteArray fingerprintBytes = fingerprint.readAll();
                if (fingerprintDigest(fingerprintBytes) != revision.contentFingerprint) {
                    return fail(tr("A revision fingerprint does not match its revision metadata."));
                }
                QJsonParseError fingerprintParseError;
                const QJsonDocument fingerprintDocument = QJsonDocument::fromJson(fingerprintBytes, &fingerprintParseError);
                if (fingerprintParseError.error != QJsonParseError::NoError || !fingerprintDocument.isArray()) {
                    return fail(tr("A revision fingerprint is not a valid JSON array."));
                }
                const auto entries = fingerprintDocument.array();
                if (entries.size() > 100000) return fail(tr("A revision fingerprint contains too many entries."));
                static const QRegularExpression fingerprintEntryPattern(QStringLiteral("^[a-f0-9]{24}:[a-f0-9]{24}$"));
                for (const auto& entry : entries) {
                    if (!entry.isString() || !fingerprintEntryPattern.match(entry.toString()).hasMatch()) {
                        return fail(tr("A revision fingerprint contains an invalid record."));
                    }
                }
            } else if (treePaths.contains(fingerprintRelativePath)) {
                return fail(tr("A revision fingerprint has no matching revision metadata."));
            }
            if (!revision.snapshotPath.isEmpty()) {
                QFile snapshot(snapshotsDirectory.filePath(QStringLiteral("%1.kdbx").arg(id)));
                if (!snapshot.open(QIODevice::ReadOnly) || snapshot.size() < 12 || snapshot.size() > MaximumEmbeddedObjectBytes) {
                    return fail(tr("The per-database history contains an invalid encrypted snapshot size."));
                }
                QCryptographicHash hash(QCryptographicHash::Sha256);
                QByteArray header;
                while (!snapshot.atEnd()) {
                    const QByteArray chunk = snapshot.read(1024 * 1024);
                    if (chunk.isEmpty() && !snapshot.atEnd()) return fail(tr("A per-database history snapshot could not be read."));
                    if (header.size() < 12) header.append(chunk.left(12 - header.size()));
                    hash.addData(chunk);
                }
                if (!isKdbx(header) || QString::fromLatin1(hash.result().toHex()) != revision.snapshotSha256) {
                    return fail(tr("A per-database history snapshot failed its container or digest check."));
                }
            }
        }
        if (snapshotFiles.size() != expectedSnapshotCount) return fail(tr("The per-database history snapshot inventory is incomplete."));
        if (fingerprintFiles.size() != expectedFingerprintCount
            || treePaths.size() != revisionFiles.size() + expectedSnapshotCount + expectedFingerprintCount + 1) {
            return fail(tr("The per-database fingerprint inventory does not match its revisions."));
        }
        if (error) error->clear();
        return true;
    }

    bool HistoryStore::commitDatabaseEvent(const HistoryRevision& revision, const QByteArray& fingerprint)
    {
        if (!isDatabaseIdentity(revision.databasePath) || !isRevisionId(revision.id) || fingerprint.isEmpty()
            || fingerprintDigest(fingerprint) != revision.contentFingerprint || m_gitExecutable.isEmpty()) return false;
        const QString repo = databaseRepositoryPath(revision.databasePath);
        if (repo.isEmpty() || !QDir().mkpath(repo)) return false;
        QLockFile lock(repo + QStringLiteral(".lock"));
        lock.setStaleLockTime(0);
        if (!lock.tryLock(LockTimeoutMs) || !initializeDatabaseHistoryRepository(m_gitExecutable, repo)) return false;
        const auto status = git(m_gitExecutable, repo, {QStringLiteral("status"), QStringLiteral("--porcelain"), QStringLiteral("--untracked-files=all")});
        if (!status.ok() || !status.out.isEmpty()) return false;
        const QString manifestPath = QDir(repo).filePath(DatabaseHistoryManifestName);
        if (QFileInfo::exists(manifestPath)) {
            QFile manifest(manifestPath);
            if (!manifest.open(QIODevice::ReadOnly) || manifest.size() > 4096
                || QJsonDocument::fromJson(manifest.readAll()).object() != databaseHistoryManifest(revision.databasePath)) return false;
        } else if (!atomicReplace(manifestPath,
                                  QJsonDocument(databaseHistoryManifest(revision.databasePath)).toJson(QJsonDocument::Compact))) {
            return false;
        }
        const QString metadataName = QStringLiteral("revisions/%1.json").arg(revision.id);
        const QString fingerprintName = QStringLiteral("fingerprints/%1.json").arg(revision.id);
        if (!revision.snapshotPath.isEmpty() || !revision.snapshotSha256.isEmpty()
            || !atomicReplace(QDir(repo).filePath(metadataName), QJsonDocument(toJson(revision)).toJson(QJsonDocument::Compact))
            || !atomicReplace(QDir(repo).filePath(fingerprintName), fingerprint)) {
            return false;
        }
        const auto add = git(m_gitExecutable,
                             repo,
                             {QStringLiteral("add"), QStringLiteral("--"), DatabaseHistoryManifestName, metadataName, fingerprintName});
        const auto commit = add.ok()
                                ? git(m_gitExecutable,
                                      repo,
                                      {QStringLiteral("commit"), QStringLiteral("--quiet"), QStringLiteral("-m"),
                                       QStringLiteral("Record local database history event %1").arg(revision.id)})
                                : ProcessResult{};
        return commit.ok();
    }

    bool HistoryStore::mergeStagedDatabaseRepository(const QString& identity,
                                                      const QString& stagingRepository,
                                                      QString* error)
    {
        const auto fail = [error](const QString& message) {
            if (error) *error = message;
            return false;
        };
        if (!isDatabaseIdentity(identity) || stagingRepository.isEmpty() || m_gitExecutable.isEmpty()) {
            return fail(tr("The staged per-database history is unavailable."));
        }
        const QString repository = databaseRepositoryPath(identity);
        if (!QDir().mkpath(repository)) return fail(tr("The per-database history location could not be created."));
        QLockFile lock(repository + QStringLiteral(".lock"));
        lock.setStaleLockTime(0);
        if (!lock.tryLock(LockTimeoutMs) || !initializeDatabaseHistoryRepository(m_gitExecutable, repository)) {
            return fail(tr("The per-database history repository could not be locked or initialized."));
        }
        const bool hasHistory = git(m_gitExecutable, repository, {QStringLiteral("rev-parse"), QStringLiteral("--verify"), QStringLiteral("HEAD")}).ok();
        if (!hasHistory) {
            const auto status = git(m_gitExecutable, repository, {QStringLiteral("status"), QStringLiteral("--porcelain"), QStringLiteral("--untracked-files=all")});
            if (!status.ok() || !status.out.isEmpty()) return fail(tr("Unrecognized local history files were left untouched."));
        } else {
            QString validationError;
            if (!validateDatabaseRepository(repository, identity, &validationError)) return fail(validationError);
        }
        const QString incomingRef = QStringLiteral("refs/remotes/prepared/main");
        const auto fetch = git(m_gitExecutable,
                               repository,
                               {QStringLiteral("fetch"), QStringLiteral("--no-tags"), stagingRepository,
                                QStringLiteral("refs/heads/main:%1").arg(incomingRef)});
        if (!fetch.ok()) return fail(tr("The staged history could not be fetched into local history."));
        if (!hasHistory) {
            const auto reset = git(m_gitExecutable, repository, {QStringLiteral("reset"), QStringLiteral("--hard"), incomingRef});
            if (!reset.ok()) return fail(tr("The staged history could not initialize local history."));
        } else {
            const auto currentHead = git(m_gitExecutable, repository, {QStringLiteral("rev-parse"), QStringLiteral("HEAD")});
            const auto stagedHead = git(m_gitExecutable, repository, {QStringLiteral("rev-parse"), incomingRef});
            if (!currentHead.ok() || !stagedHead.ok()) return fail(tr("The staged history references could not be compared."));
            if (currentHead.out.trimmed() != stagedHead.out.trimmed()) {
                const auto localBeforeStaged = git(m_gitExecutable,
                                                   repository,
                                                   {QStringLiteral("merge-base"), QStringLiteral("--is-ancestor"), QStringLiteral("HEAD"), incomingRef});
                if (localBeforeStaged.ok()) {
                    if (!git(m_gitExecutable, repository, {QStringLiteral("merge"), QStringLiteral("--ff-only"), incomingRef}).ok()) {
                        return fail(tr("The staged history could not advance local history safely."));
                    }
                } else {
                    const auto stagedBeforeLocal = git(m_gitExecutable,
                                                       repository,
                                                       {QStringLiteral("merge-base"), QStringLiteral("--is-ancestor"), incomingRef,
                                                        QStringLiteral("HEAD")});
                    if (!stagedBeforeLocal.ok()) {
                        const auto merged = git(m_gitExecutable,
                                                repository,
                                                {QStringLiteral("merge"), QStringLiteral("--no-ff"), QStringLiteral("--no-edit"),
                                                 QStringLiteral("-m"), QStringLiteral("Merge saved encrypted history"), incomingRef});
                        if (!merged.ok()) {
                            git(m_gitExecutable, repository, {QStringLiteral("merge"), QStringLiteral("--abort")});
                            return fail(tr("Concurrent encrypted history could not be merged without a conflict."));
                        }
                    }
                }
            }
        }
        QString validationError;
        if (!validateDatabaseRepository(repository, identity, &validationError)) return fail(validationError);
        if (error) error->clear();
        return true;
    }

    bool HistoryStore::loadDatabaseRepositoryRevisions(const QString& identity,
                                                       const QString& currentFingerprint,
                                                       QString* error)
    {
        const auto fail = [error](const QString& message) {
            if (error) *error = message;
            return false;
        };
        if (!isDatabaseIdentity(identity)) return fail(tr("The database identity is invalid."));
        load();
        const QString repo = databaseRepositoryPath(identity);
        if (!validateDatabaseRepository(repo, identity, error)) return false;
        const QDir revisionsDirectory(QDir(repo).filePath(QStringLiteral("revisions")));
        const QStringList revisionFiles = revisionsDirectory.entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
        bool changed = false;
        bool hasFingerprint = false;
        HistoryRevision matchingRevision;
        for (const QString& filename : revisionFiles) {
            QFile file(revisionsDirectory.filePath(filename));
            if (!file.open(QIODevice::ReadOnly) || file.size() > MaximumRevisionMetadataBytes) {
                return fail(tr("The embedded history revision could not be read."));
            }
            const auto revision = fromJson(QJsonDocument::fromJson(file.readAll()).object());
            if (!revision.isValid() || revision.databasePath != identity) return fail(tr("The embedded history contains a foreign revision."));
            if (!revision.contentFingerprint.isEmpty()) {
                hasFingerprint = true;
                if (revision.contentFingerprint == currentFingerprint
                    && (!matchingRevision.isValid() || revision.timestamp > matchingRevision.timestamp)) {
                    matchingRevision = revision;
                }
            }
            HistoryRevision normalized = revision;
            normalized.snapshotPath = revision.snapshotPath.isEmpty()
                                          ? QString()
                                          : QStringLiteral("%1/%2/%3.kdbx").arg(SnapshotsName, identity, revision.id);
            const auto existing = std::find_if(m_revisions.cbegin(), m_revisions.cend(), [&normalized](const HistoryRevision& value) {
                return value.id == normalized.id;
            });
            if (existing != m_revisions.cend()) {
                if (existing->databasePath != normalized.databasePath || existing->snapshotSha256 != normalized.snapshotSha256
                    || existing->timestamp != normalized.timestamp || existing->contentFingerprint != normalized.contentFingerprint) {
                    return fail(tr("An embedded revision conflicts with a revision already recorded on this device."));
                }
                continue;
            }
            m_revisions.append(normalized);
            changed = true;
        }
        if (hasFingerprint && !matchingRevision.isValid()) {
            return fail(tr("No embedded revision matches the current database contents."));
        }
        if (matchingRevision.isValid()) {
            QFile fingerprint(QDir(repo).filePath(QStringLiteral("fingerprints/%1.json").arg(matchingRevision.id)));
            if (!fingerprint.open(QIODevice::ReadOnly) || fingerprint.size() > 16 * 1024 * 1024) {
                return fail(tr("The matching entry fingerprint is missing or exceeds its size limit."));
            }
            const QByteArray fingerprintBytes = fingerprint.readAll();
            if (fingerprintDigest(fingerprintBytes) != currentFingerprint
                || !atomicReplace(fingerprintPath(identity), fingerprintBytes)) {
                return fail(tr("The matching entry fingerprint could not be restored to local history."));
            }
        }
        if (changed) {
            std::sort(m_revisions.begin(), m_revisions.end(), [](const HistoryRevision& first, const HistoryRevision& second) {
                return first.timestamp < second.timestamp;
            });
            emit revisionsChanged();
        }
        if (error) error->clear();
        return true;
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
        for (const HistoryRevision& cached : m_revisions) {
            if (std::none_of(committed.cbegin(), committed.cend(), [&cached](const HistoryRevision& value) { return value.id == cached.id; })) {
                committed.append(cached);
            }
        }
        std::sort(committed.begin(), committed.end(), [](const HistoryRevision& first, const HistoryRevision& second) {
            return first.timestamp < second.timestamp;
        });
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

    HistoryRevision HistoryStore::createSaveRevision(const QSharedPointer<Database>& db,
                                                     QByteArray* fingerprintBytes,
                                                     QByteArray* encryptedSnapshot) const
    {
        if (!db || !db->rootGroup() || db->filePath().isEmpty()) return {};
        const_cast<HistoryStore*>(this)->load();
        const QString identity = databaseIdentity(db);
        if (!isDatabaseIdentity(identity)) return {};
        int entryCount = 0;
        int groupCount = 0;
        const auto current = fingerprintOf(db, &entryCount, &groupCount);
        const QByteArray currentFingerprint = fingerprintJson(current);
        const bool hadFingerprint = QFileInfo::exists(fingerprintPath(identity));
        const auto previous = readFingerprint(fingerprintPath(identity));

        HistoryRevision revision;
        revision.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        revision.timestamp = QDateTime::currentDateTimeUtc();
        revision.databasePath = identity;
        revision.databaseName = QStringLiteral("Database %1").arg(identity.left(8));
        revision.entryCount = entryCount;
        revision.groupCount = groupCount;
        revision.contentFingerprint = fingerprintDigest(currentFingerprint);
        const HistoryRevision last = revisionsForDatabase(db).value(0);
        if (!hadFingerprint || !last.isValid()) {
            revision.kind = RevisionKind::Entry;
            revision.label = tr("First recorded save of this database - %n entry(s)", "", entryCount);
        } else {
            for (auto it = current.cbegin(); it != current.cend(); ++it) {
                const auto old = previous.constFind(it.key());
                if (old == previous.cend()) ++revision.added;
                else if (old.value() != it.value()) ++revision.edited;
            }
            for (auto it = previous.cbegin(); it != previous.cend(); ++it) {
                if (!current.contains(it.key())) ++revision.removed;
            }
            const int groupDelta = groupCount - last.groupCount;
            QStringList parts;
            if (revision.added) parts << tr("%n entry(s) added", "", revision.added);
            if (revision.removed) parts << tr("%n entry(s) removed", "", revision.removed);
            if (revision.edited) parts << tr("%n entry(s) edited", "", revision.edited);
            if (groupDelta > 0) parts << tr("%n group(s) added", "", groupDelta);
            if (groupDelta < 0) parts << tr("%n group(s) removed", "", -groupDelta);
            revision.label = parts.isEmpty() ? tr("Saved with no entry or group changes") : parts.join(tr(", "));
            revision.kind = revision.added + revision.removed + revision.edited > 0
                                ? RevisionKind::Entry
                                : groupDelta != 0 ? RevisionKind::Group : RevisionKind::Settings;
        }

        QString snapshotError;
        const QByteArray snapshotBytes = serializeDatabaseWithoutEmbeddedHistory(db, &snapshotError);
        QByteArray snapshot = snapshotBytes;
        if (isKdbx(snapshot)) {
            revision.snapshotPath = QStringLiteral("%1/%2/%3.kdbx").arg(SnapshotsName, identity, revision.id);
            revision.snapshotSha256 = byteDigest(snapshot);
        } else {
            snapshot.clear();
        }
        if (fingerprintBytes) *fingerprintBytes = currentFingerprint;
        if (encryptedSnapshot) *encryptedSnapshot = snapshot;
        return revision;
    }

    bool HistoryStore::recordSave(const QSharedPointer<Database>& db)
    {
        if (!db || !db->rootGroup() || db->filePath().isEmpty()) return false;
        auto pending = m_databaseSaves.find(db.data());
        if (pending != m_databaseSaves.end() && pending->staged) {
            if (pending->finalizationAttempted) return pending->finalizationSucceeded;
            pending->finalizationAttempted = true;
            const DatabaseSaveState state = pending.value();
            const bool globalRecorded = commitTransaction(state.stagedRevision, state.stagedFingerprint, state.stagedSnapshot);
            QString mergeError;
            const bool repositoryMerged = mergeStagedDatabaseRepository(state.databaseIdentity, state.stagingRepository, &mergeError);
            QString loadError;
            const bool revisionsLoaded = repositoryMerged
                                             && loadDatabaseRepositoryRevisions(state.databaseIdentity,
                                                                                state.stagedRevision.contentFingerprint,
                                                                                &loadError);
            pending->finalizationSucceeded = globalRecorded && repositoryMerged && revisionsLoaded;
            if (!globalRecorded || !repositoryMerged || !revisionsLoaded) {
                const QString reason = !globalRecorded
                                           ? tr("The database was saved with embedded history, but the application history index could not be updated.")
                                           : !repositoryMerged ? mergeError : loadError;
                emit writeFailed(reason);
                return false;
            }
            return true;
        }
        load();
        QByteArray currentFingerprint;
        QByteArray encryptedSnapshot;
        const HistoryRevision revision = createSaveRevision(db, &currentFingerprint, &encryptedSnapshot);
        if (!revision.isValid() || !commitTransaction(revision, currentFingerprint, encryptedSnapshot)
            || !commitDatabaseRepository(revision, encryptedSnapshot, currentFingerprint)) {
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
        const QByteArray currentFingerprint = fingerprintJson(fingerprint);
        HistoryRevision revision;
        revision.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        revision.timestamp = QDateTime::currentDateTimeUtc();
        revision.databasePath = databaseIdentity(db);
        if (!isDatabaseIdentity(revision.databasePath)) return false;
        revision.databaseName = QStringLiteral("Database %1").arg(revision.databasePath.left(8));
        revision.label = redactedLabel;
        revision.kind = kind;
        revision.entryCount = entries;
        revision.groupCount = groups;
        revision.contentFingerprint = fingerprintDigest(currentFingerprint);
        if (!commitTransaction(revision, currentFingerprint)) {
            emit writeFailed(tr("Local history could not be recorded. The database operation completed; retry after checking Git and application-data storage."));
            return false;
        }
        if (!commitDatabaseEvent(revision, currentFingerprint)) {
            emit writeFailed(tr("The database operation completed, but its event could not be copied into encrypted database history."));
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
    QVector<HistoryRevision> HistoryStore::revisionsForDatabase(const QSharedPointer<Database>& db) const
    {
        return db ? revisionsFor(databaseIdentity(db)) : QVector<HistoryRevision>();
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
        QByteArray bytes;
        if (file.open(QIODevice::ReadOnly)) {
            bytes = file.readAll();
        } else {
            QFile embedded(QDir(databaseRepositoryPath(value.databasePath)).filePath(QStringLiteral("snapshots/%1.kdbx").arg(value.id)));
            if (!embedded.open(QIODevice::ReadOnly)) return fail(tr("The encrypted snapshot file is missing from local and embedded history."));
            bytes = embedded.readAll();
        }
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
