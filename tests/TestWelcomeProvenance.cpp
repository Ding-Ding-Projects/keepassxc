/*
 *  Copyright (C) 2026 KeePassXC Team <team@keepassxc.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 or (at your option)
 *  version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "TestWelcomeProvenance.h"

#include "config-keepassx.h"
#include "core/Config.h"
#include "gui/WelcomeWidget.h"
#include "util/TemporaryFile.h"

#include <QDateTime>
#include <QLabel>
#include <QRegularExpression>
#include <QTest>

QTEST_MAIN(TestWelcomeProvenance)

void TestWelcomeProvenance::initTestCase()
{
    Config::createConfigFromFile(TemporaryFile::createTempConfigFile(), {});
}

void TestWelcomeProvenance::testFrontScreenNamesVersionRevisionAndUpdatedAt()
{
    WelcomeWidget welcome;
    auto* label = welcome.findChild<QLabel*>(QStringLiteral("versionProvenanceLabel"));
    QVERIFY2(label, "the welcome screen must carry the provenance label before any navigation");
    QVERIFY(label->isVisibleTo(&welcome));
    QVERIFY(!label->accessibleName().isEmpty());

    const QString text = label->text();
    QVERIFY2(text.contains(QString::fromLatin1(KEEPASSXC_VERSION)), qPrintable(text));

    const QString head = QString::fromLatin1(KEEPASSXC_GIT_HEAD);
    if (head.isEmpty()) {
        QVERIFY2(text.contains(QStringLiteral("revision unavailable")), qPrintable(text));
    } else {
        QVERIFY2(text.contains(head), qPrintable(text));
    }

    // Either a real local time with seconds and a labelled zone, derived from
    // the built revision's committer date, or the honest unavailable wording.
    const QDateTime committed = QDateTime::fromString(QString::fromLatin1(KEEPASSXC_COMMIT_DATE), Qt::ISODate);
    if (committed.isValid()) {
        const QRegularExpression stamp(QStringLiteral("updated \\d{4}-\\d{2}-\\d{2} \\d{2}:\\d{2}:\\d{2} \\S+"));
        QVERIFY2(stamp.match(text).hasMatch(), qPrintable(text));
        const QDateTime local = committed.toLocalTime();
        QVERIFY2(text.contains(local.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))), qPrintable(text));
        QVERIFY2(text.contains(local.timeZoneAbbreviation()), qPrintable(text));
        QVERIFY(!text.contains(QStringLiteral("unavailable")));
    } else {
        QVERIFY2(text.contains(QStringLiteral("updated-at time unavailable")), qPrintable(text));
    }
}

void TestWelcomeProvenance::testUpdatedAtIsNeverLaunchTime()
{
    // The label is computed from build metadata, so two widgets built seconds
    // apart must agree to the second; a launch-time clock would not.
    WelcomeWidget first;
    QTest::qWait(1100);
    WelcomeWidget second;
    const QString a = first.findChild<QLabel*>(QStringLiteral("versionProvenanceLabel"))->text();
    const QString b = second.findChild<QLabel*>(QStringLiteral("versionProvenanceLabel"))->text();
    QCOMPARE(a, b);
}
