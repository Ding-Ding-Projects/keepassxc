#ifndef KEEPASSXC_SQUIRRELLIFECYCLE_H
#define KEEPASSXC_SQUIRRELLIFECYCLE_H

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <functional>
#include <optional>

namespace SquirrelLifecycle
{
    enum class Event
    {
        None,
        Invalid,
        Install,
        Updated,
        Uninstall,
        Obsolete,
        FirstRun
    };

    enum class ExitStatus
    {
        NotStarted,
        Normal,
        Crashed
    };

    struct Layout
    {
        QString applicationDirectory;
        QString applicationExecutable;
        QString packageRoot;
        QString updateExecutable;
        QString version;
    };

    enum class RegistrationDecision
    {
        Claim,
        Refresh,
        PreserveForeign
    };

    struct ProcessResult
    {
        bool started = false;
        bool startTimedOut = false;
        bool finishTimedOut = false;
        ExitStatus exitStatus = ExitStatus::NotStarted;
        int exitCode = -1;
        QByteArray standardOutput;
        QByteArray standardError;
        qint64 durationMs = 0;

        bool succeeded() const;
    };

    struct IntegrationResult
    {
        bool browser = true;
        bool fileAssociation = true;
        bool uri = true;

        bool succeeded() const;
    };

    using ProcessRunner =
        std::function<ProcessResult(const QString&, const QStringList&, const QString&, int)>;
    using IntegrationRunner = std::function<IntegrationResult(Event, const Layout&)>;

    Event classify(const QStringList& arguments);
    bool consume(QStringList& arguments);
    bool parseVersion(const QString& value);
    std::optional<Layout> validateLayout(const QString& applicationDirectory);
    QString openCommand(const Layout& layout);
    RegistrationDecision registrationDecision(bool hasExistingValues, bool ownershipMarker);
    ProcessResult runShortHelper(const QString& program,
                                 const QStringList& arguments,
                                 const QString& workingDirectory,
                                 int timeoutMs);
    IntegrationResult updateInstallOwnedRegistrations(Event event, const Layout& layout);
    void setProcessRunnerForTests(ProcessRunner runner);
    void resetProcessRunnerForTests();
    void setIntegrationRunnerForTests(IntegrationRunner runner);
    void resetIntegrationRunnerForTests();

    /**
     * Handle an installation lifecycle event before normal UI startup.
     * Returns an exit code when the process must terminate, or nullopt when
     * ordinary application startup should continue.
     */
    std::optional<int> handle(const QStringList& arguments, const QString& applicationDirectory);
} // namespace SquirrelLifecycle

#endif // KEEPASSXC_SQUIRRELLIFECYCLE_H
