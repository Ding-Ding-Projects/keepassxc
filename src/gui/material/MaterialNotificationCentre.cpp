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
#include "MaterialSegmentedButton.h"
#include "MaterialTheme.h"
#include "MaterialTopAppBar.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QLocale>
#include <QPainter>
#include <QScrollArea>
#include <QVBoxLayout>

namespace Material
{
    namespace
    {
        constexpr int SheetWidth = 480;
        constexpr int SheetPadding = 24;
        constexpr int ListMaxHeight = 520;
        constexpr int RowPadding = 14;
        constexpr int RowSpacing = 8;
        constexpr int GlyphSize = 20;
        constexpr int HeaderGlyphSize = 26;
        constexpr int EmptyGlyphSize = 40;
        constexpr int DismissSize = 32;

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

        /** Today shows the clock only; anything older carries its date. */
        QString stamp(const QDateTime& when)
        {
            const QLocale locale = QLocale::system();
            if (when.date() == QDate::currentDate()) {
                return locale.toString(when.time(), QLocale::ShortFormat);
            }
            return locale.toString(when, QLocale::ShortFormat);
        }

        QString filterId(SeverityLevel severity)
        {
            switch (severity) {
            case SeverityLevel::Success:
                return QStringLiteral("success");
            case SeverityLevel::Warning:
                return QStringLiteral("warning");
            case SeverityLevel::Error:
                return QStringLiteral("error");
            case SeverityLevel::Info:
                break;
            }
            return QStringLiteral("info");
        }
    } // namespace

