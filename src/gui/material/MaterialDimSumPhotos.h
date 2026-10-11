#ifndef KEEPASSXC_MATERIALDIMSUMPHOTOS_H
#define KEEPASSXC_MATERIALDIMSUMPHOTOS_H
#include <QObject>
#include <QImage>
#include <QJsonObject>
#include <QPointer>
#include <QUrl>
#include <QVector>
#include <functional>
class QNetworkAccessManager;
class QNetworkReply;
class TestMaterialDimSum;
namespace Material {
class DimSumPhotos : public QObject {
    Q_OBJECT
public:
    struct Photo { QString english,cantonese,filename,url,digest,revision; qint64 bytes=0; int minimumWidth=1024,minimumHeight=1024; QImage image; QJsonObject coverage; bool officialDigestAvailable=false; };
    struct Resolution { QVector<Photo> photos; int unmatched=0,ambiguous=0; };
    explicit DimSumPhotos(QObject* parent=nullptr,const QString& cacheRoot={});
    void start();
    void cancel();
    static QVector<Photo> parseCatalogue(const QByteArray& bytes);
    static Resolution resolveAssets(const QVector<Photo>& dishes,const QVector<QJsonObject>& assets);
    static QVector<Photo> joinAssets(const QVector<Photo>& dishes,const QVector<QJsonObject>& assets);
    static bool validImage(const QByteArray& bytes,const Photo& photo,QImage* image);
    static Photo cachedPhoto(const QString& root={});
    static bool publicAssetUrl(const QUrl& url);
    static QString defaultCacheRoot();
signals:
    void ready(const Material::DimSumPhotos::Photo& photo);
    void unavailable(const QString& reason);
private:
    friend class ::TestMaterialDimSum;
    using Completion=std::function<void(const QByteArray&)>;
    void get(const QUrl& url,qint64 limit,Completion done,int redirects=0);
    void releasePage(int page);
    void assetPage(int releaseIndex,int page);
    void choosePhoto();
    void fail(const QString& reason);
    bool saveCache(const Photo& photo,const QByteArray& bytes);
    QNetworkAccessManager* m_network;
    QPointer<QNetworkReply> m_reply;
    QString m_root,m_revision;
    QVector<Photo> m_dishes;
    QVector<QJsonObject> m_assets;
    QVector<QUrl> m_releaseAssets;
    int m_requests=0;
    qint64 m_totalBytes=0;
    bool m_active=false;
    quint64 m_generation=0;
};
}
#endif
