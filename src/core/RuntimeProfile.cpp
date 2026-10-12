#include "RuntimeProfile.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QTemporaryFile>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace
{
    QString profileRoot;

    bool fail(QString* error, const QString& message)
    {
        if (error) {
            *error = message;
        }
        return false;
    }

    bool hasSymbolicAncestor(QString path)
    {
        for (;;) {
            const QFileInfo info(path);
            if (info.isSymLink()) {
                return true;
            }
#ifdef Q_OS_WIN
            const DWORD attributes = GetFileAttributesW(reinterpret_cast<const wchar_t*>(path.utf16()));
            if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT)) return true;
#endif
            const QString parent = info.absolutePath();
            if (parent == path) {
                return false;
            }
            path = parent;
        }
    }
}

bool RuntimeProfile::initialize(const QStringList& arguments, QString* error)
{
    QString requested;
    bool found = false;
    for (int i = 1; i < arguments.size(); ++i) {
        const auto& argument = arguments.at(i);
        if (argument == QStringLiteral("--verification-profile")
            || argument.startsWith(QStringLiteral("--verification-profile="))) {
            if (found) {
                return fail(error, QStringLiteral("The verification profile option may only be supplied once."));
            }
            found = true;
            if (argument == QStringLiteral("--verification-profile")) {
                if (++i >= arguments.size()) {
                    return fail(error, QStringLiteral("The verification profile requires an absolute directory."));
                }
                requested = arguments.at(i);
            } else {
                requested = argument.mid(QStringLiteral("--verification-profile=").size());
            }
        }
    }
    if (!found) {
        if (!profileRoot.isEmpty()) {
            return fail(error, QStringLiteral("An active verification profile cannot be disabled in this process."));
        }
        if (error) error->clear();
        return true;
    }
    if (requested.isEmpty() || !QDir::isAbsolutePath(requested)) {
        return fail(error, QStringLiteral("The verification profile requires an absolute directory."));
    }
    const QString normalized = QDir::cleanPath(QDir::fromNativeSeparators(requested));
    if (QDir(normalized).isRoot() || hasSymbolicAncestor(normalized)) {
        return fail(error, QStringLiteral("The verification profile must not be a filesystem root or symbolic link."));
    }
    // Validate the actual settings files before any profile marker or settings
    // operation. A safe directory does not make a linked child file safe.
    for (const auto& relative : {"config/keepassxc.ini", "cache/keepassxc.ini"}) {
        const QString path = QDir(normalized).filePath(QString::fromLatin1(relative));
        const QFileInfo settings(path);
        if (hasSymbolicAncestor(path) || (settings.exists() && !settings.isFile())) {
            return fail(error, QStringLiteral("Verification settings files must be regular, unlinked files inside the profile."));
        }
    }
    if (!profileRoot.isEmpty()) {
        return normalized == profileRoot
                   ? true
                   : fail(error, QStringLiteral("The verification profile cannot change in this process."));
    }
    // A fresh directory, or one previously marked by this mode, is required.
    // Never adopt an arbitrary existing directory containing personal data.
    const QFileInfo info(normalized);
    const QString marker = QDir(normalized).filePath(QStringLiteral(".keepassxc-verification-profile"));
    if (info.exists() && (!info.isDir()
                          || (!QDir(normalized).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot).isEmpty()
                              && !QFileInfo(marker).isFile()))) {
        return fail(error, QStringLiteral("The verification profile must be empty or an existing verification profile."));
    }
    if (QFileInfo::exists(marker)) {
        QFile existingMarker(marker);
        if (hasSymbolicAncestor(marker) || !existingMarker.open(QIODevice::ReadOnly)
            || existingMarker.read(128) != QByteArrayLiteral("KeePassXC synthetic verification profile\n")) {
            return fail(error, QStringLiteral("The verification profile marker is invalid."));
        }
    }
    if (!QDir().mkpath(normalized)) {
        return fail(error, QStringLiteral("The verification profile directory could not be created."));
    }
    const QString canonical = QFileInfo(normalized).canonicalFilePath();
    if (canonical.isEmpty() || hasSymbolicAncestor(canonical)) {
        return fail(error, QStringLiteral("The verification profile directory could not be resolved safely."));
    }
    for (const auto& child : {"config", "data", "cache", "runtime", "temp"}) {
        const QString path = QDir(canonical).filePath(QString::fromLatin1(child));
        if (hasSymbolicAncestor(path) || !QDir().mkpath(path)) {
            return fail(error, QStringLiteral("A verification profile storage directory could not be created safely."));
        }
        QTemporaryFile probe(QDir(path).filePath(QStringLiteral(".write-test-XXXXXX")));
        if (!probe.open()) {
            return fail(error, QStringLiteral("The verification profile storage is not writable."));
        }
    }
    QFile markerFile(QDir(canonical).filePath(QStringLiteral(".keepassxc-verification-profile")));
    if (!markerFile.open(QIODevice::WriteOnly | QIODevice::Truncate)
        || markerFile.write("KeePassXC synthetic verification profile\n") < 0) {
        return fail(error, QStringLiteral("The verification profile marker could not be written."));
    }
    profileRoot = canonical;
    // Qt's temporary-file helpers also need a process-local location.
    const QByteArray temporary = QDir(profileRoot).filePath(QStringLiteral("temp")).toUtf8();
    qputenv("TMPDIR", temporary);
    qputenv("TMP", temporary);
    qputenv("TEMP", temporary);
    if (error) error->clear();
    return true;
}

bool RuntimeProfile::isActive() { return !profileRoot.isEmpty(); }
QString RuntimeProfile::root() { return profileRoot; }

QString RuntimeProfile::location(QStandardPaths::StandardLocation type)
{
    if (!isActive()) return QStandardPaths::writableLocation(type);
    QString child;
    switch (type) {
    case QStandardPaths::ConfigLocation:
    case QStandardPaths::GenericConfigLocation:
    case QStandardPaths::AppConfigLocation:
        child = QStringLiteral("config");
        break;
    case QStandardPaths::CacheLocation:
    case QStandardPaths::GenericCacheLocation:
        child = QStringLiteral("cache");
        break;
    case QStandardPaths::RuntimeLocation:
        child = QStringLiteral("runtime");
        break;
    case QStandardPaths::TempLocation:
        child = QStringLiteral("temp");
        break;
    default:
        child = QStringLiteral("data");
        break;
    }
    return QDir(profileRoot).filePath(child);
}

QString RuntimeProfile::ipcIdentifier(const QString& normalIdentifier)
{
    if (!isActive()) return normalIdentifier;
    QString identityRoot = profileRoot;
#ifdef Q_OS_WIN
    identityRoot = identityRoot.toCaseFolded();
#endif
    return normalIdentifier + QStringLiteral("-verification-")
           + QString::fromLatin1(QCryptographicHash::hash(identityRoot.toUtf8(), QCryptographicHash::Sha256).toHex().left(24));
}

bool RuntimeProfile::contains(const QString& path)
{
    if (!isActive() || !QDir::isAbsolutePath(path) || hasSymbolicAncestor(QDir::cleanPath(path))) return false;
    const QString normalized = QDir::cleanPath(QDir::fromNativeSeparators(path));
#ifdef Q_OS_WIN
    const Qt::CaseSensitivity sensitivity = Qt::CaseInsensitive;
#else
    const Qt::CaseSensitivity sensitivity = Qt::CaseSensitive;
#endif
    return normalized.compare(profileRoot, sensitivity) == 0
           || normalized.startsWith(profileRoot + QLatin1Char('/'), sensitivity);
}
