#include "TestSquirrelLifecycle.h"

#include "platform/SquirrelLifecycle.h"

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QTest>

namespace
{
    QString createLayout(QTemporaryDir& directory, const QString& version = QStringLiteral("2.8.1"))
    {
        const QString root = directory.filePath(QStringLiteral("KeePassXC.Material"));
        const QString app = QDir(root).filePath(QStringLiteral("app-%1").arg(version));
        if (!QDir().mkpath(app)) {
            return {};
        }
        QFile updater(QDir(root).filePath(QStringLiteral("Update.exe")));
        if (!updater.open(QIODevice::WriteOnly) || updater.write("MZtest") <= 0) {
            return {};
        }
        updater.close();
        QFile application(QDir(app).filePath(QStringLiteral("KeePassXC.exe")));
        if (!application.open(QIODevice::WriteOnly) || application.write("MZapp") <= 0) {
            return {};
        }
        return app;
    }
}

void TestSquirrelLifecycle::cleanup()
{
    SquirrelLifecycle::resetIntegrationRunnerForTests();
    SquirrelLifecycle::resetShortcutRunnerForTests();
}

void TestSquirrelLifecycle::classification()
{
    using SquirrelLifecycle::Event;
    QCOMPARE(SquirrelLifecycle::classify({}), Event::None);
    QCOMPARE(SquirrelLifecycle::classify({QStringLiteral("KeePassXC.exe")}), Event::None);
    QCOMPARE(SquirrelLifecycle::classify(
                 {QStringLiteral("KeePassXC.exe"), QStringLiteral("--squirrel-install"), QStringLiteral("2.8.0")}),
             Event::Install);
    QCOMPARE(SquirrelLifecycle::classify(
                 {QStringLiteral("KeePassXC.exe"), QStringLiteral("--squirrel-updated"), QStringLiteral("2.8.1")}),
             Event::Updated);
    QCOMPARE(SquirrelLifecycle::classify(
                 {QStringLiteral("KeePassXC.exe"), QStringLiteral("--squirrel-uninstall"), QStringLiteral("2.8.0")}),
             Event::Uninstall);
    QCOMPARE(SquirrelLifecycle::classify(
                 {QStringLiteral("KeePassXC.exe"), QStringLiteral("--squirrel-obsolete"), QStringLiteral("2.8.0")}),
             Event::Obsolete);
    QCOMPARE(SquirrelLifecycle::classify(
                 {QStringLiteral("KeePassXC.exe"), QStringLiteral("--squirrel-firstrun")}),
             Event::FirstRun);

    QCOMPARE(SquirrelLifecycle::classify(
                 {QStringLiteral("KeePassXC.exe"), QStringLiteral("vault.kdbx"), QStringLiteral("--squirrel-uninstall")}),
             Event::Invalid);
    QCOMPARE(SquirrelLifecycle::classify(
                 {QStringLiteral("KeePassXC.exe"), QStringLiteral("--squirrel-install")}),
             Event::Invalid);
    QCOMPARE(SquirrelLifecycle::classify({QStringLiteral("KeePassXC.exe"),
                                          QStringLiteral("--squirrel-install"),
                                          QStringLiteral("2.8.0"),
                                          QStringLiteral("ordinary.kdbx")}),
             Event::Invalid);
    QCOMPARE(SquirrelLifecycle::classify({QStringLiteral("KeePassXC.exe"),
                                          QStringLiteral("--squirrel-install"),
                                          QStringLiteral("2.8.0"),
                                          QStringLiteral("--squirrel-install")}),
             Event::Invalid);
    QCOMPARE(SquirrelLifecycle::classify({QStringLiteral("KeePassXC.exe"),
                                          QStringLiteral("--squirrel-install"),
                                          QStringLiteral("2.8.0"),
                                          QStringLiteral("--squirrel-updated")}),
             Event::Invalid);
    QCOMPARE(SquirrelLifecycle::classify(
                 {QStringLiteral("KeePassXC.exe"), QStringLiteral("--SQUIRREL-INSTALL"), QStringLiteral("2.8.0")}),
             Event::None);
    QCOMPARE(SquirrelLifecycle::classify(
                 {QStringLiteral("KeePassXC.exe"), QStringLiteral("--squirrel-install"), QStringLiteral("02.8.0")}),
             Event::Invalid);
    QCOMPARE(SquirrelLifecycle::classify({QStringLiteral("KeePassXC.exe"),
                                          QStringLiteral("--squirrel-install"),
                                          QStringLiteral("2.8.0-alpha..1")}),
             Event::Invalid);
    QCOMPARE(SquirrelLifecycle::classify({QStringLiteral("KeePassXC.exe"),
                                          QStringLiteral("--squirrel-firstrun"),
                                          QStringLiteral("2.8.0")}),
             Event::Invalid);
}

