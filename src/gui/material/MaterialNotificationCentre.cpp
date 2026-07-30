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

#include "MaterialNotificationCentre.h"

#include "MaterialButtons.h"
#include "MaterialElevation.h"
#include "MaterialIcons.h"
#include "MaterialTheme.h"
#include "MaterialTopAppBar.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QPainter>
#include <QRegion>
#include <QResizeEvent>
#include <QScrollArea>
#include <QShowEvent>
#include <QVBoxLayout>

namespace Material
{
    namespace
    {
        constexpr int SheetWidth = 400;
        constexpr int SheetMaxHeight = 560;

        /** The panel hangs under the app bar, inset from the window's right edge. */
        constexpr int AnchorTop = 60;
        constexpr int AnchorRight = 16;

        /** Room the el3 shadow needs outside the panel, kept inside the mask. */
        constexpr int ShadowMargin = 24;

        constexpr int HeaderSpacing = 10;
        constexpr int HeaderTopPadding = 18;
        constexpr int HeaderBottomPadding = 10;
        constexpr int ClearButtonHeight = 32;
        constexpr int CloseSize = 32;
        constexpr int CloseGlyphSize = 20;

        constexpr int RowHorizontalPadding = 20;
        constexpr int RowVerticalPadding = 12;
        constexpr int RowGap = 14;
        /** Gap between the body copy and the timestamp below it. */
        constexpr int StampGap = 4;
        constexpr int GlyphSize = 20;
        constexpr int EmptyGlyphSize = 40;

        /** A rounded panel filled with a colour role, used for the sheet and the rows. */
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
         * One flat list row. It has no fill and no radius of its own; a hairline
         * along its top edge is the only thing that separates it from the row
         * above, and from the header when it is the first one.
         */
        class SeparatorRow : public QWidget
        {
        public:
            explicit SeparatorRow(QWidget* parent = nullptr)
                : QWidget(parent)
            {
            }

        protected:
            void paintEvent(QPaintEvent* event) override
            {
                Q_UNUSED(event)
                QPainter painter(this);
                painter.setPen(theme()->color(Role::OutlineVariant));
                painter.drawLine(0, 0, width() - 1, 0);
            }
        };

        void styleLabel(QLabel* label, TypeRole type, Role color)
        {
            label->setFont(theme()->font(type));
            label->setStyleSheet(QStringLiteral("color:%1;background:transparent;").arg(theme()->hex(color)));
        }

        QLabel* makeLabel(const QString& text, TypeRole type, Role color)
        {
            auto* label = new QLabel(text);
            styleLabel(label, type, color);
            return label;
        }

        /**
         * The stamp on its own line under the body: a full, unambiguous date and
         * time rather than a locale-shortened one, because it is set in mono and
         * read down a column.
         */
        QLabel* makeStamp(const QDateTime& when)
        {
            auto* label = new QLabel(when.toString(QStringLiteral("yyyy-MM-dd HH:mm")));
            // Mono at the 11px step, which the type scale has no role of its own for.
            QFont font = theme()->font(TypeRole::Mono);
            font.setPointSize(theme()->font(TypeRole::LabelSmall).pointSize());
            label->setFont(font);
            label->setStyleSheet(
                QStringLiteral("color:%1;background:transparent;").arg(theme()->hex(Role::OnSurfaceVariant)));
            label->setContentsMargins(0, StampGap, 0, 0);
            return label;
        }
    } // namespace

    NotificationCentre::NotificationCentre(QWidget* parent)
        : Overlay(parent)
    {
        buildSheet();
        setSheetWidth(SheetWidth);
        // Anchored under the app bar rather than centred, and non-modal: a click
        // outside the panel belongs to whatever is under it, not to the panel.
        setSheetTopMargin(AnchorTop);
        setCloseOnClickOutside(false);
        setSheetWidget(m_sheet);
        anchorSheet();

        connect(theme(), &Theme::changed, this, &NotificationCentre::applyTheme);
        applyTheme();
        rebuild();
        // Claim the app bar's notifications button now, so the bell opens the
        // panel even before anything has been filed.
        refreshBadge();
    }

