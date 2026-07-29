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

#ifndef KEEPASSXC_PASSKEYEXPORTER_H
#define KEEPASSXC_PASSKEYEXPORTER_H

#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QWidget>

class Entry;

class PasskeyExporter : public QObject
{
    Q_OBJECT

public:
    /**
     * Clipboard payload format:
     *
     *      keepassxc-passkey:v1:<base64 of the compact JSON document>
     *
     * The JSON document is either a single passkey object, using exactly the same keys as a
     * .passkey file, or an array of such objects when several passkeys are exported at once.
     * The prefix makes the payload self-identifying so the importer can explain precisely what
     * is wrong with a pasted string instead of reporting a generic failure.
     */
    static const QString PayloadScheme;
    static const QString PayloadVersion;
    static const QString PayloadPrefix;

    // Maximum accepted payload size in bytes (128 KiB). Bounds the importer before decoding.
    static const int MaxPayloadSize;

    explicit PasskeyExporter(QWidget* parent = nullptr);

    void showExportDialog(const QList<Entry*>& items);

    bool confirmClipboardExport(int entryCount);
    void exportEntryToClipboard(const Entry* entry);
    void exportEntriesToClipboard(const QList<Entry*>& entries);

    static QJsonObject buildPasskeyObject(const Entry* entry);
    static QString buildPayload(const Entry* entry);
    static QString buildPayload(const QList<Entry*>& entries);

private:
    void exportSelectedEntry(const Entry* entry, const QString& folder);

private:
    QPointer<QWidget> m_parent;
};

#endif // KEEPASSXC_PASSKEYEXPORTER_H
