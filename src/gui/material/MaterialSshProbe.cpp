#include "MaterialSshProbe.h"
#include "MaterialServerManager.h"
#include <QFileInfo>
#include <QDir>
#include <QSet>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QTimer>
#include <algorithm>

namespace Material {
SshProbe::SshProbe(QObject* parent) : QObject(parent), m_deadline(new QTimer(this))
{
    m_deadline->setSingleShot(true);
    connect(m_deadline, &QTimer::timeout, this, [this] { finish(QStringLiteral("timeout")); });
}
SshProbe::~SshProbe() { cancel(); }
bool SshProbe::active() const { return !m_process.isNull(); }
bool SshProbe::validConnection(const Connection& c)
{
    static const QRegularExpression host(QStringLiteral("\\A[A-Za-z0-9](?:[A-Za-z0-9.-]{0,251}[A-Za-z0-9])?\\z"));
    static const QRegularExpression user(QStringLiteral("\\A[A-Za-z_][A-Za-z0-9_.-]{0,63}\\z"));
    if (!host.match(c.host).hasMatch() || !user.match(c.user).hasMatch() || c.port < 1 || c.port > 65535) return false;
    for (const auto& path : {c.executable, c.identityFile, c.knownHostsFile}) {
        if (path.size()>4096 || std::any_of(path.cbegin(),path.cend(),[](QChar c){return c.unicode()<32 || c.unicode()==127;}) || path.contains(QLatin1Char('"')) || path.contains(QLatin1Char('%')) || path.contains(QLatin1Char('\n'))
            || path.contains(QLatin1Char('\r')) || !ServerManager::safePath(path) || !QFileInfo(path).isFile()) return false;
    }
    const auto name = QFileInfo(c.executable).fileName();
    return (name == QLatin1String("ssh") || name.compare(QLatin1String("ssh.exe"), Qt::CaseInsensitive) == 0)
        && QFileInfo(c.knownHostsFile).size() > 0 && QFileInfo(c.knownHostsFile).size() <= 1024 * 1024;
}
QString SshProbe::remoteCommand()
{
    // Fixed POSIX/Python probe, with bounded subprocess reads. No connection field enters this text.
    return QString::fromLatin1(R"PROBE(python3 -I -B -c 'import json,os,platform,re,selectors,shutil,subprocess,time
def run(args):
 p=subprocess.Popen(args,stdin=subprocess.DEVNULL,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,env={"PATH":"/usr/bin:/bin","HOME":"/nonexistent","DOCKER_CONFIG":"/nonexistent"})
 s=selectors.DefaultSelector();s.register(p.stdout,selectors.EVENT_READ);b=bytearray();end=time.monotonic()+3
 try:
  while time.monotonic()<end:
   for key,mask in s.select(max(0,end-time.monotonic())):
    v=os.read(key.fd,4097-len(b));b.extend(v)
    if len(b)>4096:return None
    if not v:return bytes(b) if p.wait(timeout=1)==0 else None
  return None
 finally:
  s.close()
  if p.poll() is None:p.kill()
  p.wait();p.stdout.close()
os.environ["PATH"]="/usr/bin:/bin"
system=platform.system();arch=platform.machine();major=None;daemon=None
java=shutil.which("java");docker=shutil.which("docker")
if java:
 try:
  raw=run([java,"-XX:-UsePerfData","-version"])
  match=re.search(rb"version \"([0-9]+)(?:\.([0-9]+))?",raw or b"")
  if match:major=int(match.group(2) if match.group(1)==b"1" and match.group(2) else match.group(1))
 except Exception:pass
if docker:
 try:
  raw=run([docker,"--host","unix:///var/run/docker.sock","info","--format","{{json .OSType}}"])
  if raw is not None:daemon=json.loads(raw)=="linux"
 except Exception:pass
print(json.dumps({"schema":1,"platform":system,"architecture":arch,"javaMajor":major,"dockerCli":bool(docker),"dockerDaemon":daemon},separators=(",",":")))')PROBE");
}
QStringList SshProbe::arguments(const Connection& c)
{
    if (!validConnection(c)) return {};
    QStringList result{QStringLiteral("-F"),QStringLiteral("none"),QStringLiteral("-T"),QStringLiteral("-n"),QStringLiteral("-a"),QStringLiteral("-x")};
    const QStringList options{QStringLiteral("BatchMode=yes"),QStringLiteral("PreferredAuthentications=publickey"),
        QStringLiteral("PasswordAuthentication=no"),QStringLiteral("KbdInteractiveAuthentication=no"),
        QStringLiteral("IdentitiesOnly=yes"),QStringLiteral("IdentityAgent=none"),QStringLiteral("StrictHostKeyChecking=yes"),
        QStringLiteral("UpdateHostKeys=no"),QStringLiteral("CheckHostIP=no"),QStringLiteral("CertificateFile=none"),QStringLiteral("GlobalKnownHostsFile=none"),QStringLiteral("VerifyHostKeyDNS=no"),
        QStringLiteral("UserKnownHostsFile=\"%1\"").arg(QDir::fromNativeSeparators(c.knownHostsFile)),
        QStringLiteral("ClearAllForwardings=yes"),QStringLiteral("ForwardAgent=no"),QStringLiteral("ForwardX11=no"),
        QStringLiteral("PermitLocalCommand=no"),QStringLiteral("ControlMaster=no"),QStringLiteral("ControlPath=none"),
        QStringLiteral("ProxyCommand=none"),QStringLiteral("ProxyJump=none"),QStringLiteral("ConnectTimeout=8"),
        QStringLiteral("ConnectionAttempts=1"),QStringLiteral("LogLevel=QUIET")};
    for (const auto& option : options) result << QStringLiteral("-o") << option;
    result << QStringLiteral("-i") << c.identityFile << QStringLiteral("-p") << QString::number(c.port)
           << QStringLiteral("-l") << c.user << c.host << remoteCommand();
    return result;
}
bool SshProbe::parseFacts(const QByteArray& bytes, QJsonObject& facts)
{
    if (bytes.isEmpty() || bytes.size() > 4096) return false;
    static const QRegularExpression member(QStringLiteral("\"([^\"]*)\"\\s*:"));
    auto members=member.globalMatch(QString::fromUtf8(bytes));QSet<QString> keys;int count=0;
    while(members.hasNext()){const auto key=members.next().captured(1);if(keys.contains(key))return false;keys.insert(key);++count;}
    if(count!=6 || keys!=QSet<QString>{"schema","platform","architecture","javaMajor","dockerCli","dockerDaemon"})return false;
    QJsonParseError error;const auto document=QJsonDocument::fromJson(bytes,&error);
    if (error.error!=QJsonParseError::NoError || !document.isObject()) return false;
    const auto o=document.object();
    if(o.size()!=6 || !o.value("schema").isDouble() || o.value("schema").toDouble()!=1
       || !o.value("platform").isString() || !o.value("architecture").isString()
       || !o.value("dockerCli").isBool() || (!o.value("dockerDaemon").isBool() && !o.value("dockerDaemon").isNull())) return false;
    if(!QStringList{"Linux","Darwin","FreeBSD","OpenBSD"}.contains(o.value("platform").toString())
       || !QStringList{"x86_64","amd64","aarch64","arm64","i386","i686","armv7l"}.contains(o.value("architecture").toString())) return false;
    const auto major=o.value("javaMajor");
    if(!major.isNull() && (!major.isDouble() || major.toDouble()<1 || major.toDouble()>100 || major.toDouble()!=major.toInt())) return false;
    if(!o.value("dockerCli").toBool() && !o.value("dockerDaemon").isNull()) return false;
    facts=o;return true;
}
quint64 SshProbe::start(const Connection& c)
{
    cancel();++m_attempt;
    // Register identity before validation or QProcess::start can complete synchronously.
    emit attemptRegistered(m_attempt);
    const auto args=arguments(c);
    if(args.isEmpty()){emit completed(m_attempt,QStringLiteral("invalid"),{});return m_attempt;}
    auto* process=new QProcess(this);m_process=process;m_output.clear();
    auto environment=QProcessEnvironment::systemEnvironment();
    environment.remove(QStringLiteral("SSH_AUTH_SOCK"));environment.remove(QStringLiteral("SSH_ASKPASS"));
    environment.insert(QStringLiteral("SSH_ASKPASS_REQUIRE"),QStringLiteral("never"));process->setProcessEnvironment(environment);
    process->setStandardInputFile(QProcess::nullDevice());process->setStandardErrorFile(QProcess::nullDevice());
    connect(process,&QProcess::readyReadStandardOutput,this,[this,process]{if(m_process==process)appendOutput(process->read(4097-m_output.size()));});
    connect(process,&QProcess::errorOccurred,this,[this,process](QProcess::ProcessError){if(m_process==process)finish(QStringLiteral("unavailable"));});
    connect(process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this,process](int code,QProcess::ExitStatus exit){
        if(m_process!=process)return;appendOutput(process->read(4097-m_output.size()));if(m_process!=process)return;
        QJsonObject facts;finish(code==0 && exit==QProcess::NormalExit && parseFacts(m_output,facts)?QStringLiteral("ready"):QStringLiteral("unavailable"),facts);
    });
    m_deadline->start(m_deadlineMs);
    if(m_testLaunch)m_testLaunch(c.executable,args);else process->start(c.executable,args);
    return m_attempt;
}
void SshProbe::appendOutput(const QByteArray& bytes){m_output.append(bytes);if(m_output.size()>4096)finish(QStringLiteral("unavailable"));}
void SshProbe::finish(const QString& state,const QJsonObject& facts)
{
    if(!m_process)return;
    auto* process=m_process.data();m_process.clear();m_deadline->stop();process->disconnect(this);process->kill();process->deleteLater();m_output.clear();
    emit completed(m_attempt,state,facts);
}
void SshProbe::cancel(){if(active())finish(QStringLiteral("cancelled"));}
}
