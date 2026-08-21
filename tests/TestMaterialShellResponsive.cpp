#include "TestMaterialShellResponsive.h"

#include "gui/material/MaterialNavigationRail.h"
#include "gui/material/MaterialShell.h"

#include <QApplication>
#include <QMenu>
#include <QSignalSpy>
#include <QToolButton>
#include <QTest>

using namespace Material;

void TestMaterialShellResponsive::preservesDestinationAccessAcrossBreakpoints()
{
    Shell shell;
    for (int index = 0; index < 10; ++index) {
        shell.addDestination(QStringLiteral("destination-%1").arg(index),
                             new QWidget,
                             QStringLiteral("folder"),
                             QStringLiteral("Destination %1").arg(index));
    }

    shell.resize(599, 700);
    shell.show();
    QApplication::processEvents();
    QCOMPARE(shell.breakpoint(), Breakpoint::Compact);
    QVERIFY(!shell.rail()->isVisible());
    auto* bottom = shell.findChild<QWidget*>(QStringLiteral("materialBottomNavigation"));
    QVERIFY(bottom);
    QVERIFY(bottom->isVisible());
    QCOMPARE(bottom->findChildren<QToolButton*>(QString(), Qt::FindDirectChildrenOnly).size(), 6);
    auto* more = shell.findChild<QToolButton*>(QStringLiteral("materialBottomNavigationMore"));
    QVERIFY(more);
    QVERIFY(more->menu());
    QCOMPARE(more->menu()->actions().size(), 5);

    shell.setCurrentDestination(QStringLiteral("destination-9"));
    QCOMPARE(shell.currentDestination(), QStringLiteral("destination-9"));
    shell.resize(600, 700);
    QApplication::processEvents();
    QCOMPARE(shell.breakpoint(), Breakpoint::Medium);
    QVERIFY(shell.rail()->isVisible());
    QVERIFY(shell.rail()->iconsOnly());
    QCOMPARE(shell.rail()->width(), 72);
    QCOMPARE(shell.currentDestination(), QStringLiteral("destination-9"));

    shell.resize(1200, 700);
    QApplication::processEvents();
    QCOMPARE(shell.breakpoint(), Breakpoint::Large);
    QVERIFY(!shell.rail()->iconsOnly());
    QCOMPARE(shell.rail()->width(), 88);
    QCOMPARE(shell.destinations().size(), 10);
}

QTEST_MAIN(TestMaterialShellResponsive)
