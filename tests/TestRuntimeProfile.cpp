#include "core/RuntimeProfile.h"
#include "config-keepassx.h"
#include "core/Database.h"
#include "core/Config.h"
#include "crypto/Crypto.h"
#include "keys/CompositeKey.h"
#include "keys/FileKey.h"
#include "keys/PasswordKey.h"
#include "format/OpVaultReader.h"
#include "gui/FileDialog.h"
#include "gui/Icons.h"
#include "gui/material/MaterialHistoryStore.h"
#ifdef KPXC_FEATURE_UPDATES
#include "networking/UpdateChecker.h"
#endif

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QSettings>
#ifdef KPXC_FEATURE_BROWSER
#include "browser/BrowserShared.h"
#include <QLocalServer>
#include <QLocalSocket>
#endif
#include <QTemporaryDir>
#include <QTest>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

class TestRuntimeProfile : public QObject
{
    Q_OBJECT
private slots:
    void normalIdentity();
    void invalidDirectories();
    void isolatedRoots();
    void linkedSettingsFiles();
#ifdef KPXC_FEATURE_BROWSER
    void proxyInvalidProfile();
#endif
};

static int probe(const QStringList& arguments)
{
    QString error;
    if (!RuntimeProfile::initialize({arguments.first(), QStringLiteral("--verification-profile"), arguments.at(2)}, &error)) {
        return 10;
    }
    if (!RuntimeProfile::isActive() || !error.isEmpty()) return 11;
    for (const auto type : {QStandardPaths::AppDataLocation, QStandardPaths::AppLocalDataLocation,
                           QStandardPaths::ConfigLocation, QStandardPaths::CacheLocation,
                           QStandardPaths::TempLocation, QStandardPaths::RuntimeLocation}) {
        if (!RuntimeProfile::contains(RuntimeProfile::location(type))) return 12;
    }
    if (RuntimeProfile::location(QStandardPaths::ConfigLocation) == RuntimeProfile::location(QStandardPaths::CacheLocation)) return 13;
    if (RuntimeProfile::ipcIdentifier(QStringLiteral("keepassxc")) == QStringLiteral("keepassxc")) return 14;
    const QString externalPath = QDir(RuntimeProfile::root()).absoluteFilePath(QStringLiteral("../external.kdbx"));
    if (RuntimeProfile::contains(externalPath)) return 15;
    if (!RuntimeProfile::contains(RuntimeProfile::location(QStandardPaths::TempLocation) + QStringLiteral("/../inside.kdbx"))) return 27;
    if (RuntimeProfile::contains(QStringLiteral("relative.kdbx"))) return 16;
    if (RuntimeProfile::contains(RuntimeProfile::root() + QStringLiteral("/../external.kdbx"))) return 17;
    if (RuntimeProfile::initialize({arguments.first()}, &error)) return 18;
    if (!Crypto::init()) return 28;

    // Requesting an external synthetic INI must still use the profile INI.
    const QString externalConfig = QDir(RuntimeProfile::root()).absoluteFilePath(QStringLiteral("../external.ini"));
    const QByteArray sentinel("[General]\nGUI/FontFamily=ExternalSentinel\n");
    QFile outside(externalConfig);
    if (!outside.open(QIODevice::WriteOnly) || outside.write(sentinel) != sentinel.size()) return 29;
    outside.close();
    Config::createConfigFromFile(externalConfig);
    if (!RuntimeProfile::contains(config()->getFileName())
        || config()->get(Config::GUI_FontFamily).toString() == QStringLiteral("ExternalSentinel")) return 30;
    config()->set(Config::GUI_FontFamily, QStringLiteral("ProfileOnlyFont"));
    config()->sync();
    QSettings profileSettings(QDir(RuntimeProfile::location(QStandardPaths::CacheLocation)).filePath(QStringLiteral("keepassxc.ini")), QSettings::IniFormat);
    if (profileSettings.value(QStringLiteral("GUI/FontFamily")).toString() != QStringLiteral("ProfileOnlyFont")) return 31;
    if (!outside.open(QIODevice::ReadOnly) || outside.readAll() != sentinel) return 32;
    outside.close();

    Database database;
    if (database.open(QStringLiteral("/external-personal.kdbx"), {}, &error)
        || !error.contains(QStringLiteral("verification profile"))) return 25;
    FileKey key;
    if (key.load(QStringLiteral("/external-personal.keyx"), &error)
        || !error.contains(QStringLiteral("verification profile"))) return 26;
    fileDialog()->setNextFileName(externalPath);
    if (!fileDialog()->getSaveFileName().isEmpty()) return 33;
    fileDialog()->setNextDirectory(QFileInfo(externalPath).absolutePath());
    if (!fileDialog()->getExistingDirectory().isEmpty()) return 34;
    const QString internalPath = QDir(RuntimeProfile::root()).filePath(QStringLiteral("synthetic.kdbx"));
    if (QFileInfo::exists(internalPath) && !QFile::remove(internalPath)) return 47;
    fileDialog()->setNextFileName(internalPath);
    if (fileDialog()->getSaveFileName() != internalPath) return 35;
    fileDialog()->setNextDirectory(RuntimeProfile::root());
    if (fileDialog()->getExistingDirectory() != RuntimeProfile::root()) return 36;
    if (database.saveAs(externalPath, Database::Atomic, {}, &error)
        || !error.contains(QStringLiteral("verification profile")) || QFileInfo::exists(externalPath)) return 37;
    if (database.saveAs(internalPath, Database::Atomic, externalPath, &error)
        || !error.contains(QStringLiteral("verification profile")) || QFileInfo::exists(internalPath)) return 38;
    auto compositeKey = QSharedPointer<CompositeKey>::create();
    compositeKey->addKey(QSharedPointer<PasswordKey>::create(QStringLiteral("synthetic-verification-only")));
    if (!database.setKey(compositeKey) || !database.saveAs(internalPath, Database::Atomic, {}, &error)) return 39;
    if (!QFileInfo::exists(internalPath)) return 40;
    OpVaultReader reader;
    QDir outsideVault(QFileInfo(externalPath).absolutePath());
    if (reader.convert(outsideVault, QString()).data() || !reader.errorString().contains(QStringLiteral("verification profile"))) return 41;
    Material::HistoryStore history(QStringLiteral("/external-history"));
    if (!RuntimeProfile::contains(history.logPath())) return 19;
    icons()->setApplicationLogoCacheDirectoryForTests(QStringLiteral("/external-logos"));
    if (!RuntimeProfile::contains(icons()->applicationLogoPath())) return 20;
#ifdef KPXC_FEATURE_UPDATES
    UpdateChecker checker;
    checker.checkForUpdates(true);
    if (checker.state() != UpdateChecker::State::Disabled) return 21;
    checker.downloadAvailableUpdate();
    checker.applyVerifiedUpdate(QStringLiteral("/external.nupkg"));
    if (checker.canRestartThroughSquirrel() || checker.launchUpdatedVersion()) return 22;
    bool launched = false;
    UpdateChecker::setRestartLauncherForTests([&launched](const QString&, const QStringList&, const QString&) {
        launched = true;
        return true;
    });
    if (UpdateChecker::launchRestartCommand(QStringLiteral("Update.exe"), {}, {}) || launched) return 23;
    UpdateChecker::resetRestartLauncherForTests();
#endif
#ifdef KPXC_FEATURE_BROWSER
    QLocalServer server;
    server.setSocketOptions(QLocalServer::UserAccessOption);
    if (!server.listen(BrowserShared::localServerPath())) return 42;
    QProcess proxy;
    proxy.start(QString::fromLocal8Bit(KEEPASSXC_PROXY_TEST_PATH),
                {QStringLiteral("--verification-profile"), RuntimeProfile::root()});
    if (!proxy.waitForStarted(10000) || !server.waitForNewConnection(10000)) {
        proxy.kill();
        proxy.waitForFinished(5000);
        return 43;
    }
    auto* connection = server.nextPendingConnection();
    if (!connection) return 44;
    connection->close();
    delete connection;
    proxy.closeWriteChannel();
    if (!proxy.waitForFinished(10000)) {
        proxy.kill();
        proxy.waitForFinished(5000);
        return 45;
    }
    if (proxy.exitStatus() != QProcess::NormalExit || proxy.exitCode() != 0 || !proxy.readAllStandardOutput().isEmpty()) return 46;
#endif
    QFile output(arguments.at(3));
    if (!output.open(QIODevice::WriteOnly)) return 24;
    output.write(RuntimeProfile::ipcIdentifier(QStringLiteral("keepassxc")).toUtf8());
    return 0;
}

