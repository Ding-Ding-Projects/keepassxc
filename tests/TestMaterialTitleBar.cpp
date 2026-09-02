/*
 *  Copyright (C) 2026 KeePassXC Team <team@keepassxc.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 or (at your option)
 *  version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "TestMaterialTitleBar.h"

#include "gui/material/MaterialNavigationRail.h"
#include "gui/material/MaterialShell.h"
#include "gui/material/MaterialTitleBar.h"

#include <QAbstractButton>
#include <QCoreApplication>
#include <QSignalSpy>
#include <QTest>

QTEST_MAIN(TestMaterialTitleBar)

using namespace Material;

void TestMaterialTitleBar::initTestCase()
{
    // Nothing to prepare: the bar reads only the theme.
}

void TestMaterialTitleBar::buttonsRequestWindowActions()
{
    TitleBar bar;
    bar.resize(800, TitleBar::Height);
    bar.show();
    QCoreApplication::processEvents();
    QSignalSpy minimize(&bar, &TitleBar::minimizeRequested);
    QSignalSpy maximize(&bar, &TitleBar::maximizeRequested);
    QSignalSpy close(&bar, &TitleBar::closeRequested);

    QVERIFY(bar.minimizeButton() && bar.maximizeButton() && bar.closeButton());
    QCOMPARE(bar.minimizeButton()->accessibleName(), QStringLiteral("Minimise"));
    QCOMPARE(bar.maximizeButton()->accessibleName(), QStringLiteral("Maximise"));
    QCOMPARE(bar.closeButton()->accessibleName(), QStringLiteral("Close"));
    bar.minimizeButton()->click();
    bar.maximizeButton()->click();
    bar.closeButton()->click();
    QCOMPARE(minimize.count(), 1);
    QCOMPARE(maximize.count(), 1);
    QCOMPARE(close.count(), 1);

    // Every caption button is the reference's 46 x 44 target, right-aligned in order.
    for (auto* button : {bar.minimizeButton(), bar.maximizeButton(), bar.closeButton()}) {
        QCOMPARE(button->size(), QSize(TitleBar::ButtonWidth, TitleBar::Height));
    }
    QCOMPARE(bar.closeButton()->x(), 800 - TitleBar::ButtonWidth);
    QCOMPARE(bar.maximizeButton()->x(), 800 - 2 * TitleBar::ButtonWidth);
    QCOMPARE(bar.minimizeButton()->x(), 800 - 3 * TitleBar::ButtonWidth);
}

void TestMaterialTitleBar::captionAreaExcludesButtons()
{
    TitleBar bar;
    bar.resize(600, TitleBar::Height);
    bar.show();
    QCoreApplication::processEvents();
    bar.setSubtitle(QStringLiteral("Personal.kdbx"));
    QVERIFY(bar.isCaptionArea(QPoint(200, 20)));
    QVERIFY(bar.isCaptionArea(QPoint(10, 5)));
    QVERIFY(!bar.isCaptionArea(QPoint(600 - 10, 20)));
    QVERIFY(!bar.isCaptionArea(QPoint(600 - TitleBar::ButtonWidth - 10, 20)));
    QVERIFY(!bar.isCaptionArea(QPoint(600 - 2 * TitleBar::ButtonWidth - 10, 20)));
    QVERIFY(!bar.isCaptionArea(QPoint(200, TitleBar::Height + 1)));
    QVERIFY(bar.accessibleDescription().contains(QStringLiteral("Personal.kdbx")));
}

void TestMaterialTitleBar::maximizedStateSwapsTheGlyph()
{
    TitleBar bar;
    QVERIFY(!bar.isMaximized());
    bar.setMaximized(true);
    QVERIFY(bar.isMaximized());
    QCOMPARE(bar.maximizeButton()->accessibleName(), QStringLiteral("Restore"));
    bar.setMaximized(false);
    QCOMPARE(bar.maximizeButton()->accessibleName(), QStringLiteral("Maximise"));
}

void TestMaterialTitleBar::shellHostsTheBarAboveEverything()
{
    Shell shell;
    shell.resize(1000, 700);
    shell.show();
    QCoreApplication::processEvents();
    QVERIFY(shell.titleBar());
    QCOMPARE(shell.titleBar()->geometry().top(), 0);
    QCOMPARE(shell.titleBar()->height(), TitleBar::Height);
    QCOMPARE(shell.titleBar()->width(), 1000);
    QVERIFY(shell.rail()->geometry().top() >= TitleBar::Height);
}

void TestMaterialTitleBar::narrowWidthKeepsEveryButton()
{
    TitleBar bar;
    bar.setSubtitle(QStringLiteral("A very long database name that cannot possibly fit.kdbx"));
    bar.resize(320, TitleBar::Height);
    bar.show();
    QCoreApplication::processEvents();
    for (auto* button : {bar.minimizeButton(), bar.maximizeButton(), bar.closeButton()}) {
        QVERIFY(bar.rect().contains(button->geometry()));
    }
    QVERIFY(bar.minimumSizeHint().width() <= 320);
}
