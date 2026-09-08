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

#include "TestUpdateCheck.h"
#include "crypto/Crypto.h"
#include "core/Config.h"
#include "networking/UpdateChecker.h"

#include <QTest>
#include <QCryptographicHash>
#include <QCoreApplication>
#include <QEvent>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>

#include <algorithm>
#include <cstring>

#include <../minizip/zip.h>

namespace
{
    class ControlledReply final : public QNetworkReply
    {
    public:
        explicit ControlledReply(const QNetworkRequest& request, QObject* parent)
            : QNetworkReply(parent)
        {
            setRequest(request);
            setUrl(request.url());
            setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 200);
            open(QIODevice::ReadOnly | QIODevice::Unbuffered);
            connect(this, &QNetworkReply::redirectAllowed, this, [this] {
                ++m_redirectApprovalCount;
                m_redirectPending = false;
            });
        }

        void complete(const QByteArray& body = {}, int status = 200,
                      NetworkError error = NoError, bool signalReady = true)
        {
            if (m_finished || m_redirectPending) {
                return;
            }
            m_finished = true;
            setAttribute(QNetworkRequest::HttpStatusCodeAttribute, status);
            if (error != NoError) {
                setError(error, QStringLiteral("controlled network failure"));
            }
            m_body = body;
            if (signalReady) {
                emit readyRead();
            }
            emit finished();
        }

        void stream(const QByteArray& body)
        {
            m_body.append(body);
            emit readyRead();
        }

        void emitStaleSignals()
        {
            emit readyRead();
            emit redirected(QUrl(QStringLiteral("https://example.com/stale")));
            emit finished();
        }

        void redirectTo(const QUrl& target)
        {
            m_redirectPending = true;
            emit redirected(target);
        }

        int redirectApprovalCount() const
        {
            return m_redirectApprovalCount;
        }

        void abort() override
        {
            if (m_finished) {
                return;
            }
            m_finished = true;
            setError(QNetworkReply::OperationCanceledError, QStringLiteral("aborted by test reply"));
            emit finished();
        }

    protected:
        qint64 readData(char* data, qint64 maxSize) override
        {
            const qint64 remaining = m_body.size() - m_position;
            const qint64 count = qMin(maxSize, remaining);
            if (count <= 0) {
                return -1;
            }
            memcpy(data, m_body.constData() + m_position, size_t(count));
            m_position += count;
            return count;
        }

    private:
        QByteArray m_body;
        qint64 m_position = 0;
        bool m_finished = false;
        bool m_redirectPending = false;
        int m_redirectApprovalCount = 0;
    };

    class ControlledNetworkAccessManager final : public QNetworkAccessManager
    {
    public:
        QVector<QPointer<ControlledReply>> replies;

        int liveReplyCount() const
        {
            return std::count_if(replies.cbegin(), replies.cend(), [](const auto& reply) { return !reply.isNull(); });
        }

    protected:
        QNetworkReply* createRequest(Operation operation, const QNetworkRequest& request, QIODevice* outgoingData) override
        {
            Q_UNUSED(operation)
            Q_UNUSED(outgoingData)
            auto* reply = new ControlledReply(request, this);
            replies.append(reply);
            return reply;
        }
    };

    QByteArray availableManifest()
    {
        return R"({
            "schemaVersion": 1,
            "packageId": "KeePassXC.Material",
            "architecture": "x64",
            "version": "999.0.0",
            "notesUrl": "https://github.com/Ding-Ding-Projects/keepassxc/releases/tag/v999.0.0",
            "packageUrl": "https://github.com/Ding-Ding-Projects/keepassxc/releases/download/v999.0.0/KeePassXC.Material-999.0.0-full.nupkg",
            "packageFile": "KeePassXC.Material-999.0.0-full.nupkg",
            "bytes": 1,
            "sha256": "35b271ebbf16fad19c43afb0861408b0ef09b3cba281fa73c60629365aa843f7",
            "releasesSha1": "0123456789abcdef0123456789abcdef01234567",
            "executableSha256": "8a291e5160cc6e31c5a8aa49f20c8f214529be8790f204fcfcd84beea1c52a1a"
        })";
    }

    QByteArray manifestFor(const QString& version)
    {
        return availableManifest().replace("999.0.0", version.toUtf8());
    }

    QJsonObject releaseFor(const QString& tag, bool prerelease = true, bool draft = false)
    {
        return {{QStringLiteral("tag_name"), tag},
                {QStringLiteral("prerelease"), prerelease},
                {QStringLiteral("draft"), draft},
                {QStringLiteral("assets"), QJsonArray{QJsonObject{
                    {QStringLiteral("name"), QStringLiteral("update-manifest-v1.json")},
                    {QStringLiteral("browser_download_url"),
                     QStringLiteral("https://github.com/Ding-Ding-Projects/keepassxc/releases/download/%1/update-manifest-v1.json").arg(tag)}}}}};
    }

    QByteArray releaseIndex(const QJsonArray& releases)
    {
        return QJsonDocument(releases).toJson(QJsonDocument::Compact);
    }

    bool addZipEntry(zipFile archive, const QByteArray& name, const QByteArray& data)
    {
        if (zipOpenNewFileInZip64(archive, name.constData(), nullptr, nullptr, 0, nullptr, 0, nullptr,
                                  Z_DEFLATED, Z_BEST_COMPRESSION, 1) != ZIP_OK) {
            return false;
        }
        const bool written = zipWriteInFileInZip(archive, data.constData(), unsigned(data.size())) == ZIP_OK;
        return zipCloseFileInZip(archive) == ZIP_OK && written;
    }

    bool createPackage(const QString& path, bool traversal = false)
    {
        const QByteArray native = QFile::encodeName(path);
        zipFile archive = zipOpen64(native.constData(), APPEND_STATUS_CREATE);
        if (!archive) return false;
        const QByteArray nuspec = R"(<?xml version="1.0"?><package><metadata><id>KeePassXC.Material</id><version>2.8.1</version></metadata></package>)";
        const bool ok = addZipEntry(archive, "KeePassXC.Material.nuspec", nuspec)
                        && addZipEntry(archive, traversal ? "../escape.exe" : "lib/net45/KeePassXC.exe", "MZtest");
        return zipClose(archive, nullptr) == ZIP_OK && ok;
    }
}

QTEST_GUILESS_MAIN(TestUpdateCheck)

