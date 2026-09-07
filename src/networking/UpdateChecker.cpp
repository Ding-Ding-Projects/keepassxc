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
#include "platform/SquirrelLifecycle.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
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
#include <QTimer>

#include <../minizip/unzip.h>

namespace
{
    constexpr qsizetype MaxManifestBytes = 64 * 1024;
    constexpr qsizetype MaxReleaseIndexBytes = 256 * 1024;
    const QUrl ManifestUrl(QStringLiteral("https://github.com/Ding-Ding-Projects/keepassxc/releases/latest/download/update-manifest-v1.json"));
    const QUrl ReleasesUrl(QStringLiteral("https://api.github.com/repos/Ding-Ding-Projects/keepassxc/releases?per_page=20"));

    bool packageVersionAllowed(const QString& version)
    {
        static const QRegularExpression pattern(QStringLiteral("^(0|[1-9][0-9]{0,4})[.](0|[1-9][0-9]{0,4})[.](0|[1-9][0-9]{0,4})$"));
        const auto match = pattern.match(version);
        return match.hasMatch() && match.capturedLength() == version.size() && match.captured(1).toUInt() <= 65535
               && match.captured(2).toUInt() <= 65535 && match.captured(3).toUInt() <= 65535;
    }
}

const QString UpdateChecker::ErrorVersion("error");
UpdateChecker* UpdateChecker::m_instance(nullptr);
UpdateChecker::RestartLauncher UpdateChecker::m_restartLauncher;

UpdateChecker::UpdateChecker(QObject* parent)
    : QObject(parent)
    , m_reply(nullptr)
    , m_isManuallyRequested(false)
{
}

UpdateChecker::~UpdateChecker()
{
    ++m_manifestGeneration;
    if (m_reply) {
        m_reply->disconnect(this);
        m_reply->abort();
        m_reply->deleteLater();
    }
    cancelDownload();
}

void UpdateChecker::checkForUpdates(bool manuallyRequested)
{
    // A manifest query cannot safely replace an active package transfer or
    // updater process. Keep the active operation and its state authoritative.
    if (m_reply || m_downloadReply || m_applyProcess || !transitionAllowed(m_state, State::Checking)) {
        return;
    }

    auto nextCheck = config()->get(Config::GUI_CheckForUpdatesNextCheck).toULongLong();
    m_isManuallyRequested = manuallyRequested;

    if (m_isManuallyRequested || Clock::currentSecondsSinceEpoch() >= nextCheck) {
        m_bytesReceived.clear();
        m_expectedReleaseVersion.clear();
        m_candidate = {};
        m_redirectRejected = false;
        QPointer<UpdateChecker> self(this);
        setState(State::Checking);
        if (!self) {
            return;
        }
        beginManifestRequest(config()->get(Config::GUI_CheckForUpdatesIncludeBetas).toBool() ? ReleasesUrl : ManifestUrl,
                             config()->get(Config::GUI_CheckForUpdatesIncludeBetas).toBool());
    }
}

