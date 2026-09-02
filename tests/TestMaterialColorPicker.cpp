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

#include "TestMaterialColorPicker.h"

#include "core/Config.h"
#include "util/TemporaryFile.h"
#include "gui/material/MaterialColorPicker.h"
#include "gui/material/MaterialSlider.h"
#include "gui/material/MaterialSwitch.h"

#include <QCoreApplication>
#include <QLabel>
#include <QLineEdit>
#include <QSignalSpy>
#include <QTest>

QTEST_MAIN(TestMaterialColorPicker)

using namespace Material;

namespace
{
    bool close(const QColor& a, const QColor& b, int tolerance = 2)
    {
        return qAbs(a.red() - b.red()) <= tolerance && qAbs(a.green() - b.green()) <= tolerance
               && qAbs(a.blue() - b.blue()) <= tolerance && qAbs(a.alpha() - b.alpha()) <= tolerance;
    }
} // namespace

void TestMaterialColorPicker::initTestCase()
{
    Config::createConfigFromFile(TemporaryFile::createTempConfigFile(), {});
}

void TestMaterialColorPicker::translatorWritesEveryNotation()
{
    const QColor red(255, 0, 0);
    QCOMPARE(ColorText::hex(red), QStringLiteral("#FF0000"));
    QCOMPARE(ColorText::rgb(red), QStringLiteral("rgb(255 0 0)"));
    QCOMPARE(ColorText::hsl(red), QStringLiteral("hsl(0 100% 50%)"));
    QCOMPARE(ColorText::hsv(red), QStringLiteral("hsv(0 100% 100%)"));
    QCOMPARE(ColorText::hwb(red), QStringLiteral("hwb(0 0% 0%)"));
    QCOMPARE(ColorText::cmyk(red), QStringLiteral("cmyk(0% 100% 100% 0%)"));
    QCOMPARE(ColorText::name(red), QStringLiteral("red"));
    // CIELAB and OKLab of pure sRGB red, to the published figures.
    QVERIFY2(ColorText::lab(red).startsWith(QStringLiteral("lab(53.2 80.1 67.2")), qPrintable(ColorText::lab(red)));
    QVERIFY2(ColorText::lch(red).startsWith(QStringLiteral("lch(53.2 104.6 40")), qPrintable(ColorText::lch(red)));
    QVERIFY2(ColorText::oklab(red).startsWith(QStringLiteral("oklab(0.628 0.225 0.126")), qPrintable(ColorText::oklab(red)));
    QVERIFY2(ColorText::oklch(red).startsWith(QStringLiteral("oklch(0.628 0.258 29.2")), qPrintable(ColorText::oklch(red)));
    QCOMPARE(ColorText::all(red).size(), 10);

    const QColor half(0, 128, 255, 128);
    QCOMPARE(ColorText::hex(half), QStringLiteral("#0080FF80"));
    QVERIFY(ColorText::rgb(half).endsWith(QStringLiteral("/ 0.5)")));
    QVERIFY(ColorText::name(half).isEmpty());
}

void TestMaterialColorPicker::translatorParsesEveryNotationBack()
{
    const QList<QColor> samples{QColor(255, 0, 0), QColor(0, 107, 90), QColor(18, 52, 86, 200), QColor(250, 250, 250), QColor(1, 2, 3)};
    for (const QColor& sample : samples) {
        const auto notations = ColorText::all(sample);
        for (const auto& pair : notations) {
            const QColor parsed = ColorText::parse(pair.second);
            QVERIFY2(parsed.isValid(), qPrintable(pair.first + QStringLiteral(": ") + pair.second));
            // CMYK and the Lab family round through real conversions; two
            // steps of 8-bit error is the honest tolerance for them.
            const int tolerance = (pair.first == QStringLiteral("CMYK") || pair.first.contains(QStringLiteral("LAB")) || pair.first.contains(QStringLiteral("LCH"))) ? 3 : 1;
            QVERIFY2(close(parsed, sample, tolerance), qPrintable(pair.first + QStringLiteral(": ") + pair.second + QStringLiteral(" -> ") + parsed.name(QColor::HexArgb)));
        }
    }
    QCOMPARE(ColorText::parse(QStringLiteral("goldenrod")), QColor(218, 165, 32));
    QCOMPARE(ColorText::parse(QStringLiteral("rgba(10, 20, 30, 0.5)")).alpha(), 128);
    QVERIFY(!ColorText::parse(QStringLiteral("not a colour")).isValid());
    QVERIFY(!ColorText::parse(QStringLiteral("rgb(1 2)")).isValid());
    QVERIFY(!ColorText::parse(QString()).isValid());
}

