/*
 *  Copyright (C) 2021 KeePassXC Team <team@keepassxc.org>
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

#ifndef KEEPASSXC_STATECOLORPALETTE_H
#define KEEPASSXC_STATECOLORPALETTE_H

#include <QColor>
#include <QHash>
#include <QObject>

/**
 * Status colours for item views and reports.
 *
 * Every role resolves out of the active Material scheme, so the health columns
 * follow the seed, the light or dark mode and the accessibility contrast just
 * like the rest of the interface. Instances are cheap and read the theme when
 * they are constructed; build one where you need it rather than caching it.
 */
class StateColorPalette
{
    Q_GADGET

public:
    StateColorPalette();

    enum ColorRole
    {
        Error,
        Warning,
        Info,
        Incomplete,
        HealthCritical,
        HealthBad,
        HealthPoor,
        HealthWeak,
        HealthOk,
        HealthExcellent,
        True,
        False
    };

    inline void setColor(ColorRole role, const QColor& color)
    {
        m_colorMap[role] = color;
    }

    inline QColor color(ColorRole role) const
    {
        return m_colorMap.value(role);
    }

private:
    void resolveFromTheme();

    QHash<ColorRole, QColor> m_colorMap;
};

#endif // KEEPASSXC_STATECOLORPALETTE_H
