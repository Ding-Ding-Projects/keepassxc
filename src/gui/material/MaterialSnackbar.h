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
#include <QRect>
#include <QString>
#include <QWidget>

class QPropertyAnimation;

namespace Material
{
    /**
     * One toast: a rounded inverseSurface pill with a message and an optional
     * inverseOnSurface action label. Animates in with a 14px rise and a .96
     * scale, then dismisses itself when its timer runs out.
     */
    class Snackbar : public QWidget
    {
        Q_OBJECT

        Q_PROPERTY(qreal transition READ transition WRITE setTransition)

    public:
        Snackbar(const QString& message, const QString& actionLabel, int msec, QWidget* parent = nullptr);
        ~Snackbar() override;

        QString message() const;

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
        void actionTriggered();
        void dismissed();

    protected:
        void paintEvent(QPaintEvent* event) override;
        void mousePressEvent(QMouseEvent* event) override;
        void mouseMoveEvent(QMouseEvent* event) override;
        void leaveEvent(QEvent* event) override;

    private:
        QString m_message;
        QString m_actionLabel;
        QRect m_actionRect;
        QPropertyAnimation* m_animation = nullptr;
        int m_duration = Duration::Toast;
        qreal m_transition = 0.0;
        bool m_actionHovered = false;
        bool m_dismissing = false;
    };

    /**
     * The transparent layer that owns the toasts.
     *
     * One host covers the window, passes mouse events through everywhere except
     * on a toast, and keeps the stack bottom-centred: the newest toast sits at
     * the bottom and the older ones slide up above it. Everything auto-dismisses
     * after Duration::Toast unless a different lifetime is given.
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

        /** Push a toast. @p actionLabel is optional; @p msec is its lifetime. */
        void show(const QString& message, const QString& actionLabel = {}, int msec = Duration::Toast);

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

        QList<Snackbar*> m_bars;
    };

} // namespace Material

#endif // KEEPASSXC_MATERIALSNACKBAR_H
