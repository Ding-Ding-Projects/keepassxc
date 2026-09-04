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

#include "MaterialDateField.h"

#include "MaterialButtons.h"
#include "MaterialIcons.h"
#include "MaterialSelect.h"
#include "MaterialTheme.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QVBoxLayout>
#include <QWidgetAction>

namespace Material
{
    namespace
    {
        constexpr int GlyphColumn = 36;
        constexpr int GlyphSize = 20;
        constexpr int FieldMinimumWidth = 150;
        constexpr int CellSize = 40;
        constexpr int PickerPadding = 12;
        constexpr int HeaderButtonSize = 40;
        constexpr int YearsBack = 100;
        constexpr int YearsForward = 20;
    } // namespace

    /** One day cell: a 40px circle, filled for the chosen day, outlined for today. */
    class DateField::DayButton : public QAbstractButton
    {
    public:
        explicit DayButton(QWidget* parent = nullptr)
            : QAbstractButton(parent)
        {
            setFixedSize(CellSize, CellSize);
            setCursor(Qt::PointingHandCursor);
            setFocusPolicy(Qt::TabFocus);
            setAttribute(Qt::WA_Hover, true);
        }

        void setDay(const QDate& date, bool inMonth, bool selected, bool today)
        {
            m_date = date;
            m_inMonth = inMonth;
            m_selected = selected;
            m_today = today;
            setText(QString::number(date.day()));
            setAccessibleName(QLocale().toString(date, QLocale::LongFormat));
            setEnabled(date.isValid());
            update();
        }

        QDate date() const
        {
            return m_date;
        }

    protected:
        void paintEvent(QPaintEvent*) override
        {
            QPainter painter(this);
            painter.setRenderHint(QPainter::Antialiasing);
            const QRect circle = rect().adjusted(2, 2, -2, -2);
            painter.setPen(Qt::NoPen);
            if (m_selected) {
                painter.setBrush(theme()->color(Role::Primary));
                painter.drawEllipse(circle);
            } else if (underMouse() || hasFocus()) {
                painter.setBrush(theme()->color(Role::SurfaceContainerHigh));
                painter.drawEllipse(circle);
            }
            if (m_today && !m_selected) {
                painter.setPen(QPen(theme()->color(Role::Primary), 1));
                painter.setBrush(Qt::NoBrush);
                painter.drawEllipse(circle);
            }
            painter.setFont(theme()->font(TypeRole::BodyMedium));
            painter.setPen(theme()->color(m_selected ? Role::OnPrimary : (m_inMonth ? Role::OnSurface : Role::Outline)));
            painter.drawText(rect(), Qt::AlignCenter, painter.fontMetrics().elidedText(text(), Qt::ElideRight, width() - 2));
        }

    private:
        QDate m_date;
        bool m_inMonth = true;
        bool m_selected = false;
        bool m_today = false;
    };

    DateField::DateField(QWidget* parent)
        : QDateEdit(parent)
    {
        init();
    }

    DateField::DateField(const QDate& date, QWidget* parent)
        : QDateEdit(date, parent)
    {
        init();
    }

    DateField::~DateField() = default;

    void DateField::init()
    {
        setCalendarPopup(false);
        setButtonSymbols(QAbstractSpinBox::NoButtons);
        setFocusPolicy(Qt::StrongFocus);
        setAttribute(Qt::WA_Hover, true);
        // The embedded line edit keeps the typing; the frame is painted here,
        // leaving room on the right for the calendar glyph.
        setStyleSheet(QStringLiteral("QDateEdit { background: transparent; border: none; padding: 0 %1px 0 12px; }"
                                     "QDateEdit QLineEdit { background: transparent; border: none; }")
                          .arg(GlyphColumn));
        connect(theme(), &Theme::changed, this, [this] { update(); });
    }

    void DateField::setSearchIdentity(const QString& id, const QString& label)
    {
        m_searchId = id;
        m_searchLabel = label;
        if (m_month) {
            m_month->setSearchIdentity(id + QStringLiteral(".month"), tr("%1: month").arg(label));
            m_year->setSearchIdentity(id + QStringLiteral(".year"), tr("%1: year").arg(label));
        }
    }

    QMenu* DateField::picker() const
    {
        const_cast<DateField*>(this)->buildPicker();
        return m_picker;
    }

    bool DateField::isPickerOpen() const
    {
        return m_picker && m_picker->isVisible();
    }

    Select* DateField::monthSelect() const
    {
        const_cast<DateField*>(this)->buildPicker();
        return m_month;
    }

    Select* DateField::yearSelect() const
    {
        const_cast<DateField*>(this)->buildPicker();
        return m_year;
    }

    QList<QAbstractButton*> DateField::dayButtons() const
    {
        const_cast<DateField*>(this)->buildPicker();
        QList<QAbstractButton*> buttons;
        for (DayButton* day : m_days) {
            buttons << day;
        }
        return buttons;
    }

    QDate DateField::shownMonth() const
    {
        return m_shownMonth;
    }

    // ----------------------------------------------------------------- picker

