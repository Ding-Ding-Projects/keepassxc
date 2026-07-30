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

#ifndef KEEPASSXC_MATERIALSPECSHEET_H
#define KEEPASSXC_MATERIALSPECSHEET_H

#include "MaterialChip.h"

#include <QHash>
#include <QList>
#include <QScrollArea>
#include <QString>
#include <QWidget>

class QLabel;
class QStackedWidget;
class QVBoxLayout;

namespace Material
{
    class ButtonBase;
    class Card;
    class SearchBar;

    /**
     * One 56px spec sheet row: a leading glyph, the label over its sub line,
     * and a right-aligned control pill. The whole row is clickable.
     */
    class SpecSheetRow : public QWidget
    {
        Q_OBJECT

    public:
        static constexpr int RowHeight = 56;

        SpecSheetRow(const QString& key,
                     const QString& symbol,
                     const QString& label,
                     const QString& sub,
                     PillKind kind,
                     const QString& controlText,
                     QWidget* parent = nullptr);
        ~SpecSheetRow() override;

        /** Identifier of this row within its page, "<section>/<label>". */
        QString key() const;

        void setPill(PillKind kind, const QString& text);
        PillLabel* pill() const;

        QSize sizeHint() const override;
        QSize minimumSizeHint() const override;

    signals:
        void activated(const QString& key);

    protected:
        void paintEvent(QPaintEvent* event) override;
        void mouseReleaseEvent(QMouseEvent* event) override;
        void enterEvent(QEnterEvent* event) override;
        void leaveEvent(QEvent* event) override;

    private:
        QString m_key;
        QString m_symbol;
        QString m_label;
        QString m_sub;
        PillLabel* m_pill = nullptr;
        bool m_hovered = false;
    };

    /**
     * A scrolling column of rounded-28 outlined section cards.
     *
     * Sections are created on demand by addRow(): the first row naming a
     * section builds its card, later rows append to it. Row keys are
     * "<section>/<label>", which is what rowActivated() reports.
     */
    class SpecSheetPage : public QScrollArea
    {
        Q_OBJECT

    public:
        SpecSheetPage(const QString& id, const QString& title, QWidget* parent = nullptr);
        ~SpecSheetPage() override;

        QString id() const;
        QString title() const;

        void addRow(const QString& section,
                    const QString& symbol,
                    const QString& label,
                    const QString& sub,
                    PillKind kind,
                    const QString& controlText);

        /**
         * The design's line under the page title. Empty restores the default,
         * which explains what the search bar covers.
         */
        void setNote(const QString& note);

        /**
         * The design's line under a section title. May be set before or after
         * the section's first row creates its card.
         */
        void setSectionNote(const QString& section, const QString& note);

        /** The row with @p key, or nullptr. Use it to update a pill in place. */
        SpecSheetRow* row(const QString& key) const;

        /** Every row on this page, in the order they were added. */
        QList<SpecSheetRow*> rows() const;

        /** The card for @p title, or nullptr when no row has created it yet. */
        Card* sectionCard(const QString& title) const;

        /** The search bar this surface carries; never null. */
        SearchBar* searchBar() const;

        QString searchText() const;
        /** Drive the search field from outside, e.g. to mirror a sibling page. */
        void setSearchText(const QString& text);

    signals:
        void rowActivated(const QString& rowKey);
        void searchTextChanged(const QString& text);
        /** The search bar's builder button was pressed. */
        void builderRequested();

    private:
        Card* ensureSection(const QString& title);
        void applyFilter(const QString& text);

        QString m_id;
        QString m_title;
        QString m_note;
        QWidget* m_content = nullptr;
        QVBoxLayout* m_contentLayout = nullptr;
        SearchBar* m_search = nullptr;
        QLabel* m_noteLabel = nullptr;
        QHash<QString, QString> m_sectionNotes;
        QHash<QString, Card*> m_sections;
        QHash<QString, SpecSheetRow*> m_rows;
        QList<SpecSheetRow*> m_rowOrder;
    };

    /**
     * The spec sheet destination: a 266px sidebar of 44px pill rows on the
     * left, the active page filling the rest. Selecting a page in the sidebar
     * swaps the page area; the active pill is filled with primaryContainer.
     */
    class SpecSheet : public QWidget
    {
        Q_OBJECT

    public:
        explicit SpecSheet(QWidget* parent = nullptr);
        ~SpecSheet() override;

        /** Append a page. The first one added becomes current. */
        SpecSheetPage* addPage(const QString& id, const QString& symbol, const QString& title);
        SpecSheetPage* page(const QString& id) const;

        /**
         * Append a page backed by an arbitrary widget rather than by spec sheet
         * rows, so a hand-built surface can share the same 266px sidebar. The
         * widget is reparented into the page stack.
         */
        void addWidgetPage(const QString& id, const QString& symbol, const QString& title, QWidget* page);

        /** The widget behind @p id, whether a spec sheet page or a plain one. */
        QWidget* pageWidget(const QString& id) const;

        /** An overline that groups every sidebar row added after it. */
        void addSidebarSection(const QString& title);

        /** Add a row to @p pageId, which must already exist. */
        void addRow(const QString& pageId,
                    const QString& section,
                    const QString& symbol,
                    const QString& label,
                    const QString& sub,
                    PillKind kind,
                    const QString& controlText);

        /** How many pages the sidebar carries, of either kind. */
        int pageCount() const;

        QString currentPage() const;
        void setCurrentPage(const QString& id);

    signals:
        void rowActivated(const QString& pageId, const QString& rowKey);
        void currentPageChanged(const QString& pageId);
        /** A page's search bar asked for the regex builder. */
        void builderRequested(const QString& pageId);

    private:
        void applyTheme();
        /** Mirror @p text onto every other page and refresh the cross-page notices. */
        void syncSearch(const QString& sourceId, const QString& text);
        void registerPage(const QString& id, const QString& symbol, const QString& title, QWidget* widget);

        QWidget* m_sidebar = nullptr;
        QVBoxLayout* m_sidebarLayout = nullptr;
        QStackedWidget* m_stack = nullptr;
        QHash<QString, SpecSheetPage*> m_pages;
        QHash<QString, QWidget*> m_pageWidgets;
        QHash<QString, ButtonBase*> m_pageButtons;
        QList<QString> m_pageOrder;
        QList<QLabel*> m_sidebarSections;
        QString m_currentPage;
        /** Set while a search is being mirrored, to stop the pages echoing. */
        bool m_mirroring = false;
    };

} // namespace Material

#endif // KEEPASSXC_MATERIALSPECSHEET_H
