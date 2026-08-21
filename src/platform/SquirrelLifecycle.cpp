#include "SquirrelLifecycle.h"

#include "config-keepassx.h"
#ifdef KPXC_FEATURE_BROWSER
#include "browser/NativeMessageInstaller.h"
#endif

#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QHash>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>

#include <cstdlib>

namespace
{
    constexpr int ProcessTimeoutMs = 30000;
    constexpr qsizetype MaxCapturedOutput = 64 * 1024;
    const QString OwnershipValue = QStringLiteral("KeePassXCMaterialOwned");
    const QString FileProgId = QStringLiteral("KeePassXC.Material.kdbx");
    const QString UriScheme = QStringLiteral("keepassxc");

    SquirrelLifecycle::ProcessRunner g_processRunner;
    SquirrelLifecycle::IntegrationRunner g_integrationRunner;

    void appendBounded(QByteArray& destination, const QByteArray& source)
    {
        const qsizetype remaining = MaxCapturedOutput - destination.size();
        if (remaining > 0) {
            destination.append(source.left(remaining));
        }
    }

    bool setOwnedRegistration(const QString& rootKey, const QString& command)
    {
        QSettings root(rootKey, QSettings::NativeFormat);
        const auto decision = SquirrelLifecycle::registrationDecision(!root.allKeys().isEmpty(),
                                                                      root.value(OwnershipValue).toBool());
        if (decision == SquirrelLifecycle::RegistrationDecision::PreserveForeign) {
            return false;
        }
        root.setValue(OwnershipValue, true);
        root.sync();
        if (root.status() != QSettings::NoError) {
            return false;
        }
        QSettings commandSettings(rootKey + QStringLiteral("\\shell\\open\\command"), QSettings::NativeFormat);
        commandSettings.setValue(QStringLiteral("Default"), command);
        commandSettings.sync();
        return commandSettings.status() == QSettings::NoError;
    }

    bool removeOwnedKey(const QString& key)
    {
        QSettings settings(key, QSettings::NativeFormat);
        if (!settings.value(OwnershipValue).toBool()) {
            return true;
        }
        settings.clear();
        settings.sync();
        return settings.status() == QSettings::NoError;
    }

    bool isOwnedKey(const QString& key)
    {
        QSettings settings(key, QSettings::NativeFormat);
        return settings.value(OwnershipValue).toBool();
    }
} // namespace

namespace SquirrelLifecycle
{
    bool ProcessResult::succeeded() const
    {
        return started && !startTimedOut && !finishTimedOut && exitStatus == ExitStatus::Normal && exitCode == 0;
    }

    bool IntegrationResult::succeeded() const
    {
        return browser && fileAssociation && uri;
    }

    RegistrationDecision registrationDecision(bool hasExistingValues, bool ownershipMarker)
    {
        if (ownershipMarker) {
            return RegistrationDecision::Refresh;
        }
        return hasExistingValues ? RegistrationDecision::PreserveForeign : RegistrationDecision::Claim;
    }

    bool parseVersion(const QString& value)
    {
        static const QRegularExpression expression(QStringLiteral(
            "^(0|[1-9]\\d*)\\.(0|[1-9]\\d*)\\.(0|[1-9]\\d*)"
            "(?:-(?:0|[1-9]\\d*|[A-Za-z-][0-9A-Za-z-]*)(?:\\.(?:0|[1-9]\\d*|[A-Za-z-][0-9A-Za-z-]*))*)?"
            "(?:\\+[0-9A-Za-z-]+(?:\\.[0-9A-Za-z-]+)*)?$"));
        return expression.match(value).hasMatch();
    }

    Event classify(const QStringList& arguments)
    {
        if (arguments.size() < 2) {
            return Event::None;
        }
        const QHash<QString, Event> flags = {
            {QStringLiteral("--squirrel-install"), Event::Install},
            {QStringLiteral("--squirrel-updated"), Event::Updated},
            {QStringLiteral("--squirrel-uninstall"), Event::Uninstall},
            {QStringLiteral("--squirrel-obsolete"), Event::Obsolete},
            {QStringLiteral("--squirrel-firstrun"), Event::FirstRun},
        };
        int count = 0;
        for (int index = 1; index < arguments.size(); ++index) {
            count += flags.contains(arguments.at(index)) ? 1 : 0;
        }
        if (count == 0) {
            return Event::None;
        }
        if (count != 1 || !flags.contains(arguments.at(1))) {
            return Event::Invalid;
        }
        const Event event = flags.value(arguments.at(1));
        if (event == Event::FirstRun) {
            return arguments.size() == 2 ? event : Event::Invalid;
        }
        return arguments.size() == 3 && parseVersion(arguments.at(2)) ? event : Event::Invalid;
    }