    NotificationCentre::NotificationCentre(QWidget* parent)
        : Overlay(parent)
    {
        buildSheet();
        setSheetWidth(SheetWidth);
        setSheetWidget(m_sheet);

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
        connect(bar, &TopAppBar::notificationsRequested, this, &Overlay::openOverlay, Qt::UniqueConnection);
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

    QString NotificationCentre::filter() const
    {
        return m_filter;
    }

    void NotificationCentre::setFilter(const QString& id)
    {
        if (id == m_filter) {
            return;
        }
        m_filter = id;
        if (m_filterBar) {
            m_filterBar->setCurrentSegment(id);
        }
        rebuild();
    }

    void NotificationCentre::clearAll()
    {
        if (m_items.isEmpty()) {
            return;
        }
        m_items.clear();
        rebuild();
        refreshBadge();
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
        m_sheet = new Panel(Shape::ExtraLarge, Role::SurfaceContainerLowest, false);

        auto* layout = new QVBoxLayout(m_sheet);
        layout->setContentsMargins(SheetPadding, 20, SheetPadding, 20);
        layout->setSpacing(14);
        layout->addWidget(buildHeader());

        m_filterBar = new SegmentedButton;
        m_filterBar->addSegment(QStringLiteral("all"), tr("All"));
        m_filterBar->addSegment(QStringLiteral("info"), severityName(SeverityLevel::Info));
        m_filterBar->addSegment(QStringLiteral("success"), severityName(SeverityLevel::Success));
        m_filterBar->addSegment(QStringLiteral("warning"), severityName(SeverityLevel::Warning));
        m_filterBar->addSegment(QStringLiteral("error"), severityName(SeverityLevel::Error));
        m_filterBar->setAccessibleName(tr("Filter notifications by severity"));
        connect(m_filterBar, &SegmentedButton::segmentSelected, this, &NotificationCentre::setFilter);
        layout->addWidget(m_filterBar);

        auto* list = new QWidget;
        m_listLayout = new QVBoxLayout(list);
        m_listLayout->setContentsMargins(0, 0, 0, 0);
        m_listLayout->setSpacing(RowSpacing);
        m_listLayout->addStretch(1);

        m_scroll = new QScrollArea;
        m_scroll->setWidget(list);
        m_scroll->setWidgetResizable(true);
        m_scroll->setFrameShape(QFrame::NoFrame);
        m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_scroll->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
        m_scroll->setMaximumHeight(ListMaxHeight);
        m_scroll->setAccessibleName(tr("Notification history"));
        layout->addWidget(m_scroll, 1);

        m_emptyState = new QWidget;
        auto* empty = new QVBoxLayout(m_emptyState);
        empty->setContentsMargins(0, 32, 0, 32);
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
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(12);

        auto* symbol = new QLabel;
        symbol->setObjectName(QStringLiteral("notificationHeaderGlyph"));
        layout->addWidget(symbol, 0, Qt::AlignTop);

        auto* titles = new QVBoxLayout;
        titles->setContentsMargins(0, 0, 0, 0);
        titles->setSpacing(0);
        m_headline = makeLabel(tr("Notifications"), TypeRole::TitleLarge, Role::OnSurface);
        titles->addWidget(m_headline);
        m_subhead = makeLabel(QString(), TypeRole::LabelSmall, Role::OnSurfaceVariant);
        titles->addWidget(m_subhead);
        layout->addLayout(titles, 1);

        m_clearButton = new TextButton(QStringLiteral("delete"), tr("Clear all"));
        m_clearButton->setToolTip(tr("Remove every notification from the history"));
        connect(m_clearButton, &QAbstractButton::clicked, this, &NotificationCentre::clearAll);
        layout->addWidget(m_clearButton, 0, Qt::AlignVCenter);

        auto* close = new IconButton(QStringLiteral("close"));
        close->setToolTip(tr("Close"));
        close->setAccessibleName(tr("Close the notification centre"));
        connect(close, &IconButton::clicked, this, &Overlay::closeOverlay);
        layout->addWidget(close, 0, Qt::AlignVCenter);

        return header;
    }

    QWidget* NotificationCentre::buildRow(const Notification& entry)
    {
        // Unread entries are filled so the backlog is visible at a glance.
        auto* row = new Panel(Shape::Row,
                              entry.read ? Role::SurfaceContainerLowest : Role::SurfaceContainer,
                              entry.read);
        row->setAccessibleName(QStringLiteral("%1: %2").arg(severityName(entry.severity),
                                                            entry.title.isEmpty() ? entry.body : entry.title));
        row->setAccessibleDescription(entry.body);

        auto* layout = new QHBoxLayout(row);
        layout->setContentsMargins(RowPadding, RowPadding, RowPadding - 4, RowPadding);
        layout->setSpacing(12);

        auto* glyph = new QLabel;
        glyph->setPixmap(Icons::pixmap(severitySymbol(entry.severity), GlyphSize, severityAccent(entry.severity)));
        glyph->setFixedSize(GlyphSize, GlyphSize);
        layout->addWidget(glyph, 0, Qt::AlignTop);

        auto* column = new QVBoxLayout;
        column->setContentsMargins(0, 0, 0, 0);
        column->setSpacing(2);

        auto* titleRow = new QHBoxLayout;
        titleRow->setContentsMargins(0, 0, 0, 0);
        titleRow->setSpacing(8);
        const QString title = entry.title.isEmpty() ? severityName(entry.severity) : entry.title;
        titleRow->addWidget(makeLabel(title, TypeRole::LabelLarge, Role::OnSurface), 1);
        titleRow->addWidget(makeLabel(stamp(entry.timestamp), TypeRole::LabelSmall, Role::OnSurfaceVariant));
        column->addLayout(titleRow);

        auto* body = makeLabel(entry.body, TypeRole::BodyMedium, Role::OnSurfaceVariant);
        body->setWordWrap(true);
        column->addWidget(body);

        QList<NotificationAction> live;
        for (const auto& action : entry.actions) {
            if (action.isValid() && action.handler) {
                live.append(action);
            }
        }
        if (!live.isEmpty()) {
            auto* actions = new QHBoxLayout;
            actions->setContentsMargins(0, 6, 0, 0);
            actions->setSpacing(4);
            for (const auto& action : live) {
                auto* button = new TextButton(QString(), action.label);
                connect(button, &QAbstractButton::clicked, this, [this, action] {
                    if (action.isValid() && action.handler) {
                        action.handler();
                    }
                    closeOverlay();
                });
                actions->addWidget(button);
            }
            actions->addStretch(1);
            column->addLayout(actions);
        }
        layout->addLayout(column, 1);

        auto* dismiss = new IconButton(QStringLiteral("close"));
        dismiss->setDiameter(DismissSize);
        dismiss->setSymbolSize(16);
        dismiss->setToolTip(tr("Remove this notification"));
        dismiss->setAccessibleName(tr("Remove this notification"));
        const quint64 id = entry.id;
        connect(dismiss, &IconButton::clicked, this, [this, id] { removeEntry(id); });
        layout->addWidget(dismiss, 0, Qt::AlignTop);

        return row;
    }

    void NotificationCentre::clearList()
    {
        while (m_listLayout->count() > 0) {
            QLayoutItem* item = m_listLayout->takeAt(0);
            if (QWidget* widget = item->widget()) {
                // A rebuild can be triggered from a row's own dismiss button, so
                // the row has to outlive the signal it is emitting.
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

        int shown = 0;
        for (const auto& entry : m_items) {
            if (m_filter != QStringLiteral("all") && filterId(entry.severity) != m_filter) {
                continue;
            }
            m_listLayout->addWidget(buildRow(entry));
            ++shown;
        }
        m_listLayout->addStretch(1);

        m_emptyState->setVisible(shown == 0);
        m_scroll->setVisible(shown > 0);

        const int unread = unreadCount();
        m_subhead->setText(unread > 0 ? tr("%n unread of %1", "", unread).arg(m_items.size())
                                      : tr("%n in the history", "", m_items.size()));
        m_clearButton->setEnabled(!m_items.isEmpty());

        if (m_sheet) {
            m_sheet->adjustSize();
            centreSheet();
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
        if (auto* glyph = m_sheet->findChild<QLabel*>(QStringLiteral("notificationHeaderGlyph"))) {
            glyph->setPixmap(
                Icons::pixmap(QStringLiteral("notifications"), HeaderGlyphSize, theme()->color(Role::Primary)));
        }
        if (auto* glyph = m_sheet->findChild<QLabel*>(QStringLiteral("notificationEmptyGlyph"))) {
            glyph->setPixmap(
                Icons::pixmap(QStringLiteral("search_off"), EmptyGlyphSize, theme()->color(Role::OnSurfaceVariant)));
        }
        styleLabel(m_headline, TypeRole::TitleLarge, Role::OnSurface);
        styleLabel(m_subhead, TypeRole::LabelSmall, Role::OnSurfaceVariant);
        rebuild();
        update();
    }

} // namespace Material
