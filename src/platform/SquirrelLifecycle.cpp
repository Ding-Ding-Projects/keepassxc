#include "SquirrelLifecycle.h"

#include "config-keepassx.h"
#ifdef KPXC_FEATURE_BROWSER
#include "browser/NativeMessageInstaller.h"
#endif

#include <QDir>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QStandardPaths>
#include <QHash>
#include <QRegularExpression>
#include <QSettings>

#include <algorithm>
#include <cstdlib>
#include <string>

#ifdef Q_OS_WIN
#include <objbase.h>
#include <shobjidl_core.h>
#endif

namespace
{
    const QString OwnershipValue = QStringLiteral("KeePassXCMaterialOwned");
    const QString FileProgId = QStringLiteral("KeePassXC.Material.kdbx");
    const QString UriScheme = QStringLiteral("keepassxc");
    const QString ShortcutOwnershipRoot =
        QStringLiteral("HKEY_CURRENT_USER\\Software\\KeePassXC.Material\\ShellIntegration\\Shortcuts");
    const QString ShortcutOwnershipValue = QStringLiteral("OwnedLinksV1");
    const QString ShortcutFileName = QStringLiteral("KeePassXC.lnk");
    const QString ShortcutStartMenuFolder = QStringLiteral("KeePassXC Team");
    const QString ShortcutArguments = QStringLiteral("--processStart KeePassXC.exe");

    SquirrelLifecycle::IntegrationRunner g_integrationRunner;
    SquirrelLifecycle::ShortcutRunner g_shortcutRunner;

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

#ifdef Q_OS_WIN
    class ComApartment final
    {
    public:
        ComApartment()
        {
            const HRESULT result = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
            m_initialized = SUCCEEDED(result);
            m_available = m_initialized || result == RPC_E_CHANGED_MODE;
        }

        ~ComApartment()
        {
            if (m_initialized) {
                ::CoUninitialize();
            }
        }

        bool available() const
        {
            return m_available;
        }

    private:
        bool m_initialized = false;
        bool m_available = false;
    };

    QString absolutePath(const QString& path)
    {
        return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    }

