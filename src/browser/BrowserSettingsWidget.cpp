/*
 *  Copyright (C) 2020 KeePassXC Team <team@keepassxc.org>
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

#include "BrowserSettingsWidget.h"
#include "ui_BrowserSettingsWidget.h"

#include "BrowserExtensionInstaller.h"
#include "BrowserSettings.h"
#include "config-keepassx.h"
#include "core/Global.h"
#include "gui/Icons.h"
#include "gui/material/MaterialNotifier.h"
#include "gui/material/MaterialSeverity.h"

#include <QCheckBox>
#include <QDesktopServices>
#include <QFileDialog>
#include <QToolButton>
#include <QUrl>

using namespace BrowserShared;

namespace
{
    /**
     * Returns the browsers covered by the enable checkbox of the given browser
     *
     * @param browser Browser the checkbox belongs to
     * @return QList Browsers to register the extension for
     */
    QList<SupportedBrowsers> relatedBrowsers(SupportedBrowsers browser)
    {
#ifdef Q_OS_WIN
        // Vivaldi, Brave and Tor Browser have no checkbox of their own on Windows because they share
        // the native messaging registry keys of Chrome and Firefox. Their external extension
        // registrations are separate, so they are written along with the browser they are grouped with.
        if (browser == SupportedBrowsers::CHROME) {
            return {SupportedBrowsers::CHROME, SupportedBrowsers::VIVALDI, SupportedBrowsers::BRAVE};
        }

        if (browser == SupportedBrowsers::FIREFOX) {
            return {SupportedBrowsers::FIREFOX, SupportedBrowsers::TOR_BROWSER};
        }
#endif
        return {browser};
    }
} // namespace

