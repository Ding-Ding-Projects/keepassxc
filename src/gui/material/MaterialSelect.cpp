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

#include "MaterialSelect.h"

#include "MaterialElevation.h"
#include "MaterialIcons.h"
#include "MaterialSearchBar.h"
#include "MaterialTheme.h"

#include <QEvent>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPainter>
#include <QRegularExpression>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QWidgetAction>

namespace Material
{
    namespace
    {
        constexpr int FieldPaddingX = 14;
        constexpr int ArrowSize = 22;
        constexpr int ArrowColumn = 34;
        constexpr int MinimumWidth = 120;
        constexpr int PopupMinimumWidth = 240;
        constexpr int PopupMaximumHeight = 360;
        constexpr int PopupPadding = 8;
        constexpr int ListRowHeight = 40;
    } // namespace

    Select::Select(QWidget* parent)
        : QAbstractButton(parent)
    {
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::StrongFocus);
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        setAttribute(Qt::WA_Hover, true);
        m_placeholder = tr("Search");
        connect(this, &QAbstractButton::clicked, this, &Select::showPopup);
        connect(theme(), &Theme::changed, this, [this] { applyTheme(); });
        applyTheme();
    }

    Select::~Select() = default;

    // -------------------------------------------------------------- items

    void Select::addItem(const QString& text, const QVariant& data)
    {
        Item item;
        item.text = text;
        item.data = data;
        m_items.append(item);
        if (m_currentIndex < 0) {
            m_currentIndex = 0;
            emit currentIndexChanged(0);
            emit currentTextChanged(text);
        }
        if (m_list) {
            rebuildList();
        }
        updateGeometry();
        update();
    }

    void Select::setItemFont(int index, const QFont& font)
    {
        if (index < 0 || index >= m_items.size()) {
            return;
        }
        m_items[index].font = font;
        m_items[index].hasFont = true;
        if (m_list) {
            rebuildList();
        }
    }

    void Select::clear()
    {
        m_items.clear();
        const bool had = m_currentIndex >= 0;
        m_currentIndex = -1;
        if (m_list) {
            rebuildList();
        }
        if (had) {
            emit currentIndexChanged(-1);
            emit currentTextChanged(QString());
        }
        updateGeometry();
        update();
    }

    int Select::count() const
    {
        return m_items.size();
    }

    int Select::currentIndex() const
    {
        return m_currentIndex;
    }

    void Select::setCurrentIndex(int index)
    {
        if (index < -1 || index >= m_items.size() || index == m_currentIndex) {
            return;
        }
        m_currentIndex = index;
        setAccessibleDescription(currentText());
        update();
        emit currentIndexChanged(index);
        emit currentTextChanged(currentText());
    }

    QString Select::currentText() const
    {
        return itemText(m_currentIndex);
    }

    void Select::setCurrentText(const QString& text)
    {
        const int index = findText(text);
        if (index >= 0) {
            setCurrentIndex(index);
        }
    }

    QVariant Select::currentData() const
    {
        return itemData(m_currentIndex);
    }

    QString Select::itemText(int index) const
    {
        return index >= 0 && index < m_items.size() ? m_items.at(index).text : QString();
    }

    QVariant Select::itemData(int index) const
    {
        return index >= 0 && index < m_items.size() ? m_items.at(index).data : QVariant();
    }

    int Select::findData(const QVariant& data) const
    {
        for (int index = 0; index < m_items.size(); ++index) {
            if (m_items.at(index).data == data) {
                return index;
            }
        }
        return -1;
    }

    int Select::findText(const QString& text) const
    {
        for (int index = 0; index < m_items.size(); ++index) {
            if (m_items.at(index).text.compare(text, Qt::CaseInsensitive) == 0) {
                return index;
            }
        }
        return -1;
    }

    // -------------------------------------------------------------- search

    void Select::setSearchIdentity(const QString& id, const QString& label)
    {
        m_searchId = id;
        m_searchLabel = label;
        if (m_search) {
            m_search->setIdentity(id, label);
        }
    }

    void Select::setSearchPlaceholder(const QString& placeholder)
    {
        m_placeholder = placeholder;
        if (m_search) {
            m_search->setPlaceholder(placeholder);
        }
    }

    SearchBar* Select::searchBar() const
    {
        const_cast<Select*>(this)->buildPopup();
        return m_search;
    }

    QListWidget* Select::listWidget() const
    {
        const_cast<Select*>(this)->buildPopup();
        return m_list;
    }

    QMenu* Select::popup() const
    {
        const_cast<Select*>(this)->buildPopup();
        return m_popup;
    }

    bool Select::isPopupOpen() const
    {
        return m_popup && m_popup->isVisible();
    }

    // --------------------------------------------------------------- popup

    void Select::buildPopup()
    {
        if (m_popup) {
            return;
        }
        m_popup = new QMenu(this);
        m_popup->setObjectName(objectName().isEmpty() ? QStringLiteral("materialSelectPopup")
                                                      : objectName() + QStringLiteral("Popup"));
        m_popup->setAccessibleName(accessibleName().isEmpty() ? tr("Choices") : accessibleName());

        auto* container = new QWidget(m_popup);
        auto* layout = new QVBoxLayout(container);
        layout->setContentsMargins(PopupPadding, PopupPadding, PopupPadding, PopupPadding);
        layout->setSpacing(6);

        // The list's own search bar: plain text by default, its Regex chip and
        // builder button one press away, exactly like every other search.
        m_search = new SearchBar(SearchBar::Variant::Surface, container);
        m_search->setObjectName(objectName().isEmpty() ? QStringLiteral("materialSelectSearch")
                                                       : objectName() + QStringLiteral("Search"));
        m_search->setPlaceholder(m_placeholder);
        if (!m_searchId.isEmpty()) {
            m_search->setIdentity(m_searchId, m_searchLabel);
        }
        m_search->lineEdit()->setAccessibleName(accessibleName().isEmpty() ? tr("Filter choices")
                                                                            : tr("Filter %1").arg(accessibleName()));
        m_search->lineEdit()->installEventFilter(this);
        connect(m_search, &SearchBar::textChanged, this, [this] { applyFilter(); });
        connect(m_search, &SearchBar::regexToggled, this, [this] { applyFilter(); });
        connect(m_search, &SearchBar::returnPressed, this, [this] {
            if (m_list->currentRow() >= 0 && !m_list->isRowHidden(m_list->currentRow())) {
                chooseRow(m_list->currentRow());
            }
        });
        layout->addWidget(m_search);

        m_list = new QListWidget(container);
        m_list->setObjectName(objectName().isEmpty() ? QStringLiteral("materialSelectList")
                                                     : objectName() + QStringLiteral("List"));
        m_list->setAccessibleName(accessibleName().isEmpty() ? tr("Choices") : accessibleName());
        m_list->setFrameShape(QFrame::NoFrame);
        m_list->setSelectionMode(QAbstractItemView::SingleSelection);
        m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        m_list->setUniformItemSizes(false);
        m_list->setFocusPolicy(Qt::NoFocus);
        m_list->setMouseTracking(true);
        m_list->setStyleSheet(QStringLiteral("QListWidget { background: transparent; border: none; outline: none; }"
                                             "QListWidget::item { border-radius: 10px; padding: 0 12px; }"));
        connect(m_list, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) { chooseRow(m_list->row(item)); });
        connect(m_list, &QListWidget::itemEntered, this, [this](QListWidgetItem* item) { m_list->setCurrentItem(item); });
        layout->addWidget(m_list, 1);

        m_popupAction = new QWidgetAction(m_popup);
        m_popupAction->setDefaultWidget(container);
        m_popup->addAction(m_popupAction);
        connect(m_popup, &QMenu::aboutToShow, this, [this] {
            m_search->clear();
            m_search->setRegexEnabled(false);
            rebuildList();
            const int row = m_currentIndex;
            m_list->setCurrentRow(row);
            if (row >= 0) {
                m_list->scrollToItem(m_list->item(row), QAbstractItemView::PositionAtCenter);
            }
            m_search->lineEdit()->setFocus(Qt::PopupFocusReason);
            update();
        });
        connect(m_popup, &QMenu::aboutToHide, this, [this] {
            setFocus(Qt::PopupFocusReason);
            update();
        });
        rebuildList();
        applyTheme();
    }

    void Select::rebuildList()
    {
        if (!m_list) {
            return;
        }
        m_list->clear();
        const QFont base = theme()->font(TypeRole::BodyMedium);
        for (const Item& item : m_items) {
            auto* row = new QListWidgetItem(item.text, m_list);
            row->setFont(item.hasFont ? item.font : base);
            row->setSizeHint(QSize(0, ListRowHeight));
            row->setData(Qt::UserRole, item.data);
        }
        applyFilter();
    }

    void Select::applyFilter()
    {
        if (!m_list || !m_search) {
            return;
        }
        const QString needle = m_search->text().trimmed();
        QRegularExpression pattern;
        bool useRegex = false;
        if (m_search->isRegexEnabled() && !needle.isEmpty()) {
            pattern = QRegularExpression(needle, QRegularExpression::CaseInsensitiveOption);
            // An unparsable pattern changes nothing rather than emptying the list.
            if (!pattern.isValid()) {
                return;
            }
            useRegex = true;
        }
        int shown = 0;
        for (int row = 0; row < m_list->count(); ++row) {
            const QString text = m_list->item(row)->text();
            const bool visible = needle.isEmpty() || (useRegex ? pattern.match(text).hasMatch()
                                                                : text.contains(needle, Qt::CaseInsensitive));
            m_list->setRowHidden(row, !visible);
            shown += visible ? 1 : 0;
        }
        if (m_list->currentRow() < 0 || m_list->isRowHidden(m_list->currentRow())) {
            m_list->setCurrentRow(firstVisibleRow(0, 1));
        }
        const int rows = qBound(1, shown, PopupMaximumHeight / ListRowHeight);
        m_list->setFixedHeight(rows * ListRowHeight + 4);
        m_list->setAccessibleDescription(shown == 0 ? tr("No choices match") : tr("%n choice(s)", "", shown));
        if (m_popup && m_popup->isVisible()) {
            m_popup->adjustSize();
        }
    }

    int Select::firstVisibleRow(int from, int step) const
    {
        if (!m_list) {
            return -1;
        }
        for (int row = from; row >= 0 && row < m_list->count(); row += step) {
            if (!m_list->isRowHidden(row)) {
                return row;
            }
        }
        return -1;
    }

    void Select::moveListSelection(int delta)
    {
        if (!m_list || m_list->count() == 0) {
            return;
        }
        const int start = m_list->currentRow() < 0 ? (delta > 0 ? 0 : m_list->count() - 1) : m_list->currentRow() + delta;
        const int row = firstVisibleRow(qBound(0, start, m_list->count() - 1), delta > 0 ? 1 : -1);
        if (row >= 0) {
            m_list->setCurrentRow(row);
            m_list->scrollToItem(m_list->item(row));
        }
    }

    void Select::chooseRow(int row)
    {
        if (row < 0 || row >= m_items.size()) {
            return;
        }
        hidePopup();
        setCurrentIndex(row);
    }

    void Select::showPopup()
    {
        if (!isEnabled() || m_items.isEmpty()) {
            return;
        }
        buildPopup();
        const int width = qMax(qMax(PopupMinimumWidth, this->width()), m_popup->sizeHint().width());
        m_popupAction->defaultWidget()->setMinimumWidth(width - 2);
        m_popup->setFixedWidth(width);
        m_popup->popup(mapToGlobal(QPoint(0, height() + 4)));
    }

    void Select::hidePopup()
    {
        if (m_popup) {
            m_popup->hide();
        }
    }

    bool Select::eventFilter(QObject* watched, QEvent* event)
    {
        if (m_search && watched == m_search->lineEdit() && event->type() == QEvent::KeyPress) {
            auto* key = static_cast<QKeyEvent*>(event);
            switch (key->key()) {
            case Qt::Key_Down:
                moveListSelection(1);
                return true;
            case Qt::Key_Up:
                moveListSelection(-1);
                return true;
            case Qt::Key_PageDown:
                moveListSelection(PopupMaximumHeight / ListRowHeight);
                return true;
            case Qt::Key_PageUp:
                moveListSelection(-(PopupMaximumHeight / ListRowHeight));
                return true;
            case Qt::Key_Escape:
                // Escape clears the filter first; a second press closes.
                if (!m_search->text().isEmpty()) {
                    m_search->clear();
                    return true;
                }
                return false;
            default:
                break;
            }
        }
        return QAbstractButton::eventFilter(watched, event);
    }

    // ---------------------------------------------------------------- field

    QSize Select::sizeHint() const
    {
        const QFontMetrics metrics(theme()->font(TypeRole::BodyMedium));
        int text = 0;
        for (const Item& item : m_items) {
            text = qMax(text, QFontMetrics(item.hasFont ? item.font : theme()->font(TypeRole::BodyMedium)).horizontalAdvance(item.text));
        }
        text = qMax(text, metrics.horizontalAdvance(QStringLiteral("MMMMMMMM")));
        return QSize(qMax(MinimumWidth, FieldPaddingX + text + 8 + ArrowColumn), Layout::ButtonHeight);
    }

    QSize Select::minimumSizeHint() const
    {
        return QSize(MinimumWidth, Layout::ButtonHeight);
    }

    void Select::paintEvent(QPaintEvent*)
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        const bool active = hasFocus() || isPopupOpen();
        const bool enabled = isEnabled();
        QColor fill = theme()->color(Role::SurfaceContainerLowest);
        QColor border = theme()->color(active ? Role::Primary : (underMouse() ? Role::OnSurfaceVariant : Role::Outline));
        QColor content = theme()->color(Role::OnSurface);
        if (!enabled) {
            fill = theme()->color(Role::SurfaceContainer);
            border = theme()->color(Role::OutlineVariant);
            content = theme()->color(Role::OnSurfaceVariant);
        }
        const int borderWidth = active ? 2 : 1;
        QRectF frame = QRectF(rect()).adjusted(borderWidth / 2.0, borderWidth / 2.0, -borderWidth / 2.0, -borderWidth / 2.0);
        painter.setPen(QPen(border, borderWidth));
        painter.setBrush(fill);
        painter.drawRoundedRect(frame, Shape::Large, Shape::Large);

        const QRect arrow(width() - ArrowColumn + (ArrowColumn - ArrowSize) / 2, (height() - ArrowSize) / 2, ArrowSize, ArrowSize);
        Icons::symbol(isPopupOpen() ? QStringLiteral("expand_less") : QStringLiteral("expand_more"),
                      theme()->color(enabled ? Role::OnSurfaceVariant : Role::OutlineVariant))
            .paint(&painter, arrow);

        const Item* current = m_currentIndex >= 0 && m_currentIndex < m_items.size() ? &m_items.at(m_currentIndex) : nullptr;
        const QFont font = current && current->hasFont ? current->font : theme()->font(TypeRole::BodyMedium);
        painter.setFont(font);
        painter.setPen(content);
        const QRect textRect(FieldPaddingX, 0, width() - FieldPaddingX - ArrowColumn - 4, height());
        painter.drawText(textRect,
                         Qt::AlignVCenter | Qt::AlignLeft,
                         QFontMetrics(font).elidedText(current ? current->text : QString(), Qt::ElideRight, textRect.width()));
    }

    void Select::keyPressEvent(QKeyEvent* event)
    {
        switch (event->key()) {
        case Qt::Key_Space:
        case Qt::Key_Return:
        case Qt::Key_Enter:
        case Qt::Key_Down:
        case Qt::Key_F4:
            showPopup();
            return;
        default:
            break;
        }
        QAbstractButton::keyPressEvent(event);
    }

    void Select::wheelEvent(QWheelEvent* event)
    {
        // A wheel over a closed select changes nothing: scrolling the page must
        // not silently swap a setting under the pointer.
        event->ignore();
    }

    void Select::changeEvent(QEvent* event)
    {
        QAbstractButton::changeEvent(event);
        if (event->type() == QEvent::EnabledChange || event->type() == QEvent::FontChange) {
            update();
        }
    }

    void Select::applyTheme()
    {
        setFont(theme()->font(TypeRole::BodyMedium));
        if (m_list) {
            QPalette palette = m_list->palette();
            palette.setColor(QPalette::Text, theme()->color(Role::OnSurface));
            palette.setColor(QPalette::Highlight, theme()->color(Role::SecondaryContainer));
            palette.setColor(QPalette::HighlightedText, theme()->color(Role::OnSecondaryContainer));
            m_list->setPalette(palette);
        }
        updateGeometry();
        update();
    }

} // namespace Material
