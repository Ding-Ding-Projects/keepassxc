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

#ifndef KEEPASSXC_MATERIALAPPEARANCEEDITOR_H
#define KEEPASSXC_MATERIALAPPEARANCEEDITOR_H

#include "MaterialElementOverrides.h"

#include <QFont>
#include <QHash>
#include <QPointer>
#include <QString>
#include <QWidget>

#include <functional>
#include <optional>

class QAbstractButton;
class QLabel;
class QLineEdit;
class QStackedWidget;
class QTimer;

namespace Material
{
    class ColorPicker;
    class FilledButton;
    class IconButton;
    class OutlinedButton;
    class SearchBar;
    class SegmentedButton;
    class Select;
    class Slider;
    class Switch;
    class TextButton;

    /**
     * Applies element overrides to live widgets: font, colours, shape,
     * spacing, border, elevation, opacity, and the animated rainbow. One
     * applier per application, driven by ElementOverrides::overrideChanged and
     * by widgets appearing, so a customised element looks customised wherever
     * and whenever it is shown.
     *
     * The rainbow is animated here through one shared timer whose period is
     * the level's cycle length, so every rainbow surface turns together; with
     * reduced motion the hue settles on one value instead of cycling.
     */
    class AppearanceApplier : public QObject
    {
        Q_OBJECT

    public:
        static AppearanceApplier* instance();

        /** The override key a widget answers to: its object name. */
        static QString keyFor(const QWidget* widget);
        /** Apply @p key's override (or clear it) to every matching live widget. */
        void apply(const QString& key);
        void applyTo(QWidget* widget);
        /** The hue currently painted for rainbow surfaces, 0..1. */
        double rainbowHue() const;
        bool reducedMotion() const;
        void setReducedMotion(bool reduced);

    protected:
        bool eventFilter(QObject* watched, QEvent* event) override;

    private:
        AppearanceApplier();
        void tick();
        QString styleFor(const ElementOverrides::Override& value, const QString& key) const;

        QTimer* m_rainbowTimer = nullptr;
        double m_hue = 0.0;
        bool m_reducedMotion = false;
        int m_cycleMs = 10000;
        QHash<QString, QFont> m_baseFonts;
    };

    /**
     * The per-element appearance editor: a non-modal, anchored, resizable
     * panel opened from any element's Edit appearance… command (or
     * Shift+right-click, or Ctrl+Shift+E on the focused element). Tabs:
     *
     *  - Typography: family (a searchable select with each face as a sample),
     *    size (slider plus free entry), weight, italic, underline,
     *    strikethrough, overline, capitalization, letter spacing, line height.
     *  - Colour: background and foreground through the infinite colour
     *    picker with its translator and contrast readout, plus the rainbow.
     *  - Shape & layout: corner radius, element height, spacing, border width
     *    and colour, elevation, opacity.
     *  - Presets: save the current element's style under a name, apply a saved
     *    preset, copy and paste a style between elements, export and import
     *    every preset as JSON.
     *
     * Every change writes ElementOverrides immediately and the live element
     * follows; Reset returns one element or everything to the shipped look.
     * The panel has its own property search with an anchored regex builder.
     */
    class AppearanceEditor : public QWidget
    {
        Q_OBJECT

    public:
        explicit AppearanceEditor(QWidget* parent = nullptr);
        ~AppearanceEditor() override;

        /** The one editor, created on demand and reused for every element. */
        static AppearanceEditor* instance();

        /** Open (or retarget) the editor for @p target, beside it. */
        void editElement(QWidget* target, const QString& key = QString());
        QString currentKey() const;
        QWidget* currentTarget() const;
        void setCurrentTab(const QString& id);
        QString currentTab() const;