BrowserSettingsWidget::BrowserSettingsWidget(QWidget* parent)
    : QWidget(parent)
    , m_ui(new Ui::BrowserSettingsWidget())
{
    m_ui->setupUi(this);

    // clang-format off
    m_ui->extensionLabel->setOpenExternalLinks(true);
    m_ui->extensionLabel->setText(
        tr("KeePassXC-Browser is needed for the browser integration to work. <br />Download it for %1 and %2 and %3.")
            .arg("<a href=\"https://addons.mozilla.org/firefox/addon/keepassxc-browser/\">Firefox</a>",
                 "<a href=\"https://chromewebstore.google.com/detail/keepassxc-browser/oboonakemofpalcgghocfoadofidjkkk\">"
                 "Google Chrome / Chromium / Vivaldi / Brave</a>",
                 "<a href=\"https://microsoftedge.microsoft.com/addons/detail/pdffhmdngciaglkoonimfcmckehcpafo\">Microsoft Edge</a>"));
    // clang-format on

    m_ui->tabWidget->setEnabled(m_ui->enableBrowserSupport->isChecked());
    connect(m_ui->enableBrowserSupport, SIGNAL(toggled(bool)), m_ui->tabWidget, SLOT(setEnabled(bool)));
    connect(m_ui->enableBrowserSupport, SIGNAL(toggled(bool)), SLOT(validateProxyLocation()));

    // Browser extension installation
    m_browserRows = {{SupportedBrowsers::CHROME, m_ui->chromeSupport, m_ui->chromeInstallButton},
                     {SupportedBrowsers::CHROMIUM, m_ui->chromiumSupport, m_ui->chromiumInstallButton},
                     {SupportedBrowsers::FIREFOX, m_ui->firefoxSupport, m_ui->firefoxInstallButton},
                     {SupportedBrowsers::VIVALDI, m_ui->vivaldiSupport, m_ui->vivaldiInstallButton},
                     {SupportedBrowsers::TOR_BROWSER, m_ui->torBrowserSupport, m_ui->torBrowserInstallButton},
                     {SupportedBrowsers::BRAVE, m_ui->braveSupport, m_ui->braveInstallButton},
                     {SupportedBrowsers::EDGE, m_ui->edgeSupport, m_ui->edgeInstallButton}};

    for (const auto& row : asConst(m_browserRows)) {
        const auto browser = row.browser;
        row.installButton->setIcon(icons()->icon("system-software-update"));
        row.installButton->setEnabled(row.checkbox->isChecked());
        connect(row.checkbox, &QCheckBox::toggled, row.installButton, &QToolButton::setEnabled);
        connect(row.installButton, &QToolButton::clicked, this, [this, browser] { installExtension(browser); });
    }

    connect(m_ui->installAllExtensionsButton,
            &QPushButton::clicked,
            this,
            &BrowserSettingsWidget::installExtensionsForEnabledBrowsers);

    // Custom Browser option
#ifdef Q_OS_WIN
    // TODO: Custom browser is disabled on Windows
    m_ui->customBrowserSupportRowContainer->setVisible(false);
    m_ui->customBrowserGroupBox->setVisible(false);
#else
    connect(m_ui->customBrowserLocationBrowseButton, SIGNAL(clicked()), SLOT(showCustomBrowserLocationFileDialog()));
    connect(m_ui->customBrowserSupport, SIGNAL(toggled(bool)), m_ui->customBrowserGroupBox, SLOT(setEnabled(bool)));
#endif

    // Custom Proxy option
    m_ui->customProxyLocation->setEnabled(m_ui->useCustomProxy->isChecked());
    m_ui->customProxyLocationBrowseButton->setEnabled(m_ui->useCustomProxy->isChecked());

    connect(m_ui->useCustomProxy, SIGNAL(toggled(bool)), m_ui->customProxyLocation, SLOT(setEnabled(bool)));
    connect(m_ui->useCustomProxy, SIGNAL(toggled(bool)), m_ui->customProxyLocationBrowseButton, SLOT(setEnabled(bool)));
    connect(m_ui->useCustomProxy, SIGNAL(toggled(bool)), SLOT(validateProxyLocation()));
    connect(m_ui->customProxyLocation, SIGNAL(editingFinished()), SLOT(validateProxyLocation()));
    connect(m_ui->customProxyLocationBrowseButton, SIGNAL(clicked()), this, SLOT(showProxyLocationFileDialog()));

    m_ui->messageWidget->setVisible(false);
    m_ui->messageWidget->setCloseButtonVisible(false);
    m_ui->messageWidget->setWordWrap(true);
    m_ui->messageWidget->setAutoHideTimeout(MessageWidget::DisableAutoHide);

#ifdef Q_OS_WIN
    // Brave uses Chrome's registry settings
    m_ui->braveSupport->setHidden(true);
    m_ui->braveInstallButton->setHidden(true);
    // Vivaldi uses Chrome's registry settings
    m_ui->vivaldiSupport->setHidden(true);
    m_ui->vivaldiInstallButton->setHidden(true);
    m_ui->chromeSupport->setText("Chrome, Vivaldi, and Brave");
    // Tor Browser uses Firefox's registry settings
    m_ui->torBrowserSupport->setHidden(true);
    m_ui->torBrowserInstallButton->setHidden(true);
    m_ui->firefoxSupport->setText("Firefox and Tor Browser");
#endif

#ifndef QT_DEBUG
    m_ui->customExtensionId->setVisible(false);
    m_ui->customExtensionLabel->setVisible(false);
#endif
}

BrowserSettingsWidget::~BrowserSettingsWidget()
{
}