void UpdateChecker::beginManifestRequest(const QUrl& url, bool prereleaseIndex)
{
    m_fetchingPrereleaseIndex = prereleaseIndex;
    m_bytesReceived.clear();
    m_redirectRejected = false;
    QNetworkRequest request(url);
    request.setRawHeader("Accept", prereleaseIndex ? "application/vnd.github+json" : "application/json");
    request.setRawHeader("User-Agent", "KeePassXC-Material-Updater/1");
    // Validate the destination before Qt sends the redirected request.
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::UserVerifiedRedirectPolicy);
    request.setMaximumRedirectsAllowed(5);
    request.setTransferTimeout(30000);

    const quint64 generation = ++m_manifestGeneration;
    m_reply = networkManager()->get(request);
    QNetworkReply* const reply = m_reply;
    reply->setReadBufferSize((prereleaseIndex ? MaxReleaseIndexBytes : MaxManifestBytes) + 1);
    // Transfer timeout is an inactivity limit. Bound the whole request too,
    // including a peer that trickles data without ever becoming idle.
    QTimer::singleShot(30000, this, [this, generation, reply] {
        if (generation == m_manifestGeneration && m_reply == reply) {
            reply->abort();
        }
    });
    connect(reply, &QNetworkReply::redirected, this, [this, generation, reply, prereleaseIndex](const QUrl& target) {
        if (generation != m_manifestGeneration || m_reply != reply) {
            return;
        }
        // The fixed REST endpoint needs no redirects. Asset requests may
        // redirect to GitHub storage or this repository's download path.
        bool repositoryAsset = true;
        QString redirectedVersion;
        if (target.host() == QStringLiteral("github.com")) {
            const QString prefix = QStringLiteral("/Ding-Ding-Projects/keepassxc/releases/download/v");
            const QString suffix = QStringLiteral("/update-manifest-v1.json");
            const QString path = target.path();
            redirectedVersion = path.mid(prefix.size(), path.size() - prefix.size() - suffix.size());
            repositoryAsset = path.startsWith(prefix) && path.endsWith(suffix)
                && packageVersionAllowed(redirectedVersion) && !target.hasQuery()
                && (m_expectedReleaseVersion.isEmpty() || redirectedVersion == m_expectedReleaseVersion);
        }
        if (prereleaseIndex || !redirectAllowed(target) || !repositoryAsset) {
            m_redirectRejected = true;
            reply->abort();
        } else {
            if (!redirectedVersion.isEmpty()) {
                m_expectedReleaseVersion = redirectedVersion;
            }
            reply->redirectAllowed();
        }
    });
    connect(reply, &QNetworkReply::finished, this, [this, generation, reply] {
        if (generation == m_manifestGeneration && m_reply == reply) {
            fetchFinished();
        }
    });
    connect(reply, &QIODevice::readyRead, this, [this, generation, reply] {
        if (generation == m_manifestGeneration && m_reply == reply) {
            fetchReadyRead();
        }
    });
    connect(reply, &QObject::destroyed, this, [this, generation] {
        if (generation == m_manifestGeneration && m_state == State::Checking) {
            ++m_manifestGeneration;
            m_reply = nullptr;
            failCheck(Failure::Offline);
        }
    });
}

void UpdateChecker::fetchReadyRead()
{
    if (!m_reply) {
        return;
    }
    // Retain only the bound plus one sentinel byte, even for a single huge chunk.
    const qsizetype limit = m_fetchingPrereleaseIndex ? MaxReleaseIndexBytes : MaxManifestBytes;
    m_bytesReceived += m_reply->read(limit + 1 - m_bytesReceived.size());
    if (m_bytesReceived.size() > limit) {
        m_reply->abort();
    }
}

