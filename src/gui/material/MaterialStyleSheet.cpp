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

#include "MaterialStyleSheet.h"

#include "MaterialTheme.h"

#include <QColor>
#include <QHash>
#include <QStringList>

namespace Material
{
    namespace
    {
        /**
         * The stylesheet is written with `%token%` markers and resolved once at
         * the end of the build. Markers are delimited on both sides, so no token
         * can be the prefix of another and the substitution order is irrelevant.
         */
        using Tokens = QHash<QString, QString>;

        /** Blend @p fg over @p bg at @p alpha (0..1): the Material state layer. */
        QColor mix(const QColor& fg, const QColor& bg, qreal alpha)
        {
            alpha = qBound(0.0, alpha, 1.0);
            return QColor::fromRgbF(fg.redF() * alpha + bg.redF() * (1.0 - alpha),
                                    fg.greenF() * alpha + bg.greenF() * (1.0 - alpha),
                                    fg.blueF() * alpha + bg.blueF() * (1.0 - alpha));
        }

        Tokens paletteTokens(const Theme& theme)
        {
            Tokens tokens;

            auto add = [&tokens](const char* name, const QString& value) {
                tokens.insert(QStringLiteral("%") + QString::fromLatin1(name) + QStringLiteral("%"), value);
            };
            auto addColor = [&add, &theme](const char* name, Role role) { add(name, theme.hex(role)); };
            auto addNumber = [&add](const char* name, int value) { add(name, QString::number(value)); };

            addColor("primary", Role::Primary);
            addColor("onPrimary", Role::OnPrimary);
            addColor("primaryContainer", Role::PrimaryContainer);
            addColor("onPrimaryContainer", Role::OnPrimaryContainer);
            addColor("secondaryContainer", Role::SecondaryContainer);
            addColor("onSecondaryContainer", Role::OnSecondaryContainer);

            addColor("error", Role::Error);
            addColor("onError", Role::OnError);
            addColor("errorContainer", Role::ErrorContainer);
            addColor("onErrorContainer", Role::OnErrorContainer);
            addColor("green", Role::Green);
            addColor("greenContainer", Role::GreenContainer);
            addColor("onGreenContainer", Role::OnGreenContainer);
            addColor("amber", Role::Amber);
            addColor("amberContainer", Role::AmberContainer);
            addColor("onAmberContainer", Role::OnAmberContainer);

            addColor("surface", Role::Surface);
            addColor("surfaceLowest", Role::SurfaceContainerLowest);
            addColor("surfaceLow", Role::SurfaceContainerLow);
            addColor("surfaceContainer", Role::SurfaceContainer);
            addColor("surfaceHigh", Role::SurfaceContainerHigh);
            addColor("surfaceHighest", Role::SurfaceContainerHighest);
            addColor("onSurface", Role::OnSurface);
            addColor("onSurfaceVariant", Role::OnSurfaceVariant);
            addColor("outline", Role::Outline);
            addColor("outlineVariant", Role::OutlineVariant);
            addColor("inverseSurface", Role::InverseSurface);
            addColor("inverseOnSurface", Role::InverseOnSurface);

            // State layers. Qt style sheets have no compositing, so every hover,
            // pressed and disabled tint is pre-blended here.
            const QColor surface = theme.color(Role::Surface);
            const QColor onSurface = theme.color(Role::OnSurface);
            const QColor primary = theme.color(Role::Primary);
            const QColor onPrimary = theme.color(Role::OnPrimary);
            const QColor tonal = theme.color(Role::SecondaryContainer);
            const QColor onTonal = theme.color(Role::OnSecondaryContainer);
            add("hover", mix(onSurface, surface, 0.08).name());
            add("hoverStrong", mix(onSurface, surface, 0.12).name());
            add("disabledText", mix(onSurface, surface, 0.38).name());
            add("disabledFill", mix(onSurface, surface, 0.12).name());
            add("primaryHover", mix(onPrimary, primary, 0.10).name());
            add("primaryPressed", mix(onPrimary, primary, 0.18).name());
            add("tonalHover", mix(onTonal, tonal, 0.08).name());
            add("tonalPressed", mix(onTonal, tonal, 0.14).name());
            add("flatHover", mix(primary, surface, 0.10).name());
            add("flatPressed", mix(primary, surface, 0.18).name());

            // Shape scale.
            addNumber("rSmall", Shape::Small);
            addNumber("rMedium", Shape::Medium);
            addNumber("rLarge", Shape::Large);
            addNumber("rRow", Shape::Row);
            addNumber("rRail", Shape::Rail);
            addNumber("rXL", Shape::ExtraLarge);

            // Qt cannot express a pill on a box of unknown height, so the pill
            // radii are half of the height the design gives each control.
            addNumber("btnH", Layout::ButtonHeight);
            addNumber("btnR", Layout::ButtonHeight / 2);
            addNumber("chipH", Layout::ChipHeight);
            addNumber("chipR", Layout::ChipHeight / 2);
            addNumber("searchH", Layout::SurfaceSearchHeight);
            addNumber("searchR", Layout::SurfaceSearchHeight / 2);
            addNumber("tabH", Layout::TabHeight);
            addNumber("tabStripH", Layout::TabStripHeight);
            addNumber("railW", Layout::RailWidth);
            addNumber("appBarH", Layout::AppBarHeight);
            addNumber("detailW", Layout::DetailPaneWidth);

            // Density: a row keeps the theme's height, less the padding the
            // stylesheet adds around the item itself.
            addNumber("rowInner", qMax(24, theme.rowHeight() - 8));
            addNumber("pad", theme.pagePadding());

            add("uiFont", Theme::uiFamily());
            add("monoFont", Theme::monoFamily());
            return tokens;
        }

