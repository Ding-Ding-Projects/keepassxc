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

#include "MaterialSnackbar.h"

#include "MaterialElevation.h"

#include <QEasingCurve>
#include <QFontMetrics>
#include <QGraphicsDropShadowEffect>
#include <QMouseEvent>
#include <QPainter>
#include <QPropertyAnimation>
#include <QRegion>
#include <QTimer>

namespace Material
{
    namespace
    {
        /** Travel of the rise-in transition, reserved below every toast. */
        constexpr int Rise = 14;
        constexpr qreal EnterScale = 0.96;

        constexpr int StackGap = 8;
        constexpr int EdgeMargin = 16;
        constexpr int MaxVisible = 4;

        /** Room the el3 shadow needs outside the toast geometry. */
        constexpr int ShadowMargin = 24;

        constexpr int PanelPadding = 20;
        constexpr int PanelVerticalPadding = 14;
        constexpr int ActionSpacing = 24;
        constexpr int MinPanelWidth = 300;
        constexpr int MaxPanelWidth = 440;
        constexpr int MinPanelHeight = 48;

        /** The design's emphasised curve, cubic-bezier(.2, 0, 0, 1). */
        QEasingCurve emphasizedCurve()
        {
            QEasingCurve curve(QEasingCurve::BezierSpline);
            curve.addCubicBezierSegment(QPointF(0.2, 0.0), QPointF(0.0, 1.0), QPointF(1.0, 1.0));
            return curve;
        }

        /**
         * Primary resolved against the inverse surface. A toast turns the
         * background inside out, so its action has to take the accent of the
         * opposite surface family to stay legible.
         */
        QColor inversePrimary()
        {
            const Mode inverse = theme()->isDark() ? Mode::Light : Mode::Dark;
            return ColorScheme(theme()->seed(), inverse).color(Role::Primary);
        }
    } // namespace

    // ------------------------------------------------------------------ Snackbar

    Snackbar::Snackbar(const QString& message, const QString& actionLabel, int msec, QWidget* parent)
        : QWidget(parent)
        , m_message(message)
        , m_actionLabel(actionLabel.toUpper())
        , m_duration(msec > 0 ? msec : Duration::Toast)
    {
        setAttribute(Qt::WA_TranslucentBackground);
        setMouseTracking(true);
        setFocusPolicy(Qt::NoFocus);
        setGraphicsEffect(elevation(3, this));

        m_animation = new QPropertyAnimation(this, "transition", this);
        connect(m_animation, &QPropertyAnimation::finished, this, [this] {
            if (m_dismissing) {
                emit dismissed();
                deleteLater();
            } else {
                QTimer::singleShot(m_duration, this, &Snackbar::dismiss);
            }
        });
    }

    Snackbar::~Snackbar() = default;

    QString Snackbar::message() const
    {
        return m_message;
    }

    qreal Snackbar::transition() const
    {
        return m_transition;
    }

    void Snackbar::setTransition(qreal value)
    {
        value = qBound(0.0, value, 1.0);
        if (qFuzzyCompare(value + 1.0, m_transition + 1.0)) {
            return;
        }
        m_transition = value;
        update();
    }

    QSize Snackbar::sizeHint() const
    {
        const QFontMetrics messageMetrics(theme()->font(TypeRole::BodyMedium));
        const QFontMetrics actionMetrics(theme()->font(TypeRole::LabelLarge));

        int width = 2 * PanelPadding + messageMetrics.horizontalAdvance(m_message);
        if (!m_actionLabel.isEmpty()) {
            width += ActionSpacing + actionMetrics.horizontalAdvance(m_actionLabel);
        }
        width = qBound(MinPanelWidth, width, MaxPanelWidth);

        const int textHeight = qMax(messageMetrics.height(), actionMetrics.height());
        const int panel = qMax(MinPanelHeight, textHeight + 2 * PanelVerticalPadding);
        // The rise is drawn inside the widget, so it is reserved here.
        return {width, panel + Rise};
    }

    QSize Snackbar::minimumSizeHint() const
    {
        return sizeHint();
    }

    void Snackbar::animateIn()
    {
        show();
        m_animation->stop();
        m_animation->setDuration(Duration::Long);
        m_animation->setEasingCurve(emphasizedCurve());
        m_animation->setStartValue(m_transition);
        m_animation->setEndValue(1.0);
        m_animation->start();
    }

    void Snackbar::dismiss()
    {
        if (m_dismissing) {
            return;
        }
        m_dismissing = true;
        m_animation->stop();
        m_animation->setDuration(Duration::Short);
        m_animation->setEasingCurve(QEasingCurve::Linear);
        m_animation->setStartValue(m_transition);
        m_animation->setEndValue(0.0);
        m_animation->start();
    }

