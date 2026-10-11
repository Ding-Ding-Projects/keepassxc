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
#include <QComboBox>
#include <QUuid>
#include <QProcess>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QElapsedTimer>
#include <QSignalSpy>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif
using Material::ServerManager;
class TrickleReply : public QNetworkReply {
public:
    TrickleReply(const QNetworkRequest& request,QObject* parent,int* aborted,int* trickles)
        :QNetworkReply(parent),m_aborted(aborted),m_trickles(trickles) {
        setRequest(request);setUrl(request.url());setOperation(QNetworkAccessManager::GetOperation);
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute,200);open(QIODevice::ReadOnly|QIODevice::Unbuffered);
        connect(&m_trickle,&QTimer::timeout,this,[this]{m_bytes.append('x');++*m_trickles;emit readyRead();});m_trickle.start(5);
    }
    void abort() override {if(isFinished())return;++*m_aborted;setError(OperationCanceledError,QStringLiteral("Synthetic request aborted"));finish();}
    void finish(){if(isFinished())return;m_trickle.stop();setFinished(true);emit finished();}
    qint64 bytesAvailable() const override{return m_bytes.size()+QNetworkReply::bytesAvailable();}
protected:
    qint64 readData(char* destination,qint64 maximum) override {const auto count=qMin(maximum,qint64(m_bytes.size()));if(count<=0)return 0;std::memcpy(destination,m_bytes.constData(),size_t(count));m_bytes.remove(0,count);return count;}
