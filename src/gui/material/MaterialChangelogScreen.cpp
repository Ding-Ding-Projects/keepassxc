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

#include "MaterialChangelogScreen.h"

#include "MaterialButtons.h"
#include "MaterialCard.h"
#include "MaterialElevation.h"
#include "MaterialSearchBar.h"
#include "MaterialRegexSafety.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QDateEdit>
#include <QComboBox>
#include <QLocale>
#include <QLineEdit>
#include <QPainter>
#include <QTextDocument>
#include <QDesktopServices>
#include <QUrl>
#include <QVBoxLayout>

namespace Material
{
    namespace
    {
        constexpr int TagWidth = 74;
        constexpr int TagGap = 12;
        constexpr int ItemPaddingY = 7;
        constexpr int RowSpacing = 10;
        constexpr int CardSpacing = 14;
        constexpr int HeadSpacing = 12;
        constexpr int StatusPillHeight = 26;
        constexpr int StatusPillPadding = 10;
        constexpr int ExportSymbolSize = 20;
        constexpr int ListWidth = 980;
        constexpr int SearchMaximumWidth = 520;

        /** The release heading, which the type scale has no role for. */
        constexpr int VersionSizePx = 20;
        /** The size TypeRole::BodyMedium is defined at, the scale's reference. */
        constexpr int BodySizePx = 14;

        /**
         * The 20px medium release heading.
         *
         * Derived from BodyMedium rather than written as a point size, so the
         * accessibility font size setting still moves it with everything else.
         */
        QFont versionFont()
        {
            QFont font = theme()->font(TypeRole::BodyMedium);
            font.setPointSize(qMax(1, qRound(font.pointSize() * double(VersionSizePx) / BodySizePx)));
            font.setWeight(QFont::Medium);
            return font;
        }

        /** The release date is monospace at the 13px secondary size. */
        QFont dateFont()
        {
            QFont font = theme()->font(TypeRole::BodySmall);
            font.setFamily(Theme::monoFamily());
            return font;
        }

        void tint(QLabel* label, Role role)
        {
            label->setStyleSheet(QStringLiteral("color: %1; background: transparent;").arg(theme()->hex(role)));
        }

        void clearLayout(QLayout* layout)
        {
            while (QLayoutItem* item = layout->takeAt(0)) {
                delete item->widget();
                delete item;
            }
        }

        bool matches(const ChangeItem& item, const QString& query)
        {
            return item.tag.contains(query, Qt::CaseInsensitive) || item.text.contains(query, Qt::CaseInsensitive);
        }

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

        /** One change: a fixed width tag pill and the wrapping description. */
        class ChangeRow : public QWidget
        {
        public:
            explicit ChangeRow(const ChangeItem& item, QWidget* parent = nullptr)
                : QWidget(parent)
                , m_item(item)
            {
                QSizePolicy policy(QSizePolicy::Preferred, QSizePolicy::Minimum);
                policy.setHeightForWidth(true);
                setSizePolicy(policy);
            }

            bool hasHeightForWidth() const override
            {
                return true;
            }

            int heightForWidth(int width) const override
            {
                const int textWidth = qMax(1, width - TagWidth - TagGap);
                QTextDocument document;
                document.setDefaultFont(theme()->font(TypeRole::BodySmall));
                document.setMarkdown(m_item.text, QTextDocument::MarkdownDialectGitHub);
                document.setTextWidth(textWidth);
                const int textHeight = qCeil(document.size().height());
                return qMax(textHeight, tagHeight()) + ItemPaddingY * 2;
            }

            QSize sizeHint() const override
            {
                const int width = TagWidth + TagGap + 360;
                return {width, heightForWidth(width)};
            }

            QSize minimumSizeHint() const override
            {
                const int width = TagWidth + TagGap + 120;
                return {width, heightForWidth(width)};
            }