        SearchBar* propertySearch() const;
        ColorPicker* backgroundPicker() const;
        ColorPicker* foregroundPicker() const;
        Select* fontFamily() const;
        Slider* fontSize() const;
        QLineEdit* fontSizeEntry() const;
        SegmentedButton* fontWeight() const;
        Switch* italic() const;
        Switch* underline() const;
        Switch* strikeout() const;
        Switch* overline() const;
        Select* capitalization() const;
        Slider* letterSpacing() const;
        Slider* lineHeight() const;
        Slider* radius() const;
        Slider* height() const;
        Slider* spacing() const;
        Slider* borderWidth() const;
        Slider* elevation() const;
        Slider* opacity() const;
        QAbstractButton* resetElementButton() const;
        QAbstractButton* resetAllButton() const;
        QAbstractButton* copyStyleButton() const;
        QAbstractButton* pasteStyleButton() const;
        QAbstractButton* savePresetButton() const;
        Select* presetSelect() const;
        QAbstractButton* applyPresetButton() const;
        QLineEdit* presetName() const;

        /** Presets as the JSON document the export writes and the import reads. */
        QString exportPresets() const;
        bool importPresets(const QString& json, QString* error = nullptr);
        QStringList presetNames() const;

        /** Property rows matching the search, by object name, for the tests. */
        QStringList visiblePropertyRows() const;

    public slots:
        void closeEditor();

    signals:
        void targetChanged(const QString& key);

    protected:
        void keyPressEvent(QKeyEvent* event) override;
        void paintEvent(QPaintEvent* event) override;
        bool eventFilter(QObject* watched, QEvent* event) override;
        void showEvent(QShowEvent* event) override;
        void hideEvent(QHideEvent* event) override;

    private:
        class PropertyRow;

        void buildUi();
        QWidget* buildTypographyPage();
        QWidget* buildColourPage();
        QWidget* buildShapePage();
        QWidget* buildPresetsPage();
        PropertyRow* addRow(QWidget* page, const QString& id, const QString& label, QWidget* control, const QString& keywords = {});
        void loadFromOverride();
        void writeOverride(const std::function<void(ElementOverrides::Override&)>& change);
        void applyFilter();
        void placeBesideTarget();
        void applyTheme();
        void loadPresets();
        void savePresets() const;
        void refreshPresetSelect();
        ElementOverrides::Override currentOverride() const;

        QPointer<QWidget> m_target;
        QString m_key;
        bool m_updating = false;
        QLabel* m_title = nullptr;
        QLabel* m_subtitle = nullptr;
        IconButton* m_close = nullptr;
        SearchBar* m_search = nullptr;
        SegmentedButton* m_tabs = nullptr;
        QStackedWidget* m_pages = nullptr;
        QList<PropertyRow*> m_rows;

        Select* m_fontFamily = nullptr;
        Slider* m_fontSize = nullptr;
        QLineEdit* m_fontSizeEntry = nullptr;
        SegmentedButton* m_fontWeight = nullptr;
        Switch* m_italic = nullptr;
        Switch* m_underline = nullptr;
        Switch* m_strikeout = nullptr;
        Switch* m_overline = nullptr;
        Select* m_capitalization = nullptr;
        Slider* m_letterSpacing = nullptr;
        Slider* m_lineHeight = nullptr;
        ColorPicker* m_background = nullptr;
        ColorPicker* m_foreground = nullptr;
        Slider* m_radius = nullptr;
        Slider* m_height = nullptr;
        Slider* m_spacing = nullptr;
        Slider* m_borderWidth = nullptr;
        ColorPicker* m_borderColor = nullptr;
        Slider* m_elevation = nullptr;
        Slider* m_opacity = nullptr;
        OutlinedButton* m_resetElement = nullptr;
        TextButton* m_resetAll = nullptr;
        OutlinedButton* m_copyStyle = nullptr;
        OutlinedButton* m_pasteStyle = nullptr;
        QLineEdit* m_presetName = nullptr;
        FilledButton* m_savePreset = nullptr;
        Select* m_presetSelect = nullptr;
        OutlinedButton* m_applyPreset = nullptr;
        OutlinedButton* m_deletePreset = nullptr;
        OutlinedButton* m_exportPresets = nullptr;
        OutlinedButton* m_importPresets = nullptr;
        QLabel* m_presetStatus = nullptr;
        QHash<QString, ElementOverrides::Override> m_presets;
        std::optional<ElementOverrides::Override> m_clipboard;
        QPointer<QWidget> m_returnFocus;
    };

} // namespace Material

#endif // KEEPASSXC_MATERIALAPPEARANCEEDITOR_H
