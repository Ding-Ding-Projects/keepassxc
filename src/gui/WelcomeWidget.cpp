/*
 *  Copyright (C) 2012 Felix Geyer <debfx@fobos.de>
 *  Copyright (C) 2020 KeePassXC Team <team@keepassxc.org>
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

#include "WelcomeWidget.h"
#include "ui_WelcomeWidget.h"
#include <QKeyEvent>

#include "config-keepassx.h"
#include "core/Config.h"
#include "gui/Icons.h"
#include "gui/material/MaterialTheme.h"

#include <QDateTime>
#include <QLabel>

WelcomeWidget::WelcomeWidget(QWidget* parent)
    : QWidget(parent)
    , m_ui(new Ui::WelcomeWidget())
{
    m_ui->setupUi(this);

    m_ui->welcomeLabel->setText(tr("Welcome to KeePassXC %1").arg(KEEPASSXC_VERSION));
    m_ui->welcomeLabel->setFont(theme()->font(Material::TypeRole::HeadlineSmall));

    // The running version and the exact moment that version was last updated,
    // shown on the first screen before any navigation. The moment is the
    // committer date of the built revision converted to the user's local
    // time with seconds and a labelled zone; when the build carried no Git
    // metadata the line says so instead of inventing a time.
    auto* provenance = new QLabel(this);
    provenance->setObjectName(QStringLiteral("versionProvenanceLabel"));
    provenance->setAlignment(Qt::AlignCenter);
    provenance->setWordWrap(true);
    provenance->setFont(theme()->font(Material::TypeRole::BodySmall));
    provenance->setTextInteractionFlags(Qt::TextSelectableByMouse);
    const QDateTime committed = QDateTime::fromString(QString::fromLatin1(KEEPASSXC_COMMIT_DATE), Qt::ISODate);
    QString updated;
    if (committed.isValid()) {
        const QDateTime local = committed.toLocalTime();
        updated = tr("updated %1 %2").arg(local.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")), local.timeZoneAbbreviation());
    } else {
        updated = tr("updated-at time unavailable: the build carried no Git commit date");
    }
    const QString head = QString::fromLatin1(KEEPASSXC_GIT_HEAD);
    const QString revision = head.isEmpty() ? tr("revision unavailable") : tr("revision %1").arg(head);
    provenance->setText(tr("Version %1 · %2 · %3").arg(QString::fromLatin1(KEEPASSXC_VERSION), revision, updated));
    provenance->setAccessibleName(tr("Running version and update time"));
    m_ui->verticalLayout->insertWidget(m_ui->verticalLayout->indexOf(m_ui->welcomeLabel) + 1, provenance);

    m_ui->iconLabel->setPixmap(icons()->applicationIcon().pixmap(64));
    m_ui->buttonNewDatabase->setIcon(icons()->icon("document-new"));
    m_ui->buttonNewDatabase->setStyleSheet("text-align:center;");
    m_ui->buttonOpenDatabase->setIcon(icons()->icon("document-open"));
    m_ui->buttonOpenDatabase->setStyleSheet("text-align:center;");
    m_ui->buttonImport->setIcon(icons()->icon("document-import"));
    m_ui->buttonImport->setStyleSheet("text-align:center;");

    refreshLastDatabases();

    connect(m_ui->buttonNewDatabase, SIGNAL(clicked()), SIGNAL(newDatabase()));
    connect(m_ui->buttonOpenDatabase, SIGNAL(clicked()), SIGNAL(openDatabase()));
    connect(m_ui->buttonImport, SIGNAL(clicked()), SIGNAL(importFile()));
    connect(m_ui->recentListWidget,
            SIGNAL(itemActivated(QListWidgetItem*)),
            this,
            SLOT(openDatabaseFromFile(QListWidgetItem*)));
}

WelcomeWidget::~WelcomeWidget() = default;

void WelcomeWidget::openDatabaseFromFile(QListWidgetItem* item)
{
    if (!item || item->text().isEmpty()) {
        return;
    }
    emit openDatabaseFile(item->text());
}

void WelcomeWidget::removeFromLastDatabases(QListWidgetItem* item)
{
    if (!item || item->text().isEmpty()) {
        return;
    }

    if (config()->get(Config::RememberLastDatabases).toBool()) {
        QStringList lastDatabases = config()->get(Config::LastDatabases).toStringList();
        lastDatabases.removeOne(item->text());
        config()->set(Config::LastDatabases, lastDatabases);
    }
    refreshLastDatabases();
}

void WelcomeWidget::refreshLastDatabases()
{
    m_ui->recentListWidget->clear();
    const QStringList lastDatabases = config()->get(Config::LastDatabases).toStringList();
    for (const QString& database : lastDatabases) {
        auto itm = new QListWidgetItem;
        itm->setText(database);
        m_ui->recentListWidget->addItem(itm);
    }

    bool recent_visibility = (m_ui->recentListWidget->count() > 0);
    m_ui->startLabel->setVisible(!recent_visibility);
    m_ui->recentListWidget->setVisible(recent_visibility);
    m_ui->recentLabel->setVisible(recent_visibility);
}

void WelcomeWidget::keyPressEvent(QKeyEvent* event)
{
    if (m_ui->recentListWidget->hasFocus()) {
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
            openDatabaseFromFile(m_ui->recentListWidget->currentItem());
        } else if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
            removeFromLastDatabases(m_ui->recentListWidget->currentItem());
        }
    }

    QWidget::keyPressEvent(event);
}

void WelcomeWidget::showEvent(QShowEvent* event)
{
    refreshLastDatabases();
    QWidget::showEvent(event);
}
