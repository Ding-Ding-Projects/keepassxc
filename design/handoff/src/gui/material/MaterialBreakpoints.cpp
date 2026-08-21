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

#include "MaterialBreakpoints.h"

namespace Material
{
    Breakpoint breakpointFor(int w)
    {
        if (w >= 1440) return Breakpoint::ExtraLarge;
        if (w >= 1200) return Breakpoint::Large;
        if (w >= 840) return Breakpoint::Expanded;
        if (w >= 600) return Breakpoint::Medium;
        return Breakpoint::Compact;
    }

    bool hasRail(Breakpoint bp)
    {
        return bp != Breakpoint::Compact;
    }

    bool hasGroupPane(Breakpoint bp)
    {
        return bp == Breakpoint::Large || bp == Breakpoint::ExtraLarge;
    }

    bool hasInlineDetail(Breakpoint bp)
    {
        return bp != Breakpoint::Compact && bp != Breakpoint::Medium;
    }

    int railWidth(Breakpoint bp)
    {
        if (bp == Breakpoint::Compact) return 0;
        if (bp == Breakpoint::Medium) return 72; // no labels at this size
        return 88;
    }

    int detailWidth(Breakpoint bp)
    {
        switch (bp) {
        case Breakpoint::ExtraLarge: return 392;
        case Breakpoint::Large: return 360;
        case Breakpoint::Expanded: return 340;
        default: return 0;
        }
    }
} // namespace Material
