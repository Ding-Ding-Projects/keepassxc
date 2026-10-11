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
#include <QRegularExpression>
#include <QSaveFile>
#include <QSpinBox>
#include <QSignalBlocker>
#include <QSharedPointer>
#include <algorithm>
#include <QStandardPaths>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>

namespace Material {
namespace {
constexpr qint64 MetadataLimit = 4 * 1024 * 1024;
constexpr qint64 JarLimit = 256 * 1024 * 1024;
QString say(const char* key) { return Voice::say(QString::fromLatin1(key)); }
}
QStringList ServerManager::targetNames()
{
    return {QStringLiteral("Local Windows process"), QStringLiteral("Local Docker (unavailable)"),
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
QJsonObject ServerManager::updateRecord(QJsonObject previous, const QJsonObject& changes)
{
    for (auto it = changes.begin(); it != changes.end(); ++it) previous.insert(it.key(), it.value());
    return previous;
}
ServerManager::ServerManager(QWidget* parent) : Screen(parent)
{
    setObjectName(QStringLiteral("materialServerManager"));
    setHeadline(say("server.title"));
    setSupportingText(say("server.scope"));
    setSearchVisible(true);
    searchBar()->setIdentity(QStringLiteral("servers.inventory"), say("server.search"));
    m_root = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/managed-servers");
    m_network = new QNetworkAccessManager(this);
    auto* form = new QFormLayout;
    m_target = new QComboBox(this); m_target->addItems(targetNames());
    m_edition = new QComboBox(this); m_edition->addItems({QStringLiteral("Java"), QStringLiteral("Bedrock (unavailable: installer not implemented)")});
    m_distribution = new QComboBox(this); m_distribution->addItem(QStringLiteral("Official vanilla Java"));
    m_versions = new QComboBox(this); m_versions->setEditable(false);
    m_name = new QLineEdit(this); m_name->setMaxLength(80); m_name->setClearButtonEnabled(true);
    m_runtime = new QLineEdit(this); m_runtime->setReadOnly(true);
    m_jar = new QLineEdit(this); m_jar->setReadOnly(true);
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
    contentLayout()->addLayout(form); contentLayout()->addWidget(m_eula);
    auto* eula = new QLabel(QStringLiteral("<a href=\"https://www.minecraft.net/en-us/eula\">Minecraft EULA</a>"), this); eula->setOpenExternalLinks(true); contentLayout()->addWidget(eula);
    m_versionStatus = new QLabel(say("server.no-metadata"), this); m_versionStatus->setWordWrap(true); contentLayout()->addWidget(m_versionStatus);
    auto* actions = new QHBoxLayout;
    auto button = [this, actions](const char* key, auto action) { auto* b = new TextButton(this); b->setText(say(key)); b->setAccessibleName(b->text()); actions->addWidget(b); connect(b, &QAbstractButton::clicked, this, action); };
    button("server.refresh", [this] { refreshVersions(); });
    button("server.cancel", [this] { ++m_generation; if (m_reply) m_reply->abort(); m_reply.clear(); status(say("server.cancelled")); });
    button("server.create", [this] { createServer(); }); contentLayout()->addLayout(actions);
    m_inventory = new QListWidget(this); m_inventory->setAccessibleName(say("server.inventory")); contentLayout()->addWidget(m_inventory);
    auto* lifecycle = new QHBoxLayout; contentLayout()->addLayout(lifecycle);
    auto life = [this, lifecycle](const char* key, auto action) { auto* b = new TextButton(this); b->setText(say(key)); b->setAccessibleName(b->text()); lifecycle->addWidget(b); connect(b, &QAbstractButton::clicked, this, action); };
    life("server.start", [this] { startSelected(); }); life("server.stop", [this] { stopSelected(); });
    life("server.restart", [this] { const auto id = selectedId(); auto* p = m_processes.value(id); if (!p) { startSelected(); return; } p->setProperty("restartRequested", true); stopSelected(); });
    m_status = new QLabel(say("server.ready"), this); m_status->setWordWrap(true); contentLayout()->addWidget(m_status);
    connect(m_versions, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] { resolveVersion(); });
    connect(searchBar(), &SearchBar::textChanged, this, [this](const QString& query) { for (int i=0; i<m_inventory->count(); ++i) m_inventory->item(i)->setHidden(!m_inventory->item(i)->text().contains(query, Qt::CaseInsensitive)); });
    loadInventory();
}
ServerManager::~ServerManager()
{
    ++m_generation; if (m_reply) m_reply->abort();
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
    QFile file(m_root + QStringLiteral("/inventory.json"));
    if (file.exists() && (!file.open(QIODevice::ReadOnly) || file.size() > 1024*1024)) { status(say("server.invalid-inventory")); return; }
    if (file.isOpen()) {
        QJsonParseError error; const auto document = QJsonDocument::fromJson(file.readAll(), &error);
        if (error.error != QJsonParseError::NoError || !document.isObject() || document.object().value("schema").toInt() != 1 || !document.object().value("servers").isArray() || document.object().value("servers").toArray().size()>256) { status(say("server.invalid-inventory")); return; }
        m_document = document.object();
    } else m_document = QJsonObject{{"schema",1},{"servers",QJsonArray{}}};
    rebuildInventory();
}
bool ServerManager::saveInventory()
{
    if (QFileInfo(m_root).isSymLink() || !QDir().mkpath(m_root)) return false;
    QSaveFile file(m_root + QStringLiteral("/inventory.json"));
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
    return !dir.isEmpty() && !QFileInfo(m_root).isSymLink() && !QFileInfo(dir).isSymLink() && QFileInfo(dir).absolutePath() == QFileInfo(m_root).absoluteFilePath()
           && r.value("edition").toString() == QLatin1String("java") && r.value("eulaAccepted").toBool()
           && runtime.isAbsolute() && runtime.isFile() && !runtime.isSymLink()
           && (runtime.fileName().compare(QStringLiteral("java.exe"),Qt::CaseInsensitive)==0 || runtime.fileName()==QLatin1String("java"))
           && QFileInfo(dir + QStringLiteral("/server.jar")).isFile() && !QFileInfo(dir + QStringLiteral("/server.jar")).isSymLink();
}
void ServerManager::refreshVersions()
{
    m_resolved = {}; request(QUrl(QStringLiteral("https://piston-meta.mojang.com/mc/game/version_manifest_v2.json")));
}
void ServerManager::resolveVersion()
{
    m_resolved = {}; const auto metadata = m_versions->currentData().toJsonObject();
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
    if (m_target->currentIndex()!=0 || m_edition->currentIndex()!=0) { status(say("server.unavailable-target")); return; }
    if (m_document.value("schema").toInt()!=1 || !validName(m_name->text()) || !m_eula->isChecked() || m_resolved.isEmpty()) { status(say("server.creation-needs")); return; }
    const QFileInfo runtime(m_runtime->text());
    if (!runtime.isAbsolute() || !runtime.isFile() || runtime.isSymLink() || (runtime.fileName().compare(QStringLiteral("java.exe"),Qt::CaseInsensitive)!=0 && runtime.fileName()!=QLatin1String("java"))) { status(say("server.runtime-invalid")); return; }
    QFile input(m_jar->text()); const auto distribution = m_resolved.value("downloads").toObject().value("server").toObject();
    if (QFileInfo(m_jar->text()).isSymLink() || !input.open(QIODevice::ReadOnly) || input.size()!=qint64(distribution.value("size").toDouble()) || input.size()>JarLimit) { status(say("server.integrity")); return; }
    QCryptographicHash hash(QCryptographicHash::Sha1); if (!hash.addData(&input) || hash.result().toHex()!=distribution.value("sha1").toString().toLatin1()) { status(say("server.integrity")); return; }
    const auto id=QUuid::createUuid().toString(QUuid::WithoutBraces); const auto dir=directoryFor(id);
    if (QFileInfo(m_root).isSymLink() || QFileInfo::exists(dir) || !QDir().mkpath(dir)) { status(say("server.write-error")); return; }
    auto write = [&dir](const QString& name,const QByteArray& data) { QSaveFile file(dir+QLatin1Char('/')+name); return file.open(QIODevice::WriteOnly) && file.write(data)==data.size() && file.commit(); };
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
    QJsonObject record{{"id",id},{"name",m_name->text().trimmed()},{"edition","java"},{"version",m_resolved.value("id")},{"runtime",runtime.absoluteFilePath()},{"memoryMiB",m_memory->value()},{"port",m_port->value()},{"sha1",distribution.value("sha1")},{"eulaAccepted",true},{"eulaAcceptedAt",QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}};
    const auto previous=m_document; auto records=m_document.value("servers").toArray(); records.append(record); m_document=updateRecord(m_document,QJsonObject{{"servers",records}});
    if (!saveInventory()) { m_document=previous; status(say("server.write-error")); return; }
    rebuildInventory(); status(say("server.created"));
}
void ServerManager::startSelected()
{
    const auto id=selectedId(); if (id.isEmpty() || m_processes.contains(id)) { status(say("server.select-stopped")); return; }
    QJsonObject record; for(const auto& value:m_document.value("servers").toArray()) if(value.toObject().value("id").toString()==id) record=value.toObject();
    if (!validRecord(record)) { status(say("server.invalid-record")); return; }
    QFile jar(directoryFor(id)+QStringLiteral("/server.jar")); QCryptographicHash hash(QCryptographicHash::Sha1);
    if(!jar.open(QIODevice::ReadOnly) || jar.size()>JarLimit || !hash.addData(&jar) || hash.result().toHex()!=record.value("sha1").toString().toLatin1()) { status(say("server.integrity")); return; }
    const int memory=record.value("memoryMiB").toInt(); if(memory<512 || memory>32768) { status(say("server.invalid-record")); return; }
    auto* process=new QProcess(this); m_processes.insert(id,process); process->setWorkingDirectory(directoryFor(id));
    process->setStandardOutputFile(QProcess::nullDevice()); process->setStandardErrorFile(QProcess::nullDevice());
    connect(process,&QProcess::started,this,[this,process] { status(say("server.running")+QString::number(process->processId())); });
    connect(process,&QProcess::errorOccurred,this,[this,id,process](QProcess::ProcessError error) { status(say("server.process-error")); if(error==QProcess::FailedToStart) { m_processes.remove(id); process->deleteLater(); } });
    connect(process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this,id,process](int code,QProcess::ExitStatus) { const bool restart=process->property("restartRequested").toBool(); m_processes.remove(id); process->deleteLater(); status(say("server.exited")+QString::number(code)); if(restart && selectedId()==id) startSelected(); });
    process->start(record.value("runtime").toString(),{QStringLiteral("-Xmx%1M").arg(memory),QStringLiteral("-jar"),QStringLiteral("server.jar"),QStringLiteral("nogui")});
}
void ServerManager::stopSelected()
{
    auto* process=m_processes.value(selectedId()); if(!process || process->state()!=QProcess::Running) { status(say("server.not-running")); return; }
    // QProcess retains the exact owned process handle. There is no global name/PID termination.
    if(process->write("stop\n")<0) status(say("server.process-error")); else status(say("server.stopping"));
}
}
