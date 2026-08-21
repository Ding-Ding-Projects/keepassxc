#include "TestMaterialShellResponsive.h"

#include "gui/material/MaterialNavigationRail.h"
#include "gui/material/MaterialShell.h"
#include "gui/material/MaterialVaultScreen.h"
#include "gui/material/MaterialEntryDetail.h"
#include "gui/material/MaterialVaultSidebar.h"

#include <QApplication>
#include <QMenu>
#include <QLineEdit>
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
    shell.resize(320, 700);
    QApplication::processEvents();
    for (auto* button : bottom->findChildren<QToolButton*>(QString(), Qt::FindDirectChildrenOnly)) {
        QVERIFY(button->width() >= 48);
        QVERIFY(button->height() >= 48);
        QVERIFY(bottom->rect().contains(button->geometry()));
        QVERIFY(!button->accessibleName().isEmpty());
    }
    auto* more = shell.findChild<QToolButton*>(QStringLiteral("materialBottomNavigationMore"));
    QVERIFY(more);
    QVERIFY(more->menu());
    QCOMPARE(more->menu()->actions().size(), 6); // search field and five destinations
    more->menu()->popup(more->mapToGlobal(QPoint(0, 0)));
    QApplication::processEvents();
    auto* moreSearch = more->menu()->findChild<QLineEdit*>(QStringLiteral("materialBottomNavigationMoreSearch"));
    QVERIFY(moreSearch);
    QCOMPARE(moreSearch->accessibleName(), QStringLiteral("Search more destinations"));
    moreSearch->setText(QStringLiteral("Destination 9"));
    QApplication::processEvents();
    int visibleDestinations = 0;
    for (auto* action : more->menu()->actions()) {
        if (action->objectName().startsWith(QStringLiteral("materialDestination_")) && action->isVisible()) {
            ++visibleDestinations;
        }
    }
    QCOMPARE(visibleDestinations, 1);
    more->menu()->close();

    shell.setCurrentDestination(QStringLiteral("destination-1"));
    auto* direct = shell.findChild<QToolButton*>(QStringLiteral("materialBottomDestination_destination-1"));
    QVERIFY(direct);
    QVERIFY(direct->isChecked());
    QCOMPARE(direct->accessibleDescription(), QStringLiteral("Current destination"));

    shell.setCurrentDestination(QStringLiteral("destination-9"));
    QCOMPARE(shell.currentDestination(), QStringLiteral("destination-9"));
    shell.resize(600, 700);
    QApplication::processEvents();
    QCOMPARE(shell.breakpoint(), Breakpoint::Medium);
    QVERIFY(shell.rail()->isVisible());
    QVERIFY(shell.rail()->iconsOnly());
    QCOMPARE(shell.rail()->width(), 72);
    QCOMPARE(shell.currentDestination(), QStringLiteral("destination-9"));

    shell.rail()->setFocus(Qt::OtherFocusReason);
    QVERIFY(shell.rail()->hasFocus());
    shell.resize(599, 700);
    QApplication::processEvents();
    QCOMPARE(QApplication::focusWidget(), more);
    QVERIFY(more->isChecked());
    QCOMPARE(more->accessibleDescription(), QStringLiteral("Current destination is in More"));

    shell.resize(1200, 700);
    QApplication::processEvents();
    QCOMPARE(shell.breakpoint(), Breakpoint::Large);
    QVERIFY(!shell.rail()->iconsOnly());
    QCOMPARE(shell.rail()->width(), 88);
    QCOMPARE(shell.destinations().size(), 10);
}

void TestMaterialShellResponsive::emitsOnlyOnBreakpointTransitions()
{
    Shell shell;
    shell.show();
    QSignalSpy spy(&shell, &Shell::breakpointChanged);

    const QList<QPair<int, Breakpoint>> widths{{599, Breakpoint::Compact},
                                                {600, Breakpoint::Medium},
                                                {839, Breakpoint::Medium},
                                                {840, Breakpoint::Expanded},
                                                {1199, Breakpoint::Expanded},
                                                {1200, Breakpoint::Large},
                                                {1439, Breakpoint::Large},
                                                {1440, Breakpoint::ExtraLarge}};
    Breakpoint previous = shell.breakpoint();
    int expectedSignals = 0;
    for (const auto& item : widths) {
        shell.resize(item.first, 700);
        QApplication::processEvents();
        QCOMPARE(shell.breakpoint(), item.second);
        if (item.second != previous) {
            ++expectedSignals;
            previous = item.second;
        }
        QCOMPARE(spy.count(), expectedSignals);
    }
}

void TestMaterialShellResponsive::appliesVaultPaneContract()
{
    VaultScreen vault;
    vault.resize(1500, 800);
    vault.show();
    QApplication::processEvents();

    vault.setBreakpoint(Breakpoint::ExtraLarge);
    QVERIFY(vault.groupPaneVisible());
    QCOMPARE(vault.sidebar()->width(), 250);
    QVERIFY(vault.detailPaneInline());
    QCOMPARE(vault.detail()->width(), 392);
    QVERIFY(!vault.groupScopeButton()->isVisible());
    QVERIFY(!vault.detailSheetButton()->isVisible());

    vault.setBreakpoint(Breakpoint::Large);
    QVERIFY(vault.groupPaneVisible());
    QCOMPARE(vault.sidebar()->width(), 216);
    QVERIFY(vault.detailPaneInline());
    QCOMPARE(vault.detail()->width(), 360);

    vault.setBreakpoint(Breakpoint::Expanded);
    QVERIFY(!vault.groupPaneVisible());
    QVERIFY(vault.groupScopeButton()->isVisible());
    QVERIFY(!vault.groupScopeButton()->accessibleName().isEmpty());
    QVERIFY(vault.detailPaneInline());
    QCOMPARE(vault.detail()->width(), 340);

    vault.setBreakpoint(Breakpoint::Medium);
    QVERIFY(!vault.groupPaneVisible());
    QVERIFY(!vault.detailPaneInline());
    QVERIFY(vault.detailSheetButton()->isVisible());
    QVERIFY(!vault.detailSheetButton()->accessibleName().isEmpty());

    vault.setBreakpoint(Breakpoint::Compact);
    QVERIFY(!vault.groupPaneVisible());
    QVERIFY(!vault.detailPaneInline());
    QVERIFY(vault.groupScopeButton()->isVisible());
    QVERIFY(vault.detailSheetButton()->isVisible());
}

QTEST_MAIN(TestMaterialShellResponsive)