void TestUpdateCheck::initTestCase()
{
    QVERIFY(Crypto::init());
    QLocale::setDefault(QLocale::c());
    QVERIFY(m_configDirectory.isValid());
    QStandardPaths::setTestModeEnabled(true);
    Config::createConfigFromFile(m_configDirectory.filePath(QStringLiteral("test.ini")),
                                 m_configDirectory.filePath(QStringLiteral("local.ini")));
}

void TestUpdateCheck::init()
{
    config()->set(Config::GUI_CheckForUpdatesIncludeBetas, false);
    config()->set(Config::GUI_CheckForUpdatesNextCheck, 0);
}

void TestUpdateCheck::testCompareVersion()
{
    // No upgrade
    QCOMPARE(UpdateChecker::compareVersions(QString("2.3.0"), QString("2.3.0")), false);

    // First digit upgrade
    QCOMPARE(UpdateChecker::compareVersions(QString("2.4.0"), QString("3.0.0")), true);
    QCOMPARE(UpdateChecker::compareVersions(QString("3.0.0"), QString("2.4.0")), false);

    // Second digit upgrade
    QCOMPARE(UpdateChecker::compareVersions(QString("2.3.4"), QString("2.4.0")), true);
    QCOMPARE(UpdateChecker::compareVersions(QString("2.4.0"), QString("2.3.4")), false);

    // Third digit upgrade
    QCOMPARE(UpdateChecker::compareVersions(QString("2.3.0"), QString("2.3.1")), true);
    QCOMPARE(UpdateChecker::compareVersions(QString("2.3.1"), QString("2.3.0")), false);

    // Beta builds
    QCOMPARE(UpdateChecker::compareVersions(QString("2.3.0"), QString("2.3.0-beta1")), false);
    QCOMPARE(UpdateChecker::compareVersions(QString("2.3.0"), QString("2.3.1-beta1")), true);
    QCOMPARE(UpdateChecker::compareVersions(QString("2.3.0-beta1"), QString("2.3.0")), true);
    QCOMPARE(UpdateChecker::compareVersions(QString("2.3.0-beta"), QString("2.3.0-beta1")), true);
    QCOMPARE(UpdateChecker::compareVersions(QString("2.3.0-beta1"), QString("2.3.0-beta")), false);
    QCOMPARE(UpdateChecker::compareVersions(QString("2.3.0-beta1"), QString("2.3.0-beta2")), true);
    QCOMPARE(UpdateChecker::compareVersions(QString("2.3.0-beta2"), QString("2.3.0-beta1")), false);

    // Snapshot and invalid data
    QCOMPARE(UpdateChecker::compareVersions(QString("2.3.4-snapshot"), QString("2.4.0")), false);
    QCOMPARE(UpdateChecker::compareVersions(QString("2.4.0"), QString("invalid")), false);
    QCOMPARE(UpdateChecker::compareVersions(QString("2.4.0"), QString("")), false);
}

void TestUpdateCheck::testStateTransitions()
{
    using State = UpdateChecker::State;
    QVERIFY(UpdateChecker::transitionAllowed(State::Idle, State::Checking));
    QVERIFY(UpdateChecker::transitionAllowed(State::Checking, State::Available));
    QVERIFY(UpdateChecker::transitionAllowed(State::Available, State::Downloading));
    QVERIFY(UpdateChecker::transitionAllowed(State::Downloading, State::Verifying));
    QVERIFY(UpdateChecker::transitionAllowed(State::Verifying, State::Applying));
    QVERIFY(UpdateChecker::transitionAllowed(State::Applying, State::ReadyToRestart));
    QVERIFY(UpdateChecker::transitionAllowed(State::ReadyToRestart, State::Deferred));
    QVERIFY(UpdateChecker::transitionAllowed(State::ReadyToRestart, State::Restarting));
    QVERIFY(UpdateChecker::transitionAllowed(State::Deferred, State::Restarting));
    QVERIFY(UpdateChecker::transitionAllowed(State::Failed, State::Checking));
    QVERIFY(UpdateChecker::transitionAllowed(State::NoUpdate, State::Checking));
    QVERIFY(UpdateChecker::transitionAllowed(State::Deferred, State::Checking));
    QVERIFY(!UpdateChecker::transitionAllowed(State::Checking, State::Applying));
    QVERIFY(!UpdateChecker::transitionAllowed(State::Downloading, State::ReadyToRestart));
    QVERIFY(!UpdateChecker::transitionAllowed(State::ReadyToRestart, State::Downloading));
    QVERIFY(!UpdateChecker::transitionAllowed(State::Checking, State::Checking));
    QVERIFY(UpdateChecker::transitionAllowed(State::Available, State::Checking));
    QVERIFY(!UpdateChecker::transitionAllowed(State::Downloading, State::Checking));
    QVERIFY(!UpdateChecker::transitionAllowed(State::Verifying, State::Checking));
    QVERIFY(!UpdateChecker::transitionAllowed(State::Applying, State::Checking));
    QVERIFY(!UpdateChecker::transitionAllowed(State::ReadyToRestart, State::Checking));
    QVERIFY(!UpdateChecker::transitionAllowed(State::Restarting, State::Checking));
    QVERIFY(!UpdateChecker::transitionAllowed(State::Disabled, State::Checking));
    QVERIFY(!UpdateChecker::transitionAllowed(State::NotSquirrelInstalled, State::Checking));
}

void TestUpdateCheck::testManifestContract()
{
    const QByteArray valid = R"({
        "schemaVersion": 1,
        "packageId": "KeePassXC.Material",
        "architecture": "x64",
        "version": "2.8.1",
        "notesUrl": "https://github.com/Ding-Ding-Projects/keepassxc/releases/tag/v2.8.1",
        "packageUrl": "https://github.com/Ding-Ding-Projects/keepassxc/releases/download/v2.8.1/KeePassXC.Material-2.8.1-full.nupkg",
        "packageFile": "KeePassXC.Material-2.8.1-full.nupkg",
        "bytes": 72320747,
        "sha256": "35b271ebbf16fad19c43afb0861408b0ef09b3cba281fa73c60629365aa843f7",
        "releasesSha1": "0123456789abcdef0123456789abcdef01234567",
        "executableSha256": "8a291e5160cc6e31c5a8aa49f20c8f214529be8790f204fcfcd84beea1c52a1a"
    })";
    UpdateChecker::Candidate candidate;
    UpdateChecker::Failure failure = UpdateChecker::Failure::None;
    QVERIFY(UpdateChecker::parseManifest(valid, candidate, failure));
    QCOMPARE(candidate.version, QStringLiteral("2.8.1"));
    QCOMPARE(candidate.bytes, quint64(72320747));

    QByteArray wrongIdentity = valid;
    wrongIdentity.replace("KeePassXC.Material", "Other.Package");
    QVERIFY(!UpdateChecker::parseManifest(wrongIdentity, candidate, failure));
    QCOMPARE(failure, UpdateChecker::Failure::PackageIdentityMismatch);

    QByteArray traversal = valid;
    traversal.replace("KeePassXC.Material-2.8.1-full.nupkg", "../payload.nupkg");
    QVERIFY(!UpdateChecker::parseManifest(traversal, candidate, failure));
    QCOMPARE(failure, UpdateChecker::Failure::MalformedManifest);

    QByteArray oversized(64 * 1024 + 1, 'x');
    QVERIFY(!UpdateChecker::parseManifest(oversized, candidate, failure));
    QCOMPARE(failure, UpdateChecker::Failure::OversizedManifest);
}

