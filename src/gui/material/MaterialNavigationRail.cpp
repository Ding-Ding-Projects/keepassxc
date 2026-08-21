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

#include "MaterialNavigationRail.h"

#include "MaterialButtons.h"
#include "MaterialElevation.h"
#include "MaterialIcons.h"
#include "MaterialTheme.h"

#include <QEasingCurve>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QVariantAnimation>
#include <QWheelEvent>

namespace Material
{
    namespace
    {
        constexpr int TopMargin = 14;
        constexpr int BottomMargin = 12;
        constexpr int BrandSize = 56;
        constexpr int BrandGlyphSize = 30;
        // The 10px margin under the brand tile plus the rail's own 4px item gap.
        constexpr int BrandGap = 14;
        constexpr int TileSpacing = 4;
        constexpr int TilePadTop = 6;
        constexpr int TilePadBottom = 8;
        constexpr int TileGap = 3;
        constexpr int TileIconSize = 24;
        constexpr int TileTextMargin = 4;
        constexpr int BadgeHeight = 15;
        /** Inset of the hairline that says the tile run continues past the band. */
        constexpr int ScrollHintInset = 14;
        constexpr int FooterSize = 48;
        constexpr int FooterSpacing = 4;
        constexpr int FooterGlyphSize = 22;
        constexpr qreal SublabelOpacity = 0.72;

        QFont tileLabelFont()
        {
            QFont font = theme()->font(TypeRole::LabelMedium);
            font.setLetterSpacing(QFont::AbsoluteSpacing, 0.3);
            return font;
        }

        // The design's 10px sublabel falls between two type roles; scaling the
        // 11px one keeps it following the accessibility font size.
        QFont tileSublabelFont()
        {
            QFont font = theme()->font(TypeRole::LabelSmall);
            font.setPointSizeF(font.pointSizeF() * 10.0 / 11.0);
            font.setWeight(QFont::Normal);
            return font;
        }

        int tileHeight(bool withSublabel)
        {
            int height = TilePadTop + TileIconSize + TileGap + QFontMetrics(tileLabelFont()).height() + TilePadBottom;
            if (withSublabel) {
                height += TileGap + QFontMetrics(tileSublabelFont()).height();
            }
            return height;
        }

        QColor mix(const QColor& from, const QColor& to, qreal amount)
        {
            amount = qBound(0.0, amount, 1.0);
            return QColor::fromRgbF(from.redF() * (1.0 - amount) + to.redF() * amount,
                                    from.greenF() * (1.0 - amount) + to.greenF() * amount,
                                    from.blueF() * (1.0 - amount) + to.blueF() * amount);
        }

        QColor withAlpha(QColor color, qreal alpha)
        {
            color.setAlphaF(qBound(0.0, alpha, 1.0));
            return color;
        }

        /** A 0..1 ramp on the design's standard easing curve. */
        QVariantAnimation* createTransition(QObject* parent)
        {
            auto* animation = new QVariantAnimation(parent);
            animation->setDuration(Duration::Medium);
            animation->setStartValue(0.0);
            animation->setEndValue(1.0);
            QEasingCurve curve(QEasingCurve::BezierSpline);
            curve.addCubicBezierSegment(QPointF(0.2, 0.0), QPointF(0.0, 1.0), QPointF(1.0, 1.0));
            animation->setEasingCurve(curve);
            return animation;
        }
    } // namespace

