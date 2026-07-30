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
#include "MaterialIcons.h"

#include <QAccessible>
#include <QEasingCurve>
#include <QFontMetrics>
#include <QGraphicsEffect>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPropertyAnimation>
#include <QRegion>
#include <QTimer>

#include <utility>

namespace Material
{
    namespace
    {
        /** Travel of the rise-in transition, reserved below every toast. */
        constexpr int Rise = 14;
        constexpr qreal EnterScale = 0.96;

        constexpr int StackGap = 10;
        constexpr int EdgeMargin = 20;
        /** The design keeps the two previous toasts plus the new one. */
        constexpr int MaxVisible = 3;

        /** Room the el3 shadow needs outside the toast geometry. */
        constexpr int ShadowMargin = 24;

        constexpr int PanelHorizontalPadding = 16;
        constexpr int PanelVerticalPadding = 14;
        constexpr int GlyphSize = 20;
        constexpr int GlyphGap = 14;
        constexpr int ActionSpacing = 18;
        constexpr int TitleGap = 3;
        constexpr int ProgressGap = 8;
        constexpr int ProgressHeight = 4;
        constexpr int MinPanelWidth = 300;
        constexpr int MaxPanelWidth = 440;
        /** Text column a toast keeps even when its actions are wide. */
        constexpr int MinTextWidth = 140;
        constexpr int MinPanelHeight = 52;
        constexpr int MaxBodyLines = 4;

        constexpr qreal HoverAlpha = 0.12;
        constexpr qreal TrackAlpha = 0.24;

        /**
         * The toastIn curve, cubic-bezier(.38, 1.21, .22, 1).
         *
         * Its own curve rather than the emphasised sheet one: the second control
         * point sits above 1, so a toast overshoots its resting place and settles
         * back instead of easing straight into it.
         */
        QEasingCurve toastCurve()
        {
            QEasingCurve curve(QEasingCurve::BezierSpline);
            curve.addCubicBezierSegment(QPointF(0.38, 1.21), QPointF(0.22, 1.0), QPointF(1.0, 1.0));
            return curve;
        }

        /**
         * Primary resolved against the inverse surface. An information toast
         * turns the background inside out, so its action has to take the accent
         * of the opposite surface family to stay legible.
         */
        QColor inversePrimary()
        {
            const Mode inverse = theme()->isDark() ? Mode::Light : Mode::Dark;
            return ColorScheme(theme()->seed(), inverse).color(Role::Primary);
        }
    } // namespace

    // ------------------------------------------------------------------ severity

    QString severitySymbol(SeverityLevel severity)
    {
        switch (severity) {
        case SeverityLevel::Success:
            return QStringLiteral("check_circle");
        case SeverityLevel::Warning:
            return QStringLiteral("warning");
        case SeverityLevel::Error:
            return QStringLiteral("error");
        case SeverityLevel::Info:
            break;
        }
        return QStringLiteral("info");
    }

    QString severityName(SeverityLevel severity)
    {
        switch (severity) {
        case SeverityLevel::Success:
            return QObject::tr("Success");
        case SeverityLevel::Warning:
            return QObject::tr("Warning");
        case SeverityLevel::Error:
            return QObject::tr("Error");
        case SeverityLevel::Info:
            break;
        }
        return QObject::tr("Information");
    }

    bool severityPersists(SeverityLevel severity)
    {
        return severity == SeverityLevel::Warning || severity == SeverityLevel::Error;
    }

    QColor severityContainer(SeverityLevel severity)
    {
        switch (severity) {
        case SeverityLevel::Success:
            return theme()->color(Role::GreenContainer);
        case SeverityLevel::Warning:
            return theme()->color(Role::AmberContainer);
        case SeverityLevel::Error:
            return theme()->color(Role::ErrorContainer);
        case SeverityLevel::Info:
            break;
        }
        return theme()->color(Role::InverseSurface);
    }

