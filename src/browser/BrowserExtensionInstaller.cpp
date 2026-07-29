/*
 *  Copyright (C) 2026 KeePassXC Team <team@keepassxc.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "BrowserExtensionInstaller.h"
#include "BrowserSettings.h"
#include "config-keepassx.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QSettings>
#include <QStandardPaths>

using namespace BrowserShared;

namespace
{
    // Chrome Web Store build, used by Chrome, Chromium, Vivaldi and Brave
    const QString CHROME_EXTENSION_ID = QStringLiteral("oboonakemofpalcgghocfoadofidjkkk");
    // Microsoft Edge Add-ons build
    const QString EDGE_EXTENSION_ID = QStringLiteral("pdffhmdngciaglkoonimfcmckehcpafo");
    const QString FIREFOX_EXTENSION_ID = QStringLiteral("keepassxc-browser@keepassxc.org");

    const QString CHROME_UPDATE_URL = QStringLiteral("https://clients2.google.com/service/update2/crx");
    const QString EDGE_UPDATE_URL = QStringLiteral("https://edge.microsoft.com/extensionwebstorebase/v1/crx");

    const QString CHROME_STORE_URL =
        QStringLiteral("https://chromewebstore.google.com/detail/keepassxc-browser/oboonakemofpalcgghocfoadofidjkkk");
    const QString EDGE_STORE_URL =
        QStringLiteral("https://microsoftedge.microsoft.com/addons/detail/pdffhmdngciaglkoonimfcmckehcpafo");
    const QString FIREFOX_STORE_URL = QStringLiteral("https://addons.mozilla.org/firefox/addon/keepassxc-browser/");

#if defined(Q_OS_WIN)
    // Per-user external extension registrations. Never HKEY_LOCAL_MACHINE, that would need elevation
    const QString REG_DIR_CHROME = QStringLiteral("HKEY_CURRENT_USER\\Software\\Google\\Chrome\\Extensions");
    const QString REG_DIR_CHROMIUM = QStringLiteral("HKEY_CURRENT_USER\\Software\\Chromium\\Extensions");
    const QString REG_DIR_VIVALDI = QStringLiteral("HKEY_CURRENT_USER\\Software\\Vivaldi\\Extensions");
    const QString REG_DIR_BRAVE =
        QStringLiteral("HKEY_CURRENT_USER\\Software\\BraveSoftware\\Brave-Browser\\Extensions");
    const QString REG_DIR_EDGE = QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Edge\\Extensions");
#elif !defined(Q_OS_MACOS)
    // Directories relative to the user's configuration directory
    const QString EXT_DIR_CHROME = QStringLiteral("/google-chrome/External Extensions");
    const QString EXT_DIR_CHROMIUM = QStringLiteral("/chromium/External Extensions");
    const QString EXT_DIR_VIVALDI = QStringLiteral("/vivaldi/External Extensions");
    const QString EXT_DIR_BRAVE = QStringLiteral("/BraveSoftware/Brave-Browser/External Extensions");
    const QString EXT_DIR_EDGE = QStringLiteral("/microsoft-edge/External Extensions");
#endif
} // namespace

/**
 * Returns the extension id of keepassxc-browser for the selected browser
 *
 * @param browser Selected browser
 * @return QString Extension id, empty if the browser is not supported
 */
QString BrowserExtensionInstaller::extensionId(SupportedBrowsers browser)
{
    switch (browser) {
    case SupportedBrowsers::CHROME:
    case SupportedBrowsers::CHROMIUM:
    case SupportedBrowsers::VIVALDI:
    case SupportedBrowsers::BRAVE:
        return CHROME_EXTENSION_ID;
    case SupportedBrowsers::EDGE:
        return EDGE_EXTENSION_ID;
    case SupportedBrowsers::FIREFOX:
    case SupportedBrowsers::TOR_BROWSER:
        return FIREFOX_EXTENSION_ID;
    case SupportedBrowsers::CUSTOM:
        return isFirefoxBased(browser) ? FIREFOX_EXTENSION_ID : CHROME_EXTENSION_ID;
    default:
        return {};
    }
}

/**
 * Returns the web store page of keepassxc-browser for the selected browser
 *
 * @param browser Selected browser
 * @return QString Web store URL, empty if the browser is not supported
 */
QString BrowserExtensionInstaller::webStoreUrl(SupportedBrowsers browser)
{
    switch (browser) {
    case SupportedBrowsers::CHROME:
    case SupportedBrowsers::CHROMIUM:
    case SupportedBrowsers::VIVALDI:
    case SupportedBrowsers::BRAVE:
        return CHROME_STORE_URL;
    case SupportedBrowsers::EDGE:
        return EDGE_STORE_URL;
    case SupportedBrowsers::FIREFOX:
    case SupportedBrowsers::TOR_BROWSER:
        return FIREFOX_STORE_URL;
    case SupportedBrowsers::CUSTOM:
        return isFirefoxBased(browser) ? FIREFOX_STORE_URL : CHROME_STORE_URL;
    default:
        return {};
    }
}