    void DateField::buildPicker()
    {
        if (m_picker) {
            return;
        }
        m_picker = new QMenu(this);
        m_picker->setObjectName(objectName() + QStringLiteral("Picker"));
        m_picker->setAccessibleName(tr("Choose a date"));

        auto* container = new QWidget(m_picker);
        auto* column = new QVBoxLayout(container);
        column->setContentsMargins(PickerPadding, PickerPadding, PickerPadding, PickerPadding);
        column->setSpacing(8);

        auto* header = new QHBoxLayout;
        header->setSpacing(4);
        m_previous = new IconButton(QStringLiteral("chevron_left"), container);
        m_previous->setDiameter(HeaderButtonSize);
        m_previous->setToolTip(tr("Previous month"));
        m_previous->setAccessibleName(tr("Previous month"));
        connect(m_previous, &QAbstractButton::clicked, this, [this] { showMonth(m_shownMonth.addMonths(-1)); });
        header->addWidget(m_previous);

        m_month = new Select(container);
        m_month->setObjectName(objectName() + QStringLiteral("Month"));
        m_month->setAccessibleName(tr("Month"));
        m_month->setSearchPlaceholder(tr("Search months"));
        for (int month = 1; month <= 12; ++month) {
            m_month->addItem(QLocale().standaloneMonthName(month, QLocale::LongFormat), month);
        }
        connect(m_month, &Select::currentIndexChanged, this, [this](int index) {
            if (!m_updating && index >= 0) {
                showMonth(QDate(m_shownMonth.year(), index + 1, 1));
            }
        });
        header->addWidget(m_month, 1);

        m_year = new Select(container);
        m_year->setObjectName(objectName() + QStringLiteral("Year"));
        m_year->setAccessibleName(tr("Year"));
        m_year->setSearchPlaceholder(tr("Search years"));
        const int thisYear = QDate::currentDate().year();
        const int firstYear = qMax(minimumDate().year(), thisYear - YearsBack);
        const int lastYear = qMin(maximumDate().year(), thisYear + YearsForward);
        for (int year = firstYear; year <= lastYear; ++year) {
            m_year->addItem(QString::number(year), year);
        }
        connect(m_year, &Select::currentIndexChanged, this, [this](int index) {
            if (!m_updating && index >= 0) {
                showMonth(QDate(m_year->itemData(index).toInt(), m_shownMonth.month(), 1));
            }
        });
        header->addWidget(m_year, 0);

        m_next = new IconButton(QStringLiteral("chevron_right"), container);
        m_next->setDiameter(HeaderButtonSize);
        m_next->setToolTip(tr("Next month"));
        m_next->setAccessibleName(tr("Next month"));
        connect(m_next, &QAbstractButton::clicked, this, [this] { showMonth(m_shownMonth.addMonths(1)); });
        header->addWidget(m_next);
        column->addLayout(header);

        if (!m_searchId.isEmpty()) {
            setSearchIdentity(m_searchId, m_searchLabel);
        }

        m_grid = new QWidget(container);
        auto* grid = new QGridLayout(m_grid);
        grid->setContentsMargins(0, 0, 0, 0);
        grid->setSpacing(0);
        const int firstDay = static_cast<int>(QLocale().firstDayOfWeek());
        for (int columnIndex = 0; columnIndex < 7; ++columnIndex) {
            const int day = ((firstDay - 1 + columnIndex) % 7) + 1;
            auto* label = new QLabel(QLocale().standaloneDayName(day, QLocale::NarrowFormat), m_grid);
            label->setAlignment(Qt::AlignCenter);
            label->setFixedSize(CellSize, CellSize - 8);
            label->setAccessibleName(QLocale().standaloneDayName(day, QLocale::LongFormat));
            QPalette palette = label->palette();
            palette.setColor(QPalette::WindowText, theme()->color(Role::OnSurfaceVariant));
            label->setPalette(palette);
            label->setFont(theme()->font(TypeRole::LabelMedium));
            grid->addWidget(label, 0, columnIndex);
            m_weekdays << label;
        }
        for (int row = 0; row < 6; ++row) {
            for (int columnIndex = 0; columnIndex < 7; ++columnIndex) {
                auto* day = new DayButton(m_grid);
                connect(day, &QAbstractButton::clicked, this, [this, day] {
                    if (day->date().isValid()) {
                        hidePicker();
                        setDate(day->date());
                    }
                });
                grid->addWidget(day, row + 1, columnIndex);
                m_days << day;
            }
        }
        column->addWidget(m_grid);

        auto* footer = new QHBoxLayout;
        footer->addStretch(1);
        m_today = new TextButton(QStringLiteral("today"), tr("Today"), container);
        m_today->setAccessibleName(tr("Choose today"));
        connect(m_today, &QAbstractButton::clicked, this, [this] {
            hidePicker();
            setDate(QDate::currentDate());
        });
        footer->addWidget(m_today);
        column->addLayout(footer);

        m_pickerAction = new QWidgetAction(m_picker);
        m_pickerAction->setDefaultWidget(container);
        m_picker->addAction(m_pickerAction);
        connect(m_picker, &QMenu::aboutToShow, this, [this] {
            const QDate current = date().isValid() && date() > minimumDate() ? date() : QDate::currentDate();
            showMonth(QDate(current.year(), current.month(), 1));
            update();
        });
        connect(m_picker, &QMenu::aboutToHide, this, [this] {
            setFocus(Qt::PopupFocusReason);
            update();
        });
        showMonth(QDate(QDate::currentDate().year(), QDate::currentDate().month(), 1));
    }

