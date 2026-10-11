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
#include "gui/material/MaterialButtons.h"
#include "gui/material/MaterialEntryDetail.h"
#include "gui/material/MaterialTheme.h"
#include "gui/material/MaterialEntryDelegate.h"
#include "gui/material/MaterialSearchBar.h"
#include "gui/material/MaterialVaultScreen.h"
#include "gui/material/MaterialTabOverflow.h"
#include "gui/material/MaterialVaultSidebar.h"
#include "gui/material/MaterialVoice.h"
#include <QLineEdit>
#include <QAction>
#include <QLabel>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QScrollBar>
#include <QApplication>
#include <QScopedPointer>

#include <QAbstractButton>
#include <QCoreApplication>
#include <QEvent>
#include <QStandardItemModel>
#include <QImage>
#include <QPainter>
#include <QStyleOptionViewItem>
#include <QTest>
#include <QTreeView>

QTEST_MAIN(TestMaterialVaultFilters)

using namespace Material;

void TestMaterialVaultFilters::initTestCase()
{
    Config::createConfigFromFile(TemporaryFile::createTempConfigFile(), TemporaryFile::createTempConfigFile());
}

void TestMaterialVaultFilters::contextualGuidanceInventoryAndDismissal()
{
    // Hand-maintained independently of the registration call sites.
    const QStringList required{QStringLiteral("vault.entries"), QStringLiteral("vault.groups"),
                               QStringLiteral("vault.tags"), QStringLiteral("vault.group-scope"),
                               QStringLiteral("vault.attachments"), QStringLiteral("tabs.open")};
    VaultScreen screen;
    TabOverflow tabs(&screen);
    QStringList covered;
    for (auto* bar : screen.findChildren<SearchBar*>()) {
        if (required.contains(bar->searchId()) && !bar->guidanceKey().isEmpty()) covered.append(bar->searchId());
    }
    covered.removeDuplicates();
    auto complete = [&required](const QStringList& rows) {
        for (const auto& id : required) if (!rows.contains(id)) return false;
        return true;
    };
    QVERIFY(complete(covered));
    for (const auto& id : required) {
        QStringList omitted = covered;
        omitted.removeAll(id);
        QVERIFY(!complete(omitted));
    }
    SearchBar bar;
    QVERIFY(bar.setIdentity(QStringLiteral("test.guidance"), QStringLiteral("Tags")));
    bar.setCopyKeys(QStringLiteral("search.tags"), QStringLiteral("search.tags"));
    QScopedPointer<QWidget> guidance(bar.guidanceWidget(QStringLiteral("search.guidance.tags")));
    auto* entry = guidance->findChild<QAbstractButton*>(QStringLiteral("searchGuidanceEntry"));
    auto* done = guidance->findChild<QAbstractButton*>(QStringLiteral("searchGuidanceDone"));
    auto* panel = guidance->findChild<QWidget*>(QStringLiteral("searchGuidancePanel"));
    QVERIFY(entry && done && panel);
    QVERIFY(panel->isHidden());
    bar.setText(QStringLiteral("work"));
    entry->click();
    QVERIFY(!panel->isHidden());
    const auto originalLanguage = Voice::language();
    for (auto language : {Voice::Language::English, Voice::Language::Cantonese, Voice::Language::Bilingual}) {
        Voice::setLanguage(language);
        QVERIFY(!entry->accessibleName().isEmpty());
        QVERIFY(!entry->accessibleName().contains(QStringLiteral("search.guidance")));
        QCOMPARE(bar.text(), QStringLiteral("work"));
    }
    done->click();
    QVERIFY(panel->isHidden());
    Voice::setLanguage(originalLanguage);
    QVERIFY(panel->isHidden());
    bar.setText(QStringLiteral("travel"));
    QVERIFY(panel->isHidden());
    const QStringList childControls{QStringLiteral("input"), QStringLiteral("clear"), QStringLiteral("regex"),
                                    QStringLiteral("builder"), QStringLiteral("open"), QStringLiteral("done")};
    auto* hiddenHelp = bar.findChild<QAction*>(QStringLiteral("test.guidance.help.input"));
    QVERIFY(hiddenHelp && !hiddenHelp->isEnabled());
    QVERIFY(!hiddenHelp->statusTip().isEmpty());
    bar.show();
    QCoreApplication::processEvents();
    QStringList descriptions;
    for (const auto& control : childControls) {
        auto* help = bar.findChild<QAction*>(QStringLiteral("test.guidance.help.") + control);
        QVERIFY(help);
        QVERIFY(help->isEnabled());
        QVERIFY(!help->text().contains(QStringLiteral("search.guidance")));
        help->trigger();
        QVERIFY(!panel->isHidden());
        const auto description = guidance->findChild<QLabel*>(QStringLiteral("searchGuidanceExplanation"))->text();
        QVERIFY(!description.contains(QStringLiteral("search.guidance.control")));
        QVERIFY(!descriptions.contains(description));
        descriptions.append(description);
        QCOMPARE(bar.text(), QStringLiteral("travel"));
        done->click();
        QVERIFY(panel->isHidden());
    }
    bar.hide();
    QVERIFY(!hiddenHelp->isEnabled());
    // A sibling guidance host can survive its originating field.
    auto* transient = new SearchBar;
    QVERIFY(transient->setIdentity(QStringLiteral("test.guidance-lifetime"), QStringLiteral("Tags")));
    QScopedPointer<QWidget> surviving(transient->guidanceWidget(QStringLiteral("search.guidance.tags")));
    auto* survivingEntry = surviving->findChild<QAbstractButton*>(QStringLiteral("searchGuidanceEntry"));
    auto* survivingDone = surviving->findChild<QAbstractButton*>(QStringLiteral("searchGuidanceDone"));
    survivingEntry->click();
    delete transient;
    QVERIFY(!surviving->isEnabled());
    QVERIFY(surviving->findChild<QWidget*>(QStringLiteral("searchGuidancePanel"))->isHidden());
    Voice::setLanguage(Voice::Language::Cantonese);
    survivingDone->click();
    survivingEntry->click();
    Voice::setLanguage(originalLanguage);
    QVERIFY(!surviving->isEnabled());
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
    sidebar.groupFilter()->setRegexFlags(QString());
    QVERIFY(view->isRowHidden(1, root->index()));
    sidebar.groupFilter()->setRegexFlags(QStringLiteral("i"));
    QVERIFY(!view->isRowHidden(1, root->index()));
    QVERIFY(view->isRowHidden(0, root->index()));
    sidebar.groupFilter()->setText(QStringLiteral("("));
    QVERIFY(!view->isRowHidden(1, root->index()));

    sidebar.groupFilter()->setRegexEnabled(false);
    sidebar.groupFilter()->setText(QString());
    QVERIFY(!view->isRowHidden(0, root->index()));
    QVERIFY(!view->isRowHidden(1, root->index()));
}