/**
 * Returns the update URL used for the external extension registration
 *
 * @param browser Selected browser
 * @return QString Update URL, empty for browsers that cannot be registered automatically
 */
QString BrowserExtensionInstaller::updateUrl(SupportedBrowsers browser)
{
    switch (browser) {
    case SupportedBrowsers::CHROME:
    case SupportedBrowsers::CHROMIUM:
    case SupportedBrowsers::VIVALDI:
    case SupportedBrowsers::BRAVE:
        return CHROME_UPDATE_URL;
    case SupportedBrowsers::EDGE:
        return EDGE_UPDATE_URL;
    case SupportedBrowsers::CUSTOM:
        return isFirefoxBased(browser) ? QString() : CHROME_UPDATE_URL;
    default:
        // Firefox-based browsers do not use external extension registrations
        return {};
    }
}

/**
 * Returns the display name of the selected browser
 *
 * @param browser Selected browser
 * @return QString Name shown to the user
 */
QString BrowserExtensionInstaller::browserName(SupportedBrowsers browser)
{
    switch (browser) {
    case SupportedBrowsers::CHROME:
        return QStringLiteral("Google Chrome");
    case SupportedBrowsers::CHROMIUM:
        return QStringLiteral("Chromium");
    case SupportedBrowsers::FIREFOX:
        return QStringLiteral("Firefox");
    case SupportedBrowsers::VIVALDI:
        return QStringLiteral("Vivaldi");
    case SupportedBrowsers::TOR_BROWSER:
        return QStringLiteral("Tor Browser");
    case SupportedBrowsers::BRAVE:
        return QStringLiteral("Brave");
    case SupportedBrowsers::EDGE:
        return QStringLiteral("Microsoft Edge");
    case SupportedBrowsers::CUSTOM:
        return QObject::tr("Custom Browser");
    default:
        return {};
    }
}

/**
 * Checks if the selected browser is Firefox-based
 *
 * @param browser Selected browser
 * @return bool Browser uses Mozilla add-ons
 */
bool BrowserExtensionInstaller::isFirefoxBased(SupportedBrowsers browser)
{
    if (browser == SupportedBrowsers::FIREFOX || browser == SupportedBrowsers::TOR_BROWSER) {
        return true;
    }

    return browser == SupportedBrowsers::CUSTOM
           && browserSettings()->customBrowserType() == SupportedBrowsers::FIREFOX;
}

/**
 * Checks if keepassxc-browser can be registered for the selected browser without user interaction.
 *
 * Note that even when this returns true the browser still asks the user to enable the extension.
 * There is no supported way to install and enable an extension completely silently.
 *
 * @param browser Selected browser
 * @return bool An external extension registration can be written for this browser
 */
bool BrowserExtensionInstaller::supportsAutomaticRegistration(SupportedBrowsers browser)
{
    // Firefox only installs add-ons through the browser itself or through the ExtensionSettings
    // enterprise policy. The policy file lives in the Firefox installation directory and writing it
    // requires administrator rights, so it is deliberately not used here.
    if (isFirefoxBased(browser)) {
        return false;
    }

#if defined(Q_OS_WIN)
    return !registryPath(browser).isEmpty();
#elif defined(Q_OS_MACOS)
    // The only external extensions directory Chromium reads on macOS is
    // /Library/Application Support/<Browser>/External Extensions. It is shared by all users and
    // writing to it requires administrator rights, so extensions are installed manually instead.
    return false;
#else
    return !externalExtensionDirectory(browser).isEmpty();
#endif
}

#ifdef Q_OS_WIN
/**
 * Returns the registry key that holds the external extension registrations of the selected browser
 *
 * @param browser Selected browser
 * @return QString Registry path below HKEY_CURRENT_USER, empty if unsupported
 */
QString BrowserExtensionInstaller::registryPath(SupportedBrowsers browser)
{
    switch (browser) {
    case SupportedBrowsers::CHROME:
        return REG_DIR_CHROME;
    case SupportedBrowsers::CHROMIUM:
        return REG_DIR_CHROMIUM;
    case SupportedBrowsers::VIVALDI:
        return REG_DIR_VIVALDI;
    case SupportedBrowsers::BRAVE:
        return REG_DIR_BRAVE;
    case SupportedBrowsers::EDGE:
        return REG_DIR_EDGE;
    default:
        // Firefox-based browsers and the custom browser have no registration mechanism
        return {};
    }
}
#else
/**
 * Returns the external extensions directory of the selected browser
 *
 * @param browser Selected browser
 * @return QString Absolute directory path, empty if unsupported
 */
