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

#include "MaterialStyle.h"
#include "MaterialElevation.h"
#include "MaterialIcons.h"
#include "MaterialTheme.h"

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QAbstractScrollArea>
#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFrame>
#include <QListView>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QStyleFactory>
#include <QStyleOption>
#include <QTreeView>

namespace Material
{
    namespace
    {
        constexpr int IndicatorSize = 20; // check boxes, radio buttons
        constexpr int BranchIndicatorSize = 18;
        constexpr int ButtonHPadding = 24;
        constexpr int MenuItemHeight = 36;
        constexpr int MenuSeparatorHeight = 9;
        constexpr int MenuSeparatorMargin = 12;
        constexpr int InputHeight = 40;
        constexpr int RowInsetH = 2; // breathing room around the rounded row fill
        constexpr int RowInsetV = 1;

        constexpr qreal IndicatorStroke = 2.0;
        constexpr qreal FocusRingWidth = 2.0;
        constexpr qreal DisabledOpacity = 0.38;
        constexpr qreal HoverLayerAlpha = 0.08;

        // Remembered widget state, so unpolish() can put everything back.
        const char* const HoverProperty = "materialStyleHover";
        const char* const FrameShapeProperty = "materialStyleFrameShape";
        const char* const UniformProperty = "materialStyleUniform";
        const char* const ViewportFillProperty = "materialStyleViewportFill";
        const char* const FontProperty = "materialStyleFont";

        struct IconMapping
        {
            QStyle::StandardPixmap pixmap;
            const char* symbol;
            Role tint;
        };

        const IconMapping StandardIcons[] = {
            {QStyle::SP_DialogOkButton, "check", Role::Primary},
            {QStyle::SP_DialogCancelButton, "close", Role::OnSurfaceVariant},
            {QStyle::SP_DialogCloseButton, "close", Role::OnSurfaceVariant},
            {QStyle::SP_MessageBoxInformation, "info", Role::Primary},
            {QStyle::SP_MessageBoxWarning, "warning", Role::Amber},
            {QStyle::SP_MessageBoxCritical, "error", Role::Error},
            {QStyle::SP_MessageBoxQuestion, "help", Role::Primary},
            {QStyle::SP_ArrowBack, "arrow_back", Role::OnSurfaceVariant},
            {QStyle::SP_ArrowForward, "arrow_forward", Role::OnSurfaceVariant},
            {QStyle::SP_BrowserReload, "refresh", Role::OnSurfaceVariant},
            {QStyle::SP_TrashIcon, "delete", Role::Error},
            {QStyle::SP_DirIcon, "folder", Role::OnSurfaceVariant},
            {QStyle::SP_DirClosedIcon, "folder", Role::OnSurfaceVariant},
            {QStyle::SP_DirLinkIcon, "folder", Role::OnSurfaceVariant},
            {QStyle::SP_DirOpenIcon, "folder_open", Role::OnSurfaceVariant},
            {QStyle::SP_DirLinkOpenIcon, "folder_open", Role::OnSurfaceVariant},
            {QStyle::SP_DirHomeIcon, "home", Role::OnSurfaceVariant},
        };

        /** The square the check box or radio button glyph is drawn in. */
        QRect indicatorRect(const QRect& rect)
        {
            const int side = qMin(IndicatorSize, qMin(rect.width(), rect.height()));
            QRect box(0, 0, side, side);
            box.moveCenter(rect.center());
            return box;
        }

        /** The Material check mark, sized to @p box. */
        QPainterPath checkGlyph(const QRectF& box)
        {
            QPainterPath path;
            path.moveTo(box.left() + box.width() * 0.24, box.top() + box.height() * 0.52);
            path.lineTo(box.left() + box.width() * 0.42, box.top() + box.height() * 0.71);
            path.lineTo(box.left() + box.width() * 0.77, box.top() + box.height() * 0.31);
            return path;
        }

