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

#include "MaterialDateField.h"
#include "MaterialSelect.h"
#include "MaterialVaultSidebar.h"

#include "MaterialButtons.h"
#include "MaterialElevation.h"
#include "MaterialIcons.h"
#include "MaterialSearchBar.h"

#include <QHBoxLayout>
#include <QCheckBox>
#include <QDateEdit>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPainter>
#include <functional>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainterPath>
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
        constexpr int DetailWidth = 392;
        constexpr int DetailRadius = 28;
        constexpr int DetailBreakpoint = 840; // the shell's Expanded class
        constexpr int ColumnGapWide = 16;
        constexpr int BadgeChipHeight = 24;
        constexpr int DiffRadius = 12;
        constexpr int DiffMarkWidth = 12;
        constexpr int FooterButtonHeight = 44;


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
                setCursor(revision.id.isEmpty() ? Qt::ArrowCursor : Qt::PointingHandCursor);
                setFocusPolicy(revision.id.isEmpty() ? Qt::NoFocus : Qt::TabFocus);
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

            /** The row the detail card describes paints a secondary container. */
            void setCurrentRow(bool current)
            {
                m_current = current;
                update();
            }

            bool isCurrentRow() const
            {
                return m_current;
            }

            /** Hide the inline actions while the detail card carries them. */
            void setActionsVisible(bool visible)
            {
                if (m_diff) m_diff->setVisible(visible);
                if (m_restore) m_restore->setVisible(visible);
            }

            std::function<void()> onActivated;

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
                             theme()->color(m_current ? Role::SecondaryContainer : Role::SurfaceContainerLow),
                             theme()->color(m_current ? Role::SecondaryContainer : Role::OutlineVariant));
                if (hasFocus()) {
                    painter.setPen(QPen(theme()->color(Role::Primary), 2));
                    painter.setBrush(Qt::NoBrush);
                    painter.drawRoundedRect(QRectF(rect()).adjusted(1, 1, -1, -1), RowRadius, RowRadius);
                }

                const QRect circleRect(RowPaddingX, (height() - CircleSize) / 2, CircleSize, CircleSize);
                paintSurface(&painter, circleRect, Shape::Full, tintContainer(m_revision.tint));

                const QPixmap glyph = Icons::pixmap(m_revision.symbol, GlyphSize, tintContent(m_revision.tint));
                const int inset = (CircleSize - GlyphSize) / 2;
                painter.drawPixmap(circleRect.topLeft() + QPoint(inset, inset), glyph);

                // The text stops at whichever action comes first, so a row that
                // only carries a Restore gives its label the same room.
                // Hidden actions (the detail card carries them) give the text the whole row.
                const ButtonBase* leading = m_diff && !m_diff->isHidden() ? m_diff
                                            : m_restore && !m_restore->isHidden() ? m_restore
                                                                                    : nullptr;
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
            void mousePressEvent(QMouseEvent* event) override
            {
                if (event->button() == Qt::LeftButton && onActivated) {
                    onActivated();
                }
                QWidget::mousePressEvent(event);
            }

            void keyPressEvent(QKeyEvent* event) override
            {
                if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter || event->key() == Qt::Key_Space)
                    && onActivated) {
                    onActivated();
                    return;
                }
                QWidget::keyPressEvent(event);
            }

            bool m_current = false;
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

    // -------------------------------------------------------------- DetailCard

    /**
     * The design's detail card: a secondary-container header with the kind
     * badge, the short digest, the label and the timestamp; then Record, What
     * changed, the Diff lines, the note about what the log does not hold, and
     * the Restore / Compare / Export actions.
     */
    class HistoryScreen::DetailCard : public QWidget
    {
    public:
        explicit DetailCard(QWidget* parent = nullptr)
            : QWidget(parent)
        {
            setFixedWidth(DetailWidth);
            auto* column = new QVBoxLayout(this);
            column->setContentsMargins(0, 0, 0, 0);
            column->setSpacing(0);

            m_header = new QWidget(this);
            auto* header = new QVBoxLayout(m_header);
            header->setContentsMargins(22, 20, 22, 16);
            header->setSpacing(4);
            auto* line = new QHBoxLayout;
            line->setSpacing(8);
            m_badge = new QLabel(m_header);
            m_badge->setObjectName(QStringLiteral("historyDetailBadge"));
            m_badge->setFixedHeight(BadgeChipHeight);
            m_badge->setContentsMargins(10, 0, 10, 0);
            m_badge->setAlignment(Qt::AlignCenter);
            line->addWidget(m_badge, 0);
            m_hash = new QLabel(m_header);
            m_hash->setObjectName(QStringLiteral("historyDetailHash"));
            line->addWidget(m_hash, 0);
            line->addStretch(1);
            header->addLayout(line);
            m_label = new QLabel(m_header);
            m_label->setObjectName(QStringLiteral("historyDetailLabel"));
            m_label->setWordWrap(true);
            header->addWidget(m_label);
            m_when = new QLabel(m_header);
            m_when->setObjectName(QStringLiteral("historyDetailWhen"));
            header->addWidget(m_when);
            column->addWidget(m_header);

            m_body = new QWidget(this);
            auto* body = new QVBoxLayout(m_body);
            body->setContentsMargins(18, 16, 18, 20);
            body->setSpacing(0);
            m_recordTitle = overline(HistoryScreen::tr("Record"));
            body->addWidget(m_recordTitle);
            m_record = new QLabel(m_body);
            m_record->setObjectName(QStringLiteral("historyDetailRecord"));
            m_record->setWordWrap(true);
            body->addWidget(m_record);
            body->addSpacing(16);
            m_detailTitle = overline(HistoryScreen::tr("What changed"));
            body->addWidget(m_detailTitle);
            m_detail = new QLabel(m_body);
            m_detail->setObjectName(QStringLiteral("historyDetailWhat"));
            m_detail->setWordWrap(true);
            body->addWidget(m_detail);
            body->addSpacing(16);
            m_diffTitle = overline(HistoryScreen::tr("Diff"));
            body->addWidget(m_diffTitle);
            m_diffBox = new QWidget(m_body);
            m_diffBox->setObjectName(QStringLiteral("historyDetailDiff"));
            m_diffLayout = new QVBoxLayout(m_diffBox);
            m_diffLayout->setContentsMargins(0, 0, 0, 0);
            m_diffLayout->setSpacing(0);
            body->addWidget(m_diffBox);
            body->addSpacing(18);
            m_note = new QLabel(HistoryScreen::tr("The plaintext revision log records what changed and which record it "
                                                   "belongs to. It never records entry content, passwords or attachment "
                                                   "bytes; those stay in the encrypted snapshot."),
                                m_body);
            m_note->setObjectName(QStringLiteral("historyDetailNote"));
            m_note->setWordWrap(true);
            m_note->setContentsMargins(14, 12, 14, 12);
            body->addWidget(m_note);
            body->addSpacing(16);

            auto* footer = new QHBoxLayout;
            footer->setSpacing(8);
            m_restore = new FilledButton(QStringLiteral("restore"), HistoryScreen::tr("Restore"), m_body);
            m_restore->setObjectName(QStringLiteral("historyDetailRestore"));
            m_restore->setSymbolSize(19);
            m_restore->setFixedHeight(FooterButtonHeight);
            m_restore->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            footer->addWidget(m_restore, 1);
            m_compare = new IconButton(QStringLiteral("difference"), m_body);
            m_compare->setObjectName(QStringLiteral("historyDetailCompare"));
            m_compare->setDiameter(FooterButtonHeight);
            m_compare->setToolTip(HistoryScreen::tr("Compare with the entry as it stands"));
            m_compare->setAccessibleName(HistoryScreen::tr("Compare revision"));
            footer->addWidget(m_compare, 0);
            m_export = new IconButton(QStringLiteral("download"), m_body);
            m_export->setObjectName(QStringLiteral("historyDetailExport"));
            m_export->setDiameter(FooterButtonHeight);
            m_export->setToolTip(HistoryScreen::tr("Export this revision"));
            m_export->setAccessibleName(HistoryScreen::tr("Export revision"));
            footer->addWidget(m_export, 0);
            body->addLayout(footer);
            column->addWidget(m_body);

            m_empty = new QLabel(HistoryScreen::tr("Select a revision to see what changed, which record it belongs to "
                                                    "and what restoring it would do."),
                                 this);
            m_empty->setObjectName(QStringLiteral("historyDetailEmpty"));
            m_empty->setWordWrap(true);
            m_empty->setAlignment(Qt::AlignCenter);
            m_empty->setContentsMargins(24, 40, 24, 40);
            column->addWidget(m_empty);
            column->addStretch(1);

            applyTheme();
            connect(theme(), &Theme::changed, this, [this] { applyTheme(); });
            clearRevision();
        }

        void setRevision(const Revision& revision)
        {
            m_revision = revision;
            m_hasRevision = true;
            m_badge->setText(revision.badge.isEmpty() ? HistoryScreen::tr("REVISION") : revision.badge);
            m_hash->setText(revision.hash);
            m_label->setText(revision.label);
            m_when->setText(revision.timestamp.isValid() ? revision.timestamp.toUTC().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss 'UTC'"))
                                                         : revision.meta);
            m_record->setText(revision.record.isEmpty() ? HistoryScreen::tr("Not recorded") : revision.record);
            m_detail->setText(revision.detail.isEmpty() ? revision.meta : revision.detail);
            rebuildDiff(revision.diff);
            m_restore->setText(HistoryScreen::tr("Restore %1").arg(revision.hash.isEmpty() ? revision.label : revision.hash));
            m_restore->setEnabled(revision.canRestore);
            m_restore->setToolTip(revision.canRestore ? QString()
                                                      : HistoryScreen::tr("This revision cannot be put back; it describes a save, not an entry."));
            m_restore->setAccessibleName(HistoryScreen::tr("Restore revision: %1").arg(revision.label));
            m_compare->setEnabled(revision.canDiff);
            m_export->setEnabled(!revision.id.isEmpty());
            setAccessibleName(HistoryScreen::tr("%1: %2").arg(m_badge->text(), revision.label));
            m_header->show();
            m_body->show();
            m_empty->hide();
            update();
        }

        void clearRevision()
        {
            m_revision = Revision();
            m_hasRevision = false;
            m_header->hide();
            m_body->hide();
            m_empty->show();
            setAccessibleName(m_empty->text());
            update();
        }

        QAbstractButton* restoreButton() const
        {
            return m_restore;
        }

        QAbstractButton* compareButton() const
        {
            return m_compare;
        }

        QAbstractButton* exportButton() const
        {
            return m_export;
        }

        QStringList diffTexts() const
        {
            QStringList texts;
            for (QLabel* line : m_diffLines) {
                texts << line->text();
            }
            return texts;
        }

    protected:
        void paintEvent(QPaintEvent*) override
        {
            QPainter painter(this);
            painter.setRenderHint(QPainter::Antialiasing);
            paintSurface(&painter, rect(), DetailRadius, theme()->color(Role::SurfaceContainerLow));
            if (m_hasRevision) {
                // The header band, clipped to the card's top corners.
                QPainterPath clip;
                clip.addRoundedRect(QRectF(rect()), DetailRadius, DetailRadius);
                painter.setClipPath(clip);
                painter.fillRect(m_header->geometry(), theme()->color(Role::SecondaryContainer));
            }
        }

    private:
        QLabel* overline(const QString& text)
        {
            auto* label = new QLabel(text, this);
            label->setContentsMargins(0, 0, 0, 7);
            m_overlines << label;
            return label;
        }

        void rebuildDiff(const QStringList& lines)
        {
            qDeleteAll(m_diffLines);
            m_diffLines.clear();
            qDeleteAll(m_diffBox->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly));
            const bool any = !lines.isEmpty();
            m_diffTitle->setVisible(any);
            m_diffBox->setVisible(any);
            for (const QString& raw : lines) {
                const QChar mark = raw.isEmpty() ? QLatin1Char(' ') : raw.at(0);
                const QString text = raw.mid(1).trimmed();
                auto* row = new QWidget(m_diffBox);
                auto* layout = new QHBoxLayout(row);
                layout->setContentsMargins(12, 7, 12, 7);
                layout->setSpacing(9);
                auto* markLabel = new QLabel(QString(mark), row);
                markLabel->setFixedWidth(DiffMarkWidth);
                auto* textLabel = new QLabel(text, row);
                textLabel->setWordWrap(true);
                textLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
                textLabel->setAccessibleName(mark == QLatin1Char('+') ? HistoryScreen::tr("Added: %1").arg(text)
                                             : mark == QLatin1Char('-') ? HistoryScreen::tr("Removed: %1").arg(text)
                                                                        : text);
                layout->addWidget(markLabel, 0, Qt::AlignTop);
                layout->addWidget(textLabel, 1);
                Role fill = Role::SurfaceContainer;
                Role ink = Role::OnSurface;
                Role markInk = Role::Outline;
                if (mark == QLatin1Char('+')) {
                    fill = Role::GreenContainer; ink = Role::OnGreenContainer; markInk = Role::Green;
                } else if (mark == QLatin1Char('-')) {
                    fill = Role::ErrorContainer; ink = Role::OnErrorContainer; markInk = Role::Error;
                }
                row->setAutoFillBackground(true);
                QPalette palette = row->palette();
                palette.setColor(QPalette::Window, theme()->color(fill));
                row->setPalette(palette);
                styleLabel(markLabel, monoFont(), theme()->color(markInk), true);
                styleLabel(textLabel, monoFont(), theme()->color(ink), false);
                m_diffLayout->addWidget(row);
                m_diffLines << textLabel;
            }
        }

        static QFont monoFont()
        {
            QFont font = theme()->font(TypeRole::Mono);
            font.setPointSizeF(qMax(7.0, font.pointSizeF() * 0.9));
            return font;
        }

        static void styleLabel(QLabel* label, const QFont& font, const QColor& color, bool bold)
        {
            QFont copy = font;
            if (bold) copy.setWeight(QFont::Bold);
            label->setFont(copy);
            QPalette palette = label->palette();
            palette.setColor(QPalette::WindowText, color);
            label->setPalette(palette);
        }

        void applyTheme()
        {
            const QColor onHeader = theme()->color(Role::OnSecondaryContainer);
            QFont badgeFont = theme()->font(TypeRole::LabelSmall);
            badgeFont.setWeight(QFont::Medium);
            badgeFont.setCapitalization(QFont::AllUppercase);
            badgeFont.setLetterSpacing(QFont::AbsoluteSpacing, 0.6);
            styleLabel(m_badge, badgeFont, onHeader, false);
            m_badge->setStyleSheet(QStringLiteral("background: rgba(128,128,128,56); border-radius: 6px;"));
            styleLabel(m_hash, monoFont(), onHeader, false);
            styleLabel(m_label, theme()->font(TypeRole::TitleMedium), onHeader, false);
            styleLabel(m_when, monoFont(), onHeader, false);
            QFont overlineFont = theme()->font(TypeRole::LabelSmall);
            overlineFont.setCapitalization(QFont::AllUppercase);
            overlineFont.setWeight(QFont::Medium);
            overlineFont.setLetterSpacing(QFont::AbsoluteSpacing, 0.9);
            for (QLabel* label : std::as_const(m_overlines)) {
                styleLabel(label, overlineFont, theme()->color(Role::OnSurfaceVariant), false);
            }
            styleLabel(m_record, theme()->font(TypeRole::BodyMedium), theme()->color(Role::OnSurface), false);
            styleLabel(m_detail, theme()->font(TypeRole::BodySmall), theme()->color(Role::OnSurface), false);
            styleLabel(m_note, theme()->font(TypeRole::BodySmall), theme()->color(Role::OnSurfaceVariant), false);
            m_note->setStyleSheet(QStringLiteral("background: %1; border-radius: %2px;")
                                      .arg(theme()->hex(Role::SurfaceContainer))
                                      .arg(DiffRadius));
            m_diffBox->setStyleSheet(QStringLiteral("#historyDetailDiff { border: 1px solid %1; border-radius: %2px; }")
                                         .arg(theme()->hex(Role::OutlineVariant))
                                         .arg(DiffRadius));
            m_diffBox->setAttribute(Qt::WA_StyledBackground, true);
            styleLabel(m_empty, theme()->font(TypeRole::BodyMedium), theme()->color(Role::OnSurfaceVariant), false);
            if (m_hasRevision) {
                rebuildDiff(m_revision.diff);
            }
            update();
        }

        Revision m_revision;
        bool m_hasRevision = false;
        QWidget* m_header = nullptr;
        QWidget* m_body = nullptr;
        QLabel* m_badge = nullptr;
        QLabel* m_hash = nullptr;
        QLabel* m_label = nullptr;
        QLabel* m_when = nullptr;
        QLabel* m_recordTitle = nullptr;
        QLabel* m_record = nullptr;
        QLabel* m_detailTitle = nullptr;
        QLabel* m_detail = nullptr;
        QLabel* m_diffTitle = nullptr;
        QWidget* m_diffBox = nullptr;
        QVBoxLayout* m_diffLayout = nullptr;
        QList<QLabel*> m_diffLines;
        QLabel* m_note = nullptr;
        QLabel* m_empty = nullptr;
        FilledButton* m_restore = nullptr;
        IconButton* m_compare = nullptr;
        IconButton* m_export = nullptr;
        QList<QLabel*> m_overlines;
    };

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
        // The chips wrap under the search bar when the width runs out, so the
        // strip never dictates a minimum width the rest of the screen cannot meet.
        auto filterRow = new FlowLayout(m_filterPanel, RowSpacing, RowSpacing);
        filterRow->setContentsMargins(0, 0, 0, 0);
        filterRow->addWidget(searchBar());
        filterRow->addWidget(m_entriesChip);
        filterRow->addWidget(m_settingsChip);
        filterRow->addWidget(m_recentChip);
        filterRow->addWidget(m_restoreChip);
        contentLayout()->addWidget(m_filterPanel);

        // The design's append-only banner: a primary-container strip with an
        // info glyph, stating the one rule that makes the history safe to use.
        auto* banner = new AppendOnlyBanner(this);
        banner->setObjectName(QStringLiteral("historyAppendOnlyBanner"));
        contentLayout()->addWidget(banner);

        auto* dates = new QHBoxLayout;
        m_datePreset = new Select;
        m_datePreset->setObjectName(QStringLiteral("historyDatePreset"));
        m_datePreset->setAccessibleName(tr("History date preset"));
        m_datePreset->setSearchIdentity(QStringLiteral("history.date-preset"), tr("History date preset search"));
        m_datePreset->addItem(tr("All dates"), QStringLiteral("all"));
        m_datePreset->addItem(tr("Last 7 days"), QStringLiteral("7"));
        m_datePreset->addItem(tr("Last 30 days"), QStringLiteral("30"));
        m_fromDate = new DateField;
        m_fromDate->setSearchIdentity(QStringLiteral("history.from"), tr("History start date"));
        m_fromDate->setObjectName(QStringLiteral("historyFromDate"));
        m_fromDate->setAccessibleName(tr("History start date; locale or ISO date"));
        m_fromDate->setDisplayFormat(QLocale().dateFormat(QLocale::ShortFormat));
        m_fromDate->setSpecialValueText(tr("Any start date"));
        m_fromDate->setMinimumDate(QDate(1970, 1, 1));
        m_fromDate->setDate(m_fromDate->minimumDate());
        m_toDate = new DateField(QDate::currentDate());
        m_toDate->setSearchIdentity(QStringLiteral("history.to"), tr("History end date"));
        m_toDate->setObjectName(QStringLiteral("historyToDate"));
        m_toDate->setAccessibleName(tr("History end date; locale or ISO date"));
        m_toDate->setDisplayFormat(QLocale().dateFormat(QLocale::ShortFormat));
        dates->addWidget(m_datePreset);
        dates->addWidget(m_fromDate);
        dates->addWidget(m_toDate);
        contentLayout()->addLayout(dates);
        connect(m_datePreset, &Select::currentIndexChanged, this, [this](int index) {
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

        // The design's two columns: the revision list and, beside it, the
        // 392px detail card for whichever revision is current.
        auto* columns = new QHBoxLayout;
        columns->setContentsMargins(0, 0, 0, 0);
        columns->setSpacing(ColumnGapWide);
        columns->addWidget(list, 1, Qt::AlignTop);
        m_detail = new DetailCard(this);
        m_detail->setObjectName(QStringLiteral("historyDetailCard"));
        connect(m_detail->restoreButton(), &QAbstractButton::clicked, this, [this] {
            if (!m_currentId.isEmpty()) emit restoreRequested(m_currentId);
        });
        connect(m_detail->compareButton(), &QAbstractButton::clicked, this, [this] {
            if (!m_currentId.isEmpty()) emit diffRequested(m_currentId);
        });
        connect(m_detail->exportButton(), &QAbstractButton::clicked, this, [this] {
            if (!m_currentId.isEmpty()) emit exportRequested({m_currentId});
        });
        // No alignment flag: an aligned item is laid out from its size hint and
        // never asked for height-for-width, which clips its wrapped notes.
        columns->addWidget(m_detail, 0);
        contentLayout()->addLayout(columns);
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
        // Below the breakpoint the card gives way and every row carries its
        // own Diff and Restore again, so nothing becomes unreachable.
        const bool wide = width() >= DetailBreakpoint;
        m_detail->setVisible(wide);
        for (int index = 0; index < m_revisionLayout->count(); ++index) {
            if (auto* row = dynamic_cast<RevisionRow*>(m_revisionLayout->itemAt(index)->widget())) {
                row->setActionsVisible(!wide);
            }
        }
    }

    bool HistoryScreen::detailCardVisible() const
    {
        return m_detail && !m_detail->isHidden();
    }

    QString HistoryScreen::currentRevisionId() const
    {
        return m_currentId;
    }

    void HistoryScreen::setCurrentRevision(const QString& id)
    {
        m_currentId = id;
        for (int index = 0; index < m_revisionLayout->count(); ++index) {
            if (auto* row = dynamic_cast<RevisionRow*>(m_revisionLayout->itemAt(index)->widget())) {
                row->setCurrentRow(!id.isEmpty() && row->objectName() == QStringLiteral("historyRevision_") + id);
            }
        }
        updateDetailCard();
    }

    void HistoryScreen::updateDetailCard()
    {
        for (const auto& revision : m_revisions) {
            if (!revision.id.isEmpty() && revision.id == m_currentId) {
                m_detail->setRevision(revision);
                return;
            }
        }
        m_currentId.clear();
        m_detail->clearRevision();
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
            if (!id.isEmpty()) {
                row->onActivated = [this, id] { setCurrentRevision(id); };
            }
            m_revisionLayout->addWidget(row);
        }
        updateSelectionActions();
        // The newest real revision is current until the user picks another,
        // so the card is never blank while there is something to describe.
        QString current = m_currentId;
        bool stillListed = false;
        for (const auto& revision : m_revisions) {
            if (!revision.id.isEmpty() && revision.id == current) stillListed = true;
        }
        if (!stillListed) {
            current.clear();
            for (const auto& revision : m_revisions) {
                if (!revision.id.isEmpty()) {
                    current = revision.id;
                    break;
                }
            }
        }
        setCurrentRevision(current);
        applyResponsiveLayout();
    }


} // namespace Material
