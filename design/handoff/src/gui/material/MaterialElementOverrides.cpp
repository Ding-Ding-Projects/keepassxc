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

#include "MaterialElementOverrides.h"

#include "core/Config.h"

#include <QJsonDocument>

namespace Material
{
    ElementOverrides* ElementOverrides::instance()
    {
        static ElementOverrides inst;
        return &inst;
    }

    ElementOverrides::Override ElementOverrides::get(const QString& key) const
    {
        return m_overrides.value(key);
    }

    void ElementOverrides::set(const QString& key, const Override& value)
    {
        if (value.isEmpty()) {
            reset(key);
            return;
        }
        m_overrides.insert(key, value);
        save();
        emit overrideChanged(key);
    }

    void ElementOverrides::reset(const QString& key)
    {
        if (m_overrides.remove(key) > 0) {
            save();
            emit overrideChanged(key);
        }
    }

    void ElementOverrides::resetAll()
    {
        const auto keys = m_overrides.keys();
        m_overrides.clear();
        save();
        for (const auto& k : keys) {
            emit overrideChanged(k);
        }
    }

    QStringList ElementOverrides::customisedKeys() const
    {
        return m_overrides.keys();
    }

    void ElementOverrides::load()
    {
        // TODO: config()->get(Config::GUI_ElementOverrides).toString() parsed as
        // JSON. A key that fails to parse is DROPPED with a warning, never
        // silently coerced - a half-read override is worse than none.
    }

    void ElementOverrides::save() const
    {
        // TODO: QJsonDocument(obj).toJson(QJsonDocument::Compact) into
        // Config::GUI_ElementOverrides.
    }
} // namespace Material
