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

#include "PasskeyImporter.h"
#include "PasskeyClipboardImportDialog.h"
#include "PasskeyExporter.h"
#include "PasskeyImportDialog.h"
#include "browser/BrowserMessageBuilder.h"
#include "browser/BrowserPasskeys.h"
#include "browser/BrowserService.h"
#include "core/Entry.h"
#include "core/EntryAttributes.h"
#include "core/Group.h"
#include "core/Tools.h"
#include "gui/FileDialog.h"
#include "gui/MessageBox.h"
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonValue>
#include <QUuid>

static const QString IMPORTED_PASSKEYS_GROUP = QStringLiteral("Imported Passkeys");

namespace
{
    /**
     * The private key must be a PEM private key. This is exactly the check the .passkey file
     * import applies, kept in one place so both paths enforce the same standard.
     */
    bool isPemPrivateKey(const QString& privateKey)
    {
        return privateKey.startsWith(EntryAttributes::KPEX_PASSKEY_PRIVATE_KEY_START)
               && privateKey.trimmed().endsWith(EntryAttributes::KPEX_PASSKEY_PRIVATE_KEY_END);
    }
} // namespace

/**
 * Makes a string taken from a pasted payload safe to put in front of the user.
 *
 * The 128 KiB payload bound says nothing about any single field, and a pasted string reaches both
 * rich-text message boxes and word-wrapped labels. Escaping stops it injecting markup; eliding stops
 * a single enormous field stretching a modal dialog past the edge of the screen.
 */
QString PasskeyImporter::sanitizeForDisplay(const QString& value, int maxLength)
{
    auto text = value.simplified();
    if (text.length() > maxLength) {
        text.truncate(maxLength);
        text += QStringLiteral("…");
    }
    return text.toHtmlEscaped();
}

PasskeyImporter::PasskeyImporter(QWidget* parent)
    : m_parent(parent)
{
}

QStringList PasskeyImporter::requiredPasskeyKeys()
{
    return QStringList() << "relyingParty" << "url" << "username" << "credentialId" << "userHandle" << "privateKey";
}

void PasskeyImporter::importPasskey(QSharedPointer<Database>& database, Entry* entry)
{
    auto filter = QString("%1 (*.passkey);;%2 (*)").arg(tr("Passkey file"), tr("All files"));
    auto fileName =
        fileDialog()->getOpenFileName(m_parent, tr("Open Passkey File"), FileDialog::getLastDir("passkey"), filter);
    if (fileName.isEmpty()) {
        return;
    }

    FileDialog::saveLastDir("passkey", fileName, true);

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        MessageBox::information(
            nullptr, tr("Cannot open file"), tr("Cannot open file \"%1\" for reading.").arg(fileName));
        return;
    }

    importSelectedFile(file, database, entry);
}

void PasskeyImporter::importPasskeyFromClipboard(QSharedPointer<Database>& database, Entry* entry)
{
    PasskeyClipboardImportDialog clipboardImportDialog(m_parent);
    if (clipboardImportDialog.exec() != QDialog::Accepted) {
        return;
    }

    importFromPayload(clipboardImportDialog.payload(), database, entry);
}

/**
 * Decodes and validates a "keepassxc-passkey:v1:<base64>" payload.
 *
 * Validation happens in the order the payload is consumed, so the first thing that is actually
 * wrong is what gets reported. The payload itself is never logged or written anywhere.
 */
