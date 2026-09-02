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

#include "MaterialSelect.h"

#include "MaterialButtons.h"
#include "MaterialCard.h"
#include "MaterialChip.h"
#include "MaterialElevation.h"
#include "MaterialIcons.h"
#include "MaterialSearchBar.h"

#include <QAbstractButton>
#include <QGridLayout>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QProgressBar>
#include <QResizeEvent>
#include <QToolButton>
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
                setAccessibleName(ReportsScreen::tr("%1: %2. %3").arg(card.label, card.value, card.sub));
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
                setAccessibleName(ReportsScreen::tr("%1. %2. %3").arg(row.title, row.reason, row.score));

                auto layout = new QHBoxLayout(this);
                layout->setContentsMargins(0, 0, 0, 0);
                layout->setSpacing(ColumnGap);
                m_select = new QCheckBox(this);
                m_select->setAccessibleName(ReportsScreen::tr("Select finding: %1").arg(m_row.title));
                layout->addWidget(m_select);
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
            QCheckBox* selection() const { return m_select; }

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
            QCheckBox* m_select = nullptr;
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
                setAccessibleName(ReportsScreen::tr("%1: %2").arg(key, value));
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
        constexpr int CardHeaderHeight = 64;
        constexpr int CardGlyphCircle = 36;

        /**
         * The collapsible card's header, in the reference anatomy: a tonal
         * glyph circle, title over blurb, the count pill and a chevron.
         */
        class ReportCardHeader : public QAbstractButton
        {
        public:
            ReportCardHeader(const ReportCard& card, bool expanded, QWidget* parent = nullptr)
                : QAbstractButton(parent)
                , m_card(card)
            {
                setCheckable(true);
                setChecked(expanded);
                setCursor(Qt::PointingHandCursor);
                setFocusPolicy(Qt::StrongFocus);
                setFixedHeight(CardHeaderHeight);
                setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
                auto* layout = new QHBoxLayout(this);
                layout->setContentsMargins(0, 0, 0, 0);
                layout->addStretch(1);
                m_count = new ScoreChip(card.unavailable ? PillKind::Off : healthPill(card.status), card.count, this);
                m_count->setAttribute(Qt::WA_TransparentForMouseEvents);
                layout->addWidget(m_count);
                layout->addSpacing(GlyphGap);
                layout->addSpacing(GlyphSize);
                connect(this, &QAbstractButton::toggled, this, [this] { update(); });
            }

        protected:
            void paintEvent(QPaintEvent* event) override
            {
                Q_UNUSED(event)
                QPainter painter(this);
                painter.setRenderHint(QPainter::Antialiasing);
                const QRect circle(0, (height() - CardGlyphCircle) / 2, CardGlyphCircle, CardGlyphCircle);
                painter.setPen(Qt::NoPen);
                painter.setBrush(hasFocus() ? theme()->color(Role::SecondaryContainer)
                                            : theme()->color(Role::SurfaceContainerHigh));
                painter.drawEllipse(circle);
                const QPixmap glyph = Icons::pixmap(m_card.symbol, GlyphSize, theme()->colors().healthColor(m_card.status));
                painter.drawPixmap(circle.center() - QPoint(GlyphSize / 2 - 1, GlyphSize / 2 - 1), glyph);

                const int left = CardGlyphCircle + GlyphGap;
                const int textWidth = qMax(0, m_count->x() - ColumnGap - left);
                const QFont titleFont = theme()->font(TypeRole::TitleSmall);
                const QFont blurbFont = weighted(TypeRole::LabelMedium, QFont::Normal);
                const QFontMetrics titleMetrics(titleFont);
                const QFontMetrics blurbMetrics(blurbFont);
                int y = (height() - titleMetrics.height() - blurbMetrics.height()) / 2;
                painter.setFont(titleFont);
                painter.setPen(theme()->color(Role::OnSurface));
                painter.drawText(QRect(left, y, textWidth, titleMetrics.height()),
                                 Qt::AlignLeft | Qt::AlignVCenter,
                                 titleMetrics.elidedText(m_card.title, Qt::ElideRight, textWidth));
                y += titleMetrics.height();
                painter.setFont(blurbFont);
                painter.setPen(theme()->color(Role::OnSurfaceVariant));
                painter.drawText(QRect(left, y, textWidth, blurbMetrics.height()),
                                 Qt::AlignLeft | Qt::AlignVCenter,
                                 blurbMetrics.elidedText(m_card.blurb, Qt::ElideRight, textWidth));

                const QPixmap chevron = Icons::pixmap(isChecked() ? QStringLiteral("expand_less") : QStringLiteral("expand_more"),
                                                      GlyphSize,
                                                      theme()->color(Role::OnSurfaceVariant));
                painter.drawPixmap(QPoint(width() - GlyphSize, (height() - GlyphSize) / 2), chevron);
            }

        private:
            ReportCard m_card;
            ScoreChip* m_count = nullptr;
        };
    } // namespace

    ReportsScreen::ReportsScreen(QWidget* parent)
        : Screen(parent)
    {
        // The design's reports page opens straight on the search pill and the
        // export action; the app bar already names the destination.
        setHeadline(QString());
        setSearchVisible(true);
        searchBar()->setPlaceholder(tr("Search this surface"));
        searchBar()->setIdentity(QStringLiteral("reports.findings"), tr("Report findings search"));
        // A minimum, not a fixed width: on the header's own overflow row the bar
        // stretches across the width instead of sitting at 340 px in a corner.
        searchBar()->setMinimumWidth(SearchWidth);

        m_category = new Select;
        m_category->setObjectName(QStringLiteral("reportsCategory"));
        m_category->setAccessibleName(tr("Report category"));
        m_category->setSearchIdentity(QStringLiteral("reports.category"), tr("Report category search"));
        m_category->addItem(tr("All findings"), QStringLiteral("all"));
        m_category->addItem(tr("Weak"), QStringLiteral("weak"));
        m_category->addItem(tr("Reused"), QStringLiteral("reused"));
        m_category->addItem(tr("Expired"), QStringLiteral("expired"));
        m_category->addItem(tr("Excluded"), QStringLiteral("excluded"));
        connect(m_category, &Select::currentIndexChanged, this, [this](int index) {
            emit categoryChanged(m_category->itemData(index).toString());
        });
        insertHeaderWidget(0, m_category);

        m_stateLabel = new QLabel;
        m_stateLabel->setObjectName(QStringLiteral("reportsState"));
        m_stateLabel->setAccessibleName(tr("Report state"));
        m_stateLabel->setWordWrap(true);
        m_progress = new QProgressBar;
        m_progress->setObjectName(QStringLiteral("reportsProgress"));
        m_progress->setAccessibleName(tr("Report progress"));
        m_progress->hide();
        contentLayout()->addWidget(m_stateLabel);
        contentLayout()->addWidget(m_progress);

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

        auto* exportAll = new OutlinedButton(QStringLiteral("download"), tr("Export Markdown"));
        exportAll->setObjectName(QStringLiteral("reportsExportAll"));
        exportAll->setAccessibleName(tr("Export the report as Markdown"));
        connect(exportAll, &QAbstractButton::clicked, this, &ReportsScreen::exportRequested);
        insertHeaderWidget(1, exportAll);
        m_bulkExport = new TextButton(QStringLiteral("checklist"), tr("Export selected findings"));
        m_bulkExport->setObjectName(QStringLiteral("reportsBulkExport"));
        m_bulkExport->setAccessibleName(m_bulkExport->text());
        m_bulkExport->setEnabled(false);
        connect(m_bulkExport, &QAbstractButton::clicked, this, [this] { emit bulkExportRequested(selectedFindingIds()); });
        insertHeaderWidget(2, m_bulkExport);

        m_reportCardsHost = new QWidget;
        m_reportCardsHost->setObjectName(QStringLiteral("reportsCards"));
        m_reportCardsLayout = new QGridLayout(m_reportCardsHost);
        m_reportCardsLayout->setContentsMargins(0, 0, 0, 0);
        m_reportCardsLayout->setSpacing(GridSpacing);
        contentLayout()->addWidget(m_reportCardsHost);
        contentLayout()->addStretch(1);

        connect(theme(), &Theme::changed, this, &ReportsScreen::rebuild);
        setState(State::Empty, tr("Open and unlock a database to calculate reports."));
        applyResponsiveLayout();
    }

    ReportsScreen::~ReportsScreen() = default;

    void ReportsScreen::setStatCards(const QVector<StatCard>& cards)
    {
        m_statCards = cards;
        rebuildStatCards();
    }

    void ReportsScreen::setReportCards(const QVector<ReportCard>& cards)
    {
        m_reportCards = cards;
        for (const auto& card : cards) {
            if (!m_expandedCards.contains(card.id)) m_expandedCards.insert(card.id, card.id == QLatin1String("breached"));
        }
        rebuildReportCards();
    }

    QStringList ReportsScreen::reportCardIds() const
    {
        QStringList ids;
        for (const auto& card : m_reportCards) ids.append(card.id);
        return ids;
    }

    bool ReportsScreen::isCardExpanded(const QString& id) const { return m_expandedCards.value(id); }

    void ReportsScreen::setCardExpanded(const QString& id, bool expanded)
    {
        if (!reportCardIds().contains(id) || m_expandedCards.value(id) == expanded) return;
        m_expandedCards[id] = expanded;
        rebuildReportCards();
    }

    void ReportsScreen::rebuild()
    {
        rebuildStatCards();
        rebuildReportCards();
    }

    void ReportsScreen::rebuildStatCards()
    {
        clearLayout(m_statGrid);
        const int columns = width() < 600 ? 1 : width() < 1200 ? 2 : StatColumns;
        for (int i = 0; i < m_statCards.size(); ++i) {
            m_statGrid->addWidget(new StatTile(m_statCards.at(i)), i / columns, i % columns);
        }
    }

    void ReportsScreen::rebuildReportCards()
    {
        clearLayout(m_reportCardsLayout);
        // The reference stacks the cards in one column at every width.
        const int columns = 1;
        for (int index = 0; index < m_reportCards.size(); ++index) {
            const auto cardData = m_reportCards.at(index);
            auto* card = new SectionCard;
            card->setObjectName(QStringLiteral("reportCard_") + cardData.id);
            auto* toggle = new ReportCardHeader(cardData, m_expandedCards.value(cardData.id), card);
            toggle->setObjectName(QStringLiteral("reportToggle_") + cardData.id);
            toggle->setText(tr("%1 · %2").arg(cardData.title, cardData.count));
            toggle->setAccessibleName(tr("%1 report, %2. %3").arg(cardData.title, cardData.count, cardData.blurb));
            toggle->setAccessibleDescription(cardData.unavailable
                                                 ? tr("Unavailable; not checked")
                                                 : (toggle->isChecked() ? tr("Expanded") : tr("Collapsed")));
            connect(toggle, &QToolButton::toggled, this, [this, id = cardData.id](bool expanded) {
                m_expandedCards[id] = expanded;
                rebuildReportCards();
            });
            card->contentLayout()->addWidget(toggle);

            auto* body = new QWidget(card);
            auto* bodyLayout = new QVBoxLayout(body);
            bodyLayout->setContentsMargins(0, 0, 0, 0);
            bodyLayout->setSpacing(4);
            if (cardData.id == QLatin1String("statistics")) {
                for (int row = 0; row < cardData.statistics.size(); ++row) {
                    const auto& value = cardData.statistics.at(row);
                    bodyLayout->addWidget(new StatisticsRowWidget(value.first, value.second, true));
                }
            } else {
                for (const auto& row : cardData.rows) {
                    auto* widget = new HealthRowWidget(row, tr("Fix"), true);
                    widget->selection()->setEnabled(!row.id.isEmpty());
                    widget->selection()->setChecked(m_selectedIds.contains(row.id));
                    connect(widget->selection(), &QCheckBox::toggled, this, [this, id = row.id](bool checked) {
                        if (checked) m_selectedIds.insert(id); else m_selectedIds.remove(id);
                        updateBulkActions();
                    });
                    connect(widget->fixButton(), &QAbstractButton::clicked, this, [this, id = row.id] {
                        if (!id.isEmpty()) emit fixRequested(id);
                    });
                    bodyLayout->addWidget(widget);
                }
            }
            if (cardData.unavailable || (cardData.rows.isEmpty() && cardData.statistics.isEmpty())) {
                auto* empty = new QLabel(cardData.unavailable ? tr("Not checked. Run the real breach report to obtain results.")
                                                              : tr("No rows in this report."));
                empty->setWordWrap(true);
                empty->setAccessibleName(empty->text());
                bodyLayout->addWidget(empty);
            }
            body->setVisible(toggle->isChecked());
            card->contentLayout()->addWidget(body);
            m_reportCardsLayout->addWidget(card, index / columns, index % columns, Qt::AlignTop);
        }
        updateBulkActions();
    }

    void ReportsScreen::setState(State state, const QString& message, int progress)
    {
        m_state = state;
        m_stateLabel->setText(message);
        m_stateLabel->setVisible(!message.isEmpty());
        const bool showProgress = state == State::Loading || state == State::Progress;
        m_progress->setVisible(showProgress);
        if (showProgress) {
            m_progress->setRange(progress < 0 ? 0 : 0, progress < 0 ? 0 : 100);
            if (progress >= 0) m_progress->setValue(qBound(0, progress, 100));
        }
        const bool hasContent = state == State::Populated || state == State::Warning;
        m_reportCardsHost->setVisible(hasContent);
    }

    ReportsScreen::State ReportsScreen::state() const { return m_state; }

    QStringList ReportsScreen::selectedFindingIds() const
    {
        QStringList ids(m_selectedIds.cbegin(), m_selectedIds.cend());
        ids.sort();
        return ids;
    }

    void ReportsScreen::setSearchValidation(bool valid, const QString& message)
    {
        searchBar()->lineEdit()->setAccessibleDescription(valid ? tr("Report filter is valid") : message);
        if (!valid) setState(State::Warning, message);
    }

    void ReportsScreen::updateBulkActions()
    {
        m_bulkExport->setEnabled(!m_selectedIds.isEmpty());
        m_bulkExport->setText(tr("Export %n selected finding(s)", "", m_selectedIds.size()));
    }

    void ReportsScreen::applyResponsiveLayout()
    {
        rebuildStatCards();
        rebuildReportCards();
    }

    void ReportsScreen::resizeEvent(QResizeEvent* event)
    {
        Screen::resizeEvent(event);
        applyResponsiveLayout();
    }

} // namespace Material
