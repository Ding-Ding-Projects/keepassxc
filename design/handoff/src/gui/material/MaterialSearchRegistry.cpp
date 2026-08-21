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

#include "MaterialSearchRegistry.h"

#include "MaterialSearchBar.h"

namespace Material
{
    SearchRegistry* SearchRegistry::instance()
    {
        static SearchRegistry inst;
        return &inst;
    }

    void SearchRegistry::registerBar(SearchBar* bar)
    {
        if (!bar || m_bars.contains(bar)) {
            return;
        }
        m_bars.append(bar);
    }

    void SearchRegistry::unregisterBar(SearchBar* bar)
    {
        m_bars.removeAll(bar);
        if (m_current == bar) {
            m_current = nullptr;
            emit currentChanged(nullptr);
        }
    }

    QList<SearchBar*> SearchRegistry::bars() const
    {
        QList<SearchBar*> out;
        for (int i = m_bars.size() - 1; i >= 0; --i) {
            if (m_bars.at(i).isNull()) {
                m_bars.removeAt(i);
            }
        }
        for (const auto& p : m_bars) {
            out.append(p.data());
        }
        return out;
    }

    SearchBar* SearchRegistry::current() const
    {
        return m_current.data();
    }

    void SearchRegistry::setCurrent(SearchBar* bar)
    {
        if (m_current == bar) {
            return;
        }
        m_current = bar;
        emit currentChanged(bar);
    }

    QString SearchRegistry::currentLabel() const
    {
        // TODO: SearchBar::title() when set, else accessibleName(), else the
        // parent destination's label. Never return an empty string - the
        // builder header must always say what it will write to.
        return {};
    }
} // namespace Material