        /**
         * Check box and radio button drawing. Off is a two pixel outline, on is a
         * filled primary shape carrying an onPrimary glyph; the whole thing fades
         * to 38% when the control is disabled.
         */
        void paintIndicator(QPainter* painter, const QStyleOption* option, bool radio)
        {
            const QRect box = indicatorRect(option->rect);
            if (box.width() < 4) {
                return;
            }

            const bool on = option->state & QStyle::State_On;
            const bool partial = option->state & QStyle::State_NoChange;
            const qreal radius = radio ? Shape::Full : Shape::ExtraSmall;

            painter->save();
            painter->setRenderHint(QPainter::Antialiasing, true);
            if (!(option->state & QStyle::State_Enabled)) {
                painter->setOpacity(DisabledOpacity);
            }

            if (on || partial) {
                painter->fillPath(roundedPath(QRectF(box), radius), theme()->color(Role::Primary));

                const QColor glyph = theme()->color(Role::OnPrimary);
                if (radio) {
                    painter->setPen(Qt::NoPen);
                    painter->setBrush(glyph);
                    const qreal dot = box.width() * 0.25;
                    painter->drawEllipse(QRectF(box).center(), dot, dot);
                } else {
                    QPen pen(glyph, IndicatorStroke);
                    pen.setCapStyle(Qt::RoundCap);
                    pen.setJoinStyle(Qt::RoundJoin);
                    painter->setPen(pen);
                    painter->setBrush(Qt::NoBrush);
                    if (partial) {
                        const qreal y = QRectF(box).center().y();
                        painter->drawLine(QPointF(box.left() + box.width() * 0.26, y),
                                          QPointF(box.right() - box.width() * 0.26, y));
                    } else {
                        painter->drawPath(checkGlyph(QRectF(box)));
                    }
                }
            } else {
                // Inset by half the stroke so the outline stays inside the metric.
                const QRectF outline = QRectF(box).adjusted(IndicatorStroke / 2.0,
                                                            IndicatorStroke / 2.0,
                                                            -IndicatorStroke / 2.0,
                                                            -IndicatorStroke / 2.0);
                painter->setPen(QPen(theme()->color(Role::OnSurfaceVariant), IndicatorStroke));
                painter->setBrush(Qt::NoBrush);
                painter->drawPath(roundedPath(outline, radio ? Shape::Full : Shape::ExtraSmall - 1));
            }
            painter->restore();
        }

        void enableHover(QWidget* widget)
        {
            if (!widget || widget->property(HoverProperty).isValid()) {
                return;
            }
            widget->setProperty(HoverProperty, widget->testAttribute(Qt::WA_Hover));
            widget->setAttribute(Qt::WA_Hover, true);
        }

        void restoreHover(QWidget* widget)
        {
            if (!widget) {
                return;
            }
            const QVariant previous = widget->property(HoverProperty);
            if (!previous.isValid()) {
                return;
            }
            widget->setAttribute(Qt::WA_Hover, previous.toBool());
            widget->setProperty(HoverProperty, QVariant());
        }
    } // namespace

    // ---------------------------------------------------------------------- Style

    Style::Style()
        : QProxyStyle(QStyleFactory::create(QStringLiteral("Fusion")))
    {
        setObjectName(QStringLiteral("Material"));
    }

    Style::~Style() = default;

    // -------------------------------------------------------------------- polish

    void Style::polish(QApplication* app)
    {
        QProxyStyle::polish(app);
        if (!app) {
            return;
        }
        app->setPalette(theme()->palette());
        app->setStyleSheet(theme()->styleSheet());
    }

    void Style::polish(QWidget* widget)
    {
        QProxyStyle::polish(widget);
        if (!widget) {
            return;
        }

        if (qobject_cast<QAbstractButton*>(widget) || qobject_cast<QComboBox*>(widget)) {
            enableHover(widget);
        }

        if (auto* view = qobject_cast<QAbstractItemView*>(widget)) {
            // Hover events on the viewport are what drives the row state layer.
            enableHover(view->viewport());
            view->setProperty(FrameShapeProperty, static_cast<int>(view->frameShape()));
            view->setFrameShape(QFrame::NoFrame);

            if (auto* tree = qobject_cast<QTreeView*>(view)) {
                tree->setProperty(UniformProperty, tree->uniformRowHeights());
                tree->setUniformRowHeights(true);
            } else if (auto* list = qobject_cast<QListView*>(view)) {
                list->setProperty(UniformProperty, list->uniformItemSizes());
                list->setUniformItemSizes(true);
            }
        }

        if (auto* area = qobject_cast<QAbstractScrollArea*>(widget)) {
            // The surface behind the viewport is painted by the screen, not by Qt.
            if (auto* viewport = area->viewport()) {
                viewport->setProperty(ViewportFillProperty, viewport->autoFillBackground());
                viewport->setAutoFillBackground(false);
            }
        }

        if (qobject_cast<QMenu*>(widget)) {
            widget->setFont(theme()->font(TypeRole::BodyMedium));
            widget->setProperty(FontProperty, true);
        } else if (widget->inherits("QTipLabel")) {
            widget->setFont(theme()->font(TypeRole::BodySmall));
            widget->setProperty(FontProperty, true);
        }
    }