void TestUpdateCheck::testPrereleaseManifestSelection_data()
{
    QTest::addColumn<QByteArray>("index");
    QTest::addColumn<QString>("selectedVersion");
    QTest::newRow("numeric-prerelease") << releaseIndex({releaseFor(QStringLiteral("v999.1.0"))}) << QStringLiteral("999.1.0");
    QTest::newRow("newer-stable-beats-older-beta")
        << releaseIndex({releaseFor(QStringLiteral("v999.1.0")), releaseFor(QStringLiteral("v999.2.0"), false)}) << QStringLiteral("999.2.0");
    QTest::newRow("stable-first-same-result")
        << releaseIndex({releaseFor(QStringLiteral("v999.2.0"), false), releaseFor(QStringLiteral("v999.1.0"))}) << QStringLiteral("999.2.0");
    QTest::newRow("numeric-ranking-not-lexical")
        << releaseIndex({releaseFor(QStringLiteral("v999.9.0")), releaseFor(QStringLiteral("v999.10.0"))}) << QStringLiteral("999.10.0");
    QTest::newRow("unsupported-suffix-does-not-mask-stable")
        << releaseIndex({releaseFor(QStringLiteral("v999.9.0-beta1")), releaseFor(QStringLiteral("v999.2.0"), false)}) << QStringLiteral("999.2.0");
    QTest::newRow("newer-beta-beats-stable")
        << releaseIndex({releaseFor(QStringLiteral("v999.1.0"), false), releaseFor(QStringLiteral("v999.2.0"))}) << QStringLiteral("999.2.0");
    QTest::newRow("empty-fallback") << releaseIndex({}) << QString();
    QTest::newRow("draft-excluded") << releaseIndex({releaseFor(QStringLiteral("v999.9.0"), true, true)}) << QString();
    QTest::newRow("suffix-excluded") << releaseIndex({releaseFor(QStringLiteral("v999.9.0-beta1"))}) << QString();
    QTest::newRow("oversized-component-excluded") << releaseIndex({releaseFor(QStringLiteral("v999.65536.0"))}) << QString();
    QTest::newRow("leading-zero-excluded") << releaseIndex({releaseFor(QStringLiteral("v999.01.0"))}) << QString();
    QTest::newRow("unprefixed-tag-excluded") << releaseIndex({releaseFor(QStringLiteral("999.1.0"))}) << QString();
    auto missingAsset = releaseFor(QStringLiteral("v999.1.0"));
    missingAsset.insert(QStringLiteral("assets"), QJsonArray{});
    QTest::newRow("missing-manifest-fallback") << releaseIndex({missingAsset}) << QString();
    for (const auto& pair : QVector<QPair<QByteArray, QString>>{
             {"foreign-repository", QStringLiteral("https://github.com/other/keepassxc/releases/download/v999.1.0/update-manifest-v1.json")},
             {"foreign-tag", QStringLiteral("https://github.com/Ding-Ding-Projects/keepassxc/releases/download/v999.2.0/update-manifest-v1.json")},
             {"foreign-file", QStringLiteral("https://github.com/Ding-Ding-Projects/keepassxc/releases/download/v999.1.0/other.json")},
             {"asset-query", QStringLiteral("https://github.com/Ding-Ding-Projects/keepassxc/releases/download/v999.1.0/update-manifest-v1.json?other=1")},
             {"asset-credentials", QStringLiteral("https://user@github.com/Ding-Ding-Projects/keepassxc/releases/download/v999.1.0/update-manifest-v1.json")}}) {
        auto invalid = releaseFor(QStringLiteral("v999.1.0"));
        auto asset = invalid.value(QStringLiteral("assets")).toArray().at(0).toObject();
        asset.insert(QStringLiteral("browser_download_url"), pair.second);
        invalid.insert(QStringLiteral("assets"), QJsonArray{asset});
        QTest::newRow(pair.first.constData()) << releaseIndex({invalid}) << QString();
    }
}

