#include "TestSquirrelLifecycle.h"

#include "platform/SquirrelLifecycle.h"

#include <QDir>
#include <QTest>

void TestSquirrelLifecycle::classification()
{
    using SquirrelLifecycle::Event;
    QCOMPARE(SquirrelLifecycle::classify({QStringLiteral("KeePassXC.exe")}), Event::None);
    QCOMPARE(SquirrelLifecycle::classify({QStringLiteral("KeePassXC.exe"), QStringLiteral("--squirrel-install"), QStringLiteral("2.8.0")}), Event::Install);
    QCOMPARE(SquirrelLifecycle::classify({QStringLiteral("KeePassXC.exe"), QStringLiteral("--squirrel-updated")}), Event::Updated);
    QCOMPARE(SquirrelLifecycle::classify({QStringLiteral("KeePassXC.exe"), QStringLiteral("--squirrel-uninstall")}), Event::Uninstall);
    QCOMPARE(SquirrelLifecycle::classify({QStringLiteral("KeePassXC.exe"), QStringLiteral("--squirrel-obsolete")}), Event::Obsolete);
    QCOMPARE(SquirrelLifecycle::classify({QStringLiteral("KeePassXC.exe"), QStringLiteral("--squirrel-firstrun")}), Event::FirstRun);
}

void TestSquirrelLifecycle::updatePath()
{
    const QString appDir = QDir::fromNativeSeparators(QStringLiteral("C:/Users/example/AppData/Local/KeePassXC.Material/app-2.8.0"));
    QCOMPARE(QDir::fromNativeSeparators(SquirrelLifecycle::updateExecutable(appDir)),
             QStringLiteral("C:/Users/example/AppData/Local/KeePassXC.Material/Update.exe"));
}

QTEST_GUILESS_MAIN(TestSquirrelLifecycle)
