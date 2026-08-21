/*
 *  Copyright (C) 2026 KeePassXC Team <team@keepassxc.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 or (at your option)
 *  version 3 of the License.
 */

#ifndef KEEPASSXC_MATERIALBREAKPOINTS_H
#define KEEPASSXC_MATERIALBREAKPOINTS_H

#include <QMetaType>

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

    Breakpoint breakpointFor(int logicalWidth);
    bool hasRail(Breakpoint breakpoint);
    bool hasGroupPane(Breakpoint breakpoint);
    bool hasInlineDetail(Breakpoint breakpoint);
    int railWidth(Breakpoint breakpoint);
    int detailWidth(Breakpoint breakpoint);
} // namespace Material

Q_DECLARE_METATYPE(Material::Breakpoint)

#endif // KEEPASSXC_MATERIALBREAKPOINTS_H
