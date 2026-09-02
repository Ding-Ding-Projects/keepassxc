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

#ifndef KEEPASSXC_MATERIALSEGMENTEDBUTTON_H
#define KEEPASSXC_MATERIALSEGMENTEDBUTTON_H

#include <QList>
#include <QRect>
#include <QString>
#include <QWidget>

class QKeyEvent;

namespace Material
{
    /**
     * A single-select segmented control: an outlined pill divided into equal
     * segments, the active one filled with secondaryContainer.
     *
     * Used for the theme and density choices in appearance, the language mode
     * and the sort order above the entry list. Selecting a segment from code
     * emits segmentSelected() only when the selection actually changes.
     */
    class SegmentedButton : public QWidget
    {
        Q_OBJECT

    public:
        explicit SegmentedButton(QWidget* parent = nullptr);
        ~SegmentedButton() override;

        /** Append a segment. The first segment added becomes the current one. */
        void addSegment(const QString& id, const QString& label, const QString& symbol = {});
        void clear();
        int count() const;
        /** Rename a segment in place, keeping its id and selection. */
        void setSegmentLabel(const QString& id, const QString& label);

        QString currentSegment() const;
        void setCurrentSegment(const QString& id);

        QSize sizeHint() const override;
        QSize minimumSizeHint() const override;

    signals:
        void segmentSelected(const QString& id);

    protected:
        void paintEvent(QPaintEvent* event) override;
        void mousePressEvent(QMouseEvent* event) override;
        void mouseMoveEvent(QMouseEvent* event) override;
        void leaveEvent(QEvent* event) override;
        void resizeEvent(QResizeEvent* event) override;
        void keyPressEvent(QKeyEvent* event) override;

    private:
        struct Segment
        {
            QString id;
            QString label;
            QString symbol;
            QRect rect;
        };

        int indexOf(const QString& id) const;
        int indexAt(const QPoint& pos) const;
        void relayout();
        /** Whether any segment carries a glyph, which decides if room is kept for one. */
        bool hasSymbols() const;

        QList<Segment> m_segments;
        int m_currentIndex = -1;
        int m_hoverIndex = -1;
    };

} // namespace Material

#endif // KEEPASSXC_MATERIALSEGMENTEDBUTTON_H