        QString substitute(const QString& sheet, const Tokens& tokens)
        {
            QString result = sheet;
            for (auto it = tokens.cbegin(); it != tokens.cend(); ++it) {
                result.replace(it.key(), it.value());
            }
            return result;
        }

        // ------------------------------------------------------------- fragments

        QString baseFragment()
        {
            return QStringLiteral(R"CSS(
/* ---------------------------------------------------------------- surfaces */

QWidget {
    color: %onSurface%;
    selection-background-color: %secondaryContainer%;
    selection-color: %onSecondaryContainer%;
}

QWidget:disabled {
    color: %disabledText%;
}

/* Only real windows fill themselves. Children stay unpainted so that widgets
   which draw their own Material surface keep control of every pixel. */
QMainWindow,
QDialog,
QWizard,
QWizardPage {
    background-color: %surface%;
    font-family: "%uiFont%";
}

QMainWindow::separator {
    background-color: %outlineVariant%;
    width: 1px;
    height: 1px;
    margin: 0;
}

QLabel {
    background-color: transparent;
    color: %onSurface%;
}

QLabel:disabled {
    color: %disabledText%;
}

QLabel[materialRole="overline"] {
    color: %onSurfaceVariant%;
    font-weight: 600;
}

QLabel[materialRole="mono"] {
    font-family: "%monoFont%";
    color: %onSurface%;
}

/* Honoured by the .ui files that already mark their section headings. */
*[title="true"] {
    color: %onSurfaceVariant%;
    font-weight: 600;
}

/* Both spellings: Qt converts enum properties to their number or their key
   depending on the version, and a rule that never matches costs nothing. */
QFrame[frameShape="4"],
QFrame[frameShape="HLine"] {
    background-color: %outlineVariant%;
    border: none;
    max-height: 1px;
}

QFrame[frameShape="5"],
QFrame[frameShape="VLine"] {
    background-color: %outlineVariant%;
    border: none;
    max-width: 1px;
}

QScrollArea,
QStackedWidget {
    background-color: transparent;
    border: none;
}

/* The widget a scroll area scrolls, so long pages sit on the surface. */
QScrollArea > QWidget > QWidget {
    background-color: transparent;
}

QAbstractScrollArea::corner {
    background-color: transparent;
    border: none;
}

QSplitter {
    background-color: transparent;
}

QSplitter::handle {
    background-color: transparent;
    image: none;
    border-radius: 3px;
}

QSplitter::handle:horizontal {
    width: 6px;
    margin: 10px 0;
}

QSplitter::handle:vertical {
    height: 6px;
    margin: 0 10px;
}

QSplitter::handle:hover {
    background-color: %outlineVariant%;
}

QSplitter::handle:pressed {
    background-color: %primary%;
}

QStatusBar {
    background-color: %surfaceContainer%;
    color: %onSurfaceVariant%;
    border-top: 1px solid %outlineVariant%;
    min-height: 28px;
}

QStatusBar::item {
    border: none;
}

QStatusBar QLabel {
    color: %onSurfaceVariant%;
    padding: 0 6px;
}

QToolBar {
    background-color: %surface%;
    border: none;
    padding: 6px 8px;
    spacing: 6px;
}

QToolBar::separator {
    background-color: %outlineVariant%;
    width: 1px;
    height: 1px;
    margin: 6px 8px;
}

QToolBar::handle {
    image: none;
    width: 0;
    height: 0;
}

QDockWidget {
    color: %onSurfaceVariant%;
    border: 1px solid %outlineVariant%;
    border-radius: %rMedium%px;
    font-weight: 600;
}

QDockWidget::title {
    background-color: %surfaceContainer%;
    color: %onSurfaceVariant%;
    padding: 8px 12px;
    border-top-left-radius: %rMedium%px;
    border-top-right-radius: %rMedium%px;
}
)CSS");
        }

