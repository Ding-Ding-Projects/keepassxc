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

#include "MaterialSwitch.h"
#include "MaterialElevation.h"
#include "MaterialIcons.h"
#include "MaterialTheme.h"

#include <QEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QPropertyAnimation>

namespace Material
{
    namespace
    {
        constexpr int TrackBorder = 2;
        constexpr int CheckSize = 16;
        // The design pins the knob's left edge, not its centre: 6px when off and
        // 24px when on, at 16px and 24px across. Both anchors are therefore the
        // centres those left edges imply.
        constexpr qreal KnobOffCenter = 6.0 + Switch::KnobSizeOff / 2.0; // 14
        constexpr qreal KnobOnCenter = 24.0 + Switch::KnobSizeOn / 2.0; // 36

        constexpr qreal HoverAlpha = 0.08;
        constexpr qreal DisabledOpacity = 0.38;

        /** The design's standard easing, cubic-bezier(.2, 0, 0, 1). */
        QEasingCurve standardCurve()
        {
            QEasingCurve curve(QEasingCurve::BezierSpline);
            curve.addCubicBezierSegment(QPointF(0.2, 0.0), QPointF(0.0, 1.0), QPointF(1.0, 1.0));
            return curve;
        }

        QColor lerp(const QColor& from, const QColor& to, qreal t)
        {
            t = qBound(0.0, t, 1.0);
            return QColor::fromRgbF(from.redF() + (to.redF() - from.redF()) * t,
                                    from.greenF() + (to.greenF() - from.greenF()) * t,
                                    from.blueF() + (to.blueF() - from.blueF()) * t,
                                    from.alphaF() + (to.alphaF() - from.alphaF()) * t);
        }
    } // namespace

    Switch::Switch(QWidget* parent)
        : QAbstractButton(parent)
    {
        setCheckable(true);
        setAttribute(Qt::WA_Hover);
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::StrongFocus);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

        m_animation = new QPropertyAnimation(this, "knobPosition", this);
        m_animation->setDuration(Duration::Toggle);
        m_animation->setEasingCurve(standardCurve());

        connect(theme(), &Theme::changed, this, [this] { update(); });
    }

    Switch::~Switch() = default;

    QSize Switch::sizeHint() const
    {
        return {TrackWidth, TrackHeight};
    }

    QSize Switch::minimumSizeHint() const
    {
        return {TrackWidth, TrackHeight};
    }

    qreal Switch::knobPosition() const
    {
        return m_knobPosition;
    }

    void Switch::setKnobPosition(qreal position)
    {
        position = qBound(0.0, position, 1.0);
        if (qFuzzyCompare(position + 1.0, m_knobPosition + 1.0)) {
            return;
        }
        m_knobPosition = position;
        update();
    }

    void Switch::animateTo(qreal position)
    {
        m_animation->stop();
        if (qFuzzyCompare(position + 1.0, m_knobPosition + 1.0)) {
            return;
        }
        m_animation->setStartValue(m_knobPosition);
        m_animation->setEndValue(position);
        m_animation->start();
    }

    void Switch::checkStateSet()
    {
        QAbstractButton::checkStateSet();
        animateTo(isChecked() ? 1.0 : 0.0);
    }

    void Switch::nextCheckState()
    {
        setChecked(!isChecked());
    }

    void Switch::enterEvent(QEnterEvent* event)
    {
        QAbstractButton::enterEvent(event);
        m_hovered = true;
        update();
    }

    void Switch::leaveEvent(QEvent* event)
    {
        QAbstractButton::leaveEvent(event);
        m_hovered = false;
        update();
    }

    void Switch::keyPressEvent(QKeyEvent* event)
    {
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
            if (!event->isAutoRepeat()) {
                animateClick();
            }
            event->accept();
            return;
        }
        QAbstractButton::keyPressEvent(event);
    }

    void Switch::paintEvent(QPaintEvent* event)
    {
        Q_UNUSED(event)

        QRect track(0, 0, TrackWidth, TrackHeight);
        track.moveCenter(rect().center());

        const qreal position = m_knobPosition;
        const QColor primary = theme()->color(Role::Primary);
        const QColor outline = theme()->color(Role::Outline);

        // Off is a solid surfaceContainerHighest track, so the switch still reads
        // as a control on the settings cards it sits on; it warms to primary with
        // the travel.
        const QColor fill = lerp(theme()->color(Role::SurfaceContainerHighest), primary, position);
        const QColor border = lerp(outline, primary, position);
        const QColor knobColor = lerp(outline, theme()->color(Role::OnPrimary), position);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        if (!isEnabled()) {
            painter.setOpacity(DisabledOpacity);
        }

        const qreal inset = TrackBorder / 2.0;
        const QRectF trackPath = QRectF(track).adjusted(inset, inset, -inset, -inset);
        painter.setPen(QPen(border, TrackBorder));
        painter.setBrush(fill);
        painter.drawRoundedRect(trackPath, trackPath.height() / 2.0, trackPath.height() / 2.0);

        if (isEnabled() && m_hovered) {
            painter.save();
            paintStateLayer(&painter, track, Shape::Full, theme()->color(Role::OnSurface), HoverAlpha);
            painter.restore();
        }

        // The knob grows from 16px to 24px while its centre travels from 14 to 36,
        // which leaves 6px of track outside the knob at either end.
        const qreal knobSize = KnobSizeOff + (KnobSizeOn - KnobSizeOff) * position;
        const qreal centerX = track.left() + KnobOffCenter + (KnobOnCenter - KnobOffCenter) * position;

        const QRectF knob(centerX - knobSize / 2.0, track.center().y() + 0.5 - knobSize / 2.0, knobSize, knobSize);
        painter.setPen(Qt::NoPen);
        painter.setBrush(knobColor);
        painter.drawEllipse(knob);

        if (position > 0.0) {
            const QRectF check(
                knob.center().x() - CheckSize / 2.0, knob.center().y() - CheckSize / 2.0, CheckSize, CheckSize);
            painter.setOpacity(painter.opacity() * position);
            painter.drawPixmap(check.toRect(), Icons::pixmap(QStringLiteral("check"), CheckSize, primary));
        }
    }

} // namespace Material
