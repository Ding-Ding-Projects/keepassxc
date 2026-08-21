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

#include "MaterialBreakpoints.h"

class QAction;
class QMenu;
class QStackedWidget;
class QHBoxLayout;
class QToolButton;
class QResizeEvent;
class QWidgetAction;

namespace Material
{
    class NavigationRail;
    class SnackbarHost;
    class TabStrip;
    class TopAppBar;
    class SearchBar;

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
     * The rail is painted, not built from widgets, so none of what it offers is
     * a QAction and none of it would reach the command palette, which lists
     * commands by walking the window's action tree. The shell therefore carries
     * an action per destination and one for the rail's theme toggle, grouped
     * under menus that exist only to name them - see menuPathOf(). Locking is
     * deliberately absent: the window's Lock All Databases action is what the
     * rail already triggers, and the palette lists that one under Database.
     *
     * The Go To group is a deliberate divergence from the design, whose palette
     * (design/js/palette.js) has six groups - Database, Entries, Groups, Tools,
     * View, Help - and no Go To. There every row is hand written and a few of
     * them happen to navigate; here the rows are whatever the window's live
     * action tree holds, and the design's six groups name nothing that reaches
     * Vault, Entry, Database, Tools or Help. Folding the destinations into them
     * would leave half the rail with no keyboard route at all, so they keep a
     * group of their own and the six design groups go on reading exactly as the
     * design lists them, rather than being padded with rows it does not have.
     *
     * The theme toggle diverges for the same reason. The design's View group
     * carries a single 'Theme: Automatic / Light / Dark / Classic' row, which
     * the window's own View ▸ Theme actions already are - expanded into the
     * three modes this application actually has, Classic not being one of them.
     * The toggle is listed beside them because it is the rail's control, and a
     * control the user can see should be findable by name; it flips light and
     * dark rather than choosing a mode, which none of the three can do.
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
         *
         * A command carrying @p label and @p symbol is added alongside the rail
         * tile, so the destination is reachable from the command palette too.
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

        /**
         * Enable or disable every command the shell contributes to the palette.
         *
         * These actions hang off the shell, not off the rail, so greying the
         * rail out for a sync or a long operation leaves them live and the
         * palette can still navigate. The window turns them off with its own
         * menus and tool bar instead. The theme toggle goes with them: its
         * button on the rail is greyed at the same moment, and QAction::trigger()
         * fires whatever the action's enabled state, so nothing but this stops
         * the palette running it.
         */
        void setCommandsEnabled(bool enabled);
        Breakpoint breakpoint() const;

    public slots:
        void setCurrentDestination(const QString& id);

    signals:
        /** The visible destination changed, whether by the rail or by a caller. */
        void destinationChanged(const QString& id);
        void breakpointChanged(Material::Breakpoint breakpoint);

    protected:
        void paintEvent(QPaintEvent* event) override;
        void resizeEvent(QResizeEvent* event) override;

    private:
        /**
         * Rebuild the command icons for the current theme. Icons::symbol()
         * bakes the content colour into the QIcon, so the ones already handed
         * to the palette are stale the moment the mode flips.
         */
        void retintCommands();
        void applyBreakpoint(Breakpoint breakpoint);
        void updateCompactSelection();
        void filterMoreDestinations(const QString& query);
        void handOffNavigationFocus(bool compact, bool navigationHadFocus);

        NavigationRail* m_rail = nullptr;
        TopAppBar* m_appBar = nullptr;
        TabStrip* m_tabs = nullptr;
        QStackedWidget* m_stack = nullptr;
        SnackbarHost* m_snackbars = nullptr;
        QHash<QString, QWidget*> m_pages;
        QStringList m_order;
        QString m_current;
        /**
         * The two menus the shell's commands hang off. They are never popped
         * up; they exist so menuPathOf() has a title to report and the palette
         * files the commands under a heading instead of after everything else.
         * The theme toggle is on a menu named for the window's View menu on
         * purpose, so it lists with the other theme commands.
         */
        QMenu* m_goToMenu = nullptr;
        QMenu* m_viewMenu = nullptr;
        QAction* m_themeAction = nullptr;
        QWidget* m_bottomBar = nullptr;
        QHBoxLayout* m_bottomLayout = nullptr;
        QToolButton* m_moreButton = nullptr;
        QMenu* m_moreMenu = nullptr;
        SearchBar* m_moreSearch = nullptr;
        QWidgetAction* m_moreSearchAction = nullptr;
        QList<QAction*> m_moreDestinationActions;
        QHash<QString, QToolButton*> m_bottomButtons;
        Breakpoint m_breakpoint = Breakpoint::Compact;
    };

} // namespace Material

#endif // KEEPASSXC_MATERIALSHELL_H
