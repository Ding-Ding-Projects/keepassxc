#include "TestMaterialTabs.h"

#include "gui/material/MaterialTabDescriptor.h"
#include "gui/material/MaterialTabStrip.h"
#include "gui/material/MaterialTabOverflow.h"
#include "gui/material/MaterialSearchRegistry.h"
#include "gui/material/MaterialSearchBar.h"

#include <QAbstractButton>
#include "core/Config.h"

#include <QApplication>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUuid>
#include <QMouseEvent>
#include <QSignalSpy>
#include <QTest>

using namespace Material;

void TestMaterialTabs::persistenceIdentity()
{
    const QString first = tabPersistenceKeyForPath(QStringLiteral("C:\\Vaults\\Example.kdbx"));
    const QString same = tabPersistenceKeyForPath(QStringLiteral("c:/vaults/example.kdbx"));
    const QString other = tabPersistenceKeyForPath(QStringLiteral("C:/vaults/other.kdbx"));
    QVERIFY(first.startsWith(QStringLiteral("file:")));
    QCOMPARE(first, same);
    QVERIFY(first != other);
    QVERIFY(!first.contains(QStringLiteral("vaults"), Qt::CaseInsensitive));
    QVERIFY(tabPersistenceKeyForPath(QString()).isEmpty());
}

void TestMaterialTabs::atomicReconciliation()
{
    TabStrip strip;
    const QList<TabDescriptor> initial{
        {QStringLiteral("runtime-a"), QStringLiteral("file:a"), QStringLiteral("database"), QStringLiteral("A"), true, true},
        {QStringLiteral("runtime-b"), QStringLiteral("file:b"), QStringLiteral("database"), QStringLiteral("B"), false, true},
        {QStringLiteral("runtime-c"), QString(), QStringLiteral("database"), QStringLiteral("Unsaved"), false, false},
    };
    strip.setTabs(initial, QStringLiteral("runtime-b"));
    QCOMPARE(strip.count(), 3);
    QCOMPARE(strip.currentTab(), QStringLiteral("runtime-b"));
    QCOMPARE(strip.tabs().at(0).pinned, true);
    QCOMPARE(strip.tabs().at(2).persistable, false);

    QList<TabDescriptor> refreshed = initial;
    refreshed[1].label = QStringLiteral("Renamed B");
    strip.setTabs(refreshed, QStringLiteral("runtime-b"));
    QCOMPARE(strip.currentTab(), QStringLiteral("runtime-b"));
    QCOMPARE(strip.tabs().at(1).label, QStringLiteral("Renamed B"));
}

void TestMaterialTabs::searchableOverflow()
{
    QWidget host;
    host.resize(900, 700);
    host.show();
    TabOverflow overflow(&host);
    const QList<TabDescriptor> tabs{
        {QStringLiteral("runtime-a"), QStringLiteral("file:a"), QStringLiteral("database"), QStringLiteral("Alpha"), true, true},
        {QStringLiteral("runtime-b"), QStringLiteral("file:b"), QStringLiteral("database"), QStringLiteral("Beta"), false, true},
    };
    overflow.setTabs(tabs, QStringLiteral("runtime-a"), {QStringLiteral("runtime-b")});
    overflow.openOverlay();
    QApplication::processEvents();
    QVERIFY(SearchRegistry::instance()->bar(QStringLiteral("tabs.open")));
    auto* search = SearchRegistry::instance()->bar(QStringLiteral("tabs.open"));
    QCOMPARE(search->guidanceKey(), QStringLiteral("search.guidance.tabs"));
    search->setText(QStringLiteral("Beta"));
    auto* guide = overflow.findChild<QAbstractButton*>(QStringLiteral("searchGuidanceEntry"));
    auto* done = overflow.findChild<QAbstractButton*>(QStringLiteral("searchGuidanceDone"));
    QVERIFY(guide && done);
    guide->click();
    done->click();
    QCOMPARE(search->text(), QStringLiteral("Beta"));
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    const auto results = overflow.findChildren<QWidget*>(QStringLiteral("materialTabResult"));
    QCOMPARE(results.size(), 1);
    QCOMPARE(results.first()->property("runtimeId").toString(), QStringLiteral("runtime-b"));
    QString activated;
    connect(&overflow, &TabOverflow::tabActivated, this, [&](const QString& id) { activated = id; });
    QAbstractButton* beta = nullptr;
    for (auto* button : overflow.findChildren<QAbstractButton*>()) {
        if (button->text().contains(QStringLiteral("Beta"))) { beta = button; break; }
    }
    QVERIFY(beta);
    beta->click();
    QCOMPARE(activated, QStringLiteral("runtime-b"));
}

