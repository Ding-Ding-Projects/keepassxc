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
    candidate.bytes = object.value(QStringLiteral("bytes")).toVariant().toULongLong();
    static const QRegularExpression version(QStringLiteral("^\\d+\\.\\d+\\.\\d+$"));
    static const QRegularExpression sha256(QStringLiteral("^[0-9a-f]{64}$"));
    static const QRegularExpression sha1(QStringLiteral("^[0-9a-f]{40}$"));
    const QUrl notes(candidate.notesUrl), package(candidate.packageUrl);
    if (!version.match(candidate.version).hasMatch()) { failure = Failure::InvalidVersion; return false; }
    if (!notes.isValid() || notes.scheme() != QStringLiteral("https") || !package.isValid() || package.scheme() != QStringLiteral("https") || candidate.packageFile.isEmpty() || candidate.packageFile.contains(QLatin1Char('/')) || candidate.packageFile.contains(QLatin1Char('\\')) || candidate.packageFile.contains(QStringLiteral("..")) || candidate.bytes == 0 || candidate.bytes > 1610612736ULL || !sha256.match(candidate.sha256).hasMatch() || !sha1.match(candidate.releasesSha1).hasMatch()) { failure = Failure::MalformedManifest; return false; }
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
