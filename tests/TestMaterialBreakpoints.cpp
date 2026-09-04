/*
 *  Copyright (C) 2026 KeePassXC Team <team@keepassxc.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 or (at your option)
 *  version 3 of the License.
 */

#include "TestMaterialBreakpoints.h"

#include "gui/material/MaterialBreakpoints.h"

#include <QTest>

using namespace Material;

void TestMaterialBreakpoints::boundaries()
{
    QCOMPARE(breakpointFor(-1), Breakpoint::Compact);
    QCOMPARE(breakpointFor(0), Breakpoint::Compact);
    QCOMPARE(breakpointFor(599), Breakpoint::Compact);
    QCOMPARE(breakpointFor(600), Breakpoint::Medium);
    QCOMPARE(breakpointFor(839), Breakpoint::Medium);
    QCOMPARE(breakpointFor(840), Breakpoint::Expanded);
    QCOMPARE(breakpointFor(1199), Breakpoint::Expanded);
    QCOMPARE(breakpointFor(1200), Breakpoint::Large);
    QCOMPARE(breakpointFor(1439), Breakpoint::Large);
    QCOMPARE(breakpointFor(1440), Breakpoint::ExtraLarge);
}

void TestMaterialBreakpoints::capabilities()
{
    QVERIFY(!hasRail(Breakpoint::Compact));
    QVERIFY(hasRail(Breakpoint::Medium));
    QVERIFY(!hasGroupPane(Breakpoint::Medium));
    QVERIFY(hasGroupPane(Breakpoint::Expanded));
    QVERIFY(hasGroupPane(Breakpoint::Large));
    QVERIFY(!hasInlineDetail(Breakpoint::Medium));
    QVERIFY(hasInlineDetail(Breakpoint::Expanded));
    QCOMPARE(railWidth(Breakpoint::Compact), 0);
    QCOMPARE(railWidth(Breakpoint::Medium), 72);
    QCOMPARE(railWidth(Breakpoint::Large), 88);
    QCOMPARE(detailWidth(Breakpoint::Compact), 0);
    QCOMPARE(detailWidth(Breakpoint::Expanded), 340);
    QCOMPARE(detailWidth(Breakpoint::Large), 360);
    QCOMPARE(detailWidth(Breakpoint::ExtraLarge), 392);
}

QTEST_GUILESS_MAIN(TestMaterialBreakpoints)