    void Style::unpolish(QWidget* widget)
    {
        if (!widget) {
            QProxyStyle::unpolish(widget);
            return;
        }

        restoreHover(widget);

        if (auto* view = qobject_cast<QAbstractItemView*>(widget)) {
            restoreHover(view->viewport());

            const QVariant shape = view->property(FrameShapeProperty);
            if (shape.isValid()) {
                view->setFrameShape(static_cast<QFrame::Shape>(shape.toInt()));
                view->setProperty(FrameShapeProperty, QVariant());
            }

            const QVariant uniform = view->property(UniformProperty);
            if (uniform.isValid()) {
                if (auto* tree = qobject_cast<QTreeView*>(view)) {
                    tree->setUniformRowHeights(uniform.toBool());
                } else if (auto* list = qobject_cast<QListView*>(view)) {
                    list->setUniformItemSizes(uniform.toBool());
                }
                view->setProperty(UniformProperty, QVariant());
            }
        }

        if (auto* area = qobject_cast<QAbstractScrollArea*>(widget)) {
            if (auto* viewport = area->viewport()) {
                const QVariant fill = viewport->property(ViewportFillProperty);
                if (fill.isValid()) {
                    viewport->setAutoFillBackground(fill.toBool());
                    viewport->setProperty(ViewportFillProperty, QVariant());
                }
            }
        }

        if (widget->property(FontProperty).toBool()) {
            // An unset font falls back to the parent / application font again.
            widget->setFont(QFont());
            widget->setProperty(FontProperty, QVariant());
        }

        QProxyStyle::unpolish(widget);
    }

    // --------------------------------------------------------------- metrics

    int Style::pixelMetric(PixelMetric metric, const QStyleOption* option, const QWidget* widget) const
    {
        switch (metric) {
        case PM_ButtonDefaultIndicator:
        case PM_ButtonShiftHorizontal:
        case PM_ButtonShiftVertical:
            return 0;
        case PM_DefaultFrameWidth:
            return 1;
        case PM_SmallIconSize:
        case PM_ListViewIconSize:
            return 20;
        case PM_LargeIconSize:
            return 24;
        case PM_ToolBarIconSize:
            return 22;
        case PM_IndicatorWidth:
        case PM_IndicatorHeight:
        case PM_ExclusiveIndicatorWidth:
        case PM_ExclusiveIndicatorHeight:
            return IndicatorSize;
        case PM_ScrollBarExtent:
            return 10;
        case PM_ScrollBarSliderMin:
            return 32;
        case PM_MenuButtonIndicator:
            return 20;
        case PM_FocusFrameHMargin:
        case PM_FocusFrameVMargin:
            return 2;
        case PM_TabBarTabHSpace:
            return 20;
        case PM_TabBarTabVSpace:
            return 12;
        case PM_SplitterWidth:
        case PM_DockWidgetSeparatorExtent:
            return 1;
        case PM_LayoutLeftMargin:
        case PM_LayoutRightMargin:
        case PM_LayoutTopMargin:
        case PM_LayoutBottomMargin:
            return 12;
        case PM_LayoutHorizontalSpacing:
        case PM_LayoutVerticalSpacing:
            return 8;
        default:
            break;
        }
        return QProxyStyle::pixelMetric(metric, option, widget);
    }

