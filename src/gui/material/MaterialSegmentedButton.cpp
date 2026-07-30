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

#include "MaterialSegmentedButton.h"
#include "MaterialElevation.h"
#include "MaterialIcons.h"
#include "MaterialTheme.h"

#include <QEvent>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>

namespace Material
{
    namespace
    {
        constexpr int SegmentPadding = 14;
        constexpr int SegmentGap = 8; // glyph to label
        constexpr int SegmentSymbolSize = 18;
        constexpr int MinSegmentWidth = 56;

        constexpr qreal HoverAlpha = 0.08;
        constexpr qreal DisabledOpacity = 0.38;
    } // namespace

    SegmentedButton::SegmentedButton(QWidget* parent)
        : QWidget(parent)
    {
        setMouseTracking(true);
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::NoFocus);
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

        connect(theme(), &Theme::changed, this, [this] {
            updateGeometry();
            update();
        });
    }

    SegmentedButton::~SegmentedButton() = default;

    void SegmentedButton::addSegment(const QString& id, const QString& label, const QString& symbol)
    {
        m_segments.append({id, label, symbol, QRect()});
        if (m_currentIndex < 0) {
            m_currentIndex = 0;
        }
        relayout();
        updateGeometry();
        update();
    }

    void SegmentedButton::clear()
    {
        m_segments.clear();
        m_currentIndex = -1;
        m_hoverIndex = -1;
        updateGeometry();
        update();
    }

    int SegmentedButton::count() const
    {
        return static_cast<int>(m_segments.size());
    }

    QString SegmentedButton::currentSegment() const
    {
        if (m_currentIndex < 0 || m_currentIndex >= m_segments.size()) {
            return {};
        }
        return m_segments.at(m_currentIndex).id;
    }

    void SegmentedButton::setCurrentSegment(const QString& id)
    {
        const int index = indexOf(id);
        if (index < 0 || index == m_currentIndex) {
            return;
        }
        m_currentIndex = index;
        update();
        emit segmentSelected(id);
    }

    int SegmentedButton::indexOf(const QString& id) const
    {
        for (int i = 0; i < m_segments.size(); ++i) {
            if (m_segments.at(i).id == id) {
                return i;
            }
        }
        return -1;
    }

    int SegmentedButton::indexAt(const QPoint& pos) const
    {
        for (int i = 0; i < m_segments.size(); ++i) {
            if (m_segments.at(i).rect.contains(pos)) {
                return i;
            }
        }
        return -1;
    }

    void SegmentedButton::relayout()
    {
        const int total = count();
        if (total == 0) {
            return;
        }

        // Split the pill into equal segments on exact boundaries so rounding
        // never leaves a gap or a doubled divider.
        int left = 0;
        for (int i = 0; i < total; ++i) {
            const int right = qRound(static_cast<qreal>(width()) * (i + 1) / total);
            m_segments[i].rect = QRect(left, 0, right - left, height());
            left = right;
        }
    }

    QSize SegmentedButton::sizeHint() const
    {
        if (m_segments.isEmpty()) {
            return {MinSegmentWidth, Layout::ButtonHeight};
        }

        const QFontMetrics metrics(theme()->font(TypeRole::BodySmall));
        int widest = 0;
        for (const auto& segment : m_segments) {
            // Every segment reserves glyph room, selected or not, so picking a
            // different one never resizes the control.
            const int width =
                2 * SegmentPadding + SegmentSymbolSize + SegmentGap + metrics.horizontalAdvance(segment.label);
            widest = qMax(widest, width);
        }
        return {qMax(widest, MinSegmentWidth) * count(), Layout::ButtonHeight};
    }

    QSize SegmentedButton::minimumSizeHint() const
    {
        return {MinSegmentWidth * qMax(1, count()), Layout::ButtonHeight};
    }

    void SegmentedButton::resizeEvent(QResizeEvent* event)
    {
        QWidget::resizeEvent(event);
        relayout();
    }

    void SegmentedButton::mousePressEvent(QMouseEvent* event)
    {
        if (event->button() != Qt::LeftButton || !isEnabled()) {
            QWidget::mousePressEvent(event);
            return;
        }

        const int index = indexAt(event->position().toPoint());
        if (index >= 0) {
            setCurrentSegment(m_segments.at(index).id);
        }
        event->accept();
    }

    void SegmentedButton::mouseMoveEvent(QMouseEvent* event)
    {
        QWidget::mouseMoveEvent(event);
        const int index = indexAt(event->position().toPoint());
        if (index != m_hoverIndex) {
            m_hoverIndex = index;
            update();
        }
    }

    void SegmentedButton::leaveEvent(QEvent* event)
    {
        QWidget::leaveEvent(event);
        if (m_hoverIndex != -1) {
            m_hoverIndex = -1;
            update();
        }
    }

    void SegmentedButton::paintEvent(QPaintEvent* event)
    {
        Q_UNUSED(event)

        if (m_segments.isEmpty()) {
            return;
        }
        if (m_segments.first().rect.height() != height()) {
            relayout();
        }

        const QColor outline = theme()->color(Role::Outline);
        const QFont labelFont = theme()->font(TypeRole::BodySmall);
        const QFontMetrics metrics(labelFont);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        if (!isEnabled()) {
            painter.setOpacity(DisabledOpacity);
        }
        painter.setFont(labelFont);

        // Fills and dividers live inside the pill; the outline is stroked last
        // so it stays crisp over both.
        painter.save();
        painter.setClipPath(roundedPath(QRectF(rect()), Shape::Full));

        if (m_currentIndex >= 0 && m_currentIndex < m_segments.size()) {
            painter.fillRect(m_segments.at(m_currentIndex).rect, theme()->color(Role::SecondaryContainer));
        }
        if (isEnabled() && m_hoverIndex >= 0 && m_hoverIndex != m_currentIndex) {
            paintStateLayer(
                &painter, m_segments.at(m_hoverIndex).rect, Shape::None, theme()->color(Role::OnSurface), HoverAlpha);
        }

        painter.setPen(QPen(outline, 1));
        for (int i = 1; i < m_segments.size(); ++i) {
            const qreal x = m_segments.at(i).rect.left() + 0.5;
            painter.drawLine(QPointF(x, 0), QPointF(x, height()));
        }
        painter.restore();

        painter.setPen(QPen(outline, 1));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(roundedPath(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), Shape::Full));

        for (int i = 0; i < m_segments.size(); ++i) {
            const Segment& segment = m_segments.at(i);
            const bool selected = i == m_currentIndex;
            const QColor content = theme()->color(selected ? Role::OnSecondaryContainer : Role::OnSurface);
            const QString symbol =
                !segment.symbol.isEmpty() ? segment.symbol : (selected ? QStringLiteral("check") : QString());

            const int glyph = symbol.isEmpty() ? 0 : SegmentSymbolSize;
            const int gap = (glyph > 0 && !segment.label.isEmpty()) ? SegmentGap : 0;
            const int available = qMax(0, segment.rect.width() - 2 * SegmentPadding - glyph - gap);
            const QString label = metrics.elidedText(segment.label, Qt::ElideRight, available);
            const int labelWidth = label.isEmpty() ? 0 : metrics.horizontalAdvance(label);

            int x = segment.rect.left() + (segment.rect.width() - glyph - gap - labelWidth) / 2;
            if (glyph > 0) {
                const QRect glyphRect(x, (height() - SegmentSymbolSize) / 2, SegmentSymbolSize, SegmentSymbolSize);
                painter.drawPixmap(glyphRect, Icons::pixmap(symbol, SegmentSymbolSize, content));
                x += glyph + gap;
            }
            if (!label.isEmpty()) {
                painter.setPen(content);
                painter.drawText(QRect(x, 0, labelWidth, height()), Qt::AlignLeft | Qt::AlignVCenter, label);
            }
        }
    }

} // namespace Material