void UpdateChecker::fetchFinished()
{
    const bool prereleaseIndex = m_fetchingPrereleaseIndex;
    const qsizetype limit = prereleaseIndex ? MaxReleaseIndexBytes : MaxManifestBytes;
    // A reply may expose its final bytes only with finished(). Do not abort
    // recursively while processing this terminal signal.
    m_bytesReceived += m_reply->read(limit + 1 - m_bytesReceived.size());
    const auto networkError = m_reply->error();
    const int status = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    ++m_manifestGeneration;
    m_reply->disconnect(this);
    m_reply->deleteLater();
    m_reply = nullptr;

    if (m_redirectRejected) {
        failCheck(Failure::RedirectRejected);
        return;
    }
    if (m_bytesReceived.size() > limit) {
        failCheck(Failure::OversizedManifest);
        return;
    }
    if (networkError == QNetworkReply::TimeoutError || networkError == QNetworkReply::OperationCanceledError) {
        failCheck(Failure::Timeout);
        return;
    }
    // Rate limits, missing assets and server errors never become a successful
    // stable fallback, even when their bodies happen to contain valid JSON.
    if (networkError != QNetworkReply::NoError || status != 200) {
        failCheck(Failure::Offline);
        return;
    }
    if (prereleaseIndex) {
        QJsonParseError parseError;
        const auto document = QJsonDocument::fromJson(m_bytesReceived, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isArray() || document.array().size() > 20) {
            failCheck(Failure::MalformedManifest);
            return;
        }
        QUrl manifestUrl;
        QString selectedVersion;
        for (const auto value : document.array()) {
            if (!value.isObject()) {
                failCheck(Failure::MalformedManifest);
                return;
            }
            const auto release = value.toObject();
            if (!release.value(QStringLiteral("draft")).isBool()
                || !release.value(QStringLiteral("prerelease")).isBool()
                || !release.value(QStringLiteral("tag_name")).isString()
                || !release.value(QStringLiteral("assets")).isArray()) {
                failCheck(Failure::MalformedManifest);
                return;
            }
            if (release.value(QStringLiteral("draft")).toBool()) {
                continue;
            }
            const QString tag = release.value(QStringLiteral("tag_name")).toString();
            const QString version = tag.mid(1);
            if (!tag.startsWith(QLatin1Char('v')) || !packageVersionAllowed(version)) {
                continue;
            }
            const QString expectedUrl = QStringLiteral("https://github.com/Ding-Ding-Projects/keepassxc/releases/download/%1/update-manifest-v1.json").arg(tag);
            for (const auto asset : release.value(QStringLiteral("assets")).toArray()) {
                const auto object = asset.toObject();
                if (object.value(QStringLiteral("name")).toString() == QStringLiteral("update-manifest-v1.json")
                    && object.value(QStringLiteral("browser_download_url")).toString() == expectedUrl
                    && (selectedVersion.isEmpty() || compareVersions(selectedVersion, version))) {
                    selectedVersion = version;
                    manifestUrl = QUrl(expectedUrl);
                }
            }
        }
        m_expectedReleaseVersion = selectedVersion;
        beginManifestRequest(selectedVersion.isEmpty() ? ManifestUrl : manifestUrl);
        return;
    }

    Candidate parsed;
    Failure failure = Failure::None;
    if (!parseManifest(m_bytesReceived, parsed, failure)) {
        failCheck(failure);
        return;
    }
    if (!m_expectedReleaseVersion.isEmpty() && parsed.version != m_expectedReleaseVersion) {
        failCheck(Failure::MalformedManifest);
        return;
    }
    m_expectedReleaseVersion.clear();
    m_bytesReceived.clear();
    m_candidate = parsed;
    const bool hasNewVersion = compareVersions(QString(KEEPASSXC_VERSION), parsed.version);
    const bool manuallyRequested = m_isManuallyRequested;
    // Complete bookkeeping before stateChanged: a receiver can immediately
    // start another check and replace the active request's context.
    config()->set(Config::GUI_CheckForUpdatesNextCheck, Clock::currentDateTime().addDays(7).toSecsSinceEpoch());
    QPointer<UpdateChecker> self(this);
    setState(hasNewVersion ? State::Available : State::NoUpdate);
    if (self) {
        emit updateCheckFinished(hasNewVersion, parsed.version, manuallyRequested);
    }
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
    request.setMaximumRedirectsAllowed(5);
    request.setTransferTimeout(30000);
    m_downloadRedirectRejected = false;
    m_downloadReply = networkManager()->get(request);
    QNetworkReply* const reply = m_downloadReply;
    setState(State::Downloading);
    connect(m_downloadReply, &QNetworkReply::redirected, this, [this, generation](const QUrl& target) {
        if (generation == m_generation && m_downloadReply && !redirectAllowed(target)) {
            m_downloadRedirectRejected = true;
            m_downloadReply->abort();
        }
    });
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
    connect(reply, &QObject::destroyed, this, [this, generation] {
        if (generation == m_generation && m_state == State::Downloading) {
            m_downloadReply = nullptr;
            failDownload(Failure::Offline);
        }
    });
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
    m_downloadRedirectRejected = false;
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
    const bool redirected = m_downloadRedirectRejected;
    m_downloadReply->deleteLater();
    m_downloadReply = nullptr;
    m_downloadRedirectRejected = false;
    if (!networkOk || m_downloadBytes != m_candidate.bytes) {
        failDownload(redirected ? Failure::RedirectRejected
                                : (networkOk ? Failure::ByteCountMismatch : Failure::Offline));
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
    m_downloadRedirectRejected = false;
    setState(State::Failed, failure);
}

void UpdateChecker::failCheck(Failure failure)
{
    m_bytesReceived.clear();
    m_expectedReleaseVersion.clear();
    m_fetchingPrereleaseIndex = false;
    m_redirectRejected = false;
    m_candidate = {};
    const bool manuallyRequested = m_isManuallyRequested;
    QPointer<UpdateChecker> self(this);
    setState(State::Failed, failure);
    if (self) {
        emit updateCheckFinished(false, ErrorVersion, manuallyRequested);
    }
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

void UpdateChecker::deferUpdate()
{
    if (m_state == State::ReadyToRestart) {
        setState(State::Deferred);
    }
}

bool UpdateChecker::canRestartThroughSquirrel() const
{
    if (m_state != State::ReadyToRestart && m_state != State::Deferred) {
        return false;
    }
    QString program;
    QStringList arguments;
    QString workingDirectory;
    return restartCommand(QCoreApplication::applicationDirPath(), program, arguments, workingDirectory);
}

bool UpdateChecker::launchUpdatedVersion()
{
    if (!canRestartThroughSquirrel()) {
        setState(State::Failed, Failure::RestartFailed);
        return false;
    }
    QString updater;
    QStringList arguments;
    QString workingDirectory;
    if (!restartCommand(QCoreApplication::applicationDirPath(), updater, arguments, workingDirectory)) {
        setState(State::Failed, Failure::RestartFailed);
        return false;
    }
    setState(State::Restarting);
    const bool started = launchRestartCommand(updater, arguments, workingDirectory);
    if (!started) {
        setState(State::Failed, Failure::RestartFailed);
    }
    return started;
}

bool UpdateChecker::restartCommand(const QString& applicationDirectory,
                                   QString& program,
                                   QStringList& arguments,
                                   QString& workingDirectory)
{
    program.clear();
    arguments.clear();
    workingDirectory.clear();

    const QFileInfo applicationDirectoryInfo(QDir::cleanPath(applicationDirectory));
    static const QRegularExpression appDirectoryPattern(
        QStringLiteral("^app-\\d+\\.\\d+\\.\\d+(?:[-+][0-9A-Za-z.-]+)?$"));
    if (!applicationDirectoryInfo.isDir()
        || !appDirectoryPattern.match(applicationDirectoryInfo.fileName()).hasMatch()) {
        return false;
    }

    const QDir packageRoot = applicationDirectoryInfo.dir();
    const QFileInfo updaterInfo(packageRoot.filePath(QStringLiteral("Update.exe")));
    if (!updaterInfo.isFile() || updaterInfo.isSymLink() || updaterInfo.canonicalFilePath().isEmpty()
        || QDir::cleanPath(updaterInfo.canonicalFilePath()) != QDir::cleanPath(updaterInfo.absoluteFilePath())) {
        return false;
    }

    program = updaterInfo.absoluteFilePath();
    arguments = {QStringLiteral("--processStart"), QStringLiteral("KeePassXC.exe")};
    workingDirectory = packageRoot.absolutePath();
    return true;
}

bool UpdateChecker::launchRestartCommand(const QString& program,
                                         const QStringList& arguments,
                                         const QString& workingDirectory)
{
    return m_restartLauncher ? m_restartLauncher(program, arguments, workingDirectory)
                             : QProcess::startDetached(program, arguments, workingDirectory);
}

void UpdateChecker::setRestartLauncherForTests(RestartLauncher launcher)
{
    m_restartLauncher = std::move(launcher);
}

void UpdateChecker::resetRestartLauncherForTests()
{
    m_restartLauncher = {};
}

void UpdateChecker::setNetworkAccessManagerForTests(QNetworkAccessManager* manager)
{
    Q_ASSERT(!m_reply && !m_downloadReply);
    m_networkManager = manager;
}

QNetworkAccessManager* UpdateChecker::networkManager() const
{
    return m_networkManager ? m_networkManager.data() : getNetMgr();
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
    case State::Idle: case State::NoUpdate: case State::Failed: return to == State::Checking;
    case State::Deferred: return to == State::Checking || to == State::Restarting;
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
    static const QRegularExpression sha256(QStringLiteral("^[0-9a-f]{64}$"));
    static const QRegularExpression sha1(QStringLiteral("^[0-9a-f]{40}$"));
    const QUrl notes(candidate.notesUrl), package(candidate.packageUrl);
    if (!packageVersionAllowed(candidate.version)) { failure = Failure::InvalidVersion; return false; }
    const QString expectedFile = QStringLiteral("KeePassXC.Material-%1-full.nupkg").arg(candidate.version);
    const QString releaseRoot = QStringLiteral("https://github.com/Ding-Ding-Projects/keepassxc/releases/");
    if (candidate.packageFile != expectedFile
        || candidate.notesUrl != releaseRoot + QStringLiteral("tag/v") + candidate.version
        || candidate.packageUrl != releaseRoot + QStringLiteral("download/v") + candidate.version + QLatin1Char('/') + expectedFile) {
        failure = Failure::MalformedManifest;
        return false;
    }
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

bool UpdateChecker::redirectAllowed(const QUrl& target)
{
    if (!target.isValid() || target.scheme() != QStringLiteral("https") || !target.userInfo().isEmpty()
        || (target.port() != -1 && target.port() != 443) || target.hasFragment()) {
        return false;
    }
    const QString host = target.host().toLower();
    return host == QStringLiteral("github.com") || host == QStringLiteral("objects.githubusercontent.com")
           || host == QStringLiteral("release-assets.githubusercontent.com");
}

bool UpdateChecker::isManuallyRequested() const
{
    return m_isManuallyRequested;
}

QString UpdateChecker::describeFailure(Failure failure)
{
    switch (failure) {
    case Failure::None:
        return {};
    case Failure::Offline:
        return tr("The update server could not be reached. Check the connection and try again.");
    case Failure::Timeout:
        return tr("The update server took too long to answer.");
    case Failure::RedirectRejected:
        return tr("The update server redirected somewhere other than GitHub, so the download was refused.");
    case Failure::OversizedManifest:
    case Failure::MalformedManifest:
    case Failure::InvalidVersion:
        return tr("The update description could not be read.");
    case Failure::PackageIdentityMismatch:
    case Failure::ArchitectureMismatch:
        return tr("The published update is for a different package or architecture.");
    case Failure::InsufficientStorage:
        return tr("There is not enough free space to stage the update.");
    case Failure::Cancelled:
        return tr("The download was cancelled.");
    case Failure::ByteCountMismatch:
    case Failure::Sha256Mismatch:
    case Failure::ReleasesMismatch:
    case Failure::UnsafePackage:
        return tr("The downloaded package did not match what the release describes, so it was discarded.");
    case Failure::UpdaterMissing:
        return tr("This copy was not installed through Setup.exe, so it cannot update itself. Download the "
                  "latest Setup.exe from the releases page.");
    case Failure::UpdaterStartFailed:
    case Failure::UpdaterApplyFailed:
        return tr("The installer could not apply the update.");
    case Failure::AppliedVersionMissing:
        return tr("The update was applied but the new version could not be verified.");
    case Failure::RestartRefused:
    case Failure::RestartFailed:
        return tr("The updated version could not be started. Restart KeePassXC to finish the update.");
    }
    return {};
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

    if (!lmatch.hasMatch() || !rmatch.hasMatch()) {
        return false;
    }

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
