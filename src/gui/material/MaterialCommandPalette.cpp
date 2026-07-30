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

#include "MaterialCommandPalette.h"

#include "MaterialElevation.h"
#include "MaterialIcons.h"
#include "MaterialSearchBar.h"
#include "MaterialTheme.h"

#include <QAction>
#include <QEvent>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>
#include <utility>

namespace Material
{
    namespace
    {
        constexpr int SheetWidth = 720;
        constexpr int SheetPadding = 20;
        constexpr int HeaderGlyphSize = 26;
        constexpr int ListMinHeight = 220;
        constexpr int ListMaxHeight = 340;
        constexpr int RowHeight = 44;
        constexpr int RowSpacing = 2;
        constexpr int RowPadding = 12;
        constexpr int RowGlyphSize = 22;
        constexpr int ShortcutPadding = 10;
        constexpr int EmptyGlyphSize = 40;
        constexpr qreal DisabledOpacity = 0.38;

        /** A rounded panel filled with a colour role; the sheet and the list frame. */
        class Panel : public QWidget
        {
        public:
            Panel(int radius, Role fill, bool outlined, QWidget* parent = nullptr)
                : QWidget(parent)
                , m_radius(radius)
                , m_fill(fill)
                , m_outlined(outlined)
            {
            }

        protected:
            void paintEvent(QPaintEvent* event) override
            {
                Q_UNUSED(event)
                QPainter painter(this);
                painter.setRenderHint(QPainter::Antialiasing);
                paintSurface(&painter,
                             rect(),
                             m_radius,
                             theme()->color(m_fill),
                             m_outlined ? theme()->color(Role::OutlineVariant) : QColor());
            }

        private:
            int m_radius;
            Role m_fill;
            bool m_outlined;
        };

        /**
         * One command in the list: the action's icon, its text, the menu it
         * belongs to and its shortcut. Painted rather than built from labels so
         * that a filter pass rebuilds sixty rows without a layout storm.
         */
        class CommandRow : public QWidget
        {
        public:
            CommandRow(QAction* action,
                       const QString& path,
                       const QString& shortcut,
                       std::function<void()> activate,
                       QWidget* parent = nullptr)
                : QWidget(parent)
                , m_action(action)
                , m_path(path)
                , m_shortcut(shortcut)
                , m_activate(std::move(activate))
            {
                setFixedHeight(RowHeight);
                setMouseTracking(true);
                setCursor(Qt::PointingHandCursor);
                setFocusPolicy(Qt::NoFocus);
                setAccessibleName(action ? action->text().remove(QLatin1Char('&')) : QString());
            }

            void setSelected(bool selected)
            {
                if (m_selected == selected) {
                    return;
                }
                m_selected = selected;
                update();
            }

            bool isSelected() const
            {
                return m_selected;
            }

            QAction* action() const
            {
                return m_action;
            }