        QString buttonFragment()
        {
            return QStringLiteral(R"CSS(
/* ----------------------------------------------------------------- buttons */

/* Stock buttons are the Material tonal fill; the dialog's default action is
   promoted to the high emphasis primary fill. */
QPushButton {
    background-color: %secondaryContainer%;
    color: %onSecondaryContainer%;
    border: none;
    border-radius: %btnR%px;
    padding: 0 20px;
    min-height: %btnH%px;
    min-width: 72px;
    font-weight: 600;
    outline: none;
}

QPushButton:hover {
    background-color: %tonalHover%;
}

QPushButton:pressed {
    background-color: %tonalPressed%;
}

QPushButton:default,
QPushButton:checked {
    background-color: %primary%;
    color: %onPrimary%;
}

QPushButton:default:hover,
QPushButton:checked:hover {
    background-color: %primaryHover%;
}

QPushButton:default:pressed,
QPushButton:checked:pressed {
    background-color: %primaryPressed%;
}

QPushButton:disabled {
    background-color: %disabledFill%;
    color: %disabledText%;
}

QPushButton::menu-indicator {
    subcontrol-origin: padding;
    subcontrol-position: right center;
    right: 10px;
    width: 12px;
}

/* Flat push buttons are the Material text button. */
QPushButton[flat="true"] {
    background-color: transparent;
    color: %primary%;
    padding: 0 12px;
    min-width: 0;
}

QPushButton[flat="true"]:hover {
    background-color: %flatHover%;
}

QPushButton[flat="true"]:pressed {
    background-color: %flatPressed%;
}

QPushButton[flat="true"]:checked {
    background-color: %secondaryContainer%;
    color: %onSecondaryContainer%;
}

QPushButton[flat="true"]:disabled {
    background-color: transparent;
    color: %disabledText%;
}

/* The outlined variant, for secondary actions inside a filled card. */
QPushButton[materialRole="outlined"] {
    background-color: transparent;
    color: %primary%;
    border: 1px solid %outline%;
}

QPushButton[materialRole="outlined"]:hover {
    background-color: %flatHover%;
    border-color: %primary%;
}

QPushButton[materialRole="outlined"]:disabled {
    background-color: transparent;
    border-color: %outlineVariant%;
    color: %disabledText%;
}

QToolButton {
    background-color: transparent;
    color: %onSurfaceVariant%;
    border: none;
    border-radius: %rMedium%px;
    padding: 6px;
    outline: none;
}

QToolButton:hover {
    background-color: %hover%;
    color: %onSurface%;
}

QToolButton:pressed {
    background-color: %hoverStrong%;
}

QToolButton:checked,
QToolButton:on {
    background-color: %secondaryContainer%;
    color: %onSecondaryContainer%;
}

QToolButton:disabled {
    background-color: transparent;
    color: %disabledText%;
}

/* MenuButtonPopup keeps room for the arrow. Declaring a sub-control makes Qt stop
   drawing it natively and paint whatever the rule says instead - so the fill, the
   border and the glyph all have to be spelled out here. Leaving them out is what
   turns these buttons into solid black rectangles. */
QToolButton[popupMode="1"] {
    padding-right: 20px;
}

QToolButton::menu-button {
    width: 16px;
    background-color: transparent;
    border: none;
    border-radius: 0;
}

QToolButton::menu-button:hover,
QToolButton::menu-button:pressed {
    background-color: transparent;
}

QToolButton::menu-arrow,
QToolButton::menu-indicator {
    image: url(:/material/expand_more.svg);
    width: 12px;
    height: 12px;
    subcontrol-origin: padding;
    subcontrol-position: center center;
}

QToolButton::menu-arrow:open {
    image: url(:/material/expand_less.svg);
}
)CSS");
        }

        QString inputFragment()
        {
            return QStringLiteral(R"CSS(
/* ------------------------------------------------------------------ inputs */

QLineEdit {
    background-color: %surfaceLowest%;
    color: %onSurface%;
    border: 1px solid %outline%;
    border-radius: %rLarge%px;
    padding: 0 14px;
    min-height: %btnH%px;
    selection-background-color: %primary%;
    selection-color: %onPrimary%;
}

QLineEdit:hover {
    border-color: %onSurfaceVariant%;
}

/* The focus border grows inward so the text does not jump by a pixel. */
QLineEdit:focus {
    border: 2px solid %primary%;
    padding: 0 13px;
}

QLineEdit[readOnly="true"] {
    background-color: %surfaceContainer%;
    border-color: %outlineVariant%;
}

QLineEdit:disabled {
    background-color: %disabledFill%;
    border-color: %outlineVariant%;
    color: %disabledText%;
}

QLineEdit[materialSeverity="error"],
QLineEdit[error="true"] {
    border-color: %error%;
    color: %error%;
}

QLineEdit[materialSeverity="error"]:focus,
QLineEdit[error="true"]:focus {
    border: 2px solid %error%;
    padding: 0 13px;
}

QPlainTextEdit,
QTextEdit,
QTextBrowser {
    background-color: %surfaceLowest%;
    color: %onSurface%;
    border: 1px solid %outline%;
    border-radius: %rLarge%px;
    padding: 8px 10px;
    selection-background-color: %primary%;
    selection-color: %onPrimary%;
}

QPlainTextEdit:hover,
QTextEdit:hover {
    border-color: %onSurfaceVariant%;
}

QPlainTextEdit:focus,
QTextEdit:focus {
    border: 2px solid %primary%;
    padding: 7px 9px;
}

QPlainTextEdit[readOnly="true"],
QTextEdit[readOnly="true"],
QTextBrowser {
    background-color: %surfaceLow%;
    border-color: %outlineVariant%;
}

QPlainTextEdit:disabled,
QTextEdit:disabled {
    background-color: %disabledFill%;
    border-color: %outlineVariant%;
    color: %disabledText%;
}

QPlainTextEdit[materialSeverity="error"],
QTextEdit[materialSeverity="error"] {
    border-color: %error%;
}

QComboBox {
    background-color: %surfaceLowest%;
    color: %onSurface%;
    border: 1px solid %outline%;
    border-radius: %rLarge%px;
    padding: 0 12px;
    min-height: %btnH%px;
    min-width: 96px;
}

QComboBox:hover {
    border-color: %onSurfaceVariant%;
}

QComboBox:focus,
QComboBox:on {
    border: 2px solid %primary%;
    padding: 0 11px;
}

QComboBox:disabled {
    background-color: %disabledFill%;
    border-color: %outlineVariant%;
    color: %disabledText%;
}

/* Geometry only. Qt draws either the sub-control rule or the arrow glyph, never
   both, so a background here would silently delete the arrow. Positioning it
   from the sheet and leaving the glyph to Material::Style keeps both. */
QComboBox::drop-down {
    subcontrol-origin: padding;
    subcontrol-position: center right;
    width: 28px;
}

/* An editable combo box embeds a QLineEdit; strip its own frame. */
QComboBox QLineEdit,
QAbstractSpinBox QLineEdit {
    background-color: transparent;
    border: none;
    border-radius: 0;
    padding: 0;
    min-height: 0;
}

QComboBox QAbstractItemView {
    background-color: %surfaceLow%;
    color: %onSurface%;
    border: 1px solid %outlineVariant%;
    border-radius: %rMedium%px;
    padding: 6px;
    outline: none;
    selection-background-color: %primaryContainer%;
    selection-color: %onPrimaryContainer%;
}

QComboBox QAbstractItemView::item {
    border-radius: %rSmall%px;
    padding: 6px 10px;
    min-height: 26px;
    color: %onSurface%;
}

QComboBox QAbstractItemView::item:hover {
    background-color: %surfaceHigh%;
}

QComboBox QAbstractItemView::item:selected {
    background-color: %primaryContainer%;
    color: %onPrimaryContainer%;
}

QAbstractSpinBox,
QSpinBox,
QDoubleSpinBox,
QDateEdit,
QTimeEdit,
QDateTimeEdit {
    background-color: %surfaceLowest%;
    color: %onSurface%;
    border: 1px solid %outline%;
    border-radius: %rLarge%px;
    padding: 0 4px 0 12px;
    min-height: %btnH%px;
    min-width: 90px;
    selection-background-color: %primary%;
    selection-color: %onPrimary%;
}

QAbstractSpinBox:hover {
    border-color: %onSurfaceVariant%;
}

QAbstractSpinBox:focus {
    border: 2px solid %primary%;
    padding: 0 3px 0 11px;
}

QAbstractSpinBox:disabled {
    background-color: %disabledFill%;
    border-color: %outlineVariant%;
    color: %disabledText%;
}

/* The step buttons stack inside the field. Only their geometry lives here: a
   fill or a radius on a step button replaces the arrow instead of sitting
   behind it, so the rounded tile is left to Material::Style. */
QAbstractSpinBox::up-button {
    subcontrol-origin: padding;
    subcontrol-position: top right;
    width: 24px;
    height: 15px;
    margin: 4px 6px 0 0;
}

QAbstractSpinBox::down-button {
    subcontrol-origin: padding;
    subcontrol-position: bottom right;
    width: 24px;
    height: 15px;
    margin: 0 6px 4px 0;
}
)CSS");
        }