        protected:
            void paintEvent(QPaintEvent* event) override
            {
                Q_UNUSED(event)
                QPainter painter(this);
                painter.setRenderHint(QPainter::Antialiasing);

                const QFont tagFont = theme()->font(TypeRole::LabelSmall);
                const QFontMetrics tagMetrics(tagFont);
                const QRect tagRect(0, ItemPaddingY, TagWidth, tagHeight());
                QColor container = pillContainerColor(m_item.tint);
                if (!container.isValid()) {
                    container = theme()->color(Role::SurfaceContainerHigh);
                }
                paintSurface(&painter, tagRect, Shape::ExtraSmall, container, pillBorderColor(m_item.tint));

                painter.setFont(tagFont);
                painter.setPen(pillContentColor(m_item.tint));
                painter.drawText(
                    tagRect, Qt::AlignCenter, tagMetrics.elidedText(m_item.tag, Qt::ElideRight, TagWidth - 8));

                const QRect textRect(
                    TagWidth + TagGap, ItemPaddingY, qMax(1, width() - TagWidth - TagGap), height() - ItemPaddingY * 2);
                painter.save();
                painter.translate(textRect.topLeft());
                QTextDocument document;
                document.setDefaultFont(theme()->font(TypeRole::BodySmall));
                document.setDefaultStyleSheet(QStringLiteral("body { color: %1; }").arg(theme()->hex(Role::OnSurface)));
                document.setMarkdown(m_item.text, QTextDocument::MarkdownDialectGitHub);
                document.setTextWidth(textRect.width());
                document.drawContents(&painter, QRectF(0, 0, textRect.width(), textRect.height()));
                painter.restore();
            }

        private:
            int tagHeight() const
            {
                return QFontMetrics(theme()->font(TypeRole::LabelSmall)).height() + 6;
            }

            ChangeItem m_item;
        };

        /**
         * The release status chip.
         *
         * PillLabel is fixed at 32px with 14px padding, which is right for the
         * spec sheet rows it was built for; the design draws this one at 26px
         * with 10px, so only the geometry is restated here.
         */
        class StatusPill : public PillLabel
        {
        public:
            StatusPill(PillKind kind, const QString& text, QWidget* parent = nullptr)
                : PillLabel(kind, text, parent)
            {
            }

            QSize sizeHint() const override
            {
                const QFontMetrics metrics(theme()->font(TypeRole::LabelMedium));
                return {2 * StatusPillPadding + metrics.horizontalAdvance(text()), StatusPillHeight};
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

                const QFont font = theme()->font(TypeRole::LabelMedium);
                const QFontMetrics metrics(font);
                const QRect textRect = rect().adjusted(StatusPillPadding, 0, -StatusPillPadding, 0);
                painter.setFont(font);
                painter.setPen(pillContentColor(pillKind()));
                painter.drawText(
                    textRect, Qt::AlignCenter, metrics.elidedText(text(), Qt::ElideRight, textRect.width()));
            }
        };

        /**
         * A rounded-28 release card.
         *
         * The design fills the card and outlines it; Card paints a border only
         * for the Outlined variant, which is transparent, so the hairline is
         * restated here rather than losing one of the two.
         */
        class ReleaseCard : public Card
        {
        public:
            explicit ReleaseCard(QWidget* parent = nullptr)
                : Card(Card::Variant::Filled, Material::Shape::ExtraLarge, parent)
            {
                setFillRole(Role::SurfaceContainerLow);
            }

        protected:
            void paintEvent(QPaintEvent* event) override
            {
                Q_UNUSED(event)
                QPainter painter(this);
                painter.setRenderHint(QPainter::Antialiasing);
                paintSurface(
                    &painter, rect(), radius(), theme()->color(fillRole()), theme()->color(Role::OutlineVariant));
            }
        };

        /** A rounded-28 release card holding @p items, which may be a filtered subset. */
        Card* buildReleaseCard(const Release& release, const QVector<ChangeItem>& items)
        {
            auto card = new ReleaseCard();
            card->contentLayout()->setSpacing(0);

            auto head = new QHBoxLayout();
            head->setContentsMargins(0, 0, 0, 0);
            head->setSpacing(TagGap);

            auto version = new QLabel(release.version);
            version->setFont(versionFont());
            tint(version, Role::OnSurface);
            head->addWidget(version);

            if (!release.status.isEmpty()) {
                head->addWidget(new StatusPill(release.statusTint, release.status));
            }
            head->addStretch(1);

            auto date = new QLabel(release.date);
            date->setFont(dateFont());
            date->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            tint(date, Role::OnSurfaceVariant);
            head->addWidget(date);

            auto commit = new QLabel;
            commit->setObjectName(QStringLiteral("changelogCommit_%1").arg(release.version));
            commit->setTextFormat(Qt::RichText);
            commit->setOpenExternalLinks(false);
            if (release.commitAvailable) {
                commit->setText(QStringLiteral("<a href=\"%1\">%2</a>").arg(release.commitUrl.toHtmlEscaped(), release.commitSha.left(12).toHtmlEscaped()));
                commit->setAccessibleName(ChangelogScreen::tr("Open completion commit %1").arg(release.commitSha));
                QObject::connect(commit, &QLabel::linkActivated, commit, [](const QString& link) { QDesktopServices::openUrl(QUrl(link)); });
            } else {
                commit->setText(ChangelogScreen::tr("Completion commit unavailable"));
                commit->setAccessibleName(commit->text());
            }
            card->contentLayout()->addWidget(commit);

            card->contentLayout()->addLayout(head);
            card->contentLayout()->addSpacing(HeadSpacing);

            for (const auto& item : items) {
                card->contentLayout()->addWidget(new ChangeRow(item));
            }
            return card;
        }
    } // namespace