        protected:
            void paintEvent(QPaintEvent* event) override
            {
                Q_UNUSED(event)
                if (!m_action) {
                    return;
                }

                QPainter painter(this);
                painter.setRenderHint(QPainter::Antialiasing);

                QColor content = theme()->color(Role::OnSurface);
                QColor secondary = theme()->color(Role::OnSurfaceVariant);
                if (m_selected) {
                    paintSurface(&painter, rect(), Shape::Medium, theme()->color(Role::SecondaryContainer));
                    content = theme()->color(Role::OnSecondaryContainer);
                    secondary = theme()->color(Role::OnSecondaryContainer);
                } else if (m_hovered) {
                    paintStateLayer(&painter, rect(), Shape::Medium, theme()->color(Role::OnSurface), 0.08);
                }

                if (!m_action->isEnabled()) {
                    painter.setOpacity(DisabledOpacity);
                }

                const QRect glyph(RowPadding, (height() - RowGlyphSize) / 2, RowGlyphSize, RowGlyphSize);
                const QIcon icon = m_action->icon();
                if (!icon.isNull()) {
                    painter.drawPixmap(glyph, icon.pixmap(glyph.size(), devicePixelRatioF()));
                } else {
                    painter.drawPixmap(glyph, Icons::pixmap(QStringLiteral("chevron_right"), RowGlyphSize, secondary));
                }

                int right = width() - RowPadding;

                // The shortcut pill sits hard right, then the menu path, and the
                // command text takes whatever is left over and elides into it.
                if (!m_shortcut.isEmpty()) {
                    const QFont shortcutFont = theme()->font(TypeRole::LabelSmall);
                    const QFontMetrics metrics(shortcutFont);
                    const int pillWidth = metrics.horizontalAdvance(m_shortcut) + 2 * ShortcutPadding;
                    const QRect pill(right - pillWidth, (height() - 24) / 2, pillWidth, 24);
                    paintSurface(&painter,
                                 pill,
                                 Shape::Small,
                                 theme()->color(Role::SurfaceContainerHighest),
                                 theme()->color(Role::OutlineVariant));
                    painter.setFont(shortcutFont);
                    painter.setPen(theme()->color(Role::OnSurfaceVariant));
                    painter.drawText(pill, Qt::AlignCenter, m_shortcut);
                    right = pill.left() - 10;
                }

                if (!m_path.isEmpty()) {
                    const QFont pathFont = theme()->font(TypeRole::BodySmall);
                    const QFontMetrics metrics(pathFont);
                    // A couple of pixels of slack, so a path that exactly fits
                    // is not elided by a rounding difference.
                    const int pathWidth = qMin(metrics.horizontalAdvance(m_path) + 4, width() / 4);
                    const QRect pathRect(right - pathWidth, 0, pathWidth, height());
                    painter.setFont(pathFont);
                    painter.setPen(secondary);
                    painter.drawText(pathRect,
                                     Qt::AlignRight | Qt::AlignVCenter,
                                     metrics.elidedText(m_path, Qt::ElideRight, pathWidth));
                    right = pathRect.left() - 12;
                }

                const int textLeft = glyph.right() + 12;
                const QRect textRect(textLeft, 0, qMax(0, right - textLeft), height());
                const QFont textFont = theme()->font(TypeRole::BodyMedium);
                painter.setFont(textFont);
                painter.setPen(content);
                const QString text = m_action->text().remove(QLatin1Char('&'));
                painter.drawText(textRect,
                                 Qt::AlignLeft | Qt::AlignVCenter,
                                 QFontMetrics(textFont).elidedText(text, Qt::ElideRight, textRect.width()));
            }

            void enterEvent(QEnterEvent* event) override
            {
                Q_UNUSED(event)
                m_hovered = true;
                update();
            }

            void leaveEvent(QEvent* event) override
            {
                Q_UNUSED(event)
                m_hovered = false;
                update();
            }

            void mouseReleaseEvent(QMouseEvent* event) override
            {
                if (event->button() == Qt::LeftButton && rect().contains(event->position().toPoint()) && m_activate) {
                    m_activate();
                }
                QWidget::mouseReleaseEvent(event);
            }

        private:
            QAction* m_action = nullptr;
            QString m_path;
            QString m_shortcut;
            std::function<void()> m_activate;
            bool m_selected = false;
            bool m_hovered = false;
        };

        void styleLabel(QLabel* label, TypeRole type, Role color)
        {
            label->setFont(theme()->font(type));
            label->setStyleSheet(QStringLiteral("color:%1;background:transparent;").arg(theme()->hex(color)));
        }

    } // namespace

    /**
     * The menu an action lives in, e.g. "Database" or "Database ▸ Export".
     * Empty when the action is not on a menu at all.
     */
    QString menuPathOf(const QAction* action)
    {
        QStringList parts;
        const QAction* current = action;
        // Three hops covers KeePassXC: an action, its menu, and the menu
        // that one hangs off - "Database ▸ Export ▸ CSV File…".
        for (int depth = 0; depth < 3 && current; ++depth) {
            QMenu* owner = nullptr;
            const auto associated = current->associatedObjects();
            for (QObject* object : associated) {
                if (auto* menu = qobject_cast<QMenu*>(object)) {
                    owner = menu;
                    break;
                }
            }
            if (!owner || owner->title().isEmpty()) {
                break;
            }
            parts.prepend(QString(owner->title()).remove(QLatin1Char('&')));
            current = owner->menuAction();
        }
        return parts.join(QStringLiteral(" ▸ "));
    }