        QString selectionControlFragment()
        {
            return QStringLiteral(R"CSS(
/* ------------------------------------------------------- selection controls */

/* No background here: a drawable rule on the widget takes the whole control
   away from the style, which then loses the box around the tick. */
QCheckBox,
QRadioButton {
    color: %onSurface%;
    spacing: 10px;
    padding: 2px 0;
}

QCheckBox:disabled,
QRadioButton:disabled {
    color: %disabledText%;
}

/* The box, the tick and the dot are one glyph that Material::Style paints from
   the theme roles. A border or a fill here would replace that glyph with a
   plain rectangle, so the sheet contributes the Material sizing only. */
QCheckBox::indicator,
QRadioButton::indicator {
    width: 18px;
    height: 18px;
}

QGroupBox::indicator,
QAbstractItemView::indicator {
    width: 18px;
    height: 18px;
}

QSlider {
    background-color: transparent;
    min-height: 24px;
}

QSlider::groove:horizontal {
    height: 4px;
    border: none;
    border-radius: 2px;
    background-color: %surfaceHighest%;
}

QSlider::sub-page:horizontal {
    height: 4px;
    border-radius: 2px;
    background-color: %primary%;
}

QSlider::add-page:horizontal {
    height: 4px;
    border-radius: 2px;
    background-color: %surfaceHighest%;
}

QSlider::handle:horizontal {
    width: 20px;
    height: 20px;
    margin: -8px 0;
    border: none;
    border-radius: 10px;
    background-color: %primary%;
}

QSlider::groove:vertical {
    width: 4px;
    border: none;
    border-radius: 2px;
    background-color: %surfaceHighest%;
}

QSlider::sub-page:vertical {
    width: 4px;
    border-radius: 2px;
    background-color: %surfaceHighest%;
}

QSlider::add-page:vertical {
    width: 4px;
    border-radius: 2px;
    background-color: %primary%;
}

QSlider::handle:vertical {
    width: 20px;
    height: 20px;
    margin: 0 -8px;
    border: none;
    border-radius: 10px;
    background-color: %primary%;
}

QSlider::handle:hover {
    background-color: %primaryHover%;
}

QSlider::handle:pressed {
    background-color: %primaryPressed%;
}

QSlider::handle:disabled {
    background-color: %disabledText%;
}

QSlider::sub-page:disabled,
QSlider::add-page:vertical:disabled {
    background-color: %disabledFill%;
}

QProgressBar {
    background-color: %surfaceHighest%;
    color: %onSurfaceVariant%;
    border: none;
    border-radius: 3px;
    min-height: 6px;
    max-height: 6px;
    text-align: center;
}

QProgressBar::chunk {
    background-color: %primary%;
    border-radius: 3px;
}

/* A progress bar that shows its percentage needs room for the text. */
QProgressBar[textVisible="true"] {
    background-color: %surfaceHigh%;
    min-height: 20px;
    max-height: 20px;
    border-radius: 10px;
}

QProgressBar[textVisible="true"]::chunk {
    border-radius: 10px;
}

QProgressBar:disabled {
    background-color: %disabledFill%;
}

QProgressBar::chunk:disabled {
    background-color: %disabledText%;
}
)CSS");
        }