    QColor severityOnContainer(SeverityLevel severity)
    {
        switch (severity) {
        case SeverityLevel::Success:
            return theme()->color(Role::OnGreenContainer);
        case SeverityLevel::Warning:
            return theme()->color(Role::OnAmberContainer);
        case SeverityLevel::Error:
            return theme()->color(Role::OnErrorContainer);
        case SeverityLevel::Info:
            break;
        }
        return theme()->color(Role::InverseOnSurface);
    }

    QColor severityAccent(SeverityLevel severity)
    {
        switch (severity) {
        case SeverityLevel::Success:
            return theme()->color(Role::Green);
        case SeverityLevel::Warning:
            return theme()->color(Role::Amber);
        case SeverityLevel::Error:
            return theme()->color(Role::Error);
        case SeverityLevel::Info:
            break;
        }
        return inversePrimary();
    }

    // -------------------------------------------------------- NotificationAction

    NotificationAction::NotificationAction(QString actionLabel, std::function<void()> actionHandler, QObject* guard)
        : label(std::move(actionLabel))
        , handler(std::move(actionHandler))
        , context(guard)
        , guarded(guard != nullptr)
    {
    }

    bool NotificationAction::isValid() const
    {
        return !label.isEmpty() && (!guarded || !context.isNull());
    }

    // ------------------------------------------------------------------ Snackbar

    Snackbar::Snackbar(SeverityLevel severity,
                       const QString& title,
                       const QString& message,
                       const QList<NotificationAction>& actions,
                       int msec,
                       QWidget* parent)
        : QWidget(parent)
        , m_severity(severity)
        , m_title(title)
        , m_message(message)
        , m_actions(actions)
        , m_duration(msec)
    {
        setAttribute(Qt::WA_TranslucentBackground);
        setMouseTracking(true);
        setFocusPolicy(Qt::StrongFocus);
        setGraphicsEffect(elevation(3, this));
        updateAccessibleText();

        m_lifetime = new QTimer(this);
        m_lifetime->setSingleShot(true);
        connect(m_lifetime, &QTimer::timeout, this, &Snackbar::dismiss);

        m_animation = new QPropertyAnimation(this, "transition", this);
        connect(m_animation, &QPropertyAnimation::finished, this, [this] {
            if (m_dismissing) {
                emit dismissed();
                deleteLater();
            } else {
                resumeTimer();
            }
        });
    }

    Snackbar::Snackbar(const QString& message, const QString& actionLabel, int msec, QWidget* parent)
        // A toast always carries a trailing action. With none named it reads
        // "Dismiss" and its only effect is to take the toast away, which is what
        // an action with no handler already does.
        : Snackbar(
              SeverityLevel::Info,
              QString(),
              message,
              QList<NotificationAction>{NotificationAction(actionLabel.isEmpty() ? tr("Dismiss") : actionLabel, {})},
              msec > 0 ? msec : ToastLifetime,
              parent)
    {
    }

    Snackbar::~Snackbar() = default;

    SeverityLevel Snackbar::severity() const
    {
        return m_severity;
    }

    QString Snackbar::title() const
    {
        return m_title;
    }

    QString Snackbar::message() const
    {
        return m_message;
    }

    void Snackbar::setMessage(const QString& message)
    {
        if (message == m_message) {
            return;
        }
        m_message = message;
        updateAccessibleText();
        updateGeometry();
        update();
    }

    int Snackbar::progress() const
    {
        return m_progress;
    }

    void Snackbar::setProgress(int percent)
    {
        const int value = percent < 0 ? NoProgress : qMin(percent, 100);
        if (value == m_progress) {
            return;
        }
        const bool wasVisible = m_progress != NoProgress;
        m_progress = value;
        if (wasVisible != (m_progress != NoProgress)) {
            updateGeometry();
        }
        update();
    }

