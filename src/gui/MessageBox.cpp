/*
 *  Copyright (C) 2026 KeePassXC Team <team@keepassxc.org>
 *  Copyright (C) 2013 Felix Geyer <debfx@fobos.de>
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

#include "MessageBox.h"

#include "gui/MainWindow.h"
#include "gui/material/MaterialButtons.h"
#include "gui/material/MaterialDialog.h"
#include "gui/material/MaterialTheme.h"

#include <QApplication>
#include <QCheckBox>
#include <QEventLoop>
#include <QHash>
#include <QLayout>
#include <QMap>
#include <QPointer>
#include <QPushButton>
#include <QWindow>

namespace
{
    /** The window a Material sheet can be laid over, or nullptr for none. */
    QWidget* sheetHost(QWidget* parent)
    {
        QWidget* host = parent ? parent->window() : nullptr;
        if (!host || !host->isVisible()) {
            host = getMainWindow();
        }
        if (!host || !host->isVisible() || host->isMinimized()) {
            return nullptr;
        }
        // A modal dialog of its own already has the desktop's attention; a
        // sheet under it would be unreachable.
        if (QApplication::activeModalWidget() && QApplication::activeModalWidget() != host
            && !QApplication::activeModalWidget()->isAncestorOf(parent)) {
            return QApplication::activeModalWidget()->isVisible() ? QApplication::activeModalWidget() : nullptr;
        }
        return host;
    }

    QString symbolFor(QMessageBox::Icon icon)
    {
        switch (icon) {
        case QMessageBox::Critical:
            return QStringLiteral("error");
        case QMessageBox::Warning:
            return QStringLiteral("warning");
        case QMessageBox::Question:
            return QStringLiteral("help");
        case QMessageBox::Information:
            return QStringLiteral("info");
        default:
            return {};
        }
    }
} // namespace

QWindow* MessageBox::m_overrideParent(nullptr);

MessageBox::Button MessageBox::m_nextAnswer(MessageBox::NoButton);

QHash<QAbstractButton*, MessageBox::Button> MessageBox::m_addedButtonLookup =
    QHash<QAbstractButton*, MessageBox::Button>();

QMap<MessageBox::Button, std::pair<QString, QMessageBox::ButtonRole>> MessageBox::m_buttonDefs =
    QMap<MessageBox::Button, std::pair<QString, QMessageBox::ButtonRole>>();

void MessageBox::initializeButtonDefs()
{
    m_buttonDefs = QMap<Button, std::pair<QString, QMessageBox::ButtonRole>>{
        // Reimplementation of Qt StandardButtons
        {Ok, {stdButtonText(QMessageBox::Ok), QMessageBox::ButtonRole::AcceptRole}},
        {Open, {stdButtonText(QMessageBox::Open), QMessageBox::ButtonRole::AcceptRole}},
        {Save, {stdButtonText(QMessageBox::Save), QMessageBox::ButtonRole::AcceptRole}},
        {Cancel, {stdButtonText(QMessageBox::Cancel), QMessageBox::ButtonRole::RejectRole}},
        {Close, {stdButtonText(QMessageBox::Close), QMessageBox::ButtonRole::RejectRole}},
        {Discard, {stdButtonText(QMessageBox::Discard), QMessageBox::ButtonRole::DestructiveRole}},
        {Apply, {stdButtonText(QMessageBox::Apply), QMessageBox::ButtonRole::ApplyRole}},
        {Reset, {stdButtonText(QMessageBox::Reset), QMessageBox::ButtonRole::ResetRole}},
        {RestoreDefaults, {stdButtonText(QMessageBox::RestoreDefaults), QMessageBox::ButtonRole::ResetRole}},
        {Help, {stdButtonText(QMessageBox::Help), QMessageBox::ButtonRole::HelpRole}},
        {SaveAll, {stdButtonText(QMessageBox::SaveAll), QMessageBox::ButtonRole::AcceptRole}},
        {Yes, {stdButtonText(QMessageBox::Yes), QMessageBox::ButtonRole::YesRole}},
        {YesToAll, {stdButtonText(QMessageBox::YesToAll), QMessageBox::ButtonRole::YesRole}},
        {No, {stdButtonText(QMessageBox::No), QMessageBox::ButtonRole::NoRole}},
        {NoToAll, {stdButtonText(QMessageBox::NoToAll), QMessageBox::ButtonRole::NoRole}},
        {Abort, {stdButtonText(QMessageBox::Abort), QMessageBox::ButtonRole::RejectRole}},
        {Retry, {stdButtonText(QMessageBox::Retry), QMessageBox::ButtonRole::AcceptRole}},
        {Ignore, {stdButtonText(QMessageBox::Ignore), QMessageBox::ButtonRole::AcceptRole}},

        // KeePassXC Buttons
        {Overwrite, {QMessageBox::tr("Overwrite"), QMessageBox::ButtonRole::AcceptRole}},
        {Delete, {QMessageBox::tr("Delete"), QMessageBox::ButtonRole::AcceptRole}},
        {Move, {QMessageBox::tr("Move"), QMessageBox::ButtonRole::AcceptRole}},
        {Empty, {QMessageBox::tr("Empty"), QMessageBox::ButtonRole::AcceptRole}},
        {Remove, {QMessageBox::tr("Remove"), QMessageBox::ButtonRole::AcceptRole}},
        {Skip, {QMessageBox::tr("Skip"), QMessageBox::ButtonRole::AcceptRole}},
        {Disable, {QMessageBox::tr("Disable"), QMessageBox::ButtonRole::AcceptRole}},
        {Merge, {QMessageBox::tr("Merge"), QMessageBox::ButtonRole::AcceptRole}},
        {Continue, {QMessageBox::tr("Continue"), QMessageBox::ButtonRole::AcceptRole}},
        {ContinueWithWeakPass, {QMessageBox::tr("Continue with weak password"), QMessageBox::ButtonRole::AcceptRole}},
        {CopyAnyway, {QMessageBox::tr("Copy anyway"), QMessageBox::ButtonRole::DestructiveRole}},
        {OpenAnyway, {QMessageBox::tr("Open database anyway"), QMessageBox::ButtonRole::AcceptRole}},
        {RetryWithEmptyPassword, {QMessageBox::tr("Retry with empty password"), QMessageBox::ButtonRole::AcceptRole}},
        {ContinueWithoutPassword, {QMessageBox::tr("Continue without password"), QMessageBox::ButtonRole::AcceptRole}},
        {KeepNumber, {QMessageBox::tr("Understood, keep number"), QMessageBox::ButtonRole::AcceptRole}},
    };
}