    NotificationCentre::~NotificationCentre() = default;

    NotificationCentre* NotificationCentre::centreFor(QWidget* widget)
    {
        QWidget* window = widget ? widget->window() : nullptr;
        if (!window) {
            return nullptr;
        }
        auto* centre = window->findChild<NotificationCentre*>(QString(), Qt::FindDirectChildrenOnly);
        if (!centre) {
            centre = new NotificationCentre(window);
        }
        return centre;
    }

    quint64 NotificationCentre::record(SeverityLevel severity,
                                       const QString& title,
                                       const QString& body,
                                       const QList<NotificationAction>& actions)
    {
        Notification entry;
        entry.id = m_nextId++;
        entry.severity = severity;
        entry.title = title;
        entry.body = body;
        entry.timestamp = QDateTime::currentDateTime();
        entry.actions = actions;
        entry.read = isOpen();

        m_items.prepend(entry);
        while (m_items.size() > MaxHistory) {
            m_items.removeLast();
        }

        if (isOpen()) {
            rebuild();
        }
        refreshBadge();
        return entry.id;
    }

    void NotificationCentre::updateEntry(quint64 id, const QString& body, SeverityLevel severity)
    {
        for (auto& entry : m_items) {
            if (entry.id != id) {
                continue;
            }
            entry.body = body;
            entry.severity = severity;
            entry.timestamp = QDateTime::currentDateTime();
            if (isOpen()) {
                rebuild();
            }
            return;
        }
    }

    void NotificationCentre::attachAppBar(TopAppBar* bar)
    {
        if (m_appBar == bar) {
            return;
        }
        m_appBar = bar;
        if (!bar) {
            return;
        }
        // The bell toggles: pressing it again while the panel is up puts it away.
        connect(
            bar, &TopAppBar::notificationsRequested, this, &NotificationCentre::toggleOverlay, Qt::UniqueConnection);
        bar->setNotificationCount(unreadCount());
    }

    QList<Notification> NotificationCentre::notifications() const
    {
        return m_items;
    }

    int NotificationCentre::count() const
    {
        return m_items.size();
    }

    int NotificationCentre::unreadCount() const
    {
        int unread = 0;
        for (const auto& entry : m_items) {
            if (!entry.read) {
                ++unread;
            }
        }
        return unread;
    }

    void NotificationCentre::clearAll()
    {
        m_items.clear();
        rebuild();
        refreshBadge();
        // Clearing the history is also how the panel is put away: the design
        // gives the two the same gesture.
        closeOverlay();
    }

    void NotificationCentre::markAllRead()
    {
        bool changed = false;
        for (auto& entry : m_items) {
            if (!entry.read) {
                entry.read = true;
                changed = true;
            }
        }
        if (changed) {
            refreshBadge();
        }
    }

    void NotificationCentre::removeEntry(quint64 id)
    {
        for (int i = 0; i < m_items.size(); ++i) {
            if (m_items.at(i).id == id) {
                m_items.removeAt(i);
                rebuild();
                refreshBadge();
                return;
            }
        }
    }

    void NotificationCentre::aboutToOpen()
    {
        rebuild();
        markAllRead();
    }

