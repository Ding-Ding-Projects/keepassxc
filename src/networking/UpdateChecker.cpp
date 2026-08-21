/*
 *  Copyright (C) 2025 KeePassXC Team <team@keepassxc.org>
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

#include "UpdateChecker.h"

#include "NetworkManager.h"
#include "config-keepassx.h"
#include "core/Clock.h"
#include "core/Config.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QSet>
#include <QXmlStreamReader>
#include <QProcess>
#include <QCoreApplication>
#include <QUuid>

#include <../minizip/unzip.h>

namespace
{
    constexpr qsizetype MaxManifestBytes = 64 * 1024;
    const QUrl ManifestUrl(QStringLiteral("https://github.com/Ding-Ding-Projects/keepassxc/releases/latest/download/update-manifest-v1.json"));
}

const QString UpdateChecker::ErrorVersion("error");
UpdateChecker* UpdateChecker::m_instance(nullptr);

UpdateChecker::UpdateChecker(QObject* parent)
    : QObject(parent)
    , m_reply(nullptr)
    , m_isManuallyRequested(false)
{
}

UpdateChecker::~UpdateChecker()
{
    cancelDownload();
}

void UpdateChecker::checkForUpdates(bool manuallyRequested)
{
    // Skip update if we are already performing one
    if (m_reply) {
        return;
    }

    auto nextCheck = config()->get(Config::GUI_CheckForUpdatesNextCheck).toULongLong();
    m_isManuallyRequested = manuallyRequested;

    if (m_isManuallyRequested || Clock::currentSecondsSinceEpoch() >= nextCheck) {
        m_bytesReceived.clear();
        setState(State::Checking);
        QNetworkRequest request(ManifestUrl);
        request.setRawHeader("Accept", "application/json");
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);

        m_reply = getNetMgr()->get(request);

        connect(m_reply, &QNetworkReply::finished, this, &UpdateChecker::fetchFinished);
        connect(m_reply, &QIODevice::readyRead, this, &UpdateChecker::fetchReadyRead);
    }
}

void UpdateChecker::fetchReadyRead()
{
    m_bytesReceived += m_reply->readAll();
    if (m_bytesReceived.size() > MaxManifestBytes) {
        m_reply->abort();
    }
}

void UpdateChecker::fetchFinished()
{
    bool error = (m_reply->error() != QNetworkReply::NoError);
    bool hasNewVersion = false;
    QString version = "";
    const bool redirected = m_reply->attribute(QNetworkRequest::RedirectionTargetAttribute).isValid();

    m_reply->deleteLater();
    m_reply = nullptr;

    if (!error && !redirected) {
        Candidate parsed;
        Failure failure = Failure::None;
        if (parseManifest(m_bytesReceived, parsed, failure)) {
            m_candidate = parsed;
            version = parsed.version;
            hasNewVersion = compareVersions(QString(KEEPASSXC_VERSION), version);
            setState(hasNewVersion ? State::Available : State::NoUpdate);
        } else {
            error = true;
            version = ErrorVersion;
            setState(State::Failed, failure);
        }

        if (!error) {
            // Check again in 7 days only after a validated manifest response.
            config()->set(Config::GUI_CheckForUpdatesNextCheck,
                          Clock::currentDateTime().addDays(7).toSecsSinceEpoch());
        }
    } else {
        version = ErrorVersion;
        if (redirected) {
            setState(State::Failed, Failure::RedirectRejected);
        } else if (m_bytesReceived.size() > MaxManifestBytes) {
            setState(State::Failed, Failure::OversizedManifest);
        } else {
            setState(State::Failed, Failure::Offline);
        }
    }

    emit updateCheckFinished(hasNewVersion, version, m_isManuallyRequested);
}

void UpdateChecker::downloadAvailableUpdate()
{
    if (m_state != State::Available || m_downloadReply) {
        return;
    }
    const QUrl packageUrl(m_candidate.packageUrl);
    if (!packageUrl.isValid() || packageUrl.scheme() != QStringLiteral("https")) {
        setState(State::Failed, Failure::MalformedManifest);
        return;
    }
    const QString updateDirectory = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
                                    + QStringLiteral("/updates");
    if (!QDir().mkpath(updateDirectory)) {
        setState(State::Failed, Failure::InsufficientStorage);
        return;
    }
    QStorageInfo storage(updateDirectory);
    if (!storage.isValid() || storage.bytesAvailable() < qint64(m_candidate.bytes + 64 * 1024 * 1024ULL)) {
        setState(State::Failed, Failure::InsufficientStorage);
        return;
    }

    const QString destination = QDir(updateDirectory).filePath(m_candidate.packageFile);
    m_downloadFile = new QSaveFile(destination, this);
    if (!m_downloadFile->open(QIODevice::WriteOnly)) {
        failDownload(Failure::InsufficientStorage);
        return;
    }
    m_downloadHash = new QCryptographicHash(QCryptographicHash::Sha256);
    m_downloadBytes = 0;
    const quint64 generation = ++m_generation;
    QNetworkRequest request(packageUrl);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(30000);
    m_downloadReply = getNetMgr()->get(request);
    setState(State::Downloading);
    connect(m_downloadReply, &QIODevice::readyRead, this, [this, generation] {
        if (generation != m_generation || !m_downloadReply || !m_downloadFile) {
            return;
        }
        const QByteArray chunk = m_downloadReply->readAll();
        m_downloadBytes += quint64(chunk.size());
        if (m_downloadBytes > m_candidate.bytes || m_downloadFile->write(chunk) != chunk.size()) {
            m_downloadReply->abort();
            return;
        }
        m_downloadHash->addData(chunk);
        emit downloadProgress(m_downloadBytes, m_candidate.bytes);
    });
    connect(m_downloadReply, &QNetworkReply::finished, this, [this, generation] { finishDownload(generation); });
}

void UpdateChecker::cancelDownload()
{
    if (!m_downloadReply && !m_downloadFile) {
        return;
    }
    ++m_generation;
    if (m_downloadReply) {
        m_downloadReply->abort();
        m_downloadReply->deleteLater();
        m_downloadReply = nullptr;
    }
    if (m_downloadFile) {
        m_downloadFile->cancelWriting();
        delete m_downloadFile;
        m_downloadFile = nullptr;
    }
    delete m_downloadHash;
    m_downloadHash = nullptr;
    m_downloadBytes = 0;
    if (m_state == State::Downloading) {
        setState(State::Failed, Failure::Cancelled);
    }
}

void UpdateChecker::finishDownload(quint64 generation)
{
    if (generation != m_generation || !m_downloadReply || !m_downloadFile || !m_downloadHash) {
        return;
    }
    const bool networkOk = m_downloadReply->error() == QNetworkReply::NoError;
    m_downloadReply->deleteLater();
    m_downloadReply = nullptr;
    if (!networkOk || m_downloadBytes != m_candidate.bytes) {
        failDownload(networkOk ? Failure::ByteCountMismatch : Failure::Offline);
        return;
    }
    const QString sha256 = QString::fromLatin1(m_downloadHash->result().toHex());
    delete m_downloadHash;
    m_downloadHash = nullptr;
    if (sha256 != m_candidate.sha256) {
        failDownload(Failure::Sha256Mismatch);
        return;
    }
    const QString destination = m_downloadFile->fileName();
    if (!m_downloadFile->commit()) {
        failDownload(Failure::InsufficientStorage);
        return;
    }
    delete m_downloadFile;
    m_downloadFile = nullptr;
    setState(State::Verifying);
    Failure failure = Failure::None;
    if (!verifyPackage(destination, m_candidate, failure)) {
        QFile::remove(destination);
        setState(State::Failed, failure);
        return;
    }
    emit updatePackageReady(destination);
}

void UpdateChecker::failDownload(Failure failure)
{
    if (m_downloadFile) {
        m_downloadFile->cancelWriting();
        delete m_downloadFile;
        m_downloadFile = nullptr;
    }
    delete m_downloadHash;
    m_downloadHash = nullptr;
    m_downloadBytes = 0;
    setState(State::Failed, failure);
}

void UpdateChecker::applyVerifiedUpdate(const QString& packagePath)
{
    if (m_state != State::Verifying || m_applyProcess) {
        return;
    }
    const QString appDirectory = QDir::cleanPath(QCoreApplication::applicationDirPath());
    const QFileInfo appDirectoryInfo(appDirectory);
    static const QRegularExpression appDirectoryPattern(
        QStringLiteral("^app-\\d+\\.\\d+\\.\\d+(?:[-+][0-9A-Za-z.-]+)?$"));
    if (!appDirectoryPattern.match(appDirectoryInfo.fileName()).hasMatch()) {
        setState(State::Failed, Failure::UpdaterMissing);
        return;
    }
    QDir packageRoot = appDirectoryInfo.dir();
    const QString updaterPath = packageRoot.filePath(QStringLiteral("Update.exe"));
    const QFileInfo updaterInfo(updaterPath);
    if (!updaterInfo.isFile() || updaterInfo.isSymLink()
        || QDir::cleanPath(updaterInfo.canonicalFilePath()) != QDir::cleanPath(updaterInfo.absoluteFilePath())) {
        setState(State::Failed, Failure::UpdaterMissing);
        return;
    }

    const QString feedPath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
                             + QStringLiteral("/updates/verified-feed-")
                             + QUuid::createUuid().toString(QUuid::WithoutBraces);
    QDir feed;
    if (!feed.mkpath(feedPath)) {
        setState(State::Failed, Failure::InsufficientStorage);
        return;
    }
    const QString feedPackage = QDir(feedPath).filePath(m_candidate.packageFile);
    if (!QFile::copy(packagePath, feedPackage)) {
        QDir(feedPath).removeRecursively();
        setState(State::Failed, Failure::InsufficientStorage);
        return;
    }
    QSaveFile releases(QDir(feedPath).filePath(QStringLiteral("RELEASES")));
    if (!releases.open(QIODevice::WriteOnly)
        || releases.write(QStringLiteral("%1 %2 %3\n")
                              .arg(m_candidate.releasesSha1, m_candidate.packageFile)
                              .arg(m_candidate.bytes)
                              .toUtf8()) <= 0
        || !releases.commit()) {
        QDir(feedPath).removeRecursively();
        setState(State::Failed, Failure::UnsafePackage);
        return;
    }

    m_applyProcess = new QProcess(this);
    m_applyProcess->setProgram(updaterPath);
    m_applyProcess->setArguments({QStringLiteral("--update"), QDir::toNativeSeparators(feedPath)});
    m_applyProcess->setWorkingDirectory(packageRoot.absolutePath());
    connect(m_applyProcess, &QProcess::errorOccurred, this, [this, feedPath](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart && m_applyProcess) {
            m_applyProcess->deleteLater();
            m_applyProcess = nullptr;
            QDir(feedPath).removeRecursively();
            setState(State::Failed, Failure::UpdaterStartFailed);
        }
    });
    connect(m_applyProcess,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this,
            [this, feedPath, packageRoot](int exitCode, QProcess::ExitStatus status) {
                if (!m_applyProcess) {
                    return;
                }
                m_applyProcess->deleteLater();
                m_applyProcess = nullptr;
                QDir(feedPath).removeRecursively();
                if (status != QProcess::NormalExit || exitCode != 0) {
                    setState(State::Failed, Failure::UpdaterApplyFailed);
                    return;
                }
                const QString installedExe = packageRoot.filePath(
                    QStringLiteral("app-%1/KeePassXC.exe").arg(m_candidate.version));
                QFile installed(installedExe);
                if (!installed.open(QIODevice::ReadOnly)) {
                    setState(State::Failed, Failure::AppliedVersionMissing);
                    return;
                }
                QCryptographicHash hash(QCryptographicHash::Sha256);
                while (!installed.atEnd()) {
                    hash.addData(installed.read(1024 * 1024));
                }
                if (QString::fromLatin1(hash.result().toHex()) != m_candidate.executableSha256) {
                    setState(State::Failed, Failure::AppliedVersionMissing);
                    return;
                }
                setState(State::ReadyToRestart);
                emit updateReadyToRestart(m_candidate.version);
            });
    setState(State::Applying);
    m_applyProcess->start();
}

UpdateChecker::State UpdateChecker::state() const { return m_state; }
UpdateChecker::Failure UpdateChecker::failure() const { return m_failure; }
UpdateChecker::Candidate UpdateChecker::candidate() const { return m_candidate; }

void UpdateChecker::setState(State state, Failure failure)
{
    if (!transitionAllowed(m_state, state)) {
        m_state = State::Failed;
        m_failure = Failure::MalformedManifest;
    } else {
        m_state = state;
        m_failure = failure;
    }
    emit stateChanged(m_state, m_failure);
}

bool UpdateChecker::transitionAllowed(State from, State to)
{
    if (to == State::Failed) return true;
    switch (from) {
    case State::Idle: case State::NoUpdate: case State::Deferred: case State::Failed: return to == State::Checking;
    case State::Checking: return to == State::NoUpdate || to == State::Available;
    case State::Available: return to == State::Checking || to == State::Downloading || to == State::Deferred;
    case State::Downloading: return to == State::Verifying;
    case State::Verifying: return to == State::Applying;
    case State::Applying: return to == State::ReadyToRestart;
    case State::ReadyToRestart: return to == State::Deferred || to == State::Restarting;
    default: return false;
    }
}

bool UpdateChecker::parseManifest(const QByteArray& bytes, Candidate& candidate, Failure& failure)
{
    if (bytes.size() > MaxManifestBytes) { failure = Failure::OversizedManifest; return false; }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) { failure = Failure::MalformedManifest; return false; }
    const auto object = document.object();
    if (object.value(QStringLiteral("schemaVersion")).toInt() != 1 || object.value(QStringLiteral("packageId")).toString() != QStringLiteral("KeePassXC.Material")) { failure = Failure::PackageIdentityMismatch; return false; }
    if (object.value(QStringLiteral("architecture")).toString() != QStringLiteral("x64")) { failure = Failure::ArchitectureMismatch; return false; }
    candidate.version = object.value(QStringLiteral("version")).toString();
    candidate.notesUrl = object.value(QStringLiteral("notesUrl")).toString();
    candidate.packageUrl = object.value(QStringLiteral("packageUrl")).toString();
    candidate.packageFile = object.value(QStringLiteral("packageFile")).toString();
    candidate.sha256 = object.value(QStringLiteral("sha256")).toString().toLower();
    candidate.releasesSha1 = object.value(QStringLiteral("releasesSha1")).toString().toLower();
    candidate.executableSha256 = object.value(QStringLiteral("executableSha256")).toString().toLower();
    candidate.bytes = object.value(QStringLiteral("bytes")).toVariant().toULongLong();
    static const QRegularExpression version(QStringLiteral("^\\d+\\.\\d+\\.\\d+$"));
    static const QRegularExpression sha256(QStringLiteral("^[0-9a-f]{64}$"));
    static const QRegularExpression sha1(QStringLiteral("^[0-9a-f]{40}$"));
    const QUrl notes(candidate.notesUrl), package(candidate.packageUrl);
    if (!version.match(candidate.version).hasMatch()) { failure = Failure::InvalidVersion; return false; }
    if (!notes.isValid() || notes.scheme() != QStringLiteral("https") || !package.isValid() || package.scheme() != QStringLiteral("https") || candidate.packageFile.isEmpty() || candidate.packageFile.contains(QLatin1Char('/')) || candidate.packageFile.contains(QLatin1Char('\\')) || candidate.packageFile.contains(QStringLiteral("..")) || candidate.bytes == 0 || candidate.bytes > 1610612736ULL || !sha256.match(candidate.sha256).hasMatch() || !sha256.match(candidate.executableSha256).hasMatch() || !sha1.match(candidate.releasesSha1).hasMatch()) { failure = Failure::MalformedManifest; return false; }
    failure = Failure::None;
    return true;
}

bool UpdateChecker::verifyPackage(const QString& path, const Candidate& candidate, Failure& failure)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || quint64(file.size()) != candidate.bytes) {
        failure = Failure::ByteCountMismatch;
        return false;
    }
    QCryptographicHash sha1(QCryptographicHash::Sha1);
    while (!file.atEnd()) {
        const QByteArray chunk = file.read(1024 * 1024);
        if (chunk.isEmpty() && file.error() != QFile::NoError) {
            failure = Failure::UnsafePackage;
            return false;
        }
        sha1.addData(chunk);
    }
    if (QString::fromLatin1(sha1.result().toHex()) != candidate.releasesSha1) {
        failure = Failure::ReleasesMismatch;
        return false;
    }

    const QByteArray nativePath = QFile::encodeName(QFileInfo(path).absoluteFilePath());
    unzFile archive = unzOpen64(nativePath.constData());
    if (!archive) {
        failure = Failure::UnsafePackage;
        return false;
    }
    QSet<QString> normalizedEntries;
    bool hasApplication = false;
    QString applicationSha256;
    QByteArray nuspec;
    int result = unzGoToFirstFile(archive);
    while (result == UNZ_OK) {
        unz_file_info64 info{};
        char nameBuffer[4096]{};
        if (unzGetCurrentFileInfo64(archive, &info, nameBuffer, sizeof(nameBuffer), nullptr, 0, nullptr, 0) != UNZ_OK) {
            unzClose(archive);
            failure = Failure::UnsafePackage;
            return false;
        }
        const QString entry = QString::fromUtf8(nameBuffer).replace(QLatin1Char('\\'), QLatin1Char('/'));
        const QString normalized = entry.toLower();
        if (entry.startsWith(QLatin1Char('/')) || entry.contains(QStringLiteral("../"))
            || entry.startsWith(QStringLiteral("../")) || entry.contains(QLatin1Char(':'))
            || normalizedEntries.contains(normalized)) {
            unzClose(archive);
            failure = Failure::UnsafePackage;
            return false;
        }
        normalizedEntries.insert(normalized);
        if (normalized == QStringLiteral("lib/net45/keepassxc.exe")) {
            hasApplication = true;
            if (unzOpenCurrentFile(archive) != UNZ_OK) {
                unzClose(archive);
                failure = Failure::UnsafePackage;
                return false;
            }
            QCryptographicHash applicationHash(QCryptographicHash::Sha256);
            QByteArray buffer(1024 * 1024, Qt::Uninitialized);
            int read = 0;
            while ((read = unzReadCurrentFile(archive, buffer.data(), unsigned(buffer.size()))) > 0) {
                applicationHash.addData(buffer.constData(), read);
            }
            unzCloseCurrentFile(archive);
            if (read < 0) {
                unzClose(archive);
                failure = Failure::UnsafePackage;
                return false;
            }
            applicationSha256 = QString::fromLatin1(applicationHash.result().toHex());
        }
        if (normalized.endsWith(QStringLiteral(".nuspec"))) {
            if (info.uncompressed_size > 64 * 1024 || unzOpenCurrentFile(archive) != UNZ_OK) {
                unzClose(archive);
                failure = Failure::UnsafePackage;
                return false;
            }
            nuspec.resize(qsizetype(info.uncompressed_size));
            const int read = unzReadCurrentFile(archive, nuspec.data(), unsigned(nuspec.size()));
            unzCloseCurrentFile(archive);
            if (read != nuspec.size()) {
                unzClose(archive);
                failure = Failure::UnsafePackage;
                return false;
            }
        }
        result = unzGoToNextFile(archive);
    }
    unzClose(archive);
    if (result != UNZ_END_OF_LIST_OF_FILE || !hasApplication || nuspec.isEmpty()
        || applicationSha256 != candidate.executableSha256) {
        failure = Failure::UnsafePackage;
        return false;
    }

    QString packageId;
    QString packageVersion;
    QXmlStreamReader xml(nuspec);
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement() && xml.name() == QStringLiteral("id")) {
            packageId = xml.readElementText();
        } else if (xml.isStartElement() && xml.name() == QStringLiteral("version")) {
            packageVersion = xml.readElementText();
        }
    }
    if (xml.hasError() || packageId != QStringLiteral("KeePassXC.Material") || packageVersion != candidate.version) {
        failure = Failure::PackageIdentityMismatch;
        return false;
    }
    failure = Failure::None;
    return true;
}

bool UpdateChecker::compareVersions(const QString& localVersion, const QString& remoteVersion)
{
    // Quick full-string equivalence check
    if (localVersion == remoteVersion) {
        return false;
    }

    QRegularExpression verRegex(R"(^((?:\d+\.){2}\d+)(?:-(\w+?)(\d+)?)?$)");

    auto lmatch = verRegex.match(localVersion);
    auto rmatch = verRegex.match(remoteVersion);

    auto lVersion = lmatch.captured(1).split(".");
    auto lSuffix = lmatch.captured(2);
    auto lBetaNum = lmatch.captured(3);

    auto rVersion = rmatch.captured(1).split(".");
    auto rSuffix = rmatch.captured(2);
    auto rBetaNum = rmatch.captured(3);

    if (!lVersion.isEmpty() && !rVersion.isEmpty()) {
        if (lSuffix.compare("snapshot", Qt::CaseInsensitive) == 0) {
            // Snapshots are not checked for version updates
            return false;
        }

        // Check "-beta[X]" versions
        if (lVersion == rVersion && !lSuffix.isEmpty()) {
            // Check if stable version has been released or new beta is available
            // otherwise the version numbers are equal
            return rSuffix.isEmpty() || lBetaNum.toInt() < rBetaNum.toInt();
        }

        for (int i = 0; i < 3; i++) {
            int l = lVersion[i].toInt();
            int r = rVersion[i].toInt();

            if (l == r) {
                continue;
            }

            if (l > r) {
                return false; // Installed version is newer than release
            } else {
                return true; // Installed version is outdated
            }
        }

        return false; // Installed version is the same
    }

    return false; // Invalid version string
}

UpdateChecker* UpdateChecker::instance()
{
    if (!m_instance) {
        m_instance = new UpdateChecker();
    }

    return m_instance;
}
