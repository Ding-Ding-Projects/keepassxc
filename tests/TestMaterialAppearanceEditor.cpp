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

#include "TestMaterialAppearanceEditor.h"

#include "core/Config.h"
#include "util/TemporaryFile.h"
#include "gui/material/MaterialAppearanceEditor.h"
#include "gui/material/MaterialColorPicker.h"
#include "gui/material/MaterialElementOverrides.h"
#include "gui/material/MaterialSearchBar.h"
#include "gui/material/MaterialSegmentedButton.h"
#include "gui/material/MaterialSelect.h"
#include "gui/material/MaterialSlider.h"
#include "gui/material/MaterialSwitch.h"

#include <QApplication>
#include <QCoreApplication>
#include <QLabel>
#include <QLineEdit>
#include <QSignalSpy>
#include <QTest>
#include <QVBoxLayout>

QTEST_MAIN(TestMaterialAppearanceEditor)

using namespace Material;

void TestMaterialAppearanceEditor::initTestCase()
{
    Config::createConfigFromFile(TemporaryFile::createTempConfigFile(), {});
    qApp->installEventFilter(AppearanceApplier::instance());
}

void TestMaterialAppearanceEditor::overrideModelRoundTripsEveryField()
{
    ElementOverrides::Override value;
    value.height = 52;
    value.radius = 20;
    value.fontSize = 13;
    value.spacing = 6;
    value.background = QColor(1, 2, 3, 200);
    value.foreground = QColor(250, 251, 252);
    value.fontFamily = QStringLiteral("Segoe UI");
    value.fontWeight = 700;
    value.italic = true;
    value.underline = true;
    value.strikeout = false;
    value.overline = true;
    value.letterSpacing = 1.5;
    value.lineHeight = 1.4;
    value.capitalization = int(QFont::SmallCaps);
    value.elevation = 3;
    value.borderWidth = 2;
    value.borderColor = QColor(9, 8, 7);
    value.opacity = 0.75;
    value.rainbow = true;
    value.rainbowLevel = 4;
    QVERIFY(!value.isEmpty());
    const auto back = ElementOverrides::Override::fromJson(value.toJson());
    QCOMPARE(back.toJson(), value.toJson());
    QCOMPARE(*back.fontFamily, QStringLiteral("Segoe UI"));
    QCOMPARE(*back.rainbowLevel, 4);
    QVERIFY(*back.rainbow);
    QCOMPARE(*back.capitalization, int(QFont::SmallCaps));

    // Bounds are enforced on the way in, so a hand-edited file cannot smuggle
    // a 900 px border or a 40 % line height.
    QJsonObject wild = value.toJson();
    wild[QStringLiteral("borderWidth")] = 900;
    wild[QStringLiteral("lineHeight")] = 0.1;
    wild[QStringLiteral("rainbowLevel")] = 99;
    const auto bounded = ElementOverrides::Override::fromJson(wild);
    QCOMPARE(*bounded.borderWidth, 8);
    QCOMPARE(*bounded.lineHeight, 0.8);
    QCOMPARE(*bounded.rainbowLevel, 5);
    QVERIFY(ElementOverrides::Override().isEmpty());
}

