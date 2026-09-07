/*
 *  Copyright (C) 2026 KeePassXC Team <team@keepassxc.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 or (at your option)
 *  version 3 of the License.
 */

#include "TestApplicationLogo.h"

#include "core/Config.h"
#include "gui/Icons.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QTemporaryDir>
#include <QTest>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

QTEST_MAIN(TestApplicationLogo)

namespace
{
    QString writeFixture(QTemporaryDir& directory, const QString& name, const QSize& size = {32, 16})
    {
        const auto path = directory.filePath(name);
        QImage image(size, QImage::Format_ARGB32_Premultiplied);
        image.fill(QColor(12, 34, 56, 128));
        if (!image.save(path, "PNG")) {
            return {};
        }
        return path;
    }
}

void TestApplicationLogo::initTestCase()
{
    QVERIFY(m_configDirectory.isValid());
    Config::createConfigFromFile(m_configDirectory.filePath(QStringLiteral("config.ini")),
                                 m_configDirectory.filePath(QStringLiteral("local.ini")));
    Icons::setApplicationLogoCacheDirectoryForTests(m_configDirectory.filePath(QStringLiteral("default-logos")));
}

void TestApplicationLogo::cleanupTestCase()
{
    config()->sync();
    Icons::setApplicationLogoCacheDirectoryForTests({});
}

void TestApplicationLogo::cleanup()
{
    Icons::setApplicationLogoFailureStageForTests(0);
    icons()->resetApplicationLogo();
    Icons::setApplicationLogoCacheDirectoryForTests(m_configDirectory.filePath(QStringLiteral("default-logos")));
    config()->set(Config::GUI_CustomLogoEnabled, false);
    config()->set(Config::GUI_CustomLogoFitMode, QStringLiteral("fit"));
    config()->set(Config::GUI_CustomLogoBackground, QStringLiteral("#00000000"));
}

void TestApplicationLogo::importsValidatedLocalImageAndPersistsOnlyDerivedPath()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    Icons::setApplicationLogoCacheDirectoryForTests(directory.filePath(QStringLiteral("private-logos")));
    const auto source = writeFixture(directory, QStringLiteral("neutral-fixture.png"));
    QString error;

    QVERIFY2(icons()->importApplicationLogo(source, &error), qPrintable(error));
    QVERIFY(icons()->hasCustomApplicationLogo());
    QVERIFY(QFile::exists(icons()->applicationLogoPath()));
    QVERIFY(!config()->get(Config::GUI_CustomLogoEnabled).toString().contains(source));
    QVERIFY(!config()->get(Config::GUI_CustomLogoFitMode).toString().contains(source));
    QVERIFY(!icons()->applicationIcon().isNull());
}

void TestApplicationLogo::rejectsInvalidAndOversizedSourcesWithoutReplacingActiveLogo()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    Icons::setApplicationLogoCacheDirectoryForTests(directory.filePath(QStringLiteral("private-logos")));
    const auto source = writeFixture(directory, QStringLiteral("neutral-fixture.png"));
    QString error;
    QVERIFY2(icons()->importApplicationLogo(source, &error), qPrintable(error));
    const auto activePath = icons()->applicationLogoPath();
    QFile activeFile(activePath);
    QVERIFY(activeFile.open(QIODevice::ReadOnly));
    const auto activeData = activeFile.readAll();
    activeFile.close();

    const auto malformed = directory.filePath(QStringLiteral("wrong-extension.png"));
    QFile malformedFile(malformed);
    QVERIFY(malformedFile.open(QIODevice::WriteOnly));
    malformedFile.write("not an image");
    malformedFile.close();
    QVERIFY(!icons()->importApplicationLogo(malformed, &error));
    QVERIFY(icons()->hasCustomApplicationLogo());

    const auto oversized = directory.filePath(QStringLiteral("too-large.png"));
    QFile oversizedFile(oversized);
    QVERIFY(oversizedFile.open(QIODevice::WriteOnly));
    QVERIFY(oversizedFile.resize(5 * 1024 * 1024 + 1));
    oversizedFile.close();
    QVERIFY(!icons()->importApplicationLogo(oversized, &error));
    QVERIFY(icons()->hasCustomApplicationLogo());
    QVERIFY(activeFile.open(QIODevice::ReadOnly));
    QCOMPARE(activeFile.readAll(), activeData);
}

