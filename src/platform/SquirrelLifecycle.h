#ifndef KEEPASSXC_SQUIRRELLIFECYCLE_H
#define KEEPASSXC_SQUIRRELLIFECYCLE_H

#include <QString>
#include <QStringList>

#include <optional>

namespace SquirrelLifecycle
{
    enum class Event
    {
        None,
        Install,
        Updated,
        Uninstall,
        Obsolete,
        FirstRun
    };

    Event classify(const QStringList& arguments);
    QString updateExecutable(const QString& applicationDirectory);

    /**
     * Handle an installation lifecycle event before normal UI startup.
     * Returns an exit code when the process must terminate, or nullopt when
     * ordinary application startup should continue.
     */
    std::optional<int> handle(const QStringList& arguments, const QString& applicationDirectory);
} // namespace SquirrelLifecycle

#endif // KEEPASSXC_SQUIRRELLIFECYCLE_H