    void NotificationCentre::buildSheet()
    {
        m_sheet = new Panel(Shape::ExtraLarge, Role::SurfaceContainerLowest, true);
        // The whole panel is bounded, header and rows together, not just the list.
        m_sheet->setMaximumHeight(SheetMaxHeight);

        auto* layout = new QVBoxLayout(m_sheet);
        // The panel carries no padding of its own: the header and the rows bring
        // theirs, so a row's top hairline runs the full width of the panel.
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
        layout->addWidget(buildHeader());

        auto* list = new QWidget;
        m_listLayout = new QVBoxLayout(list);
        m_listLayout->setContentsMargins(0, 0, 0, 0);
        // Rows butt against each other; the hairline is the whole separation.
        m_listLayout->setSpacing(0);
        m_listLayout->addStretch(1);

        m_scroll = new QScrollArea;
        m_scroll->setWidget(list);
        m_scroll->setWidgetResizable(true);
        m_scroll->setFrameShape(QFrame::NoFrame);
        m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_scroll->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
        m_scroll->setAccessibleName(tr("Notification history"));
        layout->addWidget(m_scroll, 1);

        m_emptyState = new QWidget;
        auto* empty = new QVBoxLayout(m_emptyState);
        empty->setContentsMargins(RowHorizontalPadding, 32, RowHorizontalPadding, 32);
        empty->setSpacing(8);
        auto* emptyGlyph = new QLabel;
        emptyGlyph->setObjectName(QStringLiteral("notificationEmptyGlyph"));
        emptyGlyph->setAlignment(Qt::AlignCenter);
        empty->addWidget(emptyGlyph);
        auto* emptyTitle = makeLabel(tr("Nothing to review"), TypeRole::TitleSmall, Role::OnSurface);
        emptyTitle->setAlignment(Qt::AlignCenter);
        empty->addWidget(emptyTitle);
        auto* emptyBody =
            makeLabel(tr("Messages the application shows you are kept here."), TypeRole::BodySmall, Role::OnSurfaceVariant);
        emptyBody->setAlignment(Qt::AlignCenter);
        emptyBody->setWordWrap(true);
        empty->addWidget(emptyBody);
        layout->addWidget(m_emptyState);
    }

    QWidget* NotificationCentre::buildHeader()
    {
        auto* header = new QWidget;
        auto* layout = new QHBoxLayout(header);
        layout->setContentsMargins(RowHorizontalPadding, HeaderTopPadding, RowHorizontalPadding, HeaderBottomPadding);
        layout->setSpacing(HeaderSpacing);

        // Three children and no more: the title, the Clear all pill, the close.
        m_headline = makeLabel(tr("Notifications"), TypeRole::TitleMedium, Role::OnSurface);
        layout->addWidget(m_headline, 1);

        m_clearButton = new TextButton(QString(), tr("Clear all"));
        m_clearButton->setFixedHeight(ClearButtonHeight);
        m_clearButton->setToolTip(tr("Remove every notification from the history"));
        connect(m_clearButton, &QAbstractButton::clicked, this, &NotificationCentre::clearAll);
        layout->addWidget(m_clearButton, 0, Qt::AlignVCenter);

        auto* close = new IconButton(QStringLiteral("close"));
        close->setDiameter(CloseSize);
        close->setSymbolSize(CloseGlyphSize);
        close->setToolTip(tr("Close"));
        close->setAccessibleName(tr("Close the notification centre"));
        connect(close, &IconButton::clicked, this, &NotificationCentre::toggleOverlay);
        layout->addWidget(close, 0, Qt::AlignVCenter);

        return header;
    }

    QWidget* NotificationCentre::buildRow(const Notification& entry)
    {
        auto* row = new SeparatorRow;
        row->setAccessibleName(QStringLiteral("%1: %2").arg(severityName(entry.severity),
                                                            entry.title.isEmpty() ? entry.body : entry.title));
        row->setAccessibleDescription(entry.body);

        auto* layout = new QHBoxLayout(row);
        layout->setContentsMargins(RowHorizontalPadding, RowVerticalPadding, RowHorizontalPadding, RowVerticalPadding);
        layout->setSpacing(RowGap);

        // Two children: the severity glyph and the text column. Removal is the
        // Clear all button's job, so a row carries no affordance of its own.
        auto* glyph = new QLabel;
        glyph->setPixmap(Icons::pixmap(severitySymbol(entry.severity), GlyphSize, severityAccent(entry.severity)));
        glyph->setFixedSize(GlyphSize, GlyphSize);
        layout->addWidget(glyph, 0, Qt::AlignTop);

        auto* column = new QVBoxLayout;
        column->setContentsMargins(0, 0, 0, 0);
        column->setSpacing(0);

        const QString title = entry.title.isEmpty() ? severityName(entry.severity) : entry.title;
        column->addWidget(makeLabel(title, TypeRole::LabelLarge, Role::OnSurface));

        auto* body = makeLabel(entry.body, TypeRole::BodyMedium, Role::OnSurfaceVariant);
        body->setWordWrap(true);
        column->addWidget(body);

        // The stamp reads under the body, not beside the title.
        column->addWidget(makeStamp(entry.timestamp));
        layout->addLayout(column, 1);

        return row;
    }

