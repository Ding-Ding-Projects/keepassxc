#ifndef KEEPASSXC_RUNTIMEPROFILE_H
#define KEEPASSXC_RUNTIMEPROFILE_H

#include <QStandardPaths>
#include <QStringList>

/** Explicit, process-local storage and IPC routing for synthetic verification runs. */
namespace RuntimeProfile
{
    bool initialize(const QStringList& arguments, QString* error = nullptr);
    bool isActive();
    QString root();
    QString location(QStandardPaths::StandardLocation type);
    QString ipcIdentifier(const QString& normalIdentifier);
    bool contains(const QString& path);
}

#endif