void TestMaterialAppearanceEditor::editorWritesOverridesAndTheElementFollows()
{
    ElementOverrides::instance()->resetAll();
    QWidget host;
    host.resize(600, 400);
    auto* layout = new QVBoxLayout(&host);
    auto* label = new QLabel(QStringLiteral("Probe"), &host);
    label->setObjectName(QStringLiteral("probeLabel"));
    layout->addWidget(label);
    host.show();
    QCoreApplication::processEvents();
    const QFont before = label->font();

    auto* editor = AppearanceEditor::instance();
    QSignalSpy targeted(editor, &AppearanceEditor::targetChanged);
    editor->editElement(label);
    QVERIFY(editor->isVisible());
    QCOMPARE(editor->currentKey(), QStringLiteral("probeLabel"));
    QCOMPARE(targeted.count(), 1);
    QCOMPARE(editor->propertySearch()->searchId(), QStringLiteral("appearance.editor"));

    // Typography: size, weight, italic, capitalization.
    editor->fontSize()->setValue(20);
    QCOMPARE(*ElementOverrides::instance()->get(QStringLiteral("probeLabel")).fontSize, 20);
    QCOMPARE(label->font().pointSize(), 20);
    editor->fontSizeEntry()->setText(QStringLiteral("14"));
    emit editor->fontSizeEntry()->editingFinished();
    QCOMPARE(label->font().pointSize(), 14);
    editor->fontWeight()->setCurrentSegment(QStringLiteral("700"));
    emit editor->fontWeight()->segmentSelected(QStringLiteral("700"));
    QCOMPARE(label->font().weight(), QFont::Bold);
    editor->italic()->setChecked(true);
    QVERIFY(label->font().italic());
    editor->capitalization()->setCurrentIndex(editor->capitalization()->findData(int(QFont::AllUppercase)));
    QCOMPARE(label->font().capitalization(), QFont::AllUppercase);

    // Colour: a typed background reaches the stylesheet and the contrast readout.
    editor->setCurrentTab(QStringLiteral("colour"));
    QLineEdit* hex = editor->backgroundPicker()->notationEdits().value(QStringLiteral("HEX"));
    hex->setFocus();
    hex->setText(QStringLiteral("#123456"));
    emit hex->editingFinished();
    QVERIFY(label->styleSheet().contains(QStringLiteral("background:#ff123456")));
    QVERIFY(label->styleSheet().startsWith(QStringLiteral("#probeLabel {")));
    QCOMPARE(editor->foregroundPicker()->referenceColor(), QColor(0x12, 0x34, 0x56));

    // Shape: radius and border land in the same stylesheet; height is a minimum.
    editor->radius()->setValue(24);
    editor->borderWidth()->setValue(2);
    editor->height()->setValue(60);
    QVERIFY(label->styleSheet().contains(QStringLiteral("border-radius:24px")));
    QVERIFY(label->styleSheet().contains(QStringLiteral("border:2px solid")));
    QCOMPARE(label->minimumHeight(), 60);

    // The rainbow is stored as a flag; the painted background changes over time.
    editor->backgroundPicker()->setRainbow(true);
    const auto rainbow = ElementOverrides::instance()->get(QStringLiteral("probeLabel"));
    QVERIFY(rainbow.rainbow.value_or(false));
    // The sentinel never enters the colour field; the rainbow is a flag beside it.
    QVERIFY(!ColorText::isRainbow(rainbow.toJson().value(QStringLiteral("background")).toString()));
    QVERIFY(label->styleSheet().contains(QStringLiteral("background:")));
    AppearanceApplier::instance()->setReducedMotion(true);
    const QString settled = label->styleSheet();
    AppearanceApplier::instance()->setReducedMotion(true);
    QCOMPARE(label->styleSheet(), settled); // one hue under reduced motion
    AppearanceApplier::instance()->setReducedMotion(false);

    // Reset restores the shipped look exactly.
    editor->resetElementButton()->click();
    QVERIFY(ElementOverrides::instance()->get(QStringLiteral("probeLabel")).isEmpty());
    QCOMPARE(label->styleSheet(), QString());
    QCOMPARE(label->font(), before);
    QCOMPARE(label->minimumHeight(), 0);

    // Escape closes and focus returns.
    QTest::keyClick(editor, Qt::Key_Escape);
    QVERIFY(!editor->isVisible());
}

