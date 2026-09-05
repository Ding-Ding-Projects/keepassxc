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

#ifndef KEEPASSXC_MATERIALCONTROLS_H
#define KEEPASSXC_MATERIALCONTROLS_H

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QProgressBar>
#include <QRadioButton>
#include <QSpinBox>
#include <QTabBar>
#include <QTabWidget>
#include <QToolButton>

class QTimer;

/**
 * Material 3 renderings of the stock controls the legacy forms still use.
 *
 * Each class keeps the stock widget's API and signals - it *is* the stock
 * widget, repainted - so a form can promote `QCheckBox` to `Material::CheckBox`
 * in its .ui file, or code can construct one, without touching a single
 * caller, cast or test. Geometry and hit testing follow the Material anatomy:
 * an 18px checkbox with a 40px state layer, a 20px radio, a 4px progress
 * track, an outlined text field for the spin box and the combo box, a 40px
 * icon button for the tool button and a low-container card for the group box.
 */
namespace Material
{
    class CheckBox : public QCheckBox
    {
        Q_OBJECT

    public:
        explicit CheckBox(QWidget* parent = nullptr);
        explicit CheckBox(const QString& text, QWidget* parent = nullptr);

        QSize sizeHint() const override;
        QSize minimumSizeHint() const override;

    protected:
        void paintEvent(QPaintEvent* event) override;
        bool hitButton(const QPoint& pos) const override;
        void enterEvent(QEnterEvent* event) override;
        void leaveEvent(QEvent* event) override;

    private:
        void init();
        bool m_hovered = false;
    };

    class RadioButton : public QRadioButton
    {
        Q_OBJECT

    public:
        explicit RadioButton(QWidget* parent = nullptr);
        explicit RadioButton(const QString& text, QWidget* parent = nullptr);

        QSize sizeHint() const override;
        QSize minimumSizeHint() const override;

    protected:
        void paintEvent(QPaintEvent* event) override;
        bool hitButton(const QPoint& pos) const override;
        void enterEvent(QEnterEvent* event) override;
        void leaveEvent(QEvent* event) override;

    private:
        void init();
        bool m_hovered = false;
    };

    /**
     * The linear progress indicator. Determinate when the range is set,
     * indeterminate (a sweeping segment) when minimum and maximum are both
     * zero, exactly as QProgressBar defines it. The text, when visible, sits
     * to the right of the track in LabelMedium.
     */
    class LinearProgress : public QProgressBar
    {
        Q_OBJECT

    public:
        explicit LinearProgress(QWidget* parent = nullptr);

        /** Track thickness; the design's default is 4px. */
        int thickness() const;
        void setThickness(int thickness);

        QSize sizeHint() const override;
        QSize minimumSizeHint() const override;

    protected:
        void paintEvent(QPaintEvent* event) override;
        void showEvent(QShowEvent* event) override;
        void hideEvent(QHideEvent* event) override;

    private:
        void syncSweep();

        int m_thickness = 4;
        QTimer* m_sweep = nullptr;
        int m_phase = 0;
    };

    /** An outlined number field with the stepper glyphs inside the outline. */
    class SpinBox : public QSpinBox
    {
        Q_OBJECT

    public:
        explicit SpinBox(QWidget* parent = nullptr);

        QSize sizeHint() const override;
        QSize minimumSizeHint() const override;

    protected:
        void paintEvent(QPaintEvent* event) override;
        void resizeEvent(QResizeEvent* event) override;
        void mousePressEvent(QMouseEvent* event) override;
    };

    class DoubleSpinBox : public QDoubleSpinBox
    {
        Q_OBJECT

    public:
        explicit DoubleSpinBox(QWidget* parent = nullptr);

        QSize sizeHint() const override;
        QSize minimumSizeHint() const override;

    protected:
        void paintEvent(QPaintEvent* event) override;
        void resizeEvent(QResizeEvent* event) override;
        void mousePressEvent(QMouseEvent* event) override;
    };

    /** An outlined menu field: the current item and a trailing expand glyph. */
    class ComboBox : public QComboBox
    {
        Q_OBJECT

    public:
        explicit ComboBox(QWidget* parent = nullptr);

        QSize sizeHint() const override;
        QSize minimumSizeHint() const override;

    protected:
        void paintEvent(QPaintEvent* event) override;
        void enterEvent(QEnterEvent* event) override;
        void leaveEvent(QEvent* event) override;

    private:
        bool m_hovered = false;
    };

    /**
     * A 40px icon button. With text beside or under the icon it widens into
     * a tonal pill, so the compact bottom navigation reads as Material too.
     */
    class ToolButton : public QToolButton
    {
        Q_OBJECT

    public:
        explicit ToolButton(QWidget* parent = nullptr);

        QSize sizeHint() const override;
        QSize minimumSizeHint() const override;

    protected:
        void paintEvent(QPaintEvent* event) override;
        void enterEvent(QEnterEvent* event) override;
        void leaveEvent(QEvent* event) override;

    private:
        bool m_hovered = false;
    };

    /**
     * A dialog's action row: the accepting button is the one filled action,
     * every other button is a text button, in the order the box lays them out.
     * The buttons are the box's own QPushButtons, restyled through the
     * `flat` and `default` states the Material style sheet already dresses.
     */
    class ButtonBox : public QDialogButtonBox
    {
        Q_OBJECT

    public:
        explicit ButtonBox(QWidget* parent = nullptr);
        explicit ButtonBox(StandardButtons buttons, QWidget* parent = nullptr);

    protected:
        bool event(QEvent* event) override;

    private:
        void restyle();
    };

    /** Material tabs: LabelLarge titles over a 3px primary indicator. */
    class TabBar : public QTabBar
    {
        Q_OBJECT

    public:
        explicit TabBar(QWidget* parent = nullptr);

        QSize tabSizeHint(int index) const override;
        QSize minimumTabSizeHint(int index) const override;

    protected:
        void paintEvent(QPaintEvent* event) override;
    };

    class TabWidget : public QTabWidget
    {
        Q_OBJECT

    public:
        explicit TabWidget(QWidget* parent = nullptr);
    };

    /** A low-container card with its title as a TitleSmall header row. */
    class GroupBox : public QGroupBox
    {
        Q_OBJECT

    public:
        explicit GroupBox(QWidget* parent = nullptr);
        explicit GroupBox(const QString& title, QWidget* parent = nullptr);

    protected:
        void paintEvent(QPaintEvent* event) override;
        void mousePressEvent(QMouseEvent* event) override;
        void mouseReleaseEvent(QMouseEvent* event) override;

    private:
        void init();
        QRect titleRect() const;
    };

} // namespace Material

#endif // KEEPASSXC_MATERIALCONTROLS_H