    bool Snackbar::autoDismisses() const
    {
        return m_duration > 0;
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
        const QFontMetrics titleMetrics(theme()->font(TypeRole::LabelLarge));
        const QFontMetrics bodyMetrics(theme()->font(TypeRole::BodyMedium));

        int chrome = 2 * PanelHorizontalPadding + GlyphSize + GlyphGap;
        for (const auto& action : m_actions) {
            chrome += ActionSpacing + titleMetrics.horizontalAdvance(action.label);
        }

        int natural = bodyMetrics.horizontalAdvance(m_message);
        if (!m_title.isEmpty()) {
            natural = qMax(natural, titleMetrics.horizontalAdvance(m_title));
        }

        // Wide actions push the panel past its usual ceiling rather than
        // squeezing the message down to nothing.
        const int ceiling = qMax(MaxPanelWidth, chrome + MinTextWidth);
        const int width = qBound(MinPanelWidth, chrome + natural, ceiling);
        const int textWidth = qMax(MinTextWidth, width - chrome);

        int text = 0;
        if (!m_title.isEmpty()) {
            text += titleMetrics.height() + TitleGap;
        }
        const QRect wrapped =
            bodyMetrics.boundingRect(QRect(0, 0, textWidth, MaxBodyLines * bodyMetrics.lineSpacing()),
                                     Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop,
                                     m_message);
        text += qBound(bodyMetrics.height(), wrapped.height(), MaxBodyLines * bodyMetrics.lineSpacing());

        int panel = qMax(MinPanelHeight, text + 2 * PanelVerticalPadding);
        if (m_progress != NoProgress) {
            panel += ProgressGap + ProgressHeight;
        }

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
        m_animation->setEasingCurve(toastCurve());
        m_animation->setStartValue(m_transition);
        m_animation->setEndValue(1.0);
        m_animation->start();

#ifndef QT_NO_ACCESSIBILITY
        // Announced as an alert so a screen reader reads it without the user
        // having to chase the focus onto the toast.
        QAccessibleEvent alert(this, QAccessible::Alert);
        QAccessible::updateAccessibility(&alert);
#endif
    }

    void Snackbar::dismiss()
    {
        if (m_dismissing) {
            return;
        }
        m_dismissing = true;
        m_lifetime->stop();
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
        layoutPanel(panel);

        // Every toast is painted on the inverse surface, whatever it says; the
        // severity is carried by the leading glyph alone.
        const QColor container = theme()->color(Role::InverseSurface);
        const QColor content = theme()->color(Role::InverseOnSurface);
        const QColor accent = inversePrimary();

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setOpacity(progress);

        const qreal scale = EnterScale + (1.0 - EnterScale) * progress;
        painter.translate(panel.center());
        painter.scale(scale, scale);
        painter.translate(-panel.center());

        paintSurface(&painter, panel, Shape::Row, container);
        if (hasFocus()) {
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(accent, 2));
            painter.drawPath(roundedPath(QRectF(panel).adjusted(1.5, 1.5, -1.5, -1.5), Shape::Row));
        }

        painter.drawPixmap(m_glyphRect,
                           Icons::pixmap(severitySymbol(m_severity), GlyphSize, severityAccent(m_severity)));

        const QFont titleFont = theme()->font(TypeRole::LabelLarge);
        const QFont bodyFont = theme()->font(TypeRole::BodyMedium);

        QRect textRect = m_textRect;
        painter.setClipRect(textRect);
        painter.setPen(content);
        if (!m_title.isEmpty()) {
            const int lineHeight = QFontMetrics(titleFont).height();
            painter.setFont(titleFont);
            painter.drawText(QRect(textRect.left(), textRect.top(), textRect.width(), lineHeight),
                             Qt::AlignLeft | Qt::AlignVCenter,
                             QFontMetrics(titleFont).elidedText(m_title, Qt::ElideRight, textRect.width()));
            textRect.setTop(textRect.top() + lineHeight + TitleGap);
        }
        painter.setFont(bodyFont);
        painter.drawText(textRect,
                         Qt::TextWordWrap | Qt::AlignLeft | (m_title.isEmpty() ? Qt::AlignVCenter : Qt::AlignTop),
                         m_message);
        painter.setClipping(false);

        painter.setFont(titleFont);
        for (int i = 0; i < m_actionRects.size(); ++i) {
            const QRect rect = m_actionRects.at(i);
            if (i == m_hoveredAction) {
                paintStateLayer(&painter, rect.adjusted(-8, 0, 8, 0), Shape::Small, accent, HoverAlpha);
            }
            painter.setPen(accent);
            painter.drawText(rect, Qt::AlignCenter, m_actions.at(i).label);
        }

