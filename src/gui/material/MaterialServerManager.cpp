#include "MaterialServerManager.h"
#include "MaterialButtons.h"
#include "MaterialSearchBar.h"
#include "MaterialVoice.h"
#include <QCheckBox>
#include <QComboBox>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSpinBox>
#include <QSignalBlocker>
#include <QSharedPointer>
#include <algorithm>
#include <QStandardPaths>
#include <QTimer>
#include <QUuid>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif
#include <QVBoxLayout>

namespace Material {
namespace {
constexpr qint64 MetadataLimit = 4 * 1024 * 1024;
constexpr qint64 JarLimit = 256 * 1024 * 1024;
QString say(const char* key) { return Voice::say(QString::fromLatin1(key)); }
}
QStringList ServerManager::targetNames()
{
    return {QStringLiteral("Local Windows process"), QStringLiteral("Local Docker (verified local image required)"),
            QStringLiteral("SSH process (unavailable)"), QStringLiteral("SSH Docker (unavailable)")};
}
bool ServerManager::validName(const QString& name)
{
    return !name.trimmed().isEmpty() && name.size() <= 80
           && std::none_of(name.cbegin(), name.cend(), [](QChar c) { return c.unicode() < 32; });
}
bool ServerManager::officialMetadataUrl(const QUrl& url)
{
    return url.scheme() == QLatin1String("https") && url.userInfo().isEmpty() && url.port(-1) == -1
           && url.fragment().isEmpty() && url.query().isEmpty()
           && (url.host() == QLatin1String("piston-meta.mojang.com") || url.host() == QLatin1String("launchermeta.mojang.com"));
}
bool ServerManager::inventoryWithinLimits(const QJsonObject& document)
{
    return document.value("schema").toInt()==1 && document.value("servers").isArray()
           && document.value("servers").toArray().size()<=256
           && QJsonDocument(document).toJson().size()<=1024*1024;
}
bool ServerManager::safePath(const QString& path)
{
    if (path.isEmpty() || !QFileInfo(path).isAbsolute()) return false;
    QString current=QDir::cleanPath(QDir::fromNativeSeparators(path));
#ifdef Q_OS_WIN
    // Local drive paths only; UNC, device namespaces and alternate streams are unsupported.
    if (current.size()<3 || current.at(1)!=QLatin1Char(':') || current.at(2)!=QLatin1Char('/') || current.mid(2).contains(QLatin1Char(':'))) return false;
#endif
    for (;;) {
        const QFileInfo info(current);
        if (info.isSymLink()) return false;
#ifdef Q_OS_WIN
        const QString native=QDir::toNativeSeparators(current);
        const DWORD attributes=GetFileAttributesW(reinterpret_cast<LPCWSTR>(native.utf16()));
        if (attributes==INVALID_FILE_ATTRIBUTES) {
            const DWORD error=GetLastError();
            if (error!=ERROR_FILE_NOT_FOUND && error!=ERROR_PATH_NOT_FOUND) return false;
        } else if (attributes & FILE_ATTRIBUTE_REPARSE_POINT) return false;
#endif
        if (info.exists() && info.canonicalFilePath().isEmpty()) return false;
        const QString parent=info.dir().absolutePath();
        if (parent==current) break;
        current=parent;
    }
    return true;
}
bool ServerManager::containedPath(const QString& root,const QString& path)
{
    if (!safePath(root) || !safePath(path)) return false;
    const QString base=QDir::cleanPath(QFileInfo(root).absoluteFilePath());
    const QString child=QDir::cleanPath(QFileInfo(path).absoluteFilePath());
#ifdef Q_OS_WIN
    constexpr auto sensitivity=Qt::CaseInsensitive;
#else
    constexpr auto sensitivity=Qt::CaseSensitive;
#endif
    if (!child.startsWith(base+QLatin1Char('/'),sensitivity)) return false;
    const QString canonicalBase=QFileInfo(root).canonicalFilePath();
    const QString canonicalChild=QFileInfo(path).canonicalFilePath();
    if (!canonicalBase.isEmpty() && !canonicalChild.isEmpty()
        && !canonicalChild.startsWith(canonicalBase+QLatin1Char('/'),sensitivity)) return false;
    return true;
}
QJsonObject ServerManager::updateRecord(QJsonObject previous, const QJsonObject& changes)
{
    for (auto it = changes.begin(); it != changes.end(); ++it) previous.insert(it.key(), it.value());
    return previous;
}
ServerManager::ServerManager(QWidget* parent, const QString& inventoryRoot) : Screen(parent)
{
    setObjectName(QStringLiteral("materialServerManager"));
    setHeadline(say("server.title"));
    setSupportingText(say("server.scope"));
    setSearchVisible(true);
    searchBar()->setIdentity(QStringLiteral("servers.inventory"), say("server.search"));
    m_root = inventoryRoot.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/managed-servers") : inventoryRoot;
    m_network = new QNetworkAccessManager(this);
    auto* form = new QFormLayout;
    m_target = new QComboBox(this); m_target->addItems(targetNames());
    m_edition = new QComboBox(this); m_edition->addItems({QStringLiteral("Java"), QStringLiteral("Bedrock (unavailable: installer not implemented)")});
    m_distribution = new QComboBox(this); m_distribution->addItem(QStringLiteral("Official vanilla Java"));
    m_versions = new QComboBox(this); m_versions->setEditable(false);
    m_name = new QLineEdit(this); m_name->setMaxLength(80); m_name->setClearButtonEnabled(true);
    m_runtime = new QLineEdit(this); m_runtime->setReadOnly(true);
    m_jar = new QLineEdit(this); m_jar->setReadOnly(true);
    m_docker = new QLineEdit(this);m_docker->setReadOnly(true);
    m_images = new QComboBox(this);m_images->setAccessibleName(tr("Verified Eclipse Temurin image and platform"));
    form->addRow(tr("Docker runtime image"),m_images);
    auto* dockerRow=new QWidget(this);auto* dockerLayout=new QHBoxLayout(dockerRow);dockerLayout->setContentsMargins(0,0,0,0);
    m_docker->setAccessibleName(tr("Local Docker CLI"));dockerLayout->addWidget(m_docker);
    auto* dockerBrowse=new TextButton(dockerRow);dockerBrowse->setText(tr("Choose Docker CLI"));dockerLayout->addWidget(dockerBrowse);form->addRow(tr("Local Docker CLI"),dockerRow);
    connect(dockerBrowse,&QAbstractButton::clicked,this,[this]{const auto path=QFileDialog::getOpenFileName(this,tr("Local Docker CLI"),{},QStringLiteral("Docker CLI (docker.exe docker)"));if(!path.isEmpty())m_docker->setText(path);});
    auto* imagesRefresh=new TextButton(this);imagesRefresh->setText(tr("Discover runtime images for selected Java version"));
    form->addRow(imagesRefresh);connect(imagesRefresh,&QAbstractButton::clicked,this,&ServerManager::refreshDockerImages);
    m_memory = new QSpinBox(this); m_memory->setRange(512, 32768); m_memory->setValue(2048); m_memory->setSuffix(QStringLiteral(" MiB"));
    m_port = new QSpinBox(this); m_port->setRange(1, 65535); m_port->setValue(25565);
    m_eula = new QCheckBox(say("server.eula"), this);
    auto add = [form](const QString& label, QWidget* control) { control->setAccessibleName(label); form->addRow(label, control); };
    add(say("server.target"), m_target); add(say("server.edition"), m_edition);
    add(say("server.distribution"), m_distribution); add(say("server.version"), m_versions);
    add(say("server.name"), m_name); add(say("server.memory"), m_memory); add(say("server.port"), m_port);
    auto picker = [this, form](QLineEdit* field, const QString& label, bool runtime) {
        auto* row = new QWidget(this); auto* layout = new QHBoxLayout(row); layout->setContentsMargins(0,0,0,0);
        field->setAccessibleName(label); layout->addWidget(field);
        auto* browse = new TextButton(row); browse->setText(say("server.browse")); browse->setAccessibleName(label + QStringLiteral(": ") + browse->text());
        layout->addWidget(browse); form->addRow(label, row);
        connect(browse, &QAbstractButton::clicked, this, [this, field, runtime] {
            const QString file = QFileDialog::getOpenFileName(this, runtime ? say("server.runtime") : say("server.jar"), {}, runtime ? QStringLiteral("Java runtime (java.exe java)") : QStringLiteral("Server distribution (*.jar)"));
            if (!file.isEmpty()) field->setText(file);
        });
    };
    picker(m_runtime, say("server.runtime"), true); picker(m_jar, say("server.jar"), false);
    auto targetChanged=[this,dockerRow,imagesRefresh]{const bool docker=m_target->currentIndex()==1;m_images->setEnabled(docker);dockerRow->setEnabled(docker);imagesRefresh->setEnabled(docker);m_runtime->parentWidget()->setEnabled(m_target->currentIndex()==0);};
    connect(m_target,qOverload<int>(&QComboBox::currentIndexChanged),this,targetChanged);targetChanged();
    contentLayout()->addLayout(form); contentLayout()->addWidget(m_eula);
    auto* eula = new QLabel(QStringLiteral("<a href=\"https://www.minecraft.net/en-us/eula\">Minecraft EULA</a>"), this); eula->setOpenExternalLinks(true); contentLayout()->addWidget(eula);
    m_versionStatus = new QLabel(say("server.no-metadata"), this); m_versionStatus->setWordWrap(true); contentLayout()->addWidget(m_versionStatus);
    auto* actions = new QHBoxLayout;
    auto button = [this, actions](const char* key, auto action) { auto* b = new TextButton(this); b->setText(say(key)); b->setAccessibleName(b->text()); actions->addWidget(b); connect(b, &QAbstractButton::clicked, this, action); };
    button("server.refresh", [this] { refreshVersions(); });
    button("server.cancel", [this] { ++m_generation; if (m_reply) m_reply->abort(); m_reply.clear(); for(auto* command:m_dockerCommands)command->kill(); status(tr("Operation cancelled. Any container state must be inspected again.")); });
    button("server.create", [this] { createServer(); }); contentLayout()->addLayout(actions);
    m_inventory = new QListWidget(this); m_inventory->setAccessibleName(say("server.inventory")); contentLayout()->addWidget(m_inventory);
    auto* lifecycle = new QHBoxLayout; contentLayout()->addLayout(lifecycle);
    auto life = [this, lifecycle](const char* key, auto action) { auto* b = new TextButton(this); b->setText(say(key)); b->setAccessibleName(b->text()); lifecycle->addWidget(b); connect(b, &QAbstractButton::clicked, this, action); };
    life("server.start", [this] { startSelected(); }); life("server.stop", [this] { stopSelected(); });
    life("server.restart", [this] { const auto id = selectedId(); for(const auto& value:m_document.value("servers").toArray())if(value.toObject().value("id").toString()==id && value.toObject().value("target").toString()==QLatin1String("docker")){dockerLifecycle(id,QStringLiteral("restart"));return;} auto* p = m_processes.value(id); if (!p) { startSelected(); return; } p->setProperty("restartRequested", true); stopSelected(); });
    m_status = new QLabel(say("server.ready"), this); m_status->setWordWrap(true); contentLayout()->addWidget(m_status);
    connect(m_versions, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] { resolveVersion(); });
    connect(searchBar(), &SearchBar::textChanged, this, [this](const QString& query) { for (int i=0; i<m_inventory->count(); ++i) m_inventory->item(i)->setHidden(!m_inventory->item(i)->text().contains(query, Qt::CaseInsensitive)); });
    loadInventory();
}
ServerManager::~ServerManager()
{
    ++m_generation; if (m_reply) m_reply->abort();
    for(auto* command:m_dockerCommands){command->disconnect(this);command->kill();command->waitForFinished(500);}
    // Only children created by this manager are touched. No PID lookup/adoption occurs.
    for (auto* p : m_processes) { p->setProperty("restartRequested", false); p->disconnect(this); p->write("stop\n"); p->waitForFinished(1500); }
}
void ServerManager::status(const QString& text) { m_status->setText(text); }
QString ServerManager::directoryFor(const QString& id) const
{
    if (QUuid(id).isNull()) return {};
    return m_root + QLatin1Char('/') + id;
}
QString ServerManager::selectedId() const
{
    return m_inventory->currentItem() ? m_inventory->currentItem()->data(Qt::UserRole).toString() : QString();
}
void ServerManager::loadInventory()
{
    const QString inventoryPath=m_root+QStringLiteral("/inventory.json");
    if (!containedPath(m_root,inventoryPath)) { status(say("server.invalid-inventory")); return; }
    QFile file(inventoryPath);
    if (file.exists() && (!file.open(QIODevice::ReadOnly) || file.size() > 1024*1024)) { status(say("server.invalid-inventory")); return; }
    if (file.isOpen()) {
        QJsonParseError error; const auto document = QJsonDocument::fromJson(file.read(1024*1024+1), &error);
        if (error.error != QJsonParseError::NoError || !document.isObject() || !inventoryWithinLimits(document.object())) { status(say("server.invalid-inventory")); return; }
        m_document = document.object();
    } else m_document = QJsonObject{{"schema",1},{"servers",QJsonArray{}}};
    rebuildInventory();
}
bool ServerManager::saveInventory()
{
    const QString inventoryPath=m_root+QStringLiteral("/inventory.json");
    if (!inventoryWithinLimits(m_document) || !containedPath(m_root,inventoryPath) || !QDir().mkpath(m_root) || !containedPath(m_root,inventoryPath)) return false;
    QSaveFile file(inventoryPath);
    const auto bytes = QJsonDocument(m_document).toJson();
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit();
}
void ServerManager::rebuildInventory()
{
    const QString current = selectedId(); m_inventory->clear();
    for (const auto& value : m_document.value("servers").toArray()) {
        const auto record = value.toObject(); const auto id = record.value("id").toString();
        auto* item = new QListWidgetItem(record.value("name").toString() + QStringLiteral(" | ") + record.value("version").toString(), m_inventory);
        item->setData(Qt::UserRole,id); item->setToolTip(validRecord(record) ? say("server.owned") : say("server.invalid-record"));
        if (id == current) m_inventory->setCurrentItem(item);
    }
}
bool ServerManager::validRecord(const QJsonObject& r) const
{
    const auto dir = directoryFor(r.value("id").toString());
    const QFileInfo runtime(r.value("runtime").toString());
    if(r.value("target").toString()==QLatin1String("docker")) {
        return !dir.isEmpty() && containedPath(m_root,dir) && containedPath(dir,dir+QStringLiteral("/server.jar"))
            && QFileInfo(dir+QStringLiteral("/server.jar")).isFile() && r.value("edition").toString()==QLatin1String("java")
            && r.value("eulaAccepted").toBool() && safePath(runtime.absoluteFilePath()) && runtime.isFile()
            && (runtime.fileName().compare(QStringLiteral("docker.exe"),Qt::CaseInsensitive)==0 || runtime.fileName()==QLatin1String("docker"))
            && validDockerImage(r.value("image").toObject(),r.value("image").toObject().value("javaMajor").toInt())
            && r.value("memoryMiB").toInt()>=512 && r.value("memoryMiB").toInt()<=32768
            && r.value("port").toInt()>=1 && r.value("port").toInt()<=65535;
    }
    return !dir.isEmpty() && containedPath(m_root,dir) && containedPath(dir,dir+QStringLiteral("/server.jar")) && QFileInfo(dir).absolutePath() == QFileInfo(m_root).absoluteFilePath()
           && r.value("edition").toString() == QLatin1String("java") && r.value("eulaAccepted").toBool()
           && runtime.isAbsolute() && safePath(runtime.filePath()) && runtime.isFile()
           && (runtime.fileName().compare(QStringLiteral("java.exe"),Qt::CaseInsensitive)==0 || runtime.fileName()==QLatin1String("java"))
           && QFileInfo(dir + QStringLiteral("/server.jar")).isFile() && !QFileInfo(dir + QStringLiteral("/server.jar")).isSymLink();
}
void ServerManager::refreshVersions()
{
    m_resolved = {}; request(QUrl(QStringLiteral("https://piston-meta.mojang.com/mc/game/version_manifest_v2.json")));
}
void ServerManager::resolveVersion()
{
    m_resolved = {}; m_images->clear(); const auto metadata = m_versions->currentData().toJsonObject();
    if (!metadata.isEmpty() && metadata.value("sha1").toString().size()==40) request(QUrl(metadata.value("url").toString()), metadata.value("sha1").toString().toLatin1());
}
void ServerManager::request(const QUrl& url, const QByteArray& expectedHash)
{
    const auto generation = ++m_generation;
    if (m_reply) m_reply->abort();
    if (!officialMetadataUrl(url)) { status(say("server.unsafe-url")); return; }
    QNetworkRequest request(url); request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy); request.setTransferTimeout(15000);
    auto* reply = m_network->get(request); m_reply = reply;
    auto bytes = QSharedPointer<QByteArray>::create();
    connect(reply, &QNetworkReply::readyRead, this, [reply,bytes] { bytes->append(reply->read(qMax(qint64(0), MetadataLimit + 1 - bytes->size()))); if (bytes->size()>MetadataLimit) reply->abort(); });
    status(say("server.loading"));
    connect(reply, &QNetworkReply::finished, this, [this,reply,bytes,generation,expectedHash] {
        reply->deleteLater(); if (generation != m_generation) return;
        bytes->append(reply->read(qMax(qint64(0), MetadataLimit + 1 - bytes->size()))); m_reply.clear();
        if (reply->error()!=QNetworkReply::NoError || reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()!=200 || bytes->size()>MetadataLimit) { status(say("server.offline")); return; }
        if (!expectedHash.isEmpty() && QCryptographicHash::hash(*bytes,QCryptographicHash::Sha1).toHex()!=expectedHash) { status(say("server.integrity")); return; }
        QJsonParseError error; const auto document = QJsonDocument::fromJson(*bytes,&error);
        if (error.error!=QJsonParseError::NoError || !document.isObject()) { status(say("server.invalid-metadata")); return; }
        const auto object = document.object();
        if (expectedHash.isEmpty()) {
            const QSignalBlocker blocker(m_versions); m_versions->clear();
            for (const auto& v : object.value("versions").toArray()) { const auto row=v.toObject(); if (!row.value("id").toString().isEmpty()) m_versions->addItem(row.value("id").toString()+QStringLiteral(" | ")+row.value("type").toString(),row); }
            m_versions->setCurrentIndex(-1);
            m_versionStatus->setText(say("server.catalogue") + QString::number(m_versions->count()) + QStringLiteral(" | ") + QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
        } else {
            const auto server = object.value("downloads").toObject().value("server").toObject();
            const auto java = object.value("javaVersion").toObject().value("majorVersion").toInt();
            if (server.value("sha1").toString().size()!=40 || server.value("size").toDouble()<=0 || server.value("size").toDouble()>JarLimit || java<=0) { status(say("server.unavailable-version")); return; }
            m_resolved = object;
            m_versionStatus->setText(QStringLiteral("Java %1 | %2 | SHA-1 %3").arg(java).arg(server.value("size").toDouble(),0,'f',0).arg(server.value("sha1").toString()));
        }
        status(say("server.metadata-ready"));
    });
}
void ServerManager::createServer()
{
    if (!inventoryWithinLimits(m_document) || m_document.value("servers").toArray().size()>=256) { status(say("server.invalid-inventory")); return; }
    if (m_target->currentIndex()>1 || m_edition->currentIndex()!=0) { status(say("server.unavailable-target")); return; }
    if (m_document.value("schema").toInt()!=1 || !validName(m_name->text()) || !m_eula->isChecked() || m_resolved.isEmpty()) { status(say("server.creation-needs")); return; }
    const bool docker=m_target->currentIndex()==1;
    const QFileInfo runtime(docker?m_docker->text():m_runtime->text());
    const auto image=m_images->currentData().toJsonObject();
    if(docker && !validDockerImage(image,m_resolved.value("javaVersion").toObject().value("majorVersion").toInt())){status(tr("Select verified runtime image metadata for this Java version."));return;}
    if(docker && !m_dockerCreationVerified) {
        const QJsonObject snapshot{{"runtime",runtime.absoluteFilePath()},{"image",image},{"memoryMiB",m_memory->value()}};
        const QString selectedVersion=m_resolved.value("id").toString();
        verifyDocker(snapshot,[this,snapshot,selectedVersion](bool ok){
            if(!ok)return;
            if(m_target->currentIndex()!=1 || m_docker->text()!=snapshot.value("runtime").toString() || m_images->currentData().toJsonObject()!=snapshot.value("image").toObject() || m_resolved.value("id").toString()!=selectedVersion || m_memory->value()!=snapshot.value("memoryMiB").toInt()){status(tr("Creation choices changed. Verify them again."));return;}
            m_dockerCreationVerified=true;createServer();m_dockerCreationVerified=false;
        });return;
    }
    if (!safePath(runtime.absoluteFilePath()) || !runtime.isFile() || (!docker && runtime.fileName().compare(QStringLiteral("java.exe"),Qt::CaseInsensitive)!=0 && runtime.fileName()!=QLatin1String("java"))) { status(say("server.runtime-invalid")); return; }
    QFile input(m_jar->text()); const auto distribution = m_resolved.value("downloads").toObject().value("server").toObject();
    if (!safePath(m_jar->text()) || !input.open(QIODevice::ReadOnly) || input.size()!=qint64(distribution.value("size").toDouble()) || input.size()>JarLimit) { status(say("server.integrity")); return; }
    QCryptographicHash hash(QCryptographicHash::Sha1); if (!hash.addData(&input) || hash.result().toHex()!=distribution.value("sha1").toString().toLatin1()) { status(say("server.integrity")); return; }
    const auto id=QUuid::createUuid().toString(QUuid::WithoutBraces); const auto dir=directoryFor(id);
    QJsonObject record{{"id",id},{"name",m_name->text().trimmed()},{"edition","java"},{"version",m_resolved.value("id")},{"runtime",runtime.absoluteFilePath()},{"memoryMiB",m_memory->value()},{"port",m_port->value()},{"sha1",distribution.value("sha1")},{"eulaAccepted",true},{"eulaAcceptedAt",QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}};
    if(docker){record.insert("target","docker");record.insert("image",image);record.insert("runtimeVersion",m_verifiedDockerVersions.value(image.value("digest").toString()));}
    const auto previous=m_document; auto records=m_document.value("servers").toArray(); records.append(record); const auto proposed=updateRecord(m_document,QJsonObject{{"servers",records}});
    if (!inventoryWithinLimits(proposed)) { status(say("server.invalid-inventory")); return; }
    if (!containedPath(m_root,dir) || QFileInfo::exists(dir) || !QDir().mkpath(dir) || !containedPath(m_root,dir)) { status(say("server.write-error")); return; }
    auto write = [&dir](const QString& name,const QByteArray& data) { if (!containedPath(dir,dir+QLatin1Char('/')+name)) return false; QSaveFile file(dir+QLatin1Char('/')+name); return file.open(QIODevice::WriteOnly) && file.write(data)==data.size() && file.commit(); };
    if (!containedPath(dir,dir+QStringLiteral("/server.jar"))) { status(say("server.write-error")); return; }
    QSaveFile copied(dir+QStringLiteral("/server.jar")); QCryptographicHash copyHash(QCryptographicHash::Sha1);
    if (!input.seek(0) || !copied.open(QIODevice::WriteOnly)) { status(say("server.write-error")); return; }
    qint64 copiedBytes=0;
    while (!input.atEnd()) {
        const QByteArray chunk=input.read(65536);
        copiedBytes += chunk.size();
        if (chunk.isEmpty() || copiedBytes > JarLimit || copied.write(chunk)!=chunk.size()) { status(say("server.write-error")); return; }
        copyHash.addData(chunk);
    }
    if(copiedBytes != qint64(distribution.value("size").toDouble()) || copyHash.result().toHex()!=distribution.value("sha1").toString().toLatin1() || !copied.commit()) { status(say("server.integrity")); return; }
    if (!write(QStringLiteral("eula.txt"),"eula=true\n") || !write(QStringLiteral("server.properties"),"server-port="+QByteArray::number(m_port->value())+"\n")) { status(say("server.write-error")); return; }
    m_document=proposed;
    if (!saveInventory()) { m_document=previous; status(say("server.write-error")); return; }
    rebuildInventory(); status(say("server.created"));
}
void ServerManager::startSelected()
{
    startById(selectedId());
}
void ServerManager::startById(const QString& id)
{
    emit startAttempted(id);
    if (id.isEmpty() || m_processes.contains(id)) { status(say("server.select-stopped")); return; }
    QJsonObject record; for(const auto& value:m_document.value("servers").toArray()) if(value.toObject().value("id").toString()==id) record=value.toObject();
    if(record.value("target").toString()==QLatin1String("docker")){dockerLifecycle(id,QStringLiteral("start"));return;}
    if (!validRecord(record)) { status(say("server.invalid-record")); return; }
    QFile jar(directoryFor(id)+QStringLiteral("/server.jar")); QCryptographicHash hash(QCryptographicHash::Sha1);
    if(!jar.open(QIODevice::ReadOnly) || jar.size()>JarLimit || !hash.addData(&jar) || hash.result().toHex()!=record.value("sha1").toString().toLatin1()) { status(say("server.integrity")); return; }
    const int memory=record.value("memoryMiB").toInt(); if(memory<512 || memory>32768) { status(say("server.invalid-record")); return; }
    auto* process=new QProcess(this); m_processes.insert(id,process); process->setWorkingDirectory(directoryFor(id));
    process->setStandardOutputFile(QProcess::nullDevice()); process->setStandardErrorFile(QProcess::nullDevice());
    connect(process,&QProcess::started,this,[this,process] { status(say("server.running")+QString::number(process->processId())); });
    connect(process,&QProcess::errorOccurred,this,[this,id,process](QProcess::ProcessError error) { status(say("server.process-error")); if(error==QProcess::FailedToStart) { m_processes.remove(id); process->deleteLater(); } });
    connect(process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this,id,process](int code,QProcess::ExitStatus) { processFinished(id,process,code); });
    if (!validRecord(record)) { m_processes.remove(id); process->deleteLater(); status(say("server.invalid-record")); return; }
    process->start(record.value("runtime").toString(),{QStringLiteral("-Xmx%1M").arg(memory),QStringLiteral("-jar"),QStringLiteral("server.jar"),QStringLiteral("nogui")});
}
void ServerManager::processFinished(const QString& id,QProcess* process,int code)
{
    const bool restart=process->property("restartRequested").toBool();
    m_processes.remove(id); process->deleteLater();
    status(say("server.exited")+QString::number(code));
    if (restart) startById(id);
}
void ServerManager::stopSelected()
{
    const QString id=selectedId();
    for(const auto& value:m_document.value("servers").toArray())if(value.toObject().value("id").toString()==id && value.toObject().value("target").toString()==QLatin1String("docker")){dockerLifecycle(id,QStringLiteral("stop"));return;}
    auto* process=m_processes.value(id); if(!process || process->state()!=QProcess::Running) { status(say("server.not-running")); return; }
    // QProcess retains the exact owned process handle. There is no global name/PID termination.
    if(process->write("stop\n")<0) status(say("server.process-error")); else status(say("server.stopping"));
}

