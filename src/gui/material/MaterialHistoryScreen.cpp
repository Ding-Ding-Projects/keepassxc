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

#include "MaterialHistoryScreen.h"

#include "MaterialButtons.h"
#include "MaterialElevation.h"
#include "MaterialIcons.h"
#include "MaterialSearchBar.h"

#include <QHBoxLayout>
#include <QPainter>
#include <QVBoxLayout>

namespace Material
{
    namespace
    {
        constexpr int RowRadius = 20;
        constexpr int RowSpacing = 10;
        constexpr int RowPaddingX = 18;
        constexpr int RowPaddingY = 14;
        constexpr int CircleSize = 40;
        constexpr int GlyphSize = 20;
        constexpr int CircleGap = 16;
        constexpr int ColumnGap = 16;
        constexpr int ActionGap = 16;
        constexpr int ActionHeight = 36;
        constexpr int DiffPadding = 14;
        constexpr int FilterChipHeight = 36;
        constexpr int RecentDays = 30;
        constexpr int ListWidth = 1000;
        constexpr int SearchMaximumWidth = 520;

        /** The fill of the glyph circle, one container role per tint. */
        QColor tintContainer(RevisionTint tint)
        {
            switch (tint) {
            case RevisionTint::Accent:
                return theme()->color(Role::PrimaryContainer);
            case RevisionTint::Positive:
                return theme()->color(Role::GreenContainer);
            case RevisionTint::Negative:
                return theme()->color(Role::ErrorContainer);
            case RevisionTint::Muted:
                return theme()->color(Role::SurfaceContainerHigh);
            case RevisionTint::Neutral:
                break;
            }
            return theme()->color(Role::SecondaryContainer);
        }

        /** The glyph colour that goes with tintContainer(). */
        QColor tintContent(RevisionTint tint)
        {
            switch (tint) {
            case RevisionTint::Accent:
                return theme()->color(Role::OnPrimaryContainer);
            case RevisionTint::Positive:
                return theme()->color(Role::OnGreenContainer);
            case RevisionTint::Negative:
                return theme()->color(Role::OnErrorContainer);
            case RevisionTint::Muted:
                return theme()->color(Role::OnSurfaceVariant);
            case RevisionTint::Neutral:
                break;
            }
            return theme()->color(Role::OnSecondaryContainer);
        }

        /** The metadata line is monospace at the 12px meta size. */
        QFont metaFont()
        {
            QFont font = theme()->font(TypeRole::LabelMedium);
            font.setFamily(Theme::monoFamily());
            font.setWeight(QFont::Normal);
            return font;
        }

        void clearLayout(QLayout* layout)
        {
            while (QLayoutItem* item = layout->takeAt(0)) {
                delete item->widget();
                delete item;
            }
        }

        /** The Diff action, which the design pads tighter than a normal button. */
        class DiffButton : public OutlinedButton
        {
        public:
            DiffButton(const QString& text, QWidget* parent)
                : OutlinedButton(QString(), text, parent)
            {
                // The base constructor pinned the minimum width using the base
                // padding, before this override existed; redo it.
                enforceLabelWidth();
            }

        protected:
            int horizontalPadding() const override
            {
                return DiffPadding;
            }
        };

        /** A rounded-20 outlined revision row. */
        class RevisionRow : public QWidget
        {
        public:
            RevisionRow(const Revision& revision,
                        const QString& diffText,
                        const QString& restoreText,
                        QWidget* parent = nullptr)
                : QWidget(parent)
                , m_revision(revision)
            {
                auto layout = new QHBoxLayout(this);
                layout->setContentsMargins(RowPaddingX, RowPaddingY, RowPaddingX, RowPaddingY);
                layout->setSpacing(ActionGap);
                layout->addStretch(1);

                // Each action is drawn only where it can do its job: a save
                // record can be compared but not put back, and the lines the
                // screen shows when there is nothing to list can do neither.
                if (m_revision.canDiff) {
                    m_diff = new DiffButton(diffText, this);
                    m_diff->setFixedHeight(ActionHeight);
                    layout->addWidget(m_diff);
                }
                if (m_revision.canRestore) {
                    m_restore = new TonalButton(QStringLiteral("restore"), restoreText, this);
                    m_restore->setFixedHeight(ActionHeight);
                    layout->addWidget(m_restore);
                }

                setMinimumHeight(CircleSize + RowPaddingY * 2);
            }

            ButtonBase* diffButton() const
            {
                return m_diff;
            }

            ButtonBase* restoreButton() const
            {
                return m_restore;
            }