void TestUpdateCheck::testPrereleaseManifestSelection()
{
    QFETCH(QByteArray, index);
    QFETCH(QString, selectedVersion);
    ControlledNetworkAccessManager manager;
    UpdateChecker checker;
    checker.setNetworkAccessManagerForTests(&manager);
    QSignalSpy finished(&checker, &UpdateChecker::updateCheckFinished);
    config()->set(Config::GUI_CheckForUpdatesIncludeBetas, true);
    checker.checkForUpdates(true);
    QCOMPARE(manager.replies.size(), 1);
    QCOMPARE(manager.replies.at(0)->url(), QUrl(QStringLiteral("https://api.github.com/repos/Ding-Ding-Projects/keepassxc/releases?per_page=20")));
    const auto indexRequest = manager.replies.at(0)->request();
    QCOMPARE(indexRequest.rawHeader("Accept"), QByteArrayLiteral("application/vnd.github+json"));
    QCOMPARE(indexRequest.rawHeader("User-Agent"), QByteArrayLiteral("KeePassXC-Material-Updater/1"));
    QCOMPARE(indexRequest.attribute(QNetworkRequest::RedirectPolicyAttribute).toInt(), int(QNetworkRequest::UserVerifiedRedirectPolicy));
    QCOMPARE(indexRequest.maximumRedirectsAllowed(), 5);
    QCOMPARE(indexRequest.transferTimeout(), 30000);
    manager.replies.at(0)->complete(index);
    QCOMPARE(manager.replies.size(), 2);
    QCOMPARE(finished.size(), 0);
    QCOMPARE(checker.state(), UpdateChecker::State::Checking);
    const QString expectedUrl = selectedVersion.isEmpty()
        ? QStringLiteral("https://github.com/Ding-Ding-Projects/keepassxc/releases/latest/download/update-manifest-v1.json")
        : QStringLiteral("https://github.com/Ding-Ding-Projects/keepassxc/releases/download/v%1/update-manifest-v1.json").arg(selectedVersion);
    QCOMPARE(manager.replies.at(1)->url(), QUrl(expectedUrl));
    QCOMPARE(manager.replies.at(1)->request().rawHeader("Accept"), QByteArrayLiteral("application/json"));
    QCOMPARE(manager.replies.at(1)->request().rawHeader("User-Agent"), QByteArrayLiteral("KeePassXC-Material-Updater/1"));
    const QString resultVersion = selectedVersion.isEmpty() ? QStringLiteral("999.0.0") : selectedVersion;
    manager.replies.at(1)->complete(manifestFor(resultVersion));
    QCOMPARE(checker.state(), UpdateChecker::State::Available);
    QCOMPARE(checker.candidate().version, resultVersion);
    QCOMPARE(finished.size(), 1);
    QCOMPARE(finished.at(0).at(0).toBool(), true);
    QCOMPARE(finished.at(0).at(1).toString(), resultVersion);
    QVERIFY(config()->get(Config::GUI_CheckForUpdatesNextCheck).toULongLong() > 0);
}

void TestUpdateCheck::testStableManifestRoute()
{
    QVERIFY(!config()->getDefault(Config::GUI_CheckForUpdatesIncludeBetas).toBool());
    ControlledNetworkAccessManager manager;
    UpdateChecker checker;
    checker.setNetworkAccessManagerForTests(&manager);
    checker.checkForUpdates(false);
    QCOMPARE(manager.replies.size(), 1);
    auto* reply = manager.replies.at(0).data();
    QCOMPARE(reply->url(), QUrl(QStringLiteral("https://github.com/Ding-Ding-Projects/keepassxc/releases/latest/download/update-manifest-v1.json")));
    QCOMPARE(reply->request().rawHeader("Accept"), QByteArrayLiteral("application/json"));
    QSignalSpy approvals(reply, &QNetworkReply::redirectAllowed);
    reply->redirectTo(QUrl(QStringLiteral("https://github.com/Ding-Ding-Projects/keepassxc/releases/download/v999.0.0/update-manifest-v1.json")));
    reply->redirectTo(QUrl(QStringLiteral("https://release-assets.githubusercontent.com/asset?signed=1")));
    QCOMPARE(approvals.size(), 2);
    // Cover a reply whose final data arrives without an earlier readyRead.
    reply->complete(availableManifest(), 200, QNetworkReply::NoError, false);
    QCOMPARE(checker.state(), UpdateChecker::State::Available);
    checker.checkForUpdates(false);
    QCOMPARE(manager.replies.size(), 1);
}

void TestUpdateCheck::testReleaseResponseFailures_data()
{
    QTest::addColumn<int>("stage");
    QTest::addColumn<QByteArray>("body");
    QTest::addColumn<int>("status");
    QTest::addColumn<int>("networkError");
    QTest::addColumn<QString>("redirect");
    QTest::addColumn<bool>("streaming");
    QTest::addColumn<int>("expectedFailure");
    for (int stage = 0; stage < 3; ++stage) {
        const QByteArray prefix = QByteArray::number(stage) + '-';
        const QByteArray valid = stage == 1 ? releaseIndex({}) : availableManifest();
        const auto row = [&](const char* name, const QByteArray& body, int status, QNetworkReply::NetworkError error,
                             const QString& redirect, bool streaming, UpdateChecker::Failure failure) {
            QTest::newRow((prefix + name).constData()) << stage << body << status << int(error) << redirect << streaming << int(failure);
        };
        row("rate-limit-429", valid, 429, QNetworkReply::NoError, {}, false, UpdateChecker::Failure::Offline);
        row("rate-limit-403", valid, 403, QNetworkReply::ContentAccessDenied, {}, false, UpdateChecker::Failure::Offline);
        row("server-500", valid, 500, QNetworkReply::NoError, {}, false, UpdateChecker::Failure::Offline);
        row("missing-status", valid, 0, QNetworkReply::NoError, {}, false, UpdateChecker::Failure::Offline);
        row("offline", {}, 0, QNetworkReply::HostNotFoundError, {}, false, UpdateChecker::Failure::Offline);
        row("timeout", {}, 0, QNetworkReply::TimeoutError, {}, false, UpdateChecker::Failure::Timeout);
        row("too-many-redirects", {}, 302, QNetworkReply::TooManyRedirectsError, {}, false, UpdateChecker::Failure::Offline);
        row("malformed-json", "{", 200, QNetworkReply::NoError, {}, false, UpdateChecker::Failure::MalformedManifest);
        row("wrong-json-root", stage == 1 ? "{}" : "[]", 200, QNetworkReply::NoError, {}, false, UpdateChecker::Failure::MalformedManifest);
        const QByteArray huge((stage == 1 ? 256 : 64) * 1024 + 1, 'x');
        row("oversize-finished", huge, 200, QNetworkReply::NoError, {}, false, UpdateChecker::Failure::OversizedManifest);
        row("oversize-streaming", huge, 200, QNetworkReply::NoError, {}, true, UpdateChecker::Failure::OversizedManifest);
        row("external-redirect", {}, 200, QNetworkReply::NoError, QStringLiteral("https://example.com/asset"), false, UpdateChecker::Failure::RedirectRejected);
        row("foreign-repository-redirect", {}, 200, QNetworkReply::NoError,
            QStringLiteral("https://github.com/other/project/releases/download/v999.0.0/update-manifest-v1.json"), false, UpdateChecker::Failure::RedirectRejected);
        row("http-redirect", {}, 200, QNetworkReply::NoError,
            QStringLiteral("http://release-assets.githubusercontent.com/asset"), false, UpdateChecker::Failure::RedirectRejected);
        row("credential-redirect", {}, 200, QNetworkReply::NoError,
            QStringLiteral("https://user@release-assets.githubusercontent.com/asset"), false, UpdateChecker::Failure::RedirectRejected);
        row("port-redirect", {}, 200, QNetworkReply::NoError,
            QStringLiteral("https://release-assets.githubusercontent.com:8443/asset"), false, UpdateChecker::Failure::RedirectRejected);
        row("path-traversal-redirect", {}, 200, QNetworkReply::NoError,
            QStringLiteral("https://github.com/Ding-Ding-Projects/keepassxc/releases/download/v999.0.0/../../other/update-manifest-v1.json"),
            false, UpdateChecker::Failure::RedirectRejected);
        if (stage == 1) {
            row("index-storage-redirect", {}, 200, QNetworkReply::NoError,
                QStringLiteral("https://release-assets.githubusercontent.com/asset"), false, UpdateChecker::Failure::RedirectRejected);
            row("nonobject-release", "[1]", 200, QNetworkReply::NoError, {}, false, UpdateChecker::Failure::MalformedManifest);
            row("missing-release-fields", "[{}]", 200, QNetworkReply::NoError, {}, false, UpdateChecker::Failure::MalformedManifest);
            auto wrongType = releaseFor(QStringLiteral("v999.0.0"));
            wrongType.insert(QStringLiteral("draft"), QStringLiteral("false"));
            row("wrong-release-field-type", releaseIndex({wrongType}), 200, QNetworkReply::NoError, {}, false, UpdateChecker::Failure::MalformedManifest);
            QJsonArray tooMany;
            for (int i = 0; i < 21; ++i) {
                tooMany.append(releaseFor(QStringLiteral("v999.0.0")));
            }
            row("too-many-releases", releaseIndex(tooMany), 200, QNetworkReply::NoError, {}, false, UpdateChecker::Failure::MalformedManifest);
        }
        if (stage == 2) {
            row("different-tag-redirect", {}, 200, QNetworkReply::NoError,
                QStringLiteral("https://github.com/Ding-Ding-Projects/keepassxc/releases/download/v998.0.0/update-manifest-v1.json"),
                false, UpdateChecker::Failure::RedirectRejected);
        }
    }
}

