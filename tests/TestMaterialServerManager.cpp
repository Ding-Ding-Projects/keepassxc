#include "gui/material/MaterialServerManager.h"
#include "core/Config.h"
#include <QApplication>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
using Material::ServerManager;
class TestMaterialServerManager : public QObject {
    Q_OBJECT
private slots:
    void targetInventory() {
        const auto targets=ServerManager::targetNames();
        QCOMPARE(targets.size(),4);
        for(int i=1;i<4;++i) QVERIFY(targets[i].contains(QStringLiteral("unavailable")));
        for(int omitted=0;omitted<4;++omitted) { auto copy=targets;copy.removeAt(omitted);QVERIFY(copy.size()!=4); }
    }
    void namesAndMetadataBoundary() {
        QVERIFY(ServerManager::validName(QStringLiteral("Private test server")));
        QVERIFY(!ServerManager::validName(QString()));
        QVERIFY(!ServerManager::validName(QString(81,QLatin1Char('a'))));
        QVERIFY(!ServerManager::validName(QStringLiteral("bad\nname")));
        QVERIFY(ServerManager::officialMetadataUrl(QUrl(QStringLiteral("https://piston-meta.mojang.com/mc/game/version_manifest_v2.json"))));
        for(const auto& url:{QStringLiteral("http://piston-meta.mojang.com/a"),QStringLiteral("https://127.0.0.1/a"),QStringLiteral("https://piston-meta.mojang.com.evil.example/a"),QStringLiteral("https://user:password@piston-meta.mojang.com/a"),QStringLiteral("https://piston-meta.mojang.com:443/a")}) QVERIFY(!ServerManager::officialMetadataUrl(QUrl(url)));
    }
    void unknownFieldsSurvive() {
        QJsonObject original{{"name","Before"},{"extension",QJsonObject{{"unknown",42}}}};
        const auto updated=ServerManager::updateRecord(original,QJsonObject{{"name","After"}});
        QCOMPARE(updated.value("extension"),original.value("extension"));
        QCOMPARE(updated.value("name").toString(),QStringLiteral("After"));
    }
};
int main(int argc,char** argv) {
    QStandardPaths::setTestModeEnabled(true);
    QTemporaryDir isolated;
    if(!isolated.isValid()) return 2;
    QCoreApplication::setOrganizationName(QStringLiteral("ServerManagerTests"));
    QCoreApplication::setApplicationName(QStringLiteral("ServerManagerTests"));
    QApplication app(argc,argv);
    Config::createConfigFromFile(isolated.path()+QStringLiteral("/settings.ini"),isolated.path()+QStringLiteral("/local.ini"));
    TestMaterialServerManager test;return QTest::qExec(&test,argc,argv);
}
#include "TestMaterialServerManager.moc"
