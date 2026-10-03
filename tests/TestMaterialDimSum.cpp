#include "core/Config.h"
#include "gui/material/MaterialDimSum.h"
#include "gui/material/MaterialNotifier.h"

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
    void init()
    {
        DimSum::resetLaunchState();
        config()->set(Config::GUI_DimSumSurprise, true);
        config()->set(Config::GUI_MinimizeOnStartup, false);
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
    QCoreApplication::setOrganizationName(QStringLiteral("KeePassXC-DimSum-Tests"));
    QApplication application(argc, argv);
    TestMaterialDimSum test;
    return QTest::qExec(&test, argc, argv);
}

#include "TestMaterialDimSum.moc"