void TestApplicationLogo::fitAndBackgroundRegenerateThenReset()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    Icons::setApplicationLogoCacheDirectoryForTests(directory.filePath(QStringLiteral("private-logos")));
    QString error;
    QVERIFY2(icons()->importApplicationLogo(writeFixture(directory, QStringLiteral("wide.png"), {64, 16}), &error), qPrintable(error));

    QVERIFY2(icons()->setApplicationLogoPresentation(QStringLiteral("crop"), QColor(QStringLiteral("#ff112233")), &error), qPrintable(error));
    QImage derived(icons()->applicationLogoPath());
    QVERIFY(!derived.isNull());
    QCOMPARE(derived.width(), derived.height());
    QCOMPARE(config()->get(Config::GUI_CustomLogoFitMode).toString(), QStringLiteral("crop"));

    QVERIFY2(icons()->resetApplicationLogo(&error), qPrintable(error));
    QVERIFY(!icons()->hasCustomApplicationLogo());
    QVERIFY(!QFile::exists(icons()->applicationLogoPath()));
}

void TestApplicationLogo::secondWriteFailureKeepsPriorLogoAndSettings()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    Icons::setApplicationLogoCacheDirectoryForTests(directory.filePath(QStringLiteral("private-logos")));
    QString error;
    QVERIFY2(icons()->importApplicationLogo(writeFixture(directory, QStringLiteral("old.png")), &error), qPrintable(error));
    const auto oldDisplay = QImage(icons()->applicationLogoPath());
    const auto oldFit = config()->get(Config::GUI_CustomLogoFitMode).toString();
    Icons::setApplicationLogoFailureStageForTests(2);
    QVERIFY(!icons()->importApplicationLogo(writeFixture(directory, QStringLiteral("new.png"), {48, 24}), &error));
    QVERIFY(icons()->hasCustomApplicationLogo());
    QVERIFY(QImage(icons()->applicationLogoPath()) == oldDisplay);
    QCOMPARE(config()->get(Config::GUI_CustomLogoFitMode).toString(), oldFit);
}

void TestApplicationLogo::presentationFailureKeepsPriorSettings()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    Icons::setApplicationLogoCacheDirectoryForTests(directory.filePath(QStringLiteral("private-logos")));
    QString error;
    QVERIFY2(icons()->importApplicationLogo(writeFixture(directory, QStringLiteral("old.png")), &error), qPrintable(error));
    const auto oldFit = config()->get(Config::GUI_CustomLogoFitMode).toString();
    const auto oldBackground = config()->get(Config::GUI_CustomLogoBackground).toString();
    Icons::setApplicationLogoFailureStageForTests(4);
    QVERIFY(!icons()->setApplicationLogoPresentation(QStringLiteral("crop"), QColor(QStringLiteral("#ff112233")), &error));
    QCOMPARE(config()->get(Config::GUI_CustomLogoFitMode).toString(), oldFit);
    QCOMPARE(config()->get(Config::GUI_CustomLogoBackground).toString(), oldBackground);
}

void TestApplicationLogo::resetFailureKeepsActiveLogo()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    Icons::setApplicationLogoCacheDirectoryForTests(directory.filePath(QStringLiteral("private-logos")));
    QString error;
    QVERIFY2(icons()->importApplicationLogo(writeFixture(directory, QStringLiteral("old.png")), &error), qPrintable(error));
    Icons::setApplicationLogoFailureStageForTests(3);
    QVERIFY(!icons()->resetApplicationLogo(&error));
    QVERIFY(icons()->hasCustomApplicationLogo());
    QVERIFY(QFile::exists(icons()->applicationLogoPath()));
}