    CommandPalette::CommandPalette(QWidget* parent)
        : Overlay(parent)
    {
        buildSheet();
        setSheetWidth(SheetWidth);
        setSheetWidget(m_sheet);

        m_source = parent ? parent->window() : nullptr;

        connect(theme(), &Theme::changed, this, &CommandPalette::applyTheme);
        connect(this, &Overlay::opened, this, [this] {
            m_search->lineEdit()->setFocus(Qt::PopupFocusReason);
            m_search->lineEdit()->selectAll();
        });
        applyTheme();
    }

    CommandPalette::~CommandPalette() = default;

    QWidget* CommandPalette::actionSource() const
    {
        return m_source;
    }

    void CommandPalette::setActionSource(QWidget* source)
    {
        m_source = source;
    }

    int CommandPalette::commandCount() const
    {
        return m_commands.size();
    }

    void CommandPalette::buildSheet()
    {
        m_sheet = new Panel(Shape::ExtraLarge, Role::SurfaceContainerHigh, true);
        m_sheet->setObjectName(QStringLiteral("commandPaletteSheet"));

        auto* layout = new QVBoxLayout(m_sheet);
        layout->setContentsMargins(SheetPadding, SheetPadding, SheetPadding, SheetPadding);
        layout->setSpacing(14);

        auto* header = new QHBoxLayout;
        header->setContentsMargins(0, 0, 0, 0);
        header->setSpacing(12);

        auto* glyph = new QLabel(m_sheet);
        glyph->setObjectName(QStringLiteral("commandPaletteGlyph"));
        glyph->setFixedSize(HeaderGlyphSize, HeaderGlyphSize);
        header->addWidget(glyph);

        m_headline = new QLabel(tr("Commands"), m_sheet);
        header->addWidget(m_headline);
        header->addStretch();

        m_countLabel = new QLabel(m_sheet);
        header->addWidget(m_countLabel);
        layout->addLayout(header);

        m_search = new SearchBar(SearchBar::Variant::Surface, m_sheet);
        m_search->setShowRegexControls(false);
        m_search->setPlaceholder(tr("Search every command…"));
        connect(m_search, &SearchBar::textChanged, this, &CommandPalette::applyFilter);
        m_search->lineEdit()->installEventFilter(this);
        layout->addWidget(m_search);

        m_scroll = new QScrollArea(m_sheet);
        m_scroll->setWidgetResizable(true);
        m_scroll->setFrameShape(QFrame::NoFrame);
        // The list is sized to its contents in applyFilter(); a scroll area
        // would otherwise collapse to a couple of rows inside the sheet.
        m_scroll->setFixedHeight(ListMinHeight);
        m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_scroll->viewport()->setAutoFillBackground(false);
        m_scroll->setStyleSheet(QStringLiteral("QScrollArea{background:transparent;}"));

        auto* list = new QWidget(m_scroll);
        list->setAutoFillBackground(false);
        m_listLayout = new QVBoxLayout(list);
        m_listLayout->setContentsMargins(0, 0, 0, 0);
        m_listLayout->setSpacing(RowSpacing);
        m_listLayout->addStretch();
        m_scroll->setWidget(list);
        layout->addWidget(m_scroll, 1);

        m_emptyState = new QWidget(m_sheet);
        auto* emptyLayout = new QVBoxLayout(m_emptyState);
        emptyLayout->setContentsMargins(0, 24, 0, 24);
        emptyLayout->setSpacing(10);
        auto* emptyGlyph = new QLabel(m_emptyState);
        emptyGlyph->setObjectName(QStringLiteral("commandPaletteEmptyGlyph"));
        emptyGlyph->setAlignment(Qt::AlignCenter);
        emptyLayout->addWidget(emptyGlyph);
        auto* emptyText = new QLabel(tr("No command matches that search."), m_emptyState);
        emptyText->setObjectName(QStringLiteral("commandPaletteEmptyText"));
        emptyText->setAlignment(Qt::AlignCenter);
        emptyLayout->addWidget(emptyText);
        m_emptyState->hide();
        layout->addWidget(m_emptyState);

        auto* hint = new QLabel(tr("↑↓ to choose · Enter to run · Esc to close"), m_sheet);
        hint->setObjectName(QStringLiteral("commandPaletteHint"));
        layout->addWidget(hint);
    }

