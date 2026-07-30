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

#ifndef KEEPASSXC_PASSKEYIMPORTER_H
#define KEEPASSXC_PASSKEYIMPORTER_H

#include "core/Database.h"
#include <QFile>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QUuid>
#include <QWidget>

class Entry;

class PasskeyImporter : public QObject
{
    Q_OBJECT

public:
    /**
     * Why a pasted clipboard payload was rejected. Every value maps to its own message so the
     * user is never told just "invalid passkey".
     */
    enum class PayloadError
    {
        None = 0,
        Empty,
        TooLarge,
        UnknownPrefix,
        UnsupportedVersion,
        InvalidBase64,
        InvalidJson,
        MissingKeys,
        InvalidPrivateKey,
    };

    struct PayloadParseResult
    {
        PayloadError error = PayloadError::None;
        QString errorMessage;
        QStringList missingKeys;
        QList<QJsonObject> passkeys;

        bool isValid() const
        {
            return error == PayloadError::None;
        }
    };

    explicit PasskeyImporter(QWidget* parent = nullptr);

    void importPasskey(QSharedPointer<Database>& database, Entry* entry = nullptr);
    void importPasskeyFromClipboard(QSharedPointer<Database>& database, Entry* entry = nullptr);
    bool importFromPayload(const QString& payload, QSharedPointer<Database>& database, Entry* entry);
    bool showImportDialog(QSharedPointer<Database>& database,
                          Entry* entry,
                          const QString& url,
                          const QString& relyingParty,
                          const QString& username,
                          const QString& credentialId,
                          const QString& userHandle,
                          const QString& privateKey,
                          const QString& titleText = {},
                          const QString& infoText = {},
                          const QString& importButtonText = {});

    // The six keys a passkey document must carry, shared by the file and clipboard import paths.
    static QStringList requiredPasskeyKeys();
    static PayloadParseResult parsePayload(const QString& payload);

    /**
     * Escapes and elides a string taken from a pasted payload so it can be shown safely.
     *
     * Payload fields are unbounded within the overall size limit and reach both rich-text message
     * boxes and word-wrapped labels, so every payload-derived string shown to the user goes through
     * this first.
     */
    static QString sanitizeForDisplay(const QString& value, int maxLength = 64);

private:
    void importSelectedFile(QFile& file, QSharedPointer<Database>& database, Entry* entry);
    Group* getDefaultGroup(QSharedPointer<Database>& database) const;

private:
    QPointer<QWidget> m_parent;
};

#endif // KEEPASSXC_PASSKEYIMPORTER_H