void BrowserSettingsWidget::loadSettings()
{
    auto settings = browserSettings();
    m_ui->enableBrowserSupport->setChecked(settings->isEnabled());

    m_ui->showNotification->setChecked(settings->showNotification());
    m_ui->bestMatchOnly->setChecked(settings->bestMatchOnly());
    m_ui->unlockDatabase->setChecked(settings->unlockDatabase());
    m_ui->matchUrlScheme->setChecked(settings->matchUrlScheme());

    // hide unimplemented options
    // TODO: fix this
    m_ui->showNotification->hide();

    m_ui->alwaysAllowAccess->setChecked(settings->alwaysAllowAccess());
    m_ui->alwaysAllowUpdate->setChecked(settings->alwaysAllowUpdate());
    m_ui->httpAuthPermission->setChecked(settings->httpAuthPermission());
    m_ui->searchInAllDatabases->setChecked(settings->searchInAllDatabases());
    m_ui->supportKphFields->setChecked(settings->supportKphFields());
    m_ui->allowLocalhostWithPasskeys->setChecked(settings->allowLocalhostWithPasskeys());
    m_ui->noMigrationPrompt->setChecked(settings->noMigrationPrompt());
    m_ui->useCustomProxy->setChecked(settings->useCustomProxy());
    m_ui->customProxyLocation->setText(settings->replaceHomePath(settings->customProxyLocation()));
    m_ui->updateBinaryPath->setChecked(settings->updateBinaryPath());
    m_ui->allowGetDatabaseEntriesRequest->setChecked(settings->allowGetDatabaseEntriesRequest());
    m_ui->allowExpiredCredentials->setChecked(settings->allowExpiredCredentials());
    m_ui->chromeSupport->setChecked(settings->browserSupport(BrowserShared::CHROME));
    m_ui->chromiumSupport->setChecked(settings->browserSupport(BrowserShared::CHROMIUM));
    m_ui->firefoxSupport->setChecked(settings->browserSupport(BrowserShared::FIREFOX));
    m_ui->edgeSupport->setChecked(settings->browserSupport(BrowserShared::EDGE));
#ifndef Q_OS_WIN
    m_ui->braveSupport->setChecked(settings->browserSupport(BrowserShared::BRAVE));
    m_ui->vivaldiSupport->setChecked(settings->browserSupport(BrowserShared::VIVALDI));
    m_ui->torBrowserSupport->setChecked(settings->browserSupport(BrowserShared::TOR_BROWSER));
#endif
    const auto customBrowserSet = settings->customBrowserSupport();
    m_ui->customBrowserSupport->setChecked(customBrowserSet);
    m_ui->customBrowserGroupBox->setEnabled(customBrowserSet);
    m_ui->browserTypeComboBox->clear();
    m_ui->browserTypeComboBox->addItem(tr("Firefox"), BrowserShared::SupportedBrowsers::FIREFOX);
    m_ui->browserTypeComboBox->addItem(tr("Chromium"), BrowserShared::SupportedBrowsers::CHROMIUM);
    auto typeIndex = m_ui->browserTypeComboBox->findData(settings->customBrowserType());
    if (typeIndex >= 0) {
        m_ui->browserTypeComboBox->setCurrentIndex(typeIndex);
    }
    m_ui->customBrowserLocation->setText(settings->replaceHomePath(settings->customBrowserLocation()));

#ifdef QT_DEBUG
    m_ui->customExtensionId->setText(settings->customExtensionId());
#endif
    // Validate the complete proxy location dependency - not only in case it is custom,
    // to make trouble-shooting for both developer and user easier
    validateProxyLocation();
}

QString BrowserSettingsWidget::resolveCustomProxyLocation()
{
    auto settings = browserSettings();
    auto proxyLocation = m_ui->customProxyLocation->text().trimmed();
    proxyLocation = settings->replaceTildeHomePath(proxyLocation);
    return proxyLocation;
}

