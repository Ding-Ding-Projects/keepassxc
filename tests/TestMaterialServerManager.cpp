#include "gui/material/MaterialServerManager.h"
#include "core/Config.h"
#include <QApplication>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QJsonArray>
#include <QJsonDocument>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <cstring>
#include <QListWidget>
#include <QLineEdit>
#include <QCheckBox>
#include <QProcess>
#include <QSignalSpy>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif
using Material::ServerManager;
class TestMaterialServerManager : public QObject {
    Q_OBJECT
private slots:
    void inventoryLimitsBeforeFileCreation() {
        QTemporaryDir temporary; QVERIFY(temporary.isValid());
        const QString root=temporary.path()+QStringLiteral("/managed");
        ServerManager manager(nullptr,root);
        const auto runtime=temporary.path()+QStringLiteral("/java.exe");
        const auto jar=temporary.path()+QStringLiteral("/input.jar");
        QFile runtimeFile(runtime); QVERIFY(runtimeFile.open(QIODevice::WriteOnly));runtimeFile.write("synthetic runtime");runtimeFile.close();
        const QByteArray bytes("synthetic distribution");
        QFile jarFile(jar);QVERIFY(jarFile.open(QIODevice::WriteOnly));jarFile.write(bytes);jarFile.close();
        manager.m_name->setText(QStringLiteral("Bounded fixture"));manager.m_runtime->setText(runtime);manager.m_jar->setText(jar);manager.m_eula->setChecked(true);
        manager.m_resolved=QJsonObject{{"id","fixture"},{"downloads",QJsonObject{{"server",QJsonObject{{"size",bytes.size()},{"sha1",QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha1).toHex())}}}}}};
        QJsonArray records;for(int i=0;i<256;++i)records.append(QJsonObject{{"id",QString::number(i)}});
        manager.m_document=QJsonObject{{"schema",1},{"servers",records}};
        QVERIFY(ServerManager::inventoryWithinLimits(manager.m_document));
        manager.createServer();
        QCOMPARE(manager.m_document.value("servers").toArray().size(),256);
        QVERIFY(!QFileInfo::exists(root));
        records.append(QJsonObject{});
        QVERIFY(!ServerManager::inventoryWithinLimits(QJsonObject{{"schema",1},{"servers",records}}));
        manager.m_document=QJsonObject{{"schema",1},{"servers",QJsonArray{}},{"unknown",QString(1024*1024,QLatin1Char('x'))}};
        QVERIFY(!ServerManager::inventoryWithinLimits(manager.m_document));
        manager.createServer();QVERIFY(!QFileInfo::exists(root));
        QVERIFY(!manager.saveInventory());QVERIFY(!QFileInfo::exists(root));
        QJsonObject boundary{{"schema",1},{"servers",QJsonArray{}},{"unknown",QString()}};
        const int overhead=QJsonDocument(boundary).toJson().size();
        boundary.insert("unknown",QString(1024*1024-overhead,QLatin1Char('x')));
        QVERIFY(ServerManager::inventoryWithinLimits(boundary));
        boundary.insert("unknown",boundary.value("unknown").toString()+QLatin1Char('x'));
        QVERIFY(!ServerManager::inventoryWithinLimits(boundary));
    }
    void restartUsesCapturedIdentityAfterSelectionChanges() {
        QTemporaryDir temporary;QVERIFY(temporary.isValid());
        ServerManager manager(nullptr,temporary.path()+QStringLiteral("/managed"));
        auto* other=new QListWidgetItem(QStringLiteral("Other"),manager.m_inventory);other->setData(Qt::UserRole,QStringLiteral("other"));manager.m_inventory->setCurrentItem(other);
        auto* process=new QProcess(&manager);process->setProperty("restartRequested",true);
        manager.m_processes.insert(QStringLiteral("original"),process);
        QSignalSpy attempts(&manager,&ServerManager::startAttempted);
        manager.processFinished(QStringLiteral("original"),process,0);
        QCOMPARE(attempts.size(),1);QCOMPARE(attempts.first().first().toString(),QStringLiteral("original"));
        QVERIFY(manager.m_processes.isEmpty()); // Invalid fixture is rejected, no executable is launched.
    }
    void redirectedAncestorIsRejected() {
        QTemporaryDir temporary;QVERIFY(temporary.isValid());
        const auto target=temporary.path()+QStringLiteral("/target");const auto alias=temporary.path()+QStringLiteral("/alias");
        QVERIFY(QDir().mkpath(target));
#ifdef Q_OS_WIN
        const QString nativeAlias=QDir::toNativeSeparators(alias),nativeTarget=QDir::toNativeSeparators(target);
        if(!CreateSymbolicLinkW(reinterpret_cast<LPCWSTR>(nativeAlias.utf16()),reinterpret_cast<LPCWSTR>(nativeTarget.utf16()),0x1|0x2)) QSKIP("Unprivileged directory symbolic links are unavailable; no elevation requested.");
#else
        if(!QFile::link(target,alias)) QSKIP("Directory symbolic links are unavailable.");
#endif
        QVERIFY(ServerManager::safePath(target));
        QVERIFY(!ServerManager::safePath(alias+QStringLiteral("/new/server.jar")));
        QVERIFY(!ServerManager::containedPath(temporary.path(),alias+QStringLiteral("/new/server.jar")));
        QVERIFY(!ServerManager::containedPath(target,temporary.path()+QStringLiteral("/outside.jar")));
        ServerManager manager(nullptr,alias+QStringLiteral("/managed"));
        QVERIFY(manager.m_document.isEmpty());QVERIFY(!manager.saveInventory());
        QVERIFY(!QFileInfo::exists(target+QStringLiteral("/managed")));
    }
    void junctionAncestorIsRejected() {
#ifndef Q_OS_WIN
        QSKIP("Windows junction fixture is not applicable on this host.");
#else
        QTemporaryDir temporary;QVERIFY(temporary.isValid());
        const QString target=QDir::toNativeSeparators(temporary.path()+QStringLiteral("/target"));
        const QString junction=QDir::toNativeSeparators(temporary.path()+QStringLiteral("/junction"));
        QVERIFY(QDir().mkpath(target));QVERIFY(QDir().mkpath(junction));
        const QString substitute=QStringLiteral("\\??\\")+target;
        struct Header { DWORD tag; WORD length,reserved,subOffset,subLength,printOffset,printLength; };
        const int substituteBytes=substitute.size()*2,printBytes=target.size()*2;
        QByteArray buffer(sizeof(Header)+substituteBytes+2+printBytes+2,0);
        auto* header=reinterpret_cast<Header*>(buffer.data());
        header->tag=0xA0000003;header->length=WORD(buffer.size()-8);header->subLength=WORD(substituteBytes);header->printOffset=WORD(substituteBytes+2);header->printLength=WORD(printBytes);
        std::memcpy(buffer.data()+sizeof(Header),substitute.utf16(),substituteBytes);
        std::memcpy(buffer.data()+sizeof(Header)+substituteBytes+2,target.utf16(),printBytes);
        const HANDLE handle=CreateFileW(reinterpret_cast<LPCWSTR>(junction.utf16()),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT|FILE_FLAG_BACKUP_SEMANTICS,nullptr);
        if(handle==INVALID_HANDLE_VALUE) QSKIP("Temporary junction handle unavailable; no elevation requested.");
        DWORD returned=0;const bool created=DeviceIoControl(handle,0x000900A4,buffer.data(),DWORD(buffer.size()),nullptr,0,&returned,nullptr);CloseHandle(handle);
        if(!created) QSKIP("Temporary junction creation unavailable; no elevation requested.");
        QVERIFY(!ServerManager::safePath(QDir::fromNativeSeparators(junction)+QStringLiteral("/new/inventory.json")));
        ServerManager manager(nullptr,QDir::fromNativeSeparators(junction)+QStringLiteral("/managed"));
        QVERIFY(manager.m_document.isEmpty());QVERIFY(!manager.saveInventory());
        QVERIFY(!QFileInfo::exists(target+QStringLiteral("/managed")));
#endif
    }
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