void TestUpdateCheck::testReleaseResponseFailures()
{
    QFETCH(int, stage);
    QFETCH(QByteArray, body);
    QFETCH(int, status);
    QFETCH(int, networkError);
    QFETCH(QString, redirect);
    QFETCH(bool, streaming);
    QFETCH(int, expectedFailure);
    ControlledNetworkAccessManager manager;
    UpdateChecker checker;
    checker.setNetworkAccessManagerForTests(&manager);
    QSignalSpy finished(&checker, &UpdateChecker::updateCheckFinished);
    config()->set(Config::GUI_CheckForUpdatesIncludeBetas, stage != 0);
    checker.checkForUpdates(true);
    if (stage == 2) {
        manager.replies.at(0)->complete(releaseIndex({releaseFor(QStringLiteral("v999.0.0"))}));
    }
    const int expectedRequests = stage == 2 ? 2 : 1;
    QCOMPARE(manager.replies.size(), expectedRequests);
    auto* reply = manager.replies.last().data();
    QSignalSpy approvals(reply, &QNetworkReply::redirectAllowed);
    if (!redirect.isEmpty()) {
        reply->redirectTo(QUrl(redirect));
    } else if (streaming) {
        reply->stream(body);
    } else {
        reply->complete(body, status, QNetworkReply::NetworkError(networkError));
    }
    QCOMPARE(checker.state(), UpdateChecker::State::Failed);
    QCOMPARE(int(checker.failure()), expectedFailure);
    QCOMPARE(finished.size(), 1);
    QCOMPARE(finished.at(0).at(0).toBool(), false);
    QCOMPARE(finished.at(0).at(1).toString(), UpdateChecker::ErrorVersion);
    QCOMPARE(finished.at(0).at(2).toBool(), true);
    QVERIFY(checker.candidate().version.isEmpty());
    QCOMPARE(config()->get(Config::GUI_CheckForUpdatesNextCheck).toULongLong(), quint64(0));
    QCOMPARE(manager.replies.size(), expectedRequests);
    QCOMPARE(approvals.size(), 0);
    reply->emitStaleSignals();
    QCOMPARE(finished.size(), 1);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCOMPARE(manager.liveReplyCount(), 0);
}

void TestUpdateCheck::testSelectedManifestIdentity_data()
{
    QTest::addColumn<QByteArray>("manifest");
    QTest::addColumn<int>("expectedFailure");
    QTest::newRow("different-selected-version") << manifestFor(QStringLiteral("999.1.0")) << int(UpdateChecker::Failure::MalformedManifest);
    for (const auto& entry : QVector<QPair<QString, QString>>{
             {QStringLiteral("notesUrl"), QStringLiteral("https://github.com/other/project/releases/tag/v999.0.0")},
             {QStringLiteral("packageUrl"), QStringLiteral("https://github.com/other/project/releases/download/v999.0.0/KeePassXC.Material-999.0.0-full.nupkg")},
             {QStringLiteral("packageFile"), QStringLiteral("unrelated.nupkg")},
             {QStringLiteral("notesUrl"), QStringLiteral("https://github.com/Ding-Ding-Projects/keepassxc/releases/tag/v998.0.0")},
             {QStringLiteral("packageUrl"), QStringLiteral("https://github.com/Ding-Ding-Projects/keepassxc/releases/download/v998.0.0/KeePassXC.Material-999.0.0-full.nupkg")}}) {
        auto manifest = QJsonDocument::fromJson(availableManifest()).object();
        manifest.insert(entry.first, entry.second);
        const QByteArray name = (entry.first + QLatin1Char('-') + entry.second).toUtf8();
        QTest::newRow(name.constData()) << QJsonDocument(manifest).toJson() << int(UpdateChecker::Failure::MalformedManifest);
    }
    QTest::newRow("suffix-version") << manifestFor(QStringLiteral("999.0.0-beta1")) << int(UpdateChecker::Failure::InvalidVersion);
    QTest::newRow("overflow-version") << manifestFor(QStringLiteral("999.65536.0")) << int(UpdateChecker::Failure::InvalidVersion);
}