void BrowserSettingsWidget::validateProxyLocation()
{
    // Reset the UI
    m_ui->messageWidget->setVisible(false);
    Material::setSeverity(m_ui->customProxyLocation, Material::Severity::None);
    m_ui->customProxyLocation->setToolTip("");

    if (m_ui->enableBrowserSupport->isChecked()) {
        // If we are using a custom proxy location, check if it exists and display warning if not
        if (m_ui->useCustomProxy->isChecked()) {
            if (!QFile::exists(resolveCustomProxyLocation())) {
                Material::setSeverity(m_ui->customProxyLocation, Material::Severity::Error);
                m_ui->customProxyLocation->setToolTip(tr("The custom proxy location does not exist."));

                m_ui->messageWidget->showMessage(tr("<b>Error:</b> The custom proxy location does not exist. Correct "
                                                    "this in the advanced settings tab."),
                                                 MessageWidget::Error);
            }
        } else {
            // Otherwise check if the installed proxy exists
            auto expectedProxyLocation = browserSettings()->proxyLocationAsInstalled();
            if (!QFile::exists(expectedProxyLocation)) {
                m_ui->messageWidget->showMessage(
                    tr("<b>Error:</b> The installed proxy executable is missing from the expected location: %1<br/>"
                       "Please set a custom proxy location in the advanced settings or reinstall the application.")
                        .arg(expectedProxyLocation),
                    MessageWidget::Error);
            }
        }
    }
}

void BrowserSettingsWidget::saveSettings()
{
    auto settings = browserSettings();
    settings->setEnabled(m_ui->enableBrowserSupport->isChecked());
    settings->setShowNotification(m_ui->showNotification->isChecked());
    settings->setBestMatchOnly(m_ui->bestMatchOnly->isChecked());
    settings->setUnlockDatabase(m_ui->unlockDatabase->isChecked());
    settings->setMatchUrlScheme(m_ui->matchUrlScheme->isChecked());

    settings->setUseCustomProxy(m_ui->useCustomProxy->isChecked());
    settings->setCustomProxyLocation(resolveCustomProxyLocation());

    settings->setUpdateBinaryPath(m_ui->updateBinaryPath->isChecked());
    settings->setAllowGetDatabaseEntriesRequest(m_ui->allowGetDatabaseEntriesRequest->isChecked());
    settings->setAllowExpiredCredentials(m_ui->allowExpiredCredentials->isChecked());
    settings->setAlwaysAllowAccess(m_ui->alwaysAllowAccess->isChecked());
    settings->setAlwaysAllowUpdate(m_ui->alwaysAllowUpdate->isChecked());
    settings->setHttpAuthPermission(m_ui->httpAuthPermission->isChecked());
    settings->setSearchInAllDatabases(m_ui->searchInAllDatabases->isChecked());
    settings->setSupportKphFields(m_ui->supportKphFields->isChecked());
    settings->setAllowLocalhostWithPasskeys(m_ui->allowLocalhostWithPasskeys->isChecked());
    settings->setNoMigrationPrompt(m_ui->noMigrationPrompt->isChecked());

#ifdef QT_DEBUG
    settings->setCustomExtensionId(m_ui->customExtensionId->text());
#endif

    settings->setBrowserSupport(BrowserShared::CHROME, m_ui->chromeSupport->isChecked());
    settings->setBrowserSupport(BrowserShared::CHROMIUM, m_ui->chromiumSupport->isChecked());
    settings->setBrowserSupport(BrowserShared::FIREFOX, m_ui->firefoxSupport->isChecked());
    settings->setBrowserSupport(BrowserShared::EDGE, m_ui->edgeSupport->isChecked());
#ifndef Q_OS_WIN
    settings->setBrowserSupport(BrowserShared::BRAVE, m_ui->braveSupport->isChecked());
    settings->setBrowserSupport(BrowserShared::VIVALDI, m_ui->vivaldiSupport->isChecked());
    settings->setBrowserSupport(BrowserShared::TOR_BROWSER, m_ui->torBrowserSupport->isChecked());

    // Custom browser settings
    auto customBrowserEnabled = m_ui->customBrowserSupport->isChecked();
    settings->setCustomBrowserType(m_ui->browserTypeComboBox->currentData().toInt());
    settings->setCustomBrowserLocation(
        customBrowserEnabled ? browserSettings()->replaceTildeHomePath(m_ui->customBrowserLocation->text()) : "");
    settings->setCustomBrowserSupport(customBrowserEnabled);
    settings->setBrowserSupport(BrowserShared::CUSTOM, customBrowserEnabled);
#endif

    // Register the browser extension with the enabled browsers if the user has opted in to it
    if (settings->autoInstallExtension()) {
        autoRegisterExtensions();
    }
}