void TestApplicationLogo::secondDeleteFailureReportsResidualDataAndCanRetry()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    Icons::setApplicationLogoCacheDirectoryForTests(directory.filePath(QStringLiteral("private-logos")));
    QString error;
    QVERIFY2(icons()->importApplicationLogo(writeFixture(directory, QStringLiteral("old.png")), &error), qPrintable(error));
    Icons::setApplicationLogoFailureStageForTests(5);
    QVERIFY(!icons()->resetApplicationLogo(&error));
    QVERIFY(!config()->get(Config::GUI_CustomLogoEnabled).toBool());
    QVERIFY(!QFile::exists(icons()->applicationLogoPath()));
    QVERIFY(QFile::exists(icons()->applicationLogoPath() + QStringLiteral(".removing")));
    Icons::setApplicationLogoFailureStageForTests(0);
    QVERIFY2(icons()->resetApplicationLogo(&error), qPrintable(error));
    QVERIFY(!QFile::exists(icons()->applicationLogoPath() + QStringLiteral(".removing")));
}

void TestApplicationLogo::linkedCacheDirectoryIsRefusedWithoutTouchingExternalTarget()
{
#ifdef Q_OS_WIN
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto external = directory.filePath(QStringLiteral("external"));
    const auto linked = directory.filePath(QStringLiteral("linked-cache"));
    QVERIFY(QDir().mkpath(external));
    if (!CreateSymbolicLinkW(reinterpret_cast<LPCWSTR>(linked.utf16()), reinterpret_cast<LPCWSTR>(external.utf16()),
                             SYMBOLIC_LINK_FLAG_DIRECTORY | SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE)) {
        QSKIP("The test account cannot create a directory link.");
    }
    Icons::setApplicationLogoCacheDirectoryForTests(linked);
    QString error;
    QVERIFY(!icons()->importApplicationLogo(writeFixture(directory, QStringLiteral("neutral.png")), &error));
    QVERIFY(QDir(external).entryList(QDir::Files | QDir::NoDotAndDotDot).isEmpty());
#else
    QSKIP("The reparse-point regression is specific to Windows.");
#endif
}

void TestApplicationLogo::linkedActiveEntryIsRefusedWithoutTouchingExternalTarget()
{
#ifdef Q_OS_WIN
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    Icons::setApplicationLogoCacheDirectoryForTests(directory.filePath(QStringLiteral("private-logos")));
    QString error;
    QVERIFY2(icons()->importApplicationLogo(writeFixture(directory, QStringLiteral("old.png")), &error), qPrintable(error));
    const auto external = writeFixture(directory, QStringLiteral("external.png"));
    QFile externalFile(external);
    QVERIFY(externalFile.open(QIODevice::ReadOnly));
    const auto externalBytes = externalFile.readAll();
    externalFile.close();
    QVERIFY(QFile::remove(icons()->applicationLogoPath()));
    if (!CreateSymbolicLinkW(reinterpret_cast<LPCWSTR>(icons()->applicationLogoPath().utf16()),
                             reinterpret_cast<LPCWSTR>(external.utf16()), SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE)) {
        QSKIP("The test account cannot create a file link.");
    }
    QVERIFY(!icons()->hasCustomApplicationLogo());
    QVERIFY(!icons()->resetApplicationLogo(&error));
    QVERIFY(externalFile.open(QIODevice::ReadOnly));
    QCOMPARE(externalFile.readAll(), externalBytes);
#else
    QSKIP("The reparse-point regression is specific to Windows.");
#endif
}

