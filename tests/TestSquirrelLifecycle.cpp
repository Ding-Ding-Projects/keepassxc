#include "TestSquirrelLifecycle.h"

#include "platform/SquirrelLifecycle.h"

#include <QDir>
#include <QTest>

void TestSquirrelLifecycle::classification()
{
    using SquirrelLifecycle::Event;
    QCOMPARE(SquirrelLifecycle::classify({QStringLiteral("KeePassXC.exe")}), Event::None);
    QCOMPARE(SquirrelLifecycle::classify({QStringLiteral("KeePassXC.exe"), QStringLiteral("--squirrel-install"), QStringLiteral("2.8.0")}), Event::Install);
    QCOMPARE(SquirrelLifecycle::classify({QStringLiteral("KeePassXC.exe"), QStringLiteral("--squirrel-updated"), QStringLiteral("2.8.1")}), Event::Updated);
    QCOMPARE(SquirrelLifecycle::classify({QStringLiteral("KeePassXC.exe"), QStringLiteral("--squirrel-uninstall"), QStringLiteral("2.8.0")}), Event::Uninstall);
    QCOMPARE(SquirrelLifecycle::classify({QStringLiteral("KeePassXC.exe"), QStringLiteral("--squirrel-obsolete"), QStringLiteral("2.8.0")}), Event::Obsolete);
    QCOMPARE(SquirrelLifecycle::classify({QStringLiteral("KeePassXC.exe"), QStringLiteral("--squirrel-firstrun")}), Event::FirstRun);
    QCOMPARE(SquirrelLifecycle::classify({QStringLiteral("KeePassXC.exe"), QStringLiteral("vault.kdbx"), QStringLiteral("--squirrel-uninstall")}), Event::Invalid);
    QCOMPARE(SquirrelLifecycle::classify({QStringLiteral("KeePassXC.exe"), QStringLiteral("--squirrel-install")}), Event::Invalid);
    QCOMPARE(SquirrelLifecycle::classify({QStringLiteral("KeePassXC.exe"), QStringLiteral("--squirrel-install"), QStringLiteral("not-a-version")}), Event::Invalid);
    QCOMPARE(SquirrelLifecycle::classify({QStringLiteral("KeePassXC.exe"), QStringLiteral("--squirrel-install"), QStringLiteral("2.8.0"), QStringLiteral("--squirrel-updated")}), Event::Invalid);
}

void TestSquirrelLifecycle::updatePath()
{
    const QString appDir = QDir::fromNativeSeparators(QStringLiteral("C:/Users/example/AppData/Local/KeePassXC.Material/app-2.8.0"));
    QCOMPARE(QDir::fromNativeSeparators(SquirrelLifecycle::updateExecutable(appDir)),
             QStringLiteral("C:/Users/example/AppData/Local/KeePassXC.Material/Update.exe"));
}

QTEST_GUILESS_MAIN(TestSquirrelLifecycle)
