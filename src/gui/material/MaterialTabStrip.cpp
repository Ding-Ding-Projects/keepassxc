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

#include "MaterialTabStrip.h"

#include "MaterialButtons.h"
#include "MaterialChip.h"
#include "MaterialElevation.h"
#include "MaterialIcons.h"
#include "MaterialTheme.h"

#include <QAction>
#include <QFontMetrics>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

namespace Material
{
    namespace
    {
        constexpr int StripPadding = 10;
        constexpr int TabSpacing = 2;
        constexpr int TabPadLeft = 12;
        constexpr int TabPadRight = 10;
        constexpr int TabGap = 8;
        constexpr int TabIconSize = 16;
        constexpr int CloseSize = 20;
        constexpr int CloseGlyphSize = 16;
        constexpr int MinTabWidth = 120;
        constexpr int MaxTabWidth = 240;
        constexpr int ControlSize = 34;
        constexpr int ControlBottomMargin = 2;
        constexpr int OverflowGap = 6;
        constexpr int OverflowPadding = 12;
        constexpr int OverflowGlyphSize = 18;
        constexpr int BadgeHeight = 18;
        constexpr int BadgeMinWidth = 18;
        constexpr int BadgePadding = 5;

        QFont tabFont(bool active)
        {
            QFont font = theme()->font(TypeRole::BodySmall);
            font.setWeight(active ? QFont::Medium : QFont::Normal);
            return font;
        }

        /** The overflow affordance carries the tabs' 13px label at medium weight. */
        QFont overflowFont()
        {
            QFont font = theme()->font(TypeRole::BodySmall);
            font.setWeight(QFont::Medium);
            return font;
        }

        /** Width of the count badge for @p hidden tabs, padding included. */
        int badgeWidthFor(int hidden)
        {
            const QFontMetrics metrics(theme()->font(TypeRole::LabelSmall));
            return qMax(BadgeMinWidth, metrics.horizontalAdvance(QString::number(hidden)) + 2 * BadgePadding);
        }

        /**
         * An open path following a tab outline: up the left edge, around the two
         * top corners and down the right edge. Filling it closes the shape along
         * the bottom, stroking it leaves the bottom edge open as the design wants.
         */
        QPainterPath tabPath(const QRectF& rect, qreal radius)
        {
            QPainterPath path;
            path.moveTo(rect.left(), rect.bottom());
            path.lineTo(rect.left(), rect.top() + radius);
            path.arcTo(QRectF(rect.left(), rect.top(), 2 * radius, 2 * radius), 180, -90);
            path.lineTo(rect.right() - radius, rect.top());
            path.arcTo(QRectF(rect.right() - 2 * radius, rect.top(), 2 * radius, 2 * radius), 90, -90);
            path.lineTo(rect.right(), rect.bottom());
            return path;
        }
    } // namespace

    TabStrip::TabStrip(QWidget* parent)
        : QWidget(parent)
    {
        setFixedHeight(Layout::TabStripHeight);
        setMouseTracking(true);

        // The trailing controls are pinned as well as sized: ButtonBase fixes a
        // minimum width from its label metrics on construction, which otherwise
        // outvotes the diameter and pushes the add button past the strip's edge.
        m_searchButton = new IconButton(QStringLiteral("search"), this);
        m_searchButton->setDiameter(ControlSize);
        m_searchButton->setFixedSize(ControlSize, ControlSize);
        m_searchButton->setSymbolSize(19);
        m_searchButton->setToolTip(tr("Search open databases"));
        m_searchButton->setAccessibleName(m_searchButton->toolTip());
        connect(m_searchButton, &QAbstractButton::clicked, this, &TabStrip::searchRequested);

        m_addButton = new IconButton(QStringLiteral("add"), this);
        m_addButton->setDiameter(ControlSize);
        m_addButton->setFixedSize(ControlSize, ControlSize);
        m_addButton->setSymbolSize(20);
        m_addButton->setToolTip(tr("Open a database in a new tab"));
        m_addButton->setAccessibleName(m_addButton->toolTip());
        connect(m_addButton, &QAbstractButton::clicked, this, &TabStrip::newTabRequested);

        connect(theme(), &Theme::changed, this, [this] {
            relayout();
            update();
        });
    }

    TabStrip::~TabStrip() = default;

    void TabStrip::addTab(const QString& id, const QString& symbol, const QString& label)
    {
        if (id.isEmpty() || indexOf(id) >= 0) {
            return;
        }

        Tab tab;
        tab.id = id;
        tab.symbol = symbol;
        tab.label = label;
        m_tabs.append(tab);

        if (m_currentIndex < 0) {
            m_currentIndex = m_tabs.size() - 1;
        }

        relayout();
        update();
    }

