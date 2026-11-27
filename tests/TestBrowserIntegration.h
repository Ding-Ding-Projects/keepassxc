#ifndef KEEPASSXC_TESTBROWSERINTEGRATION_H
#define KEEPASSXC_TESTBROWSERINTEGRATION_H

#include "browser/BrowserAction.h"
#include "core/Database.h"
#include <QObject>
#include <QScopedPointer>

class MainWindow;
class DatabaseWidget;
class Entry;

class TestBrowserIntegration : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void init();
    void cleanup();
    void lookup_data();
    void lookup();
    void unpairedRequest();
    void lockedRequest();
private:
    QJsonObject request(const QString& action, const QJsonObject& payload);
    QJsonObject decode(const QJsonObject& response);
    void exchangeKeys();
    void associate();
    QScopedPointer<MainWindow> m_window;
    QScopedPointer<DatabaseWidget> m_widget;
    QSharedPointer<Database> m_database;
    QScopedPointer<BrowserAction> m_client;
    Entry* m_entry = nullptr;
    QString m_clientPublicKey;
    QString m_clientSecretKey;
    QString m_serverPublicKey;
};
#endif