void TestMaterialVaultFilters::languageModesPreserveIndependentQueries()
{
    const auto originalLanguage = Voice::language();
    Voice::setLanguage(Voice::Language::English);
    VaultSidebar sidebar;
    sidebar.setTags({QStringLiteral("work"), QStringLiteral("travel")});
    sidebar.groupFilter()->setText(QStringLiteral("Cloud"));
    sidebar.tagFilter()->setText(QStringLiteral("work"));
    Voice::setLanguage(Voice::Language::Cantonese);
    const QString cantonesePlaceholder = sidebar.tagFilter()->placeholder();
    const QString cantoneseName = sidebar.tagFilter()->lineEdit()->accessibleName();
    Voice::setLanguage(Voice::Language::Bilingual);
    const QString bilingualPlaceholder = sidebar.tagFilter()->placeholder();
    const QString folderQuery = sidebar.groupFilter()->text();
    const QString tagQuery = sidebar.tagFilter()->text();
    Voice::setLanguage(originalLanguage);
    QCOMPARE(cantonesePlaceholder, QStringLiteral("搜尋標籤"));
    QCOMPARE(cantoneseName, cantonesePlaceholder);
    QVERIFY(bilingualPlaceholder.contains(QStringLiteral("Search tags")));
    QVERIFY(bilingualPlaceholder.contains(QStringLiteral("搜尋標籤")));
    QCOMPARE(folderQuery, QStringLiteral("Cloud"));
    QCOMPARE(tagQuery, QStringLiteral("work"));
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

void TestMaterialVaultFilters::entryRowsPaintTagChips()
{
    // A tagged row paints up to two tonal chips; an untagged row gives the room back.
    QStandardItemModel model;
    auto* tagged = new QStandardItem(QStringLiteral("Tagged"));
    tagged->setData(QStringLiteral("Tagged"), EntryDelegate::TitleRole);
    tagged->setData(QStringList{QStringLiteral("prod"), QStringLiteral("critical"), QStringLiteral("third")}, EntryDelegate::TagsRole);
    auto* plain = new QStandardItem(QStringLiteral("Plain"));
    plain->setData(QStringLiteral("Plain"), EntryDelegate::TitleRole);
    model.appendRow(tagged);
    model.appendRow(plain);

    EntryDelegate delegate;
    delegate.setCompactColumns(false);
    const QRect row(0, 0, 900, 52);
    auto render = [&](int rowIndex) {
        QImage image(row.size(), QImage::Format_ARGB32_Premultiplied);
        image.fill(theme()->color(Role::Surface));
        QPainter painter(&image);
        QStyleOptionViewItem option;
        option.rect = row;
        option.widget = nullptr;
        delegate.paint(&painter, option, model.index(rowIndex, 0));
        return image;
    };
    const QImage withTags = render(0);
    const QImage without = render(1);
    const QColor chip = theme()->color(Role::SecondaryContainer);
    int chipPixels = 0;
    int plainChipPixels = 0;
    for (int x = 0; x < row.width(); ++x) {
        for (int y = 0; y < row.height(); ++y) {
            if (QColor(withTags.pixel(x, y)) == chip) ++chipPixels;
            if (QColor(without.pixel(x, y)) == chip) ++plainChipPixels;
        }
    }
    QVERIFY2(chipPixels > 200, qPrintable(QString::number(chipPixels)));
    QCOMPARE(plainChipPixels, 0);
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
    detail.setEntryData(data);
    detail.show();
    QCoreApplication::processEvents();

    QVERIFY(detail.attachmentFilter());
    QVERIFY(detail.attachmentFilter()->isVisible());
    auto* guidance = detail.findChild<QWidget*>(QStringLiteral("vault.attachments.guidance"));
    QVERIFY(guidance && guidance->isVisible());
    detail.attachmentFilter()->lineEdit()->setFocus();
    QTest::keyClicks(detail.attachmentFilter()->lineEdit(), QStringLiteral("Username"));
    QCOMPARE(detail.visibleFieldKeys(), QStringList{QStringLiteral("Username")});
    detail.attachmentFilter()->clear();
    QCOMPARE(detail.visibleFieldKeys().size(), 4);
    data.attachments = {{QStringLiteral("recovery-codes.txt"), QStringLiteral("1 KB")},
                        {QStringLiteral("mfa-backup.png"), QStringLiteral("40 KB")}};
    detail.setEntryData(data);
    QVERIFY(detail.attachmentFilter()->isVisible());
    QCOMPARE(detail.attachmentFilter()->searchId(), QStringLiteral("vault.attachments"));
    QCOMPARE(detail.visibleFieldKeys().size(), 4);

    detail.attachmentFilter()->setText(QStringLiteral("acme"));
    QVERIFY(detail.visibleFieldKeys().isEmpty());
    detail.attachmentFilter()->setText(QStringLiteral("Username"));
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
    detail.attachmentFilter()->setRegexFlags(QString());
    QVERIFY(detail.visibleFieldKeys().isEmpty());
    detail.attachmentFilter()->setRegexFlags(QStringLiteral("i"));
    QCOMPARE(detail.visibleFieldKeys(), QStringList{QStringLiteral("Username")});
    detail.attachmentFilter()->setText(QStringLiteral("("));
    QCOMPARE(detail.visibleFieldKeys(), QStringList{QStringLiteral("Username")});

    detail.attachmentFilter()->setRegexEnabled(false);
    detail.attachmentFilter()->clear();
    QCOMPARE(detail.visibleFieldKeys().size(), 4);

    auto visibleAttachmentNames = [&detail] {
        QStringList names;
        for (auto* row : detail.findChildren<QAbstractButton*>()) {
            if ((row->toolTip().endsWith(QStringLiteral(".txt")) || row->toolTip().endsWith(QStringLiteral(".png")))
                && !row->isHidden()) {
                names.append(row->toolTip());
            }
        }
        names.sort();
        return names;
    };
    detail.attachmentFilter()->setText(QStringLiteral("recovery"));
    QCOMPARE(visibleAttachmentNames(), QStringList{QStringLiteral("recovery-codes.txt")});
    detail.setEntryData(data);
    QCOMPARE(visibleAttachmentNames(), QStringList{QStringLiteral("recovery-codes.txt")});
    data.attachments = {{QStringLiteral("recovery-new.txt"), QStringLiteral("2 KB")},
                        {QStringLiteral("unrelated.png"), QStringLiteral("3 KB")}};
    detail.setEntryData(data);
    QCOMPARE(visibleAttachmentNames(), QStringList{QStringLiteral("recovery-new.txt")});
    detail.attachmentFilter()->setRegexEnabled(true);
    detail.attachmentFilter()->setText(QStringLiteral("^recovery.*\\.txt$"));
    QCOMPARE(visibleAttachmentNames(), QStringList{QStringLiteral("recovery-new.txt")});
    QVERIFY(detail.visibleFieldKeys().isEmpty());
    detail.attachmentFilter()->setText(QStringLiteral("("));
    auto* status = detail.findChild<QLabel*>(QStringLiteral("entryDetailFilterStatus"));
    QVERIFY(status && !status->isHidden());
    QCOMPARE(status->text(), Voice::say(QStringLiteral("search.invalid")));
    detail.setEntryData(data);
    QCOMPARE(visibleAttachmentNames(), QStringList{QStringLiteral("recovery-new.txt")});
    QVERIFY(detail.visibleFieldKeys().isEmpty());
    data.attachments.append({QStringLiteral("recovery-added.txt"), QStringLiteral("4 KB")});
    detail.setEntryData(data);
    QCOMPARE(visibleAttachmentNames(), (QStringList{QStringLiteral("recovery-added.txt"), QStringLiteral("recovery-new.txt")}));
    QCOMPARE(detail.attachmentFilter()->text(), QStringLiteral("("));
    QVERIFY(detail.attachmentFilter()->isRegexEnabled());
    QCOMPARE(status->text(), Voice::say(QStringLiteral("search.invalid")));
    QVERIFY(!status->isHidden());
    detail.attachmentFilter()->clear();
    QCOMPARE(visibleAttachmentNames().size(), 3);
    QVERIFY(status->isHidden());
    detail.setEntryData(data);
    QCOMPARE(visibleAttachmentNames().size(), 3);
    detail.attachmentFilter()->setRegexEnabled(false);

    detail.attachmentFilter()->setText(QStringLiteral("Username"));
    data.attachments.clear();
    detail.setEntryData(data);
    QVERIFY(detail.attachmentFilter()->isVisible());
    QVERIFY(guidance->isVisible());
    QCOMPARE(detail.attachmentFilter()->text(), QStringLiteral("Username"));
    QCOMPARE(detail.visibleFieldKeys(), QStringList{QStringLiteral("Username")});
    detail.attachmentFilter()->clear();
    QCOMPARE(detail.visibleFieldKeys().size(), 4);

    // The hero carries the health chip, the edit and history actions and the
    // footer the copy-password and open-URL actions.
    QVERIFY(detail.findChild<QWidget*>(QStringLiteral("entryDetailHealthChip")));
    QVERIFY(detail.findChild<QAbstractButton*>(QStringLiteral("entryDetailEdit")));
    QVERIFY(detail.findChild<QAbstractButton*>(QStringLiteral("entryDetailHistory")));
    QVERIFY(detail.findChild<QAbstractButton*>(QStringLiteral("entryDetailCopyPassword"))->isEnabled());
    QVERIFY(detail.findChild<QAbstractButton*>(QStringLiteral("entryDetailOpenUrl"))->isEnabled());
    detail.clear();
    QVERIFY(!detail.attachmentFilter()->isVisible());
    QVERIFY(!guidance->isVisible());
}

void TestMaterialVaultFilters::tagSearchPreservesSelectionAndPreviousResults()
{
    VaultSidebar sidebar;
    sidebar.setTags({QStringLiteral("work"), QStringLiteral("personal"), QStringLiteral("travel")});
    sidebar.setSelectedTags({QStringLiteral("work")});
    sidebar.show();
    sidebar.tagFilter()->setText(QStringLiteral("travel"));
    auto visible = [&sidebar](const QString& text) {
        for (auto* chip : sidebar.findChildren<Chip*>()) {
            if (chip->text() == text) return !chip->isHidden();
        }
        return false;
    };
    QVERIFY(visible(QStringLiteral("work")));
    QVERIFY(visible(QStringLiteral("travel")));
    QVERIFY(!visible(QStringLiteral("personal")));
    QCOMPARE(sidebar.selectedTags(), QStringList{QStringLiteral("work")});
    QCOMPARE(sidebar.groupFilter()->text(), QString());
    sidebar.tagFilter()->setRegexEnabled(true);
    sidebar.tagFilter()->setText(QStringLiteral("["));
    QVERIFY(visible(QStringLiteral("travel")));
    QVERIFY(!visible(QStringLiteral("personal")));
    sidebar.tagFilter()->clear();
    QVERIFY(visible(QStringLiteral("personal")));
    QCOMPARE(sidebar.selectedTags(), QStringList{QStringLiteral("work")});
}

void TestMaterialVaultFilters::closingVaultClearsCategoryQueriesAndModes()
{
    VaultScreen screen;
    auto* entries = screen.searchBar();
    auto* folders = screen.sidebar()->groupFilter();
    auto* tags = screen.sidebar()->tagFilter();
    auto* details = screen.detail()->attachmentFilter();
    screen.sidebar()->setTags({QStringLiteral("selected")});
    screen.sidebar()->setSelectedTags({QStringLiteral("selected")});
    for (SearchBar* bar : {entries, folders, tags, details}) {
        bar->setText(QStringLiteral("sample"));
        bar->setRegexEnabled(true);
        bar->setRegexFlags(QStringLiteral("m"));
    }
    screen.setDatabaseWidget(nullptr);
    for (SearchBar* bar : {entries, folders, tags, details}) {
        QVERIFY(bar->text().isEmpty());
        QVERIFY(!bar->isRegexEnabled());
        QCOMPARE(bar->regexFlags(), QStringLiteral("i"));
    }
    QVERIFY(screen.sidebar()->selectedTags().isEmpty());
}

void TestMaterialVaultFilters::guidanceEntriesWrapWithinNarrowHosts()
{
    const auto originalLanguage = Voice::language();
    // Retain the observed 230-pixel failure shape as an executable negative.
    QWidget legacyHost;
    legacyHost.setFixedWidth(230);
    auto* legacyLayout = new QVBoxLayout(&legacyHost);
    legacyLayout->setContentsMargins(0, 0, 0, 0);
    legacyLayout->setSizeConstraint(QLayout::SetNoConstraint);
    auto* legacy = new TextButton(&legacyHost);
    legacy->setText(QStringLiteral("Guide this search: Filter groups"));
    legacy->setMinimumWidth(legacy->sizeHint().width());
    legacyLayout->addWidget(legacy);
    legacyHost.show();
    QCoreApplication::processEvents();
    const auto contained = [](QWidget* owner, QWidget* child) {
        return owner->rect().contains(QRect(child->mapTo(owner, QPoint()), child->size()));
    };
    QVERIFY(!contained(&legacyHost, legacy));
    legacyHost.hide();

    // Hand-maintained cases for the two observed sidebar owners.
    const QStringList categories{QStringLiteral("groups"), QStringLiteral("tags")};
    for (auto language : {Voice::Language::English, Voice::Language::Cantonese, Voice::Language::Bilingual}) {
        Voice::setLanguage(language);
        for (const auto& category : categories) {
            QWidget owner;
            owner.setFixedWidth(230);
            auto* ownerLayout = new QVBoxLayout(&owner);
            ownerLayout->setContentsMargins(0, 0, 0, 0);
            ownerLayout->setSizeConstraint(QLayout::SetNoConstraint);
            SearchBar bar(SearchBar::Variant::Surface, &owner);
            const auto id = QStringLiteral("test.guidance.wrap.") + category;
            QVERIFY(bar.setIdentity(id, category));
            bar.setCopyKeys(QStringLiteral("search.") + category, QStringLiteral("search.") + category);
            QScopedPointer<QWidget> host(bar.guidanceWidget(QStringLiteral("search.guidance.") + category));
            host->setFixedWidth(230);
            ownerLayout->addWidget(host.data());
            ownerLayout->addWidget(&bar);
            owner.show();
            owner.activateWindow();
            owner.adjustSize();
            QCoreApplication::processEvents();
            auto* entry = host->findChild<QAbstractButton*>(QStringLiteral("searchGuidanceEntry"));
            auto* panel = host->findChild<QWidget*>(QStringLiteral("searchGuidancePanel"));
            auto* done = host->findChild<QAbstractButton*>(QStringLiteral("searchGuidanceDone"));
            auto* scroll = panel->findChild<QScrollArea*>();
            auto* explanation = panel->findChild<QLabel*>(QStringLiteral("searchGuidanceExplanation"));
            QVERIFY(entry && panel && done && scroll && explanation);
            const auto expected = Voice::say(QStringLiteral("search.guidance.open")) + QStringLiteral(": ")
                                  + Voice::say(QStringLiteral("search.") + category);
            QCOMPARE(entry->text(), expected);
            QCOMPARE(entry->accessibleName(), expected);
            QVERIFY(contained(host.data(), entry));
            QVERIFY(entry->sizePolicy().hasHeightForWidth());
            QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
            QCoreApplication::processEvents();
            QTRY_VERIFY2(entry->height() >= entry->heightForWidth(entry->width()),
                         qPrintable(QStringLiteral("category=%1 language=%2 allocated=%3x%4 requiredHeight=%5 host=%6x%7")
                                        .arg(category).arg(int(language)).arg(entry->width()).arg(entry->height())
                                        .arg(entry->heightForWidth(entry->width())).arg(host->width()).arg(host->height())));
            QVERIFY(entry->height() >= Layout::ButtonHeight);
            if (language == Voice::Language::English || language == Voice::Language::Bilingual)
                QVERIFY(entry->height() > Layout::ButtonHeight);
            QVERIFY(panel->isHidden());
            bar.setText(QStringLiteral("synthetic-query"));
            entry->setFocus();
            QTest::keyClick(entry, Qt::Key_Space);
            QVERIFY(!panel->isHidden());
            owner.adjustSize();
            QCoreApplication::processEvents();
            QVERIFY(contained(host.data(), panel));
            QVERIFY(contained(panel, done));
            QVERIFY(contained(panel, scroll));
            QVERIFY(contained(scroll, scroll->viewport()));
            QCOMPARE(done->text(), Voice::say(QStringLiteral("search.guidance.done")));
            QCOMPARE(done->accessibleName(), done->text());
            QCOMPARE(explanation->accessibleName(), explanation->text());
            // Let the scroll area's deferred layout settle before checking the complete
            // wrapped explanation, not merely the currently visible viewport portion.
            QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
            QCoreApplication::processEvents();
            QTRY_VERIFY(explanation->width() > 0
                        && explanation->heightForWidth(explanation->width()) > 0
                        && explanation->height() >= explanation->heightForWidth(explanation->width()));
            QVERIFY(done->height() >= done->heightForWidth(done->width()));
            QVERIFY(done->height() >= Layout::ButtonHeight);
            // Vertical excess is intentionally exposed by this scroll area. Horizontal
            // label escape is never intentional and must not require horizontal scrolling.
            const QRect explanationRect(explanation->mapTo(scroll->viewport(), QPoint()), explanation->size());
            QVERIFY(explanationRect.left() >= 0);
            QVERIFY(explanationRect.right() < scroll->viewport()->width());
            QCOMPARE(scroll->horizontalScrollBar()->maximum(), 0);
            if (explanationRect.height() > scroll->viewport()->height())
                QVERIFY(scroll->verticalScrollBar()->maximum() > 0);
            QCOMPARE(bar.text(), QStringLiteral("synthetic-query"));
            done->setFocus();
            QTest::keyClick(done, Qt::Key_Space);
            QCoreApplication::processEvents();
            QVERIFY(panel->isHidden());
            QCOMPARE(bar.text(), QStringLiteral("synthetic-query"));
            QTRY_COMPARE(QApplication::focusWidget(), bar.lineEdit());
            QVERIFY(contained(host.data(), entry));
        }
    }
    Voice::setLanguage(originalLanguage);
}
