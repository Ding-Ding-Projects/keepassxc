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
#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPainter>
#include <QProgressBar>
#include <QResizeEvent>
#include <QToolButton>
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
        constexpr int BadgeHeight = 22;
        constexpr int BadgePaddingX = 9;
        constexpr int BadgeRadius = 6;
        constexpr int BadgeGap = 9;
        constexpr int BannerRadius = 16;
        constexpr int BannerPadding = 12;
        constexpr int BannerGlyph = 20;
        constexpr int RecentDays = 30;
        constexpr int ListWidth = 1000;
        constexpr int SearchMaximumWidth = 520;

        class FlexibleDateEdit : public QDateEdit
        {
        public:
            using QDateEdit::QDateEdit;

        protected:
            QDateTime dateTimeFromText(const QString& text) const override
            {
                const QDate iso = QDate::fromString(text.trimmed(), Qt::ISODate);
                const QDate parsed = iso.isValid() ? iso : locale().toDate(text.trimmed(), QLocale::ShortFormat);
                return parsed.isValid() ? QDateTime(parsed, QTime(0, 0)) : QDateTime();
            }
        };

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
                m_select = new QCheckBox(this);
                m_select->setAccessibleName(HistoryScreen::tr("Select revision: %1").arg(revision.label));
                m_select->setObjectName(QStringLiteral("historySelect_%1").arg(revision.id));
                m_select->setEnabled(!revision.id.isEmpty());
                layout->addWidget(m_select);
                layout->addStretch(1);
                setAccessibleName(revision.badge.isEmpty()
                                      ? HistoryScreen::tr("%1. %2").arg(revision.label, revision.meta)
                                      : HistoryScreen::tr("%1: %2. %3").arg(revision.badge, revision.label, revision.meta));

                // Each action is drawn only where it can do its job: a save
                // record can be compared but not put back, and the lines the
                // screen shows when there is nothing to list can do neither.
                if (m_revision.canDiff) {
                    m_diff = new DiffButton(diffText, this);
                    m_diff->setObjectName(QStringLiteral("historyDiff_%1").arg(revision.id));
                    m_diff->setAccessibleName(HistoryScreen::tr("Compare revision: %1").arg(revision.label));
                    m_diff->setFixedHeight(ActionHeight);
                    layout->addWidget(m_diff);
                }
                if (m_revision.canRestore) {
                    m_restore = new TonalButton(QStringLiteral("restore"), restoreText, this);
                    m_restore->setObjectName(QStringLiteral("historyRestore_%1").arg(revision.id));
                    m_restore->setAccessibleName(HistoryScreen::tr("Restore revision: %1").arg(revision.label));
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
            QCheckBox* selection() const { return m_select; }

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
                const QFont labelFont = theme()->font(TypeRole::LabelLarge);
                const QFont metaLineFont = metaFont();
                const QFontMetrics labelMetrics(labelFont);
                const QFontMetrics metaMetrics(metaLineFont);
                int y = (height() - labelMetrics.height() - metaMetrics.height()) / 2;

                // The design's row: kind badge, label, then the short digest at
                // the right edge in the outline colour, all on the first line.
                int labelLeft = left;
                if (!m_revision.badge.isEmpty()) {
                    QFont badgeFont = theme()->font(TypeRole::LabelSmall);
                    badgeFont.setWeight(QFont::Bold);
                    badgeFont.setLetterSpacing(QFont::AbsoluteSpacing, 0.5);
                    const QFontMetrics badgeMetrics(badgeFont);
                    const int badgeWidth = badgeMetrics.horizontalAdvance(m_revision.badge) + 2 * BadgePaddingX;
                    const QRect badgeRect(left, y + (labelMetrics.height() - BadgeHeight) / 2, badgeWidth, BadgeHeight);
                    paintSurface(&painter, badgeRect, BadgeRadius, tintContainer(m_revision.tint));
                    painter.setFont(badgeFont);
                    painter.setPen(tintContent(m_revision.tint));
                    painter.drawText(badgeRect, Qt::AlignCenter, m_revision.badge);
                    labelLeft += badgeWidth + BadgeGap;
                }
                int hashWidth = 0;
                if (!m_revision.hash.isEmpty()) {
                    const QFont hashFont = theme()->font(TypeRole::Mono);
                    const QFontMetrics hashMetrics(hashFont);
                    hashWidth = hashMetrics.horizontalAdvance(m_revision.hash);
                    painter.setFont(hashFont);
                    painter.setPen(theme()->color(Role::Outline));
                    painter.drawText(QRect(right - hashWidth, y, hashWidth, labelMetrics.height()),
                                     Qt::AlignRight | Qt::AlignVCenter,
                                     m_revision.hash);
                    hashWidth += ColumnGap;
                }
                const int textWidth = qMax(0, right - left);
                const int labelWidth = qMax(0, right - hashWidth - labelLeft);
                painter.setFont(labelFont);
                painter.setPen(theme()->color(Role::OnSurface));
                painter.drawText(QRect(labelLeft, y, labelWidth, labelMetrics.height()),
                                 Qt::AlignLeft | Qt::AlignVCenter,
                                 labelMetrics.elidedText(m_revision.label, Qt::ElideRight, labelWidth));

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
            QCheckBox* m_select = nullptr;
        };
        /**
         * The append-only banner. It paints its own primary container so it
         * reads as one strip whatever sits behind it, and wraps its copy.
         */
        class AppendOnlyBanner : public QWidget
        {
        public:
            explicit AppendOnlyBanner(QWidget* parent = nullptr)
                : QWidget(parent)
            {
                auto* layout = new QHBoxLayout(this);
                layout->setContentsMargins(BannerPadding + 4, BannerPadding, BannerPadding + 4, BannerPadding);
                layout->setSpacing(10);
                m_glyph = new QLabel(this);
                m_glyph->setFixedSize(BannerGlyph, BannerGlyph);
                layout->addWidget(m_glyph, 0, Qt::AlignTop);
                m_text = new QLabel(HistoryScreen::tr("History is append-only. Restoring writes a new revision rather than "
                                                      "rewriting the branch it replaces, so an undo can itself be undone, "
                                                      "and nothing is ever discarded."),
                                    this);
                m_text->setWordWrap(true);
                m_text->setTextInteractionFlags(Qt::NoTextInteraction);
                layout->addWidget(m_text, 1);
                setAccessibleName(m_text->text());
                applyTheme();
                connect(theme(), &Theme::changed, this, [this] { applyTheme(); });
            }

        protected:
            void paintEvent(QPaintEvent*) override
            {
                QPainter painter(this);
                painter.setRenderHint(QPainter::Antialiasing);
                paintSurface(&painter, rect(), BannerRadius, theme()->color(Role::PrimaryContainer));
            }

        private:
            void applyTheme()
            {
                m_glyph->setPixmap(Icons::pixmap(QStringLiteral("info"), BannerGlyph, theme()->color(Role::OnPrimaryContainer)));
                QFont font = theme()->font(TypeRole::BodySmall);
                m_text->setFont(font);
                QPalette palette = m_text->palette();
                palette.setColor(QPalette::WindowText, theme()->color(Role::OnPrimaryContainer));
                m_text->setPalette(palette);
            }

            QLabel* m_glyph = nullptr;
            QLabel* m_text = nullptr;
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
        m_restoreChip = filterChip(QStringLiteral("restore"), tr("Restored"));
        m_entriesChip->setObjectName(QStringLiteral("historyAction_entry"));
        m_settingsChip->setObjectName(QStringLiteral("historyAction_settings"));
        m_restoreChip->setObjectName(QStringLiteral("historyAction_restore"));

        connect(m_entriesChip, &QAbstractButton::toggled, this, &HistoryScreen::filterChanged);
        connect(m_settingsChip, &QAbstractButton::toggled, this, &HistoryScreen::filterChanged);
        connect(m_restoreChip, &QAbstractButton::toggled, this, &HistoryScreen::filterChanged);
        connect(m_recentChip, &QAbstractButton::toggled, this, &HistoryScreen::filterChanged);

        m_filterPanel = new QWidget;
        m_filterPanel->setObjectName(QStringLiteral("historyFilters"));
        auto filterRow = new QHBoxLayout(m_filterPanel);
        filterRow->setContentsMargins(0, 0, 0, 0);
        filterRow->setSpacing(RowSpacing);
        filterRow->addWidget(searchBar());
        filterRow->addWidget(m_entriesChip);
        filterRow->addWidget(m_settingsChip);
        filterRow->addWidget(m_recentChip);
        filterRow->addWidget(m_restoreChip);
        filterRow->addStretch(1);
        contentLayout()->addWidget(m_filterPanel);

        // The design's append-only banner: a primary-container strip with an
        // info glyph, stating the one rule that makes the history safe to use.
        auto* banner = new AppendOnlyBanner(this);
        banner->setObjectName(QStringLiteral("historyAppendOnlyBanner"));
        contentLayout()->addWidget(banner);

        auto* dates = new QHBoxLayout;
        m_datePreset = new QComboBox;
        m_datePreset->setObjectName(QStringLiteral("historyDatePreset"));
        m_datePreset->setAccessibleName(tr("History date preset"));
        m_datePreset->addItem(tr("All dates"), QStringLiteral("all"));
        m_datePreset->addItem(tr("Last 7 days"), QStringLiteral("7"));
        m_datePreset->addItem(tr("Last 30 days"), QStringLiteral("30"));
        m_fromDate = new FlexibleDateEdit;
        m_fromDate->setObjectName(QStringLiteral("historyFromDate"));
        m_fromDate->setAccessibleName(tr("History start date; locale or ISO date"));
        m_fromDate->setCalendarPopup(true);
        m_fromDate->setDisplayFormat(QLocale().dateFormat(QLocale::ShortFormat));
        m_fromDate->setSpecialValueText(tr("Any start date"));
        m_fromDate->setMinimumDate(QDate(1970, 1, 1));
        m_fromDate->setDate(m_fromDate->minimumDate());
        m_toDate = new FlexibleDateEdit(QDate::currentDate());
        m_toDate->setObjectName(QStringLiteral("historyToDate"));
        m_toDate->setAccessibleName(tr("History end date; locale or ISO date"));
        m_toDate->setCalendarPopup(true);
        m_toDate->setDisplayFormat(QLocale().dateFormat(QLocale::ShortFormat));
        dates->addWidget(m_datePreset);
        dates->addWidget(m_fromDate);
        dates->addWidget(m_toDate);
        contentLayout()->addLayout(dates);
        connect(m_datePreset, &QComboBox::currentIndexChanged, this, [this](int index) {
            const QString value = m_datePreset->itemData(index).toString();
            m_fromDate->setDate(value == QLatin1String("all") ? m_fromDate->minimumDate()
                                                               : QDate::currentDate().addDays(-value.toInt()));
            m_toDate->setDate(QDate::currentDate());
            emit filterChanged();
        });
        connect(m_fromDate, &QDateEdit::dateChanged, this, &HistoryScreen::filterChanged);
        connect(m_toDate, &QDateEdit::dateChanged, this, &HistoryScreen::filterChanged);

        m_stateLabel = new QLabel;
        m_stateLabel->setObjectName(QStringLiteral("historyState"));
        m_stateLabel->setAccessibleName(tr("History state"));
        m_progress = new QProgressBar;
        m_progress->setObjectName(QStringLiteral("historyProgress"));
        m_progress->setAccessibleName(tr("History progress"));
        m_progress->hide();
        contentLayout()->addWidget(m_stateLabel);
        contentLayout()->addWidget(m_progress);

        m_exportSelected = new QToolButton;
        m_exportSelected->setObjectName(QStringLiteral("historyExportSelected"));
        m_exportSelected->setAccessibleName(tr("Export selected history revisions"));
        m_exportSelected->setEnabled(false);
        connect(m_exportSelected, &QToolButton::clicked, this, [this] { emit exportRequested(selectedRevisionIds()); });
        insertHeaderWidget(0, m_exportSelected);
        m_deleteUnavailable = new QToolButton;
        m_deleteUnavailable->setText(tr("Delete unavailable"));
        m_deleteUnavailable->setEnabled(false);
        m_deleteUnavailable->setToolTip(tr("History is append-only; deletion is not supported."));
        m_deleteUnavailable->setAccessibleName(tr("Delete history unavailable: history is append-only"));
        insertHeaderWidget(1, m_deleteUnavailable);

        auto list = new QWidget();
        list->setMaximumWidth(ListWidth);
        m_revisionLayout = new QVBoxLayout(list);
        m_revisionLayout->setContentsMargins(0, 0, 0, 0);
        m_revisionLayout->setSpacing(RowSpacing);
        contentLayout()->addWidget(list);
        contentLayout()->addStretch(1);

        connect(theme(), &Theme::changed, this, &HistoryScreen::rebuild);
        setState(State::Empty, tr("Nothing recorded yet."));
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

    QDate HistoryScreen::fromDate() const { return m_fromDate->date(); }
    QDate HistoryScreen::toDate() const { return m_toDate->date(); }

    QStringList HistoryScreen::actionFilters() const
    {
        QStringList actions;
        if (m_entriesChip->isChecked()) actions << QStringLiteral("entry");
        if (m_settingsChip->isChecked()) actions << QStringLiteral("settings");
        if (m_restoreChip->isChecked()) actions << QStringLiteral("restore");
        return actions;
    }

    void HistoryScreen::setActionCounts(const QHash<QString, int>& counts)
    {
        m_entriesChip->setText(tr("Entries (%1)").arg(counts.value(QStringLiteral("entry"))));
        m_settingsChip->setText(tr("Settings (%1)").arg(counts.value(QStringLiteral("settings"))));
        m_restoreChip->setText(tr("Restored (%1)").arg(counts.value(QStringLiteral("restore"))));
    }

    void HistoryScreen::setState(State state, const QString& message, int progress)
    {
        m_state = state;
        m_stateLabel->setText(message);
        m_stateLabel->setVisible(!message.isEmpty());
        const bool show = state == State::Loading || state == State::Progress;
        m_progress->setVisible(show);
        if (show) {
            m_progress->setRange(0, progress < 0 ? 0 : 100);
            if (progress >= 0) m_progress->setValue(qBound(0, progress, 100));
        }
    }

    HistoryScreen::State HistoryScreen::state() const { return m_state; }

    QStringList HistoryScreen::selectedRevisionIds() const
    {
        QStringList ids(m_selectedIds.cbegin(), m_selectedIds.cend());
        ids.sort();
        return ids;
    }

    void HistoryScreen::updateSelectionActions()
    {
        m_exportSelected->setEnabled(!m_selectedIds.isEmpty());
        m_exportSelected->setText(tr("Export %n selected revision(s)", "", m_selectedIds.size()));
    }

    void HistoryScreen::applyResponsiveLayout()
    {
        const bool compact = width() < 840;
        m_filterPanel->setMaximumWidth(compact ? width() : ListWidth);
        searchBar()->setMaximumWidth(compact ? qMax(220, width() - 40) : SearchMaximumWidth);
    }

    void HistoryScreen::resizeEvent(QResizeEvent* event)
    {
        Screen::resizeEvent(event);
        applyResponsiveLayout();
    }

    void HistoryScreen::rebuild()
    {
        clearLayout(m_revisionLayout);
        m_selectedIds.clear();
        for (const auto& revision : m_revisions) {
            auto row = new RevisionRow(revision, tr("Diff"), tr("Restore"));
            row->setObjectName(QStringLiteral("historyRevision_%1").arg(revision.id));
            const QString id = revision.id;
            if (row->diffButton()) {
                connect(row->diffButton(), &QAbstractButton::clicked, this, [this, id] { emit diffRequested(id); });
            }
            if (row->restoreButton()) {
                connect(
                    row->restoreButton(), &QAbstractButton::clicked, this, [this, id] { emit restoreRequested(id); });
            }
            connect(row->selection(), &QCheckBox::toggled, this, [this, id](bool checked) {
                if (checked) m_selectedIds.insert(id); else m_selectedIds.remove(id);
                updateSelectionActions();
            });
            m_revisionLayout->addWidget(row);
        }
        updateSelectionActions();
    }

} // namespace Material
