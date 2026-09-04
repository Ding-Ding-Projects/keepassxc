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

#include "MaterialVaultSidebar.h"

#include "MaterialChip.h"
#include "MaterialElevation.h"
#include "MaterialGroupDelegate.h"
#include "MaterialIcons.h"
#include "MaterialSearchBar.h"
#include "MaterialTheme.h"

#include <QAbstractButton>
#include <QAbstractItemModel>
#include <QFontMetrics>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QRegularExpression>
#include <QTreeView>
#include <QVBoxLayout>

#include <functional>

namespace Material
{
    namespace
    {
        constexpr int HorizontalPadding = 10;
        constexpr int VerticalPadding = 14;
        constexpr int RowSpacing = 2;
        constexpr int OverlineIndent = 10;
        constexpr int GroupsOverlineTop = 4;
        constexpr int TagsOverlineTop = 18;
        constexpr int OverlineBottom = 8;
        // The design starts the root at 12px and its children at 30px.
        constexpr int IndentStep = 18;
        constexpr int TagSpacing = 8;
        constexpr int TagIndent = 4;
        constexpr int BottomSpacer = 24;
        constexpr qreal OverlineLetterSpacing = 0.8;
        /** How many group rows the tree insists on before it starts scrolling. */
        constexpr int MinimumVisibleGroupRows = 12;

        constexpr int EditorRowHeight = 44;
        constexpr int EditorRowPadding = 12;
        constexpr int EditorGlyphSize = 20;
        constexpr int EditorGlyphGap = 10;
        constexpr int EditorTextPadding = 10;
        constexpr int EditorMinimumTextWidth = 40;

        /**
         * The bottom row of the pane. A list row rather than a button: no
         * container until it is hovered, and a label that wraps onto a second
         * line instead of eliding, because the pane is only 250px wide.
         */
        class ExternalEditorRow : public QAbstractButton
        {
        public:
            explicit ExternalEditorRow(QWidget* parent = nullptr)
                : QAbstractButton(parent)
            {
                setCursor(Qt::PointingHandCursor);
                QSizePolicy policy(QSizePolicy::Preferred, QSizePolicy::Minimum);
                policy.setHeightForWidth(true);
                setSizePolicy(policy);
            }

            QSize sizeHint() const override
            {
                const QFontMetrics metrics(labelFont());
                return {contentLeft() + metrics.horizontalAdvance(text()) + EditorRowPadding, EditorRowHeight};
            }

            QSize minimumSizeHint() const override
            {
                return {contentLeft() + EditorMinimumTextWidth + EditorRowPadding, EditorRowHeight};
            }

            int heightForWidth(int width) const override
            {
                const QFontMetrics metrics(labelFont());
                const int textWidth = qMax(1, width - contentLeft() - EditorRowPadding);
                const QRect bounds =
                    metrics.boundingRect(QRect(0, 0, textWidth, 0), Qt::TextWordWrap | Qt::AlignLeft, text());
                return qMax(EditorRowHeight, bounds.height() + EditorTextPadding * 2);
            }

        protected:
            void paintEvent(QPaintEvent* event) override
            {
                Q_UNUSED(event);
                QPainter painter(this);
                painter.setRenderHint(QPainter::Antialiasing, true);
                painter.setRenderHint(QPainter::TextAntialiasing, true);

                QColor fill;
                if (isDown()) {
                    fill = theme()->color(Role::SurfaceContainerHighest);
                } else if (underMouse()) {
                    fill = theme()->color(Role::SurfaceContainerHigh);
                }
                paintSurface(&painter, rect(), Shape::Large, fill);

                const QColor content = theme()->color(Role::OnSurfaceVariant);
                const QRect glyphRect(
                    EditorRowPadding, (height() - EditorGlyphSize) / 2, EditorGlyphSize, EditorGlyphSize);
                const QPixmap glyph = Icons::pixmap(QStringLiteral("edit_document"), EditorGlyphSize, content);
                if (!glyph.isNull()) {
                    const QSizeF size = glyph.deviceIndependentSize();
                    painter.drawPixmap(QPointF(glyphRect.center().x() + 0.5 - size.width() / 2.0,
                                               glyphRect.center().y() + 0.5 - size.height() / 2.0),
                                       glyph);
                }

                const QRect textRect(contentLeft(), 0, qMax(0, width() - contentLeft() - EditorRowPadding), height());
                painter.setFont(labelFont());
                painter.setPen(content);
                painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap, text());
            }

