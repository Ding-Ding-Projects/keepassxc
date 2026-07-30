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

#ifndef KEEPASSXC_PASSKEYCLIPBOARDIMPORTDIALOG_H
#define KEEPASSXC_PASSKEYCLIPBOARDIMPORTDIALOG_H

#include <QDialog>
#include <QScopedPointer>
#include <QString>

namespace Ui
{
    class PasskeyClipboardImportDialog;
}

/**
 * Lets the user paste a "keepassxc-passkey:v1:<base64>" payload and tells them, while they type,
 * whether it can be imported and what it contains.
 */
class PasskeyClipboardImportDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PasskeyClipboardImportDialog(QWidget* parent = nullptr);
    ~PasskeyClipboardImportDialog() override;

    QString payload() const;

private slots:
    void pasteFromClipboard();
    void validatePayload();

private:
    QScopedPointer<Ui::PasskeyClipboardImportDialog> m_ui;
    bool m_valid = false;
};

#endif // KEEPASSXC_PASSKEYCLIPBOARDIMPORTDIALOG_H
