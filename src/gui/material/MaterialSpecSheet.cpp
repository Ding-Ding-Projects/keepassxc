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

#include "MaterialSpecSheet.h"

#include "MaterialButtons.h"
#include "MaterialCard.h"
#include "MaterialElevation.h"
#include "MaterialIcons.h"
#include "MaterialSearchBar.h"
#include "MaterialTheme.h"
#include "MaterialRegexSafety.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QResizeEvent>
#include <QKeyEvent>

namespace Material
{
    namespace
    {
        constexpr int GlyphColumn = 24;
        constexpr int RowGap = 14;
        constexpr int PageSearchWidth = 520;
        constexpr int PageMaxWidth = 1120;
        constexpr int NoteMaxWidth = 820;

        /** Dynamic property carrying the lower-cased text a row is searched by. */
        const char* const HaystackProperty = "materialHaystack";

        /** A 12px regular line; the type scale only offers 12px in medium weight. */
        QFont subFont()
        {
            QFont font = theme()->font(TypeRole::LabelMedium);
            font.setWeight(QFont::Normal);
            return font;
        }

        /**
         * A section card.
         *
         * The design fills every section card with surfaceContainerLow, one
         * step above the page surface, and still draws the outlineVariant
         * hairline. Card paints a fill for its Filled variant and a border for
         * its Outlined one but never both, so the card paints itself.
         */
        class SectionCard : public Card
        {
        public:
            explicit SectionCard(QWidget* parent = nullptr)
                // Qualified: QFrame::Shape shadows Material::Shape in here.
                : Card(Card::Variant::Outlined, Material::Shape::ExtraLarge, parent)
            {
            }

        protected:
            void paintEvent(QPaintEvent* event) override
            {
                Q_UNUSED(event)
                QPainter painter(this);
                painter.setRenderHint(QPainter::Antialiasing, true);
                paintSurface(&painter,
                             rect(),
                             radius(),
                             theme()->color(Role::SurfaceContainerLow),
                             theme()->color(Role::OutlineVariant));
            }
        };
    } // namespace

    // -------------------------------------------------------------- SpecSheetRow

    SpecSheetRow::SpecSheetRow(const QString& key,
                               const QString& symbol,
                               const QString& label,
                               const QString& sub,
                               PillKind kind,
                               const QString& controlText,
                               QWidget* parent)
        : QWidget(parent)
        , m_key(key)
        , m_symbol(symbol)
        , m_label(label)
        , m_sub(sub)
    {
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::StrongFocus);
        setAccessibleName(m_label);
        // Keep the press here so the release is delivered to this row and not
        // swallowed by the section card underneath.
        setAttribute(Qt::WA_NoMousePropagation, true);
        setMinimumHeight(RowHeight);

