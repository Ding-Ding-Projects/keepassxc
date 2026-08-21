#include "SquirrelLifecycle.h"

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QProcess>
#include <QRegularExpression>

#include <cstdlib>

namespace
{
    constexpr int ProcessTimeoutMs = 30000;

    bool runUpdate(const QString& updateExe, const QStringList& arguments)
    {
        if (!QFileInfo::exists(updateExe)) {
            return false;
        }
        QProcess process;
        process.setProgram(updateExe);
        process.setArguments(arguments);
        process.start();
        if (!process.waitForStarted(ProcessTimeoutMs) || !process.waitForFinished(ProcessTimeoutMs)) {
            process.kill();
            process.waitForFinished(5000);
            return false;
        }
        return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
    }
} // namespace

namespace SquirrelLifecycle
{
    Event classify(const QStringList& arguments)
    {
        if (arguments.size() < 2) {
            return Event::None;
        }

        const QHash<QString, Event> lifecycleFlags = {
            {QStringLiteral("--squirrel-install"), Event::Install},
            {QStringLiteral("--squirrel-updated"), Event::Updated},
            {QStringLiteral("--squirrel-uninstall"), Event::Uninstall},
            {QStringLiteral("--squirrel-obsolete"), Event::Obsolete},
            {QStringLiteral("--squirrel-firstrun"), Event::FirstRun},
        };
        int lifecycleCount = 0;
        for (int index = 1; index < arguments.size(); ++index) {
            lifecycleCount += lifecycleFlags.contains(arguments.at(index).toLower()) ? 1 : 0;
        }
        if (lifecycleCount == 0) {
            return Event::None;
        }
        if (lifecycleCount != 1 || !lifecycleFlags.contains(arguments.at(1).toLower())) {
            return Event::Invalid;
        }

        const Event event = lifecycleFlags.value(arguments.at(1).toLower());
        const bool validArity = event == Event::FirstRun ? arguments.size() == 2 : arguments.size() == 3;
        if (!validArity) {
            return Event::Invalid;
        }
        if (event != Event::FirstRun) {
            static const QRegularExpression versionExpression(
                QStringLiteral("^\\d+\\.\\d+\\.\\d+(?:[-+][0-9A-Za-z.-]+)?$"));
            if (!versionExpression.match(arguments.at(2)).hasMatch()) {
                return Event::Invalid;
            }
        }
        return event;
    }

    QString updateExecutable(const QString& applicationDirectory)
    {
        const QString cleanDirectory = QDir::cleanPath(applicationDirectory);
        const int separator = cleanDirectory.lastIndexOf(QLatin1Char('/'));
        const int nativeSeparator = cleanDirectory.lastIndexOf(QLatin1Char('\\'));
        const int split = qMax(separator, nativeSeparator);
        if (split <= 0) {
            return {};
        }
        return QDir::cleanPath(cleanDirectory.left(split) + QStringLiteral("/Update.exe"));
    }

    std::optional<int> handle(const QStringList& arguments, const QString& applicationDirectory)
    {
        const Event event = classify(arguments);
        if (event == Event::Invalid) {
            return EXIT_FAILURE;
        }
        if (event == Event::None || event == Event::FirstRun) {
            return std::nullopt;
        }
        if (event == Event::Obsolete) {
            return EXIT_SUCCESS;
        }

        const QString updateExe = updateExecutable(applicationDirectory);
        const QString shortcutTarget = QStringLiteral("KeePassXC.exe");
        bool succeeded = false;
        if (event == Event::Install || event == Event::Updated) {
            succeeded = runUpdate(updateExe, {QStringLiteral("--createShortcut"), shortcutTarget});
        } else if (event == Event::Uninstall) {
            succeeded = runUpdate(updateExe, {QStringLiteral("--removeShortcut"), shortcutTarget});
        }
        return succeeded ? EXIT_SUCCESS : EXIT_FAILURE;
    }
} // namespace SquirrelLifecycle
