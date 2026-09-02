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

#include "MaterialSlider.h"

#include "MaterialTheme.h"

#include <QEnterEvent>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QStyle>

namespace Material
{
    namespace
    {
        constexpr int TrackHeight = 16;
        constexpr int TrackRadius = 8;
        constexpr int HandleWidth = 4;
        constexpr int HandlePressedWidth = 2;
        constexpr int HandleHeight = 44;
        constexpr int HandleGap = 6;
        constexpr int StopSize = 4;
        constexpr int ControlHeight = 44;
        constexpr int LabelHeight = 28;
        constexpr int LabelPaddingX = 10;
        constexpr int LabelGap = 6;
        constexpr int MinimumLength = 120;
        constexpr int EndInset = 2;
    } // namespace

    Slider::Slider(QWidget* parent)
        : QSlider(Qt::Horizontal, parent)
    {
        init();
    }

    Slider::Slider(Qt::Orientation orientation, QWidget* parent)
        : QSlider(orientation, parent)
    {
        init();
    }

    void Slider::init()
    {
        setOrientation(Qt::Horizontal);
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::StrongFocus);
        setAttribute(Qt::WA_Hover, true);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        connect(theme(), &Theme::changed, this, [this] { update(); });
    }

    bool Slider::showsValueLabel() const
    {
        return m_showsValueLabel;
    }

    void Slider::setShowsValueLabel(bool show)
    {
        m_showsValueLabel = show;
        update();
    }

    void Slider::setValueLabelSuffix(const QString& suffix)
    {
        m_suffix = suffix;
        update();
    }

    QSize Slider::sizeHint() const
    {
        return QSize(MinimumLength * 2, ControlHeight);
    }

    QSize Slider::minimumSizeHint() const
    {
        return QSize(MinimumLength, ControlHeight);
    }

    // The track spans the widget minus the half handle at each end, so the
    // handle bar never leaves the control at either extreme.
    QRect Slider::trackRect() const
    {
        return QRect(HandleWidth / 2 + EndInset,
                     (height() - TrackHeight) / 2,
                     qMax(0, width() - HandleWidth - 2 * EndInset),
                     TrackHeight);
    }

    int Slider::handleCentre() const
    {
        const QRect track = trackRect();
        const int span = qMax(1, maximum() - minimum());
        const int position = QStyle::sliderPositionFromValue(minimum(), maximum(), sliderPosition(), track.width(), false);
        Q_UNUSED(span);
        return track.left() + position;
    }

    int Slider::valueAt(const QPoint& position) const
    {
        const QRect track = trackRect();
        const int x = qBound(0, position.x() - track.left(), track.width());
        return QStyle::sliderValueFromPosition(minimum(), maximum(), x, track.width(), false);
    }

    void Slider::paintEvent(QPaintEvent*)
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        const bool enabled = isEnabled();
        const QRect track = trackRect();
        const int centre = handleCentre();
        const int handleWidth = m_dragging ? HandlePressedWidth : HandleWidth;
        const QColor active = theme()->color(enabled ? Role::Primary : Role::OnSurfaceVariant);
        const QColor inactive = theme()->color(enabled ? Role::SecondaryContainer : Role::SurfaceContainerHighest);

        // Active side: from the start to the handle, minus the gap, with the
        // outer corners fully round and the inner corners tight.
        const int activeRight = centre - HandleGap - handleWidth / 2;
        if (activeRight > track.left()) {
            QPainterPath path;
            const QRect activeRect(track.left(), track.top(), activeRight - track.left(), track.height());
            path.addRoundedRect(activeRect, TrackRadius, TrackRadius);
            QPainterPath inner;
            inner.addRoundedRect(activeRect.adjusted(activeRect.width() / 2, 0, 0, 0), 2, 2);
            painter.fillPath(path.united(inner), active);
        }

        // Inactive side: from the handle plus the gap to the end.
        const int inactiveLeft = centre + HandleGap + (handleWidth + 1) / 2;
        if (inactiveLeft < track.right()) {
            QPainterPath path;
            const QRect inactiveRect(inactiveLeft, track.top(), track.right() - inactiveLeft + 1, track.height());
            path.addRoundedRect(inactiveRect, TrackRadius, TrackRadius);
            QPainterPath inner;
            inner.addRoundedRect(inactiveRect.adjusted(0, 0, -inactiveRect.width() / 2, 0), 2, 2);
            painter.fillPath(path.united(inner), inactive);

            // The stop indicator marks the far end of the range.
            const QRect stop(track.right() - StopSize - (TrackHeight - StopSize) / 2 + 1,
                             track.top() + (TrackHeight - StopSize) / 2,
                             StopSize,
                             StopSize);
            painter.setPen(Qt::NoPen);
            painter.setBrush(active);
            painter.drawEllipse(stop);
        }

        // The handle bar.
        const QRect handle(centre - handleWidth / 2, (height() - HandleHeight) / 2, handleWidth, HandleHeight);
        painter.setPen(Qt::NoPen);
        painter.setBrush(active);
        painter.drawRoundedRect(handle, handleWidth / 2.0, handleWidth / 2.0);

        // Keyboard focus: a soft primary ring around the handle.
        if (hasFocus() && !m_dragging) {
            QColor ring = theme()->color(Role::Primary);
            ring.setAlphaF(0.24);
            painter.setBrush(ring);
            painter.drawRoundedRect(handle.adjusted(-6, -2, 6, 2), 6, 6);
        }

        // The value label rides above the handle while it is dragged.
        if (m_dragging && m_showsValueLabel) {
            const QString text = QString::number(sliderPosition()) + m_suffix;
            QFont font = theme()->font(TypeRole::LabelLarge);
            const QFontMetrics metrics(font);
            const int labelWidth = metrics.horizontalAdvance(text) + 2 * LabelPaddingX;
            QRect label(centre - labelWidth / 2, handle.top() - LabelGap - LabelHeight, labelWidth, LabelHeight);
            if (label.top() < 0) {
                label.moveTop(0);
            }
            label.moveLeft(qBound(0, label.left(), qMax(0, width() - labelWidth)));
            painter.setBrush(theme()->color(Role::InverseSurface));
            painter.drawRoundedRect(label, LabelHeight / 2.0, LabelHeight / 2.0);
            painter.setFont(font);
            painter.setPen(theme()->color(Role::InverseOnSurface));
            painter.drawText(label, Qt::AlignCenter, text);
        }
    }

    void Slider::mousePressEvent(QMouseEvent* event)
    {
        if (event->button() != Qt::LeftButton || !isEnabled()) {
            event->ignore();
            return;
        }
        m_dragging = true;
        setSliderDown(true);
        setSliderPosition(valueAt(event->pos()));
        if (!hasTracking()) {
            update();
        }
        event->accept();
    }

    void Slider::mouseMoveEvent(QMouseEvent* event)
    {
        if (!m_dragging) {
            event->ignore();
            return;
        }
        setSliderPosition(valueAt(event->pos()));
        update();
        event->accept();
    }

    void Slider::mouseReleaseEvent(QMouseEvent* event)
    {
        if (!m_dragging) {
            event->ignore();
            return;
        }
        m_dragging = false;
        setSliderPosition(valueAt(event->pos()));
        setSliderDown(false);
        update();
        event->accept();
    }

    void Slider::enterEvent(QEnterEvent* event)
    {
        m_hovered = true;
        update();
        QSlider::enterEvent(event);
    }

    void Slider::leaveEvent(QEvent* event)
    {
        m_hovered = false;
        update();
        QSlider::leaveEvent(event);
    }

} // namespace Material
