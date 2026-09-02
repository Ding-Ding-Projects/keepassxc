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

#ifndef KEEPASSXC_MATERIALTITLEBAR_H
#define KEEPASSXC_MATERIALTITLEBAR_H

#include <QWidget>

class QAbstractButton;
class QLabel;

namespace Material
{
    /**
     * The window's caption, drawn by the application instead of the desktop.
     *
     * A 44 px strip at the very top of the shell in the reference anatomy: the
     * shield mark, the product name with the database name beside it, and the
     * three caption buttons (minimise, maximise or restore, close) on the
     * right. It paints from the theme, so a dark application has a dark
     * caption without asking the desktop for one.
     *
     * The bar only asks: it emits requests and the window decides. On Windows
     * WindowChrome::installFrameless() removes the native caption and tells
     * the desktop window manager that this strip, minus its buttons, is the
     * caption, so dragging, double-click to maximise, snap layouts and the
     * system menu keep working exactly as they do on a native title bar.
     */
    class TitleBar : public QWidget
    {
        Q_OBJECT

    public:
        static constexpr int Height = 44;
        static constexpr int ButtonWidth = 46;

        explicit TitleBar(QWidget* parent = nullptr);
        ~TitleBar() override;

        /** The product name; defaults to the application display name. */
        QString title() const;
        void setTitle(const QString& title);
        /** The open database's name, or empty when nothing is open. */
        QString subtitle() const;
        void setSubtitle(const QString& subtitle);

        /** Swap the maximise glyph for restore when the window is maximised. */
        void setMaximized(bool maximized);
        bool isMaximized() const;

        /**
         * True when @p pos (in this widget's coordinates) is caption: inside
         * the bar and not over one of its buttons. The window manager drags
         * the window from caption and clicks buttons everywhere else.
         */
        bool isCaptionArea(const QPoint& pos) const;

        QAbstractButton* minimizeButton() const;
        QAbstractButton* maximizeButton() const;
        QAbstractButton* closeButton() const;

        QSize sizeHint() const override;
        QSize minimumSizeHint() const override;

    signals:
        void minimizeRequested();
        void maximizeRequested();
        void closeRequested();
        /** The system menu, from a right-click on the caption. */
        void systemMenuRequested(const QPoint& globalPos);

    protected:
        void paintEvent(QPaintEvent* event) override;
        void mouseDoubleClickEvent(QMouseEvent* event) override;
        void mousePressEvent(QMouseEvent* event) override;
        void resizeEvent(QResizeEvent* event) override;

    private:
        void relayout();

        QString m_title;
        QString m_subtitle;
        bool m_maximized = false;
        QAbstractButton* m_minimize = nullptr;
        QAbstractButton* m_maximize = nullptr;
        QAbstractButton* m_close = nullptr;
    };
} // namespace Material

#endif // KEEPASSXC_MATERIALTITLEBAR_H
