#include "core/Config.h"
#include "core/Database.h"
#include "crypto/Crypto.h"
#include "crypto/kdf/AesKdf.h"
#include "gui/DatabaseWidget.h"
#include "gui/DatabaseTabWidget.h"
#include "gui/MainWindow.h"
#include "gui/MessageWidget.h"
#include "keys/CompositeKey.h"
#include "keys/PasswordKey.h"
#include "gui/material/MaterialDimSum.h"
#include "gui/material/MaterialNotifier.h"
#include "gui/material/MaterialSettingsScreen.h"
#include "gui/material/MaterialVoice.h"
#include <QAbstractButton>

#include <QApplication>
#include <QDir>
#include <QLineEdit>
#include <QMenu>
#include <QRandomGenerator>
#include <QSettings>
#include <QStandardPaths>
#include "gui/material/MaterialDimSumPhotos.h"
#include <QBuffer>
#include <QFile>
#include <QDateTime>
#include <QImageReader>
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

using Material::DimSum;
using Material::DimSumCard;

class TestMaterialDimSum : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        QVERIFY(Crypto::init());
        m_originalCatalogue=DimSum::s_catalogue;
    }
    void init()
    {
        DimSum::resetLaunchState();
        DimSum::s_testCatalogue=true;
        DimSum::s_catalogue=[] { QImage image(8,8,QImage::Format_RGB32);image.fill(Qt::white);return QVector<DimSum::Dish>{{QStringLiteral("Synthetic dish"),QStringLiteral("測試點心"),QStringLiteral("https://github.com/Ding-Ding-Projects/dim-sum-photos/releases/download/catalog-v1/fixture.png"),image}}; };
        config()->set(Config::GUI_DimSumSurprise, true);
        config()->set(Config::GUI_MinimizeOnStartup, false);
        config()->set(Config::OpenPreviousDatabasesOnStartup, true);
        config()->set(Config::LastOpenedDatabases, QStringList());
        config()->set(Config::LastDatabases, QStringList{QStringLiteral("synthetic-history-only.kdbx")});
        config()->set(Config::LastActiveDatabase, QString());
        config()->sync();
        DimSum::s_quiet = [] { return false; };
        DimSum::s_random = [](quint32) { return 0; };
    }
    void cleanup()
    {
        qApp->removeEventFilter(this);
        DimSum::s_random = [](quint32 bound) { return QRandomGenerator::system()->bounded(bound); };
        DimSum::resetLaunchState();
        if (m_window) QVERIFY(m_window->findChild<DatabaseTabWidget*>()->closeAllDatabaseTabs());
    }
    void cleanupTestCase()
    {
        DimSum::s_catalogue=m_originalCatalogue;
        DimSum::s_testCatalogue=false;
        m_window.reset();
    }
    void drawBoundary_data()
    {
        QTest::addColumn<quint32>("value");
        for (quint32 value = 0; value < 10; ++value)
            QTest::newRow(qPrintable(QString::number(value))) << value;
    }
    void drawBoundary()
    {
        QFETCH(quint32, value);
        quint32 seenBound = 0;
        int calls = 0;
        DimSum::s_random = [&](quint32 bound) { seenBound = bound; ++calls; return value; };
        QCOMPARE(DimSum::shouldShow(), value == 0);
        QCOMPARE(seenBound, quint32(10));
        for (int count = 0; count < 20; ++count) QCOMPARE(DimSum::shouldShow(), value == 0);
        QCOMPARE(calls, 1);
    }
    void legacyOffPreferenceRejoinsDraw()
    {
        config()->set(Config::GUI_DimSumSurprise, false);
        config()->sync();
        Config::createConfigFromFile(qEnvironmentVariable("KPXC_CONFIG"), qEnvironmentVariable("KPXC_CONFIG_LOCAL"));
        QVERIFY(!config()->get(Config::GUI_DimSumSurprise).toBool());
        QVERIFY(DimSum::shouldShow());
        config()->sync();
        QSettings stored(config()->getFileName(), QSettings::IniFormat);
        QVERIFY(!stored.contains(QStringLiteral("GUI/DimSumSurprise")));
    }
    void restoredLaunch_data()
    {
        QTest::addColumn<bool>("restore");
        QTest::addColumn<QStringList>("opened");
        QTest::addColumn<QString>("active");
        QTest::addColumn<bool>("eligible");
        QTest::newRow("remembered-tab") << true << QStringList{"synthetic.kdbx"} << QString() << false;
        QTest::newRow("last-active-only") << true << QStringList{} << QString("synthetic.kdbx") << false;
        QTest::newRow("empty-records") << true << QStringList{QString()} << QString() << true;
        QTest::newRow("restoration-disabled") << false << QStringList{"synthetic.kdbx"}
                                               << QString("synthetic.kdbx") << true;
    }
    void restoredLaunch()
    {
        QFETCH(bool, restore);
        QFETCH(QStringList, opened);
        QFETCH(QString, active);
        QFETCH(bool, eligible);
        config()->set(Config::OpenPreviousDatabasesOnStartup, restore);
        config()->set(Config::LastOpenedDatabases, opened);
        config()->set(Config::LastActiveDatabase, active);
        int draws = 0;
        DimSum::s_random = [&](quint32) { ++draws; return 0; };
        DimSum::beginStartup();
        QCOMPARE(DimSum::shouldShow(), eligible);
        QCOMPARE(draws, eligible ? 1 : 0);
        // Finishing or cancelling restoration must not make a later task eligible.
        config()->set(Config::LastOpenedDatabases, QStringList());
        config()->set(Config::LastActiveDatabase, QString());
        QCOMPARE(DimSum::shouldShow(), eligible);
    }
    void visibleMessageSeverityChange_data()
    {
        QTest::addColumn<int>("severity");
        QTest::addColumn<bool>("excluded");
        QTest::newRow("error") << int(MessageWidget::Error) << true;
        QTest::newRow("warning") << int(MessageWidget::Warning) << true;
        QTest::newRow("information") << int(MessageWidget::Information) << false;
        QTest::newRow("positive") << int(MessageWidget::Positive) << false;
    }
    void visibleMessageSeverityChange()
    {
        QFETCH(int, severity);
        QFETCH(bool, excluded);
        QWidget host;
        MessageWidget message(&host);
        message.setAnimate(false);
        message.showMessage(QStringLiteral("Synthetic information"), MessageWidget::Information, -1);
        host.resize(640, 480);
        host.show();
        host.activateWindow();
        QTRY_VERIFY(host.isActiveWindow());
        QVERIFY(message.isVisible());
        QVERIFY(DimSum::showNow(&host));
        QPointer<DimSumCard> card = host.findChild<DimSumCard*>();
        QVERIFY(card && card->isVisible());
        message.showMessage(QStringLiteral("Synthetic replacement"),
                            static_cast<MessageWidget::MessageType>(severity), -1);
        QVERIFY(message.isVisible());
        QCOMPARE(card && card->isVisible(), !excluded);
        if (excluded) {
            QTRY_VERIFY(card.isNull());
            QVERIFY(!DimSum::shouldShow());
        }
    }
    void legacyPreferenceCannotCancelWinningDraw()
    {
        QVERIFY(DimSum::shouldShow());
        config()->set(Config::GUI_DimSumSurprise, false);
        QVERIFY(DimSum::shouldShow());
    }

    void firstRunDoesNotDraw()
    {
        config()->set(Config::LastDatabases, QStringList());
        int calls = 0;
        DimSum::s_random = [&](quint32) { ++calls; return 0; };
        QVERIFY(!DimSum::shouldShow());
        QCOMPARE(calls, 0);
    }
    void quietDoesNotDraw()
    {
        DimSum::s_quiet = [] { return true; };
        int calls = 0;
        DimSum::s_random = [&](quint32) { ++calls; return 0; };
        QVERIFY(!DimSum::shouldShow());
        QCOMPARE(calls, 0);
    }
    void firstRunCannotBecomeEligibleMidLaunch()
    {
        config()->set(Config::LastDatabases, QStringList());
        DimSum::beginStartup();
        config()->set(Config::LastDatabases, QStringList{QStringLiteral("synthetic-history-only.kdbx")});
        QVERIFY(!DimSum::shouldShow());
    }
    void settingsHasNoOptOut()
    {
        Material::SettingsScreen settings;
        QVERIFY(!settings.findChild<QAbstractButton*>(QStringLiteral("dimSumSurpriseToggle")));
    }
    void legacyPreferenceCannotDismissVisibleCard()
    {
        QWidget host;
        host.show();
        host.activateWindow();
        QTRY_VERIFY(host.isActiveWindow());
        QVERIFY(DimSum::showNow(&host));
        QPointer<DimSumCard> card = host.findChild<DimSumCard*>();
        config()->set(Config::GUI_DimSumSurprise, false);
        QVERIFY(card && card->isVisible());
        DimSum::suppress();
        QTRY_VERIFY(card.isNull());
    }
    void quietAtPresentationCancelsWithoutRetry()
    {
        QWidget host;
        host.show();
        host.activateWindow();
        QTRY_VERIFY(host.isActiveWindow());
        DimSum::showIfDue(&host);
        DimSum::s_quiet = [] { return true; };
        QTest::qWait(1600);
        QVERIFY(!DimSum::hasShown());
        DimSum::s_quiet = [] { return false; };
        DimSum::showIfDue(&host);
        QTest::qWait(1600);
        QVERIFY(!DimSum::hasShown());
    }
    void suppressionOverridesWinningDraw()
    {
        QVERIFY(DimSum::shouldShow());
        DimSum::suppress();
        QVERIFY(!DimSum::shouldShow());
    }
    void errorButNotInformationSuppresses()
    {
        Material::Notify::info(QStringLiteral("Synthetic information"));
        QVERIFY(DimSum::shouldShow());
        Material::Notify::error(QStringLiteral("Synthetic error"));
        QVERIFY(!DimSum::shouldShow());
    }
    void interactionCancelsPendingPresentation()
    {
        QWidget host;
        QLineEdit edit(&host);
        host.resize(640, 480);
        host.show();
        host.activateWindow();
        QTRY_VERIFY(host.isActiveWindow());
        edit.setFocus();
        DimSum::showIfDue(&host);
        QTest::keyClick(&edit, Qt::Key_A);
        QTest::qWait(1600);
        QVERIFY(!DimSum::hasShown());
        QVERIFY(host.findChildren<DimSumCard*>().isEmpty());
        QVERIFY(!DimSum::shouldShow());
    }
    void popupPreventsPresentation()
    {
        QWidget host;
        host.show();
        host.activateWindow();
        QTRY_VERIFY(host.isActiveWindow());
        QMenu popup(&host);
        popup.addAction(QStringLiteral("Synthetic action"));
        popup.popup(QPoint(20, 20));
        QVERIFY(QApplication::activePopupWidget());
        QVERIFY(!DimSum::showNow(&host));
    }
    void cardDoesNotStealFocusAndExpiresOnce()
    {
        QWidget host;
        QLineEdit edit(&host);
        host.resize(640, 480);
        host.show();
        host.activateWindow();
        edit.setFocus();
        QTRY_VERIFY(edit.hasFocus());
        QVERIFY(DimSum::showNow(&host));
        QCOMPARE(QApplication::focusWidget(), &edit);
        QVERIFY(!DimSum::showNow(&host));
        QPointer<DimSumCard> card = host.findChild<DimSumCard*>();
        QVERIFY(card);
        QCOMPARE(card->focusPolicy(), Qt::NoFocus);
        auto* hold = card->findChild<QTimer*>();
        QVERIFY(hold);
        QCOMPARE(hold->interval(), 6000);
        QTRY_VERIFY_WITH_TIMEOUT(hold->isActive(), 1000);
        QVERIFY(QMetaObject::invokeMethod(hold, "timeout", Qt::DirectConnection));
        QTRY_VERIFY_WITH_TIMEOUT(card.isNull(), 1000);
        QVERIFY(!DimSum::showNow(&host));
    }
    void pendingHostCanBeDestroyed()
    {
        auto* host = new QWidget;
        host->show();
        host->activateWindow();
        QTRY_VERIFY(host->isActiveWindow());
        DimSum::showIfDue(host);
        delete host;
        QTest::qWait(1600);
        QVERIFY(!DimSum::hasShown());
    }
    void publicPhotosAreNamedAndNoLegacyFallback()
    {
        QVERIFY(!DimSum::catalogue().isEmpty());
        for (const auto& dish : DimSum::catalogue()) {
            QVERIFY(Material::DimSumPhotos::publicAssetUrl(QUrl(dish.asset)));
            QVERIFY(!dish.image.isNull());
            QVERIFY(dish.displayName().contains(dish.english));
            QVERIFY(dish.displayName().contains(dish.cantonese));
        }
    }
    void providerRejectsMissingAndAmbiguousAssets()
    {
        using Photos=Material::DimSumPhotos;
        const QJsonObject row{{"name",QJsonObject{{"en","Fixture"},{"zhHant",QStringLiteral("測試")}}},{"image",QJsonObject{{"path","images/fixture.png"}}}};
        const QJsonObject catalogue{{"schemaVersion","1.0.0"},{"total",1},{"imageSpecification",QJsonObject{{"format","png"},{"minimumWidth",1024},{"minimumHeight",1024}}},{"dishes",QJsonArray{row}}};
        const auto dishes=Photos::parseCatalogue(QJsonDocument(catalogue).toJson());QCOMPARE(dishes.size(),1);
        QVERIFY(Photos::joinAssets(dishes,{}).isEmpty());
        QCOMPARE(Photos::resolveAssets(dishes,{}).unmatched,1);
        QJsonObject asset{{"name","fixture.png"},{"state","uploaded"},{"size",100},{"browser_download_url","https://github.com/Ding-Ding-Projects/dim-sum-photos/releases/download/catalog-v1/fixture.png"},{"digest",QStringLiteral("sha256:")+QString(64,QLatin1Char('a'))}};
        QCOMPARE(Photos::joinAssets(dishes,{asset}).size(),1);
        auto conflicting=asset;conflicting.insert("digest",QStringLiteral("sha256:")+QString(64,QLatin1Char('b')));
        QVERIFY(Photos::joinAssets(dishes,{asset,conflicting}).isEmpty());
        QCOMPARE(Photos::resolveAssets(dishes,{asset,conflicting}).ambiguous,1);
        asset.insert("state","starter");QVERIFY(Photos::joinAssets(dishes,{asset}).isEmpty());
        QVERIFY(!Photos::publicAssetUrl(QUrl(QStringLiteral("https://example.com/fixture.png"))));
        QVERIFY(!Photos::publicAssetUrl(QUrl(QStringLiteral("https://github.com/Ding-Ding-Projects/dim-sum-catalog-archive/releases/download/images-v1/fixture.png"))));
        auto incomplete=catalogue;incomplete.insert("total",2);QVERIFY(Photos::parseCatalogue(QJsonDocument(incomplete).toJson()).isEmpty());
    }
    void providerValidatesPngDigestDimensionsAndNoCacheFallback()
    {
        using Photos=Material::DimSumPhotos;
        QImage fixture(1024,1024,QImage::Format_RGB32);fixture.fill(Qt::white);
        QByteArray bytes;QBuffer buffer(&bytes);QVERIFY(buffer.open(QIODevice::WriteOnly));QVERIFY(fixture.save(&buffer,"PNG"));
        Photos::Photo photo;photo.bytes=bytes.size();photo.digest=QStringLiteral("sha256:")+QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex());
        const int decoderBudget=QImageReader::allocationLimit();
        QImage result;QVERIFY(Photos::validImage(bytes,photo,&result));QCOMPARE(result.size(),fixture.size());
        QCOMPARE(QImageReader::allocationLimit(),decoderBudget);
        QVERIFY(result.sizeInBytes()<=64*1024*1024);
        auto wrong=photo;wrong.digest=QStringLiteral("sha256:")+QString(64,QLatin1Char('0'));QVERIFY(!Photos::validImage(bytes,wrong,nullptr));
        wrong=photo;wrong.minimumWidth=2048;QVERIFY(!Photos::validImage(bytes,wrong,nullptr));
        const QByteArray svg("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"1024\" height=\"1024\"/>");
        wrong=photo;wrong.bytes=svg.size();wrong.digest=QStringLiteral("sha256:")+QString::fromLatin1(QCryptographicHash::hash(svg,QCryptographicHash::Sha256).toHex());
        QVERIFY(!Photos::validImage(svg,wrong,nullptr));
        QTemporaryDir emptyCache;QVERIFY(emptyCache.isValid());QVERIFY(Photos::cachedPhoto(emptyCache.path()).image.isNull());
        DimSum::s_catalogue=[] { return QVector<DimSum::Dish>{}; };
        QVERIFY(!DimSum::showNow(nullptr));QVERIFY(!DimSum::hasShown());
    }
    void providerCacheRetainsDimensionsAndCoverage()
    {
        using Photos=Material::DimSumPhotos;
        QTemporaryDir temporary;QVERIFY(temporary.isValid());Photos provider(nullptr,temporary.path());
        QImage fixture(1024,1024,QImage::Format_RGB32);fixture.fill(Qt::white);
        QByteArray bytes;QBuffer buffer(&bytes);QVERIFY(buffer.open(QIODevice::WriteOnly));QVERIFY(fixture.save(&buffer,"PNG"));
        Photos::Photo photo;photo.english=QStringLiteral("Fixture");photo.cantonese=QStringLiteral("測試");photo.filename=QStringLiteral("fixture.png");photo.url=QStringLiteral("https://github.com/Ding-Ding-Projects/dim-sum-photos/releases/download/catalog-v1/fixture.png");photo.bytes=bytes.size();photo.digest=QStringLiteral("sha256:")+QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex());
        QVERIFY(Photos::validImage(bytes,photo,&photo.image));
        provider.m_revision=QString(40,QLatin1Char('a'));provider.m_dishes={photo};provider.m_assets={QJsonObject{{"name",photo.filename},{"state","uploaded"},{"size",photo.bytes},{"digest",photo.digest},{"browser_download_url",photo.url}}};
        QVERIFY(provider.saveCache(photo,bytes));
        auto cached=Photos::cachedPhoto(temporary.path());QVERIFY(!cached.image.isNull());QCOMPARE(cached.minimumWidth,1024);QCOMPARE(cached.minimumHeight,1024);QVERIFY(cached.officialDigestAvailable);
        QCOMPARE(cached.coverage.value("publishedDishes").toInt(),1);QCOMPARE(cached.coverage.value("unmatchedDishes").toInt(),0);
        QFile receipt(temporary.path()+QStringLiteral("/receipt.json"));QVERIFY(receipt.open(QIODevice::ReadOnly));const auto original=QJsonDocument::fromJson(receipt.readAll()).object();receipt.close();
        auto store=[&receipt](const QJsonObject& object) { QVERIFY(receipt.open(QIODevice::WriteOnly|QIODevice::Truncate));receipt.write(QJsonDocument(object).toJson());receipt.close(); };
        for(const auto& key:{QStringLiteral("minimumWidth"),QStringLiteral("minimumHeight"),QStringLiteral("discoveredAssets"),QStringLiteral("publishedDishes"),QStringLiteral("completeAssetDiscovery")}) {
            auto missing=original;missing.remove(key);store(missing);QVERIFY(Photos::cachedPhoto(temporary.path()).image.isNull());
        }
        auto changed=original;changed.insert("minimumWidth",2048);store(changed);QVERIFY(Photos::cachedPhoto(temporary.path()).image.isNull());
        changed=original;changed.insert("officialDigest",QStringLiteral("sha256:")+QString(64,QLatin1Char('0')));store(changed);QVERIFY(Photos::cachedPhoto(temporary.path()).image.isNull());
        changed=original;changed.insert("publishedDishes",2);store(changed);QVERIFY(Photos::cachedPhoto(temporary.path()).image.isNull());
        changed=original;changed.insert("english",QString(257,QLatin1Char('x')));store(changed);QVERIFY(Photos::cachedPhoto(temporary.path()).image.isNull());
        changed=original;changed.insert("retrievedAt",QDateTime::currentDateTimeUtc().addSecs(-25*60*60).toString(Qt::ISODate));store(changed);QVERIFY(Photos::cachedPhoto(temporary.path()).image.isNull());
        changed=original;changed.insert("officialDigest",QString());store(changed);cached=Photos::cachedPhoto(temporary.path());QVERIFY(!cached.image.isNull());QVERIFY(!cached.officialDigestAvailable);
        store(original);QVERIFY(!Photos::cachedPhoto(temporary.path()).image.isNull());
    }
    void programmaticOpenCancelsPendingCard()
    {
        programmaticOpen(false);
    }
    void programmaticOpenHidesVisibleCard()
    {
        programmaticOpen(true);
    }
