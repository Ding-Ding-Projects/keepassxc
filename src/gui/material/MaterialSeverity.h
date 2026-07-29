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

#ifndef KEEPASSXC_MATERIALSEVERITY_H
#define KEEPASSXC_MATERIALSEVERITY_H

#include <QStyle>
#include <QVariant>
#include <QWidget>

namespace Material
{
    /** The three status families the generated stylesheet keys rules on. */
    namespace Severity
    {
        constexpr const char* Error = "error";
        constexpr const char* Warning = "warning";
        constexpr const char* Success = "success";
        constexpr const char* None = nullptr;
    } // namespace Severity

    /**
     * Give @p widget an error, warning or success treatment.
     *
     * The colours live in the generated stylesheet under
     * `[materialSeverity="..."]`, which is why a status is set as a property
     * rather than as a local stylesheet. Property selectors are resolved when
     * the style polishes a widget, so changing one afterwards means asking the
     * style to look again. Pass Severity::None to clear the status.
     */
    inline void setSeverity(QWidget* widget, const char* severity)
    {
        if (!widget) {
            return;
        }

        const QVariant value = severity ? QVariant(QLatin1String(severity)) : QVariant();
        if (widget->property("materialSeverity") == value) {
            return;
        }

        widget->setProperty("materialSeverity", value);
        widget->style()->unpolish(widget);
        widget->style()->polish(widget);
        widget->update();
    }

} // namespace Material

#endif // KEEPASSXC_MATERIALSEVERITY_H
