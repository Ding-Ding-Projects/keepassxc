#include "TestBrowserIntegration.h"

#include "browser/BrowserMessageBuilder.h"
#include "browser/BrowserService.h"
#include "browser/BrowserSettings.h"
#include "core/Config.h"
#include "core/CustomData.h"
#include "core/Group.h"
#include "core/Metadata.h"
#include "core/RuntimeProfile.h"
#include "crypto/Crypto.h"
#include "gui/Application.h"
#include "gui/DatabaseWidget.h"
#include "gui/MainWindow.h"
#include "keys/CompositeKey.h"
#include "keys/PasswordKey.h"

#include <QApplication>
#include <QJsonArray>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

namespace
{
    const QString fixtureId = QStringLiteral("synthetic-browser");
    const QString fixtureIdentity = QStringLiteral("synthetic-identification-key");

    QJsonObject customDataSnapshot(const CustomData* data)
    {
        QJsonObject snapshot;
        for (const auto& key : data->keys()) {
            snapshot.insert(key, QJsonObject{{"value", data->value(key)},
                                             {"modified", data->lastModified(key).toString(Qt::ISODateWithMs)}});
        }
        return snapshot;
    }
}

int main(int argc, char** argv)
{
    // Initialize isolation before QApplication, configuration, history, or browser IPC exists.
    QTemporaryDir temporary;
    if (!temporary.isValid()) return 2;
    QString error;
    if (!RuntimeProfile::initialize({QString::fromLocal8Bit(argv[0]), QStringLiteral("--verification-profile"),
                                     temporary.path() + QStringLiteral("/profile")}, &error)) return 3;
    Application application(argc, argv);
    application.setApplicationName(QStringLiteral("KeePassXC"));
    application.setQuitOnLastWindowClosed(false);
    TestBrowserIntegration test;
    return QTest::qExec(&test, argc, argv);
}

void TestBrowserIntegration::initTestCase()
{
    QVERIFY(Crypto::init());
    QVERIFY(RuntimeProfile::isActive());
    config()->set(Config::AutoSaveAfterEveryChange, false);
    config()->set(Config::AutoSaveOnExit, false);
    config()->set(Config::GUI_ShowTrayIcon, false);
    config()->set(Config::UpdateCheckMessageShown, true);
    config()->set(Config::Security_QuickUnlock, false);
    browserSettings()->setEnabled(false);
    browserSettings()->setSearchInAllDatabases(false);
    browserSettings()->setBestMatchOnly(false);
    browserSettings()->setAllowExpiredCredentials(false);
    browserSettings()->setUnlockDatabase(false);
    m_window.reset(new MainWindow());
    QVERIFY(!m_window->isVisible());
}

void TestBrowserIntegration::init()
{
    m_database = QSharedPointer<Database>::create();
    auto key = QSharedPointer<CompositeKey>::create();
    key->addKey(QSharedPointer<PasswordKey>::create(QStringLiteral("synthetic-test-password")));
    QVERIFY(m_database->setKey(key));
    m_database->metadata()->customData()->set(CustomData::getKeyWithPrefix(CustomData::BrowserKeyPrefix, fixtureId),
                                             fixtureIdentity);
    m_entry = new Entry();
    m_entry->setUuid(QUuid::createUuid());
    m_entry->setGroup(m_database->rootGroup());
    m_entry->setTitle(QStringLiteral("Synthetic browser record"));
    m_entry->setUrl(QStringLiteral("https://example.com/login"));
    m_entry->setUsername(QStringLiteral("synthetic-user"));
    m_entry->setPassword(QStringLiteral("synthetic-entry-password"));
    m_widget.reset(new DatabaseWidget(m_database));
    QVERIFY(!m_widget->isLocked());
    QVERIFY(!m_widget->isVisible());
    browserService()->activeDatabaseChanged(m_widget.data());
    m_client.reset(new BrowserAction());
    const auto keys = browserMessageBuilder()->getKeyPair();
    m_clientPublicKey = keys.first;
    m_clientSecretKey = keys.second;
    exchangeKeys();
    QVERIFY(!m_serverPublicKey.isEmpty());
}

