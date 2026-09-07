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
#include <QFile>
#include <QImage>
#include <QTemporaryDir>
#include <QTest>

QTEST_MAIN(TestApplicationLogo)

namespace
{
    QString writeFixture(QTemporaryDir& directory, const QString& name, const QSize& size = {32, 16})
    {
        const auto path = directory.filePath(name);
        QImage image(size, QImage::Format_ARGB32_Premultiplied);
        image.fill(QColor(12, 34, 56, 128));
        Q_ASSERT(image.save(path, "PNG"));
        return path;
    }
}

void TestApplicationLogo::cleanup()
{
    icons()->resetApplicationLogo();
    config()->set(Config::GUI_CustomLogoFitMode, QStringLiteral("fit"));
    config()->set(Config::GUI_CustomLogoBackground, QStringLiteral("#00000000"));
}

void TestApplicationLogo::importsValidatedLocalImageAndPersistsOnlyDerivedPath()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
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
    QString error;
    QVERIFY2(icons()->importApplicationLogo(writeFixture(directory, QStringLiteral("wide.png"), {64, 16}), &error), qPrintable(error));

    config()->set(Config::GUI_CustomLogoFitMode, QStringLiteral("crop"));
    config()->set(Config::GUI_CustomLogoBackground, QStringLiteral("#ff112233"));
    QVERIFY2(icons()->refreshApplicationLogo(&error), qPrintable(error));
    QImage derived(icons()->applicationLogoPath());
    QVERIFY(!derived.isNull());
    QCOMPARE(derived.width(), derived.height());
    QCOMPARE(config()->get(Config::GUI_CustomLogoFitMode).toString(), QStringLiteral("crop"));

    icons()->resetApplicationLogo();
    QVERIFY(!icons()->hasCustomApplicationLogo());
    QVERIFY(!QFile::exists(icons()->applicationLogoPath()));
}
