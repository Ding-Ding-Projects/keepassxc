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

#include "TestMaterialSlider.h"

#include "core/Config.h"
#include "util/TemporaryFile.h"
#include "gui/material/MaterialSettingsScreen.h"
#include "gui/material/MaterialSlider.h"

#include <QCoreApplication>
#include <QSignalSpy>
#include <QTest>

QTEST_MAIN(TestMaterialSlider)

using namespace Material;

void TestMaterialSlider::initTestCase()
{
    Config::createConfigFromFile(TemporaryFile::createTempConfigFile(), {});
}

void TestMaterialSlider::pointerJumpsAndDrags()
{
    Slider slider(Qt::Horizontal);
    slider.setRange(0, 100);
    slider.resize(304, 44);
    slider.show();
    QCoreApplication::processEvents();
    QCOMPARE(slider.sizeHint().height(), 44);

    QSignalSpy pressed(&slider, &QAbstractSlider::sliderPressed);
    QSignalSpy released(&slider, &QAbstractSlider::sliderReleased);
    QSignalSpy changed(&slider, &QAbstractSlider::valueChanged);

    // A press jumps straight to the pointer rather than stepping a page.
    QTest::mousePress(&slider, Qt::LeftButton, Qt::NoModifier, QPoint(slider.width() / 2, 22));
    QVERIFY(slider.isSliderDown());
    QVERIFY(qAbs(slider.value() - 50) <= 2);
    QCOMPARE(pressed.count(), 1);

    QTest::mouseMove(&slider, QPoint(slider.width() - 4, 22));
    QCoreApplication::processEvents();
    QTest::mouseRelease(&slider, Qt::LeftButton, Qt::NoModifier, QPoint(slider.width() - 4, 22));
    QVERIFY(!slider.isSliderDown());
    QCOMPARE(slider.value(), 100);
    QCOMPARE(released.count(), 1);
    QVERIFY(changed.count() >= 2);

    QTest::mousePress(&slider, Qt::LeftButton, Qt::NoModifier, QPoint(2, 22));
    QTest::mouseRelease(&slider, Qt::LeftButton, Qt::NoModifier, QPoint(2, 22));
    QCOMPARE(slider.value(), 0);
}

void TestMaterialSlider::keyboardStepsAndSignals()
{
    Slider slider;
    slider.setRange(0, 10);
    slider.setSingleStep(1);
    slider.setPageStep(5);
    slider.setValue(3);
    slider.show();
    slider.setFocus();
    QCoreApplication::processEvents();

    QTest::keyClick(&slider, Qt::Key_Right);
    QCOMPARE(slider.value(), 4);
    QTest::keyClick(&slider, Qt::Key_Left);
    QCOMPARE(slider.value(), 3);
    QTest::keyClick(&slider, Qt::Key_PageUp);
    QCOMPARE(slider.value(), 8);
    QTest::keyClick(&slider, Qt::Key_Home);
    QCOMPARE(slider.value(), 0);
    QTest::keyClick(&slider, Qt::Key_End);
    QCOMPARE(slider.value(), 10);
    slider.setValueLabelSuffix(QStringLiteral(" px"));
    slider.setShowsValueLabel(false);
    QVERIFY(!slider.showsValueLabel());
}

void TestMaterialSlider::screensUseTheMaterialSlider()
{
    SettingsScreen screen;
    const auto sliders = screen.findChildren<QSlider*>();
    QVERIFY(!sliders.isEmpty());
    for (QSlider* slider : sliders) {
        QVERIFY2(qobject_cast<Slider*>(slider), qPrintable(slider->objectName()));
    }
}