private:
    QTimer m_trickle;QByteArray m_bytes;int* m_aborted;int* m_trickles;
};
class TrickleNetwork : public QNetworkAccessManager {
public:
    explicit TrickleNetwork(QObject* parent):QNetworkAccessManager(parent){}
    int aborted=0,trickles=0;QPointer<TrickleReply> latest;
protected:
    QNetworkReply* createRequest(Operation,const QNetworkRequest& request,QIODevice*) override {
        latest=new TrickleReply(request,this,&aborted,&trickles);return latest;
    }
};
class TestMaterialServerManager : public QObject {
    Q_OBJECT
private slots:
    void dockerMetadataAbsoluteDeadlineStopsTrickleAndCancelsOnFinish() {
        QTemporaryDir temporary;QVERIFY(temporary.isValid());ServerManager manager(nullptr,temporary.path()+QStringLiteral("/managed"));
        delete manager.m_network;auto* network=new TrickleNetwork(&manager);manager.m_network=network;
        manager.m_dockerMetadataDeadlineMs=60;
        bool completed=false,ok=true;QElapsedTimer elapsed;elapsed.start();
        manager.dockerMetadata(QUrl(QStringLiteral("https://registry-1.docker.io/synthetic")),{},[&](bool success,const QByteArray&,const QByteArray&){completed=true;ok=success;});
        QTRY_VERIFY_WITH_TIMEOUT(completed,1000);QVERIFY(!ok);QCOMPARE(network->aborted,1);QVERIFY(network->trickles>=2);
        QVERIFY(elapsed.elapsed()<1000);
        completed=false;ok=false;
        manager.dockerMetadata(QUrl(QStringLiteral("https://registry-1.docker.io/synthetic")),{},[&](bool success,const QByteArray&,const QByteArray&){completed=true;ok=success;});
        QVERIFY(network->latest);network->latest->finish();QVERIFY(completed);QVERIFY(ok);
        QTest::qWait(120);QCOMPARE(network->aborted,1);
    }
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
    void dockerRegistryIndexRequiresIntegrityAndCompleteBounds() {
        const QJsonObject entry{{"mediaType","application/vnd.oci.image.manifest.v1+json"},{"size",2048},{"digest","sha256:"+QString(64,QLatin1Char('a'))},{"platform",QJsonObject{{"os","linux"},{"architecture","amd64"}}}};
        QJsonObject index{{"schemaVersion",2},{"mediaType","application/vnd.oci.image.index.v1+json"},{"manifests",QJsonArray{entry}}};
        auto bytes=QJsonDocument(index).toJson(QJsonDocument::Compact);
        auto digest=QByteArray("sha256:")+QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex();
        const auto images=ServerManager::dockerImages(bytes,digest,21);QCOMPARE(images.size(),1);
        QVERIFY(ServerManager::validDockerImage(images.first().toObject(),21));
        QVERIFY(ServerManager::dockerImages(bytes,QByteArray("sha256:")+QByteArray(64,'b'),21).isEmpty());
        QVERIFY(ServerManager::dockerImages(bytes.left(bytes.size()-1),digest,21).isEmpty());
        QJsonArray excessive;for(int i=0;i<257;++i)excessive.append(entry);index.insert("manifests",excessive);
        bytes=QJsonDocument(index).toJson(QJsonDocument::Compact);digest=QByteArray("sha256:")+QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex();
        QVERIFY(ServerManager::dockerImages(bytes,digest,21).isEmpty());
        index.insert("manifests",QJsonArray{QJsonObject{{"digest",entry.value("digest")}}});bytes=QJsonDocument(index).toJson();digest=QByteArray("sha256:")+QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex();
        QVERIFY(ServerManager::dockerImages(bytes,digest,21).isEmpty());
    }
    void dockerImageMetadataAndTypedArguments() {
        QTemporaryDir temporary;QVERIFY(temporary.isValid());
        const QJsonObject image{{"provider","docker.io/library/eclipse-temurin"},{"source","https://registry-1.docker.io/v2/library/eclipse-temurin/manifests/21-jre"},{"javaMajor",21},{"indexDigest","sha256:"+QString(64,QLatin1Char('e'))},{"digest","sha256:"+QString(64,QLatin1Char('a'))},{"os","linux"},{"architecture","amd64"},{"retrievedAt","2026-10-11T12:00:00Z"}};
        QVERIFY(ServerManager::validDockerImage(image,21));QVERIFY(!ServerManager::validDockerImage(image,17));
        for(const auto& key:{"provider","source","digest","indexDigest","os","architecture","retrievedAt"}){auto bad=image;bad.insert(key,QStringLiteral("unverified"));QVERIFY(!ServerManager::validDockerImage(bad,21));}
        QJsonObject record{{"id",QUuid::createUuid().toString(QUuid::WithoutBraces)},{"image",image},{"memoryMiB",2048},{"port",25565}};
        const auto args=ServerManager::dockerCreateArguments(record,temporary.path(),QStringLiteral("fixture-owner"));
        QVERIFY(!args.isEmpty());QVERIFY(args.contains(QStringLiteral("never")));QVERIFY(args.contains(QStringLiteral("127.0.0.1:25565:25565/tcp")));
        QCOMPARE(args.at(args.indexOf(QStringLiteral("--entrypoint"))+1),QStringLiteral("java"));
        QCOMPARE(args.last(),QStringLiteral("nogui"));QVERIFY(!args.contains(QStringLiteral("--privileged")));
        QVERIFY(ServerManager::dockerCreateArguments(record,temporary.path()+QStringLiteral(",unexpected"),QStringLiteral("fixture-owner")).isEmpty());
        auto invalid=record;invalid.insert("port",65536);QVERIFY(ServerManager::dockerCreateArguments(invalid,temporary.path(),QStringLiteral("fixture-owner")).isEmpty());
        invalid=record;invalid.insert("memoryMiB",1);QVERIFY(ServerManager::dockerCreateArguments(invalid,temporary.path(),QStringLiteral("fixture-owner")).isEmpty());
    }
    void dockerVerificationRejectsOfflineAndMismatchedRuntime() {
        QTemporaryDir temporary;QVERIFY(temporary.isValid());ServerManager manager(nullptr,temporary.path()+QStringLiteral("/managed"));
        const QString digest=QStringLiteral("sha256:")+QString(64,QLatin1Char('a'));
        const QJsonObject image{{"provider","docker.io/library/eclipse-temurin"},{"source","https://registry-1.docker.io/v2/library/eclipse-temurin/manifests/21-jre"},{"javaMajor",21},{"indexDigest","sha256:"+QString(64,QLatin1Char('e'))},{"digest",digest},{"os","linux"},{"architecture","amd64"},{"retrievedAt","2026-10-11T12:00:00Z"}};
        const QJsonObject record{{"runtime","unused fixture"},{"memoryMiB",2048},{"image",image}};
        bool called=false,result=true;int requests=0;
        manager.m_dockerTransport=[&](const QStringList&,const std::function<void(bool,const QByteArray&)>& complete){++requests;complete(false,{});};
        manager.verifyDocker(record,[&](bool ok){called=true;result=ok;});QVERIFY(called);QVERIFY(!result);QCOMPARE(requests,1);
        requests=0;called=false;
        manager.m_dockerTransport=[&](const QStringList& args,const std::function<void(bool,const QByteArray&)>& complete){
            ++requests;
            if(args.first()==QLatin1String("info")){complete(true,QJsonDocument(QJsonObject{{"Architecture","x86_64"},{"OSType","linux"},{"MemTotal",4096.0*1024*1024}}).toJson());return;}
            complete(true,QJsonDocument(QJsonObject{{"RepoDigests",QJsonArray{"eclipse-temurin@"+digest}},{"Os","linux"},{"Architecture","amd64"},{"Config",QJsonObject{{"Env",QJsonArray{"JAVA_VERSION=jdk-17.0.1+1"}}}}}).toJson());
        };
        manager.verifyDocker(record,[&](bool ok){called=true;result=ok;});QVERIFY(called);QVERIFY(!result);QCOMPARE(requests,2);
        requests=0;called=false;
        manager.m_dockerTransport=[&](const QStringList&,const std::function<void(bool,const QByteArray&)>& complete){++requests;complete(true,QJsonDocument(QJsonObject{{"Architecture","aarch64"},{"OSType","linux"},{"MemTotal",4096.0*1024*1024}}).toJson());};
        manager.verifyDocker(record,[&](bool ok){called=true;result=ok;});QVERIFY(called);QVERIFY(!result);QCOMPARE(requests,1);
    }
    void dockerOwnershipAndSelectionSafeLifecycle() {
        QTemporaryDir temporary;QVERIFY(temporary.isValid());ServerManager manager(nullptr,temporary.path()+QStringLiteral("/managed"));
        const QString id=QUuid::createUuid().toString(QUuid::WithoutBraces),container(64,QLatin1Char('b'));
        const QString dir=manager.directoryFor(id);QVERIFY(QDir().mkpath(dir));
        const QString runtime=temporary.path()+QStringLiteral("/docker.exe");QFile cli(runtime);QVERIFY(cli.open(QIODevice::WriteOnly));cli.write("fixture, never executed");cli.close();
        const QByteArray jarBytes("synthetic server distribution");QFile jar(dir+QStringLiteral("/server.jar"));QVERIFY(jar.open(QIODevice::WriteOnly));jar.write(jarBytes);jar.close();
        const QString digest=QStringLiteral("sha256:")+QString(64,QLatin1Char('a'));
        const QJsonObject image{{"provider","docker.io/library/eclipse-temurin"},{"source","https://registry-1.docker.io/v2/library/eclipse-temurin/manifests/21-jre"},{"javaMajor",21},{"indexDigest","sha256:"+QString(64,QLatin1Char('e'))},{"digest",digest},{"os","linux"},{"architecture","amd64"},{"retrievedAt","2026-10-11T12:00:00Z"}};
        QJsonObject record{{"id",id},{"name","Synthetic container"},{"edition","java"},{"target","docker"},{"runtime",runtime},{"image",image},{"runtimeVersion","jdk-21.0.9+10"},{"memoryMiB",2048},{"port",25565},{"eulaAccepted",true},{"sha1",QString::fromLatin1(QCryptographicHash::hash(jarBytes,QCryptographicHash::Sha1).toHex())},{"containerId",container},{"futureField",42}};
        const QJsonObject config{{"Labels",QJsonObject{{"org.keepassxc.material.server",id},{"org.keepassxc.material.inventory",manager.dockerOwner()}}},{"Image","eclipse-temurin@"+digest},{"Entrypoint",QJsonArray{"java"}},{"Cmd",QJsonArray{"-Xmx2048M","-jar","/data/server.jar","nogui"}}};
        const QJsonObject host{{"Privileged",false},{"NetworkMode","bridge"},{"Memory",2048.0*1024*1024},{"PortBindings",QJsonObject{{"25565/tcp",QJsonArray{QJsonObject{{"HostIp","127.0.0.1"},{"HostPort","25565"}}}}}}};
        const QJsonObject inspected{{"Id",container},{"Name","/keepassxc-"+id},{"Config",config},{"HostConfig",host},{"Mounts",QJsonArray{QJsonObject{{"Type","bind"},{"Destination","/data"},{"Source",dir}}}}};
        QVERIFY(manager.validRecord(record));QVERIFY(manager.ownedContainer(inspected,record));
        for(const auto& key:{"Id","Name","Config","HostConfig","Mounts"}){auto bad=inspected;bad.remove(key);QVERIFY(!manager.ownedContainer(bad,record));}
        auto foreign=record;foreign.insert("containerId",QString(64,QLatin1Char('c')));QVERIFY(!manager.ownedContainer(inspected,foreign));
        manager.m_document=QJsonObject{{"schema",1},{"servers",QJsonArray{record}},{"extension","retained"}};manager.rebuildInventory();
        QList<QStringList> calls;std::function<void(bool,const QByteArray&)> pendingInspect;
        manager.m_dockerTransport=[&](const QStringList& args,const std::function<void(bool,const QByteArray&)>& complete){
            calls.append(args);
            if(args.first()==QLatin1String("info")){complete(true,QJsonDocument(QJsonObject{{"Architecture","x86_64"},{"OSType","linux"},{"MemTotal",4096.0*1024*1024}}).toJson());return;}
            if(args.first()==QLatin1String("image")){complete(true,QJsonDocument(QJsonObject{{"RepoDigests",QJsonArray{"eclipse-temurin@"+digest}},{"Os","linux"},{"Architecture","amd64"},{"Config",QJsonObject{{"Env",QJsonArray{"JAVA_VERSION=jdk-21.0.9+10"}}}}}).toJson());return;}
            if(args.value(1)==QLatin1String("inspect")){pendingInspect=complete;return;}
            complete(true,{});
        };
        manager.dockerLifecycle(id,QStringLiteral("restart"));QVERIFY(bool(pendingInspect));
        auto* other=new QListWidgetItem(QStringLiteral("Other"),manager.m_inventory);other->setData(Qt::UserRole,QStringLiteral("other"));manager.m_inventory->setCurrentItem(other);
        pendingInspect(true,QJsonDocument(inspected).toJson());
        QCOMPARE(calls.last(),QStringList({"container","restart",container}));
        QCOMPARE(manager.m_document.value("servers").toArray().first().toObject().value("futureField").toInt(),42);
        QVERIFY(manager.saveInventory());manager.loadInventory();QCOMPARE(manager.m_document.value("extension").toString(),QStringLiteral("retained"));
        calls.clear();pendingInspect={};manager.dockerLifecycle(id,QStringLiteral("stop"));QVERIFY(bool(pendingInspect));
        auto wrong=inspected;wrong.insert("Id",QString(64,QLatin1Char('d')));pendingInspect(true,QJsonDocument(wrong).toJson());
        QCOMPARE(calls.size(),1); // Stop inspects ownership directly; no launch verification or mutation.
        calls.clear();pendingInspect={};manager.dockerLifecycle(id,QStringLiteral("start"));QVERIFY(bool(pendingInspect));
        ++manager.m_generation;pendingInspect(true,QJsonDocument(inspected).toJson());QCOMPARE(calls.size(),3);
        auto badPort=inspected;auto changedHost=host;changedHost.insert("PortBindings",QJsonObject{});badPort.insert("HostConfig",changedHost);QVERIFY(!manager.ownedContainer(badPort,record));
        auto badMount=inspected;badMount.insert("Mounts",QJsonArray{QJsonObject{{"Type","bind"},{"Destination","/data"},{"Source",temporary.path()}}});QVERIFY(!manager.ownedContainer(badMount,record));
        QVERIFY(QFile::remove(dir+QStringLiteral("/server.jar")));
        manager.m_dockerTransport=[&](const QStringList& args,const std::function<void(bool,const QByteArray&)>& complete){
            calls.append(args);
            if(args.first()!=QLatin1String("container")){complete(false,{});return;} // Launch image/capacity discovery unavailable.
            if(args.value(1)==QLatin1String("inspect")){complete(true,QJsonDocument(inspected).toJson());return;}
            complete(true,{});
        };
        calls.clear();manager.dockerLifecycle(id,QStringLiteral("stop"));QCOMPARE(calls.size(),2);QCOMPARE(calls.last(),QStringList({"container","stop",container}));
        QFile replacementJar(dir+QStringLiteral("/server.jar"));QVERIFY(replacementJar.open(QIODevice::WriteOnly));replacementJar.write("changed distribution");replacementJar.close();
        calls.clear();manager.dockerLifecycle(id,QStringLiteral("stop"));QCOMPARE(calls.size(),2);QCOMPARE(calls.last(),QStringList({"container","stop",container}));
        calls.clear();manager.dockerLifecycle(id,QStringLiteral("restart"));QVERIFY(calls.isEmpty());
    }
    void targetInventory() {
        const auto targets=ServerManager::targetNames();
        QCOMPARE(targets.size(),4);
        QVERIFY(targets[1].contains(QStringLiteral("verified local image required")));
        for(int i=2;i<4;++i) QVERIFY(targets[i].contains(QStringLiteral("unavailable")));
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