    QByteArray fileSha256(const QString& path)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            return {};
        }
        return QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256);
    }

    bool readShortcut(const QString& path, SquirrelLifecycle::ShortcutOwnership& shortcut)
    {
        const QFileInfo info(path);
        if (!info.exists() || !info.isFile() || info.isSymLink()) {
            return false;
        }

        IShellLinkW* shellLink = nullptr;
        if (FAILED(::CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&shellLink)))) {
            return false;
        }
        IPersistFile* persistFile = nullptr;
        const HRESULT query = shellLink->QueryInterface(IID_PPV_ARGS(&persistFile));
        if (FAILED(query)) {
            shellLink->Release();
            return false;
        }

        const std::wstring nativePath = path.toStdWString();
        bool success = SUCCEEDED(persistFile->Load(nativePath.c_str(), STGM_READ));
        std::wstring target(32768, L'\0');
        std::wstring arguments(32768, L'\0');
        if (success) {
            success = SUCCEEDED(shellLink->GetPath(target.data(), static_cast<int>(target.size()), nullptr, SLGP_RAWPATH))
                      && SUCCEEDED(shellLink->GetArguments(arguments.data(), static_cast<int>(arguments.size())));
        }
        persistFile->Release();
        shellLink->Release();
        if (!success) {
            return false;
        }

        shortcut.path = absolutePath(path);
        shortcut.target = absolutePath(QString::fromWCharArray(target.c_str()));
        shortcut.arguments = QString::fromWCharArray(arguments.c_str());
        shortcut.sha256 = fileSha256(path);
        return shortcut.sha256.size() == QCryptographicHash::hashLength(QCryptographicHash::Sha256);
    }

    bool writeShortcut(const QString& path, const SquirrelLifecycle::Layout& layout)
    {
        if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
            return false;
        }

        IShellLinkW* shellLink = nullptr;
        if (FAILED(::CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&shellLink)))) {
            return false;
        }
        const std::wstring target = layout.updateExecutable.toStdWString();
        const std::wstring arguments = ShortcutArguments.toStdWString();
        const std::wstring workingDirectory = layout.packageRoot.toStdWString();
        const std::wstring description = QStringLiteral("KeePassXC").toStdWString();
        const std::wstring icon = layout.updateExecutable.toStdWString();
        bool success = SUCCEEDED(shellLink->SetPath(target.c_str()))
                       && SUCCEEDED(shellLink->SetArguments(arguments.c_str()))
                       && SUCCEEDED(shellLink->SetWorkingDirectory(workingDirectory.c_str()))
                       && SUCCEEDED(shellLink->SetDescription(description.c_str()))
                       && SUCCEEDED(shellLink->SetIconLocation(icon.c_str(), 0));

        IPersistFile* persistFile = nullptr;
        if (success) {
            success = SUCCEEDED(shellLink->QueryInterface(IID_PPV_ARGS(&persistFile)));
        }
        if (success) {
            const std::wstring nativePath = path.toStdWString();
            success = SUCCEEDED(persistFile->Save(nativePath.c_str(), TRUE));
        }
        if (persistFile) {
            persistFile->Release();
        }
        shellLink->Release();
        return success;
    }

    bool loadShortcutOwnership(QList<SquirrelLifecycle::ShortcutOwnership>& shortcuts)
    {
        QSettings settings(ShortcutOwnershipRoot, QSettings::NativeFormat);
        if (!settings.contains(ShortcutOwnershipValue)) {
            return true;
        }
        const QByteArray payload = settings.value(ShortcutOwnershipValue).toByteArray();
        QJsonParseError parseError{};
        const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()
            || document.object().value(QStringLiteral("schemaVersion")).toInt() != 1
            || !document.object().value(QStringLiteral("shortcuts")).isArray()) {
            return false;
        }
        const QJsonArray entries = document.object().value(QStringLiteral("shortcuts")).toArray();
        if (entries.size() > 2) {
            return false;
        }
        for (const QJsonValue& value : entries) {
            if (!value.isObject()) {
                return false;
            }
            const QJsonObject object = value.toObject();
            SquirrelLifecycle::ShortcutOwnership shortcut;
            shortcut.path = object.value(QStringLiteral("path")).toString();
            shortcut.target = object.value(QStringLiteral("target")).toString();
            shortcut.arguments = object.value(QStringLiteral("arguments")).toString();
            shortcut.sha256 = QByteArray::fromHex(object.value(QStringLiteral("sha256")).toString().toLatin1());
            if (shortcut.path.isEmpty() || shortcut.target.isEmpty()
                || shortcut.sha256.size() != QCryptographicHash::hashLength(QCryptographicHash::Sha256)) {
                return false;
            }
            shortcuts.push_back(shortcut);
        }
        return true;
    }

    bool saveShortcutOwnership(const QList<SquirrelLifecycle::ShortcutOwnership>& shortcuts)
    {
        QJsonArray entries;
        for (const auto& shortcut : shortcuts) {
            QJsonObject object;
            object.insert(QStringLiteral("path"), shortcut.path);
            object.insert(QStringLiteral("target"), shortcut.target);
            object.insert(QStringLiteral("arguments"), shortcut.arguments);
            object.insert(QStringLiteral("sha256"), QString::fromLatin1(shortcut.sha256.toHex()));
            entries.append(object);
        }
        QJsonObject document;
        document.insert(QStringLiteral("schemaVersion"), 1);
        document.insert(QStringLiteral("shortcuts"), entries);
        QSettings settings(ShortcutOwnershipRoot, QSettings::NativeFormat);
        settings.setValue(ShortcutOwnershipValue, QJsonDocument(document).toJson(QJsonDocument::Compact));
        settings.sync();
        return settings.status() == QSettings::NoError;
    }

    QStringList shortcutPaths()
    {
        const QString desktop = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
        const QString startMenu = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
        if (desktop.isEmpty() || startMenu.isEmpty()) {
            return {};
        }
        return {QDir(desktop).filePath(ShortcutFileName),
                QDir(startMenu).filePath(ShortcutStartMenuFolder + QLatin1Char('/') + ShortcutFileName)};
    }

    bool saveOrReplaceShortcutRecord(QList<SquirrelLifecycle::ShortcutOwnership>& shortcuts,
                                     const SquirrelLifecycle::ShortcutOwnership& shortcut)
    {
        for (auto it = shortcuts.begin(); it != shortcuts.end(); ++it) {
            if (it->path.compare(shortcut.path, Qt::CaseInsensitive) == 0) {
                *it = shortcut;
                return saveShortcutOwnership(shortcuts);
            }
        }
        if (shortcuts.size() >= 2) {
            return false;
        }
        shortcuts.push_back(shortcut);
        return saveShortcutOwnership(shortcuts);
    }

    bool updateInstallOwnedShortcuts(SquirrelLifecycle::Event event, const SquirrelLifecycle::Layout& layout)
    {
        const QStringList paths = shortcutPaths();
        if (paths.size() != 2) {
            return false;
        }
        ComApartment apartment;
        if (!apartment.available()) {
            return false;
        }

        QList<SquirrelLifecycle::ShortcutOwnership> recorded;
        if (!loadShortcutOwnership(recorded)) {
            return false;
        }

        if (event == SquirrelLifecycle::Event::Uninstall) {
            for (const auto& owned : recorded) {
                const bool knownPath = std::any_of(paths.cbegin(), paths.cend(), [&](const QString& path) {
                    return absolutePath(path).compare(absolutePath(owned.path), Qt::CaseInsensitive) == 0;
                });
                if (!knownPath) {
                    return false;
                }
                SquirrelLifecycle::ShortcutOwnership current;
                if (readShortcut(owned.path, current)
                    && SquirrelLifecycle::shortcutOwnershipMatches(owned, current)) {
                    if (!QFile::remove(current.path)) {
                        return false;
                    }
                }
            }
            QSettings settings(ShortcutOwnershipRoot, QSettings::NativeFormat);
            settings.remove(ShortcutOwnershipValue);
            settings.sync();
            return settings.status() == QSettings::NoError;
        }

        if (event != SquirrelLifecycle::Event::Install && event != SquirrelLifecycle::Event::Updated) {
            return true;
        }

        const QString expectedTarget = absolutePath(layout.updateExecutable);
        for (const QString& path : paths) {
            const QFileInfo pathInfo(path);
            if (pathInfo.isSymLink()) {
                return false;
            }
            if (!pathInfo.exists()) {
                continue;
            }
            SquirrelLifecycle::ShortcutOwnership current;
            if (!readShortcut(path, current)) {
                return false;
            }
            const auto recordedIt = std::find_if(recorded.cbegin(), recorded.cend(), [&](const auto& shortcut) {
                return absolutePath(shortcut.path).compare(absolutePath(path), Qt::CaseInsensitive) == 0;
            });
            const bool hasMatchingReceipt = recordedIt != recorded.cend()
                                            && SquirrelLifecycle::shortcutOwnershipMatches(*recordedIt, current);
            const bool pointsToThisApplication = current.target.compare(expectedTarget, Qt::CaseInsensitive) == 0
                                                 && current.arguments == ShortcutArguments;
            if (!hasMatchingReceipt && !pointsToThisApplication) {
                return false;
            }
        }

        for (const QString& path : paths) {
            const QString normalizedPath = absolutePath(path);
            auto recordedIt = std::find_if(recorded.begin(), recorded.end(), [&](const auto& shortcut) {
                return absolutePath(shortcut.path).compare(normalizedPath, Qt::CaseInsensitive) == 0;
            });
            const bool hasRecord = recordedIt != recorded.end();
            bool createdNow = false;
            const QFileInfo pathInfo(path);
            if (pathInfo.isSymLink()) {
                return false;
            }
            if (!pathInfo.exists()) {
                if (event == SquirrelLifecycle::Event::Updated && hasRecord) {
                    recorded.erase(recordedIt);
                    if (!saveShortcutOwnership(recorded)) {
                        return false;
                    }
                    continue;
                }
                if (!writeShortcut(path, layout)) {
                    SquirrelLifecycle::ShortcutOwnership partial;
                    if (readShortcut(path, partial)
                        && partial.target.compare(absolutePath(layout.updateExecutable), Qt::CaseInsensitive) == 0
                        && partial.arguments == ShortcutArguments) {
                        QFile::remove(partial.path);
                    }
                    return false;
                }
                createdNow = true;
            } else {
                SquirrelLifecycle::ShortcutOwnership current;
                if (!readShortcut(path, current)) {
                    return false;
                }
                const bool pointsToThisApplication =
                    current.target.compare(expectedTarget, Qt::CaseInsensitive) == 0
                    && current.arguments == ShortcutArguments;
                if (hasRecord) {
                    if (!SquirrelLifecycle::shortcutOwnershipMatches(*recordedIt, current)) {
                        if (!pointsToThisApplication) {
                            return false;
                        }
                        // Keep a user-modified shortcut that still starts this application.
                        continue;
                    }
                    // Update.exe is stable within the install root and selects the latest release.
                    // Leaving an unchanged shortcut alone also preserves user customizations.
                    continue;
                } else {
                    if (!pointsToThisApplication) {
                        return false;
                    }
                    // A link that predates our receipt may have been created by the user.
                    // Preserve it, even if it already launches this installation.
                    continue;
                }
            }

            SquirrelLifecycle::ShortcutOwnership created;
            if (!readShortcut(path, created)
                || created.target.compare(absolutePath(layout.updateExecutable), Qt::CaseInsensitive) != 0
                || created.arguments != ShortcutArguments) {
                if (createdNow) {
                    QFile::remove(path);
                }
                return false;
            }
            if (!saveOrReplaceShortcutRecord(recorded, created)) {
                if (createdNow) {
                    QFile::remove(created.path);
                }
                return false;
            }
        }
        return true;
    }