        QString scrollBarFragment()
        {
            return QStringLiteral(R"CSS(
/* ------------------------------------------------------------- scroll bars */

QScrollBar:vertical {
    background-color: transparent;
    border: none;
    width: 10px;
    margin: 0;
}

QScrollBar:horizontal {
    background-color: transparent;
    border: none;
    height: 10px;
    margin: 0;
}

QScrollBar::handle:vertical {
    background-color: %outlineVariant%;
    border: none;
    border-radius: 5px;
    min-height: 36px;
}

QScrollBar::handle:horizontal {
    background-color: %outlineVariant%;
    border: none;
    border-radius: 5px;
    min-width: 36px;
}

QScrollBar::handle:hover {
    background-color: %outline%;
}

QScrollBar::handle:pressed {
    background-color: %primary%;
}

QScrollBar::add-line,
QScrollBar::sub-line {
    background-color: transparent;
    border: none;
    width: 0;
    height: 0;
}

QScrollBar::add-page,
QScrollBar::sub-page {
    background-color: transparent;
}

QScrollBar::up-arrow,
QScrollBar::down-arrow,
QScrollBar::left-arrow,
QScrollBar::right-arrow {
    background-color: transparent;
    border: none;
    width: 0;
    height: 0;
}
)CSS");
        }

        QString tabFragment()
        {
            return QStringLiteral(R"CSS(
/* -------------------------------------------------------------------- tabs */

QTabWidget {
    background-color: transparent;
}

QTabWidget::pane {
    background-color: %surface%;
    border: none;
    border-top: 1px solid %outlineVariant%;
    top: -1px;
}

QTabWidget::tab-bar {
    alignment: left;
    left: 4px;
}

QTabBar {
    background-color: transparent;
    qproperty-drawBase: 0;
    outline: none;
}

QTabBar::tab {
    background-color: transparent;
    color: %onSurfaceVariant%;
    border: none;
    border-top-left-radius: %rMedium%px;
    border-top-right-radius: %rMedium%px;
    padding: 0 16px;
    margin-right: 2px;
    min-height: %tabH%px;
}

QTabBar::tab:hover {
    background-color: %surfaceHigh%;
    color: %onSurface%;
}

QTabBar::tab:selected {
    background-color: %surface%;
    color: %primary%;
    font-weight: 600;
}

QTabBar::tab:disabled {
    color: %disabledText%;
}

QTabBar::tab:left,
QTabBar::tab:right {
    border-radius: %rMedium%px;
    padding: 12px 0;
    margin: 2px 4px;
    min-width: %tabH%px;
}

QTabBar QToolButton {
    background-color: %surfaceContainer%;
    border: none;
    border-radius: %rSmall%px;
    margin: 4px 2px;
}

QTabBar QToolButton:hover {
    background-color: %surfaceHigh%;
}
)CSS");
        }

        QString itemViewFragment()
        {
            return QStringLiteral(R"CSS(
/* -------------------------------------------------------------- item views */

QHeaderView {
    background-color: transparent;
    border: none;
}

QHeaderView::section {
    background-color: %surfaceContainer%;
    color: %onSurfaceVariant%;
    border: none;
    padding: 6px 10px;
    min-height: 26px;
    font-weight: 600;
}

QHeaderView::section:hover {
    background-color: %surfaceHigh%;
    color: %onSurface%;
}

QHeaderView::section:first {
    border-top-left-radius: %rSmall%px;
    border-bottom-left-radius: %rSmall%px;
}

QHeaderView::section:last {
    border-top-right-radius: %rSmall%px;
    border-bottom-right-radius: %rSmall%px;
}

QHeaderView::section:only-one {
    border-radius: %rSmall%px;
}

QTableCornerButton::section {
    background-color: %surfaceContainer%;
    border: none;
}

QAbstractItemView {
    background-color: transparent;
    alternate-background-color: %surfaceLow%;
    color: %onSurface%;
    border: none;
    outline: none;
    show-decoration-selected: 1;
    selection-background-color: %secondaryContainer%;
    selection-color: %onSecondaryContainer%;
}

QTreeView,
QListView,
QTableView,
QColumnView {
    background-color: transparent;
    border: none;
}

QTableView {
    gridline-color: %outlineVariant%;
}

QTreeView::item,
QListView::item,
QTableView::item,
QColumnView::item {
    background-color: transparent;
    color: %onSurface%;
    border: none;
    border-radius: %rRow%px;
    padding: 4px 8px;
    min-height: %rowInner%px;
}

QTreeView::item:hover,
QListView::item:hover,
QTableView::item:hover,
QColumnView::item:hover {
    background-color: %surfaceHigh%;
}

QTreeView::item:selected,
QListView::item:selected,
QTableView::item:selected,
QColumnView::item:selected {
    background-color: %secondaryContainer%;
    color: %onSecondaryContainer%;
}

QTreeView::item:selected:hover,
QListView::item:selected:hover,
QTableView::item:selected:hover,
QColumnView::item:selected:hover {
    background-color: %secondaryContainer%;
    color: %onSecondaryContainer%;
}

QTreeView::item:disabled,
QListView::item:disabled,
QTableView::item:disabled {
    color: %disabledText%;
}
)CSS");
        }

