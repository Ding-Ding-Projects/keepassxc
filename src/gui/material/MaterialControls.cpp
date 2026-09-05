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

#include "MaterialControls.h"

#include "MaterialElevation.h"
#include "MaterialIcons.h"
#include "MaterialTheme.h"

#include <QCursor>
#include <QEvent>
#include <QHash>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QStyleOptionSpinBox>
#include <QTimer>

namespace Material
{
    namespace
    {
        constexpr int CheckSize = 18;
        constexpr int CheckRadius = 2;
        constexpr int RadioSize = 20;
        constexpr int RadioDot = 10;
        constexpr int StateLayer = 40;
        constexpr int LabelGap = 12;
        constexpr int ControlHeight = 40;
        constexpr int FieldHeight = 40;
        constexpr int FieldPadding = 14;
        constexpr int FieldGlyph = 20;
        constexpr int StepperWidth = 24;
        constexpr int IconGlyph = 24;
        constexpr int GroupTitleHeight = 36;
        constexpr int GroupPadding = 16;
        constexpr int SweepPeriodMs = 1400;
        constexpr int SweepTickMs = 16;

        constexpr qreal HoverAlpha = 0.08;
        constexpr qreal PressedAlpha = 0.12;
        constexpr qreal DisabledOpacity = 0.38;

        QColor withAlpha(QColor color, qreal alpha)
        {
            color.setAlphaF(alpha);
            return color;
        }

        /** The hover / pressed circle behind a selection control. */
        void paintStateLayer(QPainter& painter, const QPoint& centre, bool hovered, bool pressed, const QColor& tint)
        {
            const qreal alpha = pressed ? PressedAlpha : (hovered ? HoverAlpha : 0.0);
            if (alpha <= 0.0) {
                return;
            }
            painter.setPen(Qt::NoPen);
            painter.setBrush(withAlpha(tint, alpha));
            painter.drawEllipse(centre, StateLayer / 2, StateLayer / 2);
        }

        void paintFocusRing(QPainter& painter, const QRect& rect, int radius)
        {
            painter.setPen(QPen(theme()->color(Role::Primary), 2));
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(QRectF(rect).adjusted(1, 1, -1, -1), radius, radius);
        }

        QSize selectionHint(const QFont& font, const QString& text, int controlSize)
        {
            const QFontMetrics metrics(font);
            const int textWidth = text.isEmpty() ? 0 : metrics.horizontalAdvance(text) + LabelGap;
            const int height = qMax(ControlHeight, metrics.height() + 8);
            Q_UNUSED(controlSize)
            return {StateLayer + textWidth, height};
        }

        void paintLabel(QPainter& painter, const QRect& area, const QString& text, const QFont& font, bool enabled)
        {
            if (text.isEmpty()) {
                return;
            }
            painter.setFont(font);
            painter.setPen(theme()->color(Role::OnSurface));
            painter.setOpacity(enabled ? 1.0 : DisabledOpacity);
            painter.drawText(area,
                             Qt::AlignVCenter | Qt::AlignLeft | Qt::TextShowMnemonic,
                             painter.fontMetrics().elidedText(text, Qt::ElideRight, area.width()));
            painter.setOpacity(1.0);
        }

        /** The outlined text field every input in the design sits in. */
        void paintField(QPainter& painter, const QRect& rect, bool focused, bool hovered, bool enabled)
        {
            const QColor fill = theme()->color(Role::SurfaceContainerLowest);
            QColor border = theme()->color(focused ? Role::Primary : (hovered ? Role::OnSurface : Role::Outline));
            if (!enabled) {
                border = withAlpha(theme()->color(Role::OnSurface), 0.12);
            }
            const qreal width = focused ? 2.0 : 1.0;
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setPen(QPen(border, width));
            painter.setBrush(fill);
            const qreal inset = width / 2.0;
            painter.drawRoundedRect(
                QRectF(rect).adjusted(inset, inset, -inset, -inset), Shape::ExtraSmall, Shape::ExtraSmall);
        }

        template <typename Box> void makeFieldTransparent(Box* box)
        {
            box->setFrame(false);
            box->setButtonSymbols(QAbstractSpinBox::NoButtons);
            box->setAttribute(Qt::WA_Hover);
            box->setMinimumHeight(FieldHeight);
            if (QLineEdit* edit = box->findChild<QLineEdit*>()) {
                edit->setFrame(false);
                edit->setStyleSheet(QStringLiteral("QLineEdit{background:transparent;border:none;color:%1;}")
                                        .arg(theme()->hex(Role::OnSurface)));
                edit->setFont(theme()->font(TypeRole::BodyLarge));
            }
            box->setFont(theme()->font(TypeRole::BodyLarge));
        }