    bool consume(QStringList& arguments)
    {
        if (classify(arguments) != Event::FirstRun) {
            return false;
        }
        arguments.removeAt(1);
        return true;
    }

    std::optional<Layout> validateLayout(const QString& applicationDirectory)
    {
        const QFileInfo appInfo(QDir::cleanPath(applicationDirectory));
        if (!appInfo.exists() || !appInfo.isDir() || appInfo.isSymLink() || appInfo.canonicalFilePath().isEmpty()
            || QDir::cleanPath(appInfo.canonicalFilePath()) != QDir::cleanPath(appInfo.absoluteFilePath())
            || !appInfo.fileName().startsWith(QStringLiteral("app-"))) {
            return std::nullopt;
        }
        const QString version = appInfo.fileName().mid(4);
        if (!parseVersion(version)) {
            return std::nullopt;
        }
        const QDir root = appInfo.dir();
        if (root.dirName() != QStringLiteral("KeePassXC.Material")) {
            return std::nullopt;
        }
        const QFileInfo updater(root.filePath(QStringLiteral("Update.exe")));
        if (!updater.exists() || !updater.isFile() || updater.isSymLink() || updater.canonicalFilePath().isEmpty()
            || QDir::cleanPath(updater.canonicalFilePath()) != QDir::cleanPath(updater.absoluteFilePath())) {
            return std::nullopt;
        }
        const QFileInfo application(QDir(appInfo.absoluteFilePath()).filePath(QStringLiteral("KeePassXC.exe")));
        if (!application.exists() || !application.isFile() || application.isSymLink()
            || application.canonicalFilePath().isEmpty()
            || QDir::cleanPath(application.canonicalFilePath()) != QDir::cleanPath(application.absoluteFilePath())) {
            return std::nullopt;
        }
        return Layout{appInfo.absoluteFilePath(),
                      application.absoluteFilePath(),
                      root.absolutePath(),
                      updater.absoluteFilePath(),
                      version};
    }

    ProcessResult runShortHelper(const QString& program,
                                 const QStringList& arguments,
                                 const QString& workingDirectory,
                                 int timeoutMs)
    {
        QElapsedTimer timer;
        timer.start();
        ProcessResult result;
        QProcess process;
        process.setProgram(program);
        process.setArguments(arguments);
        process.setWorkingDirectory(workingDirectory);
        process.start();
        const int deadline = qMin(timeoutMs, ProcessTimeoutMs);
        if (!process.waitForStarted(deadline)) {
            result.startTimedOut = process.error() == QProcess::Timedout;
            appendBounded(result.standardError, process.errorString().toUtf8());
            result.durationMs = timer.elapsed();
            return result;
        }
        result.started = true;
        while (process.state() != QProcess::NotRunning && timer.elapsed() < deadline) {
            process.waitForReadyRead(qMax(1, qMin(50, deadline - int(timer.elapsed()))));
            appendBounded(result.standardOutput, process.readAllStandardOutput());
            appendBounded(result.standardError, process.readAllStandardError());
        }
        if (process.state() != QProcess::NotRunning) {
            result.finishTimedOut = true;
            // This helper is exclusively for bounded shortcut maintenance.
            // Update application uses a separate non-killing process path.
            process.kill();
            process.waitForFinished(1000);
        }
        appendBounded(result.standardOutput, process.readAllStandardOutput());
        appendBounded(result.standardError, process.readAllStandardError());
        result.exitStatus = process.exitStatus() == QProcess::NormalExit ? ExitStatus::Normal : ExitStatus::Crashed;
        result.exitCode = process.exitCode();
        result.durationMs = timer.elapsed();
        return result;
    }

    QString openCommand(const Layout& layout)
    {
        const QString executable = QDir::toNativeSeparators(layout.applicationExecutable);
        return QStringLiteral("\"") + executable + QStringLiteral("\" \"%1\"");
    }

