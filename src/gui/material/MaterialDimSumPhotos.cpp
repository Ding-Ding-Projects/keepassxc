#include "MaterialDimSumPhotos.h"
#include <QBuffer>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSharedPointer>
#include <QStandardPaths>
#include <QTimer>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif
namespace Material {
namespace {
const QString Repo=QStringLiteral("/repos/Ding-Ding-Projects/dim-sum-photos/");
constexpr qint64 PhotoLimit=8*1024*1024;
constexpr qint64 CatalogueLimit=16*1024*1024;
bool cleanHttps(const QUrl& url) { return url.scheme()==QLatin1String("https") && url.userInfo().isEmpty() && url.port(-1)==-1 && url.fragment().isEmpty(); }
bool safeFilename(const QString& name) { return QRegularExpression(QStringLiteral("^[a-zA-Z0-9][a-zA-Z0-9._-]{0,180}\\.png$")).match(name).hasMatch(); }
bool safeCachePath(const QString& path)
{
    if(!QFileInfo(path).isAbsolute())return false;
    QString current=QDir::cleanPath(path);
    for(;;) {
        QFileInfo info(current);if(info.isSymLink())return false;
#ifdef Q_OS_WIN
        const auto native=QDir::toNativeSeparators(current);
        const DWORD attributes=GetFileAttributesW(reinterpret_cast<LPCWSTR>(native.utf16()));
        if(attributes!=INVALID_FILE_ATTRIBUTES && (attributes&FILE_ATTRIBUTE_REPARSE_POINT))return false;
        if(attributes==INVALID_FILE_ATTRIBUTES && GetLastError()!=ERROR_FILE_NOT_FOUND && GetLastError()!=ERROR_PATH_NOT_FOUND)return false;
#endif
        const auto parent=info.dir().absolutePath();if(parent==current)break;current=parent;
    }
    return true;
}

}
QString DimSumPhotos::defaultCacheRoot() { return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+QStringLiteral("/dim-sum-public-photo-v1"); }
DimSumPhotos::DimSumPhotos(QObject* parent,const QString& cacheRoot) : QObject(parent),m_network(new QNetworkAccessManager(this)),m_root(cacheRoot.isEmpty()?defaultCacheRoot():cacheRoot) {}
bool DimSumPhotos::publicAssetUrl(const QUrl& url)
{
    if(!cleanHttps(url) || url.host()!=QLatin1String("github.com") || !url.query().isEmpty()) return false;
    const auto parts=url.path().split(QLatin1Char('/'),Qt::SkipEmptyParts);
    return parts.size()==6 && parts[0]==QLatin1String("Ding-Ding-Projects") && parts[1]==QLatin1String("dim-sum-photos") && parts[2]==QLatin1String("releases") && parts[3]==QLatin1String("download") && QRegularExpression(QStringLiteral("^catalog-v1(?:-[a-zA-Z0-9-]+)?$")).match(parts[4]).hasMatch() && safeFilename(parts[5]);
}
QVector<DimSumPhotos::Photo> DimSumPhotos::parseCatalogue(const QByteArray& bytes)
{
    QVector<Photo> result;if(bytes.size()>CatalogueLimit)return result;
    QJsonParseError error;const auto doc=QJsonDocument::fromJson(bytes,&error);const auto object=doc.object();
    const auto dishes=object.value("dishes").toArray();
    if(error.error!=QJsonParseError::NoError || object.value("schemaVersion").toString()!=QLatin1String("1.0.0") || dishes.isEmpty() || dishes.size()>5000 || object.value("total").toInt()!=dishes.size())return result;
    const auto spec=object.value("imageSpecification").toObject();
    const int minimumWidth=spec.value("minimumWidth").toInt(),minimumHeight=spec.value("minimumHeight").toInt();
    if(spec.value("format").toString()!=QLatin1String("png") || minimumWidth<1 || minimumHeight<1 || minimumWidth>4096 || minimumHeight>4096)return {};
    QHash<QString,bool> seen;
    for(const auto& v:dishes) {
        const auto row=v.toObject();const auto names=row.value("name").toObject();const auto path=row.value("image").toObject().value("path").toString();
        Photo photo;photo.minimumWidth=minimumWidth;photo.minimumHeight=minimumHeight;photo.english=names.value("en").toString();photo.cantonese=names.value("zhHant").toString();photo.filename=path.mid(7);
        if(!path.startsWith(QLatin1String("images/")) || !safeFilename(photo.filename) || photo.english.isEmpty() || photo.cantonese.isEmpty() || photo.english.size()>256 || photo.cantonese.size()>256 || seen.contains(photo.filename))return {};
        seen.insert(photo.filename,true);result.append(photo);
    }
    return result;
}
DimSumPhotos::Resolution DimSumPhotos::resolveAssets(const QVector<Photo>& dishes,const QVector<QJsonObject>& assets)
{
    QHash<QString,QJsonObject> available;QHash<QString,bool> ambiguous;
    for(const auto& asset:assets) {
        const QString name=asset.value("name").toString(),url=asset.value("browser_download_url").toString();
        const auto size=qint64(asset.value("size").toDouble());const auto digest=asset.value("digest").toString();
        if(asset.value("state").toString()!=QLatin1String("uploaded") || !safeFilename(name) || !publicAssetUrl(QUrl(url)) || QFileInfo(QUrl(url).path()).fileName()!=name || size<=0 || size>PhotoLimit)continue;
        if(!digest.isEmpty() && !QRegularExpression(QStringLiteral("^sha256:[a-f0-9]{64}$")).match(digest).hasMatch())continue;
        if(available.contains(name) && (available[name].value("digest")!=asset.value("digest") || available[name].value("size")!=asset.value("size") || (digest.isEmpty() && available[name].value("browser_download_url")!=asset.value("browser_download_url"))))ambiguous.insert(name,true);
        else available.insert(name,asset);
    }
    Resolution result;
    for(auto photo:dishes) {
        if(ambiguous.contains(photo.filename)) { ++result.ambiguous;continue; }
        if(!available.contains(photo.filename)) { ++result.unmatched;continue; }
        const auto asset=available[photo.filename];photo.url=asset.value("browser_download_url").toString();photo.bytes=qint64(asset.value("size").toDouble());photo.digest=asset.value("digest").toString();photo.officialDigestAvailable=!photo.digest.isEmpty();result.photos.append(photo);
    }
    return result;
}
QVector<DimSumPhotos::Photo> DimSumPhotos::joinAssets(const QVector<Photo>& dishes,const QVector<QJsonObject>& assets)
{
    return resolveAssets(dishes,assets).photos;
}

bool DimSumPhotos::validImage(const QByteArray& bytes,const Photo& photo,QImage* image)
{
    if(bytes.isEmpty() || bytes.size()>PhotoLimit || bytes.size()!=photo.bytes)return false;
    if(!photo.digest.isEmpty() && QStringLiteral("sha256:")+QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex())!=photo.digest)return false;
    QBuffer buffer;buffer.setData(bytes);buffer.open(QIODevice::ReadOnly);QImageReader reader(&buffer,"png");
    if(reader.format()!="png" || !reader.canRead())return false;
    const auto size=reader.size();if(size.width()<photo.minimumWidth || size.height()<photo.minimumHeight || size.width()>4096 || size.height()>4096 || qint64(size.width())*size.height()>16*1024*1024)return false;
    // Qt exposes a process-wide decoder budget. Narrow it for this synchronous
    // decode and restore the previous policy immediately afterwards.
    const int previousLimit=QImageReader::allocationLimit();
    QImageReader::setAllocationLimit(previousLimit>0?qMin(previousLimit,64):64);
    const auto decoded=reader.read();
    QImageReader::setAllocationLimit(previousLimit);
    if(decoded.isNull() || decoded.size()!=size || decoded.sizeInBytes()<=0 || decoded.sizeInBytes()>64*1024*1024)return false;
    if(image)*image=decoded;return true;
}
DimSumPhotos::Photo DimSumPhotos::cachedPhoto(const QString& root)
{
    const auto directory=root.isEmpty()?defaultCacheRoot():root;Photo photo;
    QFile receipt(directory+QStringLiteral("/receipt.json")),file(directory+QStringLiteral("/photo.png"));
    if(!safeCachePath(directory+QStringLiteral("/receipt.json")) || !safeCachePath(directory+QStringLiteral("/photo.png")) || !receipt.open(QIODevice::ReadOnly) || receipt.size()>16384)return photo;
    const auto object=QJsonDocument::fromJson(receipt.read(16385)).object();
    const auto retrieved=QDateTime::fromString(object.value("retrievedAt").toString(),Qt::ISODate);
    if(!retrieved.isValid() || retrieved>QDateTime::currentDateTimeUtc() || retrieved.secsTo(QDateTime::currentDateTimeUtc())>24*60*60)return photo;
    if(object.value("schema").toInt()!=1 || object.value("catalogueSource").toString()!=QLatin1String("https://raw.githubusercontent.com/Ding-Ding-Projects/dim-sum-photos/main/catalog/index.json") || !publicAssetUrl(QUrl(object.value("assetUrl").toString())) || !QRegularExpression(QStringLiteral("^[a-f0-9]{40}$")).match(object.value("catalogueRevision").toString()).hasMatch())return photo;
    photo.minimumWidth=object.value("minimumWidth").toInt();photo.minimumHeight=object.value("minimumHeight").toInt();
    if(photo.minimumWidth<1 || photo.minimumHeight<1 || photo.minimumWidth>4096 || photo.minimumHeight>4096)return {};
    photo.english=object.value("english").toString();photo.cantonese=object.value("cantonese").toString();photo.url=object.value("assetUrl").toString();photo.revision=object.value("catalogueRevision").toString();photo.digest=object.value("cachedDigest").toString();photo.bytes=qint64(object.value("bytes").toDouble());
    const auto officialDigest=object.value("officialDigest").toString();
    if(!officialDigest.isEmpty() && officialDigest!=photo.digest)return {};
    const int discovered=object.value("discoveredAssets").toInt(-1),published=object.value("publishedDishes").toInt(-1),catalogue=object.value("catalogueDishes").toInt(-1),unmatched=object.value("unmatchedDishes").toInt(-1),ambiguous=object.value("ambiguousDishes").toInt(-1);
    if(!object.value("completeAssetDiscovery").isBool() || !object.value("completeAssetDiscovery").toBool() || discovered<1 || discovered>10000 || published<1 || published>discovered || catalogue<1 || catalogue>5000 || unmatched<0 || unmatched>5000 || ambiguous<0 || ambiguous>5000 || published>catalogue || published+unmatched+ambiguous!=catalogue)return {};
    photo.officialDigestAvailable=!officialDigest.isEmpty();
    photo.coverage=QJsonObject{{"catalogueDishes",catalogue},{"publishedDishes",published},{"unmatchedDishes",unmatched},{"ambiguousDishes",ambiguous},{"discoveredAssets",discovered}};
    if(photo.english.isEmpty() || photo.cantonese.isEmpty() || photo.english.size()>256 || photo.cantonese.size()>256 || !QRegularExpression(QStringLiteral("^sha256:[a-f0-9]{64}$")).match(photo.digest).hasMatch() || !file.open(QIODevice::ReadOnly) || file.size()>PhotoLimit || !validImage(file.read(PhotoLimit+1),photo,&photo.image))return {};
    return photo;
}
bool DimSumPhotos::saveCache(const Photo& photo,const QByteArray& bytes)
{
    if(!safeCachePath(m_root+QStringLiteral("/photo.png")) || !safeCachePath(m_root+QStringLiteral("/receipt.json")) || !QDir().mkpath(m_root) || !safeCachePath(m_root))return false;
    QSaveFile image(m_root+QStringLiteral("/photo.png"));if(!image.open(QIODevice::WriteOnly) || image.write(bytes)!=bytes.size() || !image.commit())return false;
    const auto resolution=resolveAssets(m_dishes,m_assets);
    const QJsonObject object{{"schema",1},{"minimumWidth",photo.minimumWidth},{"minimumHeight",photo.minimumHeight},{"catalogueDishes",m_dishes.size()},{"unmatchedDishes",resolution.unmatched},{"ambiguousDishes",resolution.ambiguous},{"catalogueSource","https://raw.githubusercontent.com/Ding-Ding-Projects/dim-sum-photos/main/catalog/index.json"},{"catalogueRevision",m_revision},{"assetUrl",photo.url},{"officialDigest",photo.digest},{"cachedDigest",QStringLiteral("sha256:")+QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex())},{"english",photo.english},{"cantonese",photo.cantonese},{"bytes",bytes.size()},{"retrievedAt",QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},{"completeAssetDiscovery",true},{"discoveredAssets",m_assets.size()},{"publishedDishes",resolution.photos.size()}};
    QSaveFile receipt(m_root+QStringLiteral("/receipt.json"));const auto metadata=QJsonDocument(object).toJson();return receipt.open(QIODevice::WriteOnly) && receipt.write(metadata)==metadata.size() && receipt.commit();
}
void DimSumPhotos::cancel() { ++m_generation;m_active=false;if(m_reply)m_reply->abort();m_reply.clear(); }
void DimSumPhotos::fail(const QString& reason) { if(!m_active)return;cancel();emit unavailable(reason); }
void DimSumPhotos::get(const QUrl& url,qint64 limit,Completion done,int redirects)
{
    if(!m_active)return;
    if(++m_requests>100 || redirects>3) { fail(QStringLiteral("Public photo discovery exceeded its request limit."));return; }
    const bool api=url.host()==QLatin1String("api.github.com") && url.path().startsWith(Repo);
    const bool raw=url.host()==QLatin1String("raw.githubusercontent.com") && url.path()==QLatin1String("/Ding-Ding-Projects/dim-sum-photos/main/catalog/index.json") && url.query().isEmpty();
    const bool redirected=redirects>0 && url.host()==QLatin1String("release-assets.githubusercontent.com");
    if(!cleanHttps(url) || (!api && !raw && !publicAssetUrl(url) && !redirected)) { fail(QStringLiteral("Public photo source URL was rejected."));return; }
    QNetworkRequest request(url);request.setRawHeader("Accept","application/vnd.github+json");request.setRawHeader("User-Agent","KeePassXC-DimSumPhotos");request.setTransferTimeout(10000);request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
    auto* reply=m_network->get(request);m_reply=reply;const auto generation=m_generation;auto data=QSharedPointer<QByteArray>::create();
    connect(reply,&QNetworkReply::readyRead,this,[this,reply,data,limit,generation] { if(generation!=m_generation || !m_active)return;const auto chunk=reply->read(qMax(qint64(0),limit+1-data->size()));data->append(chunk);m_totalBytes+=chunk.size();if(data->size()>limit || m_totalBytes>64*1024*1024)reply->abort(); });
    connect(reply,&QNetworkReply::finished,this,[this,reply,data,url,limit,done,redirects,generation] {
        reply->deleteLater();if(!m_active || generation!=m_generation)return;m_reply.clear();const auto tail=reply->read(qMax(qint64(0),limit+1-data->size()));data->append(tail);m_totalBytes+=tail.size();
        const int code=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if(code>=300 && code<400 && (publicAssetUrl(url) || redirects>0)) { get(url.resolved(reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl()),limit,done,redirects+1);return; }
        if(reply->error()!=QNetworkReply::NoError || code!=200 || data->size()>limit || m_totalBytes>64*1024*1024) { fail(QStringLiteral("Public photo source unavailable, offline, rate limited or over its size bound."));return; }
        done(*data);
    });
}
void DimSumPhotos::start()
{
    cancel();m_active=true;m_requests=0;m_totalBytes=0;m_assets.clear();m_releaseAssets.clear();m_dishes.clear();const auto generation=m_generation;
    QTimer::singleShot(45000,this,[this,generation] { if(m_active && generation==m_generation)fail(QStringLiteral("Public photo discovery timed out.")); });
    get(QUrl(QStringLiteral("https://api.github.com")+Repo+QStringLiteral("contents/catalog/index.json")),1024*1024,[this](const QByteArray& bytes) {
        const auto object=QJsonDocument::fromJson(bytes).object();m_revision=object.value("sha").toString();
        if(!QRegularExpression(QStringLiteral("^[a-f0-9]{40}$")).match(m_revision).hasMatch() || object.value("size").toDouble()>CatalogueLimit) { fail(QStringLiteral("Public catalogue revision is unavailable."));return; }
        get(QUrl(QStringLiteral("https://raw.githubusercontent.com/Ding-Ding-Projects/dim-sum-photos/main/catalog/index.json")),CatalogueLimit,[this](const QByteArray& catalogue) {
            const QByteArray blob=QByteArray("blob ")+QByteArray::number(catalogue.size())+QByteArray(1,'\0')+catalogue;
            if(QCryptographicHash::hash(blob,QCryptographicHash::Sha1).toHex()!=m_revision.toLatin1()) { fail(QStringLiteral("Public catalogue revision integrity check failed."));return; }
            m_dishes=parseCatalogue(catalogue);if(m_dishes.isEmpty()) { fail(QStringLiteral("Public catalogue schema is unsupported or invalid."));return; }releasePage(1);
        });
    });
}
void DimSumPhotos::releasePage(int page)
{
    if(page>20) { fail(QStringLiteral("Release pagination incomplete at configured limit."));return; }
    get(QUrl(QStringLiteral("https://api.github.com")+Repo+QStringLiteral("releases?per_page=10&page=%1").arg(page)),4*1024*1024,[this,page](const QByteArray& bytes) {
        QJsonParseError error;const auto doc=QJsonDocument::fromJson(bytes,&error);if(error.error!=QJsonParseError::NoError || !doc.isArray()) { fail(QStringLiteral("Release metadata is invalid."));return; }const auto rows=doc.array();if(rows.size()>10) { fail(QStringLiteral("Release page exceeded its requested count."));return; }
        for(const auto& value:rows) { const auto row=value.toObject();if(row.value("tag_name").toString().startsWith(QLatin1String("catalog-v1")) && !row.value("draft").toBool() && !row.value("prerelease").toBool()) { const QUrl url(row.value("assets_url").toString());if(url.host()!=QLatin1String("api.github.com") || !url.path().startsWith(Repo+QStringLiteral("releases/")) || !url.path().endsWith(QLatin1String("/assets"))) { fail(QStringLiteral("Release asset metadata URL is invalid."));return; }m_releaseAssets.append(url); } }
        if(rows.size()==10)releasePage(page+1);else if(m_releaseAssets.isEmpty())fail(QStringLiteral("No published public photo releases are available."));else assetPage(0,1);
    });
}
void DimSumPhotos::assetPage(int releaseIndex,int page)
{
    if(page>100 || m_assets.size()>10000) { fail(QStringLiteral("Asset pagination incomplete at configured limit."));return; }
    QUrl url=m_releaseAssets.at(releaseIndex);url.setQuery(QStringLiteral("per_page=100&page=%1").arg(page));
    get(url,4*1024*1024,[this,releaseIndex,page](const QByteArray& bytes) {
        QJsonParseError error;const auto doc=QJsonDocument::fromJson(bytes,&error);if(error.error!=QJsonParseError::NoError || !doc.isArray()) { fail(QStringLiteral("Asset metadata is invalid."));return; }const auto rows=doc.array();if(rows.size()>100) { fail(QStringLiteral("Asset page exceeded its requested count."));return; }for(const auto& value:rows)m_assets.append(value.toObject());
        if(m_assets.size()>10000) { fail(QStringLiteral("Public asset count exceeded configured limit."));return; }
        if(rows.size()==100)assetPage(releaseIndex,page+1);else if(releaseIndex+1<m_releaseAssets.size())assetPage(releaseIndex+1,1);else choosePhoto();
    });
}
void DimSumPhotos::choosePhoto()
{
    const auto resolution=resolveAssets(m_dishes,m_assets);auto eligible=resolution.photos;if(eligible.isEmpty()) { fail(QStringLiteral("Catalogue dishes have no published validated photo assets."));return; }
    auto photo=eligible.at(QRandomGenerator::global()->bounded(int(eligible.size())));photo.revision=m_revision;photo.coverage=QJsonObject{{"catalogueDishes",m_dishes.size()},{"publishedDishes",eligible.size()},{"unmatchedDishes",resolution.unmatched},{"ambiguousDishes",resolution.ambiguous},{"discoveredAssets",m_assets.size()}};
    get(QUrl(photo.url),PhotoLimit,[this,photo](const QByteArray& bytes) mutable {
        if(!validImage(bytes,photo,&photo.image)) { fail(QStringLiteral("Published photo failed byte, digest, PNG or dimension validation."));return; }
        if(!saveCache(photo,bytes)) { fail(QStringLiteral("Validated photo could not be cached atomically."));return; }
        m_active=false;emit ready(photo);
    });
}
}
