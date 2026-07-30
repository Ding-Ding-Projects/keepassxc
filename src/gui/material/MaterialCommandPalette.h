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

#ifndef KEEPASSXC_MATERIALCOMMANDPALETTE_H
#define KEEPASSXC_MATERIALCOMMANDPALETTE_H

#include "MaterialOverlay.h"

#include <QList>
#include <QPointer>
#include <QString>

class QAction;
class QLabel;
class QScrollArea;
class QVBoxLayout;
class QWidget;

namespace Material
{
    class SearchBar;

    /**
     * Every command in the application, in one searchable list.
     *
     * The Material shell hides the menu bar and the tool bar, so this is what
     * keeps their commands reachable. It walks the whole QAction tree of its
     * source window - not a curated list - and shows each action with its icon,
     * the menu it lives in, and its shortcut. Typing filters on all three;
     * Up and Down move the highlight, Enter runs it, Escape closes.
     *
     * Actions that are separators, that carry no text, or that only exist to
     * open a submenu are left out, because none of them do anything when
     * triggered. Disabled actions are listed but dimmed: knowing a command
     * exists and is unavailable right now is worth more than hiding it.
     */
    class CommandPalette : public Overlay
    {
        Q_OBJECT

    public:
        /** Results kept after filtering, so a long list stays responsive. */
        static constexpr int MaxResults = 60;

        explicit CommandPalette(QWidget* parent = nullptr);
        ~CommandPalette() override;

        /**
         * The widget whose QAction tree is listed. Defaults to the window the
         * palette was parented to.
         */
        QWidget* actionSource() const;
        void setActionSource(QWidget* source);

        /** Number of commands collected at the last open. */
        int commandCount() const;

    signals:
        /** @p action was chosen from the list; it has already been triggered. */
        void commandTriggered(QAction* action);

    protected:
        /** The action tree changes with the database state, so it is re-walked. */
        void aboutToOpen() override;

        bool eventFilter(QObject* watched, QEvent* event) override;

    private:
        /** One collected action and the text the search matches it by. */
        struct Command
        {
            QPointer<QAction> action;
            QString text;
            QString path;
            QString shortcut;
            QString haystack;
        };

        void buildSheet();
        void collect();
        void applyFilter(const QString& query);
        void clearRows();
        void moveSelection(int delta);
        void setSelection(int index);
        void runSelected();
        void applyTheme();

        QWidget* m_sheet = nullptr;
        QLabel* m_headline = nullptr;
        QLabel* m_countLabel = nullptr;
        SearchBar* m_search = nullptr;
        QScrollArea* m_scroll = nullptr;
        QVBoxLayout* m_listLayout = nullptr;
        QWidget* m_emptyState = nullptr;
        QPointer<QWidget> m_source;
        QList<Command> m_commands;
        QList<QWidget*> m_rows;
        QList<int> m_visible;
        int m_selected = -1;
    };

} // namespace Material

#endif // KEEPASSXC_MATERIALCOMMANDPALETTE_H
