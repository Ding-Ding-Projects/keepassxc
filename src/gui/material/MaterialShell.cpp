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

#include "MaterialNavigationRail.h"
#include "MaterialSnackbar.h"
#include "MaterialTabStrip.h"
#include "MaterialTheme.h"
#include "MaterialTopAppBar.h"

#include <QHBoxLayout>
#include <QLayout>
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
        connect(theme(), &Theme::changed, this, [this] { update(); });

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

    void Shell::paintEvent(QPaintEvent* event)
    {
        Q_UNUSED(event)
        QPainter painter(this);
        painter.fillRect(rect(), theme()->color(Role::Surface));
    }

} // namespace Material
