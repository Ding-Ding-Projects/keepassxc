#include "TestMaterialAppearance.h"

#include "core/Config.h"
#include "gui/material/MaterialCard.h"
#include "gui/material/MaterialElementOverrides.h"
#include "gui/material/MaterialSearchBar.h"
#include "gui/material/MaterialSearchRegistry.h"
#include "gui/material/MaterialSegmentedButton.h"
#include "gui/material/MaterialSettingsScreen.h"
#include "gui/material/MaterialTheme.h"

#include <QComboBox>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSlider>
#include <QTest>

using namespace Material;

void TestMaterialAppearance::initTestCase()
{
    m_theme = config()->get(Config::GUI_ApplicationTheme).toString();
    m_family = config()->get(Config::GUI_FontFamily).toString();
    m_seed = config()->get(Config::GUI_MaterialSeed).toString();
    m_density = config()->get(Config::GUI_MaterialDensity).toString();
    m_scale = config()->get(Config::GUI_FontScale).toDouble();
    m_weight = config()->get(Config::GUI_FontWeight).toInt();
    m_overrides = config()->get(Config::GUI_ElementOverrides).toString();
}

void TestMaterialAppearance::cleanupTestCase()
{
    config()->set(Config::GUI_ApplicationTheme, m_theme);
    config()->set(Config::GUI_FontFamily, m_family);
    config()->set(Config::GUI_MaterialSeed, m_seed);
    config()->set(Config::GUI_MaterialDensity, m_density);
    config()->set(Config::GUI_FontScale, m_scale);
    config()->set(Config::GUI_FontWeight, m_weight);
    config()->set(Config::GUI_ElementOverrides, m_overrides);
    ElementOverrides::instance()->load();
    theme()->reload();
}

void TestMaterialAppearance::themeTypographyAndResponsiveControls()
{
    SettingsScreen screen;
    screen.resize(1200, 860);
    screen.show();
    QTest::qWait(1);

    auto* mode = screen.findChild<SegmentedButton*>(QStringLiteral("appearanceThemeMode"));
    auto* density = screen.findChild<SegmentedButton*>(QStringLiteral("appearanceDensity"));
    auto* family = screen.findChild<QComboBox*>(QStringLiteral("appearanceFontFamily"));
    auto* scale = screen.findChild<QSlider*>(QStringLiteral("appearanceFontScale"));
    auto* weight = screen.findChild<QComboBox*>(QStringLiteral("appearanceFontWeight"));
    auto* preview = screen.findChild<QLabel*>(QStringLiteral("appearanceFontPreview"));
    QVERIFY(mode && density && family && scale && weight && preview);
    QCOMPARE(mode->focusPolicy(), Qt::StrongFocus);

    mode->setCurrentSegment(QStringLiteral("auto"));
    QCOMPARE(config()->get(Config::GUI_ApplicationTheme).toString(), QStringLiteral("auto"));
    mode->setFocus();
    QTest::keyClick(mode, Qt::Key_Right);
    QCOMPARE(config()->get(Config::GUI_ApplicationTheme).toString(), QStringLiteral("light"));

    density->setCurrentSegment(QStringLiteral("compact"));
    QCOMPARE(theme()->rowHeight(), 40);
    QCOMPARE(theme()->pagePadding(), 14);

    const auto swatches = screen.findChildren<SeedSwatch*>();
    QCOMPARE(swatches.size(), 4);
    swatches.at(1)->click();
    QCOMPARE(config()->get(Config::GUI_MaterialSeed).toString(), Theme::seedToString(swatches.at(1)->seed()));
    QVERIFY(swatches.at(1)->isChecked());

    if (family->count() > 0) {
        family->setCurrentIndex(0);
        QCOMPARE(config()->get(Config::GUI_FontFamily).toString(), family->currentText());
        QCOMPARE(theme()->font(TypeRole::BodyMedium).family(), family->currentText());
    }

    scale->setValue(110);
    QCOMPARE(config()->get(Config::GUI_FontScale).toDouble(), 1.1);
    weight->setCurrentIndex(weight->findData(500));
    QCOMPARE(config()->get(Config::GUI_FontWeight).toInt(), 500);
    QCOMPARE(preview->font().family(), theme()->font(TypeRole::BodyMedium).family());

    const auto cards = screen.findChildren<Card*>();
    QVERIFY(cards.size() >= 6);
    screen.resize(599, 860);
    QTest::qWait(1);
    int left = -1;
    for (auto* card : cards) {
        if (!card->isVisible()) continue;
        if (left < 0) left = card->x();
        QCOMPARE(card->x(), left);
        QVERIFY(card->parentWidget());
        QVERIFY(card->geometry().right() <= card->parentWidget()->width());
    }
}

void TestMaterialAppearance::regexFilteringAndRegistration()
{
    SettingsScreen screen;
    screen.resize(1000, 800);
    screen.show();
    auto* search = screen.searchBar();
    QVERIFY(search);
    QCOMPARE(search->searchId(), QStringLiteral("appearance.settings"));
    QCOMPARE(SearchRegistry::instance()->bar(QStringLiteral("appearance.settings")), search);
    emit search->builderRequested();
    QCOMPARE(SearchRegistry::instance()->current(), search);
    QVERIFY(!search->isRegexEnabled());

    search->setRegexFlags(QStringLiteral("i"));
    search->setRegexEnabled(true);
    search->setText(QStringLiteral("^typography"));
    QTest::qWait(1);
    int visible = 0;
    for (auto* card : screen.findChildren<Card*>()) visible += card->isVisible() ? 1 : 0;
    QCOMPARE(visible, 1);

    search->setText(QStringLiteral("["));
    QTest::qWait(1);
    QVERIFY(search->lineEdit()->accessibleDescription().startsWith(QStringLiteral("Invalid regular expression")));
    visible = 0;
    for (auto* card : screen.findChildren<Card*>()) visible += card->isVisible() ? 1 : 0;
    QCOMPARE(visible, 0);
}

void TestMaterialAppearance::elementOverridePersistenceAndReset()
{
    auto* overrides = ElementOverrides::instance();
    overrides->resetAll();
    ElementOverrides::Override value;
    value.height = 61;
    value.radius = 17;
    value.fontSize = 15;
    value.spacing = 9;
    value.background = QColor(1, 2, 3, 4);
    overrides->set(QStringLiteral("appearance/preview"), value);
    QVERIFY(config()->get(Config::GUI_ElementOverrides).toString().contains(QStringLiteral("appearance/preview")));

    overrides->load();
    const auto loaded = overrides->get(QStringLiteral("appearance/preview"));
    QCOMPARE(loaded.height.value(), 61);
    QCOMPARE(loaded.radius.value(), 17);
    QCOMPARE(loaded.background->alpha(), 4);

    SettingsScreen screen;
    screen.show();
    auto* selector = screen.findChild<QComboBox*>(QStringLiteral("appearanceOverrideElement"));
    auto* height = screen.findChild<QSlider*>(QStringLiteral("appearanceOverrideHeight"));
    auto* reset = screen.findChild<QPushButton*>(QStringLiteral("appearanceOverrideReset"));
    QVERIFY(selector && height && reset);
    selector->setCurrentIndex(selector->findData(QStringLiteral("appearance/preview")));
    height->setValue(72);
    QCOMPARE(overrides->get(QStringLiteral("appearance/preview")).height.value(), 72);
    reset->click();
    QVERIFY(overrides->get(QStringLiteral("appearance/preview")).isEmpty());
    QVERIFY(!overrides->customisedKeys().contains(QStringLiteral("appearance/preview")));
}

QTEST_MAIN(TestMaterialAppearance)
