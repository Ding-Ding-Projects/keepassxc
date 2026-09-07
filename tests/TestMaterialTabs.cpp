#include "TestMaterialTabs.h"

#include "gui/material/MaterialTabDescriptor.h"
#include "gui/material/MaterialTabStrip.h"
#include "gui/material/MaterialTabOverflow.h"
#include "gui/material/MaterialSearchRegistry.h"

#include <QAbstractButton>
#include <QApplication>
#include <QMouseEvent>
#include <QSignalSpy>
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

void TestMaterialTabs::searchableOverflow()
{
    QWidget host;
    host.resize(900, 700);
    host.show();
    TabOverflow overflow(&host);
    const QList<TabDescriptor> tabs{
        {QStringLiteral("runtime-a"), QStringLiteral("file:a"), QStringLiteral("database"), QStringLiteral("Alpha"), true, true},
        {QStringLiteral("runtime-b"), QStringLiteral("file:b"), QStringLiteral("database"), QStringLiteral("Beta"), false, true},
    };
    overflow.setTabs(tabs, QStringLiteral("runtime-a"), {QStringLiteral("runtime-b")});
    overflow.openOverlay();
    QApplication::processEvents();
    QVERIFY(SearchRegistry::instance()->bar(QStringLiteral("tabs.open")));
    QString activated;
    connect(&overflow, &TabOverflow::tabActivated, this, [&](const QString& id) { activated = id; });
    QAbstractButton* beta = nullptr;
    for (auto* button : overflow.findChildren<QAbstractButton*>()) {
        if (button->text().contains(QStringLiteral("Beta"))) { beta = button; break; }
    }
    QVERIFY(beta);
    beta->click();
    QCOMPARE(activated, QStringLiteral("runtime-b"));
}

void TestMaterialTabs::pointerDragRequestsReorder()
{
    TabStrip strip;
    strip.resize(900, 48);
    strip.setTabs({
                      {QStringLiteral("runtime-a"), QStringLiteral("file:a"), QStringLiteral("database"), QStringLiteral("Alpha"), false, true},
                      {QStringLiteral("runtime-b"), QStringLiteral("file:b"), QStringLiteral("database"), QStringLiteral("Beta"), false, true},
                      {QStringLiteral("runtime-c"), QStringLiteral("file:c"), QStringLiteral("database"), QStringLiteral("Gamma"), false, true},
                  },
                  QStringLiteral("runtime-a"));
    strip.show();
    QVERIFY(QTest::qWaitForWindowExposed(&strip));

    QSignalSpy moves(&strip, &TabStrip::tabMoveRequested);
    QVERIFY(moves.isValid());

    const QPoint source(36, strip.height() - 18);
    const QPoint beforeThird(200, strip.height() - 18);
    QTest::mousePress(&strip, Qt::LeftButton, Qt::NoModifier, source);
    QMouseEvent drag(QEvent::MouseMove,
                     QPointF(beforeThird),
                     QPointF(strip.mapToGlobal(beforeThird)),
                     Qt::NoButton,
                     Qt::LeftButton,
                     Qt::NoModifier);
    QCoreApplication::sendEvent(&strip, &drag);
    QTest::mouseRelease(&strip, Qt::LeftButton, Qt::NoModifier, beforeThird);

    QCOMPARE(moves.count(), 1);
    const QList<QVariant> request = moves.takeFirst();
    QCOMPARE(request.at(0).toString(), QStringLiteral("runtime-a"));
    QCOMPARE(request.at(1).toString(), QStringLiteral("runtime-c"));
}

QTEST_MAIN(TestMaterialTabs)