void TestSquirrelLifecycle::firstRunConsumption()
{
    QStringList arguments{QStringLiteral("KeePassXC.exe"), QStringLiteral("--squirrel-firstrun")};
    QVERIFY(SquirrelLifecycle::consume(arguments));
    QCOMPARE(arguments, QStringList{QStringLiteral("KeePassXC.exe")});

    QStringList database{QStringLiteral("KeePassXC.exe"), QStringLiteral("--squirrel-firstrun.kdbx")};
    QVERIFY(!SquirrelLifecycle::consume(database));
    QCOMPARE(database.size(), 2);
}

void TestSquirrelLifecycle::layoutValidation()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString app = createLayout(directory);
    QVERIFY(!app.isEmpty());
    const auto layout = SquirrelLifecycle::validateLayout(app);
    QVERIFY(layout.has_value());
    QCOMPARE(layout->version, QStringLiteral("2.8.1"));
    QCOMPARE(QDir::cleanPath(layout->packageRoot),
             QDir::cleanPath(directory.filePath(QStringLiteral("KeePassXC.Material"))));
    QVERIFY(layout->updateExecutable.endsWith(QStringLiteral("Update.exe")));
    QVERIFY(layout->applicationExecutable.endsWith(QStringLiteral("KeePassXC.exe")));
    QCOMPARE(SquirrelLifecycle::openCommand(*layout),
             QStringLiteral("\"")
                 + QDir::toNativeSeparators(QDir(app).filePath(QStringLiteral("KeePassXC.exe")))
                 + QStringLiteral("\" \"%1\""));

    QVERIFY(!SquirrelLifecycle::validateLayout(directory.filePath(QStringLiteral("portable"))).has_value());
    const QString spoofRoot = directory.filePath(QStringLiteral("Other.Package/app-2.8.1"));
    QVERIFY(QDir().mkpath(spoofRoot));
    QFile spoofUpdater(directory.filePath(QStringLiteral("Other.Package/Update.exe")));
    QVERIFY(spoofUpdater.open(QIODevice::WriteOnly));
    QVERIFY(spoofUpdater.write("MZtest") > 0);
    spoofUpdater.close();
    QVERIFY(!SquirrelLifecycle::validateLayout(spoofRoot).has_value());
    QTemporaryDir malformedDirectory;
    QVERIFY(malformedDirectory.isValid());
    QVERIFY(!createLayout(malformedDirectory, QStringLiteral("02.8.1")).isEmpty());
    QVERIFY(!SquirrelLifecycle::validateLayout(
                 malformedDirectory.filePath(QStringLiteral("KeePassXC.Material/app-02.8.1")))
                 .has_value());
    QVERIFY(QFile::remove(layout->updateExecutable));
    QVERIFY(!SquirrelLifecycle::validateLayout(app).has_value());

    QTemporaryDir missingApplicationDirectory;
    QVERIFY(missingApplicationDirectory.isValid());
    const QString missingApplication = createLayout(missingApplicationDirectory);
    QVERIFY(!missingApplication.isEmpty());
    const QString applicationPath = QDir(missingApplication).filePath(QStringLiteral("KeePassXC.exe"));
    QVERIFY(QFile::remove(applicationPath));
    QVERIFY(!SquirrelLifecycle::validateLayout(missingApplication).has_value());
    QVERIFY(QDir().mkpath(applicationPath));
    QVERIFY(!SquirrelLifecycle::validateLayout(missingApplication).has_value());

    QTemporaryDir linkedApplicationDirectory;
    QVERIFY(linkedApplicationDirectory.isValid());
    const QString linkedApplication = createLayout(linkedApplicationDirectory);
    QVERIFY(!linkedApplication.isEmpty());
    const QString linkedPath = QDir(linkedApplication).filePath(QStringLiteral("KeePassXC.exe"));
    QVERIFY(QFile::remove(linkedPath));
    const QString realPath = linkedApplicationDirectory.filePath(QStringLiteral("real-keepassxc.exe"));
    QFile realApplication(realPath);
    QVERIFY(realApplication.open(QIODevice::WriteOnly));
    QVERIFY(realApplication.write("MZreal") > 0);
    realApplication.close();
    if (QFile::link(realPath, linkedPath) && QFileInfo(linkedPath).isSymLink()) {
        QVERIFY(!SquirrelLifecycle::validateLayout(linkedApplication).has_value());
    }

    QTemporaryDir junctionDirectory;
    QVERIFY(junctionDirectory.isValid());
    const QString junctionRoot = junctionDirectory.filePath(QStringLiteral("KeePassXC.Material"));
    const QString realApp = junctionDirectory.filePath(QStringLiteral("real-app-2.8.1"));
    QVERIFY(QDir().mkpath(junctionRoot));
    QVERIFY(QDir().mkpath(realApp));
    QFile junctionUpdater(QDir(junctionRoot).filePath(QStringLiteral("Update.exe")));
    QVERIFY(junctionUpdater.open(QIODevice::WriteOnly));
    QVERIFY(junctionUpdater.write("MZtest") > 0);
    junctionUpdater.close();
    QFile junctionApplication(QDir(realApp).filePath(QStringLiteral("KeePassXC.exe")));
    QVERIFY(junctionApplication.open(QIODevice::WriteOnly));
    QVERIFY(junctionApplication.write("MZapp") > 0);
    junctionApplication.close();
    const QString junctionApp = QDir(junctionRoot).filePath(QStringLiteral("app-2.8.1"));
    const int junctionExit = QProcess::execute(
        QStringLiteral("cmd.exe"),
        {QStringLiteral("/d"),
         QStringLiteral("/c"),
         QStringLiteral("mklink /J \"%1\" \"%2\" >nul").arg(QDir::toNativeSeparators(junctionApp),
                                                                  QDir::toNativeSeparators(realApp))});
    if (junctionExit == 0) {
        QVERIFY(!SquirrelLifecycle::validateLayout(junctionApp).has_value());
    }
}

