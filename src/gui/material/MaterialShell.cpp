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

#include "MaterialShell.h"

#include "MaterialIcons.h"
#include "MaterialNavigationRail.h"
#include "MaterialSnackbar.h"
#include "MaterialTabStrip.h"
#include "MaterialTheme.h"
#include "MaterialTopAppBar.h"

#include <QAction>
#include <QCoreApplication>
#include <QHBoxLayout>
#include <QLayout>
#include <QMenu>
#include <QPainter>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace Material
{
    namespace
    {
        /** The one shell of the running window; see Shell::instance(). */
        Shell* s_instance = nullptr;
    } // namespace

    Shell::Shell(QWidget* parent)
        : QWidget(parent)
    {
        setObjectName(QStringLiteral("materialShell"));
        setAutoFillBackground(false);

        auto* root = new QHBoxLayout(this);
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(0);

        m_rail = new NavigationRail(this);
        root->addWidget(m_rail);

        auto* column = new QVBoxLayout;
        column->setContentsMargins(0, 0, 0, 0);
        column->setSpacing(0);

        m_appBar = new TopAppBar(this);
        column->addWidget(m_appBar);

        m_tabs = new TabStrip(this);
        column->addWidget(m_tabs);

        m_stack = new QStackedWidget(this);
        m_stack->setObjectName(QStringLiteral("materialDestinationStack"));
        column->addWidget(m_stack, 1);

        root->addLayout(column, 1);

        // Over the stack, not over the window: a toast never covers the rail,
        // the app bar or the tab strip.
        m_snackbars = new SnackbarHost(m_stack);

        connect(m_rail, &NavigationRail::destinationActivated, this, &Shell::setCurrentDestination);

        // Neither menu is ever popped up. They are here so that menuPathOf()
        // finds a title for the shell's own commands and the palette files them
        // under a heading. Why the shell has a Go To heading at all when the
        // design's palette has no such group, and why the toggle joins the
        // window's theme actions rather than replacing them, is on the class.
        m_goToMenu = new QMenu(tr("Go To"), this);
        // The heading has to read as the same word as the window's View menu,
        // whose title comes from MainWindow.ui and so is translated under the
        // "MainWindow" context. tr() here would file a second, independently
        // translated "View", and a translated build could then list the toggle
        // under a heading worded differently from the menu it belongs to.
        m_viewMenu = new QMenu(QCoreApplication::translate("MainWindow", "View"), this);

        m_themeAction = new QAction(tr("Toggle Light / Dark Theme"), this);
        m_themeAction->setObjectName(QStringLiteral("materialToggleTheme"));
        // Which mode to switch into is the window's call - it also keeps the
        // View ▸ Theme radio group honest - and the window already listens to
        // the rail, so this relays that request instead of flipping the theme
        // itself and leaving the menu behind.
        connect(m_themeAction, &QAction::triggered, m_rail, &NavigationRail::themeToggleRequested);
        addAction(m_themeAction);
        m_viewMenu->addAction(m_themeAction);

        // The rail's other footer button gets no action: it triggers the
        // window's Lock All Databases, which is already a command the palette
        // lists under Database. A second one would be the same command twice.

        retintCommands();

        connect(theme(), &Theme::changed, this, [this] {
            retintCommands();
            update();
        });

        s_instance = this;
    }

    Shell::~Shell()
    {
        if (s_instance == this) {
            s_instance = nullptr;
        }
    }

    Shell* Shell::instance()
    {
        return s_instance;
    }

    NavigationRail* Shell::rail() const
    {
        return m_rail;
    }

    TopAppBar* Shell::appBar() const
    {
        return m_appBar;
    }

    TabStrip* Shell::tabs() const
    {
        return m_tabs;
    }

    SnackbarHost* Shell::snackbars() const
    {
        return m_snackbars;
    }

    void Shell::addDestination(const QString& id,
                               QWidget* page,
                               const QString& symbol,
                               const QString& label,
                               const QString& sublabel)
    {
        if (id.isEmpty() || m_pages.contains(id)) {
            return;
        }

        if (!page) {
            page = new QWidget(m_stack);
        } else if (QWidget* previousParent = page->parentWidget()) {
            // A page moved out of another layout leaves a stale item behind
            // unless it is taken out explicitly; reparenting alone is not enough.
            if (QLayout* previousLayout = previousParent->layout()) {
                previousLayout->removeWidget(page);
            }
        }

        m_pages.insert(id, page);
        m_order.append(id);
        m_stack->addWidget(page);
        m_rail->addDestination(id, symbol, label, sublabel);

        // The rail tile is painted, so nothing about this destination is a
        // QAction and the palette would never see it. The command is that
        // action: the tile's own label and glyph, activating the same tile.
        auto* command = new QAction(Icons::symbol(symbol), label, this);
        command->setObjectName(QStringLiteral("materialDestination_") + id);
        // The symbol name has to outlive the call so retintCommands() can build
        // the icon again in the other mode.
        command->setData(symbol);
        connect(command, &QAction::triggered, this, [this, id, label] {
            if (id == m_current) {
                // The row for the destination already on screen is the one a
                // user reaches for first, and setCurrentDestination() has
                // nothing to do for it - the palette would close on a command
                // that did nothing at all. Say where they already are instead.
                if (m_snackbars) {
                    m_snackbars->show(tr("Already showing %1.").arg(label));
                }
                return;
            }
            setCurrentDestination(id);
        });
        addAction(command);
        m_goToMenu->addAction(command);

        if (m_current.isEmpty()) {
            m_current = id;
            m_stack->setCurrentWidget(page);
            m_rail->setCurrentDestination(id);
            emit destinationChanged(id);
        }
    }

    QString Shell::currentDestination() const
    {
        return m_current;
    }

    QWidget* Shell::destination(const QString& id) const
    {
        return m_pages.value(id, nullptr);
    }

    QStringList Shell::destinations() const
    {
        return m_order;
    }

    void Shell::setCurrentDestination(const QString& id)
    {
        if (id == m_current || !m_pages.contains(id)) {
            return;
        }

        m_current = id;
        m_rail->setCurrentDestination(id);
        m_stack->setCurrentWidget(m_pages.value(id));
        emit destinationChanged(id);
    }

    void Shell::setCommandsEnabled(bool enabled)
    {
        for (QAction* command : m_goToMenu->actions()) {
            command->setEnabled(enabled);
        }
        m_themeAction->setEnabled(enabled);
    }

    void Shell::retintCommands()
    {
        for (QAction* command : m_goToMenu->actions()) {
            command->setIcon(Icons::symbol(command->data().toString()));
        }
        // Like the rail's footer button, the toggle shows the mode the user is
        // not in - that is the one it switches to.
        m_themeAction->setIcon(Icons::symbol(theme()->isDark() ? QStringLiteral("light_mode")
                                                               : QStringLiteral("dark_mode")));
    }

    void Shell::paintEvent(QPaintEvent* event)
    {
        Q_UNUSED(event)
        QPainter painter(this);
        painter.fillRect(rect(), theme()->color(Role::Surface));
    }

} // namespace Material
