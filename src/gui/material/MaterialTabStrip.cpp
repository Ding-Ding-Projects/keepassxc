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
#include "MaterialElevation.h"
#include "MaterialIcons.h"
#include "MaterialTheme.h"
#include "MaterialTabOverflow.h"

#include <QAction>
#include <QApplication>
#include <QContextMenuEvent>
#include <QFontMetrics>
#include <QMenu>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QSet>
#include <QStringList>

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
        setFocusPolicy(Qt::StrongFocus);
        setAccessibleName(tr("Open database tabs"));

        // The trailing controls are pinned as well as sized: ButtonBase fixes a
        // minimum width from its label metrics on construction, which otherwise
        // outvotes the diameter and pushes the add button past the strip's edge.
        m_searchButton = new IconButton(QStringLiteral("search"), this);
        m_searchButton->setDiameter(ControlSize);
        m_searchButton->setFixedSize(ControlSize, ControlSize);
        m_searchButton->setSymbolSize(19);
        m_searchButton->setToolTip(tr("Search open databases"));
        m_searchButton->setAccessibleName(m_searchButton->toolTip());
        connect(m_searchButton, &QAbstractButton::clicked, this, &TabStrip::openOverflow);

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
        resetPointerInteraction();
        m_tabs.append(tab);

        if (m_currentIndex < 0) {
            m_currentIndex = m_tabs.size() - 1;
            m_focusId = tab.id;
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

        const QString replacementFocus = m_focusId == id ? currentTab() : m_focusId;
        resetPointerInteraction();
        m_tabs.removeAt(index);
        if (m_tabs.isEmpty()) {
            m_currentIndex = -1;
        } else if (index < m_currentIndex) {
            --m_currentIndex;
        } else if (index == m_currentIndex) {
            m_currentIndex = qMin(m_currentIndex, m_tabs.size() - 1);
        }

        m_hoverIndex = -1;
        m_focusId = replacementFocus == id ? QString() : replacementFocus;
        if (m_focusId.isEmpty() && m_currentIndex >= 0 && m_currentIndex < m_tabs.size()) {
            m_focusId = m_tabs.at(m_currentIndex).id;
        }
        relayout();
        update();
    }

    void TabStrip::clear()
    {
        resetPointerInteraction();
        m_tabs.clear();
        m_currentIndex = -1;
        m_hoverIndex = -1;
        m_hoverClose = false;
        m_focusId.clear();
        relayout();
        update();
    }

    void TabStrip::setTabs(const QList<TabDescriptor>& descriptors, const QString& currentRuntimeId)
    {
        const QString hoveredId = m_hoverIndex >= 0 && m_hoverIndex < m_tabs.size() ? m_tabs.at(m_hoverIndex).id : QString();
        const QString focusedId = m_focusId;
        const QString previousCurrentId = currentTab();
        resetPointerInteraction();
        QList<Tab> reconciled;
        reconciled.reserve(descriptors.size());
        for (const auto& descriptor : descriptors) {
            if (descriptor.runtimeId.isEmpty()) continue;
            Tab tab;
            tab.id = descriptor.runtimeId;
            tab.persistenceKey = descriptor.persistenceKey;
            tab.symbol = descriptor.symbol;
            tab.label = descriptor.label;
            tab.pinned = descriptor.pinned;
            tab.persistable = descriptor.persistable;
            reconciled.append(tab);
        }
        m_tabs = reconciled;
        const QString requestedCurrentId = currentRuntimeId.isEmpty() ? previousCurrentId : currentRuntimeId;
        m_currentIndex = indexOf(requestedCurrentId);
        if (m_currentIndex < 0 && !m_tabs.isEmpty()) m_currentIndex = 0;
        m_hoverIndex = indexOf(hoveredId);
        m_focusId = indexOf(focusedId) >= 0 ? focusedId : currentTab();
        if (m_focusId.isEmpty() && !m_tabs.isEmpty()) m_focusId = m_tabs.first().id;
        relayout();
        update();
    }

    QList<TabDescriptor> TabStrip::tabs() const
    {
        QList<TabDescriptor> result;
        for (const auto& tab : m_tabs) {
            result.append({tab.id, tab.persistenceKey, tab.symbol, tab.label, tab.pinned, tab.persistable});
        }
        return result;
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

    void TabStrip::resetPointerInteraction()
    {
        m_pressedId.clear();
        m_pressedClose = false;
        m_dragSourceId.clear();
        m_dragBeforeId.clear();
        m_dragIndicatorX = -1;
        m_dragDropValid = false;
        m_dragging = false;
    }

    void TabStrip::updateDragInsertion(const QPoint& pos)
    {
        m_dragBeforeId.clear();
        m_dragIndicatorX = -1;
        m_dragDropValid = false;

        const int sourceIndex = indexOf(m_dragSourceId);
        if (sourceIndex < 0 || !m_tabs.at(sourceIndex).visible
            || pos.y() < m_tabs.at(sourceIndex).rect.top() || pos.y() > m_tabs.at(sourceIndex).rect.bottom()) {
            return;
        }

        const bool pinned = m_tabs.at(sourceIndex).pinned;
        QList<int> visiblePartition;
        for (int i = 0; i < m_tabs.size(); ++i) {
            if (i != sourceIndex && m_tabs.at(i).visible && m_tabs.at(i).pinned == pinned) {
                visiblePartition.append(i);
            }
        }
        if (visiblePartition.isEmpty()) {
            return;
        }

        int beforeIndex = -1;
        for (const int candidate : visiblePartition) {
            if (pos.x() < m_tabs.at(candidate).rect.center().x()) {
                beforeIndex = candidate;
                m_dragIndicatorX = m_tabs.at(candidate).rect.left();
                break;
            }
        }

        if (beforeIndex < 0) {
            const int lastVisible = visiblePartition.constLast();
            m_dragIndicatorX = m_tabs.at(lastVisible).rect.right() + 1;

            // An empty beforeId is the end of the source pin partition. If
            // hidden tabs remain in that partition, insert before the first
            // one instead so the drop does not leap past unseen tabs.
            for (int i = lastVisible + 1; i < m_tabs.size(); ++i) {
                if (i != sourceIndex && m_tabs.at(i).pinned == pinned) {
                    beforeIndex = i;
                    break;
                }
            }
        }

        if (beforeIndex >= 0) {
            m_dragBeforeId = m_tabs.at(beforeIndex).id;
        }

        QStringList currentPartition;
        for (const auto& tab : m_tabs) {
            if (tab.pinned == pinned) {
                currentPartition.append(tab.id);
            }
        }
        QStringList desiredPartition = currentPartition;
        desiredPartition.removeAll(m_dragSourceId);
        const int insertion = m_dragBeforeId.isEmpty() ? desiredPartition.size() : desiredPartition.indexOf(m_dragBeforeId);
        if (insertion < 0) {
            m_dragBeforeId.clear();
            m_dragIndicatorX = -1;
            return;
        }
        desiredPartition.insert(insertion, m_dragSourceId);
        m_dragDropValid = desiredPartition != currentPartition;
        if (!m_dragDropValid) {
            m_dragBeforeId.clear();
            m_dragIndicatorX = -1;
        }
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
        } else {
            // The badge cannot be narrower than the one every tab would need, so
            // reserving for that worst case never under-reserves.
            available -= overflowWidth(count) + TabSpacing;

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
                while (!shown.isEmpty() && needed > 0 && !m_tabs.at(shown.last()).pinned) {
                    needed -= widths.at(shown.takeLast()) + TabSpacing;
                }
                if (needed <= 0) {
                    shown.append(m_currentIndex);
                }
            }

            m_hiddenCount = count - shown.size();
        }

        int x = StripPadding;
        for (int index : shown) {
            Tab& tab = m_tabs[index];
            tab.visible = true;
            tab.rect = QRect(x, top, widths.at(index), Layout::TabHeight);
            tab.closeRect = tab.pinned ? QRect() : QRect(tab.rect.right() - TabPadRight - CloseSize + 1,
                                                         top + (Layout::TabHeight - CloseSize) / 2,
                                                         CloseSize,
                                                         CloseSize);
            x += widths.at(index) + TabSpacing;
        }

        // The chip is one more item in the tab row: same 2px gap, same baseline.
        if (m_hiddenCount > 0) {
            m_overflowRect = QRect(x, top, overflowWidth(m_hiddenCount), Layout::TabHeight);
        }
    }

    int TabStrip::overflowWidth(int hidden) const
    {
        const QFontMetrics metrics(overflowFont());
        return 2 * OverflowPadding + OverflowGlyphSize + OverflowGap + metrics.horizontalAdvance(tr("More"))
               + OverflowGap + badgeWidthFor(hidden);
    }

    void TabStrip::openOverflow()
    {
        if (!m_overflow) {
            m_overflow = new TabOverflow(window());
            connect(m_overflow, &TabOverflow::tabActivated, this, [this](const QString& id) {
                setCurrentTab(id);
                emit tabSelected(id);
            });
            connect(m_overflow, &TabOverflow::tabPinRequested, this, &TabStrip::tabPinRequested);
        }
        QSet<QString> hidden;
        for (const auto& tab : m_tabs) if (!tab.visible) hidden.insert(tab.id);
        m_overflow->setTabs(tabs(), currentTab(), hidden);
        m_overflow->openOverlay();
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
            if (hasFocus() && tab.id == m_focusId) {
                painter.setPen(QPen(theme()->color(Role::Primary), 2));
                painter.setBrush(Qt::NoBrush);
                painter.drawRoundedRect(tab.rect.adjusted(2, 2, -2, -2), Shape::Small, Shape::Small);
            }

            // Only the active tab is outlined; the rest are borderless.
            if (active) {
                painter.setBrush(Qt::NoBrush);
                painter.setPen(QPen(border, 1));
                painter.drawPath(tabPath(QRectF(tab.rect).adjusted(0.5, 0.5, -0.5, 0.0), Shape::Medium));
            }

            const QColor content = theme()->color(active ? Role::OnSurface : Role::OnSurfaceVariant);
            const QRect iconRect(tab.rect.x() + TabPadLeft,
                                 tab.rect.y() + (tab.rect.height() - TabIconSize) / 2,
                                 TabIconSize,
                                 TabIconSize);
            painter.drawPixmap(iconRect, Icons::pixmap(tab.symbol, TabIconSize, content));

            const int labelLeft = iconRect.right() + 1 + TabGap;
            const int trailingLeft = tab.pinned ? tab.rect.right() - TabPadRight - CloseSize + 1 : tab.closeRect.left();
            const int labelWidth = qMax(0, trailingLeft - TabGap - labelLeft);
            const QFont font = tabFont(active);
            const QFontMetrics metrics(font);
            painter.setFont(font);
            painter.setPen(content);
            painter.drawText(QRect(labelLeft, tab.rect.y(), labelWidth, tab.rect.height()),
                             Qt::AlignLeft | Qt::AlignVCenter,
                             metrics.elidedText(tab.label, Qt::ElideRight, labelWidth));

            if (tab.pinned) {
                const QRect pinRect(tab.rect.right() - TabPadRight - CloseSize + 1,
                                    tab.rect.y() + (tab.rect.height() - CloseSize) / 2,
                                    CloseSize,
                                    CloseSize);
                painter.drawPixmap(pinRect.adjusted(2, 2, -2, -2),
                                   Icons::pixmap(QStringLiteral("keep"), CloseGlyphSize, content));
                continue;
            }
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

        if (!m_overflowRect.isEmpty()) {
            if (m_overflowHovered) {
                painter.fillPath(tabPath(QRectF(m_overflowRect), Shape::Medium), hoverFill);
            }

            const QColor content = theme()->color(Role::OnSurfaceVariant);
            const QRect glyph(m_overflowRect.x() + OverflowPadding,
                              m_overflowRect.y() + (m_overflowRect.height() - OverflowGlyphSize) / 2,
                              OverflowGlyphSize,
                              OverflowGlyphSize);
            painter.drawPixmap(glyph, Icons::pixmap(QStringLiteral("more_horiz"), OverflowGlyphSize, content));

            // The badge is the chip's trailing content, inside its right padding.
            const QString count = QString::number(m_hiddenCount);
            const int badgeWidth = badgeWidthFor(m_hiddenCount);
            const QRect badge(m_overflowRect.right() + 1 - OverflowPadding - badgeWidth,
                              m_overflowRect.y() + (m_overflowRect.height() - BadgeHeight) / 2,
                              badgeWidth,
                              BadgeHeight);

            const int labelLeft = glyph.right() + 1 + OverflowGap;
            const QFont font = overflowFont();
            painter.setFont(font);
            painter.setPen(content);
            painter.drawText(QRect(labelLeft,
                                   m_overflowRect.y(),
                                   qMax(0, badge.left() - OverflowGap - labelLeft),
                                   m_overflowRect.height()),
                             Qt::AlignLeft | Qt::AlignVCenter,
                             tr("More"));

            const QFont countFont = theme()->font(TypeRole::LabelSmall);
            paintSurface(&painter, badge, Shape::Full, theme()->color(Role::SecondaryContainer));
            painter.setFont(countFont);
            painter.setPen(theme()->color(Role::OnSecondaryContainer));
            painter.drawText(badge, Qt::AlignCenter, count);
        }

        if (m_dragging && m_dragDropValid && m_dragIndicatorX >= 0) {
            painter.fillRect(QRect(m_dragIndicatorX - 1,
                                   height() - Layout::TabHeight + 3,
                                   3,
                                   Layout::TabHeight - 6),
                             theme()->color(Role::Primary));
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
        if (!m_overflowRect.isEmpty() && m_overflowRect.contains(pos)) {
            resetPointerInteraction();
            event->accept();
            openOverflow();
            return;
        }

        const int index = indexAt(pos);
        resetPointerInteraction();
        m_pressedId = index >= 0 ? m_tabs.at(index).id : QString();
        m_pressedClose = index >= 0 && m_tabs.at(index).closeRect.contains(pos);
        m_dragSourceId = m_pressedClose ? QString() : m_pressedId;
        m_pressPosition = pos;
        if (index >= 0) {
            m_focusId = m_tabs.at(index).id;
            setFocus(Qt::MouseFocusReason);
        }

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
        const QString pressedId = m_pressedId;
        const bool wasClose = m_pressedClose;
        const bool wasDragging = m_dragging;
        const bool dropValid = m_dragDropValid;
        const QString dragSourceId = m_dragSourceId;
        const QString dragBeforeId = m_dragBeforeId;
        resetPointerInteraction();

        if (event->button() == Qt::LeftButton && wasDragging && dropValid && !dragSourceId.isEmpty()
            && indexOf(dragSourceId) >= 0
            && (dragBeforeId.isEmpty() || indexOf(dragBeforeId) >= 0)
            && (dragBeforeId.isEmpty()
                || m_tabs.at(indexOf(dragBeforeId)).pinned == m_tabs.at(indexOf(dragSourceId)).pinned)) {
            emit tabMoveRequested(dragSourceId, dragBeforeId);
            update();
            event->accept();
            return;
        }

        const int pressedIndex = indexOf(pressedId);
        if (event->button() == Qt::LeftButton && wasClose && pressedIndex >= 0
            && m_tabs.at(pressedIndex).closeRect.contains(event->position().toPoint())) {
            emit tabCloseRequested(pressedId);
        }

        update();
        QWidget::mouseReleaseEvent(event);
    }

    void TabStrip::mouseMoveEvent(QMouseEvent* event)
    {
        const QPoint pos = event->position().toPoint();
        const int index = indexAt(pos);
        const bool overClose = !m_dragging && index >= 0 && m_tabs.at(index).closeRect.contains(pos);
        const bool overOverflow = !m_overflowRect.isEmpty() && m_overflowRect.contains(pos);
        const int sourceIndex = indexOf(m_dragSourceId);
        if (sourceIndex >= 0 && !m_pressedClose && (event->buttons() & Qt::LeftButton)
            && (m_dragging || (pos - m_pressPosition).manhattanLength() >= QApplication::startDragDistance())) {
            m_dragging = true;
            updateDragInsertion(pos);
        }
        if (index != m_hoverIndex || overClose != m_hoverClose || overOverflow != m_overflowHovered) {
            m_hoverIndex = index;
            m_hoverClose = overClose;
            m_overflowHovered = overOverflow;
            if (index >= 0) {
                setToolTip(m_tabs.at(index).label);
            } else {
                setToolTip(overOverflow ? tr("Show the remaining databases") : QString());
            }
            update();
        }
        QWidget::mouseMoveEvent(event);
    }

    void TabStrip::leaveEvent(QEvent* event)
    {
        if (m_hoverIndex >= 0 || m_hoverClose || m_overflowHovered) {
            m_hoverIndex = -1;
            m_hoverClose = false;
            m_overflowHovered = false;
            update();
        }
        QWidget::leaveEvent(event);
    }

    void TabStrip::keyPressEvent(QKeyEvent* event)
    {
        if (m_tabs.isEmpty()) {
            QWidget::keyPressEvent(event);
            return;
        }
        int focusIndex = indexOf(m_focusId);
        if (focusIndex < 0) {
            focusIndex = qMax(0, m_currentIndex);
            m_focusId = m_tabs.at(focusIndex).id;
        }
        const bool move = event->modifiers().testFlag(Qt::ControlModifier)
                          && event->modifiers().testFlag(Qt::ShiftModifier);
        const int direction = event->key() == Qt::Key_Left ? -1 : event->key() == Qt::Key_Right ? 1 : 0;
        if (direction != 0) {
            const int target = focusIndex + direction;
            if (target >= 0 && target < m_tabs.size() && m_tabs.at(target).pinned == m_tabs.at(focusIndex).pinned) {
                if (move) {
                    const QString before = direction < 0
                                               ? m_tabs.at(target).id
                                               : [&] {
                                                     for (int candidate = target + 1; candidate < m_tabs.size(); ++candidate) {
                                                         if (m_tabs.at(candidate).pinned == m_tabs.at(focusIndex).pinned) {
                                                             return m_tabs.at(candidate).id;
                                                         }
                                                     }
                                                     return QString();
                                                 }();
                    emit tabMoveRequested(m_tabs.at(focusIndex).id, before);
                } else {
                    m_focusId = m_tabs.at(target).id;
                    update();
                }
            }
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
            setCurrentTab(m_focusId);
            emit tabSelected(m_focusId);
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_Escape && m_dragging) {
            resetPointerInteraction();
            update();
            event->accept();
            return;
        }
        QWidget::keyPressEvent(event);
    }

    void TabStrip::contextMenuEvent(QContextMenuEvent* event)
    {
        const int index = indexAt(event->pos());
        if (index < 0 || index >= m_tabs.size()) {
            QWidget::contextMenuEvent(event);
            return;
        }
        const Tab& tab = m_tabs.at(index);
        QMenu menu(this);
        QAction* pin = menu.addAction(Icons::symbol(tab.pinned ? QStringLiteral("keep_off") : QStringLiteral("keep")),
                                      tab.pinned ? tr("Unpin tab") : tr("Pin tab"));
        QAction* earlier = menu.addAction(Icons::symbol(QStringLiteral("arrow_back")), tr("Move tab earlier"));
        QAction* later = menu.addAction(Icons::symbol(QStringLiteral("arrow_forward")), tr("Move tab later"));
        QAction* close = menu.addAction(Icons::symbol(QStringLiteral("close")), tr("Close tab"));
        const bool canEarlier = index > 0 && m_tabs.at(index - 1).pinned == tab.pinned;
        const bool canLater = index + 1 < m_tabs.size() && m_tabs.at(index + 1).pinned == tab.pinned;
        earlier->setEnabled(canEarlier);
        later->setEnabled(canLater);
        const QAction* chosen = menu.exec(event->globalPos());
        if (chosen == pin) {
            emit tabPinRequested(tab.id, !tab.pinned);
        } else if (chosen == earlier && canEarlier) {
            emit tabMoveRequested(tab.id, m_tabs.at(index - 1).id);
        } else if (chosen == later && canLater) {
            QString before;
            bool passedAdjacent = false;
            for (int candidate = index + 1; candidate < m_tabs.size(); ++candidate) {
                if (m_tabs.at(candidate).pinned == tab.pinned) {
                    if (passedAdjacent) {
                        before = m_tabs.at(candidate).id;
                        break;
                    }
                    // The adjacent same-partition tab is the one being moved
                    // past; the next one is the stable insertion anchor.
                    passedAdjacent = true;
                }
            }
            emit tabMoveRequested(tab.id, before);
        } else if (chosen == close) {
            emit tabCloseRequested(tab.id);
        }
        event->accept();
    }

} // namespace Material
