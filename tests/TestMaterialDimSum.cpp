#include "core/Config.h"
#include "core/Database.h"
#include "crypto/Crypto.h"
#include "crypto/kdf/AesKdf.h"
#include "gui/DatabaseWidget.h"
#include "gui/DatabaseTabWidget.h"
#include "gui/MainWindow.h"
#include "gui/MessageWidget.h"
#include "keys/CompositeKey.h"
#include "keys/PasswordKey.h"
#include "gui/material/MaterialDimSum.h"
#include "gui/material/MaterialNotifier.h"
#include "gui/material/MaterialSettingsScreen.h"
#include "gui/material/MaterialVoice.h"
#include <QAbstractButton>

#include <QApplication>
#include <QDir>
#include <QLineEdit>
#include <QMenu>
#include <QRandomGenerator>
#include <QSettings>
#include <QStandardPaths>
#include <QSvgRenderer>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

using Material::DimSum;
using Material::DimSumCard;

class TestMaterialDimSum : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        QVERIFY(Crypto::init());
    }
    void init()
    {
        DimSum::resetLaunchState();
        config()->set(Config::GUI_DimSumSurprise, true);
        config()->set(Config::GUI_MinimizeOnStartup, false);
        config()->set(Config::OpenPreviousDatabasesOnStartup, true);
        config()->set(Config::LastOpenedDatabases, QStringList());
        config()->set(Config::LastDatabases, QStringList{QStringLiteral("synthetic-history-only.kdbx")});
        config()->set(Config::LastActiveDatabase, QString());
        config()->sync();
        DimSum::s_quiet = [] { return false; };
        DimSum::s_random = [](quint32) { return 0; };
    }
    void cleanup()
    {
        DimSum::s_random = [](quint32 bound) { return QRandomGenerator::system()->bounded(bound); };
        DimSum::resetLaunchState();
        if (m_window) QVERIFY(m_window->findChild<DatabaseTabWidget*>()->closeAllDatabaseTabs());
    }
    void cleanupTestCase()
    {
        m_window.reset();
    }
    void drawBoundary_data()
    {
        QTest::addColumn<quint32>("value");
        for (quint32 value = 0; value < 100; ++value)
            QTest::newRow(qPrintable(QString::number(value))) << value;
    }
    void drawBoundary()
    {
        QFETCH(quint32, value);
        quint32 seenBound = 0;
        int calls = 0;
        DimSum::s_random = [&](quint32 bound) { seenBound = bound; ++calls; return value; };
        QCOMPARE(DimSum::shouldShow(), value == 0);
        QCOMPARE(seenBound, quint32(100));
        for (int count = 0; count < 20; ++count) QCOMPARE(DimSum::shouldShow(), value == 0);
        QCOMPARE(calls, 1);
    }
    void disabledIsPersistedAndReturnsBeforeEnvironment()
    {
        config()->set(Config::GUI_DimSumSurprise, false);
        config()->sync();
        QSettings stored(config()->getFileName(), QSettings::IniFormat);
        QCOMPARE(stored.value(QStringLiteral("GUI/DimSumSurprise")).toBool(), false);
        Config::createConfigFromFile(qEnvironmentVariable("KPXC_CONFIG"), qEnvironmentVariable("KPXC_CONFIG_LOCAL"));
        QVERIFY(!config()->get(Config::GUI_DimSumSurprise).toBool());
        int environmentCalls = 0;
        DimSum::s_quiet = [&] { ++environmentCalls; return false; };
        QVERIFY(!DimSum::shouldShow());
        QVERIFY(!DimSum::showNow(nullptr));
        QCOMPARE(environmentCalls, 0);
    }
    void restoredLaunch_data()
    {
        QTest::addColumn<bool>("restore");
        QTest::addColumn<QStringList>("opened");
        QTest::addColumn<QString>("active");
        QTest::addColumn<bool>("eligible");
        QTest::newRow("remembered-tab") << true << QStringList{"synthetic.kdbx"} << QString() << false;
        QTest::newRow("last-active-only") << true << QStringList{} << QString("synthetic.kdbx") << false;
        QTest::newRow("empty-records") << true << QStringList{QString()} << QString() << true;
        QTest::newRow("restoration-disabled") << false << QStringList{"synthetic.kdbx"}
                                               << QString("synthetic.kdbx") << true;
    }
    void restoredLaunch()
    {
        QFETCH(bool, restore);
        QFETCH(QStringList, opened);
        QFETCH(QString, active);
        QFETCH(bool, eligible);
        config()->set(Config::OpenPreviousDatabasesOnStartup, restore);
        config()->set(Config::LastOpenedDatabases, opened);
        config()->set(Config::LastActiveDatabase, active);
        int draws = 0;
        DimSum::s_random = [&](quint32) { ++draws; return 0; };
        DimSum::beginStartup();
        QCOMPARE(DimSum::shouldShow(), eligible);
        QCOMPARE(draws, eligible ? 1 : 0);
        // Finishing or cancelling restoration must not make a later task eligible.
        config()->set(Config::LastOpenedDatabases, QStringList());
        config()->set(Config::LastActiveDatabase, QString());
        QCOMPARE(DimSum::shouldShow(), eligible);
    }
    void visibleMessageSeverityChange_data()
    {
        QTest::addColumn<int>("severity");
        QTest::addColumn<bool>("excluded");
        QTest::newRow("error") << int(MessageWidget::Error) << true;
        QTest::newRow("warning") << int(MessageWidget::Warning) << true;
        QTest::newRow("information") << int(MessageWidget::Information) << false;
        QTest::newRow("positive") << int(MessageWidget::Positive) << false;
    }
    void visibleMessageSeverityChange()
    {
        QFETCH(int, severity);
        QFETCH(bool, excluded);
        QWidget host;
        MessageWidget message(&host);
        message.setAnimate(false);
        message.showMessage(QStringLiteral("Synthetic information"), MessageWidget::Information, -1);
        host.resize(640, 480);
        host.show();
        host.activateWindow();
        QTRY_VERIFY(host.isActiveWindow());
        QVERIFY(message.isVisible());
        QVERIFY(DimSum::showNow(&host));
        QPointer<DimSumCard> card = host.findChild<DimSumCard*>();
        QVERIFY(card && card->isVisible());
        message.showMessage(QStringLiteral("Synthetic replacement"),
                            static_cast<MessageWidget::MessageType>(severity), -1);
        QVERIFY(message.isVisible());
        QCOMPARE(card && card->isVisible(), !excluded);
        if (excluded) {
            QTRY_VERIFY(card.isNull());
            QVERIFY(!DimSum::shouldShow());
        }
    }
    void preferenceOverridesWinningDraw()
    {
        QVERIFY(DimSum::shouldShow());
        config()->set(Config::GUI_DimSumSurprise, false);
        QVERIFY(!DimSum::shouldShow());
    }

    void firstRunDoesNotDraw()
    {
        config()->set(Config::LastDatabases, QStringList());
        int calls = 0;
        DimSum::s_random = [&](quint32) { ++calls; return 0; };
        QVERIFY(!DimSum::shouldShow());
        QCOMPARE(calls, 0);
    }
    void quietDoesNotDraw()
    {
        DimSum::s_quiet = [] { return true; };
        int calls = 0;
        DimSum::s_random = [&](quint32) { ++calls; return 0; };
        QVERIFY(!DimSum::shouldShow());
        QCOMPARE(calls, 0);
    }
    void firstRunCannotBecomeEligibleMidLaunch()
    {
        config()->set(Config::LastDatabases, QStringList());
        DimSum::beginStartup();
        config()->set(Config::LastDatabases, QStringList{QStringLiteral("synthetic-history-only.kdbx")});
        QVERIFY(!DimSum::shouldShow());
    }
    void settingsControlIsLocalizedAndPersists()
    {
        Material::SettingsScreen settings;
        auto* toggle = settings.findChild<QAbstractButton*>(QStringLiteral("dimSumSurpriseToggle"));
        QVERIFY(toggle);
        QVERIFY(toggle->focusPolicy() != Qt::NoFocus);
        QVERIFY(toggle->isChecked());
        toggle->click();
        QVERIFY(!config()->get(Config::GUI_DimSumSurprise).toBool());
        config()->sync();
        QSettings stored(config()->getFileName(), QSettings::IniFormat);
        QVERIFY(!stored.value(QStringLiteral("GUI/DimSumSurprise"), true).toBool());
        for (auto language : {Material::Voice::Language::English, Material::Voice::Language::Cantonese,
                              Material::Voice::Language::Bilingual}) {
            Material::Voice::setLanguage(language);
            QCOMPARE(toggle->accessibleName(), Material::Voice::say(QStringLiteral("dim-sum.setting")));
            QVERIFY(toggle->accessibleDescription().contains(QStringLiteral("1%")));
            QVERIFY(!toggle->accessibleName().contains(QStringLiteral("dim-sum.setting")));
        }
        config()->set(Config::GUI_DimSumSurprise, true);
        QVERIFY(toggle->isChecked());
    }
    void disablingVisibleCardIsImmediate()
    {
        QWidget host;
        host.show();
        host.activateWindow();
        QTRY_VERIFY(host.isActiveWindow());
        QVERIFY(DimSum::showNow(&host));
        QPointer<DimSumCard> card = host.findChild<DimSumCard*>();
        QVERIFY(card && card->isVisible());
        config()->set(Config::GUI_DimSumSurprise, false);
        QVERIFY(!card || !card->isVisible());
        QTRY_VERIFY(card.isNull());
    }
    void quietAtPresentationCancelsWithoutRetry()
    {
        QWidget host;
        host.show();
        host.activateWindow();
        QTRY_VERIFY(host.isActiveWindow());
        DimSum::showIfDue(&host);
        DimSum::s_quiet = [] { return true; };
        QTest::qWait(1600);
        QVERIFY(!DimSum::hasShown());
        DimSum::s_quiet = [] { return false; };
        DimSum::showIfDue(&host);
        QTest::qWait(1600);
        QVERIFY(!DimSum::hasShown());
    }
    void suppressionOverridesWinningDraw()
    {
        QVERIFY(DimSum::shouldShow());
        DimSum::suppress();
        QVERIFY(!DimSum::shouldShow());
    }
    void errorButNotInformationSuppresses()
    {
        Material::Notify::info(QStringLiteral("Synthetic information"));
        QVERIFY(DimSum::shouldShow());
        Material::Notify::error(QStringLiteral("Synthetic error"));
        QVERIFY(!DimSum::shouldShow());
    }
    void interactionCancelsPendingPresentation()
    {
        QWidget host;
        QLineEdit edit(&host);
        host.resize(640, 480);
        host.show();
        host.activateWindow();
        QTRY_VERIFY(host.isActiveWindow());
        edit.setFocus();
        DimSum::showIfDue(&host);
        QTest::keyClick(&edit, Qt::Key_A);
        QTest::qWait(1600);
        QVERIFY(!DimSum::hasShown());
        QVERIFY(host.findChildren<DimSumCard*>().isEmpty());
        QVERIFY(!DimSum::shouldShow());
    }
    void popupPreventsPresentation()
    {
        QWidget host;
        host.show();
        host.activateWindow();
        QTRY_VERIFY(host.isActiveWindow());
        QMenu popup(&host);
        popup.addAction(QStringLiteral("Synthetic action"));
        popup.popup(QPoint(20, 20));
        QVERIFY(QApplication::activePopupWidget());
        QVERIFY(!DimSum::showNow(&host));
    }
    void cardDoesNotStealFocusAndExpiresOnce()
    {
        QWidget host;
        QLineEdit edit(&host);
        host.resize(640, 480);
        host.show();
        host.activateWindow();
        edit.setFocus();
        QTRY_VERIFY(edit.hasFocus());
        QVERIFY(DimSum::showNow(&host));
        QCOMPARE(QApplication::focusWidget(), &edit);
        QVERIFY(!DimSum::showNow(&host));
        QPointer<DimSumCard> card = host.findChild<DimSumCard*>();
        QVERIFY(card);
        QCOMPARE(card->focusPolicy(), Qt::NoFocus);
        auto* hold = card->findChild<QTimer*>();
        QVERIFY(hold);
        QCOMPARE(hold->interval(), 6000);
        QTRY_VERIFY_WITH_TIMEOUT(hold->isActive(), 1000);
        QVERIFY(QMetaObject::invokeMethod(hold, "timeout", Qt::DirectConnection));
        QTRY_VERIFY_WITH_TIMEOUT(card.isNull(), 1000);
        QVERIFY(!DimSum::showNow(&host));
    }
    void pendingHostCanBeDestroyed()
    {
        auto* host = new QWidget;
        host->show();
        host->activateWindow();
        QTRY_VERIFY(host->isActiveWindow());
        DimSum::showIfDue(host);
        delete host;
        QTest::qWait(1600);
        QVERIFY(!DimSum::hasShown());
    }
    void assetsAreLocalAndNamed()
    {
        QVERIFY(!DimSum::catalogue().isEmpty());
        for (const auto& dish : DimSum::catalogue()) {
            QVERIFY(dish.asset.startsWith(QStringLiteral(":/dimsum/")));
            QVERIFY(QSvgRenderer(dish.asset).isValid());
            QVERIFY(dish.displayName().contains(dish.english));
            QVERIFY(dish.displayName().contains(dish.cantonese));
        }
    }
    void programmaticOpenCancelsPendingCard()
    {
        programmaticOpen(false);
    }
    void programmaticOpenHidesVisibleCard()
    {
        programmaticOpen(true);
    }