QString MessageBox::stdButtonText(QMessageBox::StandardButton button)
{
    QMessageBox buttonHost;
    return buttonHost.addButton(button)->text();
}

MessageBox::Button MessageBox::messageBox(QWidget* parent,
                                          QMessageBox::Icon icon,
                                          const QString& title,
                                          const QString& text,
                                          MessageBox::Buttons buttons,
                                          MessageBox::Button defaultButton,
                                          MessageBox::Action action,
                                          QCheckBox* checkbox)
{
    if (m_nextAnswer == MessageBox::NoButton) {
        // Every message is a Material 3 dialog: a sheet over the window with
        // the symbol badge, headline, supporting text and text/filled actions.
        // The desktop's own message box only stands in when there is no
        // window on screen to lay the sheet over, or when a test has pinned
        // the transient parent.
        if (QWidget* host = m_overrideParent ? nullptr : sheetHost(parent)) {
            return materialMessageBox(host, icon, title, text, buttons, defaultButton, checkbox);
        }
        QMessageBox msgBox(parent);
        msgBox.setTextFormat(Qt::RichText);
        msgBox.setIcon(icon);
        msgBox.setWindowTitle(title);
        // Replace newlines with HTML line breaks
        auto fixedText = text;
        msgBox.setText(fixedText.replace("\n", "<br>"));

        if (m_overrideParent) {
            // Force the creation of the QWindow, without this windowHandle() will return nullptr
            msgBox.winId();
            auto msgBoxWindow = msgBox.windowHandle();
            Q_ASSERT(msgBoxWindow);
            msgBoxWindow->setTransientParent(m_overrideParent);
        }

        for (uint64_t b = First; b <= Last; b <<= 1) {
            if (b & buttons) {
                QString buttonText = m_buttonDefs[static_cast<Button>(b)].first;
                QMessageBox::ButtonRole role = m_buttonDefs[static_cast<Button>(b)].second;

                auto buttonPtr = msgBox.addButton(buttonText, role);
                m_addedButtonLookup.insert(buttonPtr, static_cast<Button>(b));
            }
        }

        if (defaultButton != MessageBox::NoButton) {
            QList<QAbstractButton*> defPtrList = m_addedButtonLookup.keys(defaultButton);
            if (defPtrList.count() > 0) {
                msgBox.setDefaultButton(static_cast<QPushButton*>(defPtrList[0]));
            }
        }

        if (checkbox) {
            checkbox->setParent(&msgBox);
            msgBox.setCheckBox(checkbox);
        }

        if (action == MessageBox::Raise) {
            msgBox.setWindowFlags(Qt::WindowStaysOnTopHint);
            msgBox.activateWindow();
            msgBox.raise();
        }
        msgBox.layout()->setSizeConstraint(QLayout::SetMinimumSize);
        msgBox.exec();

        Button returnButton = m_addedButtonLookup[msgBox.clickedButton()];
        m_addedButtonLookup.clear();
        return returnButton;

    } else {
        MessageBox::Button returnButton = m_nextAnswer;
        m_nextAnswer = MessageBox::NoButton;
        return returnButton;
    }
}

