/*
 *  Copyright (C) 2019 KeePassXC Team <team@keepassxc.org>
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

#ifndef KEEPASSX_TESTUPDATECHECK_H
#define KEEPASSX_TESTUPDATECHECK_H

#include <QObject>
#include <QTemporaryDir>

class TestUpdateCheck : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void testCompareVersion();
    void testStateTransitions();
    void testManifestContract();
    void testPrereleaseManifestSelection();
    void testPrereleaseManifestSelection_data();
    void testStableManifestRoute();
    void testReleaseResponseFailures_data();
    void testReleaseResponseFailures();
    void testSelectedManifestIdentity_data();
    void testSelectedManifestIdentity();
    void testIndexReplacementLifecycle();
    void testReentrantStateChangeKeepsRequestContext();
    void testReentrantFailureClearsSelection();
    void testDestroyedIndexAndSelectedManifest();
    void testCheckerDestructionAbortsIndex();
    void testRedirectPolicy();
    void testPackageContract();
    void testRestartCommandContract();
    void testConcurrentCheckKeepsDownloadActive();
    void testRejectedPackageRedirectReportsDiagnostic();
    void testDestroyedNetworkManagerClearsActiveReplies();
    void testDeferredManifestDeletionDoesNotFailReplacementCheck();

private:
    QTemporaryDir m_configDirectory;
};

#endif // #define KEEPASSX_TESTUPDATECHECK_H