void TestMaterialAppearanceEditor::presetsCopyPasteExportImport()
{
    ElementOverrides::instance()->resetAll();
    QWidget host;
    auto* layout = new QVBoxLayout(&host);
    auto* first = new QLabel(QStringLiteral("One"), &host);
    first->setObjectName(QStringLiteral("presetProbeOne"));
    auto* second = new QLabel(QStringLiteral("Two"), &host);
    second->setObjectName(QStringLiteral("presetProbeTwo"));
    layout->addWidget(first);
    layout->addWidget(second);
    host.show();

    auto* editor = AppearanceEditor::instance();
    editor->editElement(first);
    editor->setCurrentTab(QStringLiteral("shape"));
    editor->radius()->setValue(33);
    editor->spacing()->setValue(9);

    editor->setCurrentTab(QStringLiteral("presets"));
    editor->presetName()->setText(QStringLiteral("Loud"));
    editor->savePresetButton()->click();
    QVERIFY(editor->presetNames().contains(QStringLiteral("Loud")));
    QVERIFY(config()->get(Config::GUI_AppearancePresets).toString().contains(QStringLiteral("Loud")));
    const QString exported = editor->exportPresets();
    QVERIFY(exported.contains(QStringLiteral("\"radius\":33")));

    // Copy from one element, paste onto another.
    editor->copyStyleButton()->click();
    editor->editElement(second);
    QVERIFY(ElementOverrides::instance()->get(QStringLiteral("presetProbeTwo")).isEmpty());
    editor->pasteStyleButton()->click();
    QCOMPARE(*ElementOverrides::instance()->get(QStringLiteral("presetProbeTwo")).radius, 33);
    QVERIFY(second->styleSheet().contains(QStringLiteral("border-radius:33px")));

    // Apply the saved preset to a reset element, then round-trip the JSON.
    ElementOverrides::instance()->reset(QStringLiteral("presetProbeTwo"));
    editor->presetSelect()->setCurrentIndex(editor->presetSelect()->findData(QStringLiteral("Loud")));
    editor->applyPresetButton()->click();
    QCOMPARE(*ElementOverrides::instance()->get(QStringLiteral("presetProbeTwo")).spacing, 9);
    QString error;
    QVERIFY(!editor->importPresets(QStringLiteral("not json"), &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(editor->importPresets(QStringLiteral("{\"Quiet\":{\"radius\":4}}"), &error));
    QVERIFY(editor->presetNames().contains(QStringLiteral("Quiet")));
    QVERIFY(editor->importPresets(exported, &error));
    QVERIFY(editor->presetNames().contains(QStringLiteral("Loud")));
    editor->closeEditor();
    ElementOverrides::instance()->resetAll();
}

void TestMaterialAppearanceEditor::propertySearchAndShiftRightClick()
{
    ElementOverrides::instance()->resetAll();
    QWidget host;
    host.resize(400, 300);
    auto* layout = new QVBoxLayout(&host);
    auto* label = new QLabel(QStringLiteral("Click me"), &host);
    label->setObjectName(QStringLiteral("clickProbe"));
    layout->addWidget(label);
    host.show();
    QCoreApplication::processEvents();

    auto* editor = AppearanceEditor::instance();
    editor->closeEditor();
    QVERIFY(!editor->isVisible());
    // Shift+right-click on any element opens the editor on it.
    QTest::mouseClick(label, Qt::RightButton, Qt::ShiftModifier, label->rect().center());
    QVERIFY(editor->isVisible());
    QCOMPARE(editor->currentKey(), QStringLiteral("clickProbe"));

    // The property search narrows the rows; regex is an opt-in.
    editor->propertySearch()->setText(QStringLiteral("radius"));
    QCOMPARE(editor->visiblePropertyRows(), QStringList{QStringLiteral("radius")});
    editor->propertySearch()->setRegexEnabled(true);
    editor->propertySearch()->setText(QStringLiteral("^(font|text)"));
    const QStringList rows = editor->visiblePropertyRows();
    QVERIFY(rows.contains(QStringLiteral("fontFamily")));
    QVERIFY(rows.contains(QStringLiteral("textStyles")));
    QVERIFY(!rows.contains(QStringLiteral("radius")));
    editor->propertySearch()->setText(QStringLiteral("("));
    QCOMPARE(editor->visiblePropertyRows(), rows); // unparsable: unchanged
    editor->propertySearch()->setRegexEnabled(false);
    editor->propertySearch()->clear();
    QVERIFY(editor->visiblePropertyRows().size() > 15);
    editor->closeEditor();
}