    ChangelogScreen::ChangelogScreen(QWidget* parent)
        : Screen(parent)
    {
        setHeadline(tr("Changelog"));

        auto exportButton = new OutlinedButton(QStringLiteral("download"), tr("Export Markdown"));
        exportButton->setSymbolSize(ExportSymbolSize);
        connect(exportButton, &QAbstractButton::clicked, this, &ChangelogScreen::exportRequested);
        addHeaderWidget(exportButton);
        auto copyButton = new OutlinedButton(QStringLiteral("content_copy"), tr("Copy filtered"));
        copyButton->setObjectName(QStringLiteral("changelogCopyFiltered"));
        connect(copyButton, &QAbstractButton::clicked, this, &ChangelogScreen::copyRequested);
        addHeaderWidget(copyButton);

        setSearchVisible(true);
        searchBar()->setPlaceholder(tr("Search changelog text"));
        searchBar()->setIdentity(QStringLiteral("changelog.entries"), tr("Changelog search"));
        searchBar()->setMaximumWidth(SearchMaximumWidth);
        searchBar()->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        connect(searchBar(), &SearchBar::textChanged, this, [this](const QString& text) {
            m_query = text.trimmed();
            rebuild();
        });
        connect(searchBar(), &SearchBar::regexToggled, this, &ChangelogScreen::rebuild);

        m_dateChip = new Chip(QStringLiteral("calendar_month"), QString(), Chip::Kind::Assist);
        // The design lines the chip up with the search bar beside it.
        m_dateChip->setFixedHeight(Layout::SurfaceSearchHeight);
        m_countLabel = new QLabel();

        auto filterRow = new QHBoxLayout();
        filterRow->setContentsMargins(0, 0, 0, 0);
        filterRow->setSpacing(RowSpacing);
        filterRow->addWidget(searchBar());
        filterRow->addWidget(m_dateChip);
        filterRow->addWidget(m_countLabel);
        filterRow->addStretch(1);
        contentLayout()->addLayout(filterRow);

        auto dates = new QHBoxLayout;
        m_datePreset = new QComboBox;
        m_datePreset->setObjectName(QStringLiteral("changelogDatePreset"));
        m_datePreset->setAccessibleName(tr("Changelog date preset"));
        m_datePreset->addItem(tr("All dates"), QStringLiteral("all"));
        m_datePreset->addItem(tr("Last year"), QStringLiteral("365"));
        m_datePreset->addItem(tr("Last 5 years"), QStringLiteral("1825"));
        m_fromDate = new FlexibleDateEdit;
        m_fromDate->setObjectName(QStringLiteral("changelogFromDate"));
        m_fromDate->setCalendarPopup(true);
        m_fromDate->setDisplayFormat(QLocale().dateFormat(QLocale::ShortFormat));
        m_fromDate->setMinimumDate(QDate(1970, 1, 1));
        m_fromDate->setSpecialValueText(tr("Any start date"));
        m_fromDate->setDate(m_fromDate->minimumDate());
        m_toDate = new FlexibleDateEdit(QDate::currentDate());
        m_toDate->setObjectName(QStringLiteral("changelogToDate"));
        m_toDate->setCalendarPopup(true);
        m_toDate->setDisplayFormat(QLocale().dateFormat(QLocale::ShortFormat));
        dates->addWidget(m_datePreset); dates->addWidget(m_fromDate); dates->addWidget(m_toDate); dates->addStretch(1);
        contentLayout()->addLayout(dates);
        connect(m_datePreset, &QComboBox::currentIndexChanged, this, [this](int index) {
            const QString value = m_datePreset->itemData(index).toString();
            m_fromDate->setDate(value == QLatin1String("all") ? m_fromDate->minimumDate() : QDate::currentDate().addDays(-value.toInt()));
            m_toDate->setDate(QDate::currentDate()); rebuild();
        });
        connect(m_fromDate, &QDateEdit::dateChanged, this, &ChangelogScreen::rebuild);
        connect(m_toDate, &QDateEdit::dateChanged, this, &ChangelogScreen::rebuild);
        m_stateLabel = new QLabel;
        m_stateLabel->setObjectName(QStringLiteral("changelogState"));
        m_stateLabel->setAccessibleName(tr("Changelog state"));
        contentLayout()->addWidget(m_stateLabel);

        auto list = new QWidget();
        list->setMaximumWidth(ListWidth);
        m_releaseLayout = new QVBoxLayout(list);
        m_releaseLayout->setContentsMargins(0, 0, 0, 0);
        m_releaseLayout->setSpacing(CardSpacing);
        contentLayout()->addWidget(list);
        contentLayout()->addStretch(1);

        connect(theme(), &Theme::changed, this, &ChangelogScreen::rebuild);
        rebuild();
    }

