#include "TestMaterialShellResponsive.h"

#include "gui/material/MaterialNavigationRail.h"
#include "gui/material/MaterialShell.h"
#include "gui/material/MaterialVaultScreen.h"
#include "gui/material/MaterialEntryDetail.h"
#include "gui/material/MaterialVaultSidebar.h"
#include "gui/material/MaterialSearchBar.h"
#include "gui/material/MaterialSearchRegistry.h"
#include "gui/material/MaterialSpecSheet.h"

#include <QApplication>
#include <QMenu>
#include <QLineEdit>
#include <QSignalSpy>
#include <QToolButton>
#include <QScrollBar>
#include <QWheelEvent>
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
    auto* moreBar = more->menu()->findChild<SearchBar*>(QStringLiteral("materialBottomNavigationMoreSearch"));
    QVERIFY(moreBar);
    auto* moreSearch = moreBar->lineEdit();
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

void TestMaterialShellResponsive::fallbackSearchesAreIndependentAndRestoreFocus()
{
    Shell shell;
    for (int index = 0; index < 10; ++index) {
        shell.addDestination(QStringLiteral("destination-%1").arg(index),
                             new QWidget,
                             QStringLiteral("folder"),
                             QStringLiteral("Destination %1").arg(index));
    }
    VaultScreen vault;
    shell.resize(599, 700);
    shell.show();
    vault.show();
    QApplication::processEvents();

    auto* moreButton = shell.findChild<QToolButton*>(QStringLiteral("materialBottomNavigationMore"));
    auto* more = shell.findChild<SearchBar*>(QStringLiteral("materialBottomNavigationMoreSearch"));
    auto* groups = vault.findChild<SearchBar*>(QStringLiteral("materialVaultGroupScopeSearch"));
    QVERIFY(moreButton);
    QVERIFY(more);
    QVERIFY(groups);
    QVERIFY(more != groups);
    QCOMPARE(more->searchId(), QStringLiteral("navigation.compact-more"));
    QCOMPARE(groups->searchId(), QStringLiteral("vault.group-scope"));
    QCOMPARE(SearchRegistry::instance()->bar(more->searchId()), more);
    QCOMPARE(SearchRegistry::instance()->bar(groups->searchId()), groups);

    more->setRegexFlags(QStringLiteral("im"));
    more->setRegexEnabled(true);
    more->setText(QStringLiteral("^Destination 9$"));
    int visible = 0;
    for (auto* action : moreButton->menu()->actions()) {
        if (action->objectName().startsWith(QStringLiteral("materialDestination_")) && action->isVisible()) {
            ++visible;
        }
    }
    QCOMPARE(visible, 1);
    QVERIFY(groups->text().isEmpty());
    QVERIFY(!groups->isRegexEnabled());
    QCOMPARE(groups->regexFlags(), QStringLiteral("i"));

    more->setText(QStringLiteral("["));
    QVERIFY(more->lineEdit()->accessibleDescription().startsWith(QStringLiteral("Invalid regular expression")));
    visible = 0;
    for (auto* action : moreButton->menu()->actions()) {
        if (action->objectName().startsWith(QStringLiteral("materialDestination_")) && action->isVisible()) {
            ++visible;
        }
    }
    QCOMPARE(visible, 0);

    groups->setRegexFlags(QStringLiteral("s"));
    groups->setRegexEnabled(true);
    groups->setText(QStringLiteral("["));
    QVERIFY(groups->lineEdit()->accessibleDescription().startsWith(QStringLiteral("Invalid regular expression")));
    QCOMPARE(more->regexFlags(), QStringLiteral("im"));
    QCOMPARE(more->text(), QStringLiteral("["));

    moreButton->menu()->popup(moreButton->mapToGlobal(QPoint(0, moreButton->height())));
    QApplication::processEvents();
    emit more->builderRequested();
    QCOMPARE(SearchRegistry::instance()->current(), more);
    moreButton->menu()->close();
    SearchRegistry::instance()->restoreCurrentFocus();
    QApplication::processEvents();
    QVERIFY(moreButton->menu()->isVisible());
    QCOMPARE(QApplication::focusWidget(), more->lineEdit());
    moreButton->menu()->close();

    auto* groupButton = vault.groupScopeButton();
    groupButton->menu()->popup(groupButton->mapToGlobal(QPoint(0, groupButton->height())));
    QApplication::processEvents();
    emit groups->builderRequested();
    QCOMPARE(SearchRegistry::instance()->current(), groups);
    groupButton->menu()->close();
    SearchRegistry::instance()->restoreCurrentFocus();
    QApplication::processEvents();
    QVERIFY(groupButton->menu()->isVisible());
    QCOMPARE(QApplication::focusWidget(), groups->lineEdit());
    groupButton->menu()->close();
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

void TestMaterialShellResponsive::settingsPageScrollsFromContentAndContainsScrollbar()
{
    SpecSheetPage page(QStringLiteral("general"), QStringLiteral("General"));
    for (int index = 0; index < 24; ++index) {
        page.addRow(QStringLiteral("Section"),
                    QStringLiteral("settings"),
                    QStringLiteral("Setting %1").arg(index),
                    QStringLiteral("Description"),
                    PillKind::Value,
                    QString::number(index));
    }
    page.resize(520, 360);
    page.show();
    QApplication::processEvents();

    auto* bar = page.verticalScrollBar();
    QVERIFY(bar->maximum() > 0);
    const int before = bar->value();
    auto* row = page.rows().at(12);
    const QPoint local = row->rect().center();
    QWheelEvent wheel(local,
                      row->mapToGlobal(local),
                      QPoint(),
                      QPoint(0, -120),
                      Qt::NoButton,
                      Qt::NoModifier,
                      Qt::ScrollUpdate,
                      false);
    QCoreApplication::sendEvent(row, &wheel);
    QVERIFY(bar->value() > before);

    const QRect barGeometry(bar->mapTo(&page, QPoint()), bar->size());
    QVERIFY(page.rect().contains(barGeometry.topLeft()));
    QVERIFY(page.rect().contains(barGeometry.bottomRight()));
}

QTEST_MAIN(TestMaterialShellResponsive)
