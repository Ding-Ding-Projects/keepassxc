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

#include "MaterialOverlay.h"
#include "MaterialMotion.h"

#include "MaterialElevation.h"
#include "MaterialTheme.h"

#include <QEasingCurve>
#include <QEvent>

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>

#include <QResizeEvent>
#include <QShowEvent>

namespace Material
{
    namespace
    {
        constexpr qreal ScrimAlpha = 0.32;
        // Breathing room kept between the sheet and the window edge.
        constexpr int EdgeMargin = 32;
        constexpr int MinSheetWidth = 240;
        constexpr int MinSheetHeight = 160;

    } // namespace

    Overlay::Overlay(QWidget* parent)
        : QWidget(parent)
    {
        setFocusPolicy(Qt::StrongFocus);
        setAttribute(Qt::WA_NoMousePropagation);
        hide();

        m_animation = new MotionTransition(this);
        connect(m_animation, &MotionTransition::valueChanged, this, &Overlay::setTransition);
        connect(m_animation, &MotionTransition::settled, this, [this] {
            if (!m_open) {
                hide();
                emit closed();
            }
        });

        if (parent) {
            parent->installEventFilter(this);
            setGeometry(parent->rect());
        }
    }

    Overlay::~Overlay() = default;

    void Overlay::setSheetWidget(QWidget* sheet)
    {
        if (m_sheet == sheet) {
            return;
        }
        if (m_sheet) {
            m_sheet->removeEventFilter(this);
            delete m_sheet;
        }
        m_sheet = sheet;
        if (!m_sheet) {
            return;
        }

        m_sheet->setParent(this);
        m_sheet->installEventFilter(this);
        m_sheet->show();
        centreSheet();
    }

    QWidget* Overlay::sheetWidget() const
    {
        return m_sheet;
    }

    int Overlay::sheetWidth() const
    {
        return m_sheetWidth;
    }

    void Overlay::setSheetWidth(int width)
    {
        if (width == m_sheetWidth) {
            return;
        }
        m_sheetWidth = width;
        centreSheet();
    }

    int Overlay::sheetTopMargin() const
    {
        return m_sheetTopMargin;
    }

    void Overlay::setSheetTopMargin(int margin)
    {
        if (margin == m_sheetTopMargin) {
            return;
        }
        m_sheetTopMargin = margin;
        centreSheet();
    }

    bool Overlay::closeOnClickOutside() const
    {
        return m_closeOnClickOutside;
    }

    void Overlay::setCloseOnClickOutside(bool enabled)
    {
        m_closeOnClickOutside = enabled;
    }

    bool Overlay::isOpen() const
    {
        return m_open;
    }

    qreal Overlay::transition() const
    {
        return m_transition;
    }

    void Overlay::setTransition(qreal value)
    {
        value = qBound(0.0, value, 1.0);
        if (qFuzzyCompare(value + 1.0, m_transition + 1.0)) {
            return;
        }
        m_transition = value;
        centreSheet();
        update();
    }

    void Overlay::openOverlay()
    {
        if (m_open) {
            raise();
            return;
        }
        m_open = true;
        aboutToOpen();

        if (parentWidget()) {
            parentWidget()->installEventFilter(this);
            setGeometry(parentWidget()->rect());
        }

        show();
        raise();
        setFocus(Qt::PopupFocusReason);
        if (m_sheet) {
            m_sheet->show();
            m_sheet->setFocus(Qt::PopupFocusReason);
        }

        m_animation->animateTo(1.0, Duration::Long);

        emit opened();
    }

    void Overlay::closeOverlay()
    {
        if (!m_open) {
            return;
        }
        m_open = false;
        if (m_sheet) m_sheet->hide();

        m_animation->animateTo(0.0, Duration::Short);
    }

    void Overlay::aboutToOpen()
    {
    }

    void Overlay::paintEvent(QPaintEvent* event)
    {
        Q_UNUSED(event)
        if (m_transition <= 0.0) {
            return;
        }

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        QColor scrim(0, 0, 0);
        scrim.setAlphaF(ScrimAlpha * m_transition);
        painter.fillRect(rect(), scrim);

        if (m_sheet) {
            painter.setOpacity(m_transition);
            paintShadow(&painter, m_sheet->geometry(), Shape::ExtraLarge, 3);
        }
    }

    void Overlay::resizeEvent(QResizeEvent* event)
    {
        QWidget::resizeEvent(event);
        centreSheet();
    }

    void Overlay::showEvent(QShowEvent* event)
    {
        if (parentWidget()) {
            setGeometry(parentWidget()->rect());
        }
        QWidget::showEvent(event);
        raise();
        centreSheet();
    }

    void Overlay::hideEvent(QHideEvent* event)
    {
        QWidget::hideEvent(event);
        const bool wasOpen = m_open;
        m_open = false;
        if (m_sheet) m_sheet->hide();
        m_animation->snapTo(0.0);
        if (wasOpen) emit closed();
    }

    void Overlay::mousePressEvent(QMouseEvent* event)
    {
        const bool onSheet = m_sheet && m_sheet->geometry().contains(event->position().toPoint());
        if (!onSheet && m_closeOnClickOutside) {
            closeOverlay();
        }
        // The scrim swallows every click so nothing behind it reacts.
        event->accept();
    }

    void Overlay::keyPressEvent(QKeyEvent* event)
    {
        if (event->key() == Qt::Key_Escape) {
            closeOverlay();
            event->accept();
            return;
        }
        QWidget::keyPressEvent(event);
    }

    bool Overlay::eventFilter(QObject* watched, QEvent* event)
    {
        if (watched == parentWidget() && event->type() == QEvent::Resize) {
            setGeometry(parentWidget()->rect());
        } else if (watched == m_sheet && event->type() == QEvent::LayoutRequest) {
            // The sheet grew or shrank - a wrapped password, a longer match list.
            centreSheet();
        }
        return QWidget::eventFilter(watched, event);
    }

    void Overlay::centreSheet()
    {
        if (!m_sheet) {
            return;
        }

        const QSize hint = m_sheet->sizeHint();
        int width = m_sheetWidth > 0 ? m_sheetWidth : hint.width();
        width = qMin(width, qMax(MinSheetWidth, this->width() - 2 * EdgeMargin));

        // A top-anchored sheet only has the room below its anchor to grow into.
        const int available = m_sheetTopMargin >= 0 ? this->height() - m_sheetTopMargin - EdgeMargin
                                                   : this->height() - 2 * EdgeMargin;

        int height = m_sheet->hasHeightForWidth() ? m_sheet->heightForWidth(width) : hint.height();
        height = qMin(qMax(height, hint.height()), qMax(MinSheetHeight, available));

        const int top = m_sheetTopMargin >= 0 ? m_sheetTopMargin : qRound((this->height() - height) / 2.0);
        m_sheet->setGeometry(qRound((this->width() - width) / 2.0), top, width, height);
    }

} // namespace Material