void TestBrowserIntegration::cleanup()
{
    browserService()->activeDatabaseChanged(nullptr);
    m_client.reset();
    m_widget.reset();
    m_database.clear();
    m_entry = nullptr;
}

void TestBrowserIntegration::exchangeKeys()
{
    const auto nonce = browserMessageBuilder()->getRandomBytesAsBase64(24);
    const QJsonObject message{{"action", "change-public-keys"}, {"publicKey", m_clientPublicKey}, {"nonce", nonce}};
    const auto response = m_client->processClientMessage(nullptr, message);
    QCOMPARE(response.value("success").toString(), QStringLiteral("true"));
    m_serverPublicKey = response.value("publicKey").toString();
}

QJsonObject TestBrowserIntegration::request(const QString& action, const QJsonObject& payload)
{
    const auto nonce = browserMessageBuilder()->getRandomBytesAsBase64(24);
    const auto encrypted = browserMessageBuilder()->encryptMessage(payload, nonce, m_serverPublicKey, m_clientSecretKey);
    const QJsonObject message{{"action", action}, {"nonce", nonce}, {"message", encrypted}};
    return m_client->processClientMessage(nullptr, message);
}

QJsonObject TestBrowserIntegration::decode(const QJsonObject& response)
{
    return browserMessageBuilder()->decryptMessage(response.value("message").toString(),
                                                   response.value("nonce").toString(),
                                                   m_serverPublicKey, m_clientSecretKey);
}

void TestBrowserIntegration::associate()
{
    // Use the public association-resume route against a synthetic pre-paired database.
    const auto response = request(QStringLiteral("test-associate"), {{"id", fixtureId}, {"key", fixtureIdentity}});
    QVERIFY2(!response.contains("errorCode"), "Synthetic association failed");
    QCOMPARE(decode(response).value("id").toString(), fixtureId);
}

void TestBrowserIntegration::lookup_data()
{
    QTest::addColumn<QString>("policy");
    QTest::addColumn<bool>("httpAuth");
    QTest::addColumn<bool>("expected");
    const QList<QPair<QString, bool>> cases{{"absent", true}, {"saved", true}, {"site-block", false},
                                          {"form-block", false}, {"conflicting-block", false},
                                          {"realm-mismatch", false}, {"malformed", false},
                                          {"hidden", false}, {"recycled", false}, {"expired", false},
                                          {"restricted-key", false}};
    for (const auto& test : cases) {
        for (const bool httpAuth : {false, true}) {
            const auto name = test.first + (httpAuth ? QStringLiteral("-http") : QStringLiteral("-normal"));
            QTest::newRow(name.toLatin1().constData()) << test.first << httpAuth << test.second;
        }
    }
}