void TestUpdateCheck::testSelectedManifestIdentity()
{
    QFETCH(QByteArray, manifest);
    QFETCH(int, expectedFailure);
    for (bool includeBetas : {false, true}) {
        ControlledNetworkAccessManager manager;
        UpdateChecker checker;
        checker.setNetworkAccessManagerForTests(&manager);
        config()->set(Config::GUI_CheckForUpdatesIncludeBetas, includeBetas);
        checker.checkForUpdates(true);
        if (includeBetas) {
            manager.replies.at(0)->complete(releaseIndex({releaseFor(QStringLiteral("v999.0.0"))}));
        }
        manager.replies.last()->complete(manifest);
        // An otherwise valid newer version is allowed by the stable endpoint,
        // but cannot replace the exact version selected from the release index.
        const bool otherValidVersion = QJsonDocument::fromJson(manifest).object().value(QStringLiteral("version")).toString() == QStringLiteral("999.1.0");
        if (!includeBetas && otherValidVersion) {
            QCOMPARE(checker.state(), UpdateChecker::State::Available);
        } else {
            QCOMPARE(checker.state(), UpdateChecker::State::Failed);
            QCOMPARE(int(checker.failure()), expectedFailure);
            QVERIFY(checker.candidate().version.isEmpty());
        }
    }
}

void TestUpdateCheck::testIndexReplacementLifecycle()
{
    ControlledNetworkAccessManager manager;
    UpdateChecker checker;
    checker.setNetworkAccessManagerForTests(&manager);
    config()->set(Config::GUI_CheckForUpdatesIncludeBetas, true);
    QSignalSpy finished(&checker, &UpdateChecker::updateCheckFinished);
    checker.checkForUpdates(true);
    auto* index = manager.replies.at(0).data();
    index->complete(releaseIndex({releaseFor(QStringLiteral("v999.0.0"))}));
    QCOMPARE(manager.replies.size(), 2);
    index->emitStaleSignals();
    checker.checkForUpdates(true);
    QCOMPARE(manager.replies.size(), 2);
    QCOMPARE(checker.state(), UpdateChecker::State::Checking);
    QCOMPARE(finished.size(), 0);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QVERIFY(manager.replies.at(0).isNull());
    QCOMPARE(checker.state(), UpdateChecker::State::Checking);
    manager.replies.at(1)->complete(availableManifest());
    QCOMPARE(checker.state(), UpdateChecker::State::Available);
    QCOMPARE(finished.size(), 1);
}

void TestUpdateCheck::testReentrantStateChangeKeepsRequestContext()
{
    ControlledNetworkAccessManager manager;
    UpdateChecker checker;
    checker.setNetworkAccessManagerForTests(&manager);
    config()->set(Config::GUI_CheckForUpdatesIncludeBetas, true);
    QSignalSpy finished(&checker, &UpdateChecker::updateCheckFinished);
    connect(&checker, &UpdateChecker::stateChanged, &checker, [&](UpdateChecker::State state) {
        if (state == UpdateChecker::State::Available && manager.replies.size() == 2) {
            config()->set(Config::GUI_CheckForUpdatesNextCheck, 0);
            checker.checkForUpdates(false);
        }
    });
    checker.checkForUpdates(true);
    manager.replies.at(0)->complete(releaseIndex({releaseFor(QStringLiteral("v999.0.0"))}));
    manager.replies.at(1)->complete(availableManifest());
    QCOMPARE(manager.replies.size(), 3);
    QCOMPARE(checker.state(), UpdateChecker::State::Checking);
    QCOMPARE(finished.size(), 1);
    QCOMPARE(finished.at(0).at(2).toBool(), true);
    QCOMPARE(checker.isManuallyRequested(), false);
    QCOMPARE(config()->get(Config::GUI_CheckForUpdatesNextCheck).toULongLong(), quint64(0));
    manager.replies.at(0)->emitStaleSignals();
    manager.replies.at(1)->emitStaleSignals();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCOMPARE(checker.state(), UpdateChecker::State::Checking);
    manager.replies.at(2)->complete(releaseIndex({releaseFor(QStringLiteral("v999.1.0"))}));
    QCOMPARE(manager.replies.size(), 4);
    manager.replies.at(3)->complete(manifestFor(QStringLiteral("999.1.0")));
    QCOMPARE(finished.size(), 2);
    QCOMPARE(finished.at(1).at(1).toString(), QStringLiteral("999.1.0"));
    QCOMPARE(finished.at(1).at(2).toBool(), false);
    QCOMPARE(checker.candidate().version, QStringLiteral("999.1.0"));
}

void TestUpdateCheck::testReentrantFailureClearsSelection()
{
    ControlledNetworkAccessManager manager;
    UpdateChecker checker;
    checker.setNetworkAccessManagerForTests(&manager);
    config()->set(Config::GUI_CheckForUpdatesIncludeBetas, true);
    QSignalSpy finished(&checker, &UpdateChecker::updateCheckFinished);
    connect(&checker, &UpdateChecker::stateChanged, &checker, [&](UpdateChecker::State state) {
        if (state == UpdateChecker::State::Failed && manager.replies.size() == 2) {
            config()->set(Config::GUI_CheckForUpdatesIncludeBetas, false);
            checker.checkForUpdates(false);
        }
    });
    checker.checkForUpdates(true);
    manager.replies.at(0)->complete(releaseIndex({releaseFor(QStringLiteral("v999.9.0"))}));
    manager.replies.at(1)->complete("{malformed");
    QCOMPARE(manager.replies.size(), 3);
    QCOMPARE(checker.state(), UpdateChecker::State::Checking);
    QCOMPARE(finished.size(), 1);
    QCOMPARE(finished.at(0).at(2).toBool(), true);
    manager.replies.at(1)->emitStaleSignals();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    manager.replies.at(2)->complete(availableManifest());
    QCOMPARE(checker.state(), UpdateChecker::State::Available);
    QCOMPARE(checker.candidate().version, QStringLiteral("999.0.0"));
    QCOMPARE(finished.size(), 2);
    QCOMPARE(finished.at(1).at(2).toBool(), false);
}

void TestUpdateCheck::testDestroyedIndexAndSelectedManifest()
{
    for (bool selectedManifest : {false, true}) {
        for (bool destroyManager : {false, true}) {
            UpdateChecker checker;
            auto* manager = new ControlledNetworkAccessManager;
            checker.setNetworkAccessManagerForTests(manager);
            config()->set(Config::GUI_CheckForUpdatesIncludeBetas, true);
            QSignalSpy finished(&checker, &UpdateChecker::updateCheckFinished);
            checker.checkForUpdates(true);
            if (selectedManifest) {
                manager->replies.at(0)->complete(releaseIndex({releaseFor(QStringLiteral("v999.0.0"))}));
            }
            if (destroyManager) {
                delete manager;
            } else {
                delete manager->replies.last().data();
            }
            QCOMPARE(checker.state(), UpdateChecker::State::Failed);
            QCOMPARE(checker.failure(), UpdateChecker::Failure::Offline);
            QCOMPARE(finished.size(), 1);
            if (!destroyManager) {
                delete manager;
            }
            // The dead injected manager must not be dereferenced on retry.
            ControlledNetworkAccessManager replacement;
            checker.setNetworkAccessManagerForTests(&replacement);
            checker.checkForUpdates(true);
            replacement.replies.at(0)->complete(releaseIndex({}));
            replacement.replies.at(1)->complete(availableManifest());
            QCOMPARE(checker.state(), UpdateChecker::State::Available);
            QCOMPARE(finished.size(), 2);
        }
    }
}

