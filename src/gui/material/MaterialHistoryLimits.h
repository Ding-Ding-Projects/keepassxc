/* Copyright (C) 2026 KeePassXC Team <team@keepassxc.org>
 * SPDX-License-Identifier: GPL-2.0-or-later OR GPL-3.0-only */

#ifndef KEEPASSXC_MATERIALHISTORYLIMITS_H
#define KEEPASSXC_MATERIALHISTORYLIMITS_H

#include <QString>
#include <QtGlobal>

namespace Material::HistoryLimits
{
    inline constexpr qint64 MaximumPackedBundleBytes = 256LL * 1024 * 1024;
    inline constexpr qint64 MaximumExpandedObjectBytes = 1024LL * 1024 * 1024;
    inline constexpr qint64 MaximumGitObjects = 1'000'000;
    inline constexpr qint64 MaximumHistoryAncestors = 100'000;
    inline constexpr int MaximumNestedTreeLevels = 256;
    inline constexpr qint64 MaximumFingerprintEntries = 1'000'000;

    constexpr bool withinPackedBundleLimit(qint64 bytes)
    {
        return bytes > 0 && bytes <= MaximumPackedBundleBytes;
    }

    constexpr bool withinExpandedObjectLimit(qint64 bytes)
    {
        return bytes >= 0 && bytes <= MaximumExpandedObjectBytes;
    }

    constexpr bool withinGitObjectLimit(qint64 objects)
    {
        return objects >= 0 && objects <= MaximumGitObjects;
    }

    constexpr bool withinHistoryAncestorLimit(qint64 ancestors)
    {
        return ancestors > 0 && ancestors <= MaximumHistoryAncestors;
    }

    inline int nestedTreeLevels(const QString& path)
    {
        return path.isEmpty() ? 0 : path.count(QLatin1Char('/')) + 1;
    }

    constexpr bool withinNestedTreeLevelLimit(int levels)
    {
        return levels >= 0 && levels <= MaximumNestedTreeLevels;
    }

    constexpr bool withinFingerprintEntryLimit(qint64 entries)
    {
        return entries >= 0 && entries <= MaximumFingerprintEntries;
    }
}

#endif // KEEPASSXC_MATERIALHISTORYLIMITS_H
