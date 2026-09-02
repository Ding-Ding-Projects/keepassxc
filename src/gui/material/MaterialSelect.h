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

#ifndef KEEPASSXC_MATERIALSELECT_H
#define KEEPASSXC_MATERIALSELECT_H

#include <QAbstractButton>
#include <QFont>
#include <QList>
#include <QString>
#include <QVariant>

class QListWidget;
class QMenu;
class QWidgetAction;

namespace Material
{
    class SearchBar;

    /**
     * The Material select: an outlined field showing the chosen item that
     * opens a list box with its own search bar and anchored regex builder.
     *
     * Every dropdown in the application is one of these rather than a stock
     * combo box. The list filters as the user types (plain text by default,
     * regex on request through the search bar's own chip and builder), the
     * arrow keys move through what survives, Return chooses, Escape clears the
     * filter and then closes, and focus returns to the field either way.
     *
     * The API mirrors the slice of QComboBox the screens actually used, so a
     * call site swaps the type and keeps its code.
     */
    class Select : public QAbstractButton
    {
        Q_OBJECT

    public:
        explicit Select(QWidget* parent = nullptr);
        ~Select() override;

        void addItem(const QString& text, const QVariant& data = QVariant());
        /** Show the item in its own face, for a font family list. */
        void setItemFont(int index, const QFont& font);
        void clear();
        int count() const;

        int currentIndex() const;
        void setCurrentIndex(int index);
        QString currentText() const;
        void setCurrentText(const QString& text);
        QVariant currentData() const;
        QString itemText(int index) const;
        QVariant itemData(int index) const;
        int findData(const QVariant& data) const;
        int findText(const QString& text) const;

        /**
         * Name the list's search for the search registry and the command
         * palette. Unset, the search is a local filter only.
         */
        void setSearchIdentity(const QString& id, const QString& label);
        void setSearchPlaceholder(const QString& placeholder);
        SearchBar* searchBar() const;
        QListWidget* listWidget() const;
        QMenu* popup() const;
        bool isPopupOpen() const;

        QSize sizeHint() const override;
        QSize minimumSizeHint() const override;

    public slots:
        void showPopup();
        void hidePopup();

    signals:
        void currentIndexChanged(int index);
        void currentTextChanged(const QString& text);

    protected:
        void paintEvent(QPaintEvent* event) override;
        void keyPressEvent(QKeyEvent* event) override;
        void wheelEvent(QWheelEvent* event) override;
        bool eventFilter(QObject* watched, QEvent* event) override;
        void changeEvent(QEvent* event) override;

    private:
        struct Item
        {
            QString text;
            QVariant data;
            QFont font;
            bool hasFont = false;
        };

        void buildPopup();
        void rebuildList();
        void applyFilter();
        void chooseRow(int row);
        void moveListSelection(int delta);
        void applyTheme();
        int firstVisibleRow(int from, int step) const;

        QList<Item> m_items;
        int m_currentIndex = -1;
        QMenu* m_popup = nullptr;
        QWidgetAction* m_popupAction = nullptr;
        SearchBar* m_search = nullptr;
        QListWidget* m_list = nullptr;
        QString m_searchId;
        QString m_searchLabel;
        QString m_placeholder;
    };

} // namespace Material

#endif // KEEPASSXC_MATERIALSELECT_H