        if (m_progress != NoProgress) {
            QColor track = content;
            track.setAlphaF(TrackAlpha);
            paintSurface(&painter, m_progressRect, ProgressHeight / 2, track);
            QRect fill = m_progressRect;
            fill.setWidth(m_progressRect.width() * m_progress / 100);
            if (fill.width() > 0) {
                paintSurface(&painter, fill, ProgressHeight / 2, severityAccent(m_severity));
            }
        }
    }

    void Snackbar::mousePressEvent(QMouseEvent* event)
    {
        const QPoint pos = event->pos();
        for (int i = 0; i < m_actionRects.size(); ++i) {
            if (m_actionRects.at(i).adjusted(-8, 0, 8, 0).contains(pos)) {
                invoke(i);
                event->accept();
                return;
            }
        }
        setFocus(Qt::MouseFocusReason);
        QWidget::mousePressEvent(event);
    }

    void Snackbar::mouseMoveEvent(QMouseEvent* event)
    {
        const QPoint pos = event->pos();
        int hovered = -1;
        for (int i = 0; i < m_actionRects.size(); ++i) {
            if (m_actionRects.at(i).adjusted(-8, 0, 8, 0).contains(pos)) {
                hovered = i;
                break;
            }
        }

        if (hovered != m_hoveredAction) {
            m_hoveredAction = hovered;
            setCursor(hovered >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
            update();
        }
        QWidget::mouseMoveEvent(event);
    }

    void Snackbar::keyPressEvent(QKeyEvent* event)
    {
        switch (event->key()) {
        case Qt::Key_Escape:
        case Qt::Key_Delete:
        case Qt::Key_Backspace:
            dismiss();
            event->accept();
            return;
        case Qt::Key_Return:
        case Qt::Key_Enter:
        case Qt::Key_Space:
            if (!m_actions.isEmpty()) {
                invoke(0);
            } else {
                dismiss();
            }
            event->accept();
            return;
        default:
            break;
        }
        QWidget::keyPressEvent(event);
    }

    void Snackbar::enterEvent(QEnterEvent* event)
    {
        holdTimer();
        QWidget::enterEvent(event);
    }

    void Snackbar::leaveEvent(QEvent* event)
    {
        if (m_hoveredAction >= 0) {
            m_hoveredAction = -1;
            unsetCursor();
            update();
        }
        if (!hasFocus()) {
            resumeTimer();
        }
        QWidget::leaveEvent(event);
    }

    void Snackbar::focusInEvent(QFocusEvent* event)
    {
        holdTimer();
        update();
        QWidget::focusInEvent(event);
    }

    void Snackbar::focusOutEvent(QFocusEvent* event)
    {
        resumeTimer();
        update();
        QWidget::focusOutEvent(event);
    }

    void Snackbar::layoutPanel(const QRect& panel) const
    {
        QRect content = panel.adjusted(
            PanelHorizontalPadding, PanelVerticalPadding, -PanelHorizontalPadding, -PanelVerticalPadding);
        if (m_progress != NoProgress) {
            content.setBottom(content.bottom() - ProgressGap - ProgressHeight);
            m_progressRect = QRect(
                panel.left() + PanelHorizontalPadding, content.bottom() + ProgressGap, content.width(), ProgressHeight);
        } else {
            m_progressRect = QRect();
        }

        m_glyphRect = QRect(content.left(), content.center().y() - GlyphSize / 2, GlyphSize, GlyphSize);

        const int actionHeight = qMax(content.height(), Layout::ChipHeight);
        const int actionTop = content.center().y() - actionHeight / 2;
        const QFontMetrics actionMetrics(theme()->font(TypeRole::LabelLarge));

        m_actionRects.clear();
        int right = content.right() + 1;
        for (int i = m_actions.size() - 1; i >= 0; --i) {
            const int labelWidth = actionMetrics.horizontalAdvance(m_actions.at(i).label);
            m_actionRects.prepend(QRect(right - labelWidth, actionTop, labelWidth, actionHeight));
            right -= labelWidth + ActionSpacing;
        }

        const int textLeft = m_glyphRect.right() + GlyphGap;
        const int textRight =
            m_actionRects.isEmpty() ? content.right() + 1 : m_actionRects.first().left() - ActionSpacing;
        m_textRect = QRect(textLeft, content.top(), qMax(1, textRight - textLeft), content.height());
    }

    void Snackbar::invoke(int index)
    {
        if (index < 0 || index >= m_actions.size()) {
            return;
        }
        const NotificationAction action = m_actions.at(index);
        if (action.isValid() && action.handler) {
            action.handler();
        }
        emit actionTriggered(index);
        dismiss();
    }

    void Snackbar::holdTimer()
    {
        m_lifetime->stop();
    }

    void Snackbar::resumeTimer()
    {
        if (m_dismissing || m_duration <= 0 || underMouse() || hasFocus()) {
            return;
        }
        m_lifetime->start(m_duration);
    }

    void Snackbar::updateAccessibleText()
    {
        const QString headline = m_title.isEmpty() ? m_message : m_title;
        setAccessibleName(QStringLiteral("%1: %2").arg(severityName(m_severity), headline));
        setAccessibleDescription(m_message);
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
        // Searched recursively: the shell parents its host to the destination
        // stack so a toast never covers the rail or the app bar, and a direct
        // child lookup would miss it and build a second host over the window.
        auto* host = window->findChild<SnackbarHost*>();
        if (!host) {
            host = new SnackbarHost(window);
        }
        return host;
    }

    Snackbar* SnackbarHost::show(SeverityLevel severity,
                                 const QString& title,
                                 const QString& message,
                                 const QList<NotificationAction>& actions,
                                 int msec)
    {
        if (msec < 0) {
            msec = severityPersists(severity) ? 0 : ToastLifetime;
        }

        // Every toast carries a trailing action. With none offered it reads
        // "Dismiss" and only takes the toast away.
        QList<NotificationAction> offered = actions;
        if (offered.isEmpty()) {
            offered.append(NotificationAction(tr("Dismiss"), {}));
        }

        auto* bar = new Snackbar(severity, title, message, offered, msec, this);
        connect(bar, &Snackbar::actionTriggered, this, [this, message](int) { emit actionTriggered(message); });
        connect(bar, &Snackbar::dismissed, this, [this, bar] { remove(bar); });
        m_bars.append(bar);

        enforceCap();
        relayout();
        bar->animateIn();
        return bar;
    }

    void SnackbarHost::show(const QString& message, const QString& actionLabel, int msec)
    {
        QList<NotificationAction> actions;
        if (!actionLabel.isEmpty()) {
            actions.append(NotificationAction(actionLabel, {}));
        }
        show(SeverityLevel::Info, QString(), message, actions, msec);
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

        // Newest at the bottom; the stack grows upwards from the bottom right
        // corner, every toast flush against the same right edge.
        QRegion mask;
        int bottom = height() - EdgeMargin;
        for (int i = m_bars.size() - 1; i >= 0; --i) {
            Snackbar* bar = m_bars.at(i);
            const QSize hint = bar->sizeHint();
            const int panelHeight = hint.height() - Rise;
            const QRect geometry(
                width() - hint.width() - EdgeMargin, bottom - panelHeight, hint.width(), hint.height());
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

    void SnackbarHost::enforceCap()
    {
        while (m_bars.size() > MaxVisible) {
            // A warning or an error is the user's to close, so the oldest toast
            // that would have left on its own goes first. The newest is never
            // the victim - it would vanish before it was read.
            int victim = 0;
            for (int i = 0; i < m_bars.size() - 1; ++i) {
                if (m_bars.at(i)->autoDismisses()) {
                    victim = i;
                    break;
                }
            }
            Snackbar* dropped = m_bars.takeAt(victim);
            dropped->hide();
            dropped->deleteLater();
        }
    }

} // namespace Material
