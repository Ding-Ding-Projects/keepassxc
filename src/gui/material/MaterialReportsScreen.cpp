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

#include "MaterialReportsScreen.h"

#include "MaterialButtons.h"
#include "MaterialCard.h"
#include "MaterialChip.h"
#include "MaterialElevation.h"
#include "MaterialIcons.h"
#include "MaterialSearchBar.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QVBoxLayout>

namespace Material
{
    namespace
    {
        constexpr int StatColumns = 4;
        constexpr int GridSpacing = 16;
        constexpr int TilePadding = 20;
        constexpr int TileMinimumWidth = 140;
        constexpr int GlyphSize = 20;
        constexpr int GlyphGap = 14;
        constexpr int HealthRowHeight = 60;
        constexpr int StatisticsRowHeight = 46;
        // The health row is one flex gap throughout, the same 14px as GlyphGap.
        constexpr int ColumnGap = 14;
        constexpr int SearchWidth = 340;
        // The Fix affordance is a compact pill, not a 40px / 16px / 14px button.
        constexpr int FixButtonHeight = 32;
        constexpr int FixButtonPadding = 14;
        constexpr TypeRole FixLabelRole = TypeRole::BodySmall; // 13px regular
        // The score chip is a 12px line in 5px of vertical padding, 12px either side.
        constexpr int ScoreChipPadding = 12;
        constexpr int ScoreChipVerticalPadding = 5;
        // The stat grid is 16px from the content above it and 20px from the cards below.
        constexpr int StatGridBottomGap = 4;
        // Card content is spaced 8px, so this much makes the design's 18px.
        constexpr int ExportButtonGap = 2;

        QFont weighted(TypeRole role, QFont::Weight weight)
        {
            QFont font = theme()->font(role);
            font.setWeight(weight);
            return font;
        }

        /** Tile background. Unknown reads as information rather than as a warning. */
        QColor statContainer(Health status)
        {
            if (status == Health::Unknown) {
                return theme()->color(Role::PrimaryContainer);
            }
            return theme()->colors().healthContainer(status);
        }

        QColor statContent(Health status)
        {
            if (status == Health::Unknown) {
                return theme()->color(Role::OnPrimaryContainer);
            }
            return theme()->colors().onHealthContainer(status);
        }

        PillKind healthPill(Health status)
        {
            switch (status) {
            case Health::Ok:
                return PillKind::Good;
            case Health::Weak:
            case Health::Reused:
                return PillKind::Warn;
            case Health::Breached:
                return PillKind::Bad;
            case Health::Unknown:
                break;
            }
            return PillKind::Value;
        }

        void clearLayout(QLayout* layout)
        {
            while (QLayoutItem* item = layout->takeAt(0)) {
                delete item->widget();
                delete item;
            }
        }

        /**
         * Both report cards are filled *and* outlined. No Card::Variant carries
         * that combination - Filled paints no border, Outlined no fill - so the
         * surface is painted here from the same primitive.
         */
        class SectionCard : public Card
        {
        public:
            explicit SectionCard(QWidget* parent = nullptr)
                // Qualified: QFrame::Shape shadows Material::Shape in a QFrame.
                : Card(Variant::Filled, Material::Shape::ExtraLarge, parent)
            {
                setFillRole(Role::SurfaceContainerLow);
            }

        protected:
            void paintEvent(QPaintEvent* event) override
            {
                Q_UNUSED(event)
                QPainter painter(this);
                painter.setRenderHint(QPainter::Antialiasing, true);
                paintSurface(
                    &painter, rect(), radius(), theme()->color(fillRole()), theme()->color(Role::OutlineVariant));
            }
        };

        /** The compact outlined pill the design ends every health row with. */
        class FixButton : public OutlinedButton
        {
        public:
            FixButton(const QString& text, QWidget* parent = nullptr)
                : OutlinedButton(QString(), text, parent)
            {
                setFixedHeight(FixButtonHeight);
                // ButtonBase pinned the minimum width from its own size hint
                // while this class was still being built; redo it now that the
                // compact one answers.
                enforceLabelWidth();
            }

            QSize sizeHint() const override
            {
                const QFontMetrics metrics(theme()->font(FixLabelRole));
                return {2 * FixButtonPadding + metrics.horizontalAdvance(text()), FixButtonHeight};
            }

            QSize minimumSizeHint() const override
            {
                return sizeHint();
            }

        protected:
            int horizontalPadding() const override
            {
                return FixButtonPadding;
            }

