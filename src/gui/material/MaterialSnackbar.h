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

#ifndef KEEPASSXC_MATERIALSNACKBAR_H
#define KEEPASSXC_MATERIALSNACKBAR_H

#include "MaterialTheme.h"

#include <QList>
#include <QPointer>
#include <QRect>
#include <QString>
#include <QWidget>

#include <functional>

class QPropertyAnimation;
class QTimer;

namespace Material
{
    /**
     * How long a toast stays on screen before it removes itself.
     *
     * The toast timer is the design's own (a toast is dropped 4200ms after it
     * arrives) rather than one of the shared motion steps, which describe
     * transitions rather than reading time.
     */
    constexpr int ToastLifetime = 4200;

    /**
     * How loud a notification is.
     *
     * The level picks the colour pair the toast is painted in and decides
     * whether it leaves on its own: information and success fade out, warnings
     * and errors wait for the user.
     */
    enum class SeverityLevel
    {
        Info,
        Success,
        Warning,
        Error
    };

    /** The Material Symbols glyph that stands for @p severity. */
    QString severitySymbol(SeverityLevel severity);

    /** Translated name of @p severity, used by screen readers and the filter. */
    QString severityName(SeverityLevel severity);

    /** Whether @p severity waits for the user instead of timing out. */
    bool severityPersists(SeverityLevel severity);

    /** Surface a notification of @p severity is painted on, from the status roles. */
    QColor severityContainer(SeverityLevel severity);
    /** Text and glyph colour resolved against severityContainer(). */
    QColor severityOnContainer(SeverityLevel severity);
    /** Accent for the severity: action labels and the progress fill. */
    QColor severityAccent(SeverityLevel severity);

    /**
     * One offer attached to a notification: retry, undo, open, details.
     *
     * @a context is an optional lifetime guard. An action built with one goes
     * stale once that object dies, so an "Open entry" offer disappears from the
     * notification centre along with the entry rather than firing into nothing.
     */
    struct NotificationAction
    {
        QString label;
        std::function<void()> handler;
        QPointer<QObject> context;
        bool guarded = false;

        NotificationAction() = default;
        NotificationAction(QString actionLabel, std::function<void()> actionHandler, QObject* guard = nullptr);

        /** Whether the action still has a handler and a live guard. */
        bool isValid() const;
    };

    /**
     * One toast: a rounded-16 pill on the inverse surface carrying a severity
     * glyph, an optional title, a message and its trailing action labels.
     *
     * Animates in with a 14px rise and a .96 scale. Info and success toasts
     * dismiss themselves when their timer runs out - hovering or focusing them
     * holds the timer - while warnings and errors stay until dismissed. There is
     * no close affordance: the trailing action, which reads "Dismiss" when the
     * caller names nothing else, is what takes a toast away by hand.
     *
     * The bar is focusable and announced as an alert; Enter runs the first
     * action, Escape dismisses.
     */
    class Snackbar : public QWidget
    {
        Q_OBJECT

        Q_PROPERTY(qreal transition READ transition WRITE setTransition)

    public:
        /** Progress value that hides the readout entirely. */
        static constexpr int NoProgress = -1;

        /** @p msec of zero or less makes the toast wait for the user. */
        Snackbar(SeverityLevel severity,
                 const QString& title,
                 const QString& message,
                 const QList<NotificationAction>& actions,
                 int msec,
                 QWidget* parent = nullptr);
        Snackbar(const QString& message, const QString& actionLabel, int msec, QWidget* parent = nullptr);
        ~Snackbar() override;

        SeverityLevel severity() const;

        QString title() const;
        QString message() const;
        /** Replace the body copy in place, for a progress readout that ticks. */
        void setMessage(const QString& message);

        /** 0..100 draws the determinate bar; NoProgress hides it. */
        int progress() const;
        void setProgress(int percent);

        bool autoDismisses() const;

        qreal transition() const;
        void setTransition(qreal value);

        QSize sizeHint() const override;
        QSize minimumSizeHint() const override;

        /** Start the rise-in transition and the auto-dismiss timer. */
        void animateIn();

    public slots:
        /** Play the transition backwards, then emit dismissed() and delete. */
        void dismiss();

    signals:
        /** The action at @p index was chosen; its handler has already run. */
        void actionTriggered(int index);
        void dismissed();

    protected:
        void paintEvent(QPaintEvent* event) override;
        void mousePressEvent(QMouseEvent* event) override;
        void mouseMoveEvent(QMouseEvent* event) override;
        void keyPressEvent(QKeyEvent* event) override;
        void enterEvent(QEnterEvent* event) override;
        void leaveEvent(QEvent* event) override;
        void focusInEvent(QFocusEvent* event) override;
        void focusOutEvent(QFocusEvent* event) override;

    private:
        /** Recompute the panel geometry the painter and the hit tests share. */
        void layoutPanel(const QRect& panel) const;
        void invoke(int index);
        void holdTimer();
        void resumeTimer();
        void updateAccessibleText();

        SeverityLevel m_severity = SeverityLevel::Info;
        QString m_title;
        QString m_message;
        QList<NotificationAction> m_actions;
        QPropertyAnimation* m_animation = nullptr;
        QTimer* m_lifetime = nullptr;
        int m_duration = ToastLifetime;
        int m_progress = NoProgress;
        qreal m_transition = 0.0;
        int m_hoveredAction = -1;
        bool m_dismissing = false;

        // Filled by layoutPanel() on every paint, read by the hit tests.
        mutable QRect m_glyphRect;
        mutable QRect m_textRect;
        mutable QRect m_progressRect;
        mutable QList<QRect> m_actionRects;
    };

    /**
     * The transparent layer that owns the toasts.
     *
     * One host covers the window, passes mouse events through everywhere except
     * on a toast, and keeps the stack in the bottom right corner: the newest
     * toast sits at the bottom and the older ones slide up above it, never
     * overlapping.
     */
    class SnackbarHost : public QWidget
    {
        Q_OBJECT

    public:
        explicit SnackbarHost(QWidget* parent);
        ~SnackbarHost() override;

        /** The host covering @p widget's window, created on first use. */
        static SnackbarHost* hostFor(QWidget* widget);

        using QWidget::show;

        /**
         * Push a toast. A negative @p msec takes the lifetime from @p severity:
         * information and success time out, warnings and errors persist.
         */
        Snackbar* show(SeverityLevel severity,
                       const QString& title,
                       const QString& message,
                       const QList<NotificationAction>& actions = {},
                       int msec = -1);

        /** Push a plain information toast with a single unnamed action. */
        void show(const QString& message, const QString& actionLabel = {}, int msec = ToastLifetime);

        /** Dismiss every visible toast at once. */
        void clear();

        int count() const;

    signals:
        /** The action label of the toast carrying @p message was clicked. */
        void actionTriggered(const QString& message);

    protected:
        void resizeEvent(QResizeEvent* event) override;
        bool eventFilter(QObject* watched, QEvent* event) override;

    private:
        void relayout();
        void remove(Snackbar* bar);
        /** Drop the oldest toast once the stack is full, sparing persistent ones. */
        void enforceCap();

        QList<Snackbar*> m_bars;
    };

} // namespace Material

#endif // KEEPASSXC_MATERIALSNACKBAR_H