    void Snackbar::paintEvent(QPaintEvent* event)
    {
        Q_UNUSED(event);

        const qreal progress = qBound(0.0, m_transition, 1.0);
        const QRect panel(0, qRound(Rise * (1.0 - progress)), width(), height() - Rise);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setOpacity(progress);

        const qreal scale = EnterScale + (1.0 - EnterScale) * progress;
        painter.translate(panel.center());
        painter.scale(scale, scale);
        painter.translate(-panel.center());

        paintSurface(&painter, panel, Shape::ExtraLarge, theme()->color(Role::InverseSurface));

        int right = panel.right() - PanelPadding;
        m_actionRect = QRect();
        if (!m_actionLabel.isEmpty()) {
            const QFont font = theme()->font(TypeRole::LabelLarge);
            const int width = QFontMetrics(font).horizontalAdvance(m_actionLabel);
            m_actionRect = QRect(right - width, panel.top(), width, panel.height());

            QColor color = inversePrimary();
            if (m_actionHovered) {
                color = color.lighter(115);
            }
            painter.setFont(font);
            painter.setPen(color);
            painter.drawText(m_actionRect, Qt::AlignRight | Qt::AlignVCenter, m_actionLabel);
            right = m_actionRect.left() - ActionSpacing;
        }

        const QFont font = theme()->font(TypeRole::BodyMedium);
        const QRect textRect(
            panel.left() + PanelPadding, panel.top(), right - panel.left() - PanelPadding, panel.height());
        painter.setFont(font);
        painter.setPen(theme()->color(Role::InverseOnSurface));
        painter.drawText(textRect,
                         Qt::AlignLeft | Qt::AlignVCenter,
                         QFontMetrics(font).elidedText(m_message, Qt::ElideRight, textRect.width()));
    }

    void Snackbar::mousePressEvent(QMouseEvent* event)
    {
        if (!m_actionLabel.isEmpty() && m_actionRect.contains(event->pos())) {
            emit actionTriggered();
            dismiss();
            event->accept();
            return;
        }
        QWidget::mousePressEvent(event);
    }

    void Snackbar::mouseMoveEvent(QMouseEvent* event)
    {
        const bool hovered = !m_actionLabel.isEmpty() && m_actionRect.contains(event->pos());
        if (hovered != m_actionHovered) {
            m_actionHovered = hovered;
            setCursor(hovered ? Qt::PointingHandCursor : Qt::ArrowCursor);
            update();
        }
        QWidget::mouseMoveEvent(event);
    }

    void Snackbar::leaveEvent(QEvent* event)
    {
        if (m_actionHovered) {
            m_actionHovered = false;
            unsetCursor();
            update();
        }
        QWidget::leaveEvent(event);
    }

    // -------------------------------------------------------------- SnackbarHost

    SnackbarHost::SnackbarHost(QWidget* parent)
        : QWidget(parent)
    {
        setAttribute(Qt::WA_NoSystemBackground);
        setAttribute(Qt::WA_TranslucentBackground);
        setFocusPolicy(Qt::NoFocus);
        hide();

        if (parent) {
            setGeometry(parent->rect());
            parent->installEventFilter(this);
        }
    }

    SnackbarHost::~SnackbarHost() = default;

    SnackbarHost* SnackbarHost::hostFor(QWidget* widget)
    {
        QWidget* window = widget ? widget->window() : nullptr;
        if (!window) {
            return nullptr;
        }
        auto* host = window->findChild<SnackbarHost*>(QString(), Qt::FindDirectChildrenOnly);
        if (!host) {
            host = new SnackbarHost(window);
        }
        return host;
    }

    void SnackbarHost::show(const QString& message, const QString& actionLabel, int msec)
    {
        auto* bar = new Snackbar(message, actionLabel, msec, this);
        connect(bar, &Snackbar::actionTriggered, this, [this, message] { emit actionTriggered(message); });
        connect(bar, &Snackbar::dismissed, this, [this, bar] { remove(bar); });
        m_bars.append(bar);

        // The stack is capped; the oldest toast leaves at once to make room.
        while (m_bars.size() > MaxVisible) {
            Snackbar* oldest = m_bars.takeFirst();
            oldest->hide();
            oldest->deleteLater();
        }

        relayout();
        bar->animateIn();
    }

    void SnackbarHost::clear()
    {
        const auto bars = m_bars;
        for (auto* bar : bars) {
            bar->dismiss();
        }
    }

    int SnackbarHost::count() const
    {
        return m_bars.size();
    }

    void SnackbarHost::resizeEvent(QResizeEvent* event)
    {
        QWidget::resizeEvent(event);
        relayout();
    }

    bool SnackbarHost::eventFilter(QObject* watched, QEvent* event)
    {
        if (watched == parent() && event->type() == QEvent::Resize) {
            if (auto* parentWidget = qobject_cast<QWidget*>(watched)) {
                setGeometry(parentWidget->rect());
            }
        }
        return QWidget::eventFilter(watched, event);
    }

    void SnackbarHost::relayout()
    {
        if (m_bars.isEmpty()) {
            hide();
            return;
        }

        // Newest at the bottom; the stack grows upwards from the window edge.
        QRegion mask;
        int bottom = height() - EdgeMargin;
        for (int i = m_bars.size() - 1; i >= 0; --i) {
            Snackbar* bar = m_bars.at(i);
            const QSize hint = bar->sizeHint();
            const int panelHeight = hint.height() - Rise;
            const QRect geometry((width() - hint.width()) / 2, bottom - panelHeight, hint.width(), hint.height());
            bar->setGeometry(geometry);
            mask += geometry.adjusted(-ShadowMargin, -ShadowMargin, ShadowMargin, ShadowMargin);
            bottom -= panelHeight + StackGap;
        }

        // Only the toasts take the mouse; the rest of the window stays reachable.
        setMask(mask);
        QWidget::show();
        raise();
    }

    void SnackbarHost::remove(Snackbar* bar)
    {
        m_bars.removeOne(bar);
        relayout();
    }

} // namespace Material