    void CommandPalette::aboutToOpen()
    {
        collect();
        m_search->lineEdit()->blockSignals(true);
        m_search->clear();
        m_search->lineEdit()->blockSignals(false);
        applyFilter(QString());
    }

    void CommandPalette::collect()
    {
        m_commands.clear();

        QWidget* source = m_source ? m_source.data() : (parentWidget() ? parentWidget()->window() : nullptr);
        if (!source) {
            return;
        }

        const auto actions = source->findChildren<QAction*>();
        for (QAction* action : actions) {
            if (action->isSeparator() || action->text().isEmpty()) {
                continue;
            }
            // A submenu's own action does nothing when triggered; its children
            // are listed instead, carrying its title as their path.
            if (action->menu() && action->menu()->menuAction() == action) {
                continue;
            }

            Command command;
            command.action = action;
            command.text = action->text().remove(QLatin1Char('&'));
            command.path = menuPathOf(action);
            command.shortcut = action->shortcut().toString(QKeySequence::NativeText);
            command.haystack = (command.text + QLatin1Char(' ') + command.path + QLatin1Char(' ') + command.shortcut
                                + QLatin1Char(' ') + action->toolTip())
                                   .toLower();
            m_commands.append(command);
        }

        std::sort(m_commands.begin(), m_commands.end(), [](const Command& left, const Command& right) {
            if (left.path != right.path) {
                // Commands that live on no menu at all come last.
                if (left.path.isEmpty() != right.path.isEmpty()) {
                    return right.path.isEmpty();
                }
                return left.path.localeAwareCompare(right.path) < 0;
            }
            return left.text.localeAwareCompare(right.text) < 0;
        });
    }

    void CommandPalette::clearRows()
    {
        for (QWidget* row : m_rows) {
            m_listLayout->removeWidget(row);
            row->deleteLater();
        }
        m_rows.clear();
        m_visible.clear();
        m_selected = -1;
    }

    void CommandPalette::applyFilter(const QString& query)
    {
        clearRows();

        const QStringList tokens = query.toLower().split(QLatin1Char(' '), Qt::SkipEmptyParts);

        int matches = 0;
        for (int i = 0; i < m_commands.size(); ++i) {
            const Command& command = m_commands.at(i);
            if (!command.action) {
                continue;
            }
            bool matched = true;
            for (const QString& token : tokens) {
                if (!command.haystack.contains(token)) {
                    matched = false;
                    break;
                }
            }
            if (!matched) {
                continue;
            }
            ++matches;
            if (m_rows.size() >= MaxResults) {
                continue;
            }

            const int rowIndex = m_rows.size();
            auto* row = new CommandRow(
                command.action,
                command.path,
                command.shortcut,
                [this, rowIndex] {
                    setSelection(rowIndex);
                    runSelected();
                },
                m_scroll->widget());
            m_listLayout->insertWidget(m_listLayout->count() - 1, row);
            m_rows.append(row);
            m_visible.append(i);
        }

        const bool empty = m_rows.isEmpty();
        m_emptyState->setVisible(empty);
        m_scroll->setVisible(!empty);
        // Whole rows only, so the list never ends on a half-drawn command.
        const int rowPitch = RowHeight + RowSpacing;
        const int wanted = qBound(ListMinHeight, m_rows.size() * rowPitch, ListMaxHeight);
        m_scroll->setFixedHeight(qMax(rowPitch, wanted - wanted % rowPitch));
        m_countLabel->setText(matches > m_rows.size()
                                  ? tr("%1 of %2 commands").arg(m_rows.size()).arg(m_commands.size())
                                  : tr("%n command(s)", "", matches));

        setSelection(empty ? -1 : 0);

        if (m_sheet) {
            m_sheet->adjustSize();
            centreSheet();
        }
    }

