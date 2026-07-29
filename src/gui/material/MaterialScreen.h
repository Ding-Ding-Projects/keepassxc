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

#ifndef KEEPASSXC_MATERIALSCREEN_H
#define KEEPASSXC_MATERIALSCREEN_H

#include <QString>
#include <QWidget>

class QHBoxLayout;
class QLabel;
class QScrollArea;
class QVBoxLayout;

namespace Material
{
    class SearchBar;

    /**
     * The shared frame of the five destinations.
     *
     * A 28px headline with an optional supporting line and trailing header
     * widgets, an optional Surface-variant search bar underneath, then the
     * content column inside a scroll area.
     *
     * Vault turns the header off and the scrolling with it, because its three
     * panes manage their own scrolling; the other four screens keep both.
     */
    class Screen : public QWidget
    {
        Q_OBJECT

    public:
        explicit Screen(QWidget* parent = nullptr);
        ~Screen() override;

        QString headline() const;
        void setHeadline(const QString& text);

        /** The blurb under the headline; empty hides it. */
        QString supportingText() const;
        void setSupportingText(const QString& text);

        bool isHeaderVisible() const;
        void setHeaderVisible(bool visible);

        /** The 44px search bar, hidden until setSearchVisible(true). */
        SearchBar* searchBar() const;
        bool isSearchVisible() const;
        void setSearchVisible(bool visible);

        /** Append a trailing widget to the headline row, e.g. an export pill. */
        void addHeaderWidget(QWidget* widget);

        /** The column callers fill with cards and lists. */
        QVBoxLayout* contentLayout() const;
        QScrollArea* scrollArea() const;

        /** Turn the content scrolling off for screens that scroll internally. */
        bool isScrollable() const;
        void setScrollable(bool scrollable);

    protected:
        void paintEvent(QPaintEvent* event) override;

    private:
        void applyTheme();

        QVBoxLayout* m_rootLayout = nullptr;
        QWidget* m_header = nullptr;
        QHBoxLayout* m_headerLayout = nullptr;
        QLabel* m_headlineLabel = nullptr;
        QLabel* m_supportingLabel = nullptr;
        SearchBar* m_searchBar = nullptr;
        QScrollArea* m_scrollArea = nullptr;
        QWidget* m_content = nullptr;
        QVBoxLayout* m_contentLayout = nullptr;
        bool m_scrollable = true;
    };

} // namespace Material

#endif // KEEPASSXC_MATERIALSCREEN_H
