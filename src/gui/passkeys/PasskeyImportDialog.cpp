/*
 *  Copyright (C) 2024 KeePassXC Team <team@keepassxc.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "PasskeyImportDialog.h"
#include "ui_PasskeyImportDialog.h"

#include "PasskeyImporter.h"
#include "browser/BrowserService.h"
#include "core/Metadata.h"
#include "gui/MainWindow.h"
#include "gui/material/MaterialSearchBar.h"
#include <QCloseEvent>
#include <QFileInfo>
#include <QGridLayout>
#include <QLineEdit>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QVBoxLayout>

PasskeyImportDialog::PasskeyImportDialog(QWidget* parent)
    : QDialog(parent)
    , m_ui(new Ui::PasskeyImportDialog())
{
    setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);

    m_ui->setupUi(this);

    // A vault group can contain thousands of entries. Keep one source model
    // and filter it through the dialog's own search field instead of forcing
    // the user to scan an unbounded combo box.
    m_entrySourceModel = new QStandardItemModel(this);
    m_entryFilterModel = new QSortFilterProxyModel(this);
    m_entryFilterModel->setSourceModel(m_entrySourceModel);
    m_entryFilterModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_entryFilterModel->setFilterKeyColumn(0);
    m_ui->selectEntryComboBox->setModel(m_entryFilterModel);

    m_entrySearch = new Material::SearchBar(Material::SearchBar::Variant::Surface, m_ui->groupBox);
    m_entrySearch->setObjectName(QStringLiteral("passkeyEntrySearch"));
    m_entrySearch->setPlaceholder(tr("Search entries in this group"));
    m_entrySearch->setIdentity(QStringLiteral("passkeys.import.entries"), tr("Passkey entry search"));
    m_entrySearch->lineEdit()->setAccessibleName(tr("Search entries in this group"));
    auto* entryCell = new QWidget(m_ui->groupBox);
    auto* entryCellLayout = new QVBoxLayout(entryCell);
    entryCellLayout->setContentsMargins(0, 0, 0, 0);
    entryCellLayout->setSpacing(6);
    entryCellLayout->addWidget(m_entrySearch);
    entryCellLayout->addWidget(m_ui->selectEntryComboBox);
    if (auto* grid = qobject_cast<QGridLayout*>(m_ui->groupBox->layout())) {
        grid->removeWidget(m_ui->selectEntryComboBox);
        grid->addWidget(entryCell, 2, 1);
    }
    connect(m_entrySearch, &Material::SearchBar::textChanged, this, [this](const QString& text) {
        m_entryFilterModel->setFilterFixedString(text.trimmed());
        if (m_entryFilterModel->rowCount() > 0) {
            m_ui->selectEntryComboBox->setCurrentIndex(0);
        } else {
            m_selectedEntryUuid = {};
        }
        m_entrySearch->lineEdit()->setAccessibleDescription(
            tr("%1 entries match").arg(m_entryFilterModel->rowCount()));
    });

    connect(this, SIGNAL(updateGroups()), this, SLOT(addGroups()));
    connect(this, SIGNAL(updateEntries()), this, SLOT(addEntries()));
    connect(m_ui->importButton, SIGNAL(clicked()), SLOT(accept()));
    connect(m_ui->cancelButton, SIGNAL(clicked()), SLOT(reject()));
    connect(m_ui->selectDatabaseCombobBox, SIGNAL(currentIndexChanged(int)), SLOT(changeDatabase(int)));
    connect(m_ui->selectEntryComboBox, SIGNAL(currentIndexChanged(int)), SLOT(changeEntry(int)));
    connect(m_ui->selectGroupComboBox, SIGNAL(currentIndexChanged(int)), SLOT(changeGroup(int)));
}

PasskeyImportDialog::~PasskeyImportDialog()
{
}

void PasskeyImportDialog::setInfo(const QString& relyingParty,
                                  const QString& username,
                                  const QSharedPointer<Database>& database,
                                  bool isEntry,
                                  const QString& titleText,
                                  const QString& infoText,
                                  const QString& importButtonText)
{
    // These come straight from an imported file or a pasted payload and land in labels that do not
    // wrap - one inside a fixed-size layout - so bound them before they decide the dialog's width.
    m_ui->relyingPartyLabel->setText(tr("Relying Party: %1").arg(PasskeyImporter::sanitizeForDisplay(relyingParty)));
    m_ui->usernameLabel->setText(tr("Username: %1").arg(PasskeyImporter::sanitizeForDisplay(username)));

    if (isEntry) {
        m_ui->verticalLayout->setSizeConstraint(QLayout::SetFixedSize);
        m_ui->infoLabel->setText(tr("Import the following passkey to this entry:"));
        m_ui->groupBox->setVisible(false);
    }

    m_selectedDatabase = database;
    addDatabases();
    addGroups();

    auto openDatabaseCount = 0;
    for (auto dbWidget : getMainWindow()->getOpenDatabases()) {
        if (dbWidget && !dbWidget->isLocked()) {
            openDatabaseCount++;
        }
    }
    m_ui->selectDatabaseCombobBox->setEnabled(openDatabaseCount > 1);

    if (!titleText.isEmpty()) {
        setWindowTitle(titleText);
    }

    if (!infoText.isEmpty()) {
        m_ui->infoLabel->setText(infoText);
    }

    if (!importButtonText.isEmpty()) {
        m_ui->importButton->setText(importButtonText);
    }
}

QSharedPointer<Database> PasskeyImportDialog::getSelectedDatabase() const
{
    return m_selectedDatabase;
}

QUuid PasskeyImportDialog::getSelectedEntryUuid() const
{
    return m_selectedEntryUuid;
}

QUuid PasskeyImportDialog::getSelectedGroupUuid() const
{
    return m_selectedGroupUuid;
}

bool PasskeyImportDialog::useDefaultGroup() const
{
    return m_selectedGroupUuid.isNull();
}

bool PasskeyImportDialog::createNewEntry() const
{
    return m_selectedEntryUuid.isNull();
}

void PasskeyImportDialog::addDatabases()
{
    auto currentDatabaseIndex = 0;
    const auto openDatabases = browserService()->getOpenDatabases();
    const auto currentDatabase = browserService()->getDatabase();

    m_ui->selectDatabaseCombobBox->clear();
    for (const auto& db : openDatabases) {
        m_ui->selectDatabaseCombobBox->addItem(db->metadata()->name(), db->rootGroup()->uuid());
        if (db->rootGroup()->uuid() == currentDatabase->rootGroup()->uuid()) {
            currentDatabaseIndex = m_ui->selectDatabaseCombobBox->count() - 1;
        }
    }

    m_ui->selectDatabaseCombobBox->setCurrentIndex(currentDatabaseIndex);
}

void PasskeyImportDialog::addEntries()
{
    if (!m_selectedDatabase || !m_selectedDatabase->rootGroup()) {
        return;
    }

    m_entrySourceModel->clear();
    auto* createItem = new QStandardItem(tr("Create new entry"));
    createItem->setData(QVariant(), Qt::UserRole);
    m_entrySourceModel->appendRow(createItem);

    const auto group = m_selectedDatabase->rootGroup()->findGroupByUuid(m_selectedGroupUuid);
    if (!group) {
        return;
    }

    // Collect all entries in the group and resolve the title
    QList<QPair<QString, QUuid>> entries;
    for (const auto entry : group->entries()) {
        if (!entry || entry->isRecycled()) {
            continue;
        }
        entries.append({entry->resolveMultiplePlaceholders(entry->title()), entry->uuid()});
    }

    // Sort entries by title
    std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
        return a.first.compare(b.first, Qt::CaseInsensitive) < 0;
    });

    // Add sorted entries to the combobox
    for (const auto& pair : entries) {
        auto* item = new QStandardItem(pair.first);
        item->setData(pair.second, Qt::UserRole);
        m_entrySourceModel->appendRow(item);
    }
    m_entryFilterModel->invalidate();
    m_entrySearch->setText(QString());
    m_ui->selectEntryComboBox->setCurrentIndex(0);
}

void PasskeyImportDialog::addGroups()
{
    if (!m_selectedDatabase) {
        return;
    }

    m_ui->selectGroupComboBox->clear();
    m_ui->selectGroupComboBox->addItem(tr("Default passkeys group (Imported Passkeys)"), {});

    for (const auto& group : m_selectedDatabase->rootGroup()->groupsRecursive(true)) {
        if (!group || group->isRecycled() || group == m_selectedDatabase->metadata()->recycleBin()) {
            continue;
        }

        m_ui->selectGroupComboBox->addItem(group->fullPath(), group->uuid());
    }
}

void PasskeyImportDialog::changeDatabase(int index)
{
    m_selectedDatabaseUuid = m_ui->selectDatabaseCombobBox->itemData(index).value<QUuid>();
    m_selectedDatabase = browserService()->getDatabase(m_selectedDatabaseUuid);
    emit updateGroups();
}

void PasskeyImportDialog::changeEntry(int index)
{
    m_selectedEntryUuid = m_ui->selectEntryComboBox->itemData(index).value<QUuid>();
}

void PasskeyImportDialog::changeGroup(int index)
{
    m_selectedGroupUuid = m_ui->selectGroupComboBox->itemData(index).value<QUuid>();
    emit updateEntries();
}