MessageBox::Button MessageBox::materialMessageBox(QWidget* host,
                                                  QMessageBox::Icon icon,
                                                  const QString& title,
                                                  const QString& text,
                                                  MessageBox::Buttons buttons,
                                                  MessageBox::Button defaultButton,
                                                  QCheckBox* checkbox)
{
    QPointer<Material::Dialog> dialog = new Material::Dialog(host);
    dialog->setProperty("destructive", icon == QMessageBox::Critical);
    dialog->setSymbol(symbolFor(icon));
    dialog->setHeadline(title);
    QString body = text;
    dialog->setSupportingText(body.replace(QStringLiteral("\n"), QStringLiteral("<br>")));
    if (checkbox) {
        dialog->addContent(checkbox);
    }

    // The default button is the one filled action; with none named the first
    // accepting button is. Everything else is a text button, in the order
    // the caller listed them, which is the order people read them in.
    Button primary = defaultButton;
    if (primary == NoButton) {
        for (uint64_t b = First; b <= Last; b <<= 1) {
            if ((b & buttons) && m_buttonDefs[static_cast<Button>(b)].second == QMessageBox::ButtonRole::AcceptRole) {
                primary = static_cast<Button>(b);
                break;
            }
        }
    }
    Button dismiss = NoButton;
    for (Button candidate : {Cancel, Close, No, Abort, Ignore, Ok}) {
        if (candidate & buttons) {
            dismiss = candidate;
            break;
        }
    }

    Button answer = NoButton;
    bool answered = false;
    QEventLoop loop;
    for (uint64_t b = First; b <= Last; b <<= 1) {
        if (!(b & buttons)) {
            continue;
        }
        const Button button = static_cast<Button>(b);
        const auto& definition = m_buttonDefs[button];
        Material::ButtonBase* action = dialog->addAction(definition.first, button == primary);
        if (definition.second == QMessageBox::ButtonRole::DestructiveRole) {
            action->setRoles(Material::Role::Error, Material::Role::OnError);
        }
        QObject::connect(action, &QAbstractButton::clicked, &loop, [&, button] {
            answer = button;
            answered = true;
            loop.quit();
        });
    }
    // Escape or the scrim: the way out that loses nothing, or nothing at all
    // when the caller offered no such button.
    QObject::connect(dialog, &Material::Overlay::closed, &loop, [&] {
        if (!answered) {
            answer = dismiss;
            answered = true;
        }
        loop.quit();
    });
    dialog->setDismissable(dismiss != NoButton);

    dialog->openOverlay();
    loop.exec();

    if (checkbox) {
        // The caller owns the box and reads it after the call.
        checkbox->setParent(nullptr);
        checkbox->hide();
    }
    if (dialog) {
        dialog->hide();
        dialog->deleteLater();
    }
    return answer;
}

MessageBox::Button MessageBox::critical(QWidget* parent,
                                        const QString& title,
                                        const QString& text,
                                        MessageBox::Buttons buttons,
                                        MessageBox::Button defaultButton,
                                        MessageBox::Action action,
                                        QCheckBox* checkbox)
{
    return messageBox(parent, QMessageBox::Critical, title, text, buttons, defaultButton, action, checkbox);
}

MessageBox::Button MessageBox::warning(QWidget* parent,
                                       const QString& title,
                                       const QString& text,
                                       MessageBox::Buttons buttons,
                                       MessageBox::Button defaultButton,
                                       MessageBox::Action action,
                                       QCheckBox* checkbox)
{
    return messageBox(parent, QMessageBox::Warning, title, text, buttons, defaultButton, action, checkbox);
}

MessageBox::Button MessageBox::information(QWidget* parent,
                                           const QString& title,
                                           const QString& text,
                                           MessageBox::Buttons buttons,
                                           MessageBox::Button defaultButton,
                                           MessageBox::Action action,
                                           QCheckBox* checkbox)
{
    return messageBox(parent, QMessageBox::Information, title, text, buttons, defaultButton, action, checkbox);
}

MessageBox::Button MessageBox::question(QWidget* parent,
                                        const QString& title,
                                        const QString& text,
                                        MessageBox::Buttons buttons,
                                        MessageBox::Button defaultButton,
                                        MessageBox::Action action,
                                        QCheckBox* checkbox)
{
    return messageBox(parent, QMessageBox::Question, title, text, buttons, defaultButton, action, checkbox);
}

void MessageBox::setNextAnswer(MessageBox::Button button)
{
    m_nextAnswer = button;
}

MessageBox::OverrideParent::OverrideParent(QWindow* newParent)
    : m_oldParent(MessageBox::m_overrideParent)
{
    MessageBox::m_overrideParent = newParent;
}

MessageBox::OverrideParent::~OverrideParent()
{
    MessageBox::m_overrideParent = m_oldParent;
}
