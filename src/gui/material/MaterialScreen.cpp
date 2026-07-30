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

#include "MaterialScreen.h"

#include "MaterialSearchBar.h"
#include "MaterialTheme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QScrollArea>
#include <QVBoxLayout>

namespace Material
{
    namespace
    {
        constexpr int SidePadding = 26;
        constexpr int TopPadding = 22;
        constexpr int BottomPadding = 40;
        constexpr int HeadlineSpacing = 12;
        constexpr int ContentSpacing = 16;
        constexpr int BlurbWidth = 900;
        constexpr int SearchMinimumWidth = 240;
        // The headline row opens with the headline and the stretch that pushes
        // everything else to the right edge; the trailing run starts after them.
        constexpr int FirstTrailingItem = 2;
    } // namespace

    Screen::Screen(QWidget* parent)
        : QWidget(parent)
    {
        m_rootLayout = new QVBoxLayout(this);
        m_rootLayout->setContentsMargins(0, 0, 0, 0);
        m_rootLayout->setSpacing(0);

        m_header = new QWidget(this);
        auto headerColumn = new QVBoxLayout(m_header);
        headerColumn->setContentsMargins(SidePadding, TopPadding, SidePadding, 0);
        headerColumn->setSpacing(6);

        m_headerLayout = new QHBoxLayout();
        m_headerLayout->setContentsMargins(0, 0, 0, 0);
        m_headerLayout->setSpacing(HeadlineSpacing);

        m_headlineLabel = new QLabel(m_header);
        m_headerLayout->addWidget(m_headlineLabel);
        m_headerLayout->addStretch(1);

        // Lives on the headline row by default; the history and changelog
        // screens move it into a filter row of their own.
        m_searchBar = new SearchBar(SearchBar::Variant::Surface, m_header);
        m_searchBar->setMinimumWidth(SearchMinimumWidth);
        m_searchBar->hide();
        m_headerLayout->addWidget(m_searchBar);

        headerColumn->addLayout(m_headerLayout);

        m_supportingLabel = new QLabel(m_header);
        m_supportingLabel->setWordWrap(true);
        m_supportingLabel->setMaximumWidth(BlurbWidth);
        m_supportingLabel->hide();
        headerColumn->addWidget(m_supportingLabel);

        m_rootLayout->addWidget(m_header);

        m_content = new QWidget();
        m_content->setAutoFillBackground(false);
        m_contentLayout = new QVBoxLayout(m_content);
        m_contentLayout->setContentsMargins(SidePadding, TopPadding, SidePadding, BottomPadding);
        m_contentLayout->setSpacing(ContentSpacing);

        m_scrollArea = new QScrollArea(this);
        m_scrollArea->setFrameShape(QFrame::NoFrame);
        m_scrollArea->setWidgetResizable(true);
        m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_scrollArea->setStyleSheet(QStringLiteral("QScrollArea { background: transparent; border: none; }"));
        m_scrollArea->setWidget(m_content);
        m_scrollArea->viewport()->setAutoFillBackground(false);
        m_rootLayout->addWidget(m_scrollArea, 1);

        connect(theme(), &Theme::changed, this, &Screen::applyTheme);
        applyTheme();
    }

    Screen::~Screen() = default;

    QString Screen::headline() const
    {
        return m_headlineLabel->text();
    }

    void Screen::setHeadline(const QString& text)
    {
        m_headlineLabel->setText(text);
    }

    QString Screen::supportingText() const
    {
        return m_supportingLabel->text();
    }

    void Screen::setSupportingText(const QString& text)
    {
        m_supportingLabel->setText(text);
        m_supportingLabel->setVisible(!text.isEmpty());
    }

    bool Screen::isHeaderVisible() const
    {
        return !m_header->isHidden();
    }

    void Screen::setHeaderVisible(bool visible)
    {
        m_header->setVisible(visible);
    }

    SearchBar* Screen::searchBar() const
    {
        return m_searchBar;
    }

    bool Screen::isSearchVisible() const
    {
        return !m_searchBar->isHidden();
    }

    void Screen::setSearchVisible(bool visible)
    {
        m_searchBar->setVisible(visible);
    }

    void Screen::addHeaderWidget(QWidget* widget)
    {
        m_headerLayout->addWidget(widget);
    }

    void Screen::insertHeaderWidget(int index, QWidget* widget)
    {
        m_headerLayout->insertWidget(FirstTrailingItem + qBound(0, index, headerWidgetCount()), widget);
    }

    int Screen::headerWidgetCount() const
    {
        return qMax(0, m_headerLayout->count() - FirstTrailingItem);
    }

    QVBoxLayout* Screen::contentLayout() const
    {
        return m_contentLayout;
    }

    QScrollArea* Screen::scrollArea() const
    {
        return m_scrollArea;
    }

    bool Screen::isScrollable() const
    {
        return m_scrollable;
    }

    void Screen::setScrollable(bool scrollable)
    {
        if (scrollable == m_scrollable) {
            return;
        }
        m_scrollable = scrollable;

        // A screen that scrolls internally also owns its own padding: the vault
        // panes reach the window edge.
        m_scrollArea->setVerticalScrollBarPolicy(scrollable ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
        if (scrollable) {
            m_contentLayout->setContentsMargins(SidePadding, TopPadding, SidePadding, BottomPadding);
            m_contentLayout->setSpacing(ContentSpacing);
        } else {
            m_contentLayout->setContentsMargins(0, 0, 0, 0);
            m_contentLayout->setSpacing(0);
        }
    }

    void Screen::paintEvent(QPaintEvent* event)
    {
        Q_UNUSED(event)
        QPainter painter(this);
        painter.fillRect(rect(), theme()->color(Role::Surface));
    }

    void Screen::applyTheme()
    {
        m_headlineLabel->setFont(theme()->font(TypeRole::HeadlineSmall));
        m_headlineLabel->setStyleSheet(
            QStringLiteral("color: %1; background: transparent;").arg(theme()->hex(Role::OnSurface)));

        m_supportingLabel->setFont(theme()->font(TypeRole::BodySmall));
        m_supportingLabel->setStyleSheet(
            QStringLiteral("color: %1; background: transparent;").arg(theme()->hex(Role::OnSurfaceVariant)));

        update();
    }

} // namespace Material