        template <typename Box> void paintStepper(QPainter& painter, Box* box)
        {
            const bool focused = box->hasFocus();
            const bool hovered = box->underMouse();
            paintField(painter, box->rect(), focused, hovered, box->isEnabled());

            const QColor ink = theme()->color(Role::OnSurfaceVariant);
            const int right = box->width() - 6;
            const QRect up(right - StepperWidth, 2, StepperWidth, box->height() / 2 - 2);
            const QRect down(right - StepperWidth, box->height() / 2, StepperWidth, box->height() / 2 - 2);
            painter.setOpacity(box->isEnabled() ? 1.0 : DisabledOpacity);
            painter.drawPixmap(up.center() - QPoint(8, 8), Icons::pixmap(QStringLiteral("expand_less"), 16, ink));
            painter.drawPixmap(down.center() - QPoint(8, 8), Icons::pixmap(QStringLiteral("expand_more"), 16, ink));
            painter.setOpacity(1.0);
        }

        template <typename Box> void stepFromClick(Box* box, const QPoint& pos)
        {
            const int right = box->width() - 6;
            if (pos.x() < right - StepperWidth || pos.x() > right) {
                return;
            }
            box->stepBy(pos.y() < box->height() / 2 ? 1 : -1);
        }

        template <typename Box> void reserveStepper(Box* box)
        {
            if (QLineEdit* edit = box->findChild<QLineEdit*>()) {
                edit->setGeometry(
                    FieldPadding, 0, qMax(0, box->width() - FieldPadding - StepperWidth - 10), box->height());
            }
        }
    } // namespace

    // ------------------------------------------------------------ CheckBox

    CheckBox::CheckBox(QWidget* parent)
        : QCheckBox(parent)
    {
        init();
    }

    CheckBox::CheckBox(const QString& text, QWidget* parent)
        : QCheckBox(text, parent)
    {
        init();
    }

    void CheckBox::init()
    {
        setAttribute(Qt::WA_Hover);
        setCursor(Qt::PointingHandCursor);
        setFont(theme()->font(TypeRole::BodyMedium));
        connect(theme(), &Theme::changed, this, [this] {
            setFont(theme()->font(TypeRole::BodyMedium));
            update();
        });
    }

    QSize CheckBox::sizeHint() const
    {
        return selectionHint(font(), text(), CheckSize);
    }

    QSize CheckBox::minimumSizeHint() const
    {
        return {StateLayer, ControlHeight};
    }

    bool CheckBox::hitButton(const QPoint& pos) const
    {
        return rect().contains(pos);
    }

    void CheckBox::enterEvent(QEnterEvent* event)
    {
        m_hovered = true;
        update();
        QCheckBox::enterEvent(event);
    }

    void CheckBox::leaveEvent(QEvent* event)
    {
        m_hovered = false;
        update();
        QCheckBox::leaveEvent(event);
    }

    void CheckBox::paintEvent(QPaintEvent* event)
    {
        Q_UNUSED(event)
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        const bool on = checkState() != Qt::Unchecked;
        const QColor tint = theme()->color(on ? Role::Primary : Role::OnSurface);
        const QPoint centre(StateLayer / 2, height() / 2);
        paintStateLayer(painter, centre, m_hovered, isDown(), tint);

        const QRect box(centre.x() - CheckSize / 2, centre.y() - CheckSize / 2, CheckSize, CheckSize);
        painter.setOpacity(isEnabled() ? 1.0 : DisabledOpacity);
        if (on) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(theme()->color(Role::Primary));
            painter.drawRoundedRect(box, CheckRadius, CheckRadius);
            const QString glyph =
                checkState() == Qt::PartiallyChecked ? QStringLiteral("remove") : QStringLiteral("check");
            painter.drawPixmap(box.adjusted(2, 2, -2, -2).topLeft(),
                               Icons::pixmap(glyph, CheckSize - 4, theme()->color(Role::OnPrimary)));
        } else {
            painter.setPen(QPen(theme()->color(Role::OnSurfaceVariant), 2));
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(QRectF(box).adjusted(1, 1, -1, -1), CheckRadius, CheckRadius);
        }
        painter.setOpacity(1.0);

