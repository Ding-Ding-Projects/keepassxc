/* Copyright (C) 2026 KeePassXC Team <team@keepassxc.org>
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "core/Config.h"
#include "gui/material/MaterialMotion.h"
#include "gui/material/MaterialOverlay.h"
#include "gui/material/MaterialSwitch.h"
#include "gui/material/MaterialControls.h"
#include "gui/material/MaterialAppearanceEditor.h"
#include "gui/material/MaterialSettingsScreen.h"
#include "gui/material/MaterialVoice.h"
#include "gui/material/MaterialStyle.h"
#include "gui/material/MaterialTheme.h"
#include "util/TemporaryFile.h"

#include <QApplication>
#include <QLabel>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QUuid>
#include <limits>

using namespace Material;

class TestMaterialMotion : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        Config::createConfigFromFile(TemporaryFile::createTempConfigFile(), {});
    }
    void init()
    {
        config()->set(Config::GUI_ReducedMotion, false);
        config()->set(Config::GUI_LowStimulation, false);
    }
    void policyComposesEveryVeto()
    {
        for (int bits = 0; bits < 8; ++bits) {
            QCOMPARE(MotionPolicy::effectiveReducedMotion(bits & 1, bits & 2, bits & 4), bits != 0);
        }
        MotionPolicy policy([] { return false; });
        QCOMPARE(policy.duration(-1), 0);
        QCOMPARE(policy.duration(1000), 240);
        config()->set(Config::GUI_ReducedMotion, true);
        QCOMPARE(policy.duration(140), 0);
        config()->set(Config::GUI_ReducedMotion, false);
        config()->set(Config::GUI_LowStimulation, true);
        QVERIFY(policy.reducedMotion());
        config()->set(Config::GUI_LowStimulation, false);
        QVERIFY(!policy.reducedMotion());
    }
    void styleAnimationDurationFollowsSharedPolicy()
    {
        Style style;
        QCOMPARE(style.styleHint(QStyle::SH_Widget_Animation_Duration),
                 MotionPolicy::instance()->duration(Duration::Medium));
        config()->set(Config::GUI_ReducedMotion, true);
        QCOMPARE(style.styleHint(QStyle::SH_Widget_Animation_Duration), 0);
        config()->set(Config::GUI_ReducedMotion, false);
        config()->set(Config::GUI_LowStimulation, true);
        QCOMPARE(style.styleHint(QStyle::SH_Widget_Animation_Duration), 0);
        config()->set(Config::GUI_LowStimulation, false);
        QCOMPARE(style.styleHint(QStyle::SH_Widget_Animation_Duration),
                 MotionPolicy::instance()->duration(Duration::Medium));
    }
    void preferencePersistsWithoutDiscardingBaseChoice()
    {
        config()->set(Config::GUI_ReducedMotion, true);
        config()->set(Config::GUI_LowStimulation, true);
        config()->sync();
        QSettings persisted(config()->getFileName(), QSettings::IniFormat);
        QCOMPARE(persisted.value(QStringLiteral("GUI/ReducedMotion")).toBool(), true);
        QCOMPARE(persisted.value(QStringLiteral("GUI/LowStimulation")).toBool(), true);
        config()->set(Config::GUI_LowStimulation, false);
        MotionPolicy reloaded([] { return false; });
        QVERIFY(reloaded.reducedMotion());
    }
    void reversalStartsAtCurrentValueAndSettlesOnce()
    {
        MotionPolicy policy([] { return false; });
        QWidget owner;
        owner.show();
        MotionTransition transition(&owner, &policy);
        QSignalSpy settled(&transition, &MotionTransition::settled);
        transition.animateTo(1.0, 240);
        QVERIFY(transition.isRunning());
        QTRY_VERIFY(transition.value() > 0.0);
        const qreal current = transition.value();
        transition.animateTo(0.0, 140);
        QCOMPARE(transition.value(), current);
        QTRY_VERIFY(!transition.isRunning());
        QCOMPARE(transition.value(), 0.0);
        QCOMPARE(settled.count(), 1);
        transition.finish();
        QCOMPARE(settled.count(), 1);
    }
    void userVetoSettlesActiveTransitionImmediately()
    {
        MotionPolicy policy([] { return false; });
        QWidget owner;
        owner.show();
        MotionTransition transition(&owner, &policy);
        transition.animateTo(1.0, 240);
        QVERIFY(transition.isRunning());
        config()->set(Config::GUI_ReducedMotion, true);
        QVERIFY(!transition.isRunning());
        QCOMPARE(transition.value(), 1.0);
        transition.animateTo(0.0);
        QCOMPARE(transition.value(), 0.0);
        QVERIFY(!transition.isRunning());
    }
    void systemChangeSettlesWithoutChangingUserPreference()
    {
        bool systemReduced = false;
        MotionPolicy policy([&] { return systemReduced; });
        QWidget owner;
        owner.show();
        MotionTransition transition(&owner, &policy);
        transition.animateTo(1.0, 240);
        systemReduced = true;
        QEvent activation(QEvent::ApplicationActivate);
        QApplication::sendEvent(qApp, &activation);
        QVERIFY(policy.systemReducedMotion());
        QVERIFY(!transition.isRunning());
        QCOMPARE(transition.value(), 1.0);
        QVERIFY(!config()->get(Config::GUI_ReducedMotion).toBool());
        systemReduced = false;
        policy.refresh();
        QVERIFY(!policy.reducedMotion());
    }
    void hiddenAndDisabledOwnersDoNotAnimate()
    {
        MotionPolicy policy([] { return false; });
        QWidget owner;
        MotionTransition transition(&owner, &policy);
        transition.animateTo(1.0);
        QCOMPARE(transition.value(), 1.0);
        QVERIFY(!transition.isRunning());
        owner.show();
        transition.animateTo(0.0, 240);
        QVERIFY(transition.isRunning());
        owner.hide();
        QVERIFY(!transition.isRunning());
        QCOMPARE(transition.value(), 0.0);
        owner.show();
        transition.animateTo(1.0);
        owner.setEnabled(false);
        QVERIFY(!transition.isRunning());
        QCOMPARE(transition.value(), 1.0);
    }
    void deletingOwnerCancelsCallbacks()
    {
        MotionPolicy policy([] { return false; });
        auto* owner = new QWidget;
        owner->show();
        auto* transition = new MotionTransition(owner, &policy);
        QPointer<MotionTransition> pointer(transition);
        transition->animateTo(1.0);
        delete owner;
        QVERIFY(pointer.isNull());
        QApplication::processEvents();
    }
    void rapidTargetsRemainBoundedAndRejectNonFiniteValues()
    {
        MotionPolicy policy([] { return false; });
        QWidget owner;
        owner.show();
        MotionTransition transition(&owner, &policy);
        for (int i = 0; i < 100; ++i) transition.animateTo(i % 2 ? 1.0 : 0.0);
        transition.animateTo(std::numeric_limits<qreal>::quiet_NaN());
        transition.finish();
        QCOMPARE(transition.value(), 1.0);
        QVERIFY(!transition.isRunning());
        QSignalSpy changes(&transition, &MotionTransition::valueChanged);
        QTest::qWait(50);
        QCOMPARE(changes.count(), 0);
    }
    void reducedSwitchAndOverlayHaveImmediateFinalStates()
    {
        config()->set(Config::GUI_ReducedMotion, true);
        QWidget host;
        host.resize(640, 480);
        Switch toggle(&host);
        Overlay overlay(&host);
        auto* sheet = new QLabel(QStringLiteral("Neutral test surface"));
        overlay.setSheetWidget(sheet);
        host.show();
        toggle.setChecked(true);
        QCOMPARE(toggle.knobPosition(), 1.0);
        QSignalSpy closed(&overlay, &Overlay::closed);
        overlay.openOverlay();
        QCOMPARE(overlay.transition(), 1.0);
        QVERIFY(overlay.isOpen());
        QVERIFY(!sheet->graphicsEffect());
        overlay.closeOverlay();
        QVERIFY(sheet->isHidden());
        QVERIFY(overlay.isHidden());
        QCOMPARE(closed.count(), 1);
        overlay.openOverlay();
        host.hide();
        QVERIFY(!overlay.isOpen());
        QCOMPARE(overlay.transition(), 0.0);
        QCOMPARE(closed.count(), 2);
    }
    void reducedIndeterminateProgressHasNoTimer()
    {
        config()->set(Config::GUI_ReducedMotion, true);
        LinearProgress progress;
        progress.setRange(0, 0);
        progress.show();
        QApplication::processEvents();
        const auto timers = progress.findChildren<QTimer*>();
        QVERIFY(!timers.isEmpty());
        for (const auto* timer : timers) QVERIFY(!timer->isActive());
        progress.hide();
        config()->set(Config::GUI_ReducedMotion, false);
        for (const auto* timer : timers) QVERIFY(!timer->isActive());
    }
    void legacyAppearanceBridgeUsesSharedPolicy()
    {
        AppearanceApplier::instance()->setReducedMotion(true);
        QVERIFY(config()->get(Config::GUI_ReducedMotion).toBool());
        QVERIFY(MotionPolicy::instance()->reducedMotion());
        QVERIFY(AppearanceApplier::instance()->reducedMotion());
        config()->set(Config::GUI_LowStimulation, true);
        AppearanceApplier::instance()->setReducedMotion(false);
        QVERIFY(AppearanceApplier::instance()->reducedMotion());
    }
    void preferenceControlsArePresentLocalizedAndPersisted()
    {
        SettingsScreen screen;
        auto* reduced = screen.findChild<Switch*>(QStringLiteral("appearanceReducedMotion"));
        auto* low = screen.findChild<Switch*>(QStringLiteral("appearanceLowStimulation"));
        QVERIFY(reduced);
        QVERIFY(low);
        reduced->setChecked(true);
        QVERIFY(config()->get(Config::GUI_ReducedMotion).toBool());
        low->setChecked(true);
        QVERIFY(config()->get(Config::GUI_LowStimulation).toBool());
        Voice::setLanguage(Voice::Language::Cantonese);
        QVERIFY(reduced->accessibleName().contains(QStringLiteral("減少")));
        Voice::setLanguage(Voice::Language::Bilingual);
        QVERIFY(reduced->accessibleName().contains(QStringLiteral("Reduce motion")));
        QVERIFY(reduced->accessibleName().contains(QStringLiteral("減少")));
        Voice::setLanguage(Voice::Language::English);
    }
    void motionCatalogueHasAllLanguagesAndLevels()
    {
        const QStringList keys = {QStringLiteral("motion.reduced.label"), QStringLiteral("motion.reduced.description"),
            QStringLiteral("motion.low.label"), QStringLiteral("motion.low.description"),
            QStringLiteral("motion.status.system"), QStringLiteral("motion.status.reduced"), QStringLiteral("motion.status.standard")};
        for (const auto& key : keys) {
            QVERIFY(Voice::catalogueKeys().contains(key));
            for (auto language : {Voice::Language::English, Voice::Language::Cantonese, Voice::Language::Bilingual}) {
                for (int level = 1; level <= 5; ++level) {
                    const auto text = Voice::preview(language, level, level, key).joined();
                    QVERIFY(!text.isEmpty());
                    QVERIFY(text != key);
                }
            }
        }
        QCOMPARE(Voice::preview(Voice::Language::English, 1, 1, QStringLiteral("motion.unknown")).joined(),
                 QStringLiteral("motion.unknown"));
    }
};

int main(int argc, char** argv)
{
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName(QStringLiteral("KeePassXC-Motion-Tests"));
    QCoreApplication::setApplicationName(QStringLiteral("Motion-%1").arg(QUuid::createUuid().toString(QUuid::Id128)));
    const QByteArray testUser = QByteArray("kpxcmotion-") + QUuid::createUuid().toString(QUuid::Id128).toLatin1();
    qputenv("USERNAME", testUser);
    qputenv("USER", testUser);
    QApplication application(argc, argv);
    TestMaterialMotion tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "TestMaterialMotion.moc"
