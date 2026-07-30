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

#ifndef KEEPASSXC_BROWSEREXTENSIONINSTALLER_H
#define KEEPASSXC_BROWSEREXTENSIONINSTALLER_H

#include "BrowserShared.h"

#include <QtGlobal>

/**
 * Registers keepassxc-browser with the supported browsers.
 *
 * No browser allows a third party application to install and enable an extension silently. What the
 * Chromium family does support is an "external extension" registration that points at a web store
 * update URL. The browser picks the registration up the next time it starts and asks the user
 * whether the extension may be enabled. Everything else (Firefox-based browsers, macOS, custom
 * browser locations) has to be installed by the user from the web store.
 *
 * This class never writes outside of the current user's own registry hive or configuration
 * directory and never requires elevated privileges.
 */
class BrowserExtensionInstaller
{
public:
    enum class Result
    {
        Registered, // The external extension registration was written
        AlreadyPresent, // The registration was already in place
        RequiresManualInstall, // The user has to install the extension from the web store
        Failed // Writing the registration failed
    };

    BrowserExtensionInstaller() = default;

    Result installExtension(BrowserShared::SupportedBrowsers browser);
    bool isExtensionRegistered(BrowserShared::SupportedBrowsers browser) const;
    void removeExtensionRegistration(BrowserShared::SupportedBrowsers browser);

    static QString webStoreUrl(BrowserShared::SupportedBrowsers browser);
    static QString extensionId(BrowserShared::SupportedBrowsers browser);
    static QString updateUrl(BrowserShared::SupportedBrowsers browser);
    static bool supportsAutomaticRegistration(BrowserShared::SupportedBrowsers browser);
    static QString browserName(BrowserShared::SupportedBrowsers browser);

private:
    static bool isFirefoxBased(BrowserShared::SupportedBrowsers browser);
#ifdef Q_OS_WIN
    static QString registryPath(BrowserShared::SupportedBrowsers browser);
#else
    static QString externalExtensionDirectory(BrowserShared::SupportedBrowsers browser);
    static QString externalExtensionPath(BrowserShared::SupportedBrowsers browser);
#endif

    Q_DISABLE_COPY(BrowserExtensionInstaller);
};

#endif // KEEPASSXC_BROWSEREXTENSIONINSTALLER_H