void TestUpdateCheck::testCheckerDestructionAbortsIndex()
{
    ControlledNetworkAccessManager manager;
    auto* checker = new UpdateChecker;
    checker->setNetworkAccessManagerForTests(&manager);
    config()->set(Config::GUI_CheckForUpdatesIncludeBetas, true);
    checker->checkForUpdates(true);
    QCOMPARE(manager.replies.size(), 1);
    auto* reply = manager.replies.at(0).data();
    delete checker;
    QCOMPARE(reply->error(), QNetworkReply::OperationCanceledError);
    reply->emitStaleSignals();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCOMPARE(manager.liveReplyCount(), 0);
}

void TestUpdateCheck::testRedirectPolicy()
{
    // The release's "latest/download" link is a redirect to the asset host;
    // refusing every redirect would fail every check.
    QVERIFY(UpdateChecker::redirectAllowed(
        QUrl(QStringLiteral("https://github.com/Ding-Ding-Projects/keepassxc/releases/download/v2.8.1/update-manifest-v1.json"))));
    QVERIFY(UpdateChecker::redirectAllowed(
        QUrl(QStringLiteral("https://objects.githubusercontent.com/github-production-release-asset/abc?X-Amz=1"))));
    QVERIFY(UpdateChecker::redirectAllowed(QUrl(QStringLiteral("https://release-assets.githubusercontent.com/x"))));
    QVERIFY(!UpdateChecker::redirectAllowed(QUrl(QStringLiteral("http://github.com/insecure"))));
    QVERIFY(!UpdateChecker::redirectAllowed(QUrl(QStringLiteral("https://github.com.evil.example/x"))));
    QVERIFY(!UpdateChecker::redirectAllowed(QUrl(QStringLiteral("https://example.com/x"))));
    QVERIFY(!UpdateChecker::redirectAllowed(QUrl(QStringLiteral("https://untrusted.githubusercontent.com/x"))));
    QVERIFY(!UpdateChecker::redirectAllowed(QUrl(QStringLiteral("https://user@github.com/x"))));
    QVERIFY(!UpdateChecker::redirectAllowed(QUrl(QStringLiteral("https://github.com:8443/x"))));
    QVERIFY(!UpdateChecker::redirectAllowed(QUrl(QStringLiteral("https://github.com/x#fragment"))));
    QVERIFY(!UpdateChecker::redirectAllowed(QUrl()));
    QVERIFY(UpdateChecker::describeFailure(UpdateChecker::Failure::None).isEmpty());
    QVERIFY(!UpdateChecker::describeFailure(UpdateChecker::Failure::UpdaterMissing).isEmpty());
}

void TestUpdateCheck::testPackageContract()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString validPath = directory.filePath(QStringLiteral("valid.nupkg"));
    QVERIFY(createPackage(validPath));
    QFile valid(validPath);
    QVERIFY(valid.open(QIODevice::ReadOnly));
    const QByteArray bytes = valid.readAll();
    UpdateChecker::Candidate candidate;
    candidate.version = QStringLiteral("2.8.1");
    candidate.bytes = quint64(bytes.size());
    candidate.releasesSha1 = QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha1).toHex());
    candidate.executableSha256 = QString::fromLatin1(
        QCryptographicHash::hash(QByteArrayLiteral("MZtest"), QCryptographicHash::Sha256).toHex());
    UpdateChecker::Failure failure = UpdateChecker::Failure::None;
    QVERIFY(UpdateChecker::verifyPackage(validPath, candidate, failure));

    candidate.releasesSha1.fill(QLatin1Char('0'));
    QVERIFY(!UpdateChecker::verifyPackage(validPath, candidate, failure));
    QCOMPARE(failure, UpdateChecker::Failure::ReleasesMismatch);

    const QString unsafePath = directory.filePath(QStringLiteral("unsafe.nupkg"));
    QVERIFY(createPackage(unsafePath, true));
    QFile unsafe(unsafePath);
    QVERIFY(unsafe.open(QIODevice::ReadOnly));
    const QByteArray unsafeBytes = unsafe.readAll();
    candidate.bytes = quint64(unsafeBytes.size());
    candidate.releasesSha1 = QString::fromLatin1(QCryptographicHash::hash(unsafeBytes, QCryptographicHash::Sha1).toHex());
    QVERIFY(!UpdateChecker::verifyPackage(unsafePath, candidate, failure));
    QCOMPARE(failure, UpdateChecker::Failure::UnsafePackage);
}

void TestUpdateCheck::testRestartCommandContract()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString root = directory.filePath(QStringLiteral("KeePassXC.Material"));
    const QString appDirectory = QDir(root).filePath(QStringLiteral("app-2.8.1"));
    QVERIFY(QDir().mkpath(appDirectory));
    QFile updater(QDir(root).filePath(QStringLiteral("Update.exe")));
    QVERIFY(updater.open(QIODevice::WriteOnly));
    QVERIFY(updater.write("MZtest") > 0);
    updater.close();

    QString program;
    QStringList arguments;
    QString workingDirectory;
    QVERIFY(UpdateChecker::restartCommand(appDirectory, program, arguments, workingDirectory));
    QCOMPARE(QDir::cleanPath(program), QDir::cleanPath(updater.fileName()));
    QCOMPARE(arguments, QStringList({QStringLiteral("--processStart"), QStringLiteral("KeePassXC.exe")}));
    QCOMPARE(QDir::cleanPath(workingDirectory), QDir::cleanPath(root));

    QString launchedProgram;
    QStringList launchedArguments;
    QString launchedWorkingDirectory;
    UpdateChecker::setRestartLauncherForTests(
        [&](const QString& executable, const QStringList& args, const QString& cwd) {
            launchedProgram = executable;
            launchedArguments = args;
            launchedWorkingDirectory = cwd;
            return true;
        });
    QVERIFY(UpdateChecker::launchRestartCommand(program, arguments, workingDirectory));
    UpdateChecker::resetRestartLauncherForTests();
    QCOMPARE(launchedProgram, program);
    QCOMPARE(launchedArguments, arguments);
    QCOMPARE(launchedWorkingDirectory, workingDirectory);

    QVERIFY(!UpdateChecker::restartCommand(QDir(root).filePath(QStringLiteral("portable")),
                                           program,
                                           arguments,
                                           workingDirectory));
    QVERIFY(QFile::remove(updater.fileName()));
    QVERIFY(!UpdateChecker::restartCommand(appDirectory, program, arguments, workingDirectory));
}