void TestBrowserIntegration::lookup()
{
    QFETCH(QString, policy);
    QFETCH(bool, httpAuth);
    QFETCH(bool, expected);
    QString realm = QStringLiteral("fixture-realm");
    if (policy == "saved") {
        m_entry->customData()->set(BrowserService::KEEPASSXCBROWSER_NAME,
                                  QStringLiteral("{\"Allow\":[\"old.example\"],\"Deny\":[],\"Realm\":\"fixture-realm\"}"));
    } else if (policy == "site-block" || policy == "form-block" || policy == "conflicting-block") {
        const auto host = policy == "form-block" ? QStringLiteral("forms.example.com") : QStringLiteral("example.com");
        m_entry->customData()->set(BrowserService::KEEPASSXCBROWSER_NAME,
                                  QStringLiteral("{\"Allow\":[\"example.com\",\"forms.example.com\"],\"Deny\":[\"%1\"]}").arg(host));
    } else if (policy == "realm-mismatch") {
        m_entry->customData()->set(BrowserService::KEEPASSXCBROWSER_NAME,
                                  QStringLiteral("{\"Allow\":[\"example.com\",\"forms.example.com\"],\"Realm\":\"other-realm\"}"));
    } else if (policy == "malformed") {
        m_entry->customData()->set(BrowserService::KEEPASSXCBROWSER_NAME, QStringLiteral("{invalid"));
    } else if (policy == "hidden") {
        m_entry->customData()->set(BrowserService::OPTION_HIDE_ENTRY, QStringLiteral("true"));
    } else if (policy == "recycled") {
        m_database->recycleEntry(m_entry);
    } else if (policy == "expired") {
        m_entry->setExpires(true);
        m_entry->setExpiryTime(QDateTime::currentDateTimeUtc().addDays(-1));
    } else if (policy == "restricted-key") {
        m_database->rootGroup()->customData()->set(BrowserService::OPTION_RESTRICT_KEY,
                                                  QStringLiteral("different-browser"));
    }
    associate();
    m_database->markAsClean();
    const auto entryData = customDataSnapshot(m_entry->customData());
    const auto groupData = customDataSnapshot(m_entry->group()->customData());
    const auto databaseData = customDataSnapshot(m_database->metadata()->customData());
    const auto historyCount = m_entry->historyItems().size();
    const auto* focus = QApplication::focusWidget();
    const auto* active = QApplication::activeWindow();
    QSignalSpy diagnostic(browserService(), &BrowserService::accessSettingsInvalid);
    const QJsonObject payload{{"id", fixtureId}, {"url", "https://example.com/login"},
                              {"submitUrl", "https://forms.example.com/session"},
                              {"httpAuth", httpAuth ? "true" : "false"}, {"realm", realm},
                              {"keys", QJsonArray{QJsonObject{{"id", fixtureId}, {"key", fixtureIdentity}}}}};
    for (int repeat = 0; repeat < 2; ++repeat) {
        const auto response = request(QStringLiteral("get-logins"), payload);
        if (expected) {
            QVERIFY2(!response.contains("errorCode"), "Eligible synthetic record was not returned");
            const auto result = decode(response);
            QCOMPARE(result.value("count").toInt(), 1);
            const auto record = result.value("entries").toArray().first().toObject();
            QCOMPARE(record.value("uuid").toString(), m_entry->uuidToHex());
            QCOMPARE(record.value("login").toString(), QStringLiteral("synthetic-user"));
            QCOMPARE(record.value("password").toString(), QStringLiteral("synthetic-entry-password"));
        } else {
            QVERIFY(response.contains("errorCode"));
            QVERIFY(!response.contains("message"));
        }
        QCOMPARE(customDataSnapshot(m_entry->customData()), entryData);
        QCOMPARE(customDataSnapshot(m_entry->group()->customData()), groupData);
        QCOMPARE(customDataSnapshot(m_database->metadata()->customData()), databaseData);
        QCOMPARE(m_entry->historyItems().size(), historyCount);
        QVERIFY(!m_database->isModified());
        QVERIFY(!QApplication::activeModalWidget());
        QCOMPARE(QApplication::focusWidget(), focus);
        QCOMPARE(QApplication::activeWindow(), active);
        QVERIFY(!m_window->isVisible());
        QVERIFY(!m_widget->isVisible());
    }
    QCOMPARE(diagnostic.count(), policy == "malformed" ? 2 : 0);
}

void TestBrowserIntegration::unpairedRequest()
{
    const auto response = request(QStringLiteral("get-logins"), {{"url", "https://example.com/"}});
    QVERIFY(response.contains("errorCode"));
    QVERIFY(!response.contains("message"));
    QVERIFY(!QApplication::activeModalWidget());
    associate();
    const auto wrongKey = request(QStringLiteral("get-logins"),
                                  {{"id", fixtureId}, {"url", "https://example.com/"},
                                   {"keys", QJsonArray{QJsonObject{{"id", fixtureId}, {"key", "wrong-fixture-key"}}}}});
    QVERIFY(wrongKey.contains("errorCode"));
    QVERIFY(!wrongKey.contains("message"));
}

void TestBrowserIntegration::lockedRequest()
{
    associate();
    m_database->markAsClean();
    QVERIFY(m_widget->lock());
    QVERIFY(m_widget->isLocked());
    const auto response = request(QStringLiteral("get-logins"), {{"url", "https://example.com/"}});
    QVERIFY(response.contains("errorCode"));
    QVERIFY(!response.contains("message"));
    QVERIFY(!QApplication::activeModalWidget());
}