void TestRuntimeProfile::normalIdentity()
{
    QString error;
    QVERIFY(RuntimeProfile::initialize({QStringLiteral("test")}, &error));
    QVERIFY(!RuntimeProfile::isActive());
    QCOMPARE(RuntimeProfile::ipcIdentifier(QStringLiteral("keepassxc")), QStringLiteral("keepassxc"));
    QCOMPARE(RuntimeProfile::location(QStandardPaths::AppDataLocation), QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
}

void TestRuntimeProfile::invalidDirectories()
{
    QString error;
    for (const auto& path : {QString(), QStringLiteral("relative"), QDir::rootPath()}) {
        QVERIFY(!RuntimeProfile::initialize({QStringLiteral("test"), QStringLiteral("--verification-profile"), path}, &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!RuntimeProfile::isActive());
    }
    QVERIFY(!RuntimeProfile::initialize({QStringLiteral("test"), QStringLiteral("--verification-profile")}, &error));
    QVERIFY(!RuntimeProfile::initialize({QStringLiteral("test"), QStringLiteral("--verification-profile"), QStringLiteral("/one"),
                                        QStringLiteral("--verification-profile"), QStringLiteral("/two")}, &error));
    QTemporaryDir personal;
    QFile existing(personal.filePath(QStringLiteral("personal.ini")));
    QVERIFY(existing.open(QIODevice::WriteOnly));
    existing.write("do not load or replace");
    existing.close();
    QVERIFY(!RuntimeProfile::initialize({QStringLiteral("test"), QStringLiteral("--verification-profile"), personal.path()}, &error));
    QVERIFY(existing.open(QIODevice::ReadOnly));
    QCOMPARE(existing.readAll(), QByteArray("do not load or replace"));
    QVERIFY(!RuntimeProfile::isActive());
    existing.close();
    QFile marker(personal.filePath(QStringLiteral(".keepassxc-verification-profile")));
    QVERIFY(marker.open(QIODevice::WriteOnly));
    marker.write("invalid marker");
    marker.close();
    QVERIFY(!RuntimeProfile::initialize({QStringLiteral("test"), QStringLiteral("--verification-profile"), personal.path()}, &error));
    QVERIFY(!RuntimeProfile::isActive());
}

void TestRuntimeProfile::isolatedRoots()
{
    QTemporaryDir temporary;
    QByteArray previous;
    for (int i = 0; i < 2; ++i) {
        const QString root = temporary.filePath(QStringLiteral("profile-%1").arg(i));
        const QString receipt = temporary.filePath(QStringLiteral("receipt-%1").arg(i));
        QProcess child;
        child.start(QCoreApplication::applicationFilePath(), {QStringLiteral("--probe-profile"), root, receipt});
        QVERIFY(child.waitForFinished(30000));
        QCOMPARE(child.exitStatus(), QProcess::NormalExit);
        QCOMPARE(child.exitCode(), 0);
        QFile output(receipt);
        QVERIFY(output.open(QIODevice::ReadOnly));
        const QByteArray identifier = output.readAll();
        QVERIFY(!identifier.isEmpty());
        QVERIFY(identifier != previous);
        previous = identifier;
        QVERIFY(QFileInfo::exists(QDir(root).filePath(QStringLiteral(".keepassxc-verification-profile"))));
        output.close();
        child.start(QCoreApplication::applicationFilePath(), {QStringLiteral("--probe-profile"), root, receipt});
        QVERIFY(child.waitForFinished(30000));
        QCOMPARE(child.exitCode(), 0);
        QVERIFY(output.open(QIODevice::ReadOnly));
        QCOMPARE(output.readAll(), identifier);
    }
}

void TestRuntimeProfile::linkedSettingsFiles()
{
    QTemporaryDir temporary;
    const QString sentinelPath = temporary.filePath(QStringLiteral("outside.ini"));
    const QByteArray sentinelBytes("[General]\nUntouched=synthetic-outside-sentinel\n");
    QFile sentinel(sentinelPath);
    QVERIFY(sentinel.open(QIODevice::WriteOnly));
    QCOMPARE(sentinel.write(sentinelBytes), qint64(sentinelBytes.size()));
    sentinel.close();

    for (const auto& relative : {QStringLiteral("config/keepassxc.ini"), QStringLiteral("cache/keepassxc.ini")}) {
        const QString root = temporary.filePath(relative.startsWith(QStringLiteral("config"))
                                                   ? QStringLiteral("config-profile") : QStringLiteral("cache-profile"));
        const QString settingsPath = QDir(root).filePath(relative);
        QVERIFY(QDir().mkpath(QFileInfo(settingsPath).absolutePath()));
        QFile marker(QDir(root).filePath(QStringLiteral(".keepassxc-verification-profile")));
        const QByteArray markerBytes("KeePassXC synthetic verification profile\n");
        QVERIFY(marker.open(QIODevice::WriteOnly));
        QCOMPARE(marker.write(markerBytes), qint64(markerBytes.size()));
        marker.close();
#ifdef Q_OS_WIN
        const bool linked = CreateSymbolicLinkW(reinterpret_cast<const wchar_t*>(settingsPath.utf16()),
                                               reinterpret_cast<const wchar_t*>(sentinelPath.utf16()),
                                               SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE);
        if (!linked) QSKIP("Native symbolic-file-link creation is unavailable on this Windows host.");
#else
        QVERIFY(QFile::link(sentinelPath, settingsPath));
#endif
        QString error;
        QVERIFY(!RuntimeProfile::initialize({QStringLiteral("test"), QStringLiteral("--verification-profile"), root}, &error));
        QVERIFY(error.contains(QStringLiteral("settings files")));
        QVERIFY(!RuntimeProfile::isActive());
        QVERIFY(sentinel.open(QIODevice::ReadOnly));
        QCOMPARE(sentinel.readAll(), sentinelBytes);
        sentinel.close();
        QVERIFY(marker.open(QIODevice::ReadOnly));
        QCOMPARE(marker.readAll(), markerBytes);
        marker.close();
        QVERIFY(QFile::remove(settingsPath));
        // An existing directory at the exact INI path must also be rejected.
        QVERIFY(QDir().mkpath(settingsPath));
        QVERIFY(!RuntimeProfile::initialize({QStringLiteral("test"), QStringLiteral("--verification-profile"), root}, &error));
        QVERIFY(error.contains(QStringLiteral("settings files")));
        QVERIFY(!RuntimeProfile::isActive());
    }
}

#ifdef KPXC_FEATURE_BROWSER
void TestRuntimeProfile::proxyInvalidProfile()
{
    QProcess proxy;
    proxy.start(QString::fromLocal8Bit(KEEPASSXC_PROXY_TEST_PATH),
                {QStringLiteral("--verification-profile"), QStringLiteral("relative-invalid-profile")});
    QVERIFY(proxy.waitForFinished(10000));
    QCOMPARE(proxy.exitStatus(), QProcess::NormalExit);
    QVERIFY(proxy.exitCode() != 0);
    QVERIFY(proxy.readAllStandardOutput().isEmpty());
    QVERIFY(!proxy.readAllStandardError().isEmpty());
}
#endif

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);
    const auto arguments = application.arguments();
    if (arguments.size() == 4 && arguments.at(1) == QStringLiteral("--probe-profile")) return probe(arguments);
    TestRuntimeProfile test;
    return QTest::qExec(&test, argc, argv);
}

#include "TestRuntimeProfile.moc"