        QString menuFragment()
        {
            return QStringLiteral(R"CSS(
/* --------------------------------------------------------- menus, tooltips */

QMenuBar {
    background-color: %surface%;
    color: %onSurface%;
    border: none;
    padding: 2px 6px;
}

QMenuBar::item {
    background-color: transparent;
    color: %onSurface%;
    padding: 6px 12px;
    border-radius: %rSmall%px;
}

QMenuBar::item:selected {
    background-color: %surfaceHigh%;
}

QMenuBar::item:pressed {
    background-color: %primaryContainer%;
    color: %onPrimaryContainer%;
}

QMenuBar::item:disabled {
    color: %disabledText%;
}

QMenu {
    background-color: %surfaceLow%;
    color: %onSurface%;
    border: 1px solid %outlineVariant%;
    border-radius: %rMedium%px;
    padding: 8px;
}

QMenu::item {
    background-color: transparent;
    color: %onSurface%;
    border-radius: %rSmall%px;
    padding: 8px 28px 8px 30px;
    min-height: 22px;
}

QMenu::item:selected {
    background-color: %primaryContainer%;
    color: %onPrimaryContainer%;
}

QMenu::item:disabled {
    background-color: transparent;
    color: %disabledText%;
}

QMenu::separator {
    background-color: %outlineVariant%;
    height: 1px;
    margin: 6px 10px;
}

QMenu::icon {
    padding-left: 12px;
}

/* Size only, so the check mark keeps being drawn by the style. */
QMenu::indicator {
    width: 16px;
    height: 16px;
    margin-left: 8px;
}

QMenu QLineEdit,
QMenu QCheckBox {
    margin: 2px 6px;
}

QToolTip {
    background-color: %inverseSurface%;
    color: %inverseOnSurface%;
    border: none;
    border-radius: %rSmall%px;
    padding: 6px 10px;
    opacity: 240;
}
)CSS");
        }

        QString containerFragment()
        {
            return QStringLiteral(R"CSS(
/* -------------------------------------------------- group boxes, calendars */

/* An outlined 28px card with the legend inset over its border. */
QGroupBox {
    background-color: transparent;
    color: %onSurface%;
    border: 1px solid %outlineVariant%;
    border-radius: %rXL%px;
    margin-top: 12px;
    padding: 18px 16px 16px 16px;
    font-weight: 600;
}

QGroupBox::title {
    subcontrol-origin: margin;
    subcontrol-position: top left;
    left: 20px;
    padding: 0 6px;
    background-color: %surface%;
    color: %onSurfaceVariant%;
}

QGroupBox:disabled {
    border-color: %disabledFill%;
    color: %disabledText%;
}

QGroupBox[flat="true"] {
    border: none;
    padding: 8px 0 0 0;
}

QCalendarWidget {
    background-color: %surfaceLowest%;
    border: 1px solid %outlineVariant%;
    border-radius: %rMedium%px;
}

QCalendarWidget QWidget#qt_calendar_navigationbar {
    background-color: %surfaceContainer%;
    border-top-left-radius: %rMedium%px;
    border-top-right-radius: %rMedium%px;
    min-height: 40px;
}

QCalendarWidget QToolButton {
    background-color: transparent;
    color: %onSurface%;
    border: none;
    border-radius: %rSmall%px;
    padding: 4px 10px;
    margin: 4px 2px;
}

QCalendarWidget QToolButton:hover {
    background-color: %hover%;
}

QCalendarWidget QSpinBox {
    min-height: 28px;
    min-width: 60px;
}

QCalendarWidget QAbstractItemView {
    background-color: %surfaceLowest%;
    color: %onSurface%;
    outline: none;
    selection-background-color: %primaryContainer%;
    selection-color: %onPrimaryContainer%;
}

QCalendarWidget QAbstractItemView:disabled {
    color: %disabledText%;
}

/* ------------------------------------------------------ dialogs, messages */

QMessageBox {
    background-color: %surfaceLowest%;
}

QMessageBox QLabel {
    color: %onSurface%;
    padding: 4px 0;
}

QMessageBox QPushButton,
QDialogButtonBox QPushButton {
    min-width: 96px;
    min-height: %btnH%px;
}

QDialogButtonBox {
    dialogbuttonbox-buttons-have-icons: 0;
}

QInputDialog QLineEdit {
    min-width: 260px;
}
)CSS");
        }

        /**
         * Rules keyed on the `materialRole` property, so the widget library can
         * restyle a plain container without a local stylesheet.
         */
        QString roleFragment()
        {
            return QStringLiteral(R"CSS(
/* --------------------------------------------------------- material roles */

QWidget[materialRole="surface"] {
    background-color: %surface%;
    color: %onSurface%;
    border: none;
}

QWidget[materialRole="card"] {
    background-color: %surfaceLowest%;
    color: %onSurface%;
    border: 1px solid %outlineVariant%;
    border-radius: %rXL%px;
    padding: %pad%px;
}

QWidget[materialRole="rail"] {
    background-color: %surfaceLow%;
    border: none;
    border-right: 1px solid %outlineVariant%;
    min-width: %railW%px;
    max-width: %railW%px;
}

QWidget[materialRole="appbar"] {
    background-color: %surface%;
    border: none;
    border-bottom: 1px solid %outlineVariant%;
    min-height: %appBarH%px;
}

QWidget[materialRole="detail"] {
    background-color: %surfaceLow%;
    border: none;
    border-left: 1px solid %outlineVariant%;
    min-width: %detailW%px;
    max-width: %detailW%px;
}

/* The scrim behind a sheet: black at 32 percent. */
QWidget[materialRole="overlay"] {
    background-color: rgba(0, 0, 0, 82);
    border: none;
}

QWidget[materialRole="sheet"] {
    background-color: %surfaceLowest%;
    color: %onSurface%;
    border: 1px solid %outlineVariant%;
    border-radius: %rXL%px;
}

QWidget[materialRole="pill"] {
    background-color: %secondaryContainer%;
    color: %onSecondaryContainer%;
    border: none;
    border-radius: %chipR%px;
    padding: 0 14px;
    min-height: %chipH%px;
}

QAbstractButton[materialRole="pill"]:hover {
    background-color: %tonalHover%;
}

QAbstractButton[materialRole="pill"]:pressed {
    background-color: %tonalPressed%;
}

QAbstractButton[materialRole="pill"]:checked {
    background-color: %primary%;
    color: %onPrimary%;
}

QAbstractButton[materialRole="pill"]:disabled {
    background-color: %disabledFill%;
    color: %disabledText%;
}
)CSS");
        }

