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

#ifndef KEEPASSXC_MATERIALSEARCHREGISTRY_H
#define KEEPASSXC_MATERIALSEARCHREGISTRY_H

#include <QList>
#include <QObject>
#include <QPointer>

namespace Material
{
    class SearchBar;

    /**
     * Every live SearchBar in the application, so the regex builder can be
     * anchored to the one that asked for it.
     *
     * The shared UI requirements place a builder on every search bar AND on every settings,
     * preferences, properties and adjustment surface - including every tab
     * within them. That is more than a dozen bars, created and destroyed as
     * destinations come and go. Without a registry the builder has no way to
     * know which bar to write its result back to, and the obvious shortcut -
     * always writing to the vault bar - is exactly the bug this class exists to
     * prevent: a user refining a pattern for the Settings search would find
     * their vault filter silently replaced.
     *
     * Registration is automatic: SearchBar's constructor registers and its
     * destructor unregisters. The registry holds QPointers and prunes dead ones
     * on every read, so a destination torn down mid-edit cannot be written to.
     *
     * The "current" bar is the last one to have taken focus, not the last one
     * created. A builder opened from a bar sets that bar current explicitly, so
     * clicking into the builder itself does not change the anchor.
     */
    class SearchRegistry : public QObject
    {
        Q_OBJECT

    public:
        static SearchRegistry* instance();

        void registerBar(SearchBar* bar);
        void unregisterBar(SearchBar* bar);

        /** The bar the builder should write back to, or nullptr. */
        SearchBar* current() const;
        void setCurrent(SearchBar* bar);

        /** Every live bar, dead pointers pruned. */
        QList<SearchBar*> bars() const;

        /**
         * A human-readable name for the current anchor, shown in the builder's
         * header ("Anchored to: Vault search"). Falls back to the widget's
         * accessible name so a new bar is never anonymous.
         */
        QString currentLabel() const;

    signals:
        void currentChanged(SearchBar* bar);

    private:
        SearchRegistry() = default;
        mutable QList<QPointer<SearchBar>> m_bars;
        QPointer<SearchBar> m_current;
    };
} // namespace Material

#endif // KEEPASSXC_MATERIALSEARCHREGISTRY_H