    IntegrationResult updateInstallOwnedRegistrations(Event event, const Layout& layout)
    {
        IntegrationResult result;
#ifdef KPXC_FEATURE_BROWSER
        NativeMessageInstaller browserInstaller;
#endif
        if (event == Event::Install || event == Event::Updated) {
#ifdef KPXC_FEATURE_BROWSER
            result.browser = browserInstaller.refreshInstallOwnedRegistrations();
#endif
            const QString command = openCommand(layout);
            const QString progIdRoot =
                QStringLiteral("HKEY_CURRENT_USER\\Software\\Classes\\%1").arg(FileProgId);
            result.fileAssociation = setOwnedRegistration(progIdRoot, command);
            if (result.fileAssociation) {
                QSettings extension(
                    QStringLiteral("HKEY_CURRENT_USER\\Software\\Classes\\.kdbx\\OpenWithProgids"),
                    QSettings::NativeFormat);
                extension.setValue(FileProgId, QByteArray());
                extension.sync();
                result.fileAssociation = extension.status() == QSettings::NoError;
            }

            const QString uriRoot = QStringLiteral("HKEY_CURRENT_USER\\Software\\Classes\\%1").arg(UriScheme);
            result.uri = setOwnedRegistration(uriRoot, command);
            if (result.uri) {
                QSettings uri(uriRoot, QSettings::NativeFormat);
                uri.setValue(QStringLiteral("Default"), QStringLiteral("URL:KeePassXC Protocol"));
                uri.setValue(QStringLiteral("URL Protocol"), QString());
                uri.sync();
                result.uri = uri.status() == QSettings::NoError;
            }
        } else if (event == Event::Uninstall) {
#ifdef KPXC_FEATURE_BROWSER
            result.browser = browserInstaller.removeInstallOwnedRegistrations();
#endif
            const QString progIdRoot =
                QStringLiteral("HKEY_CURRENT_USER\\Software\\Classes\\%1").arg(FileProgId);
            const bool ownsProgId = isOwnedKey(progIdRoot);
            result.fileAssociation = removeOwnedKey(progIdRoot);
            if (ownsProgId) {
                QSettings extension(
                    QStringLiteral("HKEY_CURRENT_USER\\Software\\Classes\\.kdbx\\OpenWithProgids"),
                    QSettings::NativeFormat);
                extension.remove(FileProgId);
                extension.sync();
                result.fileAssociation = result.fileAssociation && extension.status() == QSettings::NoError;
            }
            result.uri = removeOwnedKey(QStringLiteral("HKEY_CURRENT_USER\\Software\\Classes\\%1").arg(UriScheme));
        }
        return result;
    }

    void setProcessRunnerForTests(ProcessRunner runner)
    {
        g_processRunner = std::move(runner);
    }

    void resetProcessRunnerForTests()
    {
        g_processRunner = {};
    }

    void setIntegrationRunnerForTests(IntegrationRunner runner)
    {
        g_integrationRunner = std::move(runner);
    }

    void resetIntegrationRunnerForTests()
    {
        g_integrationRunner = {};
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
        const auto layout = validateLayout(applicationDirectory);
        if (!layout) {
            return EXIT_FAILURE;
        }
        if (event == Event::Obsolete) {
            return arguments.at(2) == layout->version ? EXIT_SUCCESS : EXIT_FAILURE;
        }
        if (arguments.at(2) != layout->version) {
            return EXIT_FAILURE;
        }
        const QStringList helperArguments = event == Event::Uninstall
                                                ? QStringList{QStringLiteral("--removeShortcut"),
                                                              QStringLiteral("KeePassXC.exe")}
                                                : QStringList{QStringLiteral("--createShortcut"),
                                                              QStringLiteral("KeePassXC.exe")};
        const ProcessResult process = g_processRunner
                                          ? g_processRunner(layout->updateExecutable,
                                                            helperArguments,
                                                            layout->packageRoot,
                                                            ProcessTimeoutMs)
                                          : runShortHelper(layout->updateExecutable,
                                                           helperArguments,
                                                           layout->packageRoot,
                                                           ProcessTimeoutMs);
        const IntegrationResult integration = g_integrationRunner
                                                  ? g_integrationRunner(event, *layout)
                                                  : updateInstallOwnedRegistrations(event, *layout);
        return process.succeeded() && integration.succeeded() ? EXIT_SUCCESS : EXIT_FAILURE;
    }
} // namespace SquirrelLifecycle