    int Style::styleHint(StyleHint hint,
                         const QStyleOption* option,
                         const QWidget* widget,
                         QStyleHintReturn* returnData) const
    {
        switch (hint) {
        case SH_Menu_Scrollable:
            return 1;
        case SH_ComboBox_Popup:
#ifdef Q_OS_WIN
            // The native popup ignores the stylesheet, so use the list view.
            return 0;
#else
            break;
#endif
        case SH_UnderlineShortcut:
            return 0;
        case SH_ItemView_ShowDecorationSelected:
            return 1;
        case SH_DialogButtonLayout:
            // Material puts the confirming action last.
            return QDialogButtonBox::GnomeLayout;
        case SH_ToolTipLabel_Opacity:
            return 255;
        case SH_Widget_Animation_Duration:
            return Duration::Medium;
        case SH_FocusFrame_AboveWidget:
            return 1;
        case SH_ScrollBar_Transient:
            return 0;
        default:
            break;
        }
        return QProxyStyle::styleHint(hint, option, widget, returnData);
    }

    QIcon Style::standardIcon(StandardPixmap standardIcon, const QStyleOption* option, const QWidget* widget) const
    {
        for (const auto& mapping : StandardIcons) {
            if (mapping.pixmap != standardIcon) {
                continue;
            }
            const QString name = QString::fromLatin1(mapping.symbol);
            if (Icons::hasSymbol(name)) {
                return Icons::symbol(name, mapping.tint);
            }
            break;
        }
        return QProxyStyle::standardIcon(standardIcon, option, widget);
    }

    // -------------------------------------------------------------- primitives