PasskeyImporter::PayloadParseResult PasskeyImporter::parsePayload(const QString& payload)
{
    PayloadParseResult result;

    // Bound the input before spending any work on decoding it.
    if (payload.size() > PasskeyExporter::MaxPayloadSize) {
        result.error = PayloadError::TooLarge;
        result.errorMessage = tr("The pasted text is too large to be a passkey (%1 KiB). The maximum size is %2 KiB.")
                                  .arg((payload.size() + 1023) / 1024)
                                  .arg(PasskeyExporter::MaxPayloadSize / 1024);
        return result;
    }

    const auto trimmed = payload.trimmed();
    if (trimmed.isEmpty()) {
        result.error = PayloadError::Empty;
        result.errorMessage = tr("No passkey text was pasted. Paste the text you copied from KeePassXC.");
        return result;
    }

    const auto schemePrefix = PasskeyExporter::PayloadScheme + QLatin1Char(':');
    if (!trimmed.startsWith(schemePrefix, Qt::CaseInsensitive)) {
        result.error = PayloadError::UnknownPrefix;
        result.errorMessage = tr("The pasted text is not a KeePassXC passkey. It has to start with \"%1\".")
                                  .arg(PasskeyExporter::PayloadPrefix);
        return result;
    }

    const auto remainder = trimmed.mid(schemePrefix.length());
    const auto versionSeparator = remainder.indexOf(QLatin1Char(':'));
    const auto version = versionSeparator < 0 ? remainder : remainder.left(versionSeparator);
    if (versionSeparator < 0 || version != PasskeyExporter::PayloadVersion) {
        result.error = PayloadError::UnsupportedVersion;
        result.errorMessage = tr("The pasted passkey text uses the unsupported format version \"%1\". "
                                 "This version of KeePassXC can only read \"%2\".")
                                  .arg(sanitizeForDisplay(version), PasskeyExporter::PayloadVersion);
        return result;
    }

    // Tolerate line breaks and spaces introduced by whatever the text travelled through.
    auto encoded = remainder.mid(versionSeparator + 1).simplified();
    encoded.remove(QLatin1Char(' '));

    const auto decodedResult = QByteArray::fromBase64Encoding(
        encoded.toLatin1(), QByteArray::Base64Encoding | QByteArray::AbortOnBase64DecodingErrors);
    if (!decodedResult) {
        result.error = PayloadError::InvalidBase64;
        result.errorMessage = tr("The pasted passkey text is damaged: the part after \"%1\" is not valid base64. "
                                 "Copy the passkey again and make sure nothing was cut off.")
                                  .arg(PasskeyExporter::PayloadPrefix);
        return result;
    }

    QJsonParseError parseError{};
    const auto document = QJsonDocument::fromJson(decodedResult.decoded, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        result.error = PayloadError::InvalidJson;
        result.errorMessage =
            tr("The pasted passkey text does not contain valid JSON data (%1).").arg(parseError.errorString());
        return result;
    }

    QList<QJsonObject> passkeyObjects;
    if (document.isObject()) {
        passkeyObjects << document.object();
    } else if (document.isArray()) {
        const auto passkeyArray = document.array();
        for (const auto& value : passkeyArray) {
            if (!value.isObject()) {
                result.error = PayloadError::InvalidJson;
                result.errorMessage = tr("The pasted passkey text contains a list with an item that is not a passkey.");
                return result;
            }

            passkeyObjects << value.toObject();
        }

        if (passkeyObjects.isEmpty()) {
            result.error = PayloadError::InvalidJson;
            result.errorMessage = tr("The pasted passkey text contains an empty list of passkeys.");
            return result;
        }
    } else {
        result.error = PayloadError::InvalidJson;
        result.errorMessage = tr("The pasted passkey text does not contain a passkey object.");
        return result;
    }

    const auto passkeyCount = passkeyObjects.count();
    const auto multiple = passkeyCount > 1;
    for (qsizetype i = 0; i < passkeyCount; ++i) {
        const auto& passkeyObject = passkeyObjects.at(i);

        const auto missingKeys = Tools::getMissingValuesFromList<QString>(passkeyObject.keys(), requiredPasskeyKeys());
        if (!missingKeys.isEmpty()) {
            result.error = PayloadError::MissingKeys;
            result.missingKeys = missingKeys;
            result.errorMessage =
                multiple ? tr("Passkey %1 in the pasted text is missing the following data: %2")
                               .arg(QString::number(i + 1), missingKeys.join(", "))
                         : tr("The pasted passkey text is missing the following data: %1").arg(missingKeys.join(", "));
            return result;
        }

        if (!isPemPrivateKey(passkeyObject["privateKey"].toString())) {
            result.error = PayloadError::InvalidPrivateKey;
            result.errorMessage =
                multiple ? tr("The private key of passkey %1 in the pasted text is missing or malformed. "
                              "It has to be a PEM private key starting with \"%2\".")
                               .arg(QString::number(i + 1), EntryAttributes::KPEX_PASSKEY_PRIVATE_KEY_START)
                         : tr("The private key in the pasted passkey text is missing or malformed. "
                              "It has to be a PEM private key starting with \"%1\".")
                               .arg(EntryAttributes::KPEX_PASSKEY_PRIVATE_KEY_START);
            return result;
        }
    }

    result.passkeys = passkeyObjects;
    return result;
}

/**
 * Imports one or more passkeys from a clipboard payload. Once the payload is decoded this routes
 * into showImportDialog(), the very same code path the .passkey file import uses, so a clipboard
 * import and a file import produce an identical entry.
 */
bool PasskeyImporter::importFromPayload(const QString& payload, QSharedPointer<Database>& database, Entry* entry)
{
    const auto result = parsePayload(payload);
    if (!result.isValid()) {
        MessageBox::information(m_parent, tr("Passkey Import Failed"), result.errorMessage);
        return false;
    }

    if (entry && result.passkeys.count() > 1) {
        MessageBox::information(m_parent,
                                tr("Passkey Import Failed"),
                                tr("The pasted text contains %n passkey(s), but only a single passkey can be added to "
                                   "the selected entry. Import them without selecting an entry instead.",
                                   "",
                                   static_cast<int>(result.passkeys.count())));
        return false;
    }

    auto imported = false;
    for (const auto& passkeyObject : result.passkeys) {
        const auto relyingParty = passkeyObject["relyingParty"].toString();
        const auto url = passkeyObject["url"].toString();
        const auto username = passkeyObject["username"].toString();
        const auto credentialId = passkeyObject["credentialId"].toString();
        const auto userHandle = passkeyObject["userHandle"].toString();
        const auto privateKey = passkeyObject["privateKey"].toString();

        if (!showImportDialog(database, entry, url, relyingParty, username, credentialId, userHandle, privateKey)) {
            // The user cancelled, do not keep asking for the remaining passkeys
            break;
        }

        imported = true;
    }

    return imported;
}