            /** ButtonBase draws its label at LabelLarge; this one is 13px. */
            void paintEvent(QPaintEvent* event) override
            {
                Q_UNUSED(event)
                QPainter painter(this);
                painter.setRenderHint(QPainter::Antialiasing);
                paintSurface(&painter, rect(), radius(), containerColor(), borderColor());

                painter.setFont(theme()->font(FixLabelRole));
                painter.setPen(contentColor());
                painter.drawText(rect(), Qt::AlignCenter, text());
            }
        };

        /**
         * The score chip. PillLabel is the 32px spec-sheet pill; the report
         * chip is the same treatment at the design's tighter padding.
         */
        class ScoreChip : public PillLabel
        {
        public:
            ScoreChip(PillKind kind, const QString& text, QWidget* parent = nullptr)
                : PillLabel(kind, text, parent)
            {
            }

            QSize sizeHint() const override
            {
                const QFontMetrics metrics(theme()->font(TypeRole::LabelMedium));
                return {2 * ScoreChipPadding + metrics.horizontalAdvance(text()),
                        2 * ScoreChipVerticalPadding + metrics.height()};
            }

            QSize minimumSizeHint() const override
            {
                return sizeHint();
            }

        protected:
            void paintEvent(QPaintEvent* event) override
            {
                Q_UNUSED(event)
                QPainter painter(this);
                painter.setRenderHint(QPainter::Antialiasing);
                // Qualified: QFrame, which QLabel derives from, also has a Shape enum.
                paintSurface(&painter,
                             rect(),
                             Material::Shape::Small,
                             pillContainerColor(pillKind()),
                             pillBorderColor(pillKind()));

                painter.setFont(theme()->font(TypeRole::LabelMedium));
                painter.setPen(pillContentColor(pillKind()));
                painter.drawText(rect(), Qt::AlignCenter, text());
            }
        };

        /** A rounded-28 summary tile, tinted with its status family. */
        class StatTile : public QWidget
        {
        public:
            explicit StatTile(const StatCard& card, QWidget* parent = nullptr)
                : QWidget(parent)
                , m_card(card)
            {
                setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
            }

            QSize sizeHint() const override
            {
                const QFontMetrics label(weighted(TypeRole::BodySmall, QFont::Medium));
                const QFontMetrics value(theme()->font(TypeRole::DisplaySmall));
                const QFontMetrics sub(weighted(TypeRole::LabelMedium, QFont::Normal));

                const int height = TilePadding * 2 + label.height() + 6 + value.height() + 2 + sub.height();
                const int width =
                    TilePadding * 2 + qMax(label.horizontalAdvance(m_card.label), sub.horizontalAdvance(m_card.sub));
                return {width, height};
            }

            QSize minimumSizeHint() const override
            {
                return {TileMinimumWidth, sizeHint().height()};
            }

        protected:
            void paintEvent(QPaintEvent* event) override
            {
                Q_UNUSED(event)
                QPainter painter(this);
                painter.setRenderHint(QPainter::Antialiasing);
                paintSurface(&painter, rect(), Shape::ExtraLarge, statContainer(m_card.status));

                const QColor content = statContent(m_card.status);
                const int textWidth = qMax(0, width() - TilePadding * 2);
                int y = TilePadding;

                painter.setPen(content);
                y += drawLine(&painter, weighted(TypeRole::BodySmall, QFont::Medium), m_card.label, y, textWidth) + 6;
                y += drawLine(&painter, theme()->font(TypeRole::DisplaySmall), m_card.value, y, textWidth) + 2;

                QColor sub = content;
                sub.setAlphaF(0.8f);
                painter.setPen(sub);
                drawLine(&painter, weighted(TypeRole::LabelMedium, QFont::Normal), m_card.sub, y, textWidth);
            }

        private:
            /** Draw one elided line at @p y and answer with the height it took. */
            int drawLine(QPainter* painter, const QFont& font, const QString& text, int y, int width) const
            {
                const QFontMetrics metrics(font);
                painter->setFont(font);
                painter->drawText(QRect(TilePadding, y, width, metrics.height()),
                                  Qt::AlignLeft | Qt::AlignVCenter,
                                  metrics.elidedText(text, Qt::ElideRight, width));
                return metrics.height();
            }

            StatCard m_card;
        };

