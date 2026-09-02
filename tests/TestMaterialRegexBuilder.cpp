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

#include "TestMaterialRegexBuilder.h"

#include "core/Config.h"
#include "util/TemporaryFile.h"
#include "gui/material/MaterialRegexBuilder.h"

#include <QAbstractButton>
#include <QCoreApplication>
#include <QRegularExpression>
#include <QTest>

QTEST_MAIN(TestMaterialRegexBuilder)

using namespace Material;

void TestMaterialRegexBuilder::initTestCase()
{
    Config::createConfigFromFile(TemporaryFile::createTempConfigFile(), {});
}

void TestMaterialRegexBuilder::tokenBlocksRemoveAndReorder()
{
    RegexBuilder builder;
    builder.resize(900, 700);
    builder.show();
    QCoreApplication::processEvents();
    // a, \d and + are three token blocks.
    builder.setPattern(QStringLiteral("a\\d+"));
    QCoreApplication::processEvents();
    auto blocks = builder.findChildren<QAbstractButton*>(QRegularExpression(QStringLiteral("^regexTokenBlock_")));
    QCOMPARE(blocks.size(), 3);
    QVERIFY(builder.findChild<QWidget*>(QStringLiteral("regexTokenStripEmpty"))->isHidden());

    // Ctrl+Right on the first block moves it after the second.
    blocks.at(0)->setFocus();
    QTest::keyClick(blocks.at(0), Qt::Key_Right, Qt::ControlModifier);
    QCoreApplication::processEvents();
    QCOMPARE(builder.pattern(), QStringLiteral("\\da+"));

    // Clicking a block removes it.
    blocks = builder.findChildren<QAbstractButton*>(QRegularExpression(QStringLiteral("^regexTokenBlock_")));
    QCOMPARE(blocks.size(), 3);
    blocks.at(2)->click();
    QCoreApplication::processEvents();
    QCOMPARE(builder.pattern(), QStringLiteral("\\da"));

    builder.setPattern(QString());
    QCoreApplication::processEvents();
    QVERIFY(!builder.findChild<QWidget*>(QStringLiteral("regexTokenStripEmpty"))->isHidden());
}