    void TabStrip::removeTab(const QString& id)
    {
        const int index = indexOf(id);
        if (index < 0) {
            return;
        }

        m_tabs.removeAt(index);
        if (m_tabs.isEmpty()) {
            m_currentIndex = -1;
        } else if (index < m_currentIndex) {
            --m_currentIndex;
        } else if (index == m_currentIndex) {
            m_currentIndex = qMin(m_currentIndex, m_tabs.size() - 1);
        }

        m_hoverIndex = -1;
        m_pressedIndex = -1;
        relayout();
        update();
    }

    void TabStrip::clear()
    {
        m_tabs.clear();
        m_currentIndex = -1;
        m_hoverIndex = -1;
        m_pressedIndex = -1;
        m_pressedClose = false;
        m_hoverClose = false;
        relayout();
        update();
    }

    void TabStrip::setTabLabel(const QString& id, const QString& label)
    {
        const int index = indexOf(id);
        if (index < 0 || m_tabs.at(index).label == label) {
            return;
        }
        m_tabs[index].label = label;
        relayout();
        update();
    }

    void TabStrip::setTabSymbol(const QString& id, const QString& symbol)
    {
        const int index = indexOf(id);
        if (index < 0 || m_tabs.at(index).symbol == symbol) {
            return;
        }
        m_tabs[index].symbol = symbol;
        update();
    }

    QString TabStrip::currentTab() const
    {
        if (m_currentIndex < 0 || m_currentIndex >= m_tabs.size()) {
            return {};
        }
        return m_tabs.at(m_currentIndex).id;
    }

    void TabStrip::setCurrentTab(const QString& id)
    {
        const int index = indexOf(id);
        if (index < 0 || index == m_currentIndex) {
            return;
        }
        m_currentIndex = index;
        // The active tab is never allowed to hide in the overflow menu.
        relayout();
        update();
    }

    int TabStrip::count() const
    {
        return m_tabs.size();
    }

    QSize TabStrip::sizeHint() const
    {
        return {2 * StripPadding + 3 * MaxTabWidth, Layout::TabStripHeight};
    }

    QSize TabStrip::minimumSizeHint() const
    {
        return {2 * StripPadding + MinTabWidth + 2 * ControlSize, Layout::TabStripHeight};
    }

    int TabStrip::indexOf(const QString& id) const
    {
        for (int i = 0; i < m_tabs.size(); ++i) {
            if (m_tabs.at(i).id == id) {
                return i;
            }
        }
        return -1;
    }

    int TabStrip::indexAt(const QPoint& pos) const
    {
        for (int i = 0; i < m_tabs.size(); ++i) {
            const Tab& tab = m_tabs.at(i);
            if (tab.visible && tab.rect.contains(pos)) {
                return i;
            }
        }
        return -1;
    }