        /** A 60px health finding: status glyph, title over reason, score and Fix. */
        class HealthRowWidget : public QWidget
        {
        public:
            HealthRowWidget(const HealthRow& row, const QString& fixText, bool separator, QWidget* parent = nullptr)
                : QWidget(parent)
                , m_row(row)
                , m_separator(separator)
            {
                setFixedHeight(HealthRowHeight);

                auto layout = new QHBoxLayout(this);
                layout->setContentsMargins(0, 0, 0, 0);
                layout->setSpacing(ColumnGap);
                layout->addStretch(1);

                m_score = new ScoreChip(healthPill(m_row.status), m_row.score, this);
                layout->addWidget(m_score);

                m_fix = new FixButton(fixText, this);
                layout->addWidget(m_fix);
            }

            ButtonBase* fixButton() const
            {
                return m_fix;
            }

        protected:
            void paintEvent(QPaintEvent* event) override
            {
                Q_UNUSED(event)
                QPainter painter(this);
                painter.setRenderHint(QPainter::Antialiasing);

                const QPixmap glyph =
                    Icons::pixmap(m_row.symbol, GlyphSize, theme()->colors().healthColor(m_row.status));
                painter.drawPixmap(QPoint(0, (height() - GlyphSize) / 2), glyph);

                const int left = GlyphSize + GlyphGap;
                const int textWidth = qMax(0, m_score->x() - ColumnGap - left);
                const QFont titleFont = theme()->font(TypeRole::LabelLarge);
                const QFont reasonFont = weighted(TypeRole::LabelMedium, QFont::Normal);
                const QFontMetrics titleMetrics(titleFont);
                const QFontMetrics reasonMetrics(reasonFont);

                int y = (height() - titleMetrics.height() - reasonMetrics.height()) / 2;
                painter.setFont(titleFont);
                painter.setPen(theme()->color(Role::OnSurface));
                painter.drawText(QRect(left, y, textWidth, titleMetrics.height()),
                                 Qt::AlignLeft | Qt::AlignVCenter,
                                 titleMetrics.elidedText(m_row.title, Qt::ElideRight, textWidth));

                y += titleMetrics.height();
                painter.setFont(reasonFont);
                painter.setPen(theme()->color(Role::OnSurfaceVariant));
                painter.drawText(QRect(left, y, textWidth, reasonMetrics.height()),
                                 Qt::AlignLeft | Qt::AlignVCenter,
                                 reasonMetrics.elidedText(m_row.reason, Qt::ElideRight, textWidth));

                if (m_separator) {
                    painter.fillRect(QRectF(0, height() - 1, width(), 1), theme()->color(Role::OutlineVariant));
                }
            }

        private:
            HealthRow m_row;
            bool m_separator = true;
            PillLabel* m_score = nullptr;
            ButtonBase* m_fix = nullptr;
        };

        /** A 46px key over value row of the statistics card. */
        class StatisticsRowWidget : public QWidget
        {
        public:
            StatisticsRowWidget(const QString& key, const QString& value, bool separator, QWidget* parent = nullptr)
                : QWidget(parent)
                , m_key(key)
                , m_value(value)
                , m_separator(separator)
            {
                setFixedHeight(StatisticsRowHeight);
            }

        protected:
            void paintEvent(QPaintEvent* event) override
            {
                Q_UNUSED(event)
                QPainter painter(this);

                const QFont keyFont = theme()->font(TypeRole::BodySmall);
                const QFont valueFont = weighted(TypeRole::BodySmall, QFont::Medium);
                const QFontMetrics valueMetrics(valueFont);
                const int valueWidth = qMin(valueMetrics.horizontalAdvance(m_value), width() / 2);

                painter.setFont(valueFont);
                painter.setPen(theme()->color(Role::OnSurface));
                painter.drawText(QRect(width() - valueWidth, 0, valueWidth, height()),
                                 Qt::AlignRight | Qt::AlignVCenter,
                                 valueMetrics.elidedText(m_value, Qt::ElideRight, valueWidth));

                const QFontMetrics keyMetrics(keyFont);
                const int keyWidth = qMax(0, width() - valueWidth - ColumnGap);
                painter.setFont(keyFont);
                painter.setPen(theme()->color(Role::OnSurfaceVariant));
                painter.drawText(QRect(0, 0, keyWidth, height()),
                                 Qt::AlignLeft | Qt::AlignVCenter,
                                 keyMetrics.elidedText(m_key, Qt::ElideRight, keyWidth));

                if (m_separator) {
                    painter.fillRect(QRectF(0, height() - 1, width(), 1), theme()->color(Role::OutlineVariant));
                }
            }

        private:
            QString m_key;
            QString m_value;
            bool m_separator = true;
        };
    } // namespace