/**
 * Registers the browser extension for the browsers covered by a single enable checkbox
 *
 * @param browser Selected browser
 * @return Result Combined result of the registrations
 */
BrowserExtensionInstaller::Result BrowserSettingsWidget::registerExtension(SupportedBrowsers browser)
{
    using Result = BrowserExtensionInstaller::Result;

    auto registered = false;
    auto manualInstall = false;

    const auto browsers = relatedBrowsers(browser);
    for (auto related : browsers) {
        const auto result = m_extensionInstaller.installExtension(related);
        if (result == Result::Failed) {
            return Result::Failed;
        }

        registered = registered || (result == Result::Registered);
        manualInstall = manualInstall || (result == Result::RequiresManualInstall);
    }

    if (registered) {
        return Result::Registered;
    }

    return manualInstall ? Result::RequiresManualInstall : Result::AlreadyPresent;
}

/**
 * Registers the browser extension for a single browser and reports the outcome to the user
 *
 * @param browser Selected browser
 */
void BrowserSettingsWidget::installExtension(SupportedBrowsers browser)
{
    using Result = BrowserExtensionInstaller::Result;

    const auto name = BrowserExtensionInstaller::browserName(browser);
    switch (registerExtension(browser)) {
    case Result::Registered:
        Material::Notify::success(tr("Browser Extension"),
                                  tr("KeePassXC-Browser has been registered for %1.\n\n"
                                     "%1 will ask you to enable the extension the next time it is started. "
                                     "The extension is not enabled until you accept that request.")
                                      .arg(name));
        break;
    case Result::AlreadyPresent:
        Material::Notify::info(tr("Browser Extension"),
                               tr("KeePassXC-Browser is already registered for %1.\n\n"
                                  "If the extension is still missing, %1 will ask you to enable it the next "
                                  "time it is started.")
                                   .arg(name));
        break;
    case Result::RequiresManualInstall: {
        // The browser does not support external extension registrations, open the web store instead
        const auto storeUrl = BrowserExtensionInstaller::webStoreUrl(browser);
        if (!QDesktopServices::openUrl(QUrl(storeUrl))) {
            Material::Notify::warning(tr("Browser Extension"),
                                      tr("%1 does not allow other applications to install extensions. "
                                         "Please install KeePassXC-Browser from %2.")
                                          .arg(name, storeUrl));
        }
        break;
    }
    case Result::Failed:
        Material::Notify::error(tr("Browser Extension"),
                                tr("Could not register KeePassXC-Browser for %1.").arg(name));
        break;
    }
}

/**
 * Registers the browser extension for every browser that is currently enabled
 */
