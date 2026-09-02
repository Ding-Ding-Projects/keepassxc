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

#include "MaterialTitleBar.h"

#include "MaterialIcons.h"
#include "MaterialTheme.h"

#include <QAbstractButton>
#include <QApplication>
#include <QFocusEvent>
#include <QMouseEvent>
#include <QPainter>

namespace Material
{
    namespace
    {
        constexpr int MarkSize = 20;
        constexpr int LeftPadding = 14;
        constexpr int Gap = 12;
        constexpr int TitleGap = 8;
        constexpr int GlyphSize = 18;
        constexpr int RestoreGlyphSize = 16;

        /**
         * One caption button: 46 x 44, transparent until hovered, a container
         * tint on hover and, for close, the desktop's own red so the one
         * destructive control reads the way every Windows user expects.
         */
        class CaptionButton : public QAbstractButton
        {
        public:
            CaptionButton(const QString& symbol, const QString& name, bool destructive, QWidget* parent)
                : QAbstractButton(parent)
                , m_symbol(symbol)
                , m_destructive(destructive)
            {
                setFixedSize(TitleBar::ButtonWidth, TitleBar::Height);
                setAccessibleName(name);
                setToolTip(name);
                setFocusPolicy(Qt::TabFocus);
                setAttribute(Qt::WA_Hover);
                setCursor(Qt::ArrowCursor);
            }

            void setSymbol(const QString& symbol)
            {
                m_symbol = symbol;
                update();
            }

        protected:
            void paintEvent(QPaintEvent*) override
            {
                QPainter painter(this);
                painter.setRenderHint(QPainter::Antialiasing);
                const bool hot = underMouse() || isDown() || m_keyboardFocus;
                QColor ink = theme()->color(Role::OnSurfaceVariant);
                if (hot) {
                    if (m_destructive) {
                        painter.fillRect(rect(), QColor(0xe8, 0x11, 0x23));
                        ink = Qt::white;
                    } else {
                        painter.fillRect(rect(), theme()->color(Role::SurfaceContainer));
                        ink = theme()->color(Role::OnSurface);
                    }
                }
                if (m_keyboardFocus) {
                    painter.setPen(QPen(theme()->color(Role::Primary), 2));
                    painter.setBrush(Qt::NoBrush);
                    painter.drawRect(rect().adjusted(2, 2, -2, -2));
                }
                const int size = m_symbol == QLatin1String("crop_square") || m_symbol == QLatin1String("filter_none")
                                     ? RestoreGlyphSize
                                     : GlyphSize;
                const QPixmap glyph = Icons::pixmap(m_symbol, size, ink);
                painter.drawPixmap((width() - size) / 2, (height() - size) / 2, glyph);
            }

            // The ring is for the keyboard: initial focus and pointer focus
            // would otherwise paint a ring on Minimise before anyone pressed Tab.
            void focusInEvent(QFocusEvent* event) override
            {
                m_keyboardFocus = event->reason() == Qt::TabFocusReason || event->reason() == Qt::BacktabFocusReason;
                QAbstractButton::focusInEvent(event);
                update();
            }

            void focusOutEvent(QFocusEvent* event) override
            {
                m_keyboardFocus = false;
                QAbstractButton::focusOutEvent(event);
                update();
            }

        private:
            QString m_symbol;
            bool m_destructive;
            bool m_keyboardFocus = false;
        };
    } // namespace

    TitleBar::TitleBar(QWidget* parent)
        : QWidget(parent)
        , m_title(QApplication::applicationDisplayName().isEmpty() ? QStringLiteral("KeePassXC")
                                                                    : QApplication::applicationDisplayName())
    {
        setObjectName(QStringLiteral("materialTitleBar"));
        setFixedHeight(Height);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setAccessibleName(tr("Window title bar"));

        auto* minimize = new CaptionButton(QStringLiteral("remove"), tr("Minimise"), false, this);
        minimize->setObjectName(QStringLiteral("titleBarMinimize"));
        connect(minimize, &QAbstractButton::clicked, this, &TitleBar::minimizeRequested);
        m_minimize = minimize;

        auto* maximize = new CaptionButton(QStringLiteral("crop_square"), tr("Maximise"), false, this);
        maximize->setObjectName(QStringLiteral("titleBarMaximize"));
        connect(maximize, &QAbstractButton::clicked, this, &TitleBar::maximizeRequested);
        m_maximize = maximize;

        auto* close = new CaptionButton(QStringLiteral("close"), tr("Close"), true, this);
        close->setObjectName(QStringLiteral("titleBarClose"));
        connect(close, &QAbstractButton::clicked, this, &TitleBar::closeRequested);
        m_close = close;

        connect(theme(), &Theme::changed, this, qOverload<>(&QWidget::update));
        relayout();
    }

    TitleBar::~TitleBar() = default;