#endif
} // namespace

namespace SquirrelLifecycle
{
    bool IntegrationResult::succeeded() const
    {
        return browser && fileAssociation && uri;
    }

    bool shortcutOwnershipMatches(const ShortcutOwnership& recorded, const ShortcutOwnership& current)
    {
        const auto samePath = [](const QString& left, const QString& right) {
            return QDir::cleanPath(QFileInfo(left).absoluteFilePath())
                       .compare(QDir::cleanPath(QFileInfo(right).absoluteFilePath()), Qt::CaseInsensitive)
                   == 0;
        };
        return !recorded.path.isEmpty() && !recorded.target.isEmpty()
               && recorded.sha256.size() == QCryptographicHash::hashLength(QCryptographicHash::Sha256)
               && recorded.sha256 == current.sha256 && samePath(recorded.path, current.path)
               && samePath(recorded.target, current.target) && recorded.arguments == current.arguments;
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

    void setIntegrationRunnerForTests(IntegrationRunner runner)
    {
        g_integrationRunner = std::move(runner);
    }

    void resetIntegrationRunnerForTests()
    {
        g_integrationRunner = {};
    }

    void setShortcutRunnerForTests(ShortcutRunner runner)
    {
        g_shortcutRunner = std::move(runner);
    }

    void resetShortcutRunnerForTests()
    {
        g_shortcutRunner = {};
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
        // Keep the dotted package identity because installed updaters validate it. Squirrel's
        // shortcut helper is unreliable for dotted package IDs, so create per-user links here
        // and retain exact ownership receipts for safe removal.
        bool shortcutsSucceeded = false;
        if (g_shortcutRunner) {
            shortcutsSucceeded = g_shortcutRunner(event, *layout);
        }
#ifdef Q_OS_WIN
        else {
            shortcutsSucceeded = updateInstallOwnedShortcuts(event, *layout);
        }
#else
        else {
            shortcutsSucceeded = true;
        }
#endif
        const IntegrationResult integration = g_integrationRunner
                                                  ? g_integrationRunner(event, *layout)
                                                  : updateInstallOwnedRegistrations(event, *layout);
        return shortcutsSucceeded && integration.succeeded() ? EXIT_SUCCESS : EXIT_FAILURE;
    }
} // namespace SquirrelLifecycle