        protected:
            void paintEvent(QPaintEvent* event) override
            {
                Q_UNUSED(event)
                QPainter painter(this);
                painter.setRenderHint(QPainter::Antialiasing);
                paintSurface(&painter,
                             rect(),
                             RowRadius,
                             theme()->color(Role::SurfaceContainerLow),
                             theme()->color(Role::OutlineVariant));

                const QRect circleRect(RowPaddingX, (height() - CircleSize) / 2, CircleSize, CircleSize);
                paintSurface(&painter, circleRect, Shape::Full, tintContainer(m_revision.tint));

                const QPixmap glyph = Icons::pixmap(m_revision.symbol, GlyphSize, tintContent(m_revision.tint));
                const int inset = (CircleSize - GlyphSize) / 2;
                painter.drawPixmap(circleRect.topLeft() + QPoint(inset, inset), glyph);

                // The text stops at whichever action comes first, so a row that
                // only carries a Restore gives its label the same room.
                const ButtonBase* leading = m_diff ? m_diff : m_restore;
                const int left = circleRect.right() + CircleGap;
                const int right = leading ? leading->x() - ColumnGap : width() - RowPaddingX;
                const int textWidth = qMax(0, right - left);
                const QFont labelFont = theme()->font(TypeRole::LabelLarge);
                const QFont metaLineFont = metaFont();
                const QFontMetrics labelMetrics(labelFont);
                const QFontMetrics metaMetrics(metaLineFont);

                int y = (height() - labelMetrics.height() - metaMetrics.height()) / 2;
                painter.setFont(labelFont);
                painter.setPen(theme()->color(Role::OnSurface));
                painter.drawText(QRect(left, y, textWidth, labelMetrics.height()),
                                 Qt::AlignLeft | Qt::AlignVCenter,
                                 labelMetrics.elidedText(m_revision.label, Qt::ElideRight, textWidth));

                y += labelMetrics.height();
                painter.setFont(metaLineFont);
                painter.setPen(theme()->color(Role::OnSurfaceVariant));
                painter.drawText(QRect(left, y, textWidth, metaMetrics.height()),
                                 Qt::AlignLeft | Qt::AlignVCenter,
                                 metaMetrics.elidedText(m_revision.meta, Qt::ElideRight, textWidth));
            }

        private:
            Revision m_revision;
            ButtonBase* m_diff = nullptr;
            ButtonBase* m_restore = nullptr;
        };
    } // namespace

    HistoryScreen::HistoryScreen(QWidget* parent)
        : Screen(parent)
    {
        setHeadline(tr("Version history"));
        // The feed replaces this with what its sources actually hold; the
        // screen's own line has to be true on its own until then.
        setSupportingText(tr("Changes to the open database, newest first."));

        setSearchVisible(true);
        searchBar()->setPlaceholder(tr("Search this surface"));
        searchBar()->setIdentity(QStringLiteral("history.revisions"), tr("History revision search"));
        searchBar()->setMaximumWidth(SearchMaximumWidth);
        searchBar()->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

        // The design draws the three chips as 36px boxes rather than pills.
        const auto filterChip = [](const QString& symbol, const QString& text) {
            auto chip = new Chip(symbol, text, Chip::Kind::Filter);
            chip->setRadius(Shape::Small);
            chip->setFixedHeight(FilterChipHeight);
            return chip;
        };

        m_entriesChip = filterChip(QStringLiteral("filter_list"), tr("Entries"));
        m_settingsChip = filterChip(QString(), tr("Settings"));
        m_recentChip = filterChip(QStringLiteral("calendar_month"), tr("Last %n day(s)", "", RecentDays));

        // Entries and Settings are two halves of one question, so pressing one
        // releases the other instead of leaving an empty intersection.
        connect(m_entriesChip, &QAbstractButton::toggled, this, [this](bool on) {
            if (on) {
                m_settingsChip->setChecked(false);
            }
            emit filterChanged();
        });
        connect(m_settingsChip, &QAbstractButton::toggled, this, [this](bool on) {
            if (on) {
                m_entriesChip->setChecked(false);
            }
            emit filterChanged();
        });
        connect(m_recentChip, &QAbstractButton::toggled, this, &HistoryScreen::filterChanged);

        auto filterRow = new QHBoxLayout();
        filterRow->setContentsMargins(0, 0, 0, 0);
        filterRow->setSpacing(RowSpacing);
        filterRow->addWidget(searchBar());
        filterRow->addWidget(m_entriesChip);
        filterRow->addWidget(m_settingsChip);
        filterRow->addWidget(m_recentChip);
        filterRow->addStretch(1);
        contentLayout()->addLayout(filterRow);

        auto list = new QWidget();
        list->setMaximumWidth(ListWidth);
        m_revisionLayout = new QVBoxLayout(list);
        m_revisionLayout->setContentsMargins(0, 0, 0, 0);
        m_revisionLayout->setSpacing(RowSpacing);
        contentLayout()->addWidget(list);
        contentLayout()->addStretch(1);

        connect(theme(), &Theme::changed, this, &HistoryScreen::rebuild);
    }

    HistoryScreen::~HistoryScreen() = default;

    void HistoryScreen::setRevisions(const QVector<Revision>& revisions)
    {
        m_revisions = revisions;
        rebuild();
    }

    RevisionFilter HistoryScreen::kindFilter() const
    {
        if (m_entriesChip->isChecked()) {
            return RevisionFilter::Entries;
        }
        if (m_settingsChip->isChecked()) {
            return RevisionFilter::Settings;
        }
        return RevisionFilter::All;
    }

    bool HistoryScreen::isRecentOnly() const
    {
        return m_recentChip->isChecked();
    }

    int HistoryScreen::recentDays()
    {
        return RecentDays;
    }

    void HistoryScreen::rebuild()
    {
        clearLayout(m_revisionLayout);
        for (const auto& revision : m_revisions) {
            auto row = new RevisionRow(revision, tr("Diff"), tr("Restore"));
            const QString id = revision.id;
            if (row->diffButton()) {
                connect(row->diffButton(), &QAbstractButton::clicked, this, [this, id] { emit diffRequested(id); });
            }
            if (row->restoreButton()) {
                connect(
                    row->restoreButton(), &QAbstractButton::clicked, this, [this, id] { emit restoreRequested(id); });
            }
            m_revisionLayout->addWidget(row);
        }
    }

} // namespace Material
