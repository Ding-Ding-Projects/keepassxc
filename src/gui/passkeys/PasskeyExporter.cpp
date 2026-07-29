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

#include "PasskeyExporter.h"
#include "PasskeyExportDialog.h"

#include "browser/BrowserPasskeys.h"
#include "browser/PasskeyUtils.h"
#include "core/Config.h"
#include "core/Entry.h"
#include "core/EntryAttributes.h"
#include "core/Tools.h"
#include "gui/Clipboard.h"
#include "gui/MessageBox.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

const QString PasskeyExporter::PayloadScheme = QStringLiteral("keepassxc-passkey");
const QString PasskeyExporter::PayloadVersion = QStringLiteral("v1");
const QString PasskeyExporter::PayloadPrefix = QStringLiteral("keepassxc-passkey:v1:");
const int PasskeyExporter::MaxPayloadSize = 128 * 1024;

PasskeyExporter::PasskeyExporter(QWidget* parent)
    : m_parent(parent)
{
}

void PasskeyExporter::showExportDialog(const QList<Entry*>& items)
{
    if (items.isEmpty()) {
        return;
    }

    PasskeyExportDialog passkeyExportDialog(m_parent);
    passkeyExportDialog.setEntries(items);
    auto ret = passkeyExportDialog.exec();

    if (ret == QDialog::Accepted) {
        // Select folder
        auto folder = passkeyExportDialog.selectExportFolder();
        if (folder.isEmpty()) {
            return;
        }

        const auto selectedItems = passkeyExportDialog.getSelectedItems();
        for (const auto& item : selectedItems) {
            auto entry = items[item->row()];
            exportSelectedEntry(entry, folder);
        }
    }
}

/**
 * Builds the JSON representation of a single passkey credential.
 *
 * This is the single source of truth for the exported data. Both the .passkey file and the
 * clipboard payload are built from it, so the two can never carry different fields.
 *
 * {
 *      "privateKey": <private key>,
 *      "relyingParty: <relying party>,
 *      "url": <URL>,
 *      "userHandle": <user handle>,
 *      "credentialId": <generated credential id>,
 *      "username:" <username>
 * }
 */
QJsonObject PasskeyExporter::buildPasskeyObject(const Entry* entry)
{
    if (!entry) {
        return {};
    }

    QJsonObject passkeyObject;
    passkeyObject["relyingParty"] = entry->attributes()->value(EntryAttributes::KPEX_PASSKEY_RELYING_PARTY);
    passkeyObject["url"] = entry->url();
    passkeyObject["username"] = passkeyUtils()->getUsernameFromEntry(entry);
    passkeyObject["credentialId"] = passkeyUtils()->getCredentialIdFromEntry(entry);
    passkeyObject["userHandle"] = entry->attributes()->value(EntryAttributes::KPEX_PASSKEY_USER_HANDLE);
    passkeyObject["privateKey"] = entry->attributes()->value(EntryAttributes::KPEX_PASSKEY_PRIVATE_KEY_PEM);

    return passkeyObject;
}

/**
 * Wraps a single passkey into the versioned, self-identifying clipboard payload.
 */
QString PasskeyExporter::buildPayload(const Entry* entry)
{
    if (!entry) {
        return {};
    }

    const QJsonDocument document(buildPasskeyObject(entry));
    return PayloadPrefix + QString::fromLatin1(document.toJson(QJsonDocument::Compact).toBase64());
}

/**
 * Wraps several passkeys into a JSON array inside the same versioned clipboard payload.
 */
QString PasskeyExporter::buildPayload(const QList<Entry*>& entries)
{
    QJsonArray passkeyArray;
    for (const auto* entry : entries) {
        if (!entry) {
            continue;
        }

        passkeyArray.append(buildPasskeyObject(entry));
    }

    if (passkeyArray.isEmpty()) {
        return {};
    }

    const QJsonDocument document(passkeyArray);
    return PayloadPrefix + QString::fromLatin1(document.toJson(QJsonDocument::Compact).toBase64());
}

/**
 * Asks the user to confirm putting unencrypted passkey private keys on the clipboard.
 *
 * The safe choice (Cancel) is the default button and the accepting button carries an explicit
 * action label. This confirmation is deliberately not suppressible and offers no way to remember
 * the answer.
 */