    NavigationRail::NavigationRail(QWidget* parent)
        : QWidget(parent)
    {
        setFixedWidth(Layout::RailWidth);
        setMouseTracking(true);
        setFocusPolicy(Qt::StrongFocus);
        setAccessibleName(tr("Navigation"));

        // The footer buttons are rounded squares rather than pills, and their
        // size is pinned as well as requested: ButtonBase fixes a minimum width
        // from its label metrics on construction, which otherwise outvotes the
        // diameter and stretches them past the rail's centre column.
        m_themeButton = new IconButton(this);
        m_themeButton->setDiameter(FooterSize);
        m_themeButton->setFixedSize(FooterSize, FooterSize);
        m_themeButton->setRadius(Shape::Row);
        m_themeButton->setSymbolSize(FooterGlyphSize);
        m_themeButton->setToolTip(tr("Toggle light / dark"));
        connect(m_themeButton, &QAbstractButton::clicked, this, &NavigationRail::themeToggleRequested);

        m_lockButton = new IconButton(QStringLiteral("lock"), this);
        m_lockButton->setDiameter(FooterSize);
        m_lockButton->setFixedSize(FooterSize, FooterSize);
        m_lockButton->setRadius(Shape::Row);
        m_lockButton->setSymbolSize(FooterGlyphSize);
        m_lockButton->setToolTip(tr("Lock databases"));
        m_lockButton->setAccessibleName(m_lockButton->toolTip());
        connect(m_lockButton, &QAbstractButton::clicked, this, &NavigationRail::lockRequested);

        m_selectAnimation = createTransition(this);
        connect(m_selectAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
            m_selectProgress = value.toReal();
            update();
        });
        connect(m_selectAnimation, &QVariantAnimation::finished, this, [this] {
            m_previousIndex = -1;
            m_selectProgress = 1.0;
            update();
        });