void TestMaterialColorPicker::contrastRatioMatchesWcag()
{
    QCOMPARE(QString::number(ColorText::contrastRatio(Qt::black, Qt::white), 'f', 2), QStringLiteral("21.00"));
    QCOMPARE(QString::number(ColorText::contrastRatio(Qt::white, Qt::white), 'f', 2), QStringLiteral("1.00"));
    // #767676 on white is the canonical 4.54:1 AA boundary example.
    QCOMPARE(QString::number(ColorText::contrastRatio(QColor(0x76, 0x76, 0x76), Qt::white), 'f', 2), QStringLiteral("4.54"));
    QCOMPARE(QString::number(ColorText::contrastRatio(Qt::white, QColor(0x76, 0x76, 0x76)), 'f', 2), QStringLiteral("4.54"));
}

void TestMaterialColorPicker::pickerFieldBarsAndEditsAgree()
{
    ColorPicker picker;
    picker.resize(360, 600);
    picker.show();
    QCoreApplication::processEvents();

    QSignalSpy changed(&picker, &ColorPicker::colorChanged);
    picker.setColor(QColor(0, 107, 90));
    QCOMPARE(changed.count(), 0); // programmatic set is silent
    const auto edits = picker.notationEdits();
    QCOMPARE(edits.size(), 10);
    QCOMPARE(edits.value(QStringLiteral("HEX"))->text(), QStringLiteral("#006B5A"));
    QCOMPARE(edits.value(QStringLiteral("RGB"))->text(), QStringLiteral("rgb(0 107 90)"));

    // Typing into one notation moves every other one and emits once.
    QLineEdit* hsl = edits.value(QStringLiteral("HSL"));
    hsl->setFocus();
    hsl->setText(QStringLiteral("hsl(0 100% 50%)"));
    emit hsl->editingFinished();
    QCOMPARE(changed.count(), 1);
    QCOMPARE(picker.color().rgba(), QColor(255, 0, 0).rgba());
    QCOMPARE(edits.value(QStringLiteral("HEX"))->text(), QStringLiteral("#FF0000"));

    // Bad text is kept in front of the user and changes nothing.
    QLineEdit* hex = edits.value(QStringLiteral("HEX"));
    hex->setFocus();
    hex->setText(QStringLiteral("#zz"));
    emit hex->editingFinished();
    QCOMPARE(changed.count(), 1);
    QCOMPARE(hex->text(), QStringLiteral("#zz"));
    QVERIFY(hex->property("invalid").toBool());
    QCOMPARE(picker.color().rgba(), QColor(255, 0, 0).rgba());

    // The contrast readout follows the reference colour.
    picker.setReferenceColor(Qt::white);
    QVERIFY(picker.contrastLabel()->text().contains(QStringLiteral("4.00:1")));
    picker.setReferenceColor(Qt::black);
    QVERIFY(picker.contrastLabel()->text().contains(QStringLiteral("5.25:1")));

    picker.addRecentColor(QColor(1, 2, 3));
    picker.addRecentColor(QColor(4, 5, 6));
    QCOMPARE(picker.recentColors().first(), QColor(4, 5, 6));

    // Every driver is keyboard operable.
    QWidget* field = picker.findChild<QWidget*>(QStringLiteral("colorPickerField"));
    QWidget* hue = picker.findChild<QWidget*>(QStringLiteral("colorPickerHue"));
    QWidget* alpha = picker.findChild<QWidget*>(QStringLiteral("colorPickerAlpha"));
    QVERIFY(field && hue && alpha);
    QVERIFY(field->focusPolicy() != Qt::NoFocus && hue->focusPolicy() != Qt::NoFocus && alpha->focusPolicy() != Qt::NoFocus);
    alpha->setFocus();
    QTest::keyClick(alpha, Qt::Key_Left, Qt::ShiftModifier);
    QVERIFY(picker.color().alpha() < 255);
}

void TestMaterialColorPicker::rainbowIsASentinelNotAColour()
{
    QVERIFY(ColorText::isRainbow(ColorText::rainbowSentinel()));
    QVERIFY(ColorText::isRainbow(QStringLiteral("RAINBOW")));
    QVERIFY(!ColorText::isRainbow(QStringLiteral("#FF0000")));
    QVERIFY(!QColor(ColorText::rainbowSentinel()).isValid());
    QVERIFY(ColorText::rainbowCycleMs(1) > ColorText::rainbowCycleMs(5));
    QCOMPARE(ColorText::rainbowCycleMs(0), ColorText::rainbowCycleMs(1));
    QCOMPARE(ColorText::rainbowCycleMs(9), ColorText::rainbowCycleMs(5));

    ColorPicker picker;
    QSignalSpy rainbow(&picker, &ColorPicker::rainbowChanged);
    QVERIFY(!picker.isRainbow());
    QVERIFY(!picker.rainbowSpeed()->isEnabled());
    picker.setRainbow(true);
    QVERIFY(picker.isRainbow());
    QVERIFY(picker.rainbowSpeed()->isEnabled());
    QCOMPARE(rainbow.count(), 1);
    picker.setRainbowLevel(5);
    QCOMPARE(picker.rainbowLevel(), 5);
    QCOMPARE(rainbow.count(), 2);
    // Reduced motion settles on one hue elsewhere; the picker itself only
    // records the choice, never a colour string.
    QVERIFY(picker.color().isValid());
}
