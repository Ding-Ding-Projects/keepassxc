/* Copyright (C) 2026 KeePassXC Team <team@keepassxc.org>
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "MaterialSchoolMode.h"
#include "MaterialVoice.h"
#include "crypto/Random.h"
#include <QStandardPaths>
#include <QApplication>
#include <QTranslator>
#include <cmath>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QLockFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QRegularExpression>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QInputDialog>
#include <QLineEdit>
#include <QPointer>
#include <botan/pwdhash.h>
#include <botan/mem_ops.h>
#ifdef Q_OS_WIN
#include <windows.h>
#endif
namespace Material {
namespace {
class EnglishPresentation : public QTranslator {
public:
    explicit EnglishPresentation(QObject* parent):QTranslator(parent) {}
    bool isEmpty() const override {return false;}
    QString translate(const char*,const char* source,const char*,int) const override {return QString::fromUtf8(source);}
};
constexpr qint64 MaxBytes=8192;
bool localPath(const QString& path) {
    QFileInfo part(path);
    for (;;) {
        if(part.isSymLink())return false;
#ifdef Q_OS_WIN
        const auto attributes=GetFileAttributesW(reinterpret_cast<LPCWSTR>(part.absoluteFilePath().utf16()));
        if(attributes!=INVALID_FILE_ATTRIBUTES && (attributes&FILE_ATTRIBUTE_REPARSE_POINT))return false;
#endif
        const auto parent=part.absolutePath();
        if(parent==part.absoluteFilePath())break;
        part=QFileInfo(parent);
    }
    return true;
}
bool boundedDepth(const QJsonValue& value,int depth=0) {
    if(depth>8)return false;
    if(value.isObject()){const auto object=value.toObject();for(auto it=object.begin();it!=object.end();++it)if(!boundedDepth(it.value(),depth+1))return false;}
    if(value.isArray())for(const auto& child:value.toArray())if(!boundedDepth(child,depth+1))return false;
    return true;
}
void wipe(QByteArray& bytes) { if(!bytes.isEmpty())Botan::secure_scrub_memory(bytes.data(),bytes.size());bytes.clear(); }
QString schoolCopy(const QString& source) {
    QString id=source.toLower();id.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")),QStringLiteral("_"));
    while(id.startsWith(QLatin1Char('_')))id.remove(0,1);
    while(id.endsWith(QLatin1Char('_')))id.chop(1);
    const QString key=QStringLiteral("school.control.")+id.left(110);
    return Voice::catalogueKeys().contains(key)?Voice::say(key):source;
}
QString prompt(const QString& mode,const QString& action) { return mode+QStringLiteral(": ")+schoolCopy(action); }
class PlainInputDialog : public QInputDialog {
public:
    explicit PlainInputDialog(QWidget* parent):QInputDialog(parent) {}
    void setPlainLabel(const QString& text) {
        setLabelText(text);
        configureLabel();
    }
protected:
    void showEvent(QShowEvent* event) override {
        QInputDialog::showEvent(event);
        // QInputDialog assigns its label buddy when it prepares the layout.
        configureLabel();
    }
private:
    void configureLabel() {
        for(auto* label:findChildren<QLabel*>()) {
            label->setTextFormat(Qt::PlainText);
            label->setBuddy(nullptr);
            label->setAccessibleName(label->text());
        }
    }
};
QString ask(SchoolMode* mode,QWidget* host,const QString& action,QLineEdit::EchoMode echo,const QString& initial,bool* accepted) {
    auto* dialog=new PlainInputDialog(host ? host->window() : nullptr);
    QPointer<QInputDialog> alive(dialog);
    dialog->setWindowTitle(mode->name());dialog->setPlainLabel(prompt(mode->name(),action));
    dialog->setTextEchoMode(echo);dialog->setTextValue(initial);
    QObject::connect(mode,&SchoolMode::changed,dialog,[mode,dialog,action]{dialog->setWindowTitle(mode->name());dialog->setPlainLabel(prompt(mode->name(),action));});
    *accepted=dialog->exec()==QDialog::Accepted;
    if(!alive){*accepted=false;return {};}
    QString value=dialog->textValue();dialog->setTextValue(QString());delete dialog;return value;
}

}
QString SchoolMode::sharedRecordPath() {
    return QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)).filePath(QStringLiteral("material-shared/school-mode.json"));
}
SchoolMode* SchoolMode::s_testInstance=nullptr;
SchoolMode* SchoolMode::instance() {
    if(s_testInstance)return s_testInstance;
    static SchoolMode* mode=nullptr;
    if(!mode){
        mode=new SchoolMode(sharedRecordPath(),qApp);
        QObject::connect(mode,&SchoolMode::changed,mode,[]{Voice::notifier()->announce();});
        mode->reload();
    }
    return mode;
}
QJsonObject SchoolMode::defaults() {return {{"schemaVersion",1},{"name",QStringLiteral("School mode")},{"enabled",false},{"revision",0}};}
SchoolMode::SchoolMode(const QString& path,QObject* parent):QObject(parent),m_path(path),m_record(defaults()) {
    m_english=new EnglishPresentation(this);
    connect(&m_watcher,&QFileSystemWatcher::directoryChanged,this,[this]{reload();});
    connect(&m_watcher,&QFileSystemWatcher::fileChanged,this,[this]{reload();});
    reload();m_initializing=false;
}
SchoolMode::~SchoolMode(){if(m_englishInstalled)QCoreApplication::removeTranslator(m_english);}
QString SchoolMode::name() const {return m_record.value(QStringLiteral("name")).toString(QStringLiteral("School mode"));}
bool SchoolMode::hasCredential() const {return m_record.value(QStringLiteral("credential")).isObject();}
bool SchoolMode::validate(const QJsonObject& r,QString* error) {
    auto fail=[error](const QString& reason){if(error)*error=reason;return false;};
    if(QJsonDocument(r).toJson(QJsonDocument::Compact).size()>MaxBytes)return fail(QStringLiteral("The shared record exceeds 8192 bytes."));
    if(!boundedDepth(r))return fail(QStringLiteral("The shared record exceeds JSON depth 8."));
    if(r.value("schemaVersion")!=QJsonValue(1)||!r.value("enabled").isBool()||!r.value("name").isString())return fail(QStringLiteral("The shared record has an unavailable format."));
    const auto name=r.value("name").toString();
    if(name.trimmed()!=name||name.isEmpty()||name.size()>80||name.contains(QRegularExpression(QStringLiteral("[\\x00-\\x1f\\x7f]"))))return fail(QStringLiteral("The chosen name must contain 1..80 characters without control characters."));
    const double revision=r.value("revision").toDouble(-1);
    if(!r.value("revision").isDouble()||revision<0||revision>9007199254740990.0||std::floor(revision)!=revision)return fail(QStringLiteral("The shared revision is invalid."));
    if(r.contains("credential")) {
        if(!r.value("credential").isObject())return fail(QStringLiteral("The local verifier is invalid."));
        auto c=r.value("credential").toObject();
        const QRegularExpression salt(QStringLiteral("\\A[0-9a-f]{32}\\z")),hash(QStringLiteral("\\A[0-9a-f]{64}\\z"));
        if(c.value("algorithm")!=QJsonValue(QStringLiteral("argon2id-v1"))||c.value("memoryKiB")!=QJsonValue(16384)||c.value("iterations")!=QJsonValue(2)||c.value("parallelism")!=QJsonValue(1)||!salt.match(c.value("salt").toString()).hasMatch()||!hash.match(c.value("verifier").toString()).hasMatch())return fail(QStringLiteral("The local verifier has unsupported parameters."));
    }
    if(r.value("enabled").toBool()&&!r.value("credential").isObject())return fail(QStringLiteral("The enabled shared record has no local verifier."));
    return true;
}
bool SchoolMode::watch() {
    const auto directory=QFileInfo(m_path).absolutePath();
    if(!localPath(m_path)||!localPath(m_path+QStringLiteral(".lock"))||!QDir().mkpath(directory)||!QDir(directory).isReadable())return false;
    const auto anchor=QFileInfo(directory).absolutePath();
    if(!m_watcher.directories().contains(anchor)&&!m_watcher.addPath(anchor))return false;
    if(!m_watcher.directories().contains(directory)&&!m_watcher.addPath(directory))return false;
    if(QFileInfo::exists(m_path)&&!m_watcher.files().contains(m_path)&&!m_watcher.addPath(m_path))return false;
    return true;
}
bool SchoolMode::read(QJsonObject* record) {
    if(!localPath(m_path)){m_status=QStringLiteral("The shared record path is unavailable.");return false;}
    if(!QFileInfo::exists(m_path)){*record=defaults();return true;}
    QFile f(m_path);
    if(!QFileInfo(m_path).isFile()||QFileInfo(m_path).isSymLink()||!f.open(QIODevice::ReadOnly)||f.size()>MaxBytes){m_status=QStringLiteral("The shared record cannot be read safely.");return false;}
    QJsonParseError error;const auto d=QJsonDocument::fromJson(f.read(MaxBytes+1),&error);
    QString reason;
    if(error.error!=QJsonParseError::NoError||!d.isObject()||!validate(d.object(),&reason)){m_status=reason.isEmpty()?QStringLiteral("The shared record is invalid."):reason;return false;}
    *record=d.object();return true;
}
void SchoolMode::reload() {
    QJsonObject r;const bool readable=read(&r);const bool watched=watch();
    m_available=readable&&watched;
    if(readable)m_record=r;
    if(!watched)m_status=QStringLiteral("Live watching is unavailable. The shared record must be readable and watched before changes are allowed.");
    else if(readable)m_status=QStringLiteral("Shared local state is available.");
    if(!m_initializing&&restricted()&&!m_englishInstalled){m_englishInstalled=QCoreApplication::installTranslator(m_english);}
    else if(!restricted()&&m_englishInstalled){QCoreApplication::removeTranslator(m_english);m_englishInstalled=false;}
    emit changed();
}
QByteArray SchoolMode::derive(QByteArray credential,const QByteArray& salt) {
    QByteArray result(32,'\0');
    try {
        auto hash=Botan::PasswordHashFamily::create_or_throw("Argon2id")->from_params(16384,2,1);
        hash->derive_key(reinterpret_cast<uint8_t*>(result.data()),result.size(),credential.constData(),credential.size(),reinterpret_cast<const uint8_t*>(salt.constData()),salt.size());
    } catch (...) {wipe(result);}
    wipe(credential);return result;
}
bool SchoolMode::verifies(QByteArray credential) const {
    if(credential.size()<1||credential.size()>1024||!hasCredential()){wipe(credential);return false;}
    auto c=m_record.value("credential").toObject();auto candidate=derive(credential,QByteArray::fromHex(c.value("salt").toString().toLatin1()));wipe(credential);
    const auto expected=QByteArray::fromHex(c.value("verifier").toString().toLatin1());
    const bool match=candidate.size()==32&&Botan::constant_time_compare(reinterpret_cast<const uint8_t*>(candidate.constData()),reinterpret_cast<const uint8_t*>(expected.constData()),32);
    wipe(candidate);return match;
}
bool SchoolMode::update(const std::function<bool(QJsonObject&)>& edit) {
    if(!watch()){m_available=false;m_status=QStringLiteral("The shared record cannot be watched safely.");emit changed();return false;}
    QLockFile lock(m_path+QStringLiteral(".lock"));lock.setStaleLockTime(30000);
    if(!lock.tryLock(1000)){m_status=QStringLiteral("Another local writer is updating the shared record. Try again.");emit changed();return false;}
    QJsonObject current;if(!read(&current)||!watch()){m_available=false;emit changed();return false;}
    m_record=current;
    auto next=current;if(!edit(next))return false;
    next["revision"]=current.value("revision").toDouble()+1;
    QString diagnostic;if(!validate(next,&diagnostic)){m_status=diagnostic;emit changed();return false;}
    QSaveFile file(m_path);file.setDirectWriteFallback(false);
    const auto bytes=QJsonDocument(next).toJson(QJsonDocument::Compact);
    if(!file.open(QIODevice::WriteOnly)){m_status=QStringLiteral("The shared record cannot be saved.");emit changed();return false;}
    file.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner);
    if(file.write(bytes)!=bytes.size()||!file.commit()){m_status=QStringLiteral("The atomic shared-record update did not complete.");emit changed();return false;}
    reload();return m_available;
}
bool SchoolMode::enable(){return update([this](QJsonObject& r){if(!hasCredential()){m_status=QStringLiteral("Set a local PIN or password first.");emit changed();return false;}r["enabled"]=true;return true;});}
bool SchoolMode::unlock(QByteArray credential){const bool ok=update([this,&credential](QJsonObject& r){if(!verifies(credential)){m_status=QStringLiteral("The local credential did not match.");emit changed();return false;}r["enabled"]=false;return true;});wipe(credential);return ok;}
bool SchoolMode::rename(const QString& value,QByteArray credential){const bool ok=update([this,&value,&credential](QJsonObject& r){if(hasCredential()&&!verifies(credential)){m_status=QStringLiteral("The local credential did not match.");emit changed();return false;}r["name"]=value;return true;});wipe(credential);return ok;}
bool SchoolMode::setCredential(QByteArray oldCredential,QByteArray newCredential) {
    const bool ok=update([this,&oldCredential,&newCredential](QJsonObject& r){
        if(newCredential.size()<4||newCredential.size()>1024||(hasCredential()&&!verifies(oldCredential))){m_status=QStringLiteral("Use 4..1024 UTF-8 bytes and verify the existing local credential when replacing it.");emit changed();return false;}
        const auto salt=randomGen()->randomArray(16);auto verifier=derive(newCredential,salt);
        if(verifier.size()!=32){m_status=QStringLiteral("The local verifier could not be created.");emit changed();return false;}
        auto c=r.value("credential").toObject();c["algorithm"]=QStringLiteral("argon2id-v1");c["memoryKiB"]=16384;c["iterations"]=2;c["parallelism"]=1;c["salt"]=QString::fromLatin1(salt.toHex());c["verifier"]=QString::fromLatin1(verifier.toHex());r["credential"]=c;wipe(verifier);return true;
    });wipe(oldCredential);wipe(newCredential);return ok;
}
SchoolModeControl::SchoolModeControl(SchoolMode* mode,QWidget* parent):QWidget(parent),m_mode(mode) {
    setObjectName(QStringLiteral("sharedModeControl"));auto* column=new QVBoxLayout(this);
    auto* title=new QLabel(this);title->setTextFormat(Qt::PlainText);column->addWidget(title);auto* detail=new QLabel(this);detail->setTextFormat(Qt::PlainText);detail->setWordWrap(true);column->addWidget(detail);
    for(const auto* id:{"toggle","rename","credential"}){auto* button=new QPushButton(this);button->setObjectName(QLatin1String(id));column->addWidget(button);}
    connect(findChild<QPushButton*>(QStringLiteral("toggle")),&QPushButton::clicked,this,[this]{
        auto* service=m_mode;QPointer<QWidget> host(this);
        if(!service->restricted()){service->enable();return;}
        bool accepted=false;QString value=ask(service,host,QStringLiteral("Enter the shared local PIN or password"),QLineEdit::Password,{},&accepted);
        auto bytes=value.toUtf8();value.fill(QChar(0));value.clear();if(accepted&&host)service->unlock(bytes);wipe(bytes);
    });
    connect(findChild<QPushButton*>(QStringLiteral("rename")),&QPushButton::clicked,this,[this]{
        auto* service=m_mode;QPointer<QWidget> host(this);
        bool accepted=false;QString value=ask(service,host,QStringLiteral("Chosen name"),QLineEdit::Normal,service->name(),&accepted);
        if(!accepted||!host)return;
        QString credential;if(service->hasCredential())credential=ask(service,host,QStringLiteral("Verify the shared local PIN or password"),QLineEdit::Password,{},&accepted);
        auto bytes=credential.toUtf8();credential.fill(QChar(0));credential.clear();if(accepted&&host)service->rename(value,bytes);wipe(bytes);
    });
    connect(findChild<QPushButton*>(QStringLiteral("credential")),&QPushButton::clicked,this,[this]{
        auto* service=m_mode;QPointer<QWidget> host(this);
        bool accepted=true;QString old;if(service->hasCredential())old=ask(service,host,QStringLiteral("Existing shared local PIN or password"),QLineEdit::Password,{},&accepted);
        if(!accepted||!host){old.fill(QChar(0));old.clear();return;}
        QString next=ask(service,host,QStringLiteral("New shared local PIN or password (4..1024 UTF-8 bytes)"),QLineEdit::Password,{},&accepted);
        auto a=old.toUtf8(),b=next.toUtf8();old.fill(QChar(0));old.clear();next.fill(QChar(0));next.clear();if(accepted&&host)service->setCredential(a,b);wipe(a);wipe(b);
    });
    connect(mode,&SchoolMode::changed,this,&SchoolModeControl::refresh);connect(Voice::notifier(),&Voice::Notifier::changed,this,&SchoolModeControl::refresh);refresh();
}
void SchoolModeControl::refresh() {
    auto labels=findChildren<QLabel*>(QString{},Qt::FindDirectChildrenOnly);labels[0]->setText(m_mode->name());
    labels[1]->setText(m_mode->name()+QStringLiteral(": ")+schoolCopy(!m_mode->available()?QStringLiteral("Unavailable."):m_mode->restricted()?QStringLiteral("Active."):QStringLiteral("Inactive."))+QLatin1Char(' ')+schoolCopy(m_mode->status())+QLatin1Char(' ')+schoolCopy(QStringLiteral("This is a convenience lock, not security. Deleting the shared local record intentionally resets it.")));
    setAccessibleName(m_mode->name());setAccessibleDescription(labels[1]->text());
    for(const auto* id:{"toggle","rename","credential"}){auto* b=findChild<QPushButton*>(QLatin1String(id));const QString action=QLatin1String(id)==QLatin1String("toggle")?(m_mode->restricted()?schoolCopy(QStringLiteral("Unlock ")):schoolCopy(QStringLiteral("Enable "))):QLatin1String(id)==QLatin1String("rename")?schoolCopy(QStringLiteral("Rename ")):schoolCopy(QStringLiteral("Set or replace the local credential for "));const QString raw=action+m_mode->name();QString visible=raw;visible.replace(QLatin1Char('&'),QStringLiteral("&&"));b->setText(visible);b->setAccessibleName(raw);b->setEnabled(m_mode->available());}
}
}
