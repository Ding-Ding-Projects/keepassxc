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

#ifndef KEEPASSXC_MATERIALTOPAPPBAR_H
#define KEEPASSXC_MATERIALTOPAPPBAR_H

#include <QString>
#include <QWidget>

class QLabel;

namespace Material
{
    class IconButton;
    class TonalButton;

    /**
     * The 64px app bar above the tab strip.
     *
     * Title and subtitle on the left, then the filled Save pill and the four
     * round action buttons: command palette, generator, regex builder and
     * notifications. The notification count rides on the notifications button
     * as an error-coloured badge.
     */
    class TopAppBar : public QWidget
    {
        Q_OBJECT

    public:
        explicit TopAppBar(QWidget* parent = nullptr);
        ~TopAppBar() override;

        QString title() const;
        void setTitle(const QString& title);

        /** The 11px line under the title; empty hides it and centres the title. */
        QString subtitle() const;
        void setSubtitle(const QString& subtitle);

        bool isSaveEnabled() const;
        void setSaveEnabled(bool enabled);

        /** Zero clears the badge. */
        int notificationCount() const;
        void setNotificationCount(int count);

        QSize sizeHint() const override;
        QSize minimumSizeHint() const override;

    signals:
        void saveRequested();
        void paletteRequested();
        void generatorRequested();
        void regexRequested();
        void notificationsRequested();

    protected:
        void paintEvent(QPaintEvent* event) override;
        void resizeEvent(QResizeEvent* event) override;

    private:
        void applyTheme();
        /** Re-elide the title and subtitle against the width the layout gave them. */
        void updateLabels();

        QString m_title;
        QString m_subtitle;
        QLabel* m_titleLabel = nullptr;
        QLabel* m_subtitleLabel = nullptr;
        TonalButton* m_saveButton = nullptr;
        IconButton* m_paletteButton = nullptr;
        IconButton* m_generatorButton = nullptr;
        IconButton* m_regexButton = nullptr;
        IconButton* m_notificationsButton = nullptr;
    };

} // namespace Material

#endif // KEEPASSXC_MATERIALTOPAPPBAR_H
