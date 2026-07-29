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

#ifndef KEEPASSXC_MATERIALCARD_H
#define KEEPASSXC_MATERIALCARD_H

#include "MaterialTheme.h"

#include <QFrame>

class QLabel;
class QVBoxLayout;

namespace Material
{
    /**
     * The container every screen is built from.
     *
     * Filled cards sit on a surface container role, Outlined cards are
     * transparent with a hairline outlineVariant border, Elevated cards are
     * filled and carry a drop shadow. The radius is the design's shape scale:
     * 28 for section and stat cards, 18 for the field cards in the detail pane,
     * 16 for note cards and list rows, 14 for tiles.
     *
     * The optional title and note labels are created on first use and hidden
     * again when set to an empty string; everything else goes into
     * contentLayout().
     */
    class Card : public QFrame
    {
        Q_OBJECT

    public:
        enum class Variant
        {
            Filled,
            Outlined,
            Elevated
        };

        explicit Card(QWidget* parent = nullptr);
        Card(Variant variant, int radius, QWidget* parent = nullptr);
        ~Card() override;

        Variant variant() const;
        void setVariant(Variant variant);

        int radius() const;
        void setRadius(int radius);

        QString titleText() const;
        void setTitleText(const QString& text);

        QString noteText() const;
        void setNoteText(const QString& text);

        /** Fill role for Filled and Elevated cards; defaults to SurfaceContainerLowest. */
        Role fillRole() const;
        void setFillRole(Role role);

        /** Shadow level of an Elevated card, 1..3. Ignored by the other variants. */
        int elevationLevel() const;
        void setElevationLevel(int level);

        /** The column below the title, where callers add their own widgets. */
        QVBoxLayout* contentLayout() const;

    protected:
        void paintEvent(QPaintEvent* event) override;

    private:
        void applyTheme();

        QVBoxLayout* m_rootLayout = nullptr;
        QVBoxLayout* m_contentLayout = nullptr;
        QLabel* m_titleLabel = nullptr;
        QLabel* m_noteLabel = nullptr;
        Variant m_variant = Variant::Outlined;
        // Qualified: QFrame::Shape shadows Material::Shape inside this class.
        int m_radius = Material::Shape::ExtraLarge;
        int m_elevationLevel = 1;
        Role m_fillRole = Role::SurfaceContainerLowest;
    };

} // namespace Material

#endif // KEEPASSXC_MATERIALCARD_H
