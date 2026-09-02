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

#ifndef KEEPASSXC_MATERIALVAULTSIDEBAR_H
#define KEEPASSXC_MATERIALVAULTSIDEBAR_H

#include <QLayout>
#include <QList>
#include <QStringList>
#include <QWidget>

class QAbstractButton;
class QAbstractItemModel;
class QLabel;
class QModelIndex;
class QTreeView;

namespace Material
{
    class Chip;
    class GroupDelegate;

    /**
     * Lays its items out in a row and wraps onto the next line when they no
     * longer fit. Written for the tag chips in the group pane, where the number
     * of chips and their widths are only known at runtime.
     */
    class FlowLayout : public QLayout
    {
    public:
        explicit FlowLayout(QWidget* parent = nullptr, int horizontalSpacing = 8, int verticalSpacing = 8);
        ~FlowLayout() override;

        void addItem(QLayoutItem* item) override;
        int count() const override;
        QLayoutItem* itemAt(int index) const override;
        QLayoutItem* takeAt(int index) override;

        Qt::Orientations expandingDirections() const override;
        bool hasHeightForWidth() const override;
        int heightForWidth(int width) const override;
        void setGeometry(const QRect& rect) override;
        QSize sizeHint() const override;
        QSize minimumSize() const override;

    private:
        /** Place the items inside @p rect when @p apply, and answer the height used. */
        int layoutItems(const QRect& rect, bool apply) const;

        QList<QLayoutItem*> m_items;
        int m_horizontalSpacing;
        int m_verticalSpacing;
    };

    /**
     * The vault's 250px left pane.
     *
     * An uppercase overline over the group tree, a second overline over a
     * wrapping flow of tag filter chips, then the "open in external editor" row
     * pinned to the bottom. The tree draws its own indentation through
     * GroupDelegate, so the view itself has no expand decoration.
     *
     * The pane owns no data: the group model is set from outside and the tags
     * are a plain string list, which keeps it usable before a database is open.
     */
    class SearchBar;

    class VaultSidebar : public QWidget
    {
        Q_OBJECT

    public:
        explicit VaultSidebar(QWidget* parent = nullptr);
        ~VaultSidebar() override;

        void setGroupModel(QAbstractItemModel* model);
        QAbstractItemModel* groupModel() const;

        /** The tree itself, for callers that need its selection model. */
        QTreeView* groupView() const;
        /** The filter field above the tree; plain text by default, regex on request. */
        SearchBar* groupFilter() const;
        /** Hide every group whose name (and descendants' names) miss @p query. */
        void filterGroups(const QString& query);

        QStringList tags() const;
        void setTags(const QStringList& tags);

        /** The checked tag chips, in the order setTags() supplied them. */
        QStringList selectedTags() const;
        void setSelectedTags(const QStringList& tags);

    signals:
        void groupSelected(const QModelIndex& index);
        void tagsChanged(const QStringList& tags);
        void externalEditorRequested();

    protected:
        void paintEvent(QPaintEvent* event) override;

    private:
        void applyTheme();
        void rebuildTagChips();
        void updateGroupViewHeight();
        QStringList checkedTags() const;
        int visibleRowCount(const QModelIndex& parent) const;

        QLabel* m_groupsOverline = nullptr;
        SearchBar* m_groupFilter = nullptr;
        QLabel* m_tagsOverline = nullptr;
        QTreeView* m_groupView = nullptr;
        GroupDelegate* m_groupDelegate = nullptr;
        QWidget* m_tagContainer = nullptr;
        FlowLayout* m_tagLayout = nullptr;
        QAbstractButton* m_editorRow = nullptr;
        QList<Chip*> m_tagChips;
        QStringList m_tags;
        QStringList m_selectedTags;
        bool m_updatingChips = false;
    };

} // namespace Material

#endif // KEEPASSXC_MATERIALVAULTSIDEBAR_H