void TestApplicationLogo::danglingActiveLinkIsRefused()
{
#ifdef Q_OS_WIN
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    Icons::setApplicationLogoCacheDirectoryForTests(directory.filePath(QStringLiteral("private-logos")));
    QString error;
    QVERIFY2(icons()->importApplicationLogo(writeFixture(directory, QStringLiteral("old.png")), &error), qPrintable(error));
    QVERIFY(QFile::remove(icons()->applicationLogoPath()));
    const auto missing = directory.filePath(QStringLiteral("does-not-exist.png"));
    if (!CreateSymbolicLinkW(reinterpret_cast<LPCWSTR>(icons()->applicationLogoPath().utf16()),
                             reinterpret_cast<LPCWSTR>(missing.utf16()), SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE)) {
        QSKIP("The test account cannot create a file link.");
    }
    QVERIFY(!icons()->hasCustomApplicationLogo());
    QVERIFY(!icons()->resetApplicationLogo(&error));
#else
    QSKIP("The reparse-point regression is specific to Windows.");
#endif
}

void TestApplicationLogo::activationCleanupFailureIsReportedAndRetryable()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    Icons::setApplicationLogoCacheDirectoryForTests(directory.filePath(QStringLiteral("private-logos")));
    QString error;
    QVERIFY2(icons()->importApplicationLogo(writeFixture(directory, QStringLiteral("old.png")), &error), qPrintable(error));
    Icons::setApplicationLogoFailureStageForTests(6);
    QVERIFY(icons()->importApplicationLogo(writeFixture(directory, QStringLiteral("new.png"), {48, 24}), &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(icons()->hasCustomApplicationLogo());
    QVERIFY(QFile::exists(icons()->applicationLogoPath() + QStringLiteral(".previous")));
    Icons::setApplicationLogoFailureStageForTests(0);
    QVERIFY2(icons()->importApplicationLogo(writeFixture(directory, QStringLiteral("retry.png")), &error), qPrintable(error));
    QVERIFY(!QFile::exists(icons()->applicationLogoPath() + QStringLiteral(".previous")));
}

void TestApplicationLogo::firstActivationCleanupWarningCommitsEnabledState()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    Icons::setApplicationLogoCacheDirectoryForTests(directory.filePath(QStringLiteral("private-logos")));
    Icons::setApplicationLogoFailureStageForTests(6);
    QString error;
    QVERIFY(icons()->importApplicationLogo(writeFixture(directory, QStringLiteral("first.png")), &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(config()->get(Config::GUI_CustomLogoEnabled).toBool());
    QVERIFY(icons()->hasCustomApplicationLogo());
    QVERIFY(!QImage(icons()->applicationLogoPath()).isNull());
}

void TestApplicationLogo::presentationCleanupWarningCommitsSettings()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    Icons::setApplicationLogoCacheDirectoryForTests(directory.filePath(QStringLiteral("private-logos")));
    QString error;
    QVERIFY2(icons()->importApplicationLogo(writeFixture(directory, QStringLiteral("first.png")), &error), qPrintable(error));
    Icons::setApplicationLogoFailureStageForTests(6);
    QVERIFY(icons()->setApplicationLogoPresentation(QStringLiteral("crop"), QColor(QStringLiteral("#ff112233")), &error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(config()->get(Config::GUI_CustomLogoFitMode).toString(), QStringLiteral("crop"));
    QCOMPARE(config()->get(Config::GUI_CustomLogoBackground).toString(), QStringLiteral("#ff112233"));
    QVERIFY(!icons()->applicationIcon().isNull());
}

void TestApplicationLogo::rollbackRenameFailureLeavesResidualState()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    Icons::setApplicationLogoCacheDirectoryForTests(directory.filePath(QStringLiteral("private-logos")));
    QString error;
    QVERIFY2(icons()->importApplicationLogo(writeFixture(directory, QStringLiteral("old.png")), &error), qPrintable(error));
    Icons::setApplicationLogoFailureStageForTests(8);
    QVERIFY(!icons()->importApplicationLogo(writeFixture(directory, QStringLiteral("new.png"), {48, 24}), &error));
    const auto sourceBackup = QFileInfo(icons()->applicationLogoPath()).dir().filePath(QStringLiteral("application-logo-source.png.previous"));
    QVERIFY(QFile::exists(icons()->applicationLogoPath() + QStringLiteral(".previous")) || QFile::exists(sourceBackup));
}