QString BrowserExtensionInstaller::externalExtensionDirectory(SupportedBrowsers browser)
{
#if defined(Q_OS_MACOS)
    Q_UNUSED(browser)
    return {};
#else
    QString targetDir;
    switch (browser) {
    case SupportedBrowsers::CHROME:
        targetDir = EXT_DIR_CHROME;
        break;
    case SupportedBrowsers::CHROMIUM:
        targetDir = EXT_DIR_CHROMIUM;
        break;
    case SupportedBrowsers::VIVALDI:
        targetDir = EXT_DIR_VIVALDI;
        break;
    case SupportedBrowsers::BRAVE:
        targetDir = EXT_DIR_BRAVE;
        break;
    case SupportedBrowsers::EDGE:
        targetDir = EXT_DIR_EDGE;
        break;
    default:
        return {};
    }

    QString basePath;
#if defined(KEEPASSXC_DIST_FLATPAK)
    // Flatpak sandboxes do not have access to the XDG_CONFIG_HOME variable defined in the host
    basePath = QDir::homePath() + QStringLiteral("/.config");
#elif defined(KEEPASSXC_DIST_SNAP)
    // Snap also redefines $HOME, so $SNAP_REAL_HOME must be referenced explicitly
    basePath = qEnvironmentVariable("SNAP_REAL_HOME") + QStringLiteral("/.config");
#else
    basePath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
#endif
    if (basePath.isEmpty()) {
        return {};
    }

    return basePath + targetDir;
#endif
}

/**
 * Returns the path of the external extension file of the selected browser
 *
 * @param browser Selected browser
 * @return QString Absolute file path, empty if unsupported
 */
QString BrowserExtensionInstaller::externalExtensionPath(SupportedBrowsers browser)
{
    const auto directory = externalExtensionDirectory(browser);
    if (directory.isEmpty()) {
        return {};
    }

    return QStringLiteral("%1/%2.json").arg(directory, extensionId(browser));
}
#endif

/**
 * Checks if keepassxc-browser has been registered for the selected browser by KeePassXC.
 *
 * This only tells if the registration written by KeePassXC exists. It does not tell whether the
 * user has actually enabled the extension in the browser.
 *
 * @param browser Selected browser
 * @return bool Registration exists
 */
bool BrowserExtensionInstaller::isExtensionRegistered(SupportedBrowsers browser) const
{
    if (!supportsAutomaticRegistration(browser)) {
        return false;
    }

#ifdef Q_OS_WIN
    QSettings settings(registryPath(browser), QSettings::NativeFormat);
    return !settings.value(QStringLiteral("%1/update_url").arg(extensionId(browser))).isNull();
#else
    return QFile::exists(externalExtensionPath(browser));
#endif
}

/**
 * Registers keepassxc-browser with the selected browser.
 *
 * The browser shows an "enable extension" prompt to the user the next time it is started. The
 * extension is never enabled without the user accepting that prompt.
 *
 * @param browser Selected browser
 * @return Result Outcome of the registration
 */
BrowserExtensionInstaller::Result BrowserExtensionInstaller::installExtension(SupportedBrowsers browser)
{
    if (extensionId(browser).isEmpty()) {
        return Result::Failed;
    }

    if (!supportsAutomaticRegistration(browser)) {
        return Result::RequiresManualInstall;
    }

    if (isExtensionRegistered(browser)) {
        return Result::AlreadyPresent;
    }

#ifdef Q_OS_WIN
    QSettings settings(registryPath(browser), QSettings::NativeFormat);
    settings.setValue(QStringLiteral("%1/update_url").arg(extensionId(browser)), updateUrl(browser));
    settings.sync();

    if (settings.status() != QSettings::NoError) {
        qWarning() << "Browser Plugin: Failed to write the extension registration to " << registryPath(browser);
        return Result::Failed;
    }
#else
    const auto path = externalExtensionPath(browser);

    // Make the parent directory path if necessary
    QDir().mkpath(QFileInfo(path).absolutePath());

    QFile extensionFile(path);
    if (!extensionFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "Browser Plugin: Failed to open external extension file at " << extensionFile.fileName();
        qWarning() << extensionFile.errorString();
        return Result::Failed;
    }

    QJsonObject registration;
    registration["external_update_url"] = updateUrl(browser);

    if (extensionFile.write(QJsonDocument(registration).toJson()) < 0) {
        qWarning() << "Browser Plugin: Failed to write external extension file at " << extensionFile.fileName();
        qWarning() << extensionFile.errorString();
        return Result::Failed;
    }
    extensionFile.close();
#endif

    return isExtensionRegistered(browser) ? Result::Registered : Result::Failed;
}

/**
 * Removes the extension registration written by KeePassXC.
 *
 * An extension the user has already enabled stays installed. Only the registration is removed.
 *
 * @param browser Selected browser
 */
void BrowserExtensionInstaller::removeExtensionRegistration(SupportedBrowsers browser)
{
    if (!supportsAutomaticRegistration(browser)) {
        return;
    }

#ifdef Q_OS_WIN
    // Removes the whole <extension-id> subkey including the update_url value
    QSettings settings(registryPath(browser), QSettings::NativeFormat);
    settings.remove(extensionId(browser));
    settings.sync();
#else
    QFile::remove(externalExtensionPath(browser));
#endif
}