    void NotificationCentre::clearList()
    {
        while (m_listLayout->count() > 0) {
            QLayoutItem* item = m_listLayout->takeAt(0);
            if (QWidget* widget = item->widget()) {
                // A rebuild can be triggered from a signal a row is still
                // emitting, so the row has to outlive it.
                widget->hide();
                widget->setParent(nullptr);
                widget->deleteLater();
            }
            delete item;
        }
    }

    void NotificationCentre::rebuild()
    {
        clearList();

        for (const auto& entry : m_items) {
            m_listLayout->addWidget(buildRow(entry));
        }
        m_listLayout->addStretch(1);

        m_emptyState->setVisible(m_items.isEmpty());
        m_scroll->setVisible(!m_items.isEmpty());

        m_clearButton->setEnabled(!m_items.isEmpty());

        if (m_sheet) {
            m_sheet->adjustSize();
            centreSheet();
            anchorSheet();
        }
    }

    void NotificationCentre::refreshBadge()
    {
        if (!m_appBar) {
            // The shell owns the app bar and may build it after the centre, so
            // the lookup repeats until one turns up.
            if (QWidget* window = parentWidget() ? parentWidget()->window() : nullptr) {
                if (auto* bar = window->findChild<TopAppBar*>()) {
                    attachAppBar(bar);
                }
            }
        }
        const int unread = unreadCount();
        if (m_appBar) {
            m_appBar->setNotificationCount(unread);
        }
        emit unreadCountChanged(unread);
    }

    void NotificationCentre::applyTheme()
    {
        if (auto* glyph = m_sheet->findChild<QLabel*>(QStringLiteral("notificationEmptyGlyph"))) {
            glyph->setPixmap(
                Icons::pixmap(QStringLiteral("search_off"), EmptyGlyphSize, theme()->color(Role::OnSurfaceVariant)));
        }
        styleLabel(m_headline, TypeRole::TitleMedium, Role::OnSurface);
        rebuild();
        update();
    }

    void NotificationCentre::toggleOverlay()
    {
        isOpen() ? closeOverlay() : openOverlay();
    }

    void NotificationCentre::paintEvent(QPaintEvent* event)
    {
        Q_UNUSED(event)
        // No scrim: the design leaves the rest of the interface visible and
        // clickable while the panel is up. Only the panel's shadow is ours.
        if (!m_sheet || transition() <= 0.0) {
            return;
        }

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setOpacity(transition());
        paintShadow(&painter, m_sheet->geometry(), Shape::ExtraLarge, 3);
    }

    void NotificationCentre::resizeEvent(QResizeEvent* event)
    {
        Overlay::resizeEvent(event);
        anchorSheet();
    }

    void NotificationCentre::showEvent(QShowEvent* event)
    {
        Overlay::showEvent(event);
        anchorSheet();
    }

    bool NotificationCentre::eventFilter(QObject* watched, QEvent* event)
    {
        const bool handled = Overlay::eventFilter(watched, event);
        // The base centres the sheet from several places, including every frame
        // of the transition. Each of those arrives here as a move or a resize,
        // and is pulled straight back to the corner the design anchors it to.
        if (watched == m_sheet && (event->type() == QEvent::Move || event->type() == QEvent::Resize)) {
            anchorSheet();
        }
        return handled;
    }

    void NotificationCentre::anchorSheet()
    {
        if (!m_sheet) {
            return;
        }

        const int x = width() - m_sheet->width() - AnchorRight;
        if (m_sheet->x() != x) {
            m_sheet->move(x, m_sheet->y());
        }

        // Everything outside the panel and its shadow is masked away, so the
        // overlay covers the window without taking a single click from it.
        setMask(QRegion(m_sheet->geometry().adjusted(-ShadowMargin, -ShadowMargin, ShadowMargin, ShadowMargin)));
    }

} // namespace Material