    void DateField::showMonth(const QDate& month)
    {
        if (!month.isValid()) {
            return;
        }
        m_shownMonth = QDate(month.year(), month.month(), 1);
        m_updating = true;
        m_month->setCurrentIndex(m_shownMonth.month() - 1);
        const int yearIndex = m_year->findData(m_shownMonth.year());
        if (yearIndex >= 0) {
            m_year->setCurrentIndex(yearIndex);
        }
        m_updating = false;
        rebuildGrid();
    }

    void DateField::rebuildGrid()
    {
        const int firstDay = static_cast<int>(QLocale().firstDayOfWeek());
        const int offset = (m_shownMonth.dayOfWeek() - firstDay + 7) % 7;
        QDate cursor = m_shownMonth.addDays(-offset);
        const QDate selected = date();
        const QDate today = QDate::currentDate();
        for (DayButton* day : m_days) {
            const bool inRange = cursor >= minimumDate() && cursor <= maximumDate();
            day->setDay(inRange ? cursor : QDate(), cursor.month() == m_shownMonth.month(), cursor == selected, cursor == today);
            cursor = cursor.addDays(1);
        }
        m_grid->setAccessibleName(tr("%1 %2").arg(QLocale().standaloneMonthName(m_shownMonth.month(), QLocale::LongFormat))
                                      .arg(m_shownMonth.year()));
    }

    void DateField::showPicker()
    {
        if (!isEnabled()) {
            return;
        }
        buildPicker();
        m_picker->popup(mapToGlobal(QPoint(0, height() + 4)));
    }

    void DateField::hidePicker()
    {
        if (m_picker) {
            m_picker->hide();
        }
    }

    // ------------------------------------------------------------------ field

    QDateTime DateField::dateTimeFromText(const QString& text) const
    {
        const QDate iso = QDate::fromString(text.trimmed(), Qt::ISODate);
        const QDate parsed = iso.isValid() ? iso : locale().toDate(text.trimmed(), QLocale::ShortFormat);
        return parsed.isValid() ? QDateTime(parsed, QTime(0, 0)) : QDateTime();
    }

    QRect DateField::glyphRect() const
    {
        return QRect(width() - GlyphColumn + (GlyphColumn - GlyphSize) / 2, (height() - GlyphSize) / 2, GlyphSize, GlyphSize);
    }

    QSize DateField::sizeHint() const
    {
        QSize hint = QDateEdit::sizeHint();
        hint.setWidth(qMax(FieldMinimumWidth, hint.width() + GlyphColumn));
        hint.setHeight(Layout::ButtonHeight);
        return hint;
    }

    QSize DateField::minimumSizeHint() const
    {
        return QSize(FieldMinimumWidth, Layout::ButtonHeight);
    }

    void DateField::paintEvent(QPaintEvent*)
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const bool active = hasFocus() || isPickerOpen() || (lineEdit() && lineEdit()->hasFocus());
        const bool enabled = isEnabled();
        QColor fill = theme()->color(Role::SurfaceContainerLowest);
        QColor border = theme()->color(active ? Role::Primary : (underMouse() ? Role::OnSurfaceVariant : Role::Outline));
        if (!enabled) {
            fill = theme()->color(Role::SurfaceContainer);
            border = theme()->color(Role::OutlineVariant);
        }
        const int borderWidth = active ? 2 : 1;
        painter.setPen(QPen(border, borderWidth));
        painter.setBrush(fill);
        painter.drawRoundedRect(QRectF(rect()).adjusted(borderWidth / 2.0, borderWidth / 2.0, -borderWidth / 2.0, -borderWidth / 2.0),
                                Shape::Large,
                                Shape::Large);
        Icons::symbol(QStringLiteral("calendar_month"), theme()->color(enabled ? Role::OnSurfaceVariant : Role::OutlineVariant))
            .paint(&painter, glyphRect());
    }

    void DateField::mousePressEvent(QMouseEvent* event)
    {
        if (event->button() == Qt::LeftButton && event->pos().x() >= width() - GlyphColumn) {
            showPicker();
            event->accept();
            return;
        }
        QDateEdit::mousePressEvent(event);
    }

    void DateField::keyPressEvent(QKeyEvent* event)
    {
        if (event->key() == Qt::Key_F4 || (event->key() == Qt::Key_Down && event->modifiers().testFlag(Qt::AltModifier))) {
            showPicker();
            event->accept();
            return;
        }
        QDateEdit::keyPressEvent(event);
    }

} // namespace Material
