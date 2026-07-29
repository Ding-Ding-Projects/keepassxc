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

#ifndef KEEPASSXC_MATERIALSEARCHBAR_H
#define KEEPASSXC_MATERIALSEARCHBAR_H

#include <QString>
#include <QWidget>

class QLineEdit;

namespace Material
{
    class Chip;
    class IconButton;

    /**
     * The pill search field.
     *
     * A leading search glyph, a borderless line edit, then the optional Regex
     * toggle chip and the builder button that opens the regex overlay. The
     * Prominent variant is the 52px bar above the entry list; the Surface
     * variant is the 44px bar the report, history, changelog and settings
     * screens put under their headline.
     *
     * The widget is a container, not a QLineEdit: it forwards focus to the
     * input and exposes it through lineEdit() for completers and validators.
     */
    class SearchBar : public QWidget
    {
        Q_OBJECT

    public:
        enum class Variant
        {
            Prominent, // Layout::SearchBarHeight, surfaceContainerHigh
            Surface // Layout::SurfaceSearchHeight, surfaceContainer
        };

        explicit SearchBar(QWidget* parent = nullptr);
        explicit SearchBar(Variant variant, QWidget* parent = nullptr);
        ~SearchBar() override;

        Variant variant() const;
        void setVariant(Variant variant);

        QString text() const;
        void setText(const QString& text);
        void clear();

        void setPlaceholder(const QString& placeholder);
        QString placeholder() const;

        bool isRegexEnabled() const;
        void setRegexEnabled(bool enabled);

        /** Show or hide the Regex chip and the builder button; shown by default. */
        bool showRegexControls() const;
        void setShowRegexControls(bool show);

        QLineEdit* lineEdit() const;

        QSize sizeHint() const override;
        QSize minimumSizeHint() const override;

    signals:
        void textChanged(const QString& text);
        void regexToggled(bool enabled);
        void builderRequested();
        void returnPressed();

    protected:
        void paintEvent(QPaintEvent* event) override;
        void resizeEvent(QResizeEvent* event) override;

    private:
        void applyTheme();

        QLineEdit* m_lineEdit = nullptr;
        Chip* m_regexChip = nullptr;
        IconButton* m_builderButton = nullptr;
        Variant m_variant = Variant::Prominent;
        bool m_showRegexControls = true;
    };

} // namespace Material

#endif // KEEPASSXC_MATERIALSEARCHBAR_H