private:
    std::function<QVector<DimSum::Dish>()> m_originalCatalogue;
    QScopedPointer<MainWindow> m_window;
    QStringList m_excludedEvents;
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (event->type() == QEvent::KeyPress || event->type() == QEvent::MouseButtonPress
            || event->type() == QEvent::Wheel || event->type() == QEvent::TouchBegin) {
            m_excludedEvents.append(QString::number(event->type()));
        } else if (event->type() == QEvent::Show) {
            if (auto* widget = qobject_cast<QWidget*>(watched)) {
                auto* message = qobject_cast<KMessageWidget*>(widget);
                if (widget->isModal() || widget->windowType() == Qt::Popup
                    || (message && message->messageType() == KMessageWidget::Error)) {
                    m_excludedEvents.append(QString::fromLatin1(widget->metaObject()->className())
                                            + QStringLiteral(":") + widget->objectName());
                }
            }
        }
        return false;
    }
    void programmaticOpen(bool alreadyVisible)
    {
        QTemporaryDir databaseDirectory(QDir::tempPath() + QStringLiteral("/kds-db-XXXXXX"));
        QVERIFY(databaseDirectory.isValid());
        const auto path = databaseDirectory.filePath(QStringLiteral("synthetic-locked.kdbx"));
        {
            Database fixture;
            auto kdf = QSharedPointer<AesKdf>::create();
            kdf->setRounds(1);
            fixture.setKdf(kdf);
            auto key = QSharedPointer<CompositeKey>::create();
            key->addKey(QSharedPointer<PasswordKey>::create(QStringLiteral("synthetic-test-only")));
            QVERIFY(fixture.setKey(key));
            QString error;
            QVERIFY2(fixture.saveAs(path, Database::DirectWrite, {}, &error), qPrintable(error));
        }
        config()->set(Config::GUI_CheckForUpdates, false);
        config()->set(Config::GUI_VoiceDisclosureShown, true);
        config()->set(Config::Browser_Enabled, false);
        config()->set(Config::SSHAgent_Enabled, false);
        config()->set(Config::GUI_ShowTrayIcon, false);
        config()->set(Config::GUI_AllowScreenCapture, true);
        config()->set(Config::GlobalAutoTypeKey, 0);
        config()->set(Config::GlobalAutoTypeModifiers, 0);
        config()->set(Config::Security_LockDatabaseIdle, false);
        if (!m_window) m_window.reset(new MainWindow);
        auto& window = *m_window;
        // The service was constructed disabled. Changing this isolated preference
        // does not start it, but avoids a hidden settings-page warning masking
        // the credential-flow cancellation under test.
        config()->set(Config::Browser_Enabled, true);
        window.resize(1024, 768);
        window.show();
        window.activateWindow();
        QTRY_VERIFY(window.isActiveWindow());
        QVERIFY(window.getOpenDatabases().isEmpty());
        QVERIFY(DimSum::shouldShow());
        QPointer<DimSumCard> card;
        if (alreadyVisible) {
            QVERIFY(DimSum::showNow(&window));
            card = window.findChild<DimSumCard*>();
            QVERIFY(card && card->isVisible());
        } else {
            DimSum::showIfDue(&window);
        }
        // This is the same one-argument slot used by Application::openFile.
        // Queue it without keyboard, pointer, modal or single-instance IPC.
        m_excludedEvents.clear();
        qApp->installEventFilter(this);
        QVERIFY(QMetaObject::invokeMethod(&window, "openDatabase", Qt::QueuedConnection, Q_ARG(QString, path)));
        QTRY_COMPARE(window.getOpenDatabases().size(), 1);
        QVERIFY2(m_excludedEvents.isEmpty(), qPrintable(m_excludedEvents.join(QStringLiteral(", "))));
        QVERIFY(window.getOpenDatabases().first()->isLocked());
        QVERIFY(!QApplication::activeModalWidget());
        QVERIFY(!QApplication::activePopupWidget());
        if (alreadyVisible) {
            QVERIFY(!card || !card->isVisible());
            QTRY_VERIFY(card.isNull());
        } else {
            QVERIFY(!DimSum::shouldShow());
            QTest::qWait(1600);
            QVERIFY(!DimSum::hasShown());
            QVERIFY(window.findChildren<DimSumCard*>().isEmpty());
        }
        QVERIFY(!DimSum::shouldShow());
    }
};

int main(int argc, char** argv)
{
    QStandardPaths::setTestModeEnabled(true);
    QTemporaryDir isolated(QDir::tempPath() + QStringLiteral("/kds-XXXXXX"));
    if (!isolated.isValid()) return 2;
    qputenv("KPXC_CONFIG", (isolated.path() + QStringLiteral("/settings.ini")).toUtf8());
    qputenv("KPXC_CONFIG_LOCAL", (isolated.path() + QStringLiteral("/local.ini")).toUtf8());
    qputenv("USERNAME", "dim-sum-test");
    qputenv("USER", "dim-sum-test");
    const auto identity = QStringLiteral("KeePassXC-DimSum-Tests-") + QDir(isolated.path()).dirName();
    QCoreApplication::setOrganizationName(identity);
    QCoreApplication::setApplicationName(identity);
    QApplication application(argc, argv);
    TestMaterialDimSum test;
    return QTest::qExec(&test, argc, argv);
}

#include "TestMaterialDimSum.moc"