        /**
         * The three status families. Labels tint their text, containers tint
         * their fill, and the controls that carry a status follow along.
         */
        QString severityFragment(const QString& severity,
                                 const QString& accent,
                                 const QString& container,
                                 const QString& onContainer)
        {
            return QStringLiteral(R"CSS(
QFrame[materialSeverity="%1"],
QWidget[materialSeverity="%1"][materialRole="card"] {
    background-color: %3;
    color: %4;
    border: 1px solid %2;
    border-radius: %rMedium%px;
}

/* After the frame rule on purpose: a QLabel is a QFrame, and a status label
   tints its text instead of turning into a filled block. */
QLabel[materialSeverity="%1"] {
    background-color: transparent;
    border: none;
    color: %2;
    font-weight: 600;
}

QPushButton[materialSeverity="%1"] {
    background-color: %3;
    color: %4;
}

QPushButton[materialSeverity="%1"]:hover {
    background-color: %2;
    color: %3;
}

QProgressBar[materialSeverity="%1"]::chunk {
    background-color: %2;
}

QWidget[materialSeverity="%1"][materialRole="pill"] {
    background-color: %3;
    color: %4;
}
)CSS")
                .arg(severity, accent, container, onContainer);
        }

        /**
         * The KeePassXC widgets that used to carry their own look. Everything
         * here replaces a local setStyleSheet() call somewhere in src/gui.
         */
        QString keepassFragment()
        {
            return QStringLiteral(R"CSS(
/* ------------------------------------------------------ KeePassXC widgets */

KMessageWidget,
MessageWidget {
    background-color: transparent;
    color: %onSurface%;
    border: none;
}

KMessageWidget QFrame,
MessageWidget QFrame {
    background-color: %surfaceHigh%;
    color: %onSurface%;
    border: 1px solid %outlineVariant%;
    border-radius: %rMedium%px;
    padding: 8px 12px;
}

KMessageWidget QLabel,
MessageWidget QLabel {
    background-color: transparent;
    color: %onSurface%;
}

KMessageWidget QToolButton,
MessageWidget QToolButton {
    background-color: transparent;
    border: none;
    border-radius: %rSmall%px;
    padding: 4px;
}

/* MessageType: Positive, Information, Warning, Error. */
KMessageWidget[messageType="0"] QFrame,
KMessageWidget[messageType="Positive"] QFrame,
MessageWidget[messageType="0"] QFrame,
MessageWidget[messageType="Positive"] QFrame {
    background-color: %greenContainer%;
    color: %onGreenContainer%;
    border-color: %green%;
}

KMessageWidget[messageType="1"] QFrame,
KMessageWidget[messageType="Information"] QFrame,
MessageWidget[messageType="1"] QFrame,
MessageWidget[messageType="Information"] QFrame {
    background-color: %primaryContainer%;
    color: %onPrimaryContainer%;
    border-color: %primary%;
}

KMessageWidget[messageType="2"] QFrame,
KMessageWidget[messageType="Warning"] QFrame,
MessageWidget[messageType="2"] QFrame,
MessageWidget[messageType="Warning"] QFrame {
    background-color: %amberContainer%;
    color: %onAmberContainer%;
    border-color: %amber%;
}

KMessageWidget[messageType="3"] QFrame,
KMessageWidget[messageType="Error"] QFrame,
MessageWidget[messageType="3"] QFrame,
MessageWidget[messageType="Error"] QFrame {
    background-color: %errorContainer%;
    color: %onErrorContainer%;
    border-color: %error%;
}

EntryPreviewWidget {
    background-color: %surfaceLow%;
    border: none;
}

EntryPreviewWidget QTabWidget::pane {
    background-color: transparent;
    border: none;
    border-top: 1px solid %outlineVariant%;
}

EntryPreviewWidget QTabBar::tab:selected {
    background-color: transparent;
}

EntryPreviewWidget #entryTitleLabel,
EntryPreviewWidget #groupTitleLabel {
    color: %onSurface%;
    font-weight: 600;
}

EntryPreviewWidget #entryTotpLabel {
    font-family: "%monoFont%";
    color: %primary%;
}

EntryPreviewWidget #entryTotpProgress {
    min-height: 4px;
    max-height: 4px;
    border-radius: 2px;
}

EntryPreviewWidget #entryNotesTextEdit,
EntryPreviewWidget #groupNotesTextEdit {
    background-color: %surfaceLowest%;
    border: 1px solid %outlineVariant%;
    border-radius: %rRow%px;
    padding: 10px 12px;
}

EntryPreviewWidget #entryUsernameLabel {
    background-color: transparent;
    border: none;
    border-radius: 0;
    padding: 0;
    min-height: 0;
}

/* Kept from the previous theme: fields that must disappear into the pane. */
EntryPreviewWidget *[blendIn="true"] {
    background-color: transparent;
    border: none;
    padding-left: 0;
}

PasswordWidget QLineEdit {
    border-radius: %rLarge%px;
    font-family: "%monoFont%";
}

PasswordWidget QProgressBar {
    background-color: %surfaceHighest%;
    min-height: 6px;
    max-height: 6px;
    border-radius: 3px;
}

PasswordWidget QToolButton {
    background-color: transparent;
    border: none;
    border-radius: %rMedium%px;
    padding: 4px;
}

PasswordGeneratorWidget {
    background-color: %surfaceLowest%;
}

