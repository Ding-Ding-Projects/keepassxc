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

#ifndef KEEPASSXC_MATERIALSETTINGSSCREEN_H
#define KEEPASSXC_MATERIALSETTINGSSCREEN_H

#include "MaterialChip.h"
#include "MaterialScreen.h"
#include "MaterialTheme.h"

#include <QAbstractButton>
#include <QList>
#include <QString>

class QLabel;
class QSlider;

namespace Material
{
    class Card;
    class OutlinedButton;
    class SegmentedButton;

    /**
     * One of the four seed swatches in the appearance card: a 44px circle
     * filled with the seed's key colour, ringed with a 3px primary halo and
     * carrying a white check when it is the active seed.
     */
    class SeedSwatch : public QAbstractButton
    {
        Q_OBJECT

    public:
        static constexpr int Diameter = 44;
        /** The halo needs room outside the swatch, so the widget is larger. */
        static constexpr int Extent = 52;

        explicit SeedSwatch(Seed seed, QWidget* parent = nullptr);
        ~SeedSwatch() override;

        Seed seed() const;

        QSize sizeHint() const override;
        QSize minimumSizeHint() const override;

    protected:
        void paintEvent(QPaintEvent* event) override;

    private:
        Seed m_seed;
    };

    /**
     * A 56px rounded-16 surfaceContainer row in the integrations card: a
     * leading glyph, the name over a detail line, and a status pill.
     */
    class IntegrationRow : public QWidget
    {
        Q_OBJECT

    public:
        static constexpr int RowHeight = 56;
        /** The trailing chevron the design's external editor row ends in. */
        static constexpr int ChevronSize = 18;
        /** Padding between the trailing element and the right edge. */
        static constexpr int TrailingMargin = 16;

        IntegrationRow(const QString& symbol, const QString& title, QWidget* parent = nullptr);
        ~IntegrationRow() override;

        void setDetail(const QString& detail);
        void setStatus(PillKind kind, const QString& text);
        /** End the row in a chevron instead of a status pill. */
        void setChevron(bool chevron);

        QSize sizeHint() const override;
        QSize minimumSizeHint() const override;

    signals:
        void activated();

    protected:
        void paintEvent(QPaintEvent* event) override;
        void mouseReleaseEvent(QMouseEvent* event) override;
        void enterEvent(QEnterEvent* event) override;
        void leaveEvent(QEvent* event) override;

    private:
        QString m_symbol;
        QString m_title;
        QString m_detail;
        PillLabel* m_status = nullptr;
        bool m_hovered = false;
        bool m_chevron = false;
    };

    /**
     * The settings destination.
     *
     * A 28px headline with a search bar that filters whole cards, then the
     * design's 2x2 grid of rounded-28 cards, capped at 1180px: appearance,
     * language and tone, security and behaviour, integrations. Every control
     * is bound to the real KeePassXC configuration or to the design system, so
     * the screen is the settings rather than a preview of them.
     */
    class SettingsScreen : public Screen
    {
        Q_OBJECT

    public:
        explicit SettingsScreen(QWidget* parent = nullptr);
        ~SettingsScreen() override;

    signals:
        /** The interface font row was activated; the host opens the font dialog. */
        void interfaceFontRequested();
        /** An integration row was activated, identified by "browser", "ssh-agent", ... */
        void integrationActivated(const QString& id);

    private:
        /** A card plus the lower-cased text the settings search matches it by. */
        struct SearchableCard
        {
            Card* card;
            QString haystack;
        };

        Card* createAppearanceCard();
        Card* createLanguageCard();
        Card* createBehaviourCard();
        Card* createIntegrationsCard();

        void applyFilter(const QString& text);
        /** Pull the controls back in line with the theme after it changed. */
        void refreshFromTheme();
        /** Re-render the preview message from the pending slider positions. */
        void updateVoicePreview();
        /** Write the two humour sliders back into the configuration. */
        void commitVoiceLevels();
        /** Pull the voice controls back in line with the stored settings. */
        void refreshFromVoice();

        QList<SearchableCard> m_cards;
        SegmentedButton* m_themeSegment = nullptr;
        SegmentedButton* m_densitySegment = nullptr;
        /** English / 廣東話 / Bilingual: the interface language and the voice. */
        SegmentedButton* m_languageSegment = nullptr;
        QList<SeedSwatch*> m_swatches;
        OutlinedButton* m_fontRowButton = nullptr;
        QSlider* m_englishFunnySlider = nullptr;
        QLabel* m_englishFunnyValue = nullptr;
        QSlider* m_cantoneseFunnySlider = nullptr;
        QLabel* m_cantoneseFunnyValue = nullptr;
        /** The design's single live preview line. */
        QLabel* m_previewLabel = nullptr;
        /** Set while the screen writes into its own controls, to stop feedback. */
        bool m_updating = false;
    };

} // namespace Material

#endif // KEEPASSXC_MATERIALSETTINGSSCREEN_H