bool PasskeyExporter::confirmClipboardExport(int entryCount)
{
    if (entryCount <= 0) {
        return false;
    }

    // Only promise automatic clearing when it is actually going to happen. Clipboard::setText
    // silently skips the clear timer when the user has turned the setting off, and a security
    // warning that overstates its own mitigation is worse than one that admits the gap.
    const auto autoClearEnabled = config()->get(Config::Security_ClearClipboard).toBool();
    const auto clearTimeout = config()->get(Config::Security_ClearClipboardTimeout).toInt();

    const auto clearingText =
        autoClearEnabled
            ? tr("KeePassXC clears the clipboard automatically after %n second(s), but anything that has already "
                 "read the clipboard keeps its own copy of the private key.",
                 "",
                 clearTimeout)
            : tr("Automatic clipboard clearing is turned off in your settings, so the private key will stay on the "
                 "clipboard until something else replaces it. You will have to clear it yourself.");

    const auto text =
        tr("You are about to copy %n passkey(s) to the clipboard as plain text.", "", entryCount) + "\n\n"
        + tr("The copied text contains the passkey private key in plain text. It is not an encrypted export.") + "\n\n"
        + tr("Anyone who obtains this text can sign in as you at that site, and the site cannot tell the difference "
             "between them and you.")
        + "\n\n"
        + tr("The clipboard can be read by any other application running on this computer, and it may be "
             "synchronised to your other devices or to a cloud clipboard history.")
        + "\n\n" + clearingText + "\n\n"
        + tr("Never paste this text into a chat, an issue, a bug report, a pastebin, or a support ticket.");

    const auto answer = MessageBox::warning(m_parent,
                                            tr("Copy Passkey to Clipboard?"),
                                            text,
                                            MessageBox::CopyAnyway | MessageBox::Cancel,
                                            MessageBox::Cancel);

    return answer == MessageBox::CopyAnyway;
}

void PasskeyExporter::exportEntryToClipboard(const Entry* entry)
{
    if (!entry || !entry->hasPasskey()) {
        MessageBox::information(m_parent,
                                tr("Passkey Export Failed"),
                                tr("The selected entry does not contain a passkey that can be exported."));
        return;
    }

    if (!confirmClipboardExport(1)) {
        return;
    }

    // Rely on the shared clipboard timer so the payload is cleared like any other secret.
    clipboard()->setText(buildPayload(entry), true);
}

void PasskeyExporter::exportEntriesToClipboard(const QList<Entry*>& entries)
{
    QList<Entry*> passkeyEntries;
    for (const auto& entry : entries) {
        if (entry && entry->hasPasskey()) {
            passkeyEntries << entry;
        }
    }

    if (passkeyEntries.isEmpty()) {
        MessageBox::information(m_parent,
                                tr("Passkey Export Failed"),
                                tr("None of the selected entries contain a passkey that can be exported."));
        return;
    }

    if (!confirmClipboardExport(static_cast<int>(passkeyEntries.count()))) {
        return;
    }

    // A single entry is written as a plain object so the payload matches a .passkey file exactly.
    const auto payload =
        passkeyEntries.count() == 1 ? buildPayload(passkeyEntries.first()) : buildPayload(passkeyEntries);

    clipboard()->setText(payload, true);
}

/**
 * Creates an export file for a Passkey credential
 *
 * File contents in JSON, see buildPasskeyObject().
 */
void PasskeyExporter::exportSelectedEntry(const Entry* entry, const QString& folder)
{
    const auto fullPath = QString("%1/%2.passkey").arg(folder, Tools::cleanFilename(entry->title()));
    if (QFile::exists(fullPath)) {
        auto dialogResult = MessageBox::warning(m_parent,
                                                tr("Overwrite Existing File?"),
                                                tr("File \"%1.passkey\" already exists.\n"
                                                   "Do you want to overwrite it?\n")
                                                    .arg(entry->title()),
                                                MessageBox::Yes | MessageBox::No);

        if (dialogResult != MessageBox::Yes) {
            return;
        }
    }

    QFile passkeyFile(fullPath);
    if (!passkeyFile.open(QIODevice::WriteOnly)) {
        MessageBox::information(
            m_parent, tr("Cannot open file"), tr("Cannot open file \"%1\" for writing.").arg(fullPath));
        return;
    }

    QJsonDocument document(buildPasskeyObject(entry));
    if (passkeyFile.write(document.toJson()) < 0) {
        MessageBox::information(
            nullptr, tr("Cannot write to file"), tr("Cannot open file \"%1\" for writing.").arg(fullPath));
    }

    passkeyFile.close();
}