void TestSquirrelLifecycle::registryOwnershipDecisions()
{
    using Decision = SquirrelLifecycle::RegistrationDecision;
    QCOMPARE(SquirrelLifecycle::registrationDecision(false, false), Decision::Claim);
    QCOMPARE(SquirrelLifecycle::registrationDecision(true, true), Decision::Refresh);
    QCOMPARE(SquirrelLifecycle::registrationDecision(true, false), Decision::PreserveForeign);
    // Losing our marker after installation makes the record foreign again. It
    // must be preserved during refresh and uninstall rather than reclaimed.
    QCOMPARE(SquirrelLifecycle::registrationDecision(true, false), Decision::PreserveForeign);
}

void TestSquirrelLifecycle::shortcutOwnershipContract()
{
    const SquirrelLifecycle::ShortcutOwnership recorded{QStringLiteral("C:/Users/Test/Desktop/KeePassXC.lnk"),
                                                        QStringLiteral("C:/Users/Test/AppData/Local/KeePassXC.Material/Update.exe"),
                                                        QStringLiteral("--processStart KeePassXC.exe"),
                                                        QByteArray(32, 'a')};
    QVERIFY(SquirrelLifecycle::shortcutOwnershipMatches(recorded, recorded));

    auto changed = recorded;
    changed.sha256[0] = 'b';
    QVERIFY(!SquirrelLifecycle::shortcutOwnershipMatches(recorded, changed));
    changed = recorded;
    changed.path = QStringLiteral("C:/Users/Test/Desktop/AnotherApp.lnk");
    QVERIFY(!SquirrelLifecycle::shortcutOwnershipMatches(recorded, changed));
    changed = recorded;
    changed.target = QStringLiteral("C:/Users/Test/AppData/Local/OtherApp/Update.exe");
    QVERIFY(!SquirrelLifecycle::shortcutOwnershipMatches(recorded, changed));
    changed = recorded;
    changed.arguments = QStringLiteral("--processStart OtherApp.exe");
    QVERIFY(!SquirrelLifecycle::shortcutOwnershipMatches(recorded, changed));
}

