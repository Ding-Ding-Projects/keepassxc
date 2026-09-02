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
#include "MaterialSearchBar.h"
#include "MaterialRegexSafety.h"
#include "MaterialTabStrip.h"
#include "MaterialTheme.h"
#include "MaterialTitleBar.h"
#include "MaterialTopAppBar.h"

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QHBoxLayout>
#include <QLayout>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <QResizeEvent>
#include <QWidgetAction>

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
        // The shell must be able to enter the Compact class even though the
        // full rail and its labels have a much wider aggregate size hint.
        setMinimumSize(320, 480);

        auto* outer = new QVBoxLayout(this);
        outer->setContentsMargins(0, 0, 0, 0);
        outer->setSpacing(0);

        // The caption is the first row of the application, above the rail and
        // the app bar alike, exactly where the desktop would have drawn its own.
        m_titleBar = new TitleBar(this);
        outer->addWidget(m_titleBar);

        auto* root = new QHBoxLayout;
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(0);
        outer->addLayout(root, 1);

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

        m_bottomBar = new QWidget(this);
        m_bottomBar->setObjectName(QStringLiteral("materialBottomNavigation"));
        m_bottomBar->setFixedHeight(76);
        m_bottomLayout = new QHBoxLayout(m_bottomBar);
        m_bottomLayout->setContentsMargins(4, 4, 4, 4);
        m_bottomLayout->setSpacing(2);
        m_moreMenu = new QMenu(m_bottomBar);
        m_moreMenu->setAccessibleName(tr("More destinations"));
        m_moreSearch = new SearchBar(SearchBar::Variant::Prominent, m_moreMenu);
        m_moreSearch->setObjectName(QStringLiteral("materialBottomNavigationMoreSearch"));
        m_moreSearch->setPlaceholder(tr("Search destinations"));
        m_moreSearch->setIdentity(QStringLiteral("navigation.compact-more"), tr("Compact More destination search"));
        m_moreSearch->lineEdit()->setAccessibleName(tr("Search more destinations"));
        m_moreSearchAction = new QWidgetAction(m_moreMenu);
        m_moreSearchAction->setDefaultWidget(m_moreSearch);
        m_moreMenu->addAction(m_moreSearchAction);
        connect(m_moreSearch, &SearchBar::textChanged, this, &Shell::filterMoreDestinations);
        connect(m_moreSearch, &SearchBar::regexToggled, this, [this] { filterMoreDestinations(m_moreSearch->text()); });
        connect(m_moreMenu, &QMenu::aboutToShow, this, [this] {
            m_moreSearch->lineEdit()->setFocus(Qt::PopupFocusReason);
        });
        m_moreButton = new QToolButton(m_bottomBar);
        m_moreButton->setObjectName(QStringLiteral("materialBottomNavigationMore"));
        m_moreButton->setMinimumSize(48, 48);
        m_moreButton->setText(tr("More"));
        m_moreButton->setIcon(Icons::symbol(QStringLiteral("more_horiz")));
        m_moreButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        m_moreButton->setPopupMode(QToolButton::InstantPopup);
        m_moreButton->setMenu(m_moreMenu);
        m_moreButton->setAccessibleName(tr("More destinations"));
        m_moreButton->setCheckable(true);
        m_bottomLayout->addWidget(m_moreButton, 1);
        outer->addWidget(m_bottomBar);

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
        applyBreakpoint(breakpointFor(width()));
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

    TitleBar* Shell::titleBar() const
    {
        return m_titleBar;
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

        if (m_order.size() <= 5) {
            auto* button = new QToolButton(m_bottomBar);
            button->setDefaultAction(command);
            button->setObjectName(QStringLiteral("materialBottomDestination_") + id);
            button->setMinimumSize(48, 48);
            button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
            button->setAutoRaise(true);
            button->setCheckable(true);
            button->setProperty("destinationId", id);
            button->setAccessibleName(label);
            m_bottomButtons.insert(id, button);
            m_bottomLayout->insertWidget(m_bottomLayout->count() - 1, button, 1);
        } else {
            m_moreMenu->addAction(command);
            m_moreDestinationActions.append(command);
        }

        if (m_current.isEmpty()) {
            m_current = id;
            m_stack->setCurrentWidget(page);
            m_rail->setCurrentDestination(id);
            emit destinationChanged(id);
        }
        updateCompactSelection();
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
        updateCompactSelection();
        emit destinationChanged(id);
    }

    void Shell::setCommandsEnabled(bool enabled)
    {
        for (QAction* command : m_goToMenu->actions()) {
            command->setEnabled(enabled);
        }
        m_themeAction->setEnabled(enabled);
    }

    Breakpoint Shell::breakpoint() const
    {
        return m_breakpoint;
    }

    void Shell::applyBreakpoint(Breakpoint breakpoint)
    {
        const bool changed = m_breakpoint != breakpoint;
        const bool wasCompact = !hasRail(m_breakpoint);
        QWidget* focused = QApplication::focusWidget();
        const bool navigationHadFocus = focused
                                        && (focused == m_rail || m_rail->isAncestorOf(focused)
                                            || focused == m_moreButton || m_bottomBar->isAncestorOf(focused));
        m_breakpoint = breakpoint;
        const bool railVisible = hasRail(breakpoint);
        m_rail->setVisible(railVisible);
        m_rail->setFixedWidth(railWidth(breakpoint));
        m_rail->setIconsOnly(breakpoint == Breakpoint::Medium);
        m_bottomBar->setVisible(!railVisible);
        updateCompactSelection();
        if (changed && wasCompact != !railVisible) {
            handOffNavigationFocus(!railVisible, navigationHadFocus);
        }
        if (changed) {
            emit breakpointChanged(breakpoint);
        }
    }

    void Shell::updateCompactSelection()
    {
        for (auto it = m_bottomButtons.cbegin(); it != m_bottomButtons.cend(); ++it) {
            const bool current = it.key() == m_current;
            it.value()->setChecked(current);
            it.value()->setAccessibleDescription(current ? tr("Current destination") : QString());
        }
        const bool currentInMore = !m_current.isEmpty() && !m_bottomButtons.contains(m_current);
        m_moreButton->setChecked(currentInMore);
        m_moreButton->setAccessibleDescription(currentInMore ? tr("Current destination is in More") : QString());
        for (auto* action : m_moreDestinationActions) {
            action->setCheckable(true);
            action->setChecked(action->objectName() == QStringLiteral("materialDestination_") + m_current);
        }
    }

    void Shell::filterMoreDestinations(const QString& query)
    {
        const QString needle = query.trimmed();
        bool valid = true;
        QString error;
        const bool regex = m_moreSearch->isRegexEnabled() && !needle.isEmpty();
        if (regex) {
            const auto validation = runBounded(needle, optionsForFlags(m_moreSearch->regexFlags()), QString());
            valid = validation.compiled && !validation.blocked && !validation.timedOut;
            error = validation.error;
        }
        for (auto* action : m_moreDestinationActions) {
            bool match = needle.isEmpty() || action->text().contains(needle, Qt::CaseInsensitive);
            if (regex && valid) {
                const auto run = runBounded(needle, optionsForFlags(m_moreSearch->regexFlags()), action->text());
                match = !run.matches.isEmpty();
            } else if (regex) {
                match = false;
            }
            action->setVisible(match);
        }
        m_moreSearch->lineEdit()->setAccessibleDescription(valid ? tr("Valid destination filter")
                                                                 : tr("Invalid regular expression: %1").arg(error));
        m_moreSearch->setToolTip(valid ? QString() : tr("Invalid regular expression: %1").arg(error));
    }

    void Shell::handOffNavigationFocus(bool compact, bool navigationHadFocus)
    {
        if (!navigationHadFocus) {
            return;
        }
        if (compact) {
            if (auto* button = m_bottomButtons.value(m_current)) {
                button->setFocus(Qt::OtherFocusReason);
            } else {
                m_moreButton->setFocus(Qt::OtherFocusReason);
            }
        } else {
            m_rail->setFocus(Qt::OtherFocusReason);
        }
    }

    void Shell::resizeEvent(QResizeEvent* event)
    {
        QWidget::resizeEvent(event);
        applyBreakpoint(breakpointFor(event->size().width()));
        if (m_snackbars) {
            m_snackbars->setGeometry(m_stack->rect());
        }
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
