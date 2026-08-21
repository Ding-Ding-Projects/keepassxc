#include "TestMaterialTabs.h"

#include "gui/material/MaterialTabDescriptor.h"
#include "gui/material/MaterialTabStrip.h"

#include <QTest>

using namespace Material;

void TestMaterialTabs::persistenceIdentity()
{
    const QString first = tabPersistenceKeyForPath(QStringLiteral("C:\\Vaults\\Example.kdbx"));
    const QString same = tabPersistenceKeyForPath(QStringLiteral("c:/vaults/example.kdbx"));
    const QString other = tabPersistenceKeyForPath(QStringLiteral("C:/vaults/other.kdbx"));
    QVERIFY(first.startsWith(QStringLiteral("file:")));
    QCOMPARE(first, same);
    QVERIFY(first != other);
    QVERIFY(!first.contains(QStringLiteral("vaults"), Qt::CaseInsensitive));
    QVERIFY(tabPersistenceKeyForPath(QString()).isEmpty());
}

void TestMaterialTabs::atomicReconciliation()
{
    TabStrip strip;
    const QList<TabDescriptor> initial{
        {QStringLiteral("runtime-a"), QStringLiteral("file:a"), QStringLiteral("database"), QStringLiteral("A"), true, true},
        {QStringLiteral("runtime-b"), QStringLiteral("file:b"), QStringLiteral("database"), QStringLiteral("B"), false, true},
        {QStringLiteral("runtime-c"), QString(), QStringLiteral("database"), QStringLiteral("Unsaved"), false, false},
    };
    strip.setTabs(initial, QStringLiteral("runtime-b"));
    QCOMPARE(strip.count(), 3);
    QCOMPARE(strip.currentTab(), QStringLiteral("runtime-b"));
    QCOMPARE(strip.tabs().at(0).pinned, true);
    QCOMPARE(strip.tabs().at(2).persistable, false);

    QList<TabDescriptor> refreshed = initial;
    refreshed[1].label = QStringLiteral("Renamed B");
    strip.setTabs(refreshed, QStringLiteral("runtime-b"));
    QCOMPARE(strip.currentTab(), QStringLiteral("runtime-b"));
    QCOMPARE(strip.tabs().at(1).label, QStringLiteral("Renamed B"));
}

QTEST_MAIN(TestMaterialTabs)
