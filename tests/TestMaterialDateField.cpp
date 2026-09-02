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

#include "TestMaterialDateField.h"

#include "core/Config.h"
#include "util/TemporaryFile.h"
#include "gui/material/MaterialChangelogScreen.h"
#include "gui/material/MaterialDateField.h"
#include "gui/material/MaterialHistoryScreen.h"
#include "gui/material/MaterialSearchBar.h"
#include "gui/material/MaterialSelect.h"

#include <QAbstractButton>
#include <QCoreApplication>
#include <QLineEdit>
#include <QSignalSpy>
#include <QTest>

QTEST_MAIN(TestMaterialDateField)

using namespace Material;

void TestMaterialDateField::initTestCase()
{
    Config::createConfigFromFile(TemporaryFile::createTempConfigFile(), {});
}

void TestMaterialDateField::acceptsTypedIsoAndLocaleDates()
{
    DateField field(QDate(2026, 1, 15));
    field.setMinimumDate(QDate(1970, 1, 1));
    field.setDisplayFormat(QLocale().dateFormat(QLocale::ShortFormat));
    field.show();
    QCoreApplication::processEvents();
    QCOMPARE(field.sizeHint().height(), 40);

    // A pasted or typed ISO date is interpreted even though the display
    // format is the locale's; so is the locale's own short format.
    field.lineEdit()->setText(QStringLiteral("2026-08-14"));
    field.interpretText();
    QCOMPARE(field.date(), QDate(2026, 8, 14));

    const QString local = QLocale().toString(QDate(2025, 3, 9), QLocale::ShortFormat);
    field.lineEdit()->setText(local);
    field.interpretText();
    QCOMPARE(field.date(), QDate(2025, 3, 9));

    field.lineEdit()->setText(QStringLiteral("not a date"));
    field.interpretText();
    QCOMPARE(field.date(), QDate(2025, 3, 9));
}

void TestMaterialDateField::pickerChoosesADayAndNavigatesMonths()
{
    QWidget host;
    host.resize(400, 200);
    auto* field = new DateField(QDate(2026, 8, 14), &host);
    field->setObjectName(QStringLiteral("probe"));
    field->setSearchIdentity(QStringLiteral("test.date"), QStringLiteral("Test date"));
    host.show();
    QCoreApplication::processEvents();

    QVERIFY(!field->isPickerOpen());
    field->showPicker();
    QVERIFY(field->isPickerOpen());
    QCOMPARE(field->shownMonth(), QDate(2026, 8, 1));
    QCOMPARE(field->monthSelect()->currentData().toInt(), 8);
    QCOMPARE(field->yearSelect()->currentData().toInt(), 2026);
    QCOMPARE(field->monthSelect()->searchBar()->searchId(), QStringLiteral("test.date.month"));
    QCOMPARE(field->yearSelect()->searchBar()->searchId(), QStringLiteral("test.date.year"));
    QCOMPARE(field->dayButtons().size(), 42);

    // Month and year choosers move the grid without touching the value.
    field->monthSelect()->setCurrentIndex(0);
    QCOMPARE(field->shownMonth(), QDate(2026, 1, 1));
    QCOMPARE(field->date(), QDate(2026, 8, 14));
    field->yearSelect()->setCurrentIndex(field->yearSelect()->findData(2024));
    QCOMPARE(field->shownMonth(), QDate(2024, 1, 1));

    // Clicking a day sets it and closes the picker.
    QSignalSpy changed(field, &QDateEdit::dateChanged);
    QAbstractButton* chosen = nullptr;
    for (QAbstractButton* day : field->dayButtons()) {
        if (day->text() == QStringLiteral("20") && day->isEnabled() && day->accessibleName().contains(QStringLiteral("2024"))) {
            chosen = day;
            break;
        }
    }
    QVERIFY(chosen);
    chosen->click();
    QVERIFY(!field->isPickerOpen());
    QCOMPARE(field->date(), QDate(2024, 1, 20));
    QCOMPARE(changed.count(), 1);
}

void TestMaterialDateField::screensUseTheMaterialDateField()
{
    HistoryScreen history;
    ChangelogScreen changelog;
    for (QWidget* owner : QList<QWidget*>{&history, &changelog}) {
        const auto edits = owner->findChildren<QDateEdit*>();
        QCOMPARE(edits.size(), 2);
        for (QDateEdit* edit : edits) {
            auto* field = qobject_cast<DateField*>(edit);
            QVERIFY2(field, qPrintable(edit->objectName()));
            QVERIFY2(!field->monthSelect()->searchBar()->searchId().isEmpty(), qPrintable(edit->objectName()));
        }
    }
}
