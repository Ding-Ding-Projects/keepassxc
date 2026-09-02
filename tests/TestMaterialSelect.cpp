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

#include "TestMaterialSelect.h"

#include "core/Config.h"
#include "util/TemporaryFile.h"
#include "gui/material/MaterialHistoryScreen.h"
#include "gui/material/MaterialReportsScreen.h"
#include "gui/material/MaterialSearchBar.h"
#include "gui/material/MaterialSelect.h"
#include "gui/material/MaterialSettingsScreen.h"

#include <QCoreApplication>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QSignalSpy>
#include <QTest>

QTEST_MAIN(TestMaterialSelect)

using namespace Material;

namespace
{
    Select* fruitSelect(QWidget* parent)
    {
        auto* select = new Select(parent);
        select->setObjectName(QStringLiteral("fruit"));
        select->setAccessibleName(QStringLiteral("Fruit"));
        select->addItem(QStringLiteral("Apple"), QStringLiteral("apple"));
        select->addItem(QStringLiteral("Banana"), QStringLiteral("banana"));
        select->addItem(QStringLiteral("Blackberry"), QStringLiteral("blackberry"));
        select->addItem(QStringLiteral("Cherry"), QStringLiteral("cherry"));
        return select;
    }

    int visibleRows(QListWidget* list)
    {
        int rows = 0;
        for (int row = 0; row < list->count(); ++row) {
            rows += list->isRowHidden(row) ? 0 : 1;
        }
        return rows;
    }
} // namespace

void TestMaterialSelect::initTestCase()
{
    Config::createConfigFromFile(TemporaryFile::createTempConfigFile(), {});
}

void TestMaterialSelect::mirrorsTheComboBoxSlice()
{
    QWidget host;
    auto* select = fruitSelect(&host);
    QCOMPARE(select->count(), 4);
    QCOMPARE(select->currentIndex(), 0);
    QCOMPARE(select->currentText(), QStringLiteral("Apple"));
    QCOMPARE(select->findData(QStringLiteral("cherry")), 3);
    QCOMPARE(select->findText(QStringLiteral("banana")), 1);
    QCOMPARE(select->itemData(2).toString(), QStringLiteral("blackberry"));

    QSignalSpy indexSpy(select, &Select::currentIndexChanged);
    QSignalSpy textSpy(select, &Select::currentTextChanged);
    select->setCurrentIndex(3);
    QCOMPARE(select->currentData().toString(), QStringLiteral("cherry"));
    QCOMPARE(indexSpy.count(), 1);
    QCOMPARE(textSpy.first().first().toString(), QStringLiteral("Cherry"));
    select->setCurrentIndex(3);
    QCOMPARE(indexSpy.count(), 1);
    select->setCurrentText(QStringLiteral("Banana"));
    QCOMPARE(select->currentIndex(), 1);
    select->setCurrentIndex(99);
    QCOMPARE(select->currentIndex(), 1);
    QVERIFY(select->sizeHint().height() > 0);
}

void TestMaterialSelect::popupFiltersWithSearchAndRegex()
{
    QWidget host;
    host.resize(400, 200);
    auto* select = fruitSelect(&host);
    select->setSearchIdentity(QStringLiteral("test.fruit"), QStringLiteral("Fruit search"));
    host.show();
    QCoreApplication::processEvents();

    QVERIFY(!select->isPopupOpen());
    select->showPopup();
    QVERIFY(select->isPopupOpen());
    QCOMPARE(select->searchBar()->searchId(), QStringLiteral("test.fruit"));
    QVERIFY(select->searchBar()->lineEdit()->hasFocus());
    QCOMPARE(visibleRows(select->listWidget()), 4);
    QCOMPARE(select->listWidget()->currentRow(), 0);

    select->searchBar()->setText(QStringLiteral("b"));
    QCOMPARE(visibleRows(select->listWidget()), 2);
    QCOMPARE(select->listWidget()->currentRow(), 1);

    // Regex is an opt-in; an unparsable pattern leaves the list as it was.
    select->searchBar()->setRegexEnabled(true);
    select->searchBar()->setText(QStringLiteral("^bl"));
    QCOMPARE(visibleRows(select->listWidget()), 1);
    select->searchBar()->setText(QStringLiteral("("));
    QCOMPARE(visibleRows(select->listWidget()), 1);
    select->searchBar()->setRegexEnabled(false);
    QCOMPARE(visibleRows(select->listWidget()), 0);
    QVERIFY(!select->listWidget()->accessibleDescription().isEmpty());

    select->hidePopup();
    QVERIFY(!select->isPopupOpen());
    // Reopening clears the filter and the regex opt-in.
    select->showPopup();
    QVERIFY(select->searchBar()->text().isEmpty());
    QVERIFY(!select->searchBar()->isRegexEnabled());
    QCOMPARE(visibleRows(select->listWidget()), 4);
    select->hidePopup();
}

void TestMaterialSelect::keyboardWalksTheListAndChooses()
{
    QWidget host;
    host.resize(400, 200);
    auto* select = fruitSelect(&host);
    host.show();
    select->setFocus();
    QCoreApplication::processEvents();

    QTest::keyClick(select, Qt::Key_Down);
    QVERIFY(select->isPopupOpen());
    QLineEdit* edit = select->searchBar()->lineEdit();
    QTest::keyClick(edit, Qt::Key_Down);
    QTest::keyClick(edit, Qt::Key_Down);
    QCOMPARE(select->listWidget()->currentRow(), 2);
    QTest::keyClick(edit, Qt::Key_Up);
    QCOMPARE(select->listWidget()->currentRow(), 1);

    QSignalSpy chosen(select, &Select::currentIndexChanged);
    QTest::keyClick(edit, Qt::Key_Return);
    QVERIFY(!select->isPopupOpen());
    QCOMPARE(select->currentIndex(), 1);
    QCOMPARE(chosen.count(), 1);

    // Escape clears a filter first, then closes.
    select->showPopup();
    edit = select->searchBar()->lineEdit();
    QTest::keyClicks(edit, QStringLiteral("che"));
    QCOMPARE(visibleRows(select->listWidget()), 1);
    QTest::keyClick(edit, Qt::Key_Escape);
    QVERIFY(select->isPopupOpen());
    QVERIFY(select->searchBar()->text().isEmpty());
    QCOMPARE(visibleRows(select->listWidget()), 4);
    QTest::keyClick(edit, Qt::Key_Escape);
    QVERIFY(!select->isPopupOpen());
    QCOMPARE(select->currentIndex(), 1);
}

void TestMaterialSelect::everyScreenSelectHasASearchIdentity()
{
    // The screens' dropdowns are all selects, each with its own named search.
    SettingsScreen settings;
    ReportsScreen reports;
    HistoryScreen history;
    const QList<QWidget*> owners{&settings, &reports, &history};
    int found = 0;
    for (QWidget* owner : owners) {
        const auto selects = owner->findChildren<Select*>();
        for (Select* select : selects) {
            QVERIFY2(!select->accessibleName().isEmpty(), qPrintable(select->objectName()));
            QVERIFY2(!select->searchBar()->searchId().isEmpty(), qPrintable(select->objectName()));
            ++found;
        }
    }
    // Font family, font weight, override element, report category, history preset.
    QVERIFY2(found >= 5, qPrintable(QString::number(found)));
}