    void CommandPalette::setSelection(int index)
    {
        if (!m_rows.isEmpty()) {
            index = qBound(0, index, m_rows.size() - 1);
        } else {
            index = -1;
        }
        if (index == m_selected) {
            return;
        }

        if (m_selected >= 0 && m_selected < m_rows.size()) {
            static_cast<CommandRow*>(m_rows.at(m_selected))->setSelected(false);
        }
        m_selected = index;
        if (m_selected >= 0) {
            auto* row = static_cast<CommandRow*>(m_rows.at(m_selected));
            row->setSelected(true);
            m_scroll->ensureWidgetVisible(row, 0, RowHeight);
        }
    }

    void CommandPalette::moveSelection(int delta)
    {
        if (m_rows.isEmpty()) {
            return;
        }
        int index = m_selected + delta;
        if (index < 0) {
            index = m_rows.size() - 1;
        } else if (index >= m_rows.size()) {
            index = 0;
        }
        // setSelection() clamps, so wrap-around is resolved before the call.
        if (index == m_selected) {
            return;
        }
        static_cast<CommandRow*>(m_rows.at(m_selected))->setSelected(false);
        m_selected = index;
        auto* row = static_cast<CommandRow*>(m_rows.at(m_selected));
        row->setSelected(true);
        m_scroll->ensureWidgetVisible(row, 0, RowHeight);
    }

    void CommandPalette::runSelected()
    {
        if (m_selected < 0 || m_selected >= m_rows.size()) {
            return;
        }
        QAction* action = static_cast<CommandRow*>(m_rows.at(m_selected))->action();
        if (!action || !action->isEnabled()) {
            return;
        }

        closeOverlay();
        // The command may open a modal dialog; let the closing transition start
        // before it takes the event loop over.
        QMetaObject::invokeMethod(action, "trigger", Qt::QueuedConnection);
        emit commandTriggered(action);
    }

    bool CommandPalette::eventFilter(QObject* watched, QEvent* event)
    {
        if (m_search && watched == m_search->lineEdit() && event->type() == QEvent::KeyPress) {
            auto* keyEvent = static_cast<QKeyEvent*>(event);
            switch (keyEvent->key()) {
            case Qt::Key_Down:
                moveSelection(1);
                return true;
            case Qt::Key_Up:
                moveSelection(-1);
                return true;
            case Qt::Key_PageDown:
                moveSelection(8);
                return true;
            case Qt::Key_PageUp:
                moveSelection(-8);
                return true;
            case Qt::Key_Home:
                setSelection(0);
                return true;
            case Qt::Key_End:
                setSelection(m_rows.size() - 1);
                return true;
            case Qt::Key_Return:
            case Qt::Key_Enter:
                runSelected();
                return true;
            case Qt::Key_Escape:
                closeOverlay();
                return true;
            default:
                break;
            }
        }

        return Overlay::eventFilter(watched, event);
    }

    void CommandPalette::applyTheme()
    {
        if (auto* glyph = m_sheet->findChild<QLabel*>(QStringLiteral("commandPaletteGlyph"))) {
            glyph->setPixmap(Icons::pixmap(QStringLiteral("bolt"), HeaderGlyphSize, theme()->color(Role::Primary)));
        }
        if (auto* glyph = m_sheet->findChild<QLabel*>(QStringLiteral("commandPaletteEmptyGlyph"))) {
            glyph->setPixmap(
                Icons::pixmap(QStringLiteral("search_off"), EmptyGlyphSize, theme()->color(Role::OnSurfaceVariant)));
        }
        if (auto* label = m_sheet->findChild<QLabel*>(QStringLiteral("commandPaletteEmptyText"))) {
            styleLabel(label, TypeRole::BodyMedium, Role::OnSurfaceVariant);
        }
        if (auto* label = m_sheet->findChild<QLabel*>(QStringLiteral("commandPaletteHint"))) {
            styleLabel(label, TypeRole::LabelSmall, Role::OnSurfaceVariant);
        }
        styleLabel(m_headline, TypeRole::TitleLarge, Role::OnSurface);
        styleLabel(m_countLabel, TypeRole::LabelMedium, Role::OnSurfaceVariant);
        update();
    }

} // namespace Material
