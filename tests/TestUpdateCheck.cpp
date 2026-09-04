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
#include "networking/UpdateChecker.h"

#include <QTest>
#include <QCryptographicHash>
#include <QFile>
#include <QTemporaryDir>

#include <../minizip/zip.h>

namespace
{
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
    QVERIFY(!UpdateChecker::transitionAllowed(State::Checking, State::Applying));
    QVERIFY(!UpdateChecker::transitionAllowed(State::Downloading, State::ReadyToRestart));
    QVERIFY(!UpdateChecker::transitionAllowed(State::ReadyToRestart, State::Downloading));
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