    ReportsScreen::ReportsScreen(QWidget* parent)
        : Screen(parent)
    {
        setHeadline(tr("Database reports"));
        setSearchVisible(true);
        searchBar()->setPlaceholder(tr("Search this surface"));
        searchBar()->setFixedWidth(SearchWidth);

        m_statGrid = new QGridLayout();
        // The content column already spaces its children 16px apart; the grid
        // carries the remainder of the design's 20px below it as a margin,
        // because a spacer item would take that spacing on both of its sides.
        m_statGrid->setContentsMargins(0, 0, 0, StatGridBottomGap);
        m_statGrid->setSpacing(GridSpacing);
        for (int column = 0; column < StatColumns; ++column) {
            m_statGrid->setColumnStretch(column, 1);
        }
        contentLayout()->addLayout(m_statGrid);

        m_healthCard = new SectionCard();
        m_healthCard->setTitleText(tr("Password health"));
        m_healthLayout = new QVBoxLayout();
        m_healthLayout->setContentsMargins(0, 0, 0, 0);
        m_healthLayout->setSpacing(0);
        m_healthCard->contentLayout()->addLayout(m_healthLayout);
        m_healthCard->contentLayout()->addStretch(1);

        m_statisticsCard = new SectionCard();
        m_statisticsCard->setTitleText(tr("Statistics"));
        m_statisticsLayout = new QVBoxLayout();
        m_statisticsLayout->setContentsMargins(0, 0, 0, 0);
        m_statisticsLayout->setSpacing(0);
        m_statisticsCard->contentLayout()->addLayout(m_statisticsLayout);
        m_statisticsCard->contentLayout()->addSpacing(ExportButtonGap);

        auto exportButton = new TonalButton(QStringLiteral("download"), tr("Export report"));
        exportButton->setSymbolSize(GlyphSize);
        connect(exportButton, &QAbstractButton::clicked, this, &ReportsScreen::exportRequested);
        m_statisticsCard->contentLayout()->addWidget(exportButton);
        m_statisticsCard->contentLayout()->addStretch(1);

        auto columns = new QGridLayout();
        columns->setContentsMargins(0, 0, 0, 0);
        columns->setSpacing(GridSpacing);
        columns->addWidget(m_healthCard, 0, 0);
        columns->addWidget(m_statisticsCard, 0, 1);
        columns->setColumnStretch(0, 14);
        columns->setColumnStretch(1, 10);
        contentLayout()->addLayout(columns);
        contentLayout()->addStretch(1);

        connect(theme(), &Theme::changed, this, &ReportsScreen::rebuild);
    }

    ReportsScreen::~ReportsScreen() = default;

    void ReportsScreen::setStatCards(const QVector<StatCard>& cards)
    {
        m_statCards = cards;
        rebuildStatCards();
    }

    void ReportsScreen::setHealthRows(const QVector<HealthRow>& rows)
    {
        m_healthRows = rows;
        rebuildHealthRows();
    }

    void ReportsScreen::setStatistics(const QVector<QPair<QString, QString>>& statistics)
    {
        m_statistics = statistics;
        rebuildStatistics();
    }

    void ReportsScreen::rebuild()
    {
        rebuildStatCards();
        rebuildHealthRows();
        rebuildStatistics();
    }

    void ReportsScreen::rebuildStatCards()
    {
        clearLayout(m_statGrid);
        for (int i = 0; i < m_statCards.size(); ++i) {
            m_statGrid->addWidget(new StatTile(m_statCards.at(i)), i / StatColumns, i % StatColumns);
        }
    }

    void ReportsScreen::rebuildHealthRows()
    {
        clearLayout(m_healthLayout);
        for (int i = 0; i < m_healthRows.size(); ++i) {
            const HealthRow& row = m_healthRows.at(i);
            // The rule belongs to the row in the design, last one included.
            auto widget = new HealthRowWidget(row, tr("Fix"), true);
            const QString id = row.id;
            connect(widget->fixButton(), &QAbstractButton::clicked, this, [this, id] { emit fixRequested(id); });
            m_healthLayout->addWidget(widget);
        }
    }

    void ReportsScreen::rebuildStatistics()
    {
        clearLayout(m_statisticsLayout);
        for (int i = 0; i < m_statistics.size(); ++i) {
            const auto& entry = m_statistics.at(i);
            // As on the health rows, the design rules every row.
            m_statisticsLayout->addWidget(new StatisticsRowWidget(entry.first, entry.second, true));
        }
    }

} // namespace Material