void PasskeyImporter::importSelectedFile(QFile& file, QSharedPointer<Database>& database, Entry* entry)
{
    const auto fileData = file.readAll();
    const auto passkeyObject = browserMessageBuilder()->getJsonObject(fileData);
    if (passkeyObject.isEmpty()) {
        MessageBox::information(m_parent,
                                tr("Passkey Import Failed"),
                                tr("Cannot import passkey file \"%1\". Data is missing.").arg(file.fileName()));
        return;
    }

    const auto privateKey = passkeyObject["privateKey"].toString();
    const auto missingKeys = Tools::getMissingValuesFromList<QString>(passkeyObject.keys(), requiredPasskeyKeys());
    if (!missingKeys.isEmpty()) {
        MessageBox::information(m_parent,
                                tr("Passkey Import Failed"),
                                tr("Cannot import passkey file \"%1\".\nThe following data is missing:\n%2")
                                    .arg(file.fileName(), missingKeys.join(", ")));
    } else if (!isPemPrivateKey(privateKey)) {
        MessageBox::information(
            m_parent,
            tr("Passkey Import Failed"),
            tr("Cannot import passkey file \"%1\". Private key is missing or malformed.").arg(file.fileName()));
    } else {
        const auto relyingParty = passkeyObject["relyingParty"].toString();
        const auto url = passkeyObject["url"].toString();
        const auto username = passkeyObject["username"].toString();
        const auto credentialId = passkeyObject["credentialId"].toString();
        const auto userHandle = passkeyObject["userHandle"].toString();
        showImportDialog(database, entry, url, relyingParty, username, credentialId, userHandle, privateKey);
    }
}

bool PasskeyImporter::showImportDialog(QSharedPointer<Database>& database,
                                       Entry* entry,
                                       const QString& url,
                                       const QString& relyingParty,
                                       const QString& username,
                                       const QString& credentialId,
                                       const QString& userHandle,
                                       const QString& privateKey,
                                       const QString& titleText,
                                       const QString& infoText,
                                       const QString& importButtonText)
{
    PasskeyImportDialog passkeyImportDialog(m_parent);
    passkeyImportDialog.setInfo(
        relyingParty, username, database, entry != nullptr, titleText, infoText, importButtonText);

    auto ret = passkeyImportDialog.exec();
    if (ret != QDialog::Accepted) {
        return false;
    }

    auto db = passkeyImportDialog.getSelectedDatabase();
    if (!db) {
        db = database;
    }

    // Store to entry if given directly
    if (entry) {
        browserService()->addPasskeyToEntry(
            entry, relyingParty, relyingParty, username, credentialId, userHandle, privateKey);
        return true;
    }

    // Import to entry selected instead of creating a new one
    if (!passkeyImportDialog.createNewEntry()) {
        auto groupUuid = passkeyImportDialog.getSelectedGroupUuid();
        auto group = db->rootGroup()->findGroupByUuid(groupUuid);

        if (group) {
            auto selectedEntry = group->findEntryByUuid(passkeyImportDialog.getSelectedEntryUuid());
            if (selectedEntry) {
                browserService()->addPasskeyToEntry(
                    selectedEntry, relyingParty, relyingParty, username, credentialId, userHandle, privateKey);
            }
        }

        return true;
    }

    // Group settings. Use default group "Imported Passkeys" if user did not select a specific one.
    Group* group = nullptr;

    // Attempt to use the selected group
    if (!passkeyImportDialog.useDefaultGroup()) {
        auto groupUuid = passkeyImportDialog.getSelectedGroupUuid();
        group = db->rootGroup()->findGroupByUuid(groupUuid);
    }

    // Use default group if requested or if the selected group does not exist
    if (!group) {
        group = getDefaultGroup(db);
    }

    browserService()->addPasskeyToGroup(
        db, group, url, relyingParty, relyingParty, username, credentialId, userHandle, privateKey);

    return true;
}

Group* PasskeyImporter::getDefaultGroup(QSharedPointer<Database>& database) const
{
    auto defaultGroup = database->rootGroup()->findGroupByPath(IMPORTED_PASSKEYS_GROUP);

    // Create the default group if it does not exist
    if (!defaultGroup) {
        defaultGroup = new Group();
        defaultGroup->setName(IMPORTED_PASSKEYS_GROUP);
        defaultGroup->setUuid(QUuid::createUuid());
        defaultGroup->setParent(database->rootGroup());
    }

    return defaultGroup;
}