        m_hoverAnimation = createTransition(this);
        connect(m_hoverAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
            m_hoverProgress = value.toReal();
            update();
        });
        connect(m_hoverAnimation, &QVariantAnimation::finished, this, [this] {
            m_previousHoverIndex = -1;
            m_hoverProgress = 1.0;
            update();
        });

        // The toggle offers the mode the user is not in, so its glyph flips with
        // the theme; the tile metrics follow the type scale and have to be redone.
        // The tooltip stays the design's static label - only the accessible name,
        // which has to announce the outcome, names the mode being switched to.
        auto syncTheme = [this] {
            const bool dark = theme()->isDark();
            m_themeButton->setSymbol(dark ? QStringLiteral("light_mode") : QStringLiteral("dark_mode"));
            m_themeButton->setAccessibleName(dark ? tr("Switch to the light theme") : tr("Switch to the dark theme"));
            relayout();
            updateGeometry();
            update();
        };
        syncTheme();
        connect(theme(), &Theme::changed, this, syncTheme);
    }

    NavigationRail::~NavigationRail() = default;

    void NavigationRail::addDestination(const QString& id,
                                        const QString& symbol,
                                        const QString& label,
                                        const QString& sublabel)
    {
        if (id.isEmpty() || indexOf(id) >= 0) {
            return;
        }

        Destination destination;
        destination.id = id;
        destination.symbol = symbol;
        destination.label = label;
        destination.sublabel = sublabel;
        m_destinations.append(destination);

        if (m_currentIndex < 0) {
            m_currentIndex = m_destinations.size() - 1;
            setAccessibleDescription(label);
        }

        relayout();
        updateGeometry();
        update();
    }

    void NavigationRail::setSublabel(const QString& id, const QString& sublabel)
    {
        const int index = indexOf(id);
        if (index < 0 || m_destinations.at(index).sublabel == sublabel) {
            return;
        }
        // Only the appearance or disappearance of a sublabel changes the metrics.
        const bool metricsChanged = m_destinations.at(index).sublabel.isEmpty() != sublabel.isEmpty();
        m_destinations[index].sublabel = sublabel;
        if (metricsChanged) {
            relayout();
            updateGeometry();
        }
        update();
    }

    void NavigationRail::setBadge(const QString& id, const QString& badge)
    {
        const int index = indexOf(id);
        if (index < 0 || m_destinations.at(index).badge == badge) {
            return;
        }
        m_destinations[index].badge = badge;
        update();
    }

    QString NavigationRail::currentDestination() const
    {
        if (m_currentIndex < 0 || m_currentIndex >= m_destinations.size()) {
            return {};
        }
        return m_destinations.at(m_currentIndex).id;
    }

    void NavigationRail::setCurrentDestination(const QString& id)
    {
        const int index = indexOf(id);
        if (index < 0 || index == m_currentIndex) {
            return;
        }

        m_previousIndex = m_currentIndex;
        m_currentIndex = index;
        setAccessibleDescription(m_destinations.at(index).label);

        m_selectAnimation->stop();
        if (m_previousIndex >= 0) {
            m_selectProgress = 0.0;
            m_selectAnimation->start();
        } else {
            m_selectProgress = 1.0;
        }
        // Selecting a destination from outside the rail - the command palette,
        // a shortcut - must bring its tile into view, or the rail would show a
        // selection that is not on screen.
        ensureVisible(index);
        update();
    }

    int NavigationRail::count() const
    {
        return m_destinations.size();
    }

    void NavigationRail::setIconsOnly(bool iconsOnly)
    {
        if (m_iconsOnly == iconsOnly) {
            return;
        }
        m_iconsOnly = iconsOnly;
        relayout();
        updateGeometry();
        update();
    }

    bool NavigationRail::iconsOnly() const
    {
        return m_iconsOnly;
    }

    QSize NavigationRail::sizeHint() const
    {
        bool withSublabel = false;
        for (int i = 0; i < m_destinations.size(); ++i) {
            withSublabel = withSublabel || !m_destinations.at(i).sublabel.isEmpty();
        }

        int height = TopMargin + BrandSize + BrandGap;
        if (!m_destinations.isEmpty()) {
            height += m_destinations.size() * tileHeight(withSublabel) + (m_destinations.size() - 1) * TileSpacing;
        }
        height += TileSpacing + 2 * FooterSize + FooterSpacing + BottomMargin;
        return {Layout::RailWidth, height};
    }

    QSize NavigationRail::minimumSizeHint() const
    {
        const int height = TopMargin + BrandSize + BrandGap + tileHeight(true) + TileSpacing + 2 * FooterSize
                           + FooterSpacing + BottomMargin;
        return {Layout::RailWidth, height};
    }

    int NavigationRail::indexOf(const QString& id) const
    {
        for (int i = 0; i < m_destinations.size(); ++i) {
            if (m_destinations.at(i).id == id) {
                return i;
            }
        }
        return -1;
    }

    int NavigationRail::indexAt(const QPoint& pos) const
    {
        // A scrolled-out tile keeps its geometry, so the viewport is what makes
        // it unclickable - otherwise a tile above the brand or under the footer
        // would still answer to the pointer.
        if (!tileViewport().contains(pos)) {
            return -1;
        }
        for (int i = 0; i < m_destinations.size(); ++i) {
            const QRect& rect = m_destinations.at(i).rect;
            if (!rect.isEmpty() && rect.contains(pos)) {
                return i;
            }
        }
        return -1;
    }

    QRect NavigationRail::tileViewport() const
    {
        const int top = TopMargin + BrandSize + BrandGap;
        // The footer is pinned to the bottom, so the band the tiles may use ends
        // one item gap above the theme toggle.
        const int bottom = height() - BottomMargin - 2 * FooterSize - FooterSpacing - TileSpacing;
        return {0, top, width(), qMax(0, bottom - top)};
    }

    int NavigationRail::tileRunHeight() const
    {
        if (m_destinations.isEmpty()) {
            return 0;
        }
        bool withSublabel = false;
        for (const auto& destination : m_destinations) {
            withSublabel = withSublabel || !destination.sublabel.isEmpty();
        }
        return m_destinations.size() * tileHeight(withSublabel) + (m_destinations.size() - 1) * TileSpacing;
    }

    int NavigationRail::maximumScroll() const
    {
        return qMax(0, tileRunHeight() - tileViewport().height());
    }

    bool NavigationRail::clampScroll()
    {
        const int clamped = qBound(0, m_scrollOffset, maximumScroll());
        if (clamped == m_scrollOffset) {
            return false;
        }
        m_scrollOffset = clamped;
        return true;
    }

    void NavigationRail::ensureVisible(int index)
    {
        if (index < 0 || index >= m_destinations.size() || maximumScroll() == 0) {
            return;
        }

        bool withSublabel = false;
        for (const auto& destination : m_destinations) {
            withSublabel = withSublabel || !destination.sublabel.isEmpty();
        }
        const int tileSize = tileHeight(withSublabel);
        // Where the tile sits in the unscrolled run, not on screen.
        const int top = index * (tileSize + TileSpacing);
        const int viewport = tileViewport().height();

        const int wanted = qBound(top + tileSize - viewport, m_scrollOffset, top);
        if (wanted == m_scrollOffset) {
            return;
        }
        m_scrollOffset = qBound(0, wanted, maximumScroll());
        relayout();
        update();
    }

    void NavigationRail::relayout()
    {
        bool withSublabel = false;
        for (int i = 0; i < m_destinations.size(); ++i) {
            withSublabel = withSublabel || !m_destinations.at(i).sublabel.isEmpty();
        }

        const int tileWidth = Layout::RailItemWidth;
        const int tileLeft = (width() - tileWidth) / 2;
        const int tileSize = tileHeight(withSublabel);

        const int footerLeft = (width() - FooterSize) / 2;
        const int lockTop = height() - BottomMargin - FooterSize;
        const int themeTop = lockTop - FooterSpacing - FooterSize;
        m_lockButton->setGeometry(footerLeft, lockTop, FooterSize, FooterSize);
        m_themeButton->setGeometry(footerLeft, themeTop, FooterSize, FooterSize);

        m_scrollOffset = qBound(0, m_scrollOffset, maximumScroll());

        // The design draws the rail at 920px, where ten destinations fit with
        // room to spare. The window's own minimum is 500px, where they do not,
        // so the run scrolls inside the band between the brand tile and the
        // footer. Tiles keep their true geometry - painting and hit-testing are
        // both clipped to the viewport - so a destination that is scrolled out
        // of sight is still reachable rather than silently gone.
        const QRect viewport = tileViewport();
        int y = viewport.top() - m_scrollOffset;
        for (auto& destination : m_destinations) {
            destination.rect = QRect(tileLeft, y, tileWidth, tileSize);
            y += tileSize + TileSpacing;
        }
    }

    void NavigationRail::wheelEvent(QWheelEvent* event)
    {
        if (maximumScroll() == 0) {
            QWidget::wheelEvent(event);
            return;
        }

        const QPoint pixels = event->pixelDelta();
        const int delta = pixels.isNull() ? event->angleDelta().y() / 2 : pixels.y();
        if (delta == 0) {
            QWidget::wheelEvent(event);
            return;
        }

        m_scrollOffset = qBound(0, m_scrollOffset - delta, maximumScroll());
        relayout();
        // The tile under the pointer has changed even though the pointer has not.
        setHovered(indexAt(event->position().toPoint()));
        update();
        event->accept();
    }

    void NavigationRail::activate(int index)
    {
        if (index < 0 || index >= m_destinations.size()) {
            return;
        }
        const QString id = m_destinations.at(index).id;
        setCurrentDestination(id);
        emit destinationActivated(id);
    }

    void NavigationRail::setHovered(int index)
    {
        if (index == m_hoverIndex) {
            return;
        }

        m_previousHoverIndex = m_hoverIndex;
        m_hoverIndex = index;
        m_hoverAnimation->stop();
        m_hoverProgress = 0.0;
        m_hoverAnimation->start();

        if (index >= 0) {
            const Destination& destination = m_destinations.at(index);
            setToolTip(destination.sublabel.isEmpty() ? destination.label
                                                      : tr("%1 - %2").arg(destination.label, destination.sublabel));
        } else {
            setToolTip({});
        }
        update();
    }

    void NavigationRail::paintEvent(QPaintEvent* event)
    {
        Q_UNUSED(event)

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.fillRect(rect(), theme()->color(Role::SurfaceContainerLow));
        painter.fillRect(QRect(width() - 1, 0, 1, height()), theme()->color(Role::OutlineVariant));

        const QRect brand((width() - BrandSize) / 2, TopMargin, BrandSize, BrandSize);
        paintSurface(&painter, brand, Shape::Rail, theme()->color(Role::PrimaryContainer));
        const QRect brandGlyph(brand.x() + (BrandSize - BrandGlyphSize) / 2,
                               brand.y() + (BrandSize - BrandGlyphSize) / 2,
                               BrandGlyphSize,
                               BrandGlyphSize);
        painter.drawPixmap(
            brandGlyph,
            Icons::pixmap(QStringLiteral("shield_lock"), BrandGlyphSize, theme()->color(Role::OnPrimaryContainer)));

        // Both cross-fades run against the index they came from, so a tile that is
        // leaving keeps its fill until the incoming one has taken it over.
        auto activeAmount = [this](int index) -> qreal {
            if (index == m_currentIndex) {
                return m_previousIndex >= 0 ? m_selectProgress : 1.0;
            }
            if (index == m_previousIndex) {
                return 1.0 - m_selectProgress;
            }
            return 0.0;
        };
        auto hoverAmount = [this](int index) -> qreal {
            if (index == m_hoverIndex) {
                return m_hoverProgress;
            }
            if (index == m_previousHoverIndex) {
                return 1.0 - m_hoverProgress;
            }
            return 0.0;
        };

        const QColor idle = theme()->color(Role::OnSurface);
        const QColor selected = theme()->color(Role::OnPrimaryContainer);
        const QFont labelFont = tileLabelFont();
        const QFont sublabelFont = tileSublabelFont();
        const QFontMetrics labelMetrics(labelFont);
        const QFontMetrics sublabelMetrics(sublabelFont);

        // Everything below is clipped to the band between the brand tile and the
        // footer, so a scrolled tile is cut off cleanly at the edge of the run
        // instead of painting over either of them.
        painter.save();
        painter.setClipRect(tileViewport());

        for (int i = 0; i < m_destinations.size(); ++i) {
            const Destination& destination = m_destinations.at(i);
            if (destination.rect.isEmpty() || !destination.rect.intersects(tileViewport())) {
                continue;
            }

            const qreal active = activeAmount(i);
            const qreal hovered = hoverAmount(i) * (1.0 - active);
            if (hovered > 0.0) {
                paintSurface(&painter,
                             destination.rect,
                             Shape::Row,
                             withAlpha(theme()->color(Role::SurfaceContainerHigh), hovered));
            }
            if (active > 0.0) {
                paintSurface(&painter,
                             destination.rect,
                             Shape::Row,
                             withAlpha(theme()->color(Role::PrimaryContainer), active));
            }
            if (hasFocus() && i == m_currentIndex) {
                painter.setPen(QPen(theme()->color(Role::Primary), 2));
                painter.setBrush(Qt::NoBrush);
                painter.drawRoundedRect(
                    QRectF(destination.rect).adjusted(1, 1, -1, -1), Shape::Row - 1, Shape::Row - 1);
            }

            const QColor content = mix(idle, selected, active);
            const QRect iconRect(destination.rect.x() + (destination.rect.width() - TileIconSize) / 2,
                                 m_iconsOnly ? destination.rect.center().y() - TileIconSize / 2
                                             : destination.rect.y() + TilePadTop,
                                 TileIconSize,
                                 TileIconSize);
            painter.drawPixmap(iconRect, Icons::pixmap(destination.symbol, TileIconSize, content));

            if (m_iconsOnly) {
                continue;
            }

            const int textLeft = destination.rect.x() + TileTextMargin;
            const int textWidth = destination.rect.width() - 2 * TileTextMargin;
            const QRect labelRect(textLeft, iconRect.bottom() + 1 + TileGap, textWidth, labelMetrics.height());
            painter.setFont(labelFont);
            painter.setPen(content);
            painter.drawText(labelRect,
                             Qt::AlignHCenter | Qt::AlignVCenter,
                             labelMetrics.elidedText(destination.label, Qt::ElideRight, textWidth));

            if (!destination.sublabel.isEmpty()) {
                const QRect sublabelRect(
                    textLeft, labelRect.bottom() + 1 + TileGap, textWidth, sublabelMetrics.height());
                painter.setFont(sublabelFont);
                painter.setPen(withAlpha(content, SublabelOpacity));
                painter.drawText(sublabelRect,
                                 Qt::AlignHCenter | Qt::AlignVCenter,
                                 sublabelMetrics.elidedText(destination.sublabel, Qt::ElideRight, textWidth));
            }

            if (!destination.badge.isEmpty()) {
                const QFontMetrics badgeMetrics(sublabelFont);
                const int badgeWidth = qMax(BadgeHeight, badgeMetrics.horizontalAdvance(destination.badge) + 8);
                QRect badgeRect(0, 0, badgeWidth, BadgeHeight);
                badgeRect.moveCenter(QPoint(iconRect.right() + 2, iconRect.top() + 2));
                paintSurface(&painter, badgeRect, Shape::Full, theme()->color(Role::Error));
                painter.setFont(sublabelFont);
                painter.setPen(theme()->color(Role::OnError));
                painter.drawText(badgeRect, Qt::AlignCenter, destination.badge);
            }
        }

        painter.restore();

        // Two hairlines say the run continues past the band, so a rail that has
        // scrolled does not look like a rail that has run out of destinations.
        const QRect viewport = tileViewport();
        painter.setPen(theme()->color(Role::OutlineVariant));
        if (m_scrollOffset > 0) {
            painter.drawLine(viewport.left() + ScrollHintInset,
                             viewport.top(),
                             viewport.right() - ScrollHintInset,
                             viewport.top());
        }
        if (m_scrollOffset < maximumScroll()) {
            painter.drawLine(viewport.left() + ScrollHintInset,
                             viewport.bottom(),
                             viewport.right() - ScrollHintInset,
                             viewport.bottom());
        }
    }

    void NavigationRail::resizeEvent(QResizeEvent* event)
    {
        QWidget::resizeEvent(event);
        relayout();
        // A taller rail may have made the tail reachable without scrolling.
        ensureVisible(m_currentIndex);
    }

    void NavigationRail::mousePressEvent(QMouseEvent* event)
    {
        if (event->button() != Qt::LeftButton) {
            QWidget::mousePressEvent(event);
            return;
        }

        const int index = indexAt(event->position().toPoint());
        if (index < 0) {
            QWidget::mousePressEvent(event);
            return;
        }
        activate(index);
        event->accept();
    }

    void NavigationRail::mouseMoveEvent(QMouseEvent* event)
    {
        setHovered(indexAt(event->position().toPoint()));
        QWidget::mouseMoveEvent(event);
    }

    void NavigationRail::leaveEvent(QEvent* event)
    {
        setHovered(-1);
        QWidget::leaveEvent(event);
    }

    void NavigationRail::keyPressEvent(QKeyEvent* event)
    {
        if (m_destinations.isEmpty()) {
            QWidget::keyPressEvent(event);
            return;
        }

        const int last = m_destinations.size() - 1;
        const int current = qMax(0, m_currentIndex);
        switch (event->key()) {
        case Qt::Key_Up:
            activate(current > 0 ? current - 1 : last);
            break;
        case Qt::Key_Down:
            activate(current < last ? current + 1 : 0);
            break;
        case Qt::Key_Home:
            activate(0);
            break;
        case Qt::Key_End:
            activate(last);
            break;
        case Qt::Key_Space:
        case Qt::Key_Return:
        case Qt::Key_Enter:
            activate(current);
            break;
        default:
            QWidget::keyPressEvent(event);
            return;
        }
        event->accept();
    }

} // namespace Material
