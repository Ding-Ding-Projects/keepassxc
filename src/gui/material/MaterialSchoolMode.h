#ifndef KEEPASSXC_MATERIALSCHOOLMODE_H
#define KEEPASSXC_MATERIALSCHOOLMODE_H
#include <QObject>
#include <QByteArray>
#include <QJsonObject>
#include <QFileSystemWatcher>
#include <QWidget>
#include <functional>
class QTranslator;
class TestMaterialSchoolMode;
namespace Material {
class SchoolMode : public QObject {
    Q_OBJECT
public:
    static SchoolMode* instance();
    explicit SchoolMode(const QString& recordPath, QObject* parent = nullptr);
    ~SchoolMode() override;
    static QString sharedRecordPath();
    static bool validate(const QJsonObject& record, QString* error = nullptr);
    bool available() const { return m_available; }
    bool restricted() const { return !m_available || m_record.value(QStringLiteral("enabled")).toBool(); }
    QString name() const;
    QString status() const { return m_status; }
    bool hasCredential() const;
    bool enable();
    bool unlock(QByteArray credential);
    bool rename(const QString& name, QByteArray credential);
    bool setCredential(QByteArray oldCredential, QByteArray newCredential);
    void reload();
signals:
    void changed();
private:
    friend class ::TestMaterialSchoolMode;
    static SchoolMode* s_testInstance;
    static QJsonObject defaults();
    static QByteArray derive(QByteArray credential, const QByteArray& salt);
    bool verifies(QByteArray credential) const;
    bool update(const std::function<bool(QJsonObject&)>& edit);
    bool read(QJsonObject* record);
    bool watch();
    QTranslator* m_english = nullptr;
    bool m_englishInstalled = false;
    bool m_initializing = true;
    QString m_path;
    QJsonObject m_record;
    QFileSystemWatcher m_watcher;
    bool m_available = false;
    QString m_status;
};
class SchoolModeControl : public QWidget {
    Q_OBJECT
public:
    explicit SchoolModeControl(SchoolMode* mode, QWidget* parent = nullptr);
private:
    void refresh();
    SchoolMode* m_mode;
};
}
#endif