void TestUpdateCheck::testConcurrentCheckKeepsDownloadActive()
{
    ControlledNetworkAccessManager manager;
    UpdateChecker checker;
    checker.setNetworkAccessManagerForTests(&manager);

    checker.checkForUpdates(true);
    QCOMPARE(manager.replies.size(), 1);
    manager.replies.at(0)->complete(availableManifest());
    QCOMPARE(checker.state(), UpdateChecker::State::Available);

    checker.downloadAvailableUpdate();
    QCOMPARE(manager.replies.size(), 2);
    QCOMPARE(checker.state(), UpdateChecker::State::Downloading);

    checker.checkForUpdates(true);
    QCOMPARE(manager.replies.size(), 2);
    QCOMPARE(checker.state(), UpdateChecker::State::Downloading);

    checker.cancelDownload();
    QCOMPARE(checker.state(), UpdateChecker::State::Failed);
    QCOMPARE(checker.failure(), UpdateChecker::Failure::Cancelled);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCOMPARE(manager.liveReplyCount(), 0);
}

void TestUpdateCheck::testPackageRedirectRequiresExplicitApproval()
{
    ControlledNetworkAccessManager manager;
    UpdateChecker checker;
    checker.setNetworkAccessManagerForTests(&manager);

    checker.checkForUpdates(true);
    QCOMPARE(manager.replies.size(), 1);
    manager.replies.at(0)->complete(availableManifest());
    QCOMPARE(checker.state(), UpdateChecker::State::Available);

    checker.downloadAvailableUpdate();
    QCOMPARE(manager.replies.size(), 2);
    const auto redirectPolicy = manager.replies.at(1)->request().attribute(QNetworkRequest::RedirectPolicyAttribute);
    QCOMPARE(redirectPolicy.toInt(), int(QNetworkRequest::UserVerifiedRedirectPolicy));

    manager.replies.at(1)->redirectTo(QUrl(QStringLiteral("https://release-assets.githubusercontent.com/package")));
    QCOMPARE(manager.replies.at(1)->redirectApprovalCount(), 1);
    QCOMPARE(checker.state(), UpdateChecker::State::Downloading);

    checker.cancelDownload();
    QCOMPARE(checker.state(), UpdateChecker::State::Failed);
    QCOMPARE(checker.failure(), UpdateChecker::Failure::Cancelled);
}

void TestUpdateCheck::testRejectedPackageRedirectReportsDiagnostic()
{
    ControlledNetworkAccessManager manager;
    UpdateChecker checker;
    checker.setNetworkAccessManagerForTests(&manager);

    checker.checkForUpdates(true);
    QCOMPARE(manager.replies.size(), 1);
    manager.replies.at(0)->complete(availableManifest());
    QCOMPARE(checker.state(), UpdateChecker::State::Available);

    checker.downloadAvailableUpdate();
    QCOMPARE(manager.replies.size(), 2);
    manager.replies.at(1)->redirectTo(QUrl(QStringLiteral("https://example.com/unsafe.nupkg")));

    QCOMPARE(manager.replies.at(1)->redirectApprovalCount(), 0);
    QCOMPARE(checker.state(), UpdateChecker::State::Failed);
    QCOMPARE(checker.failure(), UpdateChecker::Failure::RedirectRejected);
    QVERIFY(!UpdateChecker::describeFailure(checker.failure()).isEmpty());
}

void TestUpdateCheck::testDestroyedNetworkManagerClearsActiveReplies()
{
    UpdateChecker checker;
    auto* manager = new ControlledNetworkAccessManager;
    checker.setNetworkAccessManagerForTests(manager);

    checker.checkForUpdates(true);
    QCOMPARE(manager->replies.size(), 1);
    delete manager;

    QCOMPARE(checker.state(), UpdateChecker::State::Failed);
    QCOMPARE(checker.failure(), UpdateChecker::Failure::Offline);

    auto* retryManager = new ControlledNetworkAccessManager;
    checker.setNetworkAccessManagerForTests(retryManager);
    checker.checkForUpdates(true);
    QCOMPARE(retryManager->replies.size(), 1);
    delete retryManager;

    auto* downloadManager = new ControlledNetworkAccessManager;
    UpdateChecker downloading;
    downloading.setNetworkAccessManagerForTests(downloadManager);
    downloading.checkForUpdates(true);
    QCOMPARE(downloadManager->replies.size(), 1);
    downloadManager->replies.at(0)->complete(availableManifest());
    downloading.downloadAvailableUpdate();
    QCOMPARE(downloadManager->replies.size(), 2);
    delete downloadManager;

    QCOMPARE(downloading.state(), UpdateChecker::State::Failed);
    QCOMPARE(downloading.failure(), UpdateChecker::Failure::Offline);
}

void TestUpdateCheck::testDeferredManifestDeletionDoesNotFailReplacementCheck()
{
    ControlledNetworkAccessManager manager;
    UpdateChecker checker;
    checker.setNetworkAccessManagerForTests(&manager);
    connect(&checker, &UpdateChecker::updateCheckFinished, &checker, [&checker, &manager] {
        if (manager.replies.size() == 1) {
            checker.checkForUpdates(true);
        }
    });

    checker.checkForUpdates(true);
    QCOMPARE(manager.replies.size(), 1);
    manager.replies.at(0)->complete(availableManifest());
    QCOMPARE(manager.replies.size(), 2);
    QCOMPARE(checker.state(), UpdateChecker::State::Checking);

    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCOMPARE(manager.replies.size(), 2);
    QCOMPARE(checker.state(), UpdateChecker::State::Checking);

    manager.replies.at(1)->complete(availableManifest());
    QCOMPARE(checker.state(), UpdateChecker::State::Available);
}
