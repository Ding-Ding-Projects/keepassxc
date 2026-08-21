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

#ifndef KEEPASSXC_MATERIALELEMENTOVERRIDES_H
#define KEEPASSXC_MATERIALELEMENTOVERRIDES_H

#include <QColor>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QString>

namespace Material
{
    /**
     * Per-element appearance overrides, persisted and individually resettable.
     *
     * The shared UI requirements ask for in-app editors that let the user customise font, colour,
     * size, radius and spacing of individual elements, toolbars and surfaces.
     * That is a per-element layer sitting on top of the theme, and the shape of
     * it matters:
     *
     * - An element is addressed by a STABLE STRING KEY ("material/entryRow"),
     *   never by pointer, index or object name. Keys survive a rebuild of the
     *   widget tree, a theme change and a restart. A key that no longer
     *   resolves is kept, not deleted: a destination the user has not visited
     *   this session must not lose its customisation.
     * - An override stores only what the user changed. An absent property means
     *   "follow the theme", so changing the seed still restyles everything the
     *   user did not pin.
     * - reset(key) removes one element's overrides. resetAll() removes every
     *   one. Neither touches the theme.
     *
     * Overrides are part of the settings snapshot the history store takes, so
     * an accidental reset is recoverable like any other change.
     */
    class ElementOverrides : public QObject
    {
        Q_OBJECT

    public:
        struct Override
        {
            std::optional<int> height;
            std::optional<int> radius;
            std::optional<int> fontSize;
            std::optional<int> fontWeight;
            std::optional<int> spacing;
            std::optional<QColor> background;
            std::optional<QColor> foreground;
            std::optional<QString> fontFamily;

            QJsonObject toJson() const;
            static Override fromJson(const QJsonObject& o);
            bool isEmpty() const;
        };

        static ElementOverrides* instance();

        /** The override for @p key, or an empty one. Never null. */
        Override get(const QString& key) const;
        void set(const QString& key, const Override& value);
        void reset(const QString& key);
        void resetAll();

        /** Keys that currently carry at least one property. */
        QStringList customisedKeys() const;

        /** Read from / write to Config key GUI_ElementOverrides. */
        void load();
        void save() const;

    signals:
        /** One element changed. Only that element needs restyling. */
        void overrideChanged(const QString& key);

    private:
        ElementOverrides() = default;
        QHash<QString, Override> m_overrides;
    };
} // namespace Material

#endif // KEEPASSXC_MATERIALELEMENTOVERRIDES_H
