#include "SquirrelLifecycle.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>

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
        if (arguments.contains(QStringLiteral("--squirrel-install"), Qt::CaseInsensitive)) {
            return Event::Install;
        }
        if (arguments.contains(QStringLiteral("--squirrel-updated"), Qt::CaseInsensitive)) {
            return Event::Updated;
        }
        if (arguments.contains(QStringLiteral("--squirrel-uninstall"), Qt::CaseInsensitive)) {
            return Event::Uninstall;
        }
        if (arguments.contains(QStringLiteral("--squirrel-obsolete"), Qt::CaseInsensitive)) {
            return Event::Obsolete;
        }
        if (arguments.contains(QStringLiteral("--squirrel-firstrun"), Qt::CaseInsensitive)) {
            return Event::FirstRun;
        }
        return Event::None;
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
