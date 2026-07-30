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

#ifndef KEEPASSXC_MATERIALNOTIFIER_H
#define KEEPASSXC_MATERIALNOTIFIER_H

#include "MaterialSnackbar.h"

#include <QList>
#include <QString>

class QWidget;

namespace Material
{
    class NotificationCentre;

    /**
     * The one way the application says something that is not a question.
     *
     * Calling code states what happened and how loud it is; the router decides
     * the presentation. Every call files the message in the notification centre
     * and raises a snackbar over the active window - information and success
     * fade out, warnings and errors wait to be dismissed. Nothing here blocks,
     * so a modal dialog stays reserved for decisions the user has to make.
     */
    namespace Notify
    {
        /** Pin the window notifications appear on. Cleared with nullptr. */
        void setHost(QWidget* host);
        /** The pinned window, or the active one when nothing is pinned. */
        QWidget* host();

        void info(const QString& text);
        void info(const QString& title, const QString& body, const QList<NotificationAction>& actions = {});

        void success(const QString& text);
        void success(const QString& title, const QString& body, const QList<NotificationAction>& actions = {});

        void warning(const QString& text);
        void warning(const QString& title, const QString& body, const QList<NotificationAction>& actions = {});

        void error(const QString& text);
        void error(const QString& title, const QString& body, const QList<NotificationAction>& actions = {});

        /**
         * Report a running job. Calls sharing an @p id update one toast in
         * place instead of stacking. A @p percent of 100 completes the job and
         * lets the toast go; a negative percent hides the bar but keeps the
         * message.
         */
        void progress(const QString& id, const QString& text, int percent);

        /** Drop the toast for @p id without reporting completion. */
        void endProgress(const QString& id);

        /** The centre for the current host, or nullptr when there is no window. */
        NotificationCentre* centre();

    } // namespace Notify

} // namespace Material

#endif // KEEPASSXC_MATERIALNOTIFIER_H