    QString TitleBar::title() const
    {
        return m_title;
    }

    void TitleBar::setTitle(const QString& title)
    {
        m_title = title;
        update();
    }

    QString TitleBar::subtitle() const
    {
        return m_subtitle;
    }

    void TitleBar::setSubtitle(const QString& subtitle)
    {
        m_subtitle = subtitle;
        setAccessibleDescription(subtitle.isEmpty() ? m_title : tr("%1, %2").arg(m_title, subtitle));
        update();
    }

    void TitleBar::setMaximized(bool maximized)
    {
        if (m_maximized == maximized) {
            return;
        }
        m_maximized = maximized;
        auto* button = static_cast<CaptionButton*>(m_maximize);
        button->setSymbol(maximized ? QStringLiteral("filter_none") : QStringLiteral("crop_square"));
        button->setAccessibleName(maximized ? tr("Restore") : tr("Maximise"));
        button->setToolTip(button->accessibleName());
    }

    bool TitleBar::isMaximized() const
    {
        return m_maximized;
    }

    bool TitleBar::isCaptionArea(const QPoint& pos) const
    {
        if (!rect().contains(pos)) {
            return false;
        }
        for (QAbstractButton* button : {m_minimize, m_maximize, m_close}) {
            if (button->geometry().contains(pos)) {
                return false;
            }
        }
        return true;
    }

    QAbstractButton* TitleBar::minimizeButton() const
    {
        return m_minimize;
    }

    QAbstractButton* TitleBar::maximizeButton() const
    {
        return m_maximize;
    }

    QAbstractButton* TitleBar::closeButton() const
    {
        return m_close;
    }

    QSize TitleBar::sizeHint() const
    {
        return {LeftPadding + MarkSize + Gap + 200 + 3 * ButtonWidth, Height};
    }

    QSize TitleBar::minimumSizeHint() const
    {
        return {LeftPadding + MarkSize + Gap + 3 * ButtonWidth, Height};
    }

    void TitleBar::relayout()
    {
        int x = width() - ButtonWidth;
        m_close->move(x, 0);
        x -= ButtonWidth;
        m_maximize->move(x, 0);
        x -= ButtonWidth;
        m_minimize->move(x, 0);
    }

    void TitleBar::resizeEvent(QResizeEvent* event)
    {
        QWidget::resizeEvent(event);
        relayout();
    }

    void TitleBar::paintEvent(QPaintEvent*)
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.fillRect(rect(), theme()->color(Role::SurfaceContainerLow));
        painter.setPen(theme()->color(Role::OutlineVariant));
        painter.drawLine(0, height() - 1, width(), height() - 1);

        const QPixmap mark = Icons::pixmap(QStringLiteral("shield_lock"), MarkSize, theme()->color(Role::Primary));
        painter.drawPixmap(LeftPadding, (height() - MarkSize) / 2, mark);

        const QFont titleFont = theme()->font(TypeRole::LabelLarge);
        const QFont subtitleFont = theme()->font(TypeRole::LabelMedium);
        const QFontMetrics titleMetrics(titleFont);
        const QFontMetrics subtitleMetrics(subtitleFont);
        const int available = m_minimize->x() - (LeftPadding + MarkSize + Gap) - Gap;
        int x = LeftPadding + MarkSize + Gap;
        const int baseline = (height() + titleMetrics.ascent() - titleMetrics.descent()) / 2;

        painter.setFont(titleFont);
        painter.setPen(theme()->color(Role::OnSurface));
        const QString title = titleMetrics.elidedText(m_title, Qt::ElideRight, available);
        painter.drawText(QPoint(x, baseline), title);
        x += titleMetrics.horizontalAdvance(title) + TitleGap;

        if (!m_subtitle.isEmpty()) {
            const int remaining = m_minimize->x() - x - Gap;
            if (remaining > subtitleMetrics.averageCharWidth() * 4) {
                painter.setFont(subtitleFont);
                painter.setPen(theme()->color(Role::OnSurfaceVariant));
                painter.drawText(QPoint(x, baseline), subtitleMetrics.elidedText(m_subtitle, Qt::ElideRight, remaining));
            }
        }
    }

    void TitleBar::mouseDoubleClickEvent(QMouseEvent* event)
    {
        if (event->button() == Qt::LeftButton && isCaptionArea(event->pos())) {
            emit maximizeRequested();
            event->accept();
            return;
        }
        QWidget::mouseDoubleClickEvent(event);
    }

    void TitleBar::mousePressEvent(QMouseEvent* event)
    {
        if (event->button() == Qt::RightButton && isCaptionArea(event->pos())) {
            emit systemMenuRequested(event->globalPosition().toPoint());
            event->accept();
            return;
        }
        QWidget::mousePressEvent(event);
    }
} // namespace Material