            void enterEvent(QEnterEvent* event) override
            {
                QAbstractButton::enterEvent(event);
                update();
            }

            void leaveEvent(QEvent* event) override
            {
                QAbstractButton::leaveEvent(event);
                update();
            }

        private:
            QFont labelFont() const
            {
                return theme()->font(TypeRole::BodySmall);
            }

            static int contentLeft()
            {
                return EditorRowPadding + EditorGlyphSize + EditorGlyphGap;
            }
        };

        QLabel* createOverline(const QString& text, int topMargin, QWidget* parent)
        {
            auto* label = new QLabel(text, parent);
            label->setContentsMargins(OverlineIndent, topMargin, OverlineIndent, OverlineBottom);
            return label;
        }
    } // namespace

    // ----------------------------------------------------------------- FlowLayout

    FlowLayout::FlowLayout(QWidget* parent, int horizontalSpacing, int verticalSpacing)
        : QLayout(parent)
        , m_horizontalSpacing(horizontalSpacing)
        , m_verticalSpacing(verticalSpacing)
    {
    }

    FlowLayout::~FlowLayout()
    {
        while (QLayoutItem* item = takeAt(0)) {
            delete item;
        }
    }

    void FlowLayout::addItem(QLayoutItem* item)
    {
        m_items.append(item);
    }

    int FlowLayout::count() const
    {
        return static_cast<int>(m_items.count());
    }

    QLayoutItem* FlowLayout::itemAt(int index) const
    {
        return m_items.value(index);
    }

    QLayoutItem* FlowLayout::takeAt(int index)
    {
        if (index < 0 || index >= m_items.count()) {
            return nullptr;
        }
        return m_items.takeAt(index);
    }

    Qt::Orientations FlowLayout::expandingDirections() const
    {
        return {};
    }

    bool FlowLayout::hasHeightForWidth() const
    {
        return true;
    }

    int FlowLayout::heightForWidth(int width) const
    {
        return layoutItems(QRect(0, 0, width, 0), false);
    }

    void FlowLayout::setGeometry(const QRect& rect)
    {
        QLayout::setGeometry(rect);
        layoutItems(rect, true);
    }

    QSize FlowLayout::sizeHint() const
    {
        return minimumSize();
    }

    QSize FlowLayout::minimumSize() const
    {
        QSize size;
        for (const QLayoutItem* item : m_items) {
            size = size.expandedTo(item->minimumSize());
        }
        const QMargins margins = contentsMargins();
        return size + QSize(margins.left() + margins.right(), margins.top() + margins.bottom());
    }

    int FlowLayout::layoutItems(const QRect& rect, bool apply) const
    {
        const QMargins margins = contentsMargins();
        const QRect content = rect.adjusted(margins.left(), margins.top(), -margins.right(), -margins.bottom());

        int x = content.left();
        int y = content.top();
        int lineHeight = 0;
        for (QLayoutItem* item : m_items) {
            const QSize hint = item->sizeHint();
            if (lineHeight > 0 && x + hint.width() > content.right() + 1) {
                x = content.left();
                y += lineHeight + m_verticalSpacing;
                lineHeight = 0;
            }
            if (apply) {
                item->setGeometry(QRect(QPoint(x, y), hint));
            }
            x += hint.width() + m_horizontalSpacing;
            lineHeight = qMax(lineHeight, hint.height());
        }
        return y + lineHeight + margins.bottom() - rect.top();
    }

    // --------------------------------------------------------------- VaultSidebar

    VaultSidebar::VaultSidebar(QWidget* parent)
        : QWidget(parent)
    {
        setObjectName(QStringLiteral("materialVaultSidebar"));
        setMinimumWidth(180);

        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(HorizontalPadding, VerticalPadding, HorizontalPadding, VerticalPadding);
        root->setSpacing(RowSpacing);

        m_groupsOverline = createOverline(tr("Groups"), GroupsOverlineTop, this);
        root->addWidget(m_groupsOverline);

        // The reference's "Filter groups" field sits above the tree with its
        // own regex affordance; a match keeps the group's ancestors visible.
        m_groupFilter = new SearchBar(SearchBar::Variant::Surface, this);
        m_groupFilter->setObjectName(QStringLiteral("materialVaultGroupFilter"));
        m_groupFilter->setPlaceholder(tr("Filter groups"));
        m_groupFilter->setIdentity(QStringLiteral("vault.groups"), tr("Vault group filter"));
        m_groupFilter->lineEdit()->setAccessibleName(tr("Filter groups"));
        connect(m_groupFilter, &SearchBar::textChanged, this, &VaultSidebar::filterGroups);
        connect(m_groupFilter, &SearchBar::regexToggled, this, [this] { filterGroups(m_groupFilter->text()); });
        root->addWidget(m_groupFilter);

        m_groupDelegate = new GroupDelegate(this);
        m_groupDelegate->setIndentStep(IndentStep);

        m_groupView = new QTreeView(this);
        m_groupView->setItemDelegate(m_groupDelegate);
        m_groupView->setHeaderHidden(true);
        m_groupView->setFrameShape(QFrame::NoFrame);
        m_groupView->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_groupView->setSelectionMode(QAbstractItemView::SingleSelection);
        m_groupView->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_groupView->setUniformRowHeights(true);
        m_groupView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_groupView->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        m_groupView->setAttribute(Qt::WA_MacShowFocusRect, false);
        // The delegate draws the indentation itself, so the view adds none.
        m_groupView->setIndentation(0);
        m_groupView->setRootIsDecorated(false);
        m_groupView->setExpandsOnDoubleClick(false);
        m_groupView->viewport()->setAutoFillBackground(false);
        // Hover has to reach the delegate for the state layer on the pills.
        m_groupView->viewport()->setAttribute(Qt::WA_Hover, true);
        m_groupView->viewport()->setMouseTracking(true);
        // The delegate owns the whole row, so the application sheet's item
        // padding and its square selection fill are switched off here.
        m_groupView->setStyleSheet(
            QStringLiteral("QTreeView { background: transparent; border: none; }"
                           "QTreeView::item { background: transparent; border: none; padding: 0; min-height: 0; }"
                           "QTreeView::item:hover, QTreeView::item:selected { background: transparent; }"));
        root->addWidget(m_groupView);

        m_tagsOverline = createOverline(tr("Tags"), TagsOverlineTop, this);
        root->addWidget(m_tagsOverline);

        m_tagContainer = new QWidget(this);
        m_tagLayout = new FlowLayout(m_tagContainer, TagSpacing, TagSpacing);
        m_tagLayout->setContentsMargins(TagIndent, 0, TagIndent, 0);
        QSizePolicy tagPolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
        tagPolicy.setHeightForWidth(true);
        m_tagContainer->setSizePolicy(tagPolicy);
        root->addWidget(m_tagContainer);

        root->addSpacing(BottomSpacer);
        root->addStretch(1);

        auto* editorRow = new ExternalEditorRow(this);
        editorRow->setText(tr("Open database folder in external editor"));
        m_editorRow = editorRow;
        root->addWidget(m_editorRow);
        connect(m_editorRow, &QAbstractButton::clicked, this, &VaultSidebar::externalEditorRequested);

        m_tagsOverline->setVisible(false);
        m_tagContainer->setVisible(false);

        connect(theme(), &Theme::changed, this, &VaultSidebar::applyTheme);
        applyTheme();
        updateGroupViewHeight();
    }

    VaultSidebar::~VaultSidebar() = default;

    void VaultSidebar::setGroupModel(QAbstractItemModel* model)
    {
        if (m_groupView->model() == model) {
            return;
        }
        if (auto* previous = m_groupView->model()) {
            previous->disconnect(this);
        }

        m_groupView->setModel(model);
        if (model) {
            connect(model, &QAbstractItemModel::modelReset, this, &VaultSidebar::updateGroupViewHeight);
            connect(model, &QAbstractItemModel::rowsInserted, this, &VaultSidebar::updateGroupViewHeight);
            connect(model, &QAbstractItemModel::rowsRemoved, this, &VaultSidebar::updateGroupViewHeight);
            connect(model, &QAbstractItemModel::layoutChanged, this, &VaultSidebar::updateGroupViewHeight);
        }
        if (auto* selection = m_groupView->selectionModel()) {
            connect(selection, &QItemSelectionModel::currentChanged, this, [this](const QModelIndex& current) {
                emit groupSelected(current);
            });
        }
        updateGroupViewHeight();
    }

    QAbstractItemModel* VaultSidebar::groupModel() const
    {
        return m_groupView->model();
    }

    SearchBar* VaultSidebar::groupFilter() const
    {
        return m_groupFilter;
    }

    void VaultSidebar::filterGroups(const QString& query)
    {
        auto* model = m_groupView->model();
        if (!model) {
            return;
        }
        const QString needle = query.trimmed();
        const bool regex = m_groupFilter->isRegexEnabled();
        QRegularExpression pattern;
        if (regex && !needle.isEmpty()) {
            pattern = QRegularExpression(needle, QRegularExpression::CaseInsensitiveOption);
            if (!pattern.isValid()) {
                return; // an unparsable pattern changes nothing until it parses
            }
        }
        std::function<bool(const QModelIndex&)> apply = [&](const QModelIndex& parent) -> bool {
            bool anyVisible = false;
            for (int row = 0; row < model->rowCount(parent); ++row) {
                const QModelIndex index = model->index(row, 0, parent);
                const QString name = index.data(Qt::DisplayRole).toString();
                const bool self = needle.isEmpty() || (regex ? pattern.match(name).hasMatch()
                                                             : name.contains(needle, Qt::CaseInsensitive));
                const bool descendant = apply(index);
                const bool visible = self || descendant;
                m_groupView->setRowHidden(row, parent, !visible);
                if (visible && descendant && !needle.isEmpty()) {
                    m_groupView->setExpanded(index, true);
                }
                anyVisible = anyVisible || visible;
            }
            return anyVisible;
        };
        apply(QModelIndex());
        updateGroupViewHeight();
    }

    QTreeView* VaultSidebar::groupView() const
    {
        return m_groupView;
    }

    QStringList VaultSidebar::tags() const
    {
        return m_tags;
    }

    void VaultSidebar::setTags(const QStringList& tags)
    {
        if (tags == m_tags) {
            return;
        }
        m_tags = tags;

        const QStringList previous = m_selectedTags;
        QStringList kept;
        for (const QString& tag : previous) {
            if (m_tags.contains(tag)) {
                kept.append(tag);
            }
        }
        m_selectedTags = kept;

        rebuildTagChips();
        m_tagsOverline->setVisible(!m_tags.isEmpty());
        m_tagContainer->setVisible(!m_tags.isEmpty());

        if (m_selectedTags != previous) {
            emit tagsChanged(m_selectedTags);
        }
    }

    QStringList VaultSidebar::selectedTags() const
    {
        return m_selectedTags;
    }

    void VaultSidebar::setSelectedTags(const QStringList& tags)
    {
        QStringList selected;
        for (const QString& tag : m_tags) {
            if (tags.contains(tag)) {
                selected.append(tag);
            }
        }
        if (selected == m_selectedTags) {
            return;
        }
        m_selectedTags = selected;

        m_updatingChips = true;
        for (Chip* chip : m_tagChips) {
            chip->setChecked(m_selectedTags.contains(chip->text()));
        }
        m_updatingChips = false;

        emit tagsChanged(m_selectedTags);
    }

    void VaultSidebar::paintEvent(QPaintEvent* event)
    {
        Q_UNUSED(event);
        QPainter painter(this);
        painter.fillRect(rect(), theme()->color(Role::SurfaceContainerLow));
        painter.fillRect(QRect(width() - 1, 0, 1, height()), theme()->color(Role::OutlineVariant));
    }

    void VaultSidebar::applyTheme()
    {
        QFont overline = theme()->font(TypeRole::LabelSmall);
        overline.setCapitalization(QFont::AllUppercase);
        overline.setLetterSpacing(QFont::AbsoluteSpacing, OverlineLetterSpacing);
        const QString overlineStyle =
            QStringLiteral("color: %1; background: transparent;").arg(theme()->hex(Role::OnSurfaceVariant));
        for (QLabel* label : {m_groupsOverline, m_tagsOverline}) {
            label->setFont(overline);
            label->setStyleSheet(overlineStyle);
        }

        m_groupView->setFont(theme()->font(TypeRole::BodyMedium));
        m_editorRow->updateGeometry();
        updateGroupViewHeight();
        update();
    }

    void VaultSidebar::rebuildTagChips()
    {
        // Deleting a chip takes it out of the flow layout with it.
        qDeleteAll(m_tagChips);
        m_tagChips.clear();

        for (const QString& tag : m_tags) {
            auto* chip = new Chip(QString(), tag, Chip::Kind::Filter, m_tagContainer);
            chip->setRadius(Shape::Small);
            chip->setChecked(m_selectedTags.contains(tag));
            m_tagLayout->addWidget(chip);
            m_tagChips.append(chip);

            connect(chip, &QAbstractButton::toggled, this, [this] {
                if (m_updatingChips) {
                    return;
                }
                const QStringList selected = checkedTags();
                if (selected == m_selectedTags) {
                    return;
                }
                m_selectedTags = selected;
                emit tagsChanged(m_selectedTags);
            });
        }
        m_tagContainer->updateGeometry();
    }

    QStringList VaultSidebar::checkedTags() const
    {
        QStringList selected;
        for (const Chip* chip : m_tagChips) {
            if (chip->isChecked()) {
                selected.append(chip->text());
            }
        }
        return selected;
    }

    void VaultSidebar::updateGroupViewHeight()
    {
        // Without an expand decoration there is nothing to expand with, so the
        // whole tree stays open and the pane sizes itself to the visible rows.
        m_groupView->expandAll();

        const int rowHeight = qMax(m_groupView->sizeHintForRow(0), GroupDelegate::RowHeight);
        const int rows = qMax(1, visibleRowCount(QModelIndex()));
        const int height = rows * rowHeight;
        // A scroll area's size hint is a fixed 192px whatever it holds, so the
        // layout would squeeze a short tree into a scroller with the pane half
        // empty below it. The minimum is what the rows actually need, capped so
        // a large group tree still leaves room for the tags and the bottom row.
        m_groupView->setMinimumHeight(qMin(height, MinimumVisibleGroupRows * rowHeight));
        m_groupView->setMaximumHeight(height);
        m_groupView->updateGeometry();
    }

    int VaultSidebar::visibleRowCount(const QModelIndex& parent) const
    {
        auto* model = m_groupView->model();
        if (!model) {
            return 0;
        }

        int count = 0;
        const int rows = model->rowCount(parent);
        for (int row = 0; row < rows; ++row) {
            const QModelIndex index = model->index(row, 0, parent);
            ++count;
            if (m_groupView->isExpanded(index)) {
                count += visibleRowCount(index);
            }
        }
        return count;
    }

} // namespace Material
