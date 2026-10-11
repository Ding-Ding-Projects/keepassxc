#ifndef KEEPASSXC_MATERIALSERVERMANAGER_H
#define KEEPASSXC_MATERIALSERVERMANAGER_H

#include "MaterialScreen.h"
#include <QJsonObject>
#include <QHash>
#include <QPointer>
#include <QUrl>

class QComboBox;
class QLineEdit;
class QSpinBox;
class QCheckBox;
class QLabel;
class QListWidget;
class QNetworkAccessManager;
class QNetworkReply;
class QProcess;

namespace Material {
class ServerManager : public Screen
{
    Q_OBJECT
public:
    explicit ServerManager(QWidget* parent = nullptr);
    ~ServerManager() override;
    static QStringList targetNames();
    static bool validName(const QString& name);
    static bool officialMetadataUrl(const QUrl& url);
    static QJsonObject updateRecord(QJsonObject previous, const QJsonObject& changes);
private:
    void request(const QUrl& url, const QByteArray& expectedHash = {});
    void refreshVersions();
    void resolveVersion();
    void createServer();
    void loadInventory();
    bool saveInventory();
    void rebuildInventory();
    void startSelected();
    void stopSelected();
    QString selectedId() const;
    QString directoryFor(const QString& id) const;
    bool validRecord(const QJsonObject& record) const;
    void status(const QString& text);
    QComboBox *m_target, *m_edition, *m_distribution, *m_versions;
    QLineEdit *m_name, *m_runtime, *m_jar;
    QSpinBox *m_memory, *m_port;
    QCheckBox* m_eula;
    QLabel *m_status, *m_versionStatus;
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
