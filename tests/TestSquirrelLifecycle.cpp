#include "TestSquirrelLifecycle.h"

#include "platform/SquirrelLifecycle.h"

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QTest>

namespace
{
    SquirrelLifecycle::ProcessResult successfulProcess()
    {
        SquirrelLifecycle::ProcessResult result;
        result.started = true;
        result.exitStatus = SquirrelLifecycle::ExitStatus::Normal;
        result.exitCode = 0;
        return result;
    }

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
    SquirrelLifecycle::resetProcessRunnerForTests();
    SquirrelLifecycle::resetIntegrationRunnerForTests();
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

void TestSquirrelLifecycle::processResultContract()
{
    auto result = successfulProcess();
    QVERIFY(result.succeeded());
    result.exitCode = 5;
    QVERIFY(!result.succeeded());
    result = successfulProcess();
    result.finishTimedOut = true;
    QVERIFY(!result.succeeded());
    result = successfulProcess();
    result.exitStatus = SquirrelLifecycle::ExitStatus::Crashed;
    QVERIFY(!result.succeeded());
    result = successfulProcess();
    result.startTimedOut = true;
    QVERIFY(!result.succeeded());
    result = {};
    QVERIFY(!result.succeeded());
}

void TestSquirrelLifecycle::shortHelperEvidence()
{
    const auto success = SquirrelLifecycle::runShortHelper(
        QStringLiteral("cmd.exe"),
        {QStringLiteral("/d"),
         QStringLiteral("/s"),
         QStringLiteral("/c"),
         QStringLiteral("echo lifecycle-out & echo lifecycle-err 1>&2 & exit /b 0")},
        QDir::tempPath(),
        5000);
    QVERIFY(success.succeeded());
    QVERIFY(success.standardOutput.contains("lifecycle-out"));
    QVERIFY(success.standardError.contains("lifecycle-err"));
    QVERIFY(success.durationMs >= 0);

    const auto missing = SquirrelLifecycle::runShortHelper(
        QStringLiteral("definitely-missing-squirrel-helper.exe"), {}, QDir::tempPath(), 100);
    QVERIFY(!missing.started);
    QVERIFY(!missing.succeeded());
    QVERIFY(!missing.standardError.isEmpty());

    const auto timedOut = SquirrelLifecycle::runShortHelper(
        QStringLiteral("cmd.exe"),
        {QStringLiteral("/d"),
         QStringLiteral("/s"),
         QStringLiteral("/c"),
         QStringLiteral("ping -n 4 127.0.0.1 >nul")},
        QDir::tempPath(),
        50);
    QVERIFY(timedOut.started);
    QVERIFY(timedOut.finishTimedOut);
    QVERIFY(!timedOut.succeeded());
}

void TestSquirrelLifecycle::handleUsesExactOwnedSeams()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString app = createLayout(directory);
    QVERIFY(!app.isEmpty());

    QString program;
    QStringList helperArguments;
    QString workingDirectory;
    int helperTimeout = 0;
    int processCalls = 0;
    int integrationCalls = 0;
    SquirrelLifecycle::Event integratedEvent = SquirrelLifecycle::Event::None;
    SquirrelLifecycle::setProcessRunnerForTests(
        [&](const QString& executable, const QStringList& arguments, const QString& cwd, int timeout) {
            ++processCalls;
            program = executable;
            helperArguments = arguments;
            workingDirectory = cwd;
            helperTimeout = timeout;
            return successfulProcess();
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
    QCOMPARE(processCalls, 1);
    QCOMPARE(integrationCalls, 1);
    QCOMPARE(helperTimeout, 30000);
    QCOMPARE(integratedEvent, SquirrelLifecycle::Event::Install);
    QCOMPARE(helperArguments,
             QStringList({QStringLiteral("--createShortcut"), QStringLiteral("KeePassXC.exe")}));
    QCOMPARE(QDir::cleanPath(workingDirectory), QDir::cleanPath(QFileInfo(program).absolutePath()));

    QCOMPARE(SquirrelLifecycle::handle(install, app), std::optional<int>(EXIT_SUCCESS));
    QCOMPARE(processCalls, 2);
    QCOMPARE(integrationCalls, 2);

    const QStringList wrongVersion{QStringLiteral("KeePassXC.exe"),
                                   QStringLiteral("--squirrel-updated"),
                                   QStringLiteral("2.8.2")};
    QCOMPARE(SquirrelLifecycle::handle(wrongVersion, app), std::optional<int>(EXIT_FAILURE));
    QCOMPARE(processCalls, 2);
    QCOMPARE(integrationCalls, 2);

    const QStringList uninstall{QStringLiteral("KeePassXC.exe"),
                                QStringLiteral("--squirrel-uninstall"),
                                QStringLiteral("2.8.1")};
    QCOMPARE(SquirrelLifecycle::handle(uninstall, app), std::optional<int>(EXIT_SUCCESS));
    QCOMPARE(helperArguments,
             QStringList({QStringLiteral("--removeShortcut"), QStringLiteral("KeePassXC.exe")}));
    QCOMPARE(integratedEvent, SquirrelLifecycle::Event::Uninstall);
    QCOMPARE(SquirrelLifecycle::handle(uninstall, app), std::optional<int>(EXIT_SUCCESS));
    QCOMPARE(processCalls, 4);
    QCOMPARE(integrationCalls, 4);

    SquirrelLifecycle::setProcessRunnerForTests(
        [](const QString&, const QStringList&, const QString&, int) { return SquirrelLifecycle::ProcessResult{}; });
    QCOMPARE(SquirrelLifecycle::handle(install, app), std::optional<int>(EXIT_FAILURE));

    SquirrelLifecycle::setProcessRunnerForTests(
        [](const QString&, const QStringList&, const QString&, int) { return successfulProcess(); });
    SquirrelLifecycle::setIntegrationRunnerForTests(
        [](SquirrelLifecycle::Event, const SquirrelLifecycle::Layout&) {
            SquirrelLifecycle::IntegrationResult result;
            result.browser = false;
            return result;
        });
    QCOMPARE(SquirrelLifecycle::handle(install, app), std::optional<int>(EXIT_FAILURE));
}

QTEST_GUILESS_MAIN(TestSquirrelLifecycle)
