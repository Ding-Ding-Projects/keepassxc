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

#ifndef KEEPASSXC_MATERIALBREAKPOINTS_H
#define KEEPASSXC_MATERIALBREAKPOINTS_H

#include <QSize>

/**
 * The five window-size classes the Material shell lays out for.
 *
 * These are logical pixels, so they are already divided by the device pixel
 * ratio: a 200% display at 2880 physical px is Large, not ExtraLarge. The
 * boundaries are Material 3's own window size classes, with Large split out
 * because that is where the group pane earns its width.
 *
 * The shell drops panes in a fixed order as the window narrows, and every
 * dropped pane's content stays reachable somewhere else:
 *
 *   ExtraLarge  >=1440  rail + group pane + list + detail, wide gutters
 *   Large       >=1200  rail + group pane + list + detail
 *   Expanded    >=840   rail + list + detail; groups move into the search scope
 *   Medium      >=600   rail (icons only) + list; detail opens as a full sheet
 *   Compact     <600    bottom bar + list; rail destinations 6-10 in More
 *
 * Nothing is ever merely clipped. If a surface cannot be shown at a size, the
 * shell must move it, not truncate it - see the no-clipping requirement.
 */
namespace Material
{
    enum class Breakpoint
    {
        Compact,
        Medium,
        Expanded,
        Large,
        ExtraLarge
    };

    /** The class @p logicalWidth falls into. Boundaries are inclusive lows. */
    Breakpoint breakpointFor(int logicalWidth);

    /** True when the rail is shown as a rail rather than as a bottom bar. */
    bool hasRail(Breakpoint bp);
    /** True when the group pane has room. */
    bool hasGroupPane(Breakpoint bp);
    /** True when the detail pane sits beside the list instead of over it. */
    bool hasInlineDetail(Breakpoint bp);
    /** Rail width in logical px: 88 normally, 72 without labels. */
    int railWidth(Breakpoint bp);
    /** Detail pane width in logical px, or 0 when it is not inline. */
    int detailWidth(Breakpoint bp);
} // namespace Material

#endif // KEEPASSXC_MATERIALBREAKPOINTS_H