void TestSquirrelLifecycle::handleUsesExactOwnedSeams()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString app = createLayout(directory);
    QVERIFY(!app.isEmpty());

    int integrationCalls = 0;
    int shortcutCalls = 0;
    SquirrelLifecycle::Event integratedEvent = SquirrelLifecycle::Event::None;
    SquirrelLifecycle::setShortcutRunnerForTests(
        [&](SquirrelLifecycle::Event event, const SquirrelLifecycle::Layout&) {
            ++shortcutCalls;
            integratedEvent = event;
            return true;
        });
    SquirrelLifecycle::setIntegrationRunnerForTests(
        [&](SquirrelLifecycle::Event event, const SquirrelLifecycle::Layout&) {
            ++integrationCalls;
            integratedEvent = event;
            return SquirrelLifecycle::IntegrationResult{};
        });

    const QStringList install{QStringLiteral("KeePassXC.exe"),
                              QStringLiteral("--squirrel-install"),
                              QStringLiteral("2.8.1")};
    QCOMPARE(SquirrelLifecycle::handle(install, app), std::optional<int>(EXIT_SUCCESS));
    QCOMPARE(shortcutCalls, 1);
    QCOMPARE(integrationCalls, 1);
    QCOMPARE(integratedEvent, SquirrelLifecycle::Event::Install);

    QCOMPARE(SquirrelLifecycle::handle(install, app), std::optional<int>(EXIT_SUCCESS));
    QCOMPARE(shortcutCalls, 2);
    QCOMPARE(integrationCalls, 2);

    const QStringList updated{QStringLiteral("KeePassXC.exe"),
                              QStringLiteral("--squirrel-updated"),
                              QStringLiteral("2.8.1")};
    QCOMPARE(SquirrelLifecycle::handle(updated, app), std::optional<int>(EXIT_SUCCESS));
    QCOMPARE(shortcutCalls, 3);
    QCOMPARE(integratedEvent, SquirrelLifecycle::Event::Updated);

    const QStringList wrongVersion{QStringLiteral("KeePassXC.exe"),
                                   QStringLiteral("--squirrel-updated"),
                                   QStringLiteral("2.8.2")};
    QCOMPARE(SquirrelLifecycle::handle(wrongVersion, app), std::optional<int>(EXIT_FAILURE));
    QCOMPARE(shortcutCalls, 3);
    QCOMPARE(integrationCalls, 3);

    const QStringList uninstall{QStringLiteral("KeePassXC.exe"),
                                QStringLiteral("--squirrel-uninstall"),
                                QStringLiteral("2.8.1")};
    QCOMPARE(SquirrelLifecycle::handle(uninstall, app), std::optional<int>(EXIT_SUCCESS));
    QCOMPARE(shortcutCalls, 4);
    QCOMPARE(integratedEvent, SquirrelLifecycle::Event::Uninstall);
    QCOMPARE(SquirrelLifecycle::handle(uninstall, app), std::optional<int>(EXIT_SUCCESS));
    QCOMPARE(shortcutCalls, 5);
    QCOMPARE(integrationCalls, 5);

    SquirrelLifecycle::setIntegrationRunnerForTests(
        [](SquirrelLifecycle::Event, const SquirrelLifecycle::Layout&) {
            return SquirrelLifecycle::IntegrationResult{};
        });
    SquirrelLifecycle::setShortcutRunnerForTests(
        [](SquirrelLifecycle::Event, const SquirrelLifecycle::Layout&) { return false; });
    QCOMPARE(SquirrelLifecycle::handle(install, app), std::optional<int>(EXIT_FAILURE));
    QCOMPARE(shortcutCalls, 6);
    QCOMPARE(integrationCalls, 6);

    SquirrelLifecycle::setShortcutRunnerForTests(
        [&](SquirrelLifecycle::Event event, const SquirrelLifecycle::Layout&) {
            ++shortcutCalls;
            integratedEvent = event;
            return true;
        });
    SquirrelLifecycle::setIntegrationRunnerForTests(
        [](SquirrelLifecycle::Event, const SquirrelLifecycle::Layout&) {
            SquirrelLifecycle::IntegrationResult result;
            result.browser = false;
            return result;
        });
    QCOMPARE(SquirrelLifecycle::handle(install, app), std::optional<int>(EXIT_FAILURE));
    QCOMPARE(shortcutCalls, 7);
    QCOMPARE(integrationCalls, 7);
}

QTEST_GUILESS_MAIN(TestSquirrelLifecycle)
