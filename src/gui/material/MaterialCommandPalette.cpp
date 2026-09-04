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

#include "MaterialButtons.h"
#include "MaterialElevation.h"
#include "MaterialIcons.h"
#include "MaterialTheme.h"
#include "MaterialSearchBar.h"
#include "MaterialRegexSafety.h"

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
#include <QPalette>
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
        constexpr int SheetWidth = 720; // under the 840 px minimum window, so it is never clamped
        constexpr int SheetMaxHeight = 720;
        // The palette hangs below the top of the window; it is not centred.
        constexpr int SheetTopMargin = 90;
        constexpr int HeaderHeight = 64;
        constexpr int HeaderPadding = 20;
        constexpr int HeaderSpacing = 12;
        constexpr int HeaderGlyphSize = 24;
        constexpr int HeaderButtonSize = 36;
        constexpr int ListMarginSide = 12;
        constexpr int ListMarginTop = 8;
        constexpr int ListMarginBottom = 16;
        constexpr int ListMinHeight = 220;
        // Everything the sheet has left once the header has taken its 64px.
        constexpr int ListMaxHeight = SheetMaxHeight - HeaderHeight;
        constexpr int RowHeight = 44;
        constexpr int RowSpacing = 2;
        constexpr int RowPadding = 12;
        constexpr int RowGlyphSize = 20;
        constexpr int RowTextGap = 14;
        constexpr int ShortcutPadding = 8;
        constexpr int ShortcutHeight = 24;
        // Padding around the group heading: 14 above, 10 either side, 6 below.
        constexpr int HeadingSide = 10;
        constexpr int HeadingTop = 14;
        constexpr int HeadingBottom = 6;
        constexpr int EmptyPadding = 40;
        constexpr qreal DisabledOpacity = 0.38;

        /** The 12px monospace face the shortcut pill is set in. */
        QFont shortcutFont()
        {
            QFont font = theme()->font(TypeRole::Mono);
            font.setPointSize(qMax(1, qRound(font.pointSize() * 12.0 / 14.0)));
            return font;
        }

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

        /** The 64px header row, with the design's hairline underneath it. */
        class HeaderRow : public QWidget
        {
        public:
            explicit HeaderRow(QWidget* parent = nullptr)
                : QWidget(parent)
            {
                setFixedHeight(HeaderHeight);
            }

        protected:
            void paintEvent(QPaintEvent* event) override
            {
                Q_UNUSED(event)
                QPainter painter(this);
                painter.fillRect(0, height() - 1, width(), 1, theme()->color(Role::OutlineVariant));
            }
        };

        /**
         * One command in the list: the action's icon, its text and its
         * shortcut - the menu it belongs to is the heading above it. Painted
         * rather than built from labels so that a filter pass rebuilds sixty
         * rows without a layout storm.
         */
        class CommandRow : public QWidget
        {
        public:
            CommandRow(QAction* action,
                       const QString& shortcut,
                       std::function<void()> activate,
                       QWidget* parent = nullptr)
                : QWidget(parent)
                , m_action(action)
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
                    paintSurface(&painter, rect(), Shape::Large, theme()->color(Role::SecondaryContainer));
                    content = theme()->color(Role::OnSecondaryContainer);
                    secondary = theme()->color(Role::OnSecondaryContainer);
                } else if (m_hovered) {
                    paintStateLayer(&painter, rect(), Shape::Large, theme()->color(Role::OnSurface), 0.08);
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

                // The shortcut pill sits hard right and the command text takes
                // whatever is left over, eliding into it.
                if (!m_shortcut.isEmpty()) {
                    const QFont keysFont = shortcutFont();
                    const QFontMetrics metrics(keysFont);
                    const int pillWidth = metrics.horizontalAdvance(m_shortcut) + 2 * ShortcutPadding;
                    const QRect pill(right - pillWidth, (height() - ShortcutHeight) / 2, pillWidth, ShortcutHeight);
                    paintSurface(&painter, pill, Shape::ExtraSmall, theme()->color(Role::SurfaceContainer));
                    painter.setFont(keysFont);
                    painter.setPen(theme()->color(Role::OnSurfaceVariant));
                    painter.drawText(pill, Qt::AlignCenter, m_shortcut);
                    right = pill.left() - 10;
                }

                const int textLeft = glyph.right() + RowTextGap;
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

        /** The uppercase overline that opens each menu's block of commands. */
        void styleHeading(QLabel* label)
        {
            QFont font = theme()->font(TypeRole::LabelSmall);
            font.setCapitalization(QFont::AllUppercase);
            font.setLetterSpacing(QFont::AbsoluteSpacing, 0.8);
            label->setFont(font);
            label->setStyleSheet(
                QStringLiteral("color:%1;background:transparent;").arg(theme()->hex(Role::OnSurfaceVariant)));
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
        setSheetTopMargin(SheetTopMargin);
        setSheetWidget(m_sheet);

        m_source = parent ? parent->window() : nullptr;

        connect(theme(), &Theme::changed, this, &CommandPalette::applyTheme);
        connect(this, &Overlay::opened, this, [this] {
            m_searchEdit->lineEdit()->setFocus(Qt::PopupFocusReason);
            m_searchEdit->lineEdit()->selectAll();
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
        m_sheet = new Panel(Shape::ExtraLarge, Role::SurfaceContainerLowest, false);
        m_sheet->setObjectName(QStringLiteral("commandPaletteSheet"));

        auto* layout = new QVBoxLayout(m_sheet);
        // The header and the list carry their own padding, and the divider
        // between them has to run the full width of the sheet.
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        auto* header = new HeaderRow(m_sheet);
        auto* headerLayout = new QHBoxLayout(header);
        headerLayout->setContentsMargins(HeaderPadding, 0, HeaderPadding, 0);
        headerLayout->setSpacing(HeaderSpacing);

        auto* glyph = new QLabel(header);
        glyph->setObjectName(QStringLiteral("commandPaletteGlyph"));
        glyph->setFixedSize(HeaderGlyphSize, HeaderGlyphSize);
        headerLayout->addWidget(glyph);

        // The search field is the header: there is no headline over it and no
        // filled pill around it.
        m_searchEdit = new SearchBar(SearchBar::Variant::Surface, header);
        m_searchEdit->setPlaceholder(tr("Search every action, setting and shortcut"));
        m_searchEdit->setIdentity(QStringLiteral("command-palette.commands"), tr("Command palette search"));
        connect(m_searchEdit, &SearchBar::textChanged, this, &CommandPalette::applyFilter);
        m_searchEdit->lineEdit()->installEventFilter(this);
        headerLayout->addWidget(m_searchEdit, 1);

        auto* close = new IconButton(QStringLiteral("close"), header);
        close->setDiameter(HeaderButtonSize);
        close->setToolTip(tr("Close"));
        connect(close, &IconButton::clicked, this, &Overlay::closeOverlay);
        headerLayout->addWidget(close);

        layout->addWidget(header);

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
        m_listLayout->setContentsMargins(ListMarginSide, ListMarginTop, ListMarginSide, ListMarginBottom);
        m_listLayout->setSpacing(RowSpacing);
        m_listLayout->addStretch();
        m_scroll->setWidget(list);
        layout->addWidget(m_scroll, 1);

        // One centred line quoting the query, in place of the list.
        m_emptyLabel = new QLabel(m_sheet);
        m_emptyLabel->setObjectName(QStringLiteral("commandPaletteEmpty"));
        m_emptyLabel->setAlignment(Qt::AlignCenter);
        m_emptyLabel->setWordWrap(true);
        m_emptyLabel->setContentsMargins(EmptyPadding, EmptyPadding, EmptyPadding, EmptyPadding);
        m_emptyLabel->hide();
        layout->addWidget(m_emptyLabel);
    }

    void CommandPalette::aboutToOpen()
    {
        collect();
        m_searchEdit->blockSignals(true);
        m_searchEdit->clear();
        m_searchEdit->blockSignals(false);
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
            // The heading a command is filed under is its top level menu, so
            // "Database ▸ Export ▸ CSV File…" lands under Database.
            command.group = command.path.section(QStringLiteral(" ▸ "), 0, 0);
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
        for (QWidget* heading : m_headings) {
            m_listLayout->removeWidget(heading);
            heading->deleteLater();
        }
        m_rows.clear();
        m_headings.clear();
        m_visible.clear();
        m_selected = -1;
    }

    void CommandPalette::applyFilter(const QString& query)
    {
        clearRows();

        const QStringList tokens = query.toLower().split(QLatin1Char(' '), Qt::SkipEmptyParts);

        QString group;
        for (int i = 0; i < m_commands.size(); ++i) {
            const Command& command = m_commands.at(i);
            if (!command.action) {
                continue;
            }
            bool matched = true;
            if (m_searchEdit->isRegexEnabled() && !query.isEmpty()) {
                const auto run = runBounded(query, optionsForFlags(m_searchEdit->regexFlags()), command.haystack);
                matched = run.compiled && !run.blocked && !run.timedOut && !run.matches.isEmpty();
            } else {
                for (const QString& token : tokens) {
                    if (!command.haystack.contains(token)) {
                        matched = false;
                        break;
                    }
                }
            }
            if (!matched) {
                continue;
            }
            if (m_rows.size() >= MaxResults) {
                break;
            }

            // A heading opens each menu's block. Commands are sorted by path,
            // so a filter that empties a menu drops its heading with it.
            if (command.group != group) {
                group = command.group;
                if (!group.isEmpty()) {
                    auto* heading = new QLabel(group, m_scroll->widget());
                    heading->setObjectName(QStringLiteral("commandPaletteHeading"));
                    heading->setContentsMargins(HeadingSide, HeadingTop, HeadingSide, HeadingBottom);
                    styleHeading(heading);
                    m_listLayout->insertWidget(m_listLayout->count() - 1, heading);
                    m_headings.append(heading);
                }
            }

            const int rowIndex = m_rows.size();
            auto* row = new CommandRow(
                command.action,
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
        m_emptyLabel->setText(tr("No action matches “%1”.").arg(query));
        m_emptyLabel->setVisible(empty);
        m_scroll->setVisible(!empty);

        // The list flexes with its content until the sheet reaches its 720px.
        QWidget* list = m_scroll->widget();
        list->layout()->activate();
        m_scroll->setFixedHeight(qBound(ListMinHeight, list->sizeHint().height(), ListMaxHeight));

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
        if (m_searchEdit && watched == m_searchEdit->lineEdit() && event->type() == QEvent::KeyPress) {
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
            glyph->setPixmap(
                Icons::pixmap(QStringLiteral("bolt"), HeaderGlyphSize, theme()->color(Role::OnSurfaceVariant)));
        }
        styleLabel(m_emptyLabel, TypeRole::BodyLarge, Role::OnSurfaceVariant);
        for (QLabel* heading : m_sheet->findChildren<QLabel*>(QStringLiteral("commandPaletteHeading"))) {
            styleHeading(heading);
        }

        // A bare input, the way the design's header row draws it.
        m_searchEdit->lineEdit()->setFont(theme()->font(TypeRole::TitleSmall));
        m_searchEdit->lineEdit()->setStyleSheet(QStringLiteral("QLineEdit{border:none;background:transparent;padding:0;"
                                                   "color:%1;selection-background-color:%2;selection-color:%3;}")
                                        .arg(theme()->hex(Role::OnSurface),
                                             theme()->hex(Role::SecondaryContainer),
                                             theme()->hex(Role::OnSecondaryContainer)));
        QPalette editPalette = m_searchEdit->lineEdit()->palette();
        editPalette.setColor(QPalette::Text, theme()->color(Role::OnSurface));
        editPalette.setColor(QPalette::PlaceholderText, theme()->color(Role::OnSurfaceVariant));
        m_searchEdit->lineEdit()->setPalette(editPalette);

        update();
    }

} // namespace Material
