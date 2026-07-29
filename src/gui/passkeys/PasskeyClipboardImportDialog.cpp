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

#include "PasskeyClipboardImportDialog.h"
#include "ui_PasskeyClipboardImportDialog.h"

#include "PasskeyImporter.h"
#include "gui/styles/StateColorPalette.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QStringList>

PasskeyClipboardImportDialog::PasskeyClipboardImportDialog(QWidget* parent)
    : QDialog(parent)
    , m_ui(new Ui::PasskeyClipboardImportDialog())
{
    setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);

    m_ui->setupUi(this);

    connect(m_ui->pasteButton, SIGNAL(clicked()), SLOT(pasteFromClipboard()));
    connect(m_ui->payloadEdit, SIGNAL(textChanged()), SLOT(validatePayload()));
    connect(m_ui->importButton, SIGNAL(clicked()), SLOT(accept()));
    connect(m_ui->cancelButton, SIGNAL(clicked()), SLOT(reject()));

    validatePayload();
    m_ui->payloadEdit->setFocus();
}

PasskeyClipboardImportDialog::~PasskeyClipboardImportDialog()
{
}

QString PasskeyClipboardImportDialog::payload() const
{
    return m_valid ? m_ui->payloadEdit->toPlainText() : QString();
}

void PasskeyClipboardImportDialog::pasteFromClipboard()
{
    const auto clipboardText = QGuiApplication::clipboard()->text();
    m_ui->payloadEdit->setPlainText(clipboardText);
    m_ui->payloadEdit->setFocus();
}

/**
 * Re-validates the pasted text on every change and reports the exact reason it cannot be
 * imported. Only the relying party and username are echoed back, never the private key.
 */
void PasskeyClipboardImportDialog::validatePayload()
{
    const StateColorPalette statePalette;
    const auto text = m_ui->payloadEdit->toPlainText();

    m_ui->detailsLabel->clear();

    if (text.trimmed().isEmpty()) {
        m_valid = false;
        m_ui->validationLabel->setStyleSheet({});
        m_ui->validationLabel->setText(tr("Paste the passkey text copied from KeePassXC to continue."));
        m_ui->importButton->setEnabled(false);
        return;
    }

    const auto result = PasskeyImporter::parsePayload(text);
    m_valid = result.isValid();

    if (!m_valid) {
        m_ui->validationLabel->setStyleSheet(
            QString("color: %1;").arg(statePalette.color(StateColorPalette::ColorRole::Error).name()));
        m_ui->validationLabel->setText(result.errorMessage);
        m_ui->importButton->setEnabled(false);
        return;
    }

    m_ui->validationLabel->setStyleSheet(
        QString("color: %1;").arg(statePalette.color(StateColorPalette::ColorRole::True).name()));
    m_ui->validationLabel->setText(
        tr("Valid passkey text containing %n passkey(s).", "", static_cast<int>(result.passkeys.count())));

    // A valid payload can still carry enormous fields, and this label is word-wrapped inside a modal
    // dialog with no scroll area: show a bounded number of bounded lines so the buttons stay onscreen.
    constexpr int MaxDetailLines = 10;
    QStringList details;
    for (const auto& passkeyObject : result.passkeys) {
        if (details.count() == MaxDetailLines) {
            details << tr("… and %n more passkey(s).", "", static_cast<int>(result.passkeys.count()) - MaxDetailLines);
            break;
        }
        details << tr("Relying Party: %1, Username: %2")
                       .arg(PasskeyImporter::sanitizeForDisplay(passkeyObject["relyingParty"].toString()),
                            PasskeyImporter::sanitizeForDisplay(passkeyObject["username"].toString()));
    }
    m_ui->detailsLabel->setText(details.join("\n"));

    m_ui->importButton->setEnabled(true);
}
