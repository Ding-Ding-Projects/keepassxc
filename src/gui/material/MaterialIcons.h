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

#ifndef KEEPASSXC_MATERIALICONS_H
#define KEEPASSXC_MATERIALICONS_H

#include "MaterialTheme.h"

#include <QIcon>
#include <QPixmap>
#include <QString>

namespace Material
{
    /**
     * Material Symbols Rounded glyph lookup.
     *
     * The design names every glyph with its Material Symbols name
     * (`shield_lock`, `casino`, `regular_expression`, ...). Those names are
     * resolved through a mapping table onto the SVGs KeePassXC already ships,
     * recoloured to the requested tint and cached per name/size/colour. Widgets
     * always go through here so that a theme change re-tints every glyph at
     * once; they never build a resource path themselves.
     */
    namespace Icons
    {
        /** The glyph tinted with Role::OnSurfaceVariant, the default content colour. */
        QIcon symbol(const QString& name);

        /** The glyph tinted with a theme role. */
        QIcon symbol(const QString& name, Role tint);

        /** The glyph tinted with an explicit colour, for state-dependent content. */
        QIcon symbol(const QString& name, const QColor& tint);

        /** A single rendering at @p size logical pixels, for painting inside delegates. */
        QPixmap pixmap(const QString& name, int size, const QColor& tint);

        /** The bundled icon name @p name maps onto, empty when the symbol is unknown. */
        QString resolve(const QString& name);

        /** Whether @p name is present in the mapping table. */
        bool hasSymbol(const QString& name);

        /** Drop every cached rendering. Called when the theme or density changes. */
        void clearCache();

    } // namespace Icons

} // namespace Material

#endif // KEEPASSXC_MATERIALICONS_H
