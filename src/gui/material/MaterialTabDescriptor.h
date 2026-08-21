#ifndef KEEPASSXC_MATERIALTABDESCRIPTOR_H
#define KEEPASSXC_MATERIALTABDESCRIPTOR_H

#include <QString>

namespace Material
{
    struct TabDescriptor
    {
        QString runtimeId;
        QString persistenceKey;
        QString symbol;
        QString label;
        bool pinned = false;
        bool persistable = false;
    };

    /** Return a stable, non-reversible identity for a Windows database path. */
    QString tabPersistenceKeyForPath(const QString& path);
} // namespace Material

#endif