void BrowserSettingsWidget::installExtensionsForEnabledBrowsers()
{
    using Result = BrowserExtensionInstaller::Result;

    QStringList registered;
    QStringList alreadyPresent;
    QStringList manualInstall;
    QStringList failed;
    QStringList storeUrls;

    for (const auto& row : asConst(m_browserRows)) {
        if (!row.checkbox->isChecked()) {
            continue;
        }

        const auto name = BrowserExtensionInstaller::browserName(row.browser);
        switch (registerExtension(row.browser)) {
        case Result::Registered:
            registered << name;
            break;
        case Result::AlreadyPresent:
            alreadyPresent << name;
            break;
        case Result::RequiresManualInstall: {
            const auto storeUrl = BrowserExtensionInstaller::webStoreUrl(row.browser);
            manualInstall << name;
            if (!storeUrl.isEmpty() && !storeUrls.contains(storeUrl)) {
                storeUrls << storeUrl;
            }
            break;
        }
        case Result::Failed:
            failed << name;
            break;
        }
    }

    if (registered.isEmpty() && alreadyPresent.isEmpty() && manualInstall.isEmpty() && failed.isEmpty()) {
        Material::Notify::warning(tr("Browser Extension"), tr("Enable at least one browser first."));
        return;
    }

    QStringList message;
    if (!registered.isEmpty()) {
        message << tr("KeePassXC-Browser has been registered for: %1.\n"
                      "These browsers will ask you to enable the extension the next time they are started.")
                       .arg(registered.join(", "));
    }

    if (!alreadyPresent.isEmpty()) {
        message << tr("KeePassXC-Browser was already registered for: %1.").arg(alreadyPresent.join(", "));
    }

    if (!manualInstall.isEmpty()) {
        message << tr("These browsers do not allow other applications to install extensions: %1.\n"
                      "Their extension pages have been opened for a manual installation.")
                       .arg(manualInstall.join(", "));
    }

    if (!failed.isEmpty()) {
        message << tr("Registering KeePassXC-Browser failed for: %1.").arg(failed.join(", "));
    }

    for (const auto& storeUrl : asConst(storeUrls)) {
        QDesktopServices::openUrl(QUrl(storeUrl));
    }

    if (failed.isEmpty()) {
        Material::Notify::success(tr("Browser Extension"), message.join("\n\n"));
    } else {
        Material::Notify::warning(tr("Browser Extension"), message.join("\n\n"));
    }
}

/**
 * Registers the browser extension for the enabled browsers without any user interaction.
 *
 * Only used when the user has opted in with Browser_AutoInstallExtension. Browsers that cannot be
 * registered automatically are skipped, their web store pages are never opened unattended.
 */
void BrowserSettingsWidget::autoRegisterExtensions()
{
    for (const auto& row : asConst(m_browserRows)) {
        if (!row.checkbox->isChecked()) {
            continue;
        }

        const auto browsers = relatedBrowsers(row.browser);
        for (auto browser : browsers) {
            if (BrowserExtensionInstaller::supportsAutomaticRegistration(browser)) {
                m_extensionInstaller.installExtension(browser);
            }
        }
    }
}

void BrowserSettingsWidget::showProxyLocationFileDialog()
{
#ifdef Q_OS_WIN
    QString fileTypeFilter(QString("%1 (*.exe);;%2 (*.*)").arg(tr("Executable Files"), tr("All Files")));
#else
    QString fileTypeFilter(QString("%1 (*)").arg(tr("Executable Files")));
#endif

    auto initialFilePath = resolveCustomProxyLocation();
    if (QFileInfo::exists(initialFilePath)) {
        initialFilePath = QFileInfo(initialFilePath).filePath();
    } else {
        // ignore current status and set as it would be installed
        initialFilePath = QFileInfo(browserSettings()->proxyLocationAsInstalled()).filePath();
    }

    QString proxyLocation =
        QFileDialog::getOpenFileName(this, tr("Select custom proxy location"), initialFilePath, fileTypeFilter);

    if (!proxyLocation.isEmpty()) {
        proxyLocation = browserSettings()->replaceHomePath(proxyLocation);
        m_ui->customProxyLocation->setText(proxyLocation);
        validateProxyLocation();
    } else {
        // do not overwrite old proxy setting
    }
}

void BrowserSettingsWidget::showCustomBrowserLocationFileDialog()
{
    auto location = QFileDialog::getExistingDirectory(this,
                                                      tr("Select native messaging host folder location"),
                                                      QFileInfo(QCoreApplication::applicationDirPath()).filePath());

    location = browserSettings()->replaceHomePath(location);
    if (!location.isEmpty()) {
        m_ui->customBrowserLocation->setText(location);
    }
}
