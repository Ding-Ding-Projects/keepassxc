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

#ifndef KEEPASSXC_MATERIALNOTIFICATIONCENTRE_H
#define KEEPASSXC_MATERIALNOTIFICATIONCENTRE_H

#include "MaterialOverlay.h"
#include "MaterialSnackbar.h"

#include <QDateTime>
#include <QList>
#include <QPointer>
#include <QString>

class QLabel;
class QScrollArea;
class QVBoxLayout;

namespace Material
{
    class ButtonBase;
    class SearchBar;
    class TopAppBar;

    /** One line of the notification history. */
    struct Notification
    {
        quint64 id = 0;
        SeverityLevel severity = SeverityLevel::Info;
        QString title;
        QString body;
        QDateTime timestamp;
        QList<NotificationAction> actions;
        bool read = false;
    };

    /**
     * The reviewable history behind the snackbars.
     *
     * A snackbar is gone in a few seconds; everything it said is kept here,
     * newest first, with its severity and its timestamp. The panel is toggled
     * from the app bar's notifications button, whose badge carries the unread
     * count, and Clear all empties the history and closes it in one gesture.
     *
     * It is an Overlay for its sheet transition and its Escape handling only:
     * unlike every other overlay this one is not modal. It hangs under the app
     * bar at the top right of the window, draws no scrim, and masks everything
     * but its own panel away so the interface behind it stays live.
     *
     * History is capped at MaxHistory entries; the oldest fall off the end.
     */
    class NotificationCentre : public Overlay
    {
        Q_OBJECT

    public:
        /** Entries kept before the oldest are forgotten. */
        static constexpr int MaxHistory = 200;

        explicit NotificationCentre(QWidget* parent = nullptr);
        ~NotificationCentre() override;

        /** The centre covering @p widget's window, created on first use. */
        static NotificationCentre* centreFor(QWidget* widget);

        /** File a notification and return the id that identifies it later. */
        quint64 record(SeverityLevel severity,
                       const QString& title,
                       const QString& body,
                       const QList<NotificationAction>& actions = {});

        /** Rewrite an entry filed earlier, e.g. a job that has since finished. */
        void updateEntry(quint64 id, const QString& body, SeverityLevel severity);

        /** Take the badge over, and open on the app bar's notifications button. */
        void attachAppBar(TopAppBar* bar);

        QList<Notification> notifications() const;
        int count() const;
        int unreadCount() const;

    public slots:
        /** Empty the history and close the panel, the way the design pairs them. */
        void clearAll();
        void markAllRead();
        void removeEntry(quint64 id);

    signals:
        void unreadCountChanged(int count);

    protected:
        /** Opening the panel is what marks the backlog as seen. */
        void aboutToOpen() override;

        /** The panel's own el3 shadow and nothing else - there is no scrim. */
        void paintEvent(QPaintEvent* event) override;
        void resizeEvent(QResizeEvent* event) override;
        void showEvent(QShowEvent* event) override;
        bool eventFilter(QObject* watched, QEvent* event) override;

    private:
        void buildSheet();
        QWidget* buildHeader();
        QWidget* buildRow(const Notification& notification);
        void clearList();
        void rebuild();
        void refreshBadge();
        void applyTheme();
        /** Toggle, which is what both the app bar bell and the close button do. */
        void toggleOverlay();
        /**
         * Pull the sheet over to the window's top right corner and mask the rest
         * of the overlay away, so the panel is anchored and non-modal.
         */
        void anchorSheet();

        QWidget* m_sheet = nullptr;
        QLabel* m_headline = nullptr;
        ButtonBase* m_clearButton = nullptr;
        SearchBar* m_search = nullptr;
        QScrollArea* m_scroll = nullptr;
        QVBoxLayout* m_listLayout = nullptr;
        QWidget* m_emptyState = nullptr;
        QPointer<TopAppBar> m_appBar;
        QList<Notification> m_items;
        quint64 m_nextId = 1;
    };

} // namespace Material

#endif // KEEPASSXC_MATERIALNOTIFICATIONCENTRE_H