        if (hasFocus()) {
            paintFocusRing(painter, box.adjusted(-4, -4, 4, 4), CheckRadius + 4);
        }

        paintLabel(painter, QRect(StateLayer, 0, width() - StateLayer, height()), text(), font(), isEnabled());
    }

    // --------------------------------------------------------- RadioButton

    RadioButton::RadioButton(QWidget* parent)
        : QRadioButton(parent)
    {
        init();
    }

    RadioButton::RadioButton(const QString& text, QWidget* parent)
        : QRadioButton(text, parent)
    {
        init();
    }

    void RadioButton::init()
    {
        setAttribute(Qt::WA_Hover);
        setCursor(Qt::PointingHandCursor);
        setFont(theme()->font(TypeRole::BodyMedium));
        connect(theme(), &Theme::changed, this, [this] {
            setFont(theme()->font(TypeRole::BodyMedium));
            update();
        });
    }

    QSize RadioButton::sizeHint() const
    {
        return selectionHint(font(), text(), RadioSize);
    }

    QSize RadioButton::minimumSizeHint() const
    {
        return {StateLayer, ControlHeight};
    }

    bool RadioButton::hitButton(const QPoint& pos) const
    {
        return rect().contains(pos);
    }

    void RadioButton::enterEvent(QEnterEvent* event)
    {
        m_hovered = true;
        update();
        QRadioButton::enterEvent(event);
    }

    void RadioButton::leaveEvent(QEvent* event)
    {
        m_hovered = false;
        update();
        QRadioButton::leaveEvent(event);
    }

    void RadioButton::paintEvent(QPaintEvent* event)
    {
        Q_UNUSED(event)
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        const bool on = isChecked();
        const QColor tint = theme()->color(on ? Role::Primary : Role::OnSurface);
        const QPoint centre(StateLayer / 2, height() / 2);
        paintStateLayer(painter, centre, m_hovered, isDown(), tint);

        painter.setOpacity(isEnabled() ? 1.0 : DisabledOpacity);
        painter.setPen(QPen(theme()->color(on ? Role::Primary : Role::OnSurfaceVariant), 2));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(centre, RadioSize / 2 - 1, RadioSize / 2 - 1);
        if (on) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(theme()->color(Role::Primary));
            painter.drawEllipse(centre, RadioDot / 2, RadioDot / 2);
        }
        painter.setOpacity(1.0);

        if (hasFocus()) {
            painter.setPen(QPen(theme()->color(Role::Primary), 2));
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(centre, RadioSize / 2 + 4, RadioSize / 2 + 4);
        }

        paintLabel(painter, QRect(StateLayer, 0, width() - StateLayer, height()), text(), font(), isEnabled());
    }

    // ------------------------------------------------------ LinearProgress

    LinearProgress::LinearProgress(QWidget* parent)
        : QProgressBar(parent)
        , m_sweep(new QTimer(this))
    {
        setTextVisible(false);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_sweep->setInterval(SweepTickMs);
        connect(m_sweep, &QTimer::timeout, this, [this] {
            m_phase = (m_phase + SweepTickMs) % SweepPeriodMs;
            update();
        });
        connect(this, &QProgressBar::valueChanged, this, [this] { syncSweep(); });
        connect(theme(), &Theme::changed, this, [this] { update(); });
    }

    int LinearProgress::thickness() const
    {
        return m_thickness;
    }

    void LinearProgress::setThickness(int thickness)
    {
        m_thickness = qMax(2, thickness);
        updateGeometry();
        update();
    }

    QSize LinearProgress::sizeHint() const
    {
        const int text = isTextVisible() ? QFontMetrics(theme()->font(TypeRole::LabelMedium)).height() : 0;
        return {200, qMax(m_thickness, text)};
    }

    QSize LinearProgress::minimumSizeHint() const
    {
        return {40, sizeHint().height()};
    }

    void LinearProgress::syncSweep()
    {
        const bool indeterminate = minimum() == 0 && maximum() == 0;
        if (indeterminate && isVisible()) {
            if (!m_sweep->isActive()) {
                m_sweep->start();
            }
        } else if (m_sweep->isActive()) {
            m_sweep->stop();
        }
    }

    void LinearProgress::showEvent(QShowEvent* event)
    {
        QProgressBar::showEvent(event);
        syncSweep();
    }

    void LinearProgress::hideEvent(QHideEvent* event)
    {
        QProgressBar::hideEvent(event);
        m_sweep->stop();
    }

    void LinearProgress::paintEvent(QPaintEvent* event)
    {
        Q_UNUSED(event)
        syncSweep();
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        int trackWidth = width();
        if (isTextVisible()) {
            painter.setFont(theme()->font(TypeRole::LabelMedium));
            const QString label = text();
            const int labelWidth = painter.fontMetrics().horizontalAdvance(label) + 12;
            trackWidth = qMax(0, width() - labelWidth);
            painter.setPen(theme()->color(Role::OnSurfaceVariant));
            painter.drawText(
                QRect(trackWidth + 12, 0, labelWidth - 12, height()), Qt::AlignVCenter | Qt::AlignRight, label);
        }

        const QRectF track(0, (height() - m_thickness) / 2.0, trackWidth, m_thickness);
        const qreal radius = m_thickness / 2.0;
        painter.setPen(Qt::NoPen);
        painter.setBrush(theme()->color(Role::SecondaryContainer));
        painter.drawRoundedRect(track, radius, radius);

        painter.setBrush(theme()->color(Role::Primary));
        if (minimum() == 0 && maximum() == 0) {
            // A segment that grows from the left and shrinks into the right.
            const qreal t = m_phase / qreal(SweepPeriodMs);
            const qreal head = qMin(1.0, t * 1.5);
            const qreal tail = qMax(0.0, (t - 0.35) * 1.55);
            const QRectF segment(
                track.left() + tail * trackWidth, track.top(), (head - tail) * trackWidth, m_thickness);
            if (segment.width() > 0) {
                painter.drawRoundedRect(segment, radius, radius);
            }
        } else if (maximum() > minimum()) {
            const qreal fraction = qBound(0.0, (value() - minimum()) / qreal(maximum() - minimum()), 1.0);
            if (fraction > 0.0) {
                painter.drawRoundedRect(
                    QRectF(track.left(), track.top(), trackWidth * fraction, m_thickness), radius, radius);
            }
        }
    }

    // ------------------------------------------------------------- SpinBox

    SpinBox::SpinBox(QWidget* parent)
        : QSpinBox(parent)
    {
        makeFieldTransparent(this);
        connect(theme(), &Theme::changed, this, [this] {
            makeFieldTransparent(this);
            update();
        });
    }

    QSize SpinBox::sizeHint() const
    {
        const QSize base = QSpinBox::sizeHint();
        return {base.width() + StepperWidth + FieldPadding, qMax(FieldHeight, base.height())};
    }

    QSize SpinBox::minimumSizeHint() const
    {
        return {StepperWidth + FieldPadding + 40, FieldHeight};
    }

    void SpinBox::resizeEvent(QResizeEvent* event)
    {
        QSpinBox::resizeEvent(event);
        reserveStepper(this);
    }

    void SpinBox::mousePressEvent(QMouseEvent* event)
    {
        if (event->button() == Qt::LeftButton && event->pos().x() >= width() - 6 - StepperWidth) {
            stepFromClick(this, event->pos());
            event->accept();
            return;
        }
        QSpinBox::mousePressEvent(event);
    }

    void SpinBox::paintEvent(QPaintEvent* event)
    {
        Q_UNUSED(event)
        reserveStepper(this);
        QPainter painter(this);
        paintStepper(painter, this);
    }

    // ------------------------------------------------------- DoubleSpinBox

    DoubleSpinBox::DoubleSpinBox(QWidget* parent)
        : QDoubleSpinBox(parent)
    {
        makeFieldTransparent(this);
        connect(theme(), &Theme::changed, this, [this] {
            makeFieldTransparent(this);
            update();
        });
    }

    QSize DoubleSpinBox::sizeHint() const
    {
        const QSize base = QDoubleSpinBox::sizeHint();
        return {base.width() + StepperWidth + FieldPadding, qMax(FieldHeight, base.height())};
    }

    QSize DoubleSpinBox::minimumSizeHint() const
    {
        return {StepperWidth + FieldPadding + 40, FieldHeight};
    }

    void DoubleSpinBox::resizeEvent(QResizeEvent* event)
    {
        QDoubleSpinBox::resizeEvent(event);
        reserveStepper(this);
    }

    void DoubleSpinBox::mousePressEvent(QMouseEvent* event)
    {
        if (event->button() == Qt::LeftButton && event->pos().x() >= width() - 6 - StepperWidth) {
            stepFromClick(this, event->pos());
            event->accept();
            return;
        }
        QDoubleSpinBox::mousePressEvent(event);
    }

    void DoubleSpinBox::paintEvent(QPaintEvent* event)
    {
        Q_UNUSED(event)
        reserveStepper(this);
        QPainter painter(this);
        paintStepper(painter, this);
    }

    // ------------------------------------------------------------ ComboBox

    ComboBox::ComboBox(QWidget* parent)
        : QComboBox(parent)
    {
        setAttribute(Qt::WA_Hover);
        setCursor(Qt::PointingHandCursor);
        setMinimumHeight(FieldHeight);
        setFont(theme()->font(TypeRole::BodyLarge));
        connect(theme(), &Theme::changed, this, [this] {
            setFont(theme()->font(TypeRole::BodyLarge));
            update();
        });
    }

    QSize ComboBox::sizeHint() const
    {
        const QSize base = QComboBox::sizeHint();
        return {base.width() + FieldPadding, qMax(FieldHeight, base.height())};
    }

    QSize ComboBox::minimumSizeHint() const
    {
        const QSize base = QComboBox::minimumSizeHint();
        return {qMax(base.width(), FieldPadding * 2 + FieldGlyph + 24), FieldHeight};
    }

    void ComboBox::enterEvent(QEnterEvent* event)
    {
        m_hovered = true;
        update();
        QComboBox::enterEvent(event);
    }

    void ComboBox::leaveEvent(QEvent* event)
    {
        m_hovered = false;
        update();
        QComboBox::leaveEvent(event);
    }

    void ComboBox::paintEvent(QPaintEvent* event)
    {
        Q_UNUSED(event)
        if (isEditable()) {
            // The line edit paints the text; the outline and glyph are ours.
            QPainter painter(this);
            paintField(painter, rect(), hasFocus() || (lineEdit() && lineEdit()->hasFocus()), m_hovered, isEnabled());
            painter.drawPixmap(
                width() - FieldPadding - FieldGlyph,
                (height() - FieldGlyph) / 2,
                Icons::pixmap(QStringLiteral("expand_more"), FieldGlyph, theme()->color(Role::OnSurfaceVariant)));
            return;
        }
        QPainter painter(this);
        paintField(painter, rect(), hasFocus(), m_hovered, isEnabled());

        painter.setOpacity(isEnabled() ? 1.0 : DisabledOpacity);
        const QRect textArea(FieldPadding, 0, width() - FieldPadding * 2 - FieldGlyph - 8, height());
        int x = textArea.left();
        const QIcon icon = itemIcon(currentIndex());
        if (!icon.isNull()) {
            icon.paint(&painter, QRect(x, (height() - 18) / 2, 18, 18));
            x += 18 + 8;
        }
        painter.setFont(font());
        painter.setPen(theme()->color(Role::OnSurface));
        const QString label = currentText().isEmpty() ? placeholderText() : currentText();
        painter.drawText(QRect(x, 0, textArea.right() - x, height()),
                         Qt::AlignVCenter | Qt::AlignLeft,
                         painter.fontMetrics().elidedText(label, Qt::ElideRight, textArea.right() - x));
        painter.drawPixmap(
            width() - FieldPadding - FieldGlyph,
            (height() - FieldGlyph) / 2,
            Icons::pixmap(QStringLiteral("expand_more"), FieldGlyph, theme()->color(Role::OnSurfaceVariant)));
        painter.setOpacity(1.0);
    }

    // ---------------------------------------------------------- ToolButton

    ToolButton::ToolButton(QWidget* parent)
        : QToolButton(parent)
    {
        setAttribute(Qt::WA_Hover);
        setCursor(Qt::PointingHandCursor);
        setAutoRaise(true);
        setFont(theme()->font(TypeRole::LabelMedium));
        connect(theme(), &Theme::changed, this, [this] {
            setFont(theme()->font(TypeRole::LabelMedium));
            update();
        });
    }

    QSize ToolButton::sizeHint() const
    {
        if (toolButtonStyle() == Qt::ToolButtonIconOnly || text().isEmpty()) {
            return {ControlHeight, ControlHeight};
        }
        const QFontMetrics metrics(font());
        const int textWidth = metrics.horizontalAdvance(text());
        if (toolButtonStyle() == Qt::ToolButtonTextUnderIcon) {
            return {qMax(64, textWidth + 24), IconGlyph + metrics.height() + 20};
        }
        const int iconWidth = toolButtonStyle() == Qt::ToolButtonTextOnly ? 0 : IconGlyph + 8;
        return {textWidth + iconWidth + 32, ControlHeight};
    }

    QSize ToolButton::minimumSizeHint() const
    {
        return sizeHint();
    }

    void ToolButton::enterEvent(QEnterEvent* event)
    {
        m_hovered = true;
        update();
        QToolButton::enterEvent(event);
    }

    void ToolButton::leaveEvent(QEvent* event)
    {
        m_hovered = false;
        update();
        QToolButton::leaveEvent(event);
    }

    void ToolButton::paintEvent(QPaintEvent* event)
    {
        Q_UNUSED(event)
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        const bool on = isChecked();
        const bool iconOnly = toolButtonStyle() == Qt::ToolButtonIconOnly || text().isEmpty();
        const QColor tint = theme()->color(on ? Role::OnSecondaryContainer : Role::OnSurfaceVariant);

        // The container: a circle for a lone icon, a pill for icon and text.
        QRectF container =
            iconOnly
                ? QRectF(
                      (width() - ControlHeight) / 2.0, (height() - ControlHeight) / 2.0, ControlHeight, ControlHeight)
                : QRectF(rect());
        if (toolButtonStyle() == Qt::ToolButtonTextUnderIcon) {
            container = QRectF((width() - 56) / 2.0, 4, 56, 32);
        }
        const qreal radius = qMin(container.width(), container.height()) / 2.0;
        painter.setPen(Qt::NoPen);
        if (on) {
            painter.setBrush(theme()->color(Role::SecondaryContainer));
            painter.drawRoundedRect(container, radius, radius);
        }
        const qreal alpha = isDown() ? PressedAlpha : (m_hovered ? HoverAlpha : 0.0);
        if (alpha > 0.0) {
            painter.setBrush(withAlpha(tint, alpha));
            painter.drawRoundedRect(container, radius, radius);
        }

        painter.setOpacity(isEnabled() ? 1.0 : DisabledOpacity);
        QIcon glyph = icon();
        if (glyph.isNull() && arrowType() != Qt::NoArrow) {
            // A bare arrow button (list scrollers) gets the matching chevron.
            static const QHash<Qt::ArrowType, QString> arrows = {{Qt::UpArrow, QStringLiteral("expand_less")},
                                                                 {Qt::DownArrow, QStringLiteral("expand_more")},
                                                                 {Qt::LeftArrow, QStringLiteral("chevron_left")},
                                                                 {Qt::RightArrow, QStringLiteral("chevron_right")}};
            glyph = Icons::symbol(arrows.value(arrowType()), tint);
        }
        const QIcon::Mode mode = isEnabled() ? QIcon::Normal : QIcon::Disabled;
        if (toolButtonStyle() == Qt::ToolButtonTextUnderIcon) {
            if (!glyph.isNull()) {
                glyph.paint(&painter,
                            QRect(int(container.center().x()) - IconGlyph / 2,
                                  int(container.center().y()) - IconGlyph / 2,
                                  IconGlyph,
                                  IconGlyph),
                            Qt::AlignCenter,
                            mode,
                            on ? QIcon::On : QIcon::Off);
            }
            painter.setFont(font());
            painter.setPen(theme()->color(on ? Role::OnSurface : Role::OnSurfaceVariant));
            const QRect labelArea(0, int(container.bottom()) + 4, width(), height() - int(container.bottom()) - 4);
            painter.drawText(labelArea,
                             Qt::AlignHCenter | Qt::AlignTop,
                             painter.fontMetrics().elidedText(text(), Qt::ElideRight, width() - 4));
        } else if (iconOnly) {
            if (!glyph.isNull()) {
                glyph.paint(&painter,
                            QRect(int(container.center().x()) - IconGlyph / 2,
                                  int(container.center().y()) - IconGlyph / 2,
                                  IconGlyph,
                                  IconGlyph),
                            Qt::AlignCenter,
                            mode,
                            on ? QIcon::On : QIcon::Off);
            } else {
                painter.setFont(font());
                painter.setPen(tint);
                painter.drawText(
                    rect(), Qt::AlignCenter, painter.fontMetrics().elidedText(text(), Qt::ElideRight, width() - 8));
            }
        } else {
            int x = 16;
            if (toolButtonStyle() != Qt::ToolButtonTextOnly && !glyph.isNull()) {
                glyph.paint(&painter,
                            QRect(x, (height() - IconGlyph) / 2, IconGlyph, IconGlyph),
                            Qt::AlignCenter,
                            mode,
                            on ? QIcon::On : QIcon::Off);
                x += IconGlyph + 8;
            }
            painter.setFont(font());
            painter.setPen(tint);
            painter.drawText(QRect(x, 0, width() - x - 16, height()),
                             Qt::AlignVCenter | Qt::AlignLeft,
                             painter.fontMetrics().elidedText(text(), Qt::ElideRight, width() - x - 16));
        }
        painter.setOpacity(1.0);

        if (hasFocus()) {
            paintFocusRing(painter, container.toRect(), int(radius));
        }
    }

    // ----------------------------------------------------------- ButtonBox

    ButtonBox::ButtonBox(QWidget* parent)
        : QDialogButtonBox(parent)
    {
        restyle();
    }

    ButtonBox::ButtonBox(StandardButtons buttons, QWidget* parent)
        : QDialogButtonBox(buttons, parent)
    {
        restyle();
    }

    bool ButtonBox::event(QEvent* event)
    {
        const bool handled = QDialogButtonBox::event(event);
        if (event->type() == QEvent::ChildAdded || event->type() == QEvent::LayoutRequest
            || event->type() == QEvent::Show) {
            restyle();
        }
        return handled;
    }

    void ButtonBox::restyle()
    {
        for (QAbstractButton* button : buttons()) {
            auto* push = qobject_cast<QPushButton*>(button);
            if (!push) {
                continue;
            }
            const ButtonRole role = buttonRole(button);
            const bool primary = role == AcceptRole || role == YesRole || role == ApplyRole;
            push->setFlat(!primary);
            push->setMinimumHeight(FieldHeight);
            push->setCursor(Qt::PointingHandCursor);
            push->setFont(theme()->font(TypeRole::LabelLarge));
        }
    }

    // -------------------------------------------------------------- TabBar

    namespace
    {
        constexpr int TabHeight = 48;
        constexpr int TabPadding = 16;
        constexpr int TabIndicator = 3;
    } // namespace

    TabBar::TabBar(QWidget* parent)
        : QTabBar(parent)
    {
        setDrawBase(false);
        setExpanding(false);
        setElideMode(Qt::ElideRight);
        setUsesScrollButtons(true);
        setFont(theme()->font(TypeRole::LabelLarge));
        connect(theme(), &Theme::changed, this, [this] {
            setFont(theme()->font(TypeRole::LabelLarge));
            update();
        });
    }

    QSize TabBar::tabSizeHint(int index) const
    {
        const QFontMetrics metrics(font());
        const int icon = tabIcon(index).isNull() ? 0 : 18 + 8;
        return {metrics.horizontalAdvance(tabText(index)) + icon + TabPadding * 2, TabHeight};
    }

    QSize TabBar::minimumTabSizeHint(int index) const
    {
        Q_UNUSED(index)
        return {TabPadding * 2 + 40, TabHeight};
    }

    void TabBar::paintEvent(QPaintEvent* event)
    {
        Q_UNUSED(event)
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(theme()->color(Role::OutlineVariant));
        painter.drawLine(0, height() - 1, width(), height() - 1);

        for (int index = 0; index < count(); ++index) {
            const QRect tab = tabRect(index);
            if (!tab.isValid() || !tab.intersects(rect())) {
                continue;
            }
            const bool selected = index == currentIndex();
            const bool enabled = isTabEnabled(index);
            const bool hovered = tab.contains(mapFromGlobal(QCursor::pos())) && underMouse();
            if (hovered && enabled) {
                painter.setPen(Qt::NoPen);
                painter.setBrush(withAlpha(theme()->color(Role::OnSurface), HoverAlpha));
                painter.drawRect(tab);
            }
            painter.setOpacity(enabled ? 1.0 : DisabledOpacity);
            painter.setFont(font());
            painter.setPen(theme()->color(selected ? Role::Primary : Role::OnSurfaceVariant));
            int x = tab.left() + TabPadding;
            const QIcon icon = tabIcon(index);
            if (!icon.isNull()) {
                icon.paint(&painter, QRect(x, tab.center().y() - 9, 18, 18));
                x += 18 + 8;
            }
            painter.drawText(QRect(x, tab.top(), tab.right() - x - TabPadding + 1, tab.height()),
                             Qt::AlignVCenter | Qt::AlignLeft,
                             painter.fontMetrics().elidedText(
                                 tabText(index), Qt::ElideRight, tab.width() - (x - tab.left()) - TabPadding));
            painter.setOpacity(1.0);
            if (selected) {
                painter.setPen(Qt::NoPen);
                painter.setBrush(theme()->color(Role::Primary));
                const QRect indicator(
                    x, tab.bottom() - TabIndicator + 1, tab.right() - x - TabPadding + 1, TabIndicator);
                painter.drawRoundedRect(QRectF(indicator.adjusted(0, 0, 0, TabIndicator)), TabIndicator, TabIndicator);
                painter.drawRect(indicator);
            }
        }
    }

    TabWidget::TabWidget(QWidget* parent)
        : QTabWidget(parent)
    {
        setTabBar(new TabBar(this));
        setDocumentMode(true);
        setStyleSheet(QStringLiteral("QTabWidget::pane{border:none;background:transparent;}"));
    }

    // ------------------------------------------------------------ GroupBox

    GroupBox::GroupBox(QWidget* parent)
        : QGroupBox(parent)
    {
        init();
    }

    GroupBox::GroupBox(const QString& title, QWidget* parent)
        : QGroupBox(title, parent)
    {
        init();
    }

    void GroupBox::init()
    {
        setFlat(false);
        setFont(theme()->font(TypeRole::TitleSmall));
        // The card's own padding; the style sheet's margins would double it.
        setStyleSheet(QStringLiteral("QGroupBox{border:none;margin-top:%1px;padding:%2px;background:transparent;}"
                                     "QGroupBox::title{subcontrol-origin:margin;left:0;top:0;color:transparent;}")
                          .arg(GroupTitleHeight)
                          .arg(GroupPadding));
        connect(theme(), &Theme::changed, this, [this] {
            setFont(theme()->font(TypeRole::TitleSmall));
            update();
        });
    }

    QRect GroupBox::titleRect() const
    {
        return {0, 0, width(), GroupTitleHeight};
    }

    void GroupBox::paintEvent(QPaintEvent* event)
    {
        Q_UNUSED(event)
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        // The card behind the whole group, header row included.
        paintSurface(&painter,
                     rect(),
                     Shape::Medium,
                     theme()->color(Role::SurfaceContainerLow),
                     theme()->color(Role::OutlineVariant));

        int x = GroupPadding;
        if (isCheckable()) {
            const QRect box(x, (GroupTitleHeight - CheckSize) / 2, CheckSize, CheckSize);
            if (isChecked()) {
                painter.setPen(Qt::NoPen);
                painter.setBrush(theme()->color(Role::Primary));
                painter.drawRoundedRect(box, CheckRadius, CheckRadius);
                painter.drawPixmap(
                    box.adjusted(2, 2, -2, -2).topLeft(),
                    Icons::pixmap(QStringLiteral("check"), CheckSize - 4, theme()->color(Role::OnPrimary)));
            } else {
                painter.setPen(QPen(theme()->color(Role::OnSurfaceVariant), 2));
                painter.setBrush(Qt::NoBrush);
                painter.drawRoundedRect(QRectF(box).adjusted(1, 1, -1, -1), CheckRadius, CheckRadius);
            }
            x += CheckSize + LabelGap;
        }
        painter.setFont(font());
        painter.setPen(theme()->color(Role::OnSurface));
        painter.setOpacity(isEnabled() ? 1.0 : DisabledOpacity);
        const QRect titleArea(x, 0, width() - x - GroupPadding, GroupTitleHeight);
        painter.drawText(titleArea,
                         Qt::AlignVCenter | Qt::AlignLeft | Qt::TextShowMnemonic,
                         painter.fontMetrics().elidedText(title(), Qt::ElideRight, titleArea.width()));
        painter.setOpacity(1.0);
        if (!title().isEmpty()) {
            painter.setPen(theme()->color(Role::OutlineVariant));
            painter.drawLine(GroupPadding, GroupTitleHeight - 1, width() - GroupPadding, GroupTitleHeight - 1);
        }
    }

    void GroupBox::mousePressEvent(QMouseEvent* event)
    {
        if (isCheckable() && event->button() == Qt::LeftButton && titleRect().contains(event->pos())) {
            event->accept();
            return;
        }
        QGroupBox::mousePressEvent(event);
    }

    void GroupBox::mouseReleaseEvent(QMouseEvent* event)
    {
        if (isCheckable() && event->button() == Qt::LeftButton && titleRect().contains(event->pos())) {
            setChecked(!isChecked());
            event->accept();
            return;
        }
        QGroupBox::mouseReleaseEvent(event);
    }

} // namespace Material