    void TabStrip::relayout()
    {
        const int top = height() - Layout::TabHeight;
        const int controlTop = height() - ControlBottomMargin - ControlSize;

        int right = width() - StripPadding;
        m_addButton->setGeometry(right - ControlSize, controlTop, ControlSize, ControlSize);
        right -= ControlSize + 2;
        m_searchButton->setGeometry(right - ControlSize, controlTop, ControlSize, ControlSize);
        right -= ControlSize + OverflowGap;

        for (auto& tab : m_tabs) {
            tab.visible = false;
            tab.rect = QRect();
            tab.closeRect = QRect();
        }
        m_overflowRect = QRect();
        m_hiddenCount = 0;

        const int count = m_tabs.size();
        if (count == 0) {
            return;
        }

        int available = right - StripPadding;

        // Tabs size to their content, capped at 240px; when the row overflows they
        // share the space evenly down to a floor, and only then start collapsing
        // into the overflow chip.
        QList<int> widths;
        widths.reserve(count);
        int total = (count - 1) * TabSpacing;
        for (int i = 0; i < count; ++i) {
            const QFontMetrics metrics(tabFont(i == m_currentIndex));
            const int content = TabPadLeft + TabIconSize + TabGap + metrics.horizontalAdvance(m_tabs.at(i).label)
                                + TabGap + CloseSize + TabPadRight;
            widths.append(qMin(MaxTabWidth, content));
            total += widths.at(i);
        }

        if (total > available) {
            const int share = qMax(MinTabWidth, (available - (count - 1) * TabSpacing) / count);
            total = (count - 1) * TabSpacing;
            for (int i = 0; i < count; ++i) {
                widths[i] = qMin(widths.at(i), share);
                total += widths.at(i);
            }
        }

        QList<int> shown;
        if (total <= available) {
            for (int i = 0; i < count; ++i) {
                shown.append(i);
            }
            m_overflowChip->hide();
        } else {
            const int chipWidth = m_overflowChip->sizeHint().width() + OverflowBadgeReserve;
            available -= chipWidth + OverflowGap;

            int used = 0;
            for (int i = 0; i < count; ++i) {
                const int step = widths.at(i) + (shown.isEmpty() ? 0 : TabSpacing);
                if (used + step > available) {
                    break;
                }
                used += step;
                shown.append(i);
            }

            // The active tab always keeps a slot, even when it sorts into the tail.
            if (m_currentIndex >= 0 && !shown.contains(m_currentIndex)) {
                int needed = widths.at(m_currentIndex) + TabSpacing;
                while (!shown.isEmpty() && needed > 0) {
                    needed -= widths.at(shown.takeLast()) + TabSpacing;
                }
                shown.append(m_currentIndex);
            }

            m_overflowChip->setVisible(shown.size() < count);
        }

        int x = StripPadding;
        for (int index : shown) {
            Tab& tab = m_tabs[index];
            tab.visible = true;
            tab.rect = QRect(x, top, widths.at(index), Layout::TabHeight);
            tab.closeRect = QRect(tab.rect.right() - TabPadRight - CloseSize + 1,
                                  top + (Layout::TabHeight - CloseSize) / 2,
                                  CloseSize,
                                  CloseSize);
            x += widths.at(index) + TabSpacing;
        }

        if (m_overflowChip->isVisible()) {
            const QSize chipSize = m_overflowChip->sizeHint();
            m_overflowChip->setGeometry(x + OverflowGap - TabSpacing,
                                        height() - ControlBottomMargin - chipSize.height(),
                                        chipSize.width(),
                                        chipSize.height());
        }
    }

    void TabStrip::showOverflowMenu()
    {
        QMenu menu(this);
        menu.setFont(theme()->font(TypeRole::BodyMedium));
        for (int i = 0; i < m_tabs.size(); ++i) {
            const Tab& tab = m_tabs.at(i);
            if (tab.visible) {
                continue;
            }
            QAction* action = menu.addAction(Icons::symbol(tab.symbol), tab.label);
            action->setData(tab.id);
        }
        if (menu.isEmpty()) {
            return;
        }

        const QPoint origin(m_overflowChip->x(), m_overflowChip->geometry().bottom() + 2);
        const QAction* chosen = menu.exec(mapToGlobal(origin));
        if (!chosen) {
            return;
        }

        const QString id = chosen->data().toString();
        setCurrentTab(id);
        emit tabSelected(id);
    }

    void TabStrip::paintEvent(QPaintEvent* event)
    {
        Q_UNUSED(event)

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.fillRect(rect(), theme()->color(Role::SurfaceContainer));
        painter.fillRect(QRect(0, height() - 1, width(), 1), theme()->color(Role::OutlineVariant));

        const QColor border = theme()->color(Role::OutlineVariant);
        const QColor activeFill = theme()->color(Role::Surface);
        const QColor hoverFill = theme()->color(Role::SurfaceContainerHigh);

        for (int i = 0; i < m_tabs.size(); ++i) {
            const Tab& tab = m_tabs.at(i);
            if (!tab.visible || tab.rect.isEmpty()) {
                continue;
            }

            const bool active = i == m_currentIndex;
            const QPainterPath fillPath = tabPath(QRectF(tab.rect), Shape::Medium);
            if (active) {
                // Merges with the content below: same colour, and it covers the
                // strip's bottom hairline.
                painter.fillPath(fillPath, activeFill);
            } else if (i == m_hoverIndex) {
                painter.fillPath(fillPath, hoverFill);
            }

            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(border, 1));
            painter.drawPath(tabPath(QRectF(tab.rect).adjusted(0.5, 0.5, -0.5, 0.0), Shape::Medium));

            const QColor content = theme()->color(active ? Role::OnSurface : Role::OnSurfaceVariant);
            const QRect iconRect(tab.rect.x() + TabPadLeft,
                                 tab.rect.y() + (tab.rect.height() - TabIconSize) / 2,
                                 TabIconSize,
                                 TabIconSize);
            painter.drawPixmap(iconRect, Icons::pixmap(tab.symbol, TabIconSize, content));

            const int labelLeft = iconRect.right() + 1 + TabGap;
            const int labelWidth = qMax(0, tab.closeRect.left() - TabGap - labelLeft);
            const QFont font = tabFont(active);
            const QFontMetrics metrics(font);
            painter.setFont(font);
            painter.setPen(content);
            painter.drawText(QRect(labelLeft, tab.rect.y(), labelWidth, tab.rect.height()),
                             Qt::AlignLeft | Qt::AlignVCenter,
                             metrics.elidedText(tab.label, Qt::ElideRight, labelWidth));

            const bool closeHovered = i == m_hoverIndex && m_hoverClose;
            if (closeHovered) {
                paintSurface(&painter, tab.closeRect, Shape::Full, theme()->color(Role::SurfaceContainerHighest));
            }
            QColor closeTint = content;
            closeTint.setAlphaF(closeHovered ? 1.0 : 0.55);
            const QRect closeGlyph(tab.closeRect.x() + (CloseSize - CloseGlyphSize) / 2,
                                   tab.closeRect.y() + (CloseSize - CloseGlyphSize) / 2,
                                   CloseGlyphSize,
                                   CloseGlyphSize);
            painter.drawPixmap(closeGlyph, Icons::pixmap(QStringLiteral("close"), CloseGlyphSize, closeTint));
        }

