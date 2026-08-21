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

#ifndef KEEPASSXC_MATERIALEXTERNALEDITOR_H
#define KEEPASSXC_MATERIALEXTERNALEDITOR_H

#include <QList>
#include <QObject>
#include <QString>

namespace Material
{
    /**
     * Open the database folder, or a selected file, in the user's own editor.
     *
     * Detection is a fixed list of well-known install paths plus whatever the
     * user adds. An editor that is not found is SHOWN as not found rather than
     * omitted from the list - a user who installed Sublime and cannot see it
     * needs to know the app looked and failed, not wonder whether it looked.
     *
     * The failure path is the whole point of this class. "Open in editor" that
     * does nothing when no editor is configured is exactly the kind of defect caused by a
     * control that looks like it works but does not. Every failure here emits a
     * message naming what is missing and what to do about it.
     */
    class ExternalEditor : public QObject
    {
        Q_OBJECT

    public:
        struct Editor
        {
            QString id;
            QString name;
            QString path;
            QString arguments; // %1 is substituted with the target
            bool found = false;
        };

        explicit ExternalEditor(QObject* parent = nullptr);

        /** Well-known editors plus user-added ones, found or not. */
        QList<Editor> detect() const;

        Editor current() const;
        void setCurrent(const QString& id);
        void addCustom(const Editor& editor);

        /**
         * Launch the configured editor on @p path.
         *
         * @return false when no editor is configured, the binary is missing, or
         * the process failed to start. failed() carries the reason in both
         * languages; the caller shows it as a snackbar, never as a modal.
         */
        bool open(const QString& path);

    signals:
        void opened(const QString& path, const QString& editorName);
        void failed(const QString& reasonEn, const QString& reasonYue);
    };
} // namespace Material

#endif // KEEPASSXC_MATERIALEXTERNALEDITOR_H
