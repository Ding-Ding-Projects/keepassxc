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

#include "TestMaterialVaultFilters.h"

#include "core/Config.h"
#include "util/TemporaryFile.h"
#include "gui/material/MaterialChip.h"
#include "gui/material/MaterialEntryDetail.h"
#include "gui/material/MaterialSearchBar.h"
#include "gui/material/MaterialVaultScreen.h"
#include "gui/material/MaterialVaultSidebar.h"

#include <QAbstractButton>
#include <QCoreApplication>
#include <QStandardItemModel>
#include <QTest>
#include <QTreeView>

QTEST_MAIN(TestMaterialVaultFilters)

using namespace Material;

void TestMaterialVaultFilters::initTestCase()
{
    Config::createConfigFromFile(TemporaryFile::createTempConfigFile(), {});
}

void TestMaterialVaultFilters::groupFilterKeepsAncestorsOfMatches()
{
    VaultSidebar sidebar;
    auto* model = new QStandardItemModel(&sidebar);
    auto* root = new QStandardItem(QStringLiteral("Passwords"));
    auto* development = new QStandardItem(QStringLiteral("Development"));
    auto* cloud = new QStandardItem(QStringLiteral("Cloud"));
    auto* banking = new QStandardItem(QStringLiteral("Banking"));
    development->appendRow(cloud);
    root->appendRow(development);
    root->appendRow(banking);
    model->appendRow(root);
    sidebar.setGroupModel(model);
    sidebar.show();
    QCoreApplication::processEvents();

    QVERIFY(sidebar.groupFilter());
    QCOMPARE(sidebar.groupFilter()->placeholder(), QStringLiteral("Filter groups"));
    auto* view = sidebar.groupView();

    sidebar.groupFilter()->setText(QStringLiteral("cloud"));
    QVERIFY(!view->isRowHidden(0, QModelIndex())); // Passwords, an ancestor
    QVERIFY(!view->isRowHidden(0, root->index())); // Development, an ancestor
    QVERIFY(view->isRowHidden(1, root->index())); // Banking misses
    QVERIFY(!view->isRowHidden(0, development->index())); // Cloud matches
    QVERIFY(view->isExpanded(development->index()));

    // Regex is an explicit opt-in and an unparsable pattern changes nothing.
    sidebar.groupFilter()->setRegexEnabled(true);
    sidebar.groupFilter()->setText(QStringLiteral("^bank"));
    QVERIFY(!view->isRowHidden(1, root->index()));
    QVERIFY(view->isRowHidden(0, root->index()));
    sidebar.groupFilter()->setText(QStringLiteral("("));
    QVERIFY(!view->isRowHidden(1, root->index()));

    sidebar.groupFilter()->setRegexEnabled(false);
    sidebar.groupFilter()->setText(QString());
    QVERIFY(!view->isRowHidden(0, root->index()));
    QVERIFY(!view->isRowHidden(1, root->index()));
}

void TestMaterialVaultFilters::healthChipsArePresentAndCheckable()
{
    VaultScreen screen;
    screen.show();
    QCoreApplication::processEvents();
    const QStringList ids{QStringLiteral("breached"), QStringLiteral("weak"), QStringLiteral("reused"), QStringLiteral("healthy")};
    for (const QString& id : ids) {
        auto* chip = screen.findChild<Chip*>(QStringLiteral("vaultHealthChip_") + id);
        QVERIFY2(chip, qPrintable(id));
        QVERIFY(chip->isCheckable());
        QVERIFY(!chip->accessibleName().isEmpty());
        QCOMPARE(chip->kind(), Chip::Kind::Filter);
    }
    auto* weak = screen.findChild<Chip*>(QStringLiteral("vaultHealthChip_weak"));
    weak->setChecked(true);
    QVERIFY(weak->isChecked());
    weak->setChecked(false);
    QVERIFY(!weak->isChecked());
}

void TestMaterialVaultFilters::detailFilterNarrowsFieldsAndAttachments()
{
    EntryDetail detail;
    EntryDetailData data;
    data.title = QStringLiteral("AWS");
    data.username = QStringLiteral("ops@acme.example");
    data.password = QStringLiteral("secret");
    data.url = QStringLiteral("https://console.aws.amazon.com");
    data.modified = QStringLiteral("2026-08-14 09:12");
    data.attachments = {{QStringLiteral("recovery-codes.txt"), QStringLiteral("1 KB")},
                        {QStringLiteral("mfa-backup.png"), QStringLiteral("40 KB")}};
    detail.setEntryData(data);
    detail.show();
    QCoreApplication::processEvents();

    QVERIFY(detail.attachmentFilter());
    QCOMPARE(detail.attachmentFilter()->searchId(), QStringLiteral("vault.attachments"));
    QCOMPARE(detail.visibleFieldKeys().size(), 4);

    detail.attachmentFilter()->setText(QStringLiteral("acme"));
    QCOMPARE(detail.visibleFieldKeys(), QStringList{QStringLiteral("Username")});
    auto rows = detail.findChildren<QAbstractButton*>();
    int visibleAttachments = 0;
    for (auto* row : rows) {
        if (row->toolTip().endsWith(QStringLiteral(".txt")) || row->toolTip().endsWith(QStringLiteral(".png"))) {
            visibleAttachments += row->isHidden() ? 0 : 1;
        }
    }
    QCOMPARE(visibleAttachments, 0);

    // Regex is an opt-in; an unparsable pattern leaves the previous result
    // standing rather than hiding everything or searching it literally.
    detail.attachmentFilter()->setRegexEnabled(true);
    detail.attachmentFilter()->setText(QStringLiteral("^user"));
    QCOMPARE(detail.visibleFieldKeys(), QStringList{QStringLiteral("Username")});
    detail.attachmentFilter()->setText(QStringLiteral("("));
    QCOMPARE(detail.visibleFieldKeys(), QStringList{QStringLiteral("Username")});

    detail.attachmentFilter()->setRegexEnabled(false);
    detail.attachmentFilter()->clear();
    QCOMPARE(detail.visibleFieldKeys().size(), 4);

    // The hero carries the health chip, the edit and history actions and the
    // footer the copy-password and open-URL actions.
    QVERIFY(detail.findChild<QWidget*>(QStringLiteral("entryDetailHealthChip")));
    QVERIFY(detail.findChild<QAbstractButton*>(QStringLiteral("entryDetailEdit")));
    QVERIFY(detail.findChild<QAbstractButton*>(QStringLiteral("entryDetailHistory")));
    QVERIFY(detail.findChild<QAbstractButton*>(QStringLiteral("entryDetailCopyPassword"))->isEnabled());
    QVERIFY(detail.findChild<QAbstractButton*>(QStringLiteral("entryDetailOpenUrl"))->isEnabled());
}