        m_pill = new PillLabel(kind, controlText, this);
        m_pill->setMaximumWidth(320);

        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(RowGap);
        layout->addStretch(1);
        layout->addWidget(m_pill, 0, Qt::AlignVCenter);
    }

    SpecSheetRow::~SpecSheetRow() = default;

    QString SpecSheetRow::key() const
    {
        return m_key;
    }

    void SpecSheetRow::setPill(PillKind kind, const QString& text)
    {
        m_pill->setPillKind(kind);
        m_pill->setPillText(text);
        updateGeometry();
        update();
    }

    void SpecSheetRow::setProvenance(const QString& text)
    {
        m_provenance = text;
        setAccessibleDescription(QStringLiteral("%1. %2").arg(m_sub, m_provenance));
        setMinimumHeight(m_provenance.isEmpty() ? RowHeight : 72);
        updateGeometry();
        update();
    }

    QString SpecSheetRow::provenance() const { return m_provenance; }

    PillLabel* SpecSheetRow::pill() const
    {
        return m_pill;
    }

    QSize SpecSheetRow::sizeHint() const
    {
        const int text = QFontMetrics(theme()->font(TypeRole::BodyMedium)).horizontalAdvance(m_label);
        return QSize(GlyphColumn + RowGap + qMax(text, 220) + RowGap + m_pill->sizeHint().width(), RowHeight);
    }

    QSize SpecSheetRow::minimumSizeHint() const
    {
        return QSize(GlyphColumn + RowGap + 120 + RowGap + m_pill->minimumSizeHint().width(), RowHeight);
    }

    void SpecSheetRow::paintEvent(QPaintEvent* event)
    {
        Q_UNUSED(event)

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        if (m_hovered) {
            paintStateLayer(&painter, rect(), Shape::Medium, theme()->color(Role::OnSurface), 0.05);
        }

        // Rows are separated by the border above them, so every row draws one.
        painter.setPen(theme()->color(Role::OutlineVariant));
        painter.drawLine(0, 0, width(), 0);

        if (!m_symbol.isEmpty()) {
            const QRect glyph((GlyphColumn - 20) / 2, (height() - 20) / 2, 20, 20);
            painter.drawPixmap(glyph, Icons::pixmap(m_symbol, 20, theme()->color(Role::OnSurfaceVariant)));
        }

        const int left = GlyphColumn + RowGap;
        const int right = m_pill->isVisible() ? m_pill->geometry().left() - RowGap : width();
        const int available = qMax(0, right - left);

        const QFont labelFont = theme()->font(TypeRole::BodyMedium);
        const QFont noteFont = subFont();
        const QFontMetrics labelMetrics(labelFont);
        const QFontMetrics noteMetrics(noteFont);

        const bool hasSub = !m_sub.isEmpty();
        const bool hasProvenance = !m_provenance.isEmpty();
        const int block = labelMetrics.height() + (hasSub ? noteMetrics.height() + 2 : 0)
                          + (hasProvenance ? noteMetrics.height() + 2 : 0);
        int top = (height() - block) / 2;

        painter.setFont(labelFont);
        painter.setPen(theme()->color(Role::OnSurface));
        painter.drawText(QRect(left, top, available, labelMetrics.height()),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         labelMetrics.elidedText(m_label, Qt::ElideRight, available));

        if (hasSub) {
            top += labelMetrics.height() + 2;
            painter.setFont(noteFont);
            painter.setPen(theme()->color(Role::OnSurfaceVariant));
            painter.drawText(QRect(left, top, available, noteMetrics.height()),
                             Qt::AlignLeft | Qt::AlignVCenter,
                             noteMetrics.elidedText(m_sub, Qt::ElideRight, available));
        }
        if (hasProvenance) {
            top += (hasSub ? noteMetrics.height() + 2 : labelMetrics.height() + 2);
            painter.setFont(noteFont);
            painter.setPen(theme()->color(Role::Primary));
            painter.drawText(QRect(left, top, available, noteMetrics.height()), Qt::AlignLeft | Qt::AlignVCenter,
                             noteMetrics.elidedText(m_provenance, Qt::ElideRight, available));
        }
    }

    void SpecSheetRow::mouseReleaseEvent(QMouseEvent* event)
    {
        if (event->button() == Qt::LeftButton && rect().contains(event->position().toPoint())) {
            emit activated(m_key);
        }
        QWidget::mouseReleaseEvent(event);
    }

    void SpecSheetRow::keyPressEvent(QKeyEvent* event)
    {
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter || event->key() == Qt::Key_Space) {
            emit activated(m_key);
            event->accept();
            return;
        }
        QWidget::keyPressEvent(event);
    }

    void SpecSheetRow::enterEvent(QEnterEvent* event)
    {
        m_hovered = true;
        update();
        QWidget::enterEvent(event);
    }

    void SpecSheetRow::leaveEvent(QEvent* event)
    {
        m_hovered = false;
        update();
        QWidget::leaveEvent(event);
    }

    // ------------------------------------------------------------- SpecSheetPage

    SpecSheetPage::SpecSheetPage(const QString& id, const QString& title, QWidget* parent)
        : QScrollArea(parent)
        , m_id(id)
        , m_title(title)
    {
        setFrameShape(QFrame::NoFrame);
        setWidgetResizable(true);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        viewport()->setAutoFillBackground(false);

        m_content = new QWidget(this);
        m_content->setAutoFillBackground(false);

        auto* root = new QVBoxLayout(m_content);
        root->setContentsMargins(26, 20, 26, 48);
        root->setSpacing(0);

        auto* titleLabel = new QLabel(m_title, m_content);

        m_search = new SearchBar(SearchBar::Variant::Surface, m_content);
        m_search->setFixedWidth(PageSearchWidth);
        m_search->setPlaceholder(tr("Search this surface"));
        m_search->setIdentity(QStringLiteral("sheet.%1").arg(m_id), tr("%1 search").arg(m_title));

        auto* header = new QHBoxLayout;
        header->setContentsMargins(0, 0, 0, 0);
        header->setSpacing(12);
        header->addWidget(titleLabel);
        header->addStretch(1);
        header->addWidget(m_search);

        m_noteLabel = new QLabel(m_content);
        m_noteLabel->setWordWrap(true);
        m_noteLabel->setMaximumWidth(NoteMaxWidth);
        m_note = tr("Search every option label, description and current value on this surface.");
        m_noteLabel->setText(m_note);

        QLabel* noteLabel = m_noteLabel;
        auto restyle = [titleLabel, noteLabel] {
            QFont title = theme()->font(TypeRole::HeadlineSmall);
            // The page title sits between the 28px screen headline and a card title.
            title.setPointSizeF(title.pointSizeF() * 26.0 / 28.0);
            titleLabel->setFont(title);

            noteLabel->setFont(theme()->font(TypeRole::BodySmall));
            QPalette palette = noteLabel->palette();
            palette.setColor(QPalette::WindowText, theme()->color(Role::OnSurfaceVariant));
            noteLabel->setPalette(palette);
        };
        restyle();
        connect(theme(), &Theme::changed, noteLabel, restyle);

        m_contentLayout = new QVBoxLayout;
        m_contentLayout->setContentsMargins(0, 0, 0, 0);
        m_contentLayout->setSpacing(16);

        root->addLayout(header);
        root->addSpacing(6);
        root->addWidget(m_noteLabel);
        root->addSpacing(18);
        root->addLayout(m_contentLayout);
        root->addStretch(1);

        m_content->setMaximumWidth(PageMaxWidth + 52);
        setWidget(m_content);

        connect(m_search, &SearchBar::textChanged, this, [this](const QString& text) {
            applyFilter(text);
            emit searchTextChanged(text);
        });
        connect(m_search, &SearchBar::regexToggled, this, [this] { applyFilter(m_search->text()); });
        connect(m_search, &SearchBar::builderRequested, this, &SpecSheetPage::builderRequested);
    }

    void SpecSheetPage::applyFilter(const QString& text)
    {
        const QString raw = text.trimmed();
        const QString needle = raw.toLower();
        const bool regex = m_search->isRegexEnabled() && !raw.isEmpty();
        bool valid = true;
        QString error;
        if (regex) {
            const auto check = runBounded(raw, optionsForFlags(m_search->regexFlags()), QString());
            valid = check.compiled && !check.blocked && !check.timedOut;
            error = check.error;
        }
        int shown = 0;
        for (auto it = m_sections.constBegin(); it != m_sections.constEnd(); ++it) {
            Card* card = it.value();
            const auto rows = card->findChildren<SpecSheetRow*>();
            int visible = 0;
            for (auto* row : rows) {
                const QString haystack = row->property(HaystackProperty).toString();
                const bool match = needle.isEmpty() || (!regex && haystack.contains(needle))
                                   || (regex && valid && !runBounded(raw, optionsForFlags(m_search->regexFlags()), haystack).matches.isEmpty());
                row->setVisible(match);
                visible += match ? 1 : 0;
            }
            card->setVisible(visible > 0);
            shown += visible;
        }
        // The design keeps the section line as the section's own description and
        // reports what a search hid here, under the page title, instead.
        m_noteLabel->setText(needle.isEmpty()
                                 ? m_note
                                 : tr("%1 of %2 options match “%3” on this page. "
                                      "Other pages are searched from the same field.")
                                       .arg(shown)
                                       .arg(m_rowOrder.size())
                                       .arg(text.trimmed()));
        m_search->lineEdit()->setAccessibleDescription(valid ? tr("%1 settings match on this tab").arg(shown)
                                                            : tr("Invalid regular expression: %1").arg(error));
    }

    SpecSheetPage::~SpecSheetPage() = default;

    QString SpecSheetPage::id() const
    {
        return m_id;
    }

    QString SpecSheetPage::title() const
    {
        return m_title;
    }

    Card* SpecSheetPage::ensureSection(const QString& title)
    {
        const auto it = m_sections.constFind(title);
        if (it != m_sections.constEnd()) {
            return it.value();
        }

        auto* card = new SectionCard(m_content);
        card->setTitleText(title);
        // The design's note is the section's own description and nothing else;
        // a section it gives no description to shows no note line.
        card->setNoteText(m_sectionNotes.value(title));
        card->contentLayout()->setSpacing(0);
        m_contentLayout->addWidget(card);
        m_sections.insert(title, card);
        return card;
    }

    void SpecSheetPage::addRow(const QString& section,
                               const QString& symbol,
                               const QString& label,
                               const QString& sub,
                               PillKind kind,
                               const QString& controlText)
    {
        Card* card = ensureSection(section);
        const QString key = section + QLatin1Char('/') + label;

        auto* row = new SpecSheetRow(key, symbol, label, sub, kind, controlText, card);
        row->setProperty(HaystackProperty,
                         QStringLiteral("%1 %2 %3 %4").arg(section, label, sub, controlText).toLower());
        card->contentLayout()->addWidget(row);

        m_rows.insert(key, row);
        m_rowOrder.append(row);
        connect(row, &SpecSheetRow::activated, this, &SpecSheetPage::rowActivated);
    }

    void SpecSheetPage::setNote(const QString& note)
    {
        m_note = note.isEmpty()
                     ? tr("Search every option label, description and current value on this surface.")
                     : note;
        // Only refresh when nothing is being searched; a live filter owns the line.
        if (m_search->text().trimmed().isEmpty()) {
            m_noteLabel->setText(m_note);
        }
    }

    void SpecSheetPage::setSectionNote(const QString& section, const QString& note)
    {
        m_sectionNotes.insert(section, note);
        // A section built before its note arrived is corrected in place.
        if (auto* card = m_sections.value(section)) {
            card->setNoteText(note);
        }
    }

    SpecSheetRow* SpecSheetPage::row(const QString& key) const
    {
        return m_rows.value(key);
    }

    QList<SpecSheetRow*> SpecSheetPage::rows() const
    {
        return m_rowOrder;
    }

    Card* SpecSheetPage::sectionCard(const QString& title) const
    {
        return m_sections.value(title);
    }

    SearchBar* SpecSheetPage::searchBar() const
    {
        return m_search;
    }

    QString SpecSheetPage::searchText() const
    {
        return m_search->text();
    }

    void SpecSheetPage::setSearchText(const QString& text)
    {
        if (m_search->text() == text) {
            return;
        }
        m_search->setText(text);
    }

    // ----------------------------------------------------------------- SpecSheet

    SpecSheet::SpecSheet(QWidget* parent)
        : QWidget(parent)
    {
        auto* root = new QHBoxLayout(this);
        m_rootLayout = root;
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(0);

        m_pagePicker = new QComboBox(this);
        m_pagePicker->setObjectName(QStringLiteral("settingsPagePicker"));
        m_pagePicker->setAccessibleName(tr("Settings tab"));
        m_pagePicker->hide();
        connect(m_pagePicker, &QComboBox::currentIndexChanged, this, [this](int index) {
            if (index >= 0) setCurrentPage(m_pagePicker->itemData(index).toString());
        });
        root->addWidget(m_pagePicker);

        m_sidebar = new QWidget(this);
        m_sidebar->setObjectName(QStringLiteral("materialSpecSheetSidebar"));
        m_sidebar->setAttribute(Qt::WA_StyledBackground, true);
        m_sidebar->setFixedWidth(Layout::SheetNavWidth);

        m_sidebarLayout = new QVBoxLayout(m_sidebar);
        m_sidebarLayout->setContentsMargins(10, 14, 10, 14);
        m_sidebarLayout->setSpacing(2);

        // The first page inserts a default overline; a host that wants its own
        // grouping calls addSidebarSection() before adding any page.
        m_sidebarLayout->addStretch(1);

        m_stack = new QStackedWidget(this);

        root->addWidget(m_sidebar);
        root->addWidget(m_stack, 1);

        connect(theme(), &Theme::changed, this, &SpecSheet::applyTheme);
        applyTheme();
    }

    SpecSheet::~SpecSheet() = default;

    void SpecSheet::addSidebarSection(const QString& title)
    {
        auto* overline = new QLabel(title, m_sidebar);
        overline->setObjectName(QStringLiteral("materialSpecSheetOverline"));
        overline->setContentsMargins(10, m_sidebarSections.isEmpty() ? 4 : 14, 10, 8);
        // Keep the trailing stretch last so the rows stay at the top.
        m_sidebarLayout->insertWidget(m_sidebarLayout->count() - 1, overline);
        m_sidebarSections.append(overline);
        applyTheme();
    }

    void SpecSheet::registerPage(const QString& id, const QString& symbol, const QString& title, QWidget* widget)
    {
        if (m_sidebarSections.isEmpty()) {
            addSidebarSection(tr("SPEC SHEETS"));
        }
        m_pageWidgets.insert(id, widget);
        m_pageOrder.append(id);
        m_stack->addWidget(widget);

        auto* button = new ButtonBase(symbol, title, m_sidebar);
        button->setRadius(Shape::Full);
        button->setSymbolSize(20);
        button->setMinimumHeight(44);
        button->setCursor(Qt::PointingHandCursor);
        button->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        connect(button, &QPushButton::clicked, this, [this, id] { setCurrentPage(id); });

        m_sidebarLayout->insertWidget(m_sidebarLayout->count() - 1, button);
        m_pageButtons.insert(id, button);
        m_pagePicker->addItem(title, id);

        if (m_currentPage.isEmpty()) {
            m_currentPage = id;
            m_stack->setCurrentWidget(widget);
            applyTheme();
            emit currentPageChanged(id);
        } else {
            applyTheme();
        }
    }

    SpecSheetPage* SpecSheet::addPage(const QString& id, const QString& symbol, const QString& title)
    {
        if (m_pageWidgets.contains(id)) {
            return m_pages.value(id);
        }

        auto* page = new SpecSheetPage(id, title, m_stack);
        m_pages.insert(id, page);
        connect(page, &SpecSheetPage::rowActivated, this, [this, id](const QString& rowKey) {
            emit rowActivated(id, rowKey);
        });
        // Every tab owns its query, flags, validation, and mode independently.
        connect(page, &SpecSheetPage::builderRequested, this, [this, id] { emit builderRequested(id); });

        registerPage(id, symbol, title, page);
        return page;
    }

    void SpecSheet::addWidgetPage(const QString& id, const QString& symbol, const QString& title, QWidget* page)
    {
        if (!page || m_pageWidgets.contains(id)) {
            return;
        }
        // A page moved out of another layout leaves a stale item behind unless
        // it is taken out explicitly; reparenting alone is not enough.
        if (QWidget* previousParent = page->parentWidget()) {
            if (QLayout* previousLayout = previousParent->layout()) {
                previousLayout->removeWidget(page);
            }
        }
        registerPage(id, symbol, title, page);
    }

    SpecSheetPage* SpecSheet::page(const QString& id) const
    {
        return m_pages.value(id);
    }

    QWidget* SpecSheet::pageWidget(const QString& id) const
    {
        return m_pageWidgets.value(id);
    }

    void SpecSheet::syncSearch(const QString& sourceId, const QString& text)
    {
        if (m_mirroring) {
            return;
        }
        m_mirroring = true;
        for (const auto& id : m_pageOrder) {
            auto* other = m_pages.value(id);
            if (other && id != sourceId) {
                other->setSearchText(text);
            }
        }
        m_mirroring = false;
    }

    void SpecSheet::addRow(const QString& pageId,
                           const QString& section,
                           const QString& symbol,
                           const QString& label,
                           const QString& sub,
                           PillKind kind,
                           const QString& controlText)
    {
        if (auto* target = page(pageId)) {
            target->addRow(section, symbol, label, sub, kind, controlText);
        }
    }

    int SpecSheet::pageCount() const
    {
        return m_pageOrder.size();
    }

    QString SpecSheet::currentPage() const
    {
        return m_currentPage;
    }

    void SpecSheet::setCurrentPage(const QString& id)
    {
        QWidget* target = m_pageWidgets.value(id);
        if (!target || id == m_currentPage) {
            return;
        }
        m_currentPage = id;
        m_stack->setCurrentWidget(target);
        applyTheme();
        const int pickerIndex = m_pagePicker->findData(id);
        if (pickerIndex >= 0 && pickerIndex != m_pagePicker->currentIndex()) m_pagePicker->setCurrentIndex(pickerIndex);
        emit currentPageChanged(id);
    }

    void SpecSheet::resizeEvent(QResizeEvent* event)
    {
        QWidget::resizeEvent(event);
        const int width = event->size().width();
        const bool compact = width < 600;
        m_rootLayout->setDirection(compact ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
        m_pagePicker->setVisible(compact);
        m_sidebar->setVisible(!compact);
        if (!compact) {
            m_sidebar->setFixedWidth(width < 840 ? 160 : width < 1200 ? 216 : Layout::SheetNavWidth);
        }
    }

    void SpecSheet::applyTheme()
    {
        m_sidebar->setStyleSheet(QStringLiteral("QWidget#materialSpecSheetSidebar {"
                                                "background-color: %1;"
                                                "border-right: 1px solid %2;"
                                                "}"
                                                "QLabel#materialSpecSheetOverline { color: %3; }")
                                     .arg(theme()->hex(Role::SurfaceContainerLow),
                                          theme()->hex(Role::OutlineVariant),
                                          theme()->hex(Role::OnSurfaceVariant)));

        QFont overlineFont = theme()->font(TypeRole::LabelSmall);
        overlineFont.setLetterSpacing(QFont::AbsoluteSpacing, 0.8);
        for (auto* overline : m_sidebarSections) {
            overline->setFont(overlineFont);
        }

        for (const auto& id : m_pageOrder) {
            auto* button = m_pageButtons.value(id);
            if (!button) {
                continue;
            }
            const bool active = id == m_currentPage;
            // Inactive rows take the sidebar fill so only the active pill reads.
            button->setRoles(active ? Role::PrimaryContainer : Role::SurfaceContainerLow,
                             active ? Role::OnPrimaryContainer : Role::OnSurface);
            button->update();
        }
    }

} // namespace Material