void TestMaterialTabs::rejectedPatternsRetainRowsAndLifecycleResetClearsSearch()
{
    QWidget host;
    host.resize(900, 700);
    host.show();
    TabOverflow overflow(&host);
    overflow.setTabs({
        {QStringLiteral("runtime-a"), {}, QStringLiteral("database"), QStringLiteral("Alpha"), false, false},
        {QStringLiteral("runtime-b"), {}, QStringLiteral("database"), QStringLiteral("Beta"), false, false}
    }, QStringLiteral("runtime-a"), {});
    overflow.openOverlay();
    auto* search = SearchRegistry::instance()->bar(QStringLiteral("tabs.open"));
    QVERIFY(search);
    search->setText(QStringLiteral("Beta"));
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCOMPARE(overflow.findChildren<QWidget*>(QStringLiteral("materialTabResult")).size(), 1);
    search->setRegexEnabled(true);
    search->setText(QStringLiteral("(a+)+"));
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    auto rows = overflow.findChildren<QWidget*>(QStringLiteral("materialTabResult"));
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.first()->property("runtimeId").toString(), QStringLiteral("runtime-b"));
    search->setText(QStringLiteral("("));
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCOMPARE(overflow.findChildren<QWidget*>(QStringLiteral("materialTabResult")).size(), 1);
    overflow.clearSearch();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QVERIFY(search->text().isEmpty());
    QVERIFY(!search->isRegexEnabled());
    QCOMPARE(search->regexFlags(), QStringLiteral("i"));
    QVERIFY(overflow.findChildren<QWidget*>(QStringLiteral("materialTabResult")).isEmpty());
}

void TestMaterialTabs::pointerDragRequestsReorder()
{
    TabStrip strip;
    strip.resize(900, 48);
    strip.setTabs({
                      {QStringLiteral("runtime-a"), QStringLiteral("file:a"), QStringLiteral("database"), QStringLiteral("Alpha"), false, true},
                      {QStringLiteral("runtime-b"), QStringLiteral("file:b"), QStringLiteral("database"), QStringLiteral("Beta"), false, true},
                      {QStringLiteral("runtime-c"), QStringLiteral("file:c"), QStringLiteral("database"), QStringLiteral("Gamma"), false, true},
                  },
                  QStringLiteral("runtime-a"));
    strip.show();
    QVERIFY(QTest::qWaitForWindowExposed(&strip));

    QSignalSpy moves(&strip, &TabStrip::tabMoveRequested);
    QVERIFY(moves.isValid());

    const QPoint source(36, strip.height() - 18);
    const QPoint beforeThird(200, strip.height() - 18);
    QTest::mousePress(&strip, Qt::LeftButton, Qt::NoModifier, source);
    QMouseEvent drag(QEvent::MouseMove,
                     QPointF(beforeThird),
                     QPointF(strip.mapToGlobal(beforeThird)),
                     Qt::NoButton,
                     Qt::LeftButton,
                     Qt::NoModifier);
    QCoreApplication::sendEvent(&strip, &drag);
    QTest::mouseRelease(&strip, Qt::LeftButton, Qt::NoModifier, beforeThird);

    QCOMPARE(moves.count(), 1);
    const QList<QVariant> request = moves.takeFirst();
    QCOMPARE(request.at(0).toString(), QStringLiteral("runtime-a"));
    QCOMPARE(request.at(1).toString(), QStringLiteral("runtime-c"));
}

int main(int argc, char** argv)
{
    QStandardPaths::setTestModeEnabled(true);
    const QString identity = QStringLiteral("TestMaterialTabs-%1").arg(QUuid::createUuid().toString(QUuid::Id128));
    QCoreApplication::setOrganizationName(identity);
    QCoreApplication::setApplicationName(identity);
    qputenv("USERNAME", identity.toLatin1());
    qputenv("USER", identity.toLatin1());
    QTemporaryDir profile;
    if (!profile.isValid()) return 1;
    QApplication application(argc, argv);
    Config::createConfigFromFile(profile.filePath(QStringLiteral("roaming.ini")),
                                 profile.filePath(QStringLiteral("local.ini")));
    TestMaterialTabs tests;
    return QTest::qExec(&tests, argc, argv);
}
