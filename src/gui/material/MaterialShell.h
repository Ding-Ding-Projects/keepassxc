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

#ifndef KEEPASSXC_MATERIALSHELL_H
#define KEEPASSXC_MATERIALSHELL_H

#include <QHash>
#include <QString>
#include <QStringList>
#include <QWidget>

class QStackedWidget;

namespace Material
{
    class NavigationRail;
    class SnackbarHost;
    class TabStrip;
    class TopAppBar;

    /**
     * The whole interior of the main window.
     *
     * A navigation rail down the left edge, and to its right a column of the
     * app bar, the database tab strip and the destination stack:
     *
     *     [ rail | appBar     ]
     *     [      | tabStrip   ]
     *     [      | stack      ]
     *
     * The rail owns which destination is showing; activating a tile switches
     * the stack and announces destinationChanged(). Everything else - what the
     * app bar says, what the tabs are, what a destination contains - is the
     * host's to wire, which is why the three chrome widgets are handed out
     * rather than driven from here.
     *
     * A SnackbarHost covers the stack so toasts float over the content and
     * never over the rail or the app bar.
     *
     * The shell takes ownership of every page passed to addDestination(),
     * removing it from its previous layout first, so an existing widget can be
     * moved into a destination without leaving a stale layout item behind.
     */
    class Shell : public QWidget
    {
        Q_OBJECT

    public:
        explicit Shell(QWidget* parent = nullptr);
        ~Shell() override;

        /** The shell of the running main window, or nullptr before it is built. */
        static Shell* instance();

        NavigationRail* rail() const;
        TopAppBar* appBar() const;
        TabStrip* tabs() const;
        SnackbarHost* snackbars() const;

        /**
         * Append a destination. The first one added becomes current.
         *
         * @p page is reparented into the destination stack; passing nullptr
         * creates an empty page so the rail tile still has somewhere to go.
         */
        void addDestination(const QString& id,
                            QWidget* page,
                            const QString& symbol,
                            const QString& label,
                            const QString& sublabel = {});

        QString currentDestination() const;
        QWidget* destination(const QString& id) const;
        /** Destination ids in the order they were added. */
        QStringList destinations() const;

    public slots:
        void setCurrentDestination(const QString& id);

    signals:
        /** The visible destination changed, whether by the rail or by a caller. */
        void destinationChanged(const QString& id);

    protected:
        void paintEvent(QPaintEvent* event) override;

    private:
        NavigationRail* m_rail = nullptr;
        TopAppBar* m_appBar = nullptr;
        TabStrip* m_tabs = nullptr;
        QStackedWidget* m_stack = nullptr;
        SnackbarHost* m_snackbars = nullptr;
        QHash<QString, QWidget*> m_pages;
        QStringList m_order;
        QString m_current;
    };

} // namespace Material

#endif // KEEPASSXC_MATERIALSHELL_H
