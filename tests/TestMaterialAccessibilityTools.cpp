/* Copyright (C) 2026 KeePassXC Team <team@keepassxc.org>
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "core/Config.h"
#include "gui/material/MaterialAccessibilityTools.h"
#include "gui/material/MaterialOverlay.h"
#include "gui/material/MaterialTheme.h"
#include "gui/material/MaterialVoice.h"
#include "gui/entry/EditEntryWidget.h"
#include "gui/EditWidgetIcons.h"
#include "crypto/Crypto.h"
#include "util/TemporaryFile.h"
#include <QApplication>
#include <QAction>
#include <QCheckBox>
#include <QDialog>
#include <QLabel>
#include <QMessageBox>
#include <QMenu>
#include <QPushButton>
#include <QSettings>
#include <QSlider>
#include <QTest>
#include <QVBoxLayout>

class TestMaterialAccessibilityTools : public QObject
{
    Q_OBJECT
    QString m_local;
private slots:
    void initTestCase()
    {
        m_local = TemporaryFile::createTempConfigFile();
        Config::createConfigFromFile(TemporaryFile::createTempConfigFile(), m_local);
        QVERIFY(Crypto::init());
        Material::AccessibilityTools::install(qApp);
    }
    void init()
    {
        Material::Voice::setLanguage(Material::Voice::Language::English);
        Material::AccessibilityTools::reset();
    }
    void rebuiltMenusKeepExactlyOneLiveEntry()
    {
        QMenu menu;
        menu.addAction(QStringLiteral("Original command"));
        menu.show();
        QPointer<QAction> first = menu.findChild<QAction*>(QStringLiteral("accessibilityMenuAction"));
        QVERIFY(first);
        menu.hide();
        menu.clear();
        QVERIFY(first.isNull());
        menu.addAction(QStringLiteral("Rebuilt command"));
        menu.show();
        auto* current = menu.findChild<QAction*>(QStringLiteral("accessibilityMenuAction"));
        QVERIFY(current);
        QCOMPARE(menu.findChildren<QAction*>(QStringLiteral("accessibilityMenuAction")).size(), 1);
        config()->set(Config::GUI_AccessibilityButtonVisible, false);
        QVERIFY(!current->isVisible());
        config()->set(Config::GUI_AccessibilityButtonVisible, true);
        QVERIFY(current->isVisible());
        menu.hide();
        menu.show();
        QCOMPARE(menu.findChildren<QAction*>(QStringLiteral("accessibilityMenuAction")).size(), 1);
    }
    void accessibilityActionsDoNotChangeEntrySettings()
    {
        EditEntryWidget editor;
        auto* preset = editor.findChild<QPushButton*>(QStringLiteral("expirePresets"));
        auto* expiry = editor.findChild<QCheckBox*>(QStringLiteral("expireCheck"));
        QVERIFY(preset && preset->menu() && expiry);
        expiry->setChecked(false);
        preset->menu()->show();
        auto* accessibility = preset->menu()->findChild<QAction*>(QStringLiteral("accessibilityMenuAction"));
        QVERIFY(accessibility);
        accessibility->trigger();
        QVERIFY(!expiry->isChecked());

        EditWidgetIcons icons;
        auto* apply = icons.findChild<QPushButton*>(QStringLiteral("applyIconToPushButton"));
        QVERIFY(apply && apply->menu());
        const auto previous = apply->text();
        apply->menu()->show();
        accessibility = apply->menu()->findChild<QAction*>(QStringLiteral("accessibilityMenuAction"));
        QVERIFY(accessibility);
        accessibility->trigger();
        QCOMPARE(apply->text(), previous);
    }
    void languagesUpdateExistingControlsAndGeometry()
    {
        QWidget shell;
        shell.resize(640, 480);
        shell.show();
        auto* entry = shell.findChild<QPushButton*>(QStringLiteral("accessibilityToolsButton"));
        QVERIFY(entry);
        Material::AccessibilityTools::open(&shell);
        auto* reset = shell.findChild<QPushButton*>(QStringLiteral("accessibilityReset"));
        QVERIFY(reset);
        for (auto language : {Material::Voice::Language::English, Material::Voice::Language::Cantonese,
                              Material::Voice::Language::Bilingual}) {
            Material::Voice::setLanguage(language);
            QCOMPARE(entry->text(), Material::Voice::say(QStringLiteral("accessibility.entry")));
            QCOMPARE(reset->text(), Material::Voice::say(QStringLiteral("accessibility.reset")));
            QVERIFY(!entry->text().contains(QStringLiteral("accessibility.entry")));
            QCoreApplication::processEvents();
            auto* footer = shell.findChild<QWidget*>(QStringLiteral("accessibilityFooter"));
            QVERIFY(footer->height() >= footer->minimumSizeHint().height());
            QVERIFY(entry->height() >= entry->minimumSizeHint().height());
        }
    }
    void surfaceEntryAndReservedSpace()
    {
        QWidget shell;
        shell.resize(640, 480);
        shell.setMinimumSize(320, 240);
        auto* layout = new QVBoxLayout(&shell);
        auto* content = new QPushButton(QStringLiteral("Content"), &shell);
        layout->addWidget(content);
        shell.show();
        QCoreApplication::processEvents();
        auto* footer = shell.findChild<QWidget*>(QStringLiteral("accessibilityFooter"));
        QVERIFY(footer);
        QVERIFY(shell.findChild<QPushButton*>(QStringLiteral("accessibilityToolsButton"))->isVisible());
        QVERIFY(content->geometry().bottom() < footer->geometry().top());
        QVERIFY(shell.minimumHeight() >= 296);
        shell.resize(380, 310);
        QCoreApplication::processEvents();
        QVERIFY(content->geometry().bottom() < footer->geometry().top());
        Material::Overlay overlay(&shell);
        overlay.setSheetWidget(new QLabel(QStringLiteral("Modal content")));
        overlay.openOverlay();
        QVERIFY(overlay.findChild<QPushButton*>(QStringLiteral("accessibilityToolsButton"))->isVisible());
        QVERIFY(overlay.sheetWidget()->geometry().bottom() < overlay.height() - 56);
    }
    void cancelHidePreservesPreferencesAndRestoreRemainsReachable()
    {
        QWidget shell;
        shell.show();
        Material::AccessibilityTools::open(&shell);
        auto* panel = shell.findChild<QDialog*>(QStringLiteral("accessibilityTools"));
        QVERIFY(panel);
        const QStringList required = {QStringLiteral("accessibilityTextSize"), QStringLiteral("accessibilityHighContrast"),
            QStringLiteral("accessibilityReducedMotion"), QStringLiteral("accessibilityStrongFocus"),
            QStringLiteral("accessibilityLargeTargets"), QStringLiteral("accessibilitySpeechUnavailable"),
            QStringLiteral("accessibilityReset"), QStringLiteral("accessibilityHide")};
        for (const auto& name : required) {
            QVERIFY2(panel->findChild<QWidget*>(name), qPrintable(name));
        }
        panel->findChild<QSlider*>(QStringLiteral("accessibilityTextSize"))->setValue(125);
        panel->findChild<QCheckBox*>(QStringLiteral("accessibilityReducedMotion"))->setChecked(true);
        panel->findChild<QPushButton*>(QStringLiteral("accessibilityHide"))->click();
        auto* confirmation = panel->findChild<QMessageBox*>(QStringLiteral("accessibilityHideConfirmation"));
        QVERIFY(confirmation);
        QCOMPARE(confirmation->defaultButton(), confirmation->button(QMessageBox::Cancel));
        QVERIFY(config()->get(Config::GUI_AccessibilityButtonVisible).toBool());
        confirmation->button(QMessageBox::Cancel)->click();
        QVERIFY(config()->get(Config::GUI_AccessibilityButtonVisible).toBool());
        panel->findChild<QPushButton*>(QStringLiteral("accessibilityHide"))->click();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        confirmation = panel->findChild<QMessageBox*>(QStringLiteral("accessibilityHideConfirmation"));
        QVERIFY(confirmation);
        confirmation->button(QMessageBox::Yes)->click();
        QVERIFY(!config()->get(Config::GUI_AccessibilityButtonVisible).toBool());
        QCOMPARE(config()->get(Config::GUI_FontScale).toDouble(), 1.25);
        QVERIFY(config()->get(Config::GUI_ReducedMotion).toBool());
        config()->sync();
        QSettings disk(m_local, QSettings::IniFormat);
        QCOMPARE(disk.value(QStringLiteral("GUI/AccessibilityButtonVisible")).toBool(), false);
        auto* settings = shell.findChild<QPushButton*>(QStringLiteral("accessibilitySettingsButton"));
        QVERIFY(settings->isVisible());
        settings->click();
        auto* restore = shell.findChild<QPushButton*>(QStringLiteral("showAccessibilityButton"));
        QVERIFY(restore);
        restore->click();
        QVERIFY(config()->get(Config::GUI_AccessibilityButtonVisible).toBool());
        QCOMPARE(config()->get(Config::GUI_FontScale).toDouble(), 1.25);
    }
    void ordinaryCloseDoesNotHideAndResetRestoresVisibility()
    {
        QWidget shell;
        shell.show();
        Material::AccessibilityTools::open(&shell);
        auto* panel = shell.findChild<QDialog*>(QStringLiteral("accessibilityTools"));
        QTest::keyClick(panel, Qt::Key_Escape);
        QVERIFY(config()->get(Config::GUI_AccessibilityButtonVisible).toBool());
        config()->set(Config::GUI_AccessibilityButtonVisible, false);
        Material::AccessibilityTools::reset();
        QVERIFY(shell.findChild<QPushButton*>(QStringLiteral("accessibilityToolsButton"))->isVisible());
    }
    void contrastAndTargetsHaveRealConsumers()
    {
        config()->set(Config::GUI_AccessibilityHighContrast, true);
        const auto background = theme()->color(Material::Role::Surface);
        const auto foreground = theme()->color(Material::Role::OnSurface);
        QVERIFY((background == Qt::black && foreground == Qt::white)
                || (background == Qt::white && foreground == Qt::black));
        config()->set(Config::GUI_AccessibilityLargeTargets, true);
        QCOMPARE(theme()->rowHeight(), 64);
    }
};
QTEST_MAIN(TestMaterialAccessibilityTools)
#include "TestMaterialAccessibilityTools.moc"