    void Style::drawPrimitive(PrimitiveElement element,
                              const QStyleOption* option,
                              QPainter* painter,
                              const QWidget* widget) const
    {
        if (!option || !painter) {
            return;
        }

        switch (element) {
        case PE_FrameFocusRect: {
            // A solid Material ring instead of the platform's dotted rectangle.
            const QRectF ring = QRectF(option->rect).adjusted(FocusRingWidth / 2.0,
                                                              FocusRingWidth / 2.0,
                                                              -FocusRingWidth / 2.0,
                                                              -FocusRingWidth / 2.0);
            if (ring.width() <= 0 || ring.height() <= 0) {
                return;
            }
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing, true);
            painter->setPen(QPen(theme()->color(Role::Primary), FocusRingWidth));
            painter->setBrush(Qt::NoBrush);
            painter->drawPath(roundedPath(ring, Shape::Small));
            painter->restore();
            return;
        }
        case PE_IndicatorCheckBox:
            paintIndicator(painter, option, false);
            return;
        case PE_IndicatorRadioButton:
            paintIndicator(painter, option, true);
            return;
        case PE_PanelItemViewRow:
            // The row fill belongs to the item panel, which draws it rounded.
            return;
        case PE_PanelItemViewItem: {
            const bool selected = option->state & State_Selected;
            const bool hovered = option->state & State_MouseOver;
            if (!selected && !hovered) {
                return;
            }
            const QRect row = option->rect.adjusted(RowInsetH, RowInsetV, -RowInsetH, -RowInsetV);
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing, true);
            if (selected) {
                paintSurface(painter, row, Shape::Row, theme()->color(Role::SecondaryContainer));
            }
            if (hovered) {
                paintStateLayer(painter, row, Shape::Row, theme()->color(Role::OnSurface), HoverLayerAlpha);
            }
            painter->restore();
            return;
        }
        case PE_FrameLineEdit:
        case PE_PanelLineEdit:
            // Inputs are drawn entirely by the stylesheet.
            return;
        case PE_IndicatorBranch: {
            if (!(option->state & State_Children)) {
                return;
            }
            const QString name = (option->state & State_Open) ? QStringLiteral("expand_more")
                                                              : QStringLiteral("chevron_right");
            const QPixmap glyph = Icons::pixmap(name, BranchIndicatorSize, theme()->color(Role::OnSurfaceVariant));
            if (glyph.isNull()) {
                return;
            }
            QRect target(0, 0, BranchIndicatorSize, BranchIndicatorSize);
            target.moveCenter(option->rect.center());
            painter->drawPixmap(target, glyph);
            return;
        }
        default:
            break;
        }

        QProxyStyle::drawPrimitive(element, option, painter, widget);
    }

    // ---------------------------------------------------------------- controls

    void Style::drawControl(ControlElement element,
                            const QStyleOption* option,
                            QPainter* painter,
                            const QWidget* widget) const
    {
        if (!option || !painter) {
            return;
        }

        switch (element) {
        case CE_ShapedFrame: {
            const auto* frame = qstyleoption_cast<const QStyleOptionFrame*>(option);
            if (!frame) {
                break;
            }
            const QRect rect = option->rect;
            const QColor line = theme()->color(Role::OutlineVariant);
            switch (frame->frameShape) {
            case QFrame::NoFrame:
                return;
            case QFrame::HLine:
                painter->fillRect(QRect(rect.left(), rect.center().y(), rect.width(), 1), line);
                return;
            case QFrame::VLine:
                painter->fillRect(QRect(rect.center().x(), rect.top(), 1, rect.height()), line);
                return;
            default:
                break;
            }
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing, true);
            paintSurface(painter, rect, Shape::Large, QColor(), line);
            painter->restore();
            return;
        }
        case CE_MenuItem: {
            const auto* item = qstyleoption_cast<const QStyleOptionMenuItem*>(option);
            if (item && item->menuItemType == QStyleOptionMenuItem::Separator) {
                const QRect rect = option->rect;
                const int width = rect.width() - 2 * MenuSeparatorMargin;
                if (width > 0) {
                    painter->fillRect(QRect(rect.left() + MenuSeparatorMargin, rect.center().y(), width, 1),
                                      theme()->color(Role::OutlineVariant));
                }
                return;
            }
            break;
        }
        case CE_ItemViewItem: {
            const auto* item = qstyleoption_cast<const QStyleOptionViewItem*>(option);
            if (!item) {
                break;
            }
            // Selected rows are filled with the secondary container, so the text
            // has to switch to its on-colour rather than the palette highlight.
            QStyleOptionViewItem copy(*item);
            copy.font = theme()->font(TypeRole::BodyMedium);
            copy.fontMetrics = QFontMetrics(copy.font);
            copy.palette.setColor(QPalette::Highlight, theme()->color(Role::SecondaryContainer));
            copy.palette.setColor(QPalette::HighlightedText, theme()->color(Role::OnSecondaryContainer));

            painter->save();
            painter->setFont(copy.font);
            QProxyStyle::drawControl(element, &copy, painter, widget);
            painter->restore();
            return;
        }
        default:
            break;
        }

        QProxyStyle::drawControl(element, option, painter, widget);
    }

    // ----------------------------------------------------------------- geometry

    QRect Style::subElementRect(SubElement element, const QStyleOption* option, const QWidget* widget) const
    {
        switch (element) {
        case SE_ItemViewItemFocusRect:
            // The rounded row fill already marks the current item.
            return QRect();
        case SE_PushButtonFocusRect:
        case SE_CheckBoxFocusRect:
        case SE_RadioButtonFocusRect:
            // Material wraps the whole component, label included.
            return option ? option->rect : QRect();
        default:
            break;
        }
        return QProxyStyle::subElementRect(element, option, widget);
    }

    QSize Style::sizeFromContents(ContentsType type,
                                  const QStyleOption* option,
                                  const QSize& contentsSize,
                                  const QWidget* widget) const
    {
        QSize size = QProxyStyle::sizeFromContents(type, option, contentsSize, widget);

        switch (type) {
        case CT_PushButton:
            size.setWidth(qMax(size.width(), contentsSize.width() + 2 * ButtonHPadding));
            size.setHeight(qMax(size.height(), Layout::ButtonHeight));
            break;
        case CT_MenuItem: {
            const auto* item = qstyleoption_cast<const QStyleOptionMenuItem*>(option);
            if (item && item->menuItemType == QStyleOptionMenuItem::Separator) {
                size.setHeight(MenuSeparatorHeight);
            } else {
                size.setHeight(qMax(size.height(), MenuItemHeight));
            }
            break;
        }
        case CT_ComboBox:
        case CT_LineEdit:
            size.setHeight(qMax(size.height(), InputHeight));
            break;
        case CT_ItemViewItem:
            size.setHeight(qMax(size.height(), theme()->rowHeight()));
            break;
        default:
            break;
        }
        return size;
    }

} // namespace Material