private:
    QScopedPointer<MainWindow> m_window;
    void programmaticOpen(bool alreadyVisible)
    {
        QTemporaryDir databaseDirectory(QDir::tempPath() + QStringLiteral("/kds-db-XXXXXX"));
        QVERIFY(databaseDirectory.isValid());
        const auto path = databaseDirectory.filePath(QStringLiteral("synthetic-locked.kdbx"));
        {
            Database fixture;
            auto kdf = QSharedPointer<AesKdf>::create();
            kdf->setRounds(1);
            fixture.setKdf(kdf);
            auto key = QSharedPointer<CompositeKey>::create();
            key->addKey(QSharedPointer<PasswordKey>::create(QStringLiteral("synthetic-test-only")));
            QVERIFY(fixture.setKey(key));
            QString error;
            QVERIFY2(fixture.saveAs(path, Database::DirectWrite, {}, &error), qPrintable(error));
        }
        config()->set(Config::GUI_CheckForUpdates, false);
        config()->set(Config::Browser_Enabled, false);
        config()->set(Config::SSHAgent_Enabled, false);
        config()->set(Config::GUI_ShowTrayIcon, false);
        config()->set(Config::GUI_AllowScreenCapture, true);
        config()->set(Config::GlobalAutoTypeKey, 0);
        config()->set(Config::GlobalAutoTypeModifiers, 0);
        config()->set(Config::Security_LockDatabaseIdle, false);
        if (!m_window) m_window.reset(new MainWindow);
        auto& window = *m_window;
        window.resize(1024, 768);
        window.show();
        window.activateWindow();
        QTRY_VERIFY(window.isActiveWindow());
        QVERIFY(window.getOpenDatabases().isEmpty());
        QVERIFY(DimSum::shouldShow());
        QPointer<DimSumCard> card;
        if (alreadyVisible) {
            QVERIFY(DimSum::showNow(&window));
            card = window.findChild<DimSumCard*>();
            QVERIFY(card && card->isVisible());
        } else {
            DimSum::showIfDue(&window);
        }
        // This is the same one-argument slot used by Application::openFile.
        // Queue it without keyboard, pointer, modal or single-instance IPC.
        QVERIFY(QMetaObject::invokeMethod(&window, "openDatabase", Qt::QueuedConnection, Q_ARG(QString, path)));
        QTRY_COMPARE(window.getOpenDatabases().size(), 1);
        QVERIFY(window.getOpenDatabases().first()->isLocked());
        QVERIFY(!QApplication::activeModalWidget());
        QVERIFY(!QApplication::activePopupWidget());
        if (alreadyVisible) {
            QVERIFY(!card || !card->isVisible());
            QTRY_VERIFY(card.isNull());
        } else {
            QVERIFY(!DimSum::shouldShow());
            QTest::qWait(1600);
            QVERIFY(!DimSum::hasShown());
            QVERIFY(window.findChildren<DimSumCard*>().isEmpty());
        }
        QVERIFY(!DimSum::shouldShow());
    }
};

int main(int argc, char** argv)
{
    QStandardPaths::setTestModeEnabled(true);
    QTemporaryDir isolated(QDir::tempPath() + QStringLiteral("/kds-XXXXXX"));
    if (!isolated.isValid()) return 2;
    qputenv("KPXC_CONFIG", (isolated.path() + QStringLiteral("/settings.ini")).toUtf8());
    qputenv("KPXC_CONFIG_LOCAL", (isolated.path() + QStringLiteral("/local.ini")).toUtf8());
    qputenv("USERNAME", "dim-sum-test");
    qputenv("USER", "dim-sum-test");
    const auto identity = QStringLiteral("KeePassXC-DimSum-Tests-") + QDir(isolated.path()).dirName();
    QCoreApplication::setOrganizationName(identity);
    QCoreApplication::setApplicationName(identity);
    QApplication application(argc, argv);
    TestMaterialDimSum test;
    return QTest::qExec(&test, argc, argv);
}

#include "TestMaterialDimSum.moc"
