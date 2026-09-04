/*
 *  Copyright (C) 2026 KeePassXC Team <team@keepassxc.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 or (at your option)
 *  version 3 of the License.
 */

#include "MaterialBreakpoints.h"

namespace Material
{
    Breakpoint breakpointFor(int width)
    {
        if (width >= 1440) {
            return Breakpoint::ExtraLarge;
        }
        if (width >= 1200) {
            return Breakpoint::Large;
        }
        if (width >= 840) {
            return Breakpoint::Expanded;
        }
        if (width >= 600) {
            return Breakpoint::Medium;
        }
        return Breakpoint::Compact;
    }

    bool hasRail(Breakpoint breakpoint)
    {
        return breakpoint != Breakpoint::Compact;
    }

    bool hasGroupPane(Breakpoint breakpoint)
    {
        // The group pane joins the entry list and the inline detail as soon
        // as the window is Expanded: a window sized for the detail pane is
        // sized for the groups too, and every pane of the vault is on show.
        return breakpoint == Breakpoint::Expanded || breakpoint == Breakpoint::Large
               || breakpoint == Breakpoint::ExtraLarge;
    }

    bool hasInlineDetail(Breakpoint breakpoint)
    {
        return breakpoint == Breakpoint::Expanded || breakpoint == Breakpoint::Large
               || breakpoint == Breakpoint::ExtraLarge;
    }

    int railWidth(Breakpoint breakpoint)
    {
        if (breakpoint == Breakpoint::Compact) {
            return 0;
        }
        return breakpoint == Breakpoint::Medium ? 72 : 88;
    }

    int detailWidth(Breakpoint breakpoint)
    {
        switch (breakpoint) {
        case Breakpoint::Expanded:
            return 340;
        case Breakpoint::Large:
            return 360;
        case Breakpoint::ExtraLarge:
            return 392;
        default:
            return 0;
        }
    }
} // namespace Material
