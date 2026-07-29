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

#ifndef KEEPASSXC_MATERIALSTYLE_H
#define KEEPASSXC_MATERIALSTYLE_H

#include <QIcon>
#include <QProxyStyle>

class QApplication;
class QWidget;

namespace Material
{
    /**
     * The application style.
     *
     * Sits on top of Fusion, which gives predictable geometry on every
     * platform, and overrides the parts the Material design changes: focus
     * rings, item view selection, menu and scrollbar drawing, the metrics that
     * follow the density setting and the standard icons.
     *
     * Widgets that draw themselves (everything in namespace Material) bypass
     * the style entirely; this exists so that the stock Qt widgets KeePassXC
     * still uses - menus, dialogs, scroll areas, tooltips - match the design.
     */
    class Style : public QProxyStyle
    {
        Q_OBJECT

    public:
        Style();
        ~Style() override;

        using QProxyStyle::polish;
        using QProxyStyle::unpolish;

        void polish(QApplication* app) override;
        void polish(QWidget* widget) override;
        void unpolish(QWidget* widget) override;

        int pixelMetric(PixelMetric metric,
                        const QStyleOption* option = nullptr,
                        const QWidget* widget = nullptr) const override;

        int styleHint(StyleHint hint,
                      const QStyleOption* option = nullptr,
                      const QWidget* widget = nullptr,
                      QStyleHintReturn* returnData = nullptr) const override;

        QIcon standardIcon(StandardPixmap standardIcon,
                           const QStyleOption* option = nullptr,
                           const QWidget* widget = nullptr) const override;

        void drawPrimitive(PrimitiveElement element,
                           const QStyleOption* option,
                           QPainter* painter,
                           const QWidget* widget = nullptr) const override;

        void drawControl(ControlElement element,
                         const QStyleOption* option,
                         QPainter* painter,
                         const QWidget* widget = nullptr) const override;

        QRect
        subElementRect(SubElement element, const QStyleOption* option, const QWidget* widget = nullptr) const override;

        QSize sizeFromContents(ContentsType type,
                               const QStyleOption* option,
                               const QSize& contentsSize,
                               const QWidget* widget = nullptr) const override;
    };

} // namespace Material

#endif // KEEPASSXC_MATERIALSTYLE_H
