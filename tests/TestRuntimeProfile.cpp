#include "core/RuntimeProfile.h"
#include "config-keepassx.h"
#include "core/Database.h"
#include "keys/FileKey.h"
#include "gui/Icons.h"
#include "gui/material/MaterialHistoryStore.h"
#ifdef KPXC_FEATURE_UPDATES
#include "networking/UpdateChecker.h"
#endif

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QTest>

class TestRuntimeProfile : public QObject
{
    Q_OBJECT
private slots:
    void normalIdentity();
    void invalidDirectories();
    void isolatedRoots();
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
    if (RuntimeProfile::contains(QDir::tempPath() + QStringLiteral("/../external.kdbx"))) return 15;
    if (RuntimeProfile::contains(QStringLiteral("relative.kdbx"))) return 16;
    if (RuntimeProfile::contains(RuntimeProfile::root() + QStringLiteral("/../external.kdbx"))) return 17;
    if (RuntimeProfile::initialize({arguments.first()}, &error)) return 18;
    Database database;
    if (database.open(QStringLiteral("/external-personal.kdbx"), {}, &error)
        || !error.contains(QStringLiteral("verification profile"))) return 25;
    FileKey key;
    if (key.load(QStringLiteral("/external-personal.keyx"), &error)
        || !error.contains(QStringLiteral("verification profile"))) return 26;
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

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);
    const auto arguments = application.arguments();
    if (arguments.size() == 4 && arguments.at(1) == QStringLiteral("--probe-profile")) return probe(arguments);
    TestRuntimeProfile test;
    return QTest::qExec(&test, argc, argv);
}

#include "TestRuntimeProfile.moc"
