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

#include "MaterialExternalEditor.h"

#include "core/Config.h"

#include <QFileInfo>
#include <QProcess>

namespace Material
{
    ExternalEditor::ExternalEditor(QObject* parent)
        : QObject(parent)
    {
    }

    QList<ExternalEditor::Editor> ExternalEditor::detect() const
    {
        // TODO: probe the known paths, then append Config::GUI_ExternalEditors.
        // Keep not-found entries in the list with found == false.
        return {};
    }

    bool ExternalEditor::open(const QString& path)
    {
        const Editor e = current();
        if (e.id.isEmpty()) {
            emit failed(tr("No external editor is configured. Choose one in Tools > External editor."),
                        QStringLiteral("未揀外部編輯器。去 工具 › 外部編輯器 揀返個先。"));
            return false;
        }
        if (!QFileInfo::exists(e.path)) {
            emit failed(tr("%1 is configured but its executable is missing at %2.").arg(e.name, e.path),
                        QStringLiteral("揀咗 %1，但 %2 度搵唔到個執行檔。").arg(e.name, e.path));
            return false;
        }
        QStringList args;
        for (const auto& a : e.arguments.split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
            args << QString(a).replace(QLatin1String("%1"), path);
        }
        if (!QProcess::startDetached(e.path, args)) {
            emit failed(tr("%1 failed to start.").arg(e.name), QStringLiteral("%1 開唔到。").arg(e.name));
            return false;
        }
        emit opened(path, e.name);
        return true;
    }
} // namespace Material