bool ServerManager::validDockerImage(const QJsonObject& image,int javaMajor)
{
    const QString tag=QString::number(javaMajor)+QStringLiteral("-jre");
    return javaMajor>0 && javaMajor<=100 && image.value("javaMajor").toInt()==javaMajor
        && image.value("provider").toString()==QLatin1String("docker.io/library/eclipse-temurin")
        && image.value("source").toString()==QStringLiteral("https://registry-1.docker.io/v2/library/eclipse-temurin/manifests/")+tag
        && image.value("os").toString()==QLatin1String("linux")
        && (image.value("architecture").toString()==QLatin1String("amd64") || image.value("architecture").toString()==QLatin1String("arm64"))
        && QRegularExpression(QStringLiteral("\\Asha256:[0-9a-f]{64}\\z")).match(image.value("digest").toString()).hasMatch()
        && QRegularExpression(QStringLiteral("\\Asha256:[0-9a-f]{64}\\z")).match(image.value("indexDigest").toString()).hasMatch()
        && QDateTime::fromString(image.value("retrievedAt").toString(),Qt::ISODate).isValid();
}
QJsonArray ServerManager::dockerImages(const QByteArray& manifest,const QByteArray& digest,int major)
{
    QJsonArray accepted;if(manifest.size()>MetadataLimit || major<=0 || major>100)return accepted;
    const QString source=QStringLiteral("https://registry-1.docker.io/v2/library/eclipse-temurin/manifests/%1-jre").arg(major);
    QJsonParseError parse;const auto document=QJsonDocument::fromJson(manifest,&parse);const auto object=document.object();const auto entries=object.value("manifests").toArray();
    const QString type=object.value("mediaType").toString();
    if(parse.error!=QJsonParseError::NoError || object.value("schemaVersion").toInt()!=2
        || (type!=QLatin1String("application/vnd.oci.image.index.v1+json") && type!=QLatin1String("application/vnd.docker.distribution.manifest.list.v2+json"))
        || digest!=QByteArray("sha256:")+QCryptographicHash::hash(manifest,QCryptographicHash::Sha256).toHex() || entries.isEmpty() || entries.size()>256)return accepted;
    for(const auto& value:entries){const auto entry=value.toObject(),platform=entry.value("platform").toObject();
        QJsonObject image{{"provider","docker.io/library/eclipse-temurin"},{"source",source},{"javaMajor",major},{"digest",entry.value("digest")},{"indexDigest",QString::fromLatin1(digest)},{"os",platform.value("os")},{"architecture",platform.value("architecture")},{"retrievedAt",QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}};
        const QString media=entry.value("mediaType").toString();
        if((media==QLatin1String("application/vnd.oci.image.manifest.v1+json") || media==QLatin1String("application/vnd.docker.distribution.manifest.v2+json"))
            && entry.value("size").toDouble()>0 && entry.value("size").toDouble()<=MetadataLimit && validDockerImage(image,major))accepted.append(image);
    }
    return accepted;
}
void ServerManager::dockerMetadata(const QUrl& url,const QByteArray& authorization,const std::function<void(bool,const QByteArray&,const QByteArray&)>& completed)
{
    const auto generation=m_generation;
    QNetworkRequest request(url);request.setTransferTimeout(15000);request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
    if(!authorization.isEmpty())request.setRawHeader("Authorization",QByteArray("Bearer ")+authorization);
    request.setRawHeader("Accept","application/vnd.oci.image.index.v1+json, application/vnd.docker.distribution.manifest.list.v2+json");
    auto* reply=m_network->get(request);m_reply=reply;auto bytes=QSharedPointer<QByteArray>::create();
    connect(reply,&QNetworkReply::readyRead,this,[reply,bytes]{bytes->append(reply->read(qMax(qint64(0),MetadataLimit+1-bytes->size())));if(bytes->size()>MetadataLimit)reply->abort();});
    connect(reply,&QNetworkReply::finished,this,[this,reply,bytes,generation,completed]{reply->deleteLater();if(generation!=m_generation)return;m_reply.clear();
        bytes->append(reply->read(qMax(qint64(0),MetadataLimit+1-bytes->size())));
        completed(reply->error()==QNetworkReply::NoError && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()==200 && bytes->size()<=MetadataLimit,*bytes,reply->rawHeader("Docker-Content-Digest"));
    });
}
void ServerManager::refreshDockerImages()
{
    m_images->clear();
    const int major=m_resolved.value("javaVersion").toObject().value("majorVersion").toInt();
    if(major<=0 || major>100){status(tr("Resolve an official server version before discovering runtime images."));return;}
    const QString source=QStringLiteral("https://registry-1.docker.io/v2/library/eclipse-temurin/manifests/%1-jre").arg(major);
    ++m_generation;if(m_reply)m_reply->abort();
    const QUrl auth(QStringLiteral("https://auth.docker.io/token?service=registry.docker.io&scope=repository:library/eclipse-temurin:pull"));
    dockerMetadata(auth,{},[this,source,major](bool ok,const QByteArray& bytes,const QByteArray&){
        QJsonParseError error;const auto authorization=QJsonDocument::fromJson(bytes,&error).object().value("token").toString().toLatin1();
        if(!ok || error.error!=QJsonParseError::NoError || authorization.isEmpty() || authorization.size()>16384 || authorization.contains('\r') || authorization.contains('\n')){status(tr("Anonymous public registry access is unavailable. No image was downloaded."));return;}
        dockerMetadata(QUrl(source),authorization,[this,source,major](bool received,const QByteArray& manifest,const QByteArray& digest){
            const auto images=received?dockerImages(manifest,digest,major):QJsonArray{};
            for(const auto& value:images){const auto image=value.toObject();m_images->addItem(QStringLiteral("Java %1 | Linux/%2 | %3").arg(major).arg(image.value("architecture").toString(),image.value("digest").toString()),image);}
            status(m_images->count()?tr("Registry metadata verified. The exact image must already be installed locally."):tr("Runtime metadata is unavailable, invalid, or has no compatible Linux platform. No image was downloaded."));
        });
    });
    status(tr("Discovering Eclipse Temurin metadata. No image download is requested."));
}
QString ServerManager::dockerOwner() const
{
    return QString::fromLatin1(QCryptographicHash::hash(QFileInfo(m_root).absoluteFilePath().toUtf8(),QCryptographicHash::Sha256).toHex());
}
void ServerManager::dockerCommand(const QString& executable,const QStringList& arguments,const std::function<void(bool,const QByteArray&)>& completed)
{
    if(m_dockerTransport){const auto generation=m_generation;m_dockerTransport(arguments,[this,generation,completed](bool ok,const QByteArray& bytes){if(generation==m_generation)completed(ok,bytes);});return;}
    const QFileInfo cli(executable);
    if(!safePath(executable) || !cli.isFile() || (cli.fileName().compare(QStringLiteral("docker.exe"),Qt::CaseInsensitive)!=0 && cli.fileName()!=QLatin1String("docker")) || !m_dockerCommands.isEmpty()) {status(tr("A verified local Docker CLI is required, or another operation is still running."));completed(false,{});return;}
    auto* process=new QProcess(this);m_dockerCommands.insert(QStringLiteral("active"),process);
    auto environment=QProcessEnvironment::systemEnvironment();
    for(const auto& key:environment.keys())if(key.startsWith(QStringLiteral("DOCKER_")))environment.remove(key);
    process->setProcessEnvironment(environment);process->setStandardErrorFile(QProcess::nullDevice());
    auto output=QSharedPointer<QByteArray>::create();auto done=QSharedPointer<bool>::create(false);
    const auto generation=m_generation;
    auto finish=[this,process,output,done,completed,generation](bool ok){if(*done)return;*done=true;m_dockerCommands.remove(QStringLiteral("active"));process->deleteLater();if(generation==m_generation)completed(ok,*output);};
    connect(process,&QProcess::readyReadStandardOutput,this,[process,output]{output->append(process->read(qMax(qint64(0),qint64(256*1024+1-output->size()))));if(output->size()>256*1024)process->kill();});
    connect(process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[process,output,finish](int code,QProcess::ExitStatus exit){output->append(process->read(qMax(qint64(0),qint64(256*1024+1-output->size()))));finish(code==0 && exit==QProcess::NormalExit && output->size()<=256*1024);});
    connect(process,&QProcess::errorOccurred,this,[finish](QProcess::ProcessError error){if(error==QProcess::FailedToStart)finish(false);});
    QTimer::singleShot(40000,process,[process]{if(process->state()!=QProcess::NotRunning)process->kill();});
#ifdef Q_OS_WIN
    QStringList args{QStringLiteral("--host"),QStringLiteral("npipe:////./pipe/docker_engine")};
#else
    QStringList args{QStringLiteral("--host"),QStringLiteral("unix:///var/run/docker.sock")};
#endif
    args.append(arguments);process->start(executable,args);
}
void ServerManager::verifyDocker(const QJsonObject& record,const std::function<void(bool)>& completed)
{
    const auto image=record.value("image").toObject();
    if(!validDockerImage(image,image.value("javaMajor").toInt())){status(tr("Verified runtime metadata is required."));completed(false);return;}
    const QString runtime=record.value("runtime").toString();
    dockerCommand(runtime,{QStringLiteral("info"),QStringLiteral("--format"),QStringLiteral("{\"Architecture\":{{json .Architecture}},\"OSType\":{{json .OSType}},\"MemTotal\":{{json .MemTotal}}}")},[this,record,image,runtime,completed](bool ok,const QByteArray& bytes){
        const auto info=QJsonDocument::fromJson(bytes).object();QString arch=info.value("Architecture").toString();if(arch==QLatin1String("x86_64"))arch=QStringLiteral("amd64");if(arch==QLatin1String("aarch64"))arch=QStringLiteral("arm64");
        if(!ok || info.value("OSType").toString()!=QLatin1String("linux") || arch!=image.value("architecture").toString() || info.value("MemTotal").toDouble()<record.value("memoryMiB").toInt()*1024.0*1024.0){status(tr("Local daemon is unavailable, incompatible, or has insufficient reported memory."));completed(false);return;}
        const QString reference=QStringLiteral("eclipse-temurin@")+image.value("digest").toString();
        dockerCommand(runtime,{QStringLiteral("image"),QStringLiteral("inspect"),QStringLiteral("--format"),QStringLiteral("{\"RepoDigests\":{{json .RepoDigests}},\"Os\":{{json .Os}},\"Architecture\":{{json .Architecture}},\"Config\":{\"Env\":{{json .Config.Env}}}}"),reference},[this,image,reference,completed](bool inspected,const QByteArray& output){
            const auto local=QJsonDocument::fromJson(output).object();bool digest=false;for(const auto& value:local.value("RepoDigests").toArray())if(value.toString()==reference || value.toString()==QStringLiteral("docker.io/library/")+reference)digest=true;
            QString runtimeVersion;
            bool java=false;for(const auto& value:local.value("Config").toObject().value("Env").toArray())if(value.toString().startsWith(QStringLiteral("JAVA_VERSION="))){const auto version=value.toString().mid(13);runtimeVersion=version;java=QRegularExpression(QStringLiteral("\\A(?:jdk-)?%1(?:\\.[0-9]{1,4}){0,3}(?:\\+[0-9]{1,5})?\\z").arg(image.value("javaMajor").toInt())).match(version).hasMatch();}
            const bool valid=inspected && digest && java && local.value("Os").toString()==QLatin1String("linux") && local.value("Architecture")==image.value("architecture");
            if(valid)m_verifiedDockerVersions.insert(image.value("digest").toString(),runtimeVersion);
            if(!valid)status(tr("The exact verified runtime image is not installed or its platform/version differs. No pull was attempted."));completed(valid);
        });
    });
}
QStringList ServerManager::dockerCreateArguments(const QJsonObject& record,const QString& directory,const QString& owner)
{
    const QString id=record.value("id").toString();const auto image=record.value("image").toObject();
    if(QUuid(id).isNull() || directory.contains(QLatin1Char(',')) || !safePath(directory) || !validDockerImage(image,image.value("javaMajor").toInt()))return {};
    const int memory=record.value("memoryMiB").toInt(),port=record.value("port").toInt();if(memory<512 || memory>32768 || port<1 || port>65535)return {};
    return {"container","create","--pull","never","--name",QStringLiteral("keepassxc-")+id,
        "--label",QStringLiteral("org.keepassxc.material.server=")+id,"--label",QStringLiteral("org.keepassxc.material.inventory=")+owner,
        "--mount",QStringLiteral("type=bind,src=%1,dst=/data").arg(QDir::fromNativeSeparators(directory)),"--workdir","/data",
        "--memory",QString::number(memory)+"m","--publish",QStringLiteral("127.0.0.1:%1:%1/tcp").arg(port),
        "--network","bridge","--log-driver","none","--pids-limit","512","--cap-drop","ALL","--security-opt","no-new-privileges:true","--stop-timeout","30","--entrypoint","java",
        QStringLiteral("eclipse-temurin@")+image.value("digest").toString(),QStringLiteral("-Xmx%1M").arg(memory),"-jar","/data/server.jar","nogui"};
}
bool ServerManager::ownedContainer(const QJsonObject& inspected,const QJsonObject& record) const
{
    const auto labels=inspected.value("Config").toObject().value("Labels").toObject();
    const auto id=record.value("id").toString();const auto actual=inspected.value("Id").toString();
    if(!QRegularExpression(QStringLiteral("\\A[0-9a-f]{64}\\z")).match(actual).hasMatch()
        || labels.value("org.keepassxc.material.server").toString()!=id || labels.value("org.keepassxc.material.inventory").toString()!=dockerOwner()
        || inspected.value("Name").toString()!=QStringLiteral("/keepassxc-")+id
        || inspected.value("Config").toObject().value("Image").toString()!=QStringLiteral("eclipse-temurin@")+record.value("image").toObject().value("digest").toString())return false;
    if(record.contains("containerId") && record.value("containerId").toString()!=actual)return false;
    const auto host=inspected.value("HostConfig").toObject();
    const int port=record.value("port").toInt();const QString portKey=QString::number(port)+QStringLiteral("/tcp");
    const auto bindings=host.value("PortBindings").toObject();const auto binding=bindings.value(portKey).toArray();
    if(host.value("Privileged").toBool() || host.value("NetworkMode").toString()!=QLatin1String("bridge")
        || host.value("Memory").toDouble()!=record.value("memoryMiB").toInt()*1024.0*1024.0
        || bindings.size()!=1 || binding.size()!=1 || binding.first().toObject().value("HostIp").toString()!=QLatin1String("127.0.0.1")
        || binding.first().toObject().value("HostPort").toString()!=QString::number(port))return false;
    const auto config=inspected.value("Config").toObject();
    if(config.value("Entrypoint").toArray()!=QJsonArray{QStringLiteral("java")}
        || config.value("Cmd").toArray()!=QJsonArray{QStringLiteral("-Xmx%1M").arg(record.value("memoryMiB").toInt()),QStringLiteral("-jar"),QStringLiteral("/data/server.jar"),QStringLiteral("nogui")})return false;
    const auto mounts=inspected.value("Mounts").toArray();if(mounts.size()!=1)return false;
    const auto mount=mounts.first().toObject();
    return mount.value("Type").toString()==QLatin1String("bind") && mount.value("Destination").toString()==QLatin1String("/data")
        && QDir::cleanPath(QDir::fromNativeSeparators(mount.value("Source").toString()))==QDir::cleanPath(directoryFor(id));
}
void ServerManager::dockerLifecycle(const QString& id,const QString& operation)
{
    QJsonObject record;for(const auto& value:m_document.value("servers").toArray())if(value.toObject().value("id").toString()==id)record=value.toObject();
    if(!validRecord(record) || !QStringList{"start","stop","restart"}.contains(operation)){status(tr("The selected container record is invalid."));return;}
    QFile jar(directoryFor(id)+QStringLiteral("/server.jar"));QCryptographicHash hash(QCryptographicHash::Sha1);
    if(!jar.open(QIODevice::ReadOnly) || jar.size()>JarLimit || !hash.addData(&jar) || hash.result().toHex()!=record.value("sha1").toString().toLatin1()){status(say("server.integrity"));return;}
    verifyDocker(record,[this,record,id,operation](bool verified){if(!verified)return;
        if(record.value("runtimeVersion").toString()!=m_verifiedDockerVersions.value(record.value("image").toObject().value("digest").toString())){status(tr("Installed Java runtime version differs from the saved record."));return;}
        if(!record.contains("containerId")) {
            if(operation!=QLatin1String("start")){status(tr("No owned container has been created for this server."));return;}
            const auto args=dockerCreateArguments(record,directoryFor(id),dockerOwner());if(args.isEmpty()){status(tr("Container configuration is invalid."));return;}
            dockerCommand(record.value("runtime").toString(),args,[this,id](bool created,const QByteArray& bytes){
                const QString container=QString::fromLatin1(bytes.trimmed());
                if(!created || !QRegularExpression(QStringLiteral("\\A[0-9a-f]{64}\\z")).match(container).hasMatch()){status(tr("Container creation failed. No existing container was adopted."));return;}
                const auto before=m_document;auto rows=m_document.value("servers").toArray();for(int i=0;i<rows.size();++i)if(rows[i].toObject().value("id").toString()==id)rows[i]=updateRecord(rows[i].toObject(),QJsonObject{{"containerId",container}});m_document.insert("servers",rows);
                if(!saveInventory()){m_document=before;status(tr("Container was created but its identity could not be saved. It was not started."));return;}
                dockerLifecycle(id,QStringLiteral("start"));
            });return;
        }
        const QString identity=record.value("containerId").toString();
        if(!QRegularExpression(QStringLiteral("\\A[0-9a-f]{64}\\z")).match(identity).hasMatch()){status(tr("Saved container identity is invalid."));return;}
        dockerCommand(record.value("runtime").toString(),{"container","inspect","--format","{{json .}}",identity},[this,record,operation](bool ok,const QByteArray& bytes){
            if(!ok){status(tr("Container state is unavailable. It has not been reported stopped."));return;}
            const auto inspected=QJsonDocument::fromJson(bytes).object();if(!ownedContainer(inspected,record)){status(tr("Container ownership or configuration differs. No lifecycle action was sent."));return;}
            const QString container=inspected.value("Id").toString();
            dockerCommand(record.value("runtime").toString(),{"container",operation,container},[this](bool applied,const QByteArray&){status(applied?tr("Owned container lifecycle request completed. Game readiness is not verified."):tr("Container operation failed or connection was lost. Current state is unknown."));});
        });
    });
}

}
