#ifndef KEEPASSXC_MATERIALSSHPROBE_H
#define KEEPASSXC_MATERIALSSHPROBE_H
#include <QObject>
#include <QJsonObject>
#include <QPointer>
#include <QStringList>
#include <functional>
class QProcess;
class QTimer;
class TestMaterialServerManager;
namespace Material {
class SshProbe : public QObject {
    Q_OBJECT
public:
    struct Connection { QString executable, host, user, identityFile, knownHostsFile; int port = 22; };
    explicit SshProbe(QObject* parent = nullptr);
    ~SshProbe() override;
    static bool validConnection(const Connection& connection);
    static QStringList arguments(const Connection& connection);
    static QString remoteCommand();
    static bool parseFacts(const QByteArray& bytes, QJsonObject& facts);
    quint64 start(const Connection& connection);
    void cancel();
    bool active() const;
signals:
    void completed(quint64 attempt, const QString& state, const QJsonObject& facts);
private:
    friend class ::TestMaterialServerManager;
    void finish(const QString& state, const QJsonObject& facts = {});
    void appendOutput(const QByteArray& bytes);
    QPointer<QProcess> m_process;
    QTimer* m_deadline;
    QByteArray m_output;
    quint64 m_attempt = 0;
    int m_deadlineMs = 15000;
    std::function<void(const QString&, const QStringList&)> m_testLaunch;
};
}
#endif