PasswordGeneratorWidget #editNewPassword QLineEdit {
    font-family: "%monoFont%";
    min-height: %searchH%px;
    border-radius: %rRail%px;
}

PasswordGeneratorWidget #entropyProgressBar {
    min-height: 6px;
    max-height: 6px;
    border-radius: 3px;
}

/* The charset toggles are checkable push buttons: draw them as filter chips. */
PasswordGeneratorWidget QPushButton[checkable="true"] {
    background-color: transparent;
    color: %onSurfaceVariant%;
    border: 1px solid %outline%;
    border-radius: %chipR%px;
    padding: 0 12px;
    min-width: 40px;
    min-height: %chipH%px;
    font-weight: 500;
}

PasswordGeneratorWidget QPushButton[checkable="true"]:hover {
    background-color: %hover%;
    color: %onSurface%;
}

PasswordGeneratorWidget QPushButton[checkable="true"]:checked {
    background-color: %secondaryContainer%;
    color: %onSecondaryContainer%;
    border-color: transparent;
}

PasswordGeneratorWidget #buttonGenerate,
PasswordGeneratorWidget #buttonCopy {
    min-width: 40px;
}

WelcomeWidget {
    background-color: %surface%;
}

WelcomeWidget #welcomeLabel {
    color: %onSurface%;
    font-weight: 600;
}

WelcomeWidget #startLabel,
WelcomeWidget #recentLabel {
    color: %onSurfaceVariant%;
}

WelcomeWidget #recentListWidget {
    background-color: %surfaceLowest%;
    border: 1px solid %outlineVariant%;
    border-radius: %rXL%px;
    padding: 8px;
}

WelcomeWidget #recentListWidget::item {
    border-radius: %rRow%px;
    padding: 6px 10px;
    min-height: %rowInner%px;
}

SearchWidget QLineEdit {
    background-color: %surfaceHigh%;
    border: 1px solid transparent;
    border-radius: %searchR%px;
    padding: 0 16px;
    min-height: %searchH%px;
    min-width: 220px;
}

SearchWidget QLineEdit:hover {
    background-color: %surfaceHighest%;
    border-color: %outlineVariant%;
}

SearchWidget QLineEdit:focus {
    background-color: %surfaceLowest%;
    border: 2px solid %primary%;
    padding: 0 15px;
}

SearchWidget QToolButton {
    background-color: transparent;
    border: none;
    border-radius: %rMedium%px;
}

DatabaseTabWidget::pane {
    background-color: %surface%;
    border: none;
}

DatabaseTabWidget > QTabBar {
    background-color: %surfaceLow%;
    min-height: %tabStripH%px;
}

DatabaseTabWidget > QTabBar::tab {
    background-color: %surfaceLow%;
    color: %onSurfaceVariant%;
    border-top-left-radius: %rMedium%px;
    border-top-right-radius: %rMedium%px;
    padding: 0 12px;
    margin: 5px 2px 0 0;
    min-height: %tabH%px;
    min-width: 132px;
    max-width: 240px;
}

DatabaseTabWidget > QTabBar::tab:hover {
    background-color: %surfaceHigh%;
    color: %onSurface%;
}

DatabaseTabWidget > QTabBar::tab:selected {
    background-color: %surface%;
    color: %onSurface%;
}

EditWidget {
    background-color: %surface%;
}

EditWidget #headerLabel {
    color: %onSurface%;
    font-weight: 600;
    padding: 4px 0 12px 0;
}

EditWidget #stackedWidget {
    background-color: transparent;
}

CategoryListWidget {
    background-color: %surfaceLow%;
    border: none;
    border-right: 1px solid %outlineVariant%;
}

CategoryListWidget #categoryList {
    background-color: transparent;
    border: none;
    outline: none;
    padding: 6px;
}

CategoryListWidget #categoryList::item {
    border-radius: %rRow%px;
    padding: 8px 10px;
    min-height: %rowInner%px;
    color: %onSurfaceVariant%;
}

CategoryListWidget #categoryList::item:hover {
    background-color: %surfaceHigh%;
    color: %onSurface%;
}

CategoryListWidget #categoryList::item:selected {
    background-color: %secondaryContainer%;
    color: %onSecondaryContainer%;
}

CategoryListWidget QToolButton {
    background-color: transparent;
    border: none;
    border-radius: %rSmall%px;
    padding: 2px;
}

CategoryListWidget QToolButton:hover {
    background-color: %hover%;
}
)CSS");
        }

    } // namespace

    QString buildStyleSheet(const Theme& theme)
    {
        const QStringList fragments = {baseFragment(),
                                       buttonFragment(),
                                       inputFragment(),
                                       selectionControlFragment(),
                                       scrollBarFragment(),
                                       tabFragment(),
                                       itemViewFragment(),
                                       menuFragment(),
                                       containerFragment(),
                                       roleFragment(),
                                       severityFragment(QStringLiteral("error"),
                                                        QStringLiteral("%error%"),
                                                        QStringLiteral("%errorContainer%"),
                                                        QStringLiteral("%onErrorContainer%")),
                                       severityFragment(QStringLiteral("warning"),
                                                        QStringLiteral("%amber%"),
                                                        QStringLiteral("%amberContainer%"),
                                                        QStringLiteral("%onAmberContainer%")),
                                       severityFragment(QStringLiteral("success"),
                                                        QStringLiteral("%green%"),
                                                        QStringLiteral("%greenContainer%"),
                                                        QStringLiteral("%onGreenContainer%")),
                                       keepassFragment()};

        return substitute(fragments.join(QLatin1Char('\n')), paletteTokens(theme));
    }

} // namespace Material
