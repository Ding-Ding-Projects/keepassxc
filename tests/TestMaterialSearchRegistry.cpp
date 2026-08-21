#include "TestMaterialSearchRegistry.h"

#include "gui/material/MaterialSearchBar.h"
#include "gui/material/MaterialSearchRegistry.h"
#include "gui/material/MaterialCommandPalette.h"
#include "gui/material/MaterialNotificationCentre.h"

#include <QApplication>
#include <QAbstractButton>
#include <QLineEdit>
#include <QTest>

using namespace Material;

void TestMaterialSearchRegistry::registrationAndOwnership()
{
    auto* first = new SearchBar;
    auto* second = new SearchBar;
    QVERIFY(first->setIdentity(QStringLiteral("test.registry.first"), QStringLiteral("First search")));
    QVERIFY(second->setIdentity(QStringLiteral("test.registry.second"), QStringLiteral("Second search")));
    QCOMPARE(SearchRegistry::instance()->bar(QStringLiteral("test.registry.first")), first);
    QCOMPARE(SearchRegistry::instance()->bars().contains(second), true);

    int requests = 0;
    SearchBar* requested = nullptr;
    const auto connection = connect(SearchRegistry::instance(), &SearchRegistry::builderRequested,
                                    this, [&](SearchBar* bar) { ++requests; requested = bar; });
    emit first->builderRequested();
    QCOMPARE(requests, 1);
    QCOMPARE(requested, first);
    QCOMPARE(SearchRegistry::instance()->current(), first);
    QCOMPARE(SearchRegistry::instance()->currentLabel(), QStringLiteral("First search"));

    second->show();
    second->lineEdit()->setFocus();
    QApplication::processEvents();
    QCOMPARE(SearchRegistry::instance()->current(), second);
    disconnect(connection);
    delete second;
    QCOMPARE(SearchRegistry::instance()->current(), nullptr);
    QCOMPARE(SearchRegistry::instance()->bar(QStringLiteral("test.registry.second")), nullptr);
    delete first;
}

void TestMaterialSearchRegistry::duplicateIdentityRejected()
{
    SearchBar first;
    SearchBar second;
    QVERIFY(first.setIdentity(QStringLiteral("test.registry.duplicate"), QStringLiteral("First")));
    QVERIFY(!second.setIdentity(QStringLiteral("test.registry.duplicate"), QStringLiteral("Second")));
    QCOMPARE(SearchRegistry::instance()->bar(QStringLiteral("test.registry.duplicate")), &first);
    QVERIFY(!second.setIdentity(QString(), QStringLiteral("Missing ID")));
    QVERIFY(!second.setIdentity(QStringLiteral("missing-label"), QString()));
}

void TestMaterialSearchRegistry::existingConsumerSurfacesRegister()
{
    QWidget host;
    auto* palette = new CommandPalette(&host);
    auto* centre = new NotificationCentre(&host);
    QVERIFY(SearchRegistry::instance()->bar(QStringLiteral("command-palette.commands")));
    QVERIFY(SearchRegistry::instance()->bar(QStringLiteral("notification-centre.history")));
    int actionCount = 0;
    centre->record(SeverityLevel::Warning,
                   QStringLiteral("Update ready"),
                   QStringLiteral("Restart when convenient"),
                   {{QStringLiteral("Restart to install update"), [&] { ++actionCount; }, &host}});
    centre->openOverlay();
    QApplication::processEvents();
    QAbstractButton* restart = nullptr;
    for (auto* button : centre->findChildren<QAbstractButton*>()) {
        if (button->text() == QStringLiteral("Restart to install update")) {
            restart = button;
            break;
        }
    }
    QVERIFY(restart);
    restart->click();
    QCOMPARE(actionCount, 1);
    delete palette;
    delete centre;
    QCOMPARE(SearchRegistry::instance()->bar(QStringLiteral("command-palette.commands")), nullptr);
    QCOMPARE(SearchRegistry::instance()->bar(QStringLiteral("notification-centre.history")), nullptr);
}

QTEST_MAIN(TestMaterialSearchRegistry)
