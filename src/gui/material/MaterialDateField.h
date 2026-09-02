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

#ifndef KEEPASSXC_MATERIALDATEFIELD_H
#define KEEPASSXC_MATERIALDATEFIELD_H

#include <QDate>
#include <QDateEdit>
#include <QList>

class QAbstractButton;
class QLabel;
class QMenu;
class QWidgetAction;

namespace Material
{
    class IconButton;
    class Select;
    class TextButton;

    /**
     * The Material date field: an outlined field that accepts a typed date in
     * the locale's short format or plain ISO, with a trailing calendar glyph
     * that opens the anchored date picker.
     *
     * The picker is a month grid with previous/next month buttons, a month
     * chooser and a year chooser (both Material selects, so each has its own
     * search bar and anchored regex builder), the weekday row, and a Today
     * action. Choosing a day sets the date and closes; Escape closes without a
     * change; focus returns to the field either way.
     *
     * It is a QDateEdit, so every range, format and dateChanged call the
     * screens make keeps working.
     */
    class DateField : public QDateEdit
    {
        Q_OBJECT

    public:
        explicit DateField(QWidget* parent = nullptr);
        explicit DateField(const QDate& date, QWidget* parent = nullptr);
        ~DateField() override;

        /** Base id for the picker's month and year searches, e.g. "history.from". */
        void setSearchIdentity(const QString& id, const QString& label);

        QMenu* picker() const;
        bool isPickerOpen() const;
        Select* monthSelect() const;
        Select* yearSelect() const;
        /** The day buttons of the current grid, in reading order. */
        QList<QAbstractButton*> dayButtons() const;
        QDate shownMonth() const;

        /** The typing surface, public so a test can type into it. */
        using QAbstractSpinBox::lineEdit;

        QSize sizeHint() const override;
        QSize minimumSizeHint() const override;

    public slots:
        void showPicker();
        void hidePicker();

    protected:
        QDateTime dateTimeFromText(const QString& text) const override;
        void paintEvent(QPaintEvent* event) override;
        void mousePressEvent(QMouseEvent* event) override;
        void keyPressEvent(QKeyEvent* event) override;

    private:
        class DayButton;

        void init();
        void buildPicker();
        void showMonth(const QDate& month);
        void rebuildGrid();
        QRect glyphRect() const;

        QMenu* m_picker = nullptr;
        QWidgetAction* m_pickerAction = nullptr;
        IconButton* m_previous = nullptr;
        IconButton* m_next = nullptr;
        Select* m_month = nullptr;
        Select* m_year = nullptr;
        TextButton* m_today = nullptr;
        QWidget* m_grid = nullptr;
        QList<DayButton*> m_days;
        QList<QLabel*> m_weekdays;
        QDate m_shownMonth;
        QString m_searchId;
        QString m_searchLabel;
        bool m_updating = false;
    };

} // namespace Material

#endif // KEEPASSXC_MATERIALDATEFIELD_H
