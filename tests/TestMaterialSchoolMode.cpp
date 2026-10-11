/* Copyright (C) 2026 KeePassXC Team <team@keepassxc.org>
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "gui/material/MaterialSchoolMode.h"
#include "gui/material/MaterialVoice.h"
#include "gui/material/MaterialSettingsScreen.h"
#include "gui/material/MaterialSettingsHub.h"
#include "gui/material/MaterialSearchBar.h"
#include "core/Config.h"
#include "crypto/Crypto.h"
#include <QTest>
#include <QTemporaryDir>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QPushButton>
#include <QSignalSpy>
#include <QScopedPointer>
#include <QFile>
#include <QStandardPaths>
#include <QUuid>
#include <QApplication>
#include "gui/material/MaterialDimSum.h"
using namespace Material;
class TestMaterialSchoolMode : public QObject {
    Q_OBJECT
private:
    QTemporaryDir m_profile;
    QScopedPointer<SchoolMode> m_mode;
    QString path() const {return m_profile.filePath(QStringLiteral("shared/school-mode.json"));}
    bool write(const QJsonObject& object) {
        QSaveFile file(path());const auto bytes=QJsonDocument(object).toJson(QJsonDocument::Compact);
        return file.open(QIODevice::WriteOnly)&&file.write(bytes)==bytes.size()&&file.commit();
    }
    QJsonObject read() const {QFile file(path());if(!file.open(QIODevice::ReadOnly))return {};return QJsonDocument::fromJson(file.readAll()).object();}
private slots:
    void initTestCase() {
        QVERIFY(m_profile.isValid());QVERIFY(Crypto::init());
        Config::createConfigFromFile(m_profile.filePath(QStringLiteral("roaming.ini")),m_profile.filePath(QStringLiteral("local.ini")));
        m_mode.reset(new SchoolMode(path()));SchoolMode::s_testInstance=m_mode.data();
        connect(m_mode.data(),&SchoolMode::changed,Voice::notifier(),&Voice::Notifier::announce);
    }
    void init(){QFile::remove(path());m_mode->reload();QVERIFY(m_mode->available());QVERIFY(!m_mode->restricted());}
    void cleanupTestCase(){SchoolMode::s_testInstance=nullptr;m_mode.reset();}
    void schemaRejectsMalformedAndBoundedInputs() {
        auto r=read();
        r={{"schemaVersion",1},{"name",QStringLiteral("Focus time")},{"enabled",false},{"revision",0}};
        QVERIFY(SchoolMode::validate(r));
        auto bad=r;bad["schemaVersion"]=2;QVERIFY(!SchoolMode::validate(bad));
        bad=r;bad["enabled"]=QStringLiteral("false");QVERIFY(!SchoolMode::validate(bad));
        bad=r;bad["revision"]=0.5;QVERIFY(!SchoolMode::validate(bad));
        bad=r;bad["name"]=QString(81,QLatin1Char('x'));QVERIFY(!SchoolMode::validate(bad));
        bad=r;bad["extension"]=QString(8192,QLatin1Char('x'));QVERIFY(!SchoolMode::validate(bad));
        bad=r;bad["enabled"]=true;QVERIFY(!SchoolMode::validate(bad));
        bad=r;bad["credential"]=QJsonArray{};QVERIFY(!SchoolMode::validate(bad));
        QJsonObject nested;for(int i=0;i<9;++i)nested=QJsonObject{{"child",nested}};
        bad=r;bad["extension"]=nested;QVERIFY(!SchoolMode::validate(bad));
        QVERIFY(m_mode->setCredential({},QByteArray("synthetic-fixture-only")));
        auto verifierRecord=read();auto verifier=verifierRecord.value("credential").toObject();
        verifier["memoryKiB"]=1048576;verifierRecord["credential"]=verifier;QVERIFY(!SchoolMode::validate(verifierRecord));
        verifierRecord=read();verifier=verifierRecord.value("credential").toObject();verifier["salt"]=verifier.value("salt").toString()+QLatin1Char('\n');verifierRecord["credential"]=verifier;QVERIFY(!SchoolMode::validate(verifierRecord));
    }
    void credentialRenameResetAndUnknownPreservation() {
        QVERIFY(!m_mode->enable());
        QVERIFY(m_mode->setCredential({},QByteArray("synthetic-fixture-only")));
        QVERIFY(!m_mode->rename(QStringLiteral("Focus time"),QByteArray("wrong-fixture")));
        QVERIFY(m_mode->rename(QStringLiteral("Focus time"),QByteArray("synthetic-fixture-only")));
        auto record=read();record["futureExtension"]=QJsonObject{{"retained",17}};
        auto verifier=record.value("credential").toObject();verifier["futureVerifierField"]=true;record["credential"]=verifier;QVERIFY(write(record));m_mode->reload();
        QVERIFY(m_mode->enable());QVERIFY(m_mode->restricted());
        QVERIFY(!m_mode->unlock(QByteArray("wrong-fixture")));QVERIFY(m_mode->restricted());
        QVERIFY(!m_mode->setCredential(QByteArray("wrong-fixture"),QByteArray("replacement-fixture-only")));
        QVERIFY(m_mode->setCredential(QByteArray("synthetic-fixture-only"),QByteArray("replacement-fixture-only")));
        QVERIFY(!m_mode->unlock(QByteArray("synthetic-fixture-only")));
        QVERIFY(m_mode->unlock(QByteArray("replacement-fixture-only")));QVERIFY(!m_mode->restricted());
        const auto after=read();QCOMPARE(after.value("futureExtension").toObject().value("retained").toInt(),17);
        QVERIFY(after.value("credential").toObject().value("futureVerifierField").toBool());
        QVERIFY(!after.contains("password"));QVERIFY(!after.contains("pin"));
        QCOMPARE(m_mode->name(),QStringLiteral("Focus time"));
        QVERIFY(QFile::remove(path()));m_mode->reload();QVERIFY(!m_mode->restricted());QVERIFY(!m_mode->hasCredential());
    }
    void liveReplacementRenameAndDeletion() {
        SchoolMode second(path());QSignalSpy changes(&second,&SchoolMode::changed);
        QVERIFY(m_mode->setCredential({},QByteArray("synthetic-fixture-only")));
        QTRY_VERIFY(second.hasCredential());
        QVERIFY(m_mode->rename(QStringLiteral("Quiet work"),QByteArray("synthetic-fixture-only")));
        QTRY_COMPARE(second.name(),QStringLiteral("Quiet work"));
        QVERIFY(m_mode->enable());QTRY_VERIFY(second.restricted());
        QVERIFY(second.unlock(QByteArray("synthetic-fixture-only")));QTRY_VERIFY(!m_mode->restricted());
        QVERIFY(changes.count()>0);
        QVERIFY(QFile::remove(path()));QTRY_VERIFY(!second.hasCredential());QVERIFY(!second.restricted());
    }
    void corruptionAndUnwatchableAreUnavailable() {
        QVERIFY(m_mode->rename(QStringLiteral("Quiet work"),{}));
        QFile file(path());QVERIFY(file.open(QIODevice::WriteOnly|QIODevice::Truncate));QCOMPARE(file.write("{"),qint64(1));file.close();m_mode->reload();
        QVERIFY(!m_mode->available());QVERIFY(m_mode->restricted());QCOMPARE(m_mode->name(),QStringLiteral("Quiet work"));QVERIFY(!m_mode->status().isEmpty());
        QFile parent(m_profile.filePath(QStringLiteral("not-a-directory")));QVERIFY(parent.open(QIODevice::WriteOnly));parent.close();
        SchoolMode unavailable(parent.fileName()+QStringLiteral("/state.json"));QVERIFY(!unavailable.available());QVERIFY(unavailable.restricted());
    }
    void effectiveVoicePreservesPreferencesAndSettingsOmitControls() {
        Voice::setLanguage(Voice::Language::Bilingual);Voice::setFunnyLevel(Voice::Language::English,5);Voice::setFunnyLevel(Voice::Language::Cantonese,4);
        SettingsScreen overview;SettingsHub hub;
        QVERIFY(overview.findChild<QWidget*>(QStringLiteral("sharedModeControl")));
        QVERIFY(overview.findChild<QWidget*>(QStringLiteral("appearanceLanguageAndTone")));
        QVERIFY(m_mode->setCredential({},QByteArray("synthetic-fixture-only")));
        QVERIFY(m_mode->rename(QStringLiteral("Quiet work"),QByteArray("synthetic-fixture-only")));
        QVERIFY(m_mode->enable());
        QVERIFY(!overview.findChild<QWidget*>(QStringLiteral("appearanceLanguageAndTone")));
        QVERIFY(DimSum::catalogue().isEmpty());
        QCOMPARE(Voice::language(),Voice::Language::English);QCOMPARE(Voice::funnyLevel(Voice::Language::English),1);
        Voice::setLanguage(Voice::Language::Cantonese);Voice::setFunnyLevel(Voice::Language::English,2);
        QCOMPARE(config()->get(Config::GUI_VoiceLanguage).toString(),QStringLiteral("Bilingual"));QCOMPARE(config()->get(Config::GUI_FunnyLevelEnglish).toInt(),5);
        auto* control=overview.findChild<SchoolModeControl*>();QVERIFY(control);QCOMPARE(control->accessibleName(),QStringLiteral("Quiet work"));
        QVERIFY(m_mode->unlock(QByteArray("synthetic-fixture-only")));
        QVERIFY(overview.findChild<QWidget*>(QStringLiteral("appearanceLanguageAndTone")));
        QCOMPARE(Voice::language(),Voice::Language::Bilingual);QCOMPARE(Voice::funnyLevel(Voice::Language::English),5);QCOMPARE(Voice::funnyLevel(Voice::Language::Cantonese),4);
    }
};
int main(int argc,char** argv) {
    QStandardPaths::setTestModeEnabled(true);
    const QString identity=QStringLiteral("TestMaterialSchoolMode-%1").arg(QUuid::createUuid().toString(QUuid::Id128));
    QCoreApplication::setOrganizationName(identity);QCoreApplication::setApplicationName(identity);
    qputenv("USERNAME",identity.toLatin1());qputenv("USER",identity.toLatin1());
    QApplication app(argc,argv);TestMaterialSchoolMode tests;return QTest::qExec(&tests,argc,argv);
}
#include "TestMaterialSchoolMode.moc"