    ChangelogScreen::~ChangelogScreen() = default;

    void ChangelogScreen::setReleases(const QVector<Release>& releases)
    {
        m_releases = releases;
        rebuild();
    }

    void ChangelogScreen::rebuild()
    {
        clearLayout(m_releaseLayout);

        int shown = 0;
        QStringList dates;
        for (const auto& release : filteredReleases()) {
            const QVector<ChangeItem> items = release.items;
            m_releaseLayout->addWidget(buildReleaseCard(release, items));
            dates.append(release.date);
            ++shown;
        }

        m_countLabel->setText(tr("%1 of %2 releases shown").arg(shown).arg(m_releases.size()));
        m_countLabel->setFont(theme()->font(TypeRole::BodySmall));
        tint(m_countLabel, Role::OnSurfaceVariant);
        // The chip and the count sit in the same row and describe the same set,
        // so the range is taken from the cards that were actually added.
        updateDateRange(dates);
        m_stateLabel->setText(shown == 0 ? tr("No release matches the active search and date range.")
                                         : tr("Bundled changelog loaded: %n release(s) shown.", "", shown));
    }

    QDate ChangelogScreen::fromDate() const { return m_fromDate->date(); }
    QDate ChangelogScreen::toDate() const { return m_toDate->date(); }

    QVector<Release> ChangelogScreen::filteredReleases() const
    {
        QVector<Release> shown;
        bool valid = true;
        if (searchBar()->isRegexEnabled() && !m_query.isEmpty()) {
            const auto check = runBounded(m_query, optionsForFlags(searchBar()->regexFlags()), QString());
            valid = check.compiled && !check.blocked && !check.timedOut;
            searchBar()->lineEdit()->setAccessibleDescription(valid ? tr("Changelog filter is valid") : tr("Invalid regular expression: %1").arg(check.error));
        }
        if (!valid) return shown;
        const auto textMatches = [this](const QString& text) {
            if (m_query.isEmpty()) return true;
            if (!searchBar()->isRegexEnabled()) return text.contains(m_query, Qt::CaseInsensitive);
            return !runBounded(m_query, optionsForFlags(searchBar()->regexFlags()), text).matches.isEmpty();
        };
        for (const auto& release : m_releases) {
            const QDate date = QDate::fromString(release.date, Qt::ISODate);
            if (date.isValid() && (date < fromDate() || date > toDate())) continue;
            Release filtered = release;
            if (!m_query.isEmpty() && !textMatches(release.version) && !textMatches(release.commitSha)) {
                filtered.items.clear();
                for (const auto& item : release.items) if (textMatches(item.tag) || textMatches(item.text)) filtered.items.append(item);
                if (filtered.items.isEmpty()) continue;
            }
            shown.append(filtered);
        }
        return shown;
    }

    void ChangelogScreen::updateDateRange(const QStringList& dates)
    {
        QString first;
        QString last;
        for (const auto& date : dates) {
            if (date.isEmpty()) {
                continue;
            }
            if (first.isEmpty() || date < first) {
                first = date;
            }
            if (last.isEmpty() || date > last) {
                last = date;
            }
        }

        m_dateChip->setVisible(!first.isEmpty());
        if (!first.isEmpty()) {
            m_dateChip->setText(QStringLiteral("%1 %2 %3").arg(first, QString(QChar(0x2192)), last));
        }
    }

} // namespace Material
