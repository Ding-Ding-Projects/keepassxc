#ifndef KEEPASSXC_MATERIALSERVERMANAGER_H
#define KEEPASSXC_MATERIALSERVERMANAGER_H

#include "MaterialScreen.h"
#include <QJsonObject>
#include <QJsonArray>
#include <QHash>
#include <QPointer>
#include <QUrl>
#include <functional>

class QComboBox;
class QLineEdit;
class QSpinBox;
class QCheckBox;
class QLabel;
class QListWidget;
class QNetworkAccessManager;
class QNetworkReply;
class QProcess;

class TestMaterialServerManager;
namespace Material {
class SshProbe;
class ServerManager : public Screen
{
    Q_OBJECT
public:
    explicit ServerManager(QWidget* parent = nullptr, const QString& inventoryRoot = {});
    ~ServerManager() override;
    static QStringList targetNames();
    static bool validName(const QString& name);
    static bool officialMetadataUrl(const QUrl& url);
    static bool inventoryWithinLimits(const QJsonObject& document);
    static bool safePath(const QString& path);
    static bool containedPath(const QString& root, const QString& path);
    static QJsonObject updateRecord(QJsonObject previous, const QJsonObject& changes);
signals:
    void startAttempted(const QString& id);
private:
    friend class ::TestMaterialServerManager;
    void request(const QUrl& url, const QByteArray& expectedHash = {});
    void refreshDockerImages();
    void discoverSsh();
    void invalidateSsh();
    SshProbe* m_sshProbe;
    QLineEdit *m_sshHost, *m_sshUser, *m_sshExecutable, *m_sshIdentity, *m_sshKnownHosts;
    QSpinBox* m_sshPort;
    QJsonObject m_sshFacts;
    quint64 m_sshAttempt = 0;
    void dockerMetadata(const QUrl& url, const QByteArray& authorization,
                        const std::function<void(bool,const QByteArray&,const QByteArray&)>& completed);
    void dockerCommand(const QString& executable, const QStringList& arguments,
                       const std::function<void(bool, const QByteArray&)>& completed);
    void verifyDocker(const QJsonObject& record, const std::function<void(bool)>& completed);
    void dockerLifecycle(const QString& id, const QString& operation);
    static QJsonArray dockerImages(const QByteArray& manifest, const QByteArray& digest, int javaMajor);
    static bool validDockerImage(const QJsonObject& image, int javaMajor);
    static QStringList dockerCreateArguments(const QJsonObject& record, const QString& directory, const QString& owner);
    bool validDockerStopRecord(const QJsonObject& record) const;
    void inspectDockerLifecycle(const QJsonObject& record, const QString& operation);
    bool ownedContainer(const QJsonObject& inspected, const QJsonObject& record) const;
    QString dockerOwner() const;
    void refreshVersions();
    void resolveVersion();
    void createServer();
    void loadInventory();
    bool saveInventory();
    void rebuildInventory();
    void startSelected();
    void startById(const QString& id);
    void processFinished(const QString& id, QProcess* process, int code);
    void stopSelected();
    QString selectedId() const;
    QString directoryFor(const QString& id) const;
    bool validRecord(const QJsonObject& record) const;
    void bindCopy(QWidget* widget, const char* key, bool caption = true);
    void refreshCopy();
    void statusKey(const char* key, const QString& suffix = {});
    QHash<QWidget*, QString> m_captionKeys, m_accessibleKeys;
    QString m_statusKey = QStringLiteral("server.ready"), m_statusSuffix;
    QComboBox *m_target, *m_edition, *m_distribution, *m_versions;
    QLineEdit *m_name, *m_runtime, *m_jar, *m_docker;
    QComboBox* m_images;
    QHash<QString, QProcess*> m_dockerCommands;
    bool m_dockerCreationVerified = false;
    int m_dockerMetadataDeadlineMs = 15000;
    QHash<QString, QString> m_verifiedDockerVersions;
    std::function<void(const QStringList&, const std::function<void(bool,const QByteArray&)>&)> m_dockerTransport;
    QSpinBox *m_memory, *m_port;
    QCheckBox* m_eula;
    QLabel *m_status = nullptr, *m_versionStatus = nullptr;
    QListWidget* m_inventory;
    QNetworkAccessManager* m_network;
    QPointer<QNetworkReply> m_reply;
    QJsonObject m_document;
    QJsonObject m_resolved;
    QHash<QString, QProcess*> m_processes;
    QString m_root;
    quint64 m_generation = 0;
};
}
#endif
