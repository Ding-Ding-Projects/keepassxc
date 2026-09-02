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
#include <QResizeEvent>
#include <QScopedValueRollback>
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
        constexpr int MinimumScreenWidth = 320;
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
        // An explicit minimum stops the root layout from imposing the header's
        // one-row width; the header re-wraps below it (see minimumSizeHint()).
        setMinimumWidth(MinimumScreenWidth);

        m_header = new QWidget(this);
        auto headerColumn = new QVBoxLayout(m_header);
        m_headerColumn = headerColumn;
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

        // The two overflow rows the headline row spills into at narrow widths.
        m_headerWrapRow = new QHBoxLayout();
        m_headerWrapRow->setContentsMargins(0, 0, 0, 0);
        m_headerWrapRow->setSpacing(HeadlineSpacing);
        headerColumn->addLayout(m_headerWrapRow);
        m_headerSearchRow = new QHBoxLayout();
        m_headerSearchRow->setContentsMargins(0, 0, 0, 0);
        headerColumn->addLayout(m_headerSearchRow);

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
        const int trailingIndex = qBound(0, index, m_trailing.size());
        m_trailing.insert(trailingIndex, widget);
        m_headerLayout->insertWidget(FirstTrailingItem + trailingIndex, widget);
        relayoutHeader();
    }

    int Screen::headerWidgetCount() const
    {
        // The search bar counts while it lives in one of the header rows.
        const bool searchInHeader = m_searchBar && m_searchBar->parentWidget() == m_header && !m_searchBar->isHidden();
        return m_trailing.size() + (searchInHeader ? 1 : 0);
    }

    QList<QWidget*> Screen::trailingWidgets() const
    {
        QList<QWidget*> visible;
        for (QWidget* widget : m_trailing) {
            if (widget && !widget->isHidden()) {
                visible << widget;
            }
        }
        return visible;
    }

    void Screen::resizeEvent(QResizeEvent* event)
    {
        QWidget::resizeEvent(event);
        relayoutHeader();
    }

    QSize Screen::minimumSizeHint() const
    {
        return QSize(MinimumScreenWidth, QWidget::minimumSizeHint().height());
    }

    void Screen::relayoutHeader()
    {
        if (m_relayoutingHeader || !m_headerColumn || m_header->isHidden()) {
            return;
        }
        QScopedValueRollback<bool> guard(m_relayoutingHeader, true);

        const bool searchHere = m_searchBar && m_searchBar->parentWidget() == m_header && !m_searchBar->isHidden();
        const QList<QWidget*> trailing = trailingWidgets();
        const int available = m_header->width() - m_headerColumn->contentsMargins().left()
                              - m_headerColumn->contentsMargins().right();
        int inlineWidth = m_headlineLabel->sizeHint().width();
        int trailingWidth = 0;
        for (QWidget* widget : trailing) {
            inlineWidth += HeadlineSpacing + widget->sizeHint().width();
            trailingWidth += (trailingWidth ? HeadlineSpacing : 0) + widget->sizeHint().width();
        }
        const int searchWidth = searchHere ? HeadlineSpacing + m_searchBar->minimumWidth() : 0;
        inlineWidth += searchWidth;

        enum class Mode
        {
            Inline,
            Wrapped,
            ThreeRows
        };
        Mode mode = Mode::Inline;
        if (available > 0 && inlineWidth > available) {
            mode = trailingWidth + searchWidth > available ? Mode::ThreeRows : Mode::Wrapped;
        }

        // Take every trailing widget and the search bar out of whichever row
        // holds it, then put them back where the mode says.
        for (QWidget* widget : m_trailing) {
            m_headerLayout->removeWidget(widget);
            m_headerWrapRow->removeWidget(widget);
        }
        if (m_searchBar) {
            m_headerLayout->removeWidget(m_searchBar);
            m_headerWrapRow->removeWidget(m_searchBar);
            m_headerSearchRow->removeWidget(m_searchBar);
        }
        while (m_headerWrapRow->count()) {
            delete m_headerWrapRow->takeAt(0);
        }
        while (m_headerSearchRow->count()) {
            delete m_headerSearchRow->takeAt(0);
        }

        if (mode == Mode::Inline) {
            for (QWidget* widget : m_trailing) {
                m_headerLayout->addWidget(widget);
            }
            if (searchHere) {
                m_headerLayout->addWidget(m_searchBar);
            }
        } else {
            for (QWidget* widget : m_trailing) {
                m_headerWrapRow->addWidget(widget);
            }
            m_headerWrapRow->addStretch(1);
            if (searchHere) {
                if (mode == Mode::Wrapped) {
                    m_headerWrapRow->addWidget(m_searchBar);
                } else {
                    m_headerSearchRow->addWidget(m_searchBar, 1);
                }
            }
        }
        m_header->setProperty("headerMode",
                              mode == Mode::Inline    ? QStringLiteral("inline")
                              : mode == Mode::Wrapped ? QStringLiteral("wrapped")
                                                      : QStringLiteral("three-rows"));

        // A wrapped blurb grows with its lines: the header column does not ask
        // the label for its height-for-width, so the label asks for itself.
        if (m_supportingLabel && !m_supportingLabel->isHidden()) {
            const int blurbWidth = qMin(BlurbWidth, qMax(1, available));
            m_supportingLabel->setMinimumHeight(m_supportingLabel->heightForWidth(blurbWidth));
        }
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
        // A headline row that also carries 44 px controls can round the label a
        // pixel short of its font height; pin the minimum so it never does.
        m_headlineLabel->setMinimumHeight(m_headlineLabel->fontMetrics().height());
        m_headlineLabel->setStyleSheet(
            QStringLiteral("color: %1; background: transparent;").arg(theme()->hex(Role::OnSurface)));

        m_supportingLabel->setFont(theme()->font(TypeRole::BodySmall));
        m_supportingLabel->setStyleSheet(
            QStringLiteral("color: %1; background: transparent;").arg(theme()->hex(Role::OnSurfaceVariant)));

        update();
    }

} // namespace Material