        if (m_overflowChip->isVisible()) {
            int hidden = 0;
            for (int i = 0; i < m_tabs.size(); ++i) {
                hidden += m_tabs.at(i).visible ? 0 : 1;
            }
            if (hidden > 0) {
                const QString text = QString::number(hidden);
                const QFont font = theme()->font(TypeRole::LabelSmall);
                const QFontMetrics metrics(font);
                const int badgeWidth = qMax(BadgeHeight, metrics.horizontalAdvance(text) + 10);
                QRect badge(0, 0, badgeWidth, BadgeHeight);
                badge.moveCenter(QPoint(m_overflowChip->geometry().right() + 4 + badgeWidth / 2,
                                        m_overflowChip->geometry().center().y()));
                paintSurface(&painter, badge, Shape::Full, theme()->color(Role::SecondaryContainer));
                painter.setFont(font);
                painter.setPen(theme()->color(Role::OnSecondaryContainer));
                painter.drawText(badge, Qt::AlignCenter, text);
            }
        }
    }

    void TabStrip::resizeEvent(QResizeEvent* event)
    {
        QWidget::resizeEvent(event);
        relayout();
    }

    void TabStrip::mousePressEvent(QMouseEvent* event)
    {
        if (event->button() != Qt::LeftButton) {
            QWidget::mousePressEvent(event);
            return;
        }

        const QPoint pos = event->position().toPoint();
        const int index = indexAt(pos);
        m_pressedIndex = index;
        m_pressedClose = index >= 0 && m_tabs.at(index).closeRect.contains(pos);

        if (index >= 0 && !m_pressedClose && index != m_currentIndex) {
            m_currentIndex = index;
            relayout();
            emit tabSelected(m_tabs.at(m_currentIndex).id);
        }

        update();
        event->accept();
    }

    void TabStrip::mouseReleaseEvent(QMouseEvent* event)
    {
        const int pressed = m_pressedIndex;
        const bool wasClose = m_pressedClose;
        m_pressedIndex = -1;
        m_pressedClose = false;

        if (event->button() == Qt::LeftButton && wasClose && pressed >= 0 && pressed < m_tabs.size()
            && m_tabs.at(pressed).closeRect.contains(event->position().toPoint())) {
            emit tabCloseRequested(m_tabs.at(pressed).id);
        }

        update();
        QWidget::mouseReleaseEvent(event);
    }

    void TabStrip::mouseMoveEvent(QMouseEvent* event)
    {
        const QPoint pos = event->position().toPoint();
        const int index = indexAt(pos);
        const bool overClose = index >= 0 && m_tabs.at(index).closeRect.contains(pos);
        if (index != m_hoverIndex || overClose != m_hoverClose) {
            m_hoverIndex = index;
            m_hoverClose = overClose;
            setToolTip(index >= 0 ? m_tabs.at(index).label : QString());
            update();
        }
        QWidget::mouseMoveEvent(event);
    }

    void TabStrip::leaveEvent(QEvent* event)
    {
        if (m_hoverIndex >= 0 || m_hoverClose) {
            m_hoverIndex = -1;
            m_hoverClose = false;
            update();
        }
        QWidget::leaveEvent(event);
    }

} // namespace Material
