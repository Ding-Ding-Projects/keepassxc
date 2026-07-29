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

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace Material
{
    namespace
    {
        constexpr int GlyphColumn = 24;
        constexpr int RowGap = 14;
        constexpr int PageSearchWidth = 340;
        constexpr int PageMaxWidth = 1120;
        constexpr int NoteMaxWidth = 820;

        /** Dynamic property carrying the lower-cased text a row is searched by. */
        const char* const HaystackProperty = "materialHaystack";
        /** Dynamic property carrying how many rows a section card holds. */
        const char* const RowCountProperty = "materialRowCount";

        /** The 12px line under a section title, which doubles as the match count. */
        QString sectionNote(int visible, int total)
        {
            if (visible >= total) {
                return SpecSheetPage::tr("%1 options").arg(total);
            }
            return SpecSheetPage::tr("%1 of %2 options match").arg(visible).arg(total);
        }

        /** A 12px regular line; the type scale only offers 12px in medium weight. */
        QFont subFont()
        {
            QFont font = theme()->font(TypeRole::LabelMedium);
            font.setWeight(QFont::Normal);
            return font;
        }
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
        const int block = labelMetrics.height() + (hasSub ? noteMetrics.height() + 2 : 0);
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
    }

    void SpecSheetRow::mouseReleaseEvent(QMouseEvent* event)
    {
        if (event->button() == Qt::LeftButton && rect().contains(event->position().toPoint())) {
            emit activated(m_key);
        }
        QWidget::mouseReleaseEvent(event);
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

        auto* search = new SearchBar(SearchBar::Variant::Surface, m_content);
        search->setFixedWidth(PageSearchWidth);
        search->setPlaceholder(tr("Search this surface"));

        auto* header = new QHBoxLayout;
        header->setContentsMargins(0, 0, 0, 0);
        header->setSpacing(12);
        header->addWidget(titleLabel);
        header->addStretch(1);
        header->addWidget(search);

        auto* noteLabel = new QLabel(m_content);
        noteLabel->setWordWrap(true);
        noteLabel->setMaximumWidth(NoteMaxWidth);
        noteLabel->setText(tr("Search every option label, description and current value on this surface."));

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
        root->addWidget(noteLabel);
        root->addSpacing(18);
        root->addLayout(m_contentLayout);
        root->addStretch(1);

        m_content->setMaximumWidth(PageMaxWidth + 52);
        setWidget(m_content);

        connect(search, &SearchBar::textChanged, this, [this, noteLabel](const QString& text) {
            const QString needle = text.trimmed().toLower();
            for (auto it = m_sections.constBegin(); it != m_sections.constEnd(); ++it) {
                Card* card = it.value();
                const auto rows = card->findChildren<SpecSheetRow*>();
                int visible = 0;
                for (auto* row : rows) {
                    const bool match = needle.isEmpty() || row->property(HaystackProperty).toString().contains(needle);
                    row->setVisible(match);
                    visible += match ? 1 : 0;
                }
                card->setNoteText(sectionNote(visible, card->property(RowCountProperty).toInt()));
                card->setVisible(visible > 0);
            }
            noteLabel->setText(needle.isEmpty()
                                   ? tr("Search every option label, description and current value on this surface.")
                                   : tr("Matching “%1” across every section on this page.").arg(text.trimmed()));
        });
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

        auto* card = new Card(Card::Variant::Outlined, Material::Shape::ExtraLarge, m_content);
        card->setTitleText(title);
        card->setNoteText(sectionNote(0, 0));
        card->setProperty(RowCountProperty, 0);
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

        const int total = card->property(RowCountProperty).toInt() + 1;
        card->setProperty(RowCountProperty, total);
        card->setNoteText(sectionNote(total, total));

        m_rows.insert(key, row);
        connect(row, &SpecSheetRow::activated, this, &SpecSheetPage::rowActivated);
    }

    SpecSheetRow* SpecSheetPage::row(const QString& key) const
    {
        return m_rows.value(key);
    }

    Card* SpecSheetPage::sectionCard(const QString& title) const
    {
        return m_sections.value(title);
    }

    // ----------------------------------------------------------------- SpecSheet

    SpecSheet::SpecSheet(QWidget* parent)
        : QWidget(parent)
    {
        auto* root = new QHBoxLayout(this);
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(0);

        m_sidebar = new QWidget(this);
        m_sidebar->setObjectName(QStringLiteral("materialSpecSheetSidebar"));
        m_sidebar->setAttribute(Qt::WA_StyledBackground, true);
        m_sidebar->setFixedWidth(Layout::SheetNavWidth);

        m_sidebarLayout = new QVBoxLayout(m_sidebar);
        m_sidebarLayout->setContentsMargins(10, 14, 10, 14);
        m_sidebarLayout->setSpacing(2);

        auto* overline = new QLabel(tr("SPEC SHEETS"), m_sidebar);
        overline->setObjectName(QStringLiteral("materialSpecSheetOverline"));
        overline->setContentsMargins(10, 4, 10, 8);
        m_sidebarLayout->addWidget(overline);
        m_sidebarLayout->addStretch(1);

        m_stack = new QStackedWidget(this);

        root->addWidget(m_sidebar);
        root->addWidget(m_stack, 1);

        connect(theme(), &Theme::changed, this, &SpecSheet::applyTheme);
        applyTheme();
    }

    SpecSheet::~SpecSheet() = default;

    SpecSheetPage* SpecSheet::addPage(const QString& id, const QString& symbol, const QString& title)
    {
        if (m_pages.contains(id)) {
            return m_pages.value(id);
        }

        auto* page = new SpecSheetPage(id, title, m_stack);
        m_stack->addWidget(page);
        m_pages.insert(id, page);
        m_pageOrder.append(id);
        connect(page, &SpecSheetPage::rowActivated, this, [this, id](const QString& rowKey) {
            emit rowActivated(id, rowKey);
        });

        auto* button = new ButtonBase(symbol, title, m_sidebar);
        button->setRadius(Shape::Full);
        button->setSymbolSize(20);
        button->setMinimumHeight(44);
        button->setCursor(Qt::PointingHandCursor);
        button->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        connect(button, &QPushButton::clicked, this, [this, id] { setCurrentPage(id); });

        // Keep the trailing stretch last so the rows stay at the top.
        m_sidebarLayout->insertWidget(m_sidebarLayout->count() - 1, button);
        m_pageButtons.insert(id, button);

        if (m_currentPage.isEmpty()) {
            setCurrentPage(id);
        } else {
            applyTheme();
        }
        return page;
    }

    SpecSheetPage* SpecSheet::page(const QString& id) const
    {
        return m_pages.value(id);
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

    QString SpecSheet::currentPage() const
    {
        return m_currentPage;
    }

    void SpecSheet::setCurrentPage(const QString& id)
    {
        auto* target = page(id);
        if (!target || id == m_currentPage) {
            return;
        }
        m_currentPage = id;
        m_stack->setCurrentWidget(target);
        applyTheme();
        emit currentPageChanged(id);
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

        if (auto* overline = m_sidebar->findChild<QLabel*>(QStringLiteral("materialSpecSheetOverline"))) {
            QFont font = theme()->font(TypeRole::LabelSmall);
            font.setLetterSpacing(QFont::AbsoluteSpacing, 0.8);
            overline->setFont(font);
        }

        for (const auto& id : m_pageOrder) {
            auto* button = m_pageButtons.value(id);
            if (!button) {
                continue;
            }
            const bool active = id == m_currentPage;
            // Inactive rows take the sidebar fill so only the active pill reads.
            button->setRoles(active ? Role::SecondaryContainer : Role::SurfaceContainerLow,
                             active ? Role::OnSecondaryContainer : Role::OnSurfaceVariant);
            button->update();
        }
    }

} // namespace Material
