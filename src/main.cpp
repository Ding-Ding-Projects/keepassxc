/*
 *  Copyright (C) 2026 KeePassXC Team <team@keepassxc.org>
 *  Copyright (C) 2010 Felix Geyer <debfx@fobos.de>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 or (at your option)
 *  version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QThreadPool>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QWindow>

#include "cli/Utils.h"
#include "config-keepassx.h"
#include "core/Tools.h"
#include "crypto/Crypto.h"
#include "gui/Application.h"
#include "gui/MainWindow.h"
#include "gui/MessageBox.h"
#include "gui/material/MaterialCaptureRoute.h"
#include "gui/material/MaterialDimSum.h"
#include "gui/osutils/OSUtils.h"
#include "platform/SquirrelLifecycle.h"
#ifdef KPXC_FEATURE_UPDATES
#include "networking/UpdateChecker.h"
#endif

#if defined(WITH_ASAN) && defined(WITH_LSAN)
#include <sanitizer/lsan_interface.h>
#endif

#ifdef QT_STATIC
#include <QtPlugin>

#if defined(Q_OS_WIN)
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)
#endif
#endif

#ifdef Q_OS_WIN
#include <windows.h>
#endif

int main(int argc, char** argv)
{
    QT_REQUIRE_VERSION(argc, argv, QT_VERSION_STR)

#ifdef Q_OS_WIN
    // Set OPENSSL_* variables to an invalid location to prevent DLL injection via openssl.cnf.
    // vcpkg by default hard-codes this to its packages location, which may be user-writable.
    qputenv("OPENSSL_CONF", "::");
    qputenv("OPENSSL_MODULES", "::");
    qputenv("OPENSSL_ENGINES", "::");
#endif

    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QGuiApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QGuiApplication::setDesktopFileName("org.keepassxc.KeePassXC");
#if defined(Q_OS_WIN)
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
#endif
    // A display-scale override for capture matrices has to reach Qt before the
    // application object exists, so it is read from the raw arguments here.
    for (int i = 1; i + 1 < argc; ++i) {
        if (qstrcmp(argv[i], "--capture-scale") == 0) {
            qputenv("QT_SCALE_FACTOR", argv[i + 1]);
            qputenv("QT_ENABLE_HIGHDPI_SCALING", "0");
        }
    }
    QString verificationProfile;
    QString verificationCaptureRoute;
    bool conflictingConfig = false;
    bool malformedVerificationOption = false;
    bool verificationRequested = false;
    for (int i = 1; i < argc; ++i) {
        const QString argument = QString::fromLocal8Bit(argv[i]);
        if (argument == QLatin1String("--")) break;
        if (argument == QLatin1String("--capture-route") && i + 1 < argc) {
            verificationCaptureRoute = QString::fromLocal8Bit(argv[i + 1]);
        } else if (argument.startsWith(QLatin1String("--capture-route="))) {
            verificationCaptureRoute = argument.mid(QStringLiteral("--capture-route=").size());
        }
        conflictingConfig |= argument == QLatin1String("--config") || argument == QLatin1String("--localconfig")
                             || argument.startsWith(QLatin1String("--config="))
                             || argument.startsWith(QLatin1String("--localconfig="));
        if (argument == QLatin1String("--verification-profile")) {
            verificationRequested = true;
            if (i + 1 < argc) verificationProfile = QString::fromLocal8Bit(argv[++i]);
        } else if (argument.startsWith(QLatin1String("--verification-profile="))) {
            verificationRequested = true;
            verificationProfile = argument.mid(QStringLiteral("--verification-profile=").size());
        } else if (argument.startsWith(QLatin1String("--verification-profile"))) {
            verificationRequested = true;
            malformedVerificationOption = true;
        }
    }
    if (verificationRequested) {
        Material::CaptureRoute::Request verificationRequest;
        if (verificationProfile.isEmpty() || conflictingConfig || malformedVerificationOption
            || !Material::CaptureRoute::parse(verificationCaptureRoute, verificationRequest)
            || QRegularExpression(QStringLiteral("^[A-Za-z0-9_-]{1,64}$")).match(verificationProfile).capturedLength() != verificationProfile.size()) {
            qCritical("Verification profiles require a valid capture route, a safe unique identifier, and no configuration override.");
            return EXIT_FAILURE;
        }
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setApplicationName(QStringLiteral("KeePassXC-Verification-") + verificationProfile);
    }
    Application app(argc, argv);
    // don't set organizationName as that changes the return value of
    // QStandardPaths::writableLocation(QDesktopServices::DataLocation)
    if (verificationProfile.isEmpty()) Application::setApplicationName("KeePassXC");
    Application::setApplicationVersion(KEEPASSXC_VERSION);
    app.setProperty("KPXC_QUALIFIED_APPNAME", "org.keepassxc.KeePassXC");

    QStringList applicationArguments = app.arguments();
    if (const auto lifecycleExit =
            SquirrelLifecycle::handle(applicationArguments, QCoreApplication::applicationDirPath())) {
        return *lifecycleExit;
    }
    SquirrelLifecycle::consume(applicationArguments);

    // HACK: Prevent long-running threads from deadlocking the program with only 1 CPU
    // See https://github.com/keepassxreboot/keepassxc/issues/10391
    // HACK: increased to a minimum of 3 threads
    // See https://github.com/keepassxreboot/keepassxc/issues/12909
    if (QThreadPool::globalInstance()->maxThreadCount() < 3) {
        QThreadPool::globalInstance()->setMaxThreadCount(3);
    }

    QCommandLineParser parser;
    parser.setApplicationDescription(QObject::tr("KeePassXC - cross-platform password manager"));
    parser.addPositionalArgument(
        "filename(s)", QObject::tr("filenames of the password databases to open (*.kdbx)"), "[filename(s)]");

    QCommandLineOption configOption("config", QObject::tr("path to a custom config file"), "config");
    QCommandLineOption localConfigOption(
        "localconfig", QObject::tr("path to a custom local config file"), "localconfig");
    QCommandLineOption lockOption("lock", QObject::tr("lock all open databases"));
    QCommandLineOption keyfileOption("keyfile", QObject::tr("key file of the database"), "keyfile");
    QCommandLineOption pwstdinOption("pw-stdin", QObject::tr("read password of the database from stdin"));
    QCommandLineOption allowScreenCaptureOption("allow-screencapture",
                                                QObject::tr("allow screenshots and app recording (Windows/macOS)"));
    QCommandLineOption preventScreenCaptureOption(
        "prevent-screencapture",
        QObject::tr("exclude the application from screenshots and recordings (Windows/macOS)"));
    QCommandLineOption startMinimized("minimized", QObject::tr("start minimized to the system tray"));
    QCommandLineOption captureRouteOption(
        "capture-route", QObject::tr("open a design-parity capture route (kpxc://capture/<screen>)"), "url");
    QCommandLineOption captureReceiptOption(
        "capture-receipt", QObject::tr("write a JSON readiness receipt for the capture route"), "path");

    QCommandLineOption captureScaleOption(
        "capture-scale", QObject::tr("display scale factor for a capture route, e.g. 1.25"), "factor");

    QCommandLineOption verificationProfileOption(
        "verification-profile", QObject::tr("use isolated standard paths for a capture run"), "identifier");
    QCommandLineOption helpOption = parser.addHelpOption();
    QCommandLineOption versionOption = parser.addVersionOption();
    QCommandLineOption debugInfoOption(QStringList() << "debug-info", QObject::tr("Displays debugging information."));
    parser.addOption(configOption);
    parser.addOption(localConfigOption);
    parser.addOption(lockOption);
    parser.addOption(keyfileOption);
    parser.addOption(pwstdinOption);
    parser.addOption(debugInfoOption);
    parser.addOption(allowScreenCaptureOption);
    parser.addOption(preventScreenCaptureOption);
    parser.addOption(startMinimized);
    parser.addOption(captureRouteOption);
    parser.addOption(captureReceiptOption);
    parser.addOption(captureScaleOption);
    parser.addOption(verificationProfileOption);

    parser.process(applicationArguments);

    // Exit early if we're only showing the help / version
    if (parser.isSet(versionOption) || parser.isSet(helpOption)) {
        return EXIT_SUCCESS;
    }

    // Show debug information and then exit
    if (parser.isSet(debugInfoOption)) {
        QTextStream out(stdout, QIODevice::WriteOnly);
        QString debugInfo = Tools::debugInfo().append("\n").append(Crypto::debugInfo());
        out << debugInfo << Qt::endl;
        return EXIT_SUCCESS;
    }

    // Process config file options early
    if (parser.isSet(configOption) || parser.isSet(localConfigOption)) {
        Config::createConfigFromFile(parser.value(configOption), parser.value(localConfigOption));
    }

    // Capture first-run eligibility before opening a database can populate history.
    Material::DimSum::beginStartup();
    if (!parser.positionalArguments().isEmpty() || parser.isSet(pwstdinOption)
        || parser.isSet(startMinimized) || parser.isSet(captureRouteOption)) {
        Material::DimSum::suppress();
    }

    // Extract file names provided on the command line for opening
    QStringList fileNames;
#ifdef Q_OS_WIN
    // Get correct case for Windows filenames (fixes #7139)
    for (const auto& file : parser.positionalArguments()) {
        const auto fileInfo = QFileInfo(file);
        WIN32_FIND_DATAW findFileData;
        HANDLE hFind;
        const QString absolutePath = fileInfo.absoluteFilePath();
        const wchar_t* absolutePathWchar = reinterpret_cast<const wchar_t*>(absolutePath.utf16());
        hFind = FindFirstFileW(absolutePathWchar, &findFileData);
        if (hFind != INVALID_HANDLE_VALUE) {
            fileNames << QString("%1/%2").arg(fileInfo.absolutePath(), QString::fromWCharArray(findFileData.cFileName));
            FindClose(hFind);
        }
    }
#else
    for (const auto& file : parser.positionalArguments()) {
        if (QFile::exists(file)) {
            fileNames << QDir::toNativeSeparators(file);
        }
    }
#endif

    // Process single instance and early exit if already running
    if (app.isAlreadyRunning()) {
        if (parser.isSet(lockOption)) {
            if (app.sendLockToInstance()) {
                qInfo() << QObject::tr("Databases have been locked.").toUtf8().constData();
            } else {
                qWarning() << QObject::tr("Database failed to lock.").toUtf8().constData();
                return EXIT_FAILURE;
            }
        } else {
            if (!fileNames.isEmpty()) {
                app.sendFileNamesToRunningInstance(fileNames);
            }

            qWarning() << QObject::tr("Another instance of KeePassXC is already running.").toUtf8().constData();
        }
        return EXIT_SUCCESS;
    }

    if (parser.isSet(lockOption)) {
        qWarning() << QObject::tr("KeePassXC is not running. No open database to lock").toUtf8().constData();

        // still return with EXIT_SUCCESS because when used within a script for ensuring that there is no unlocked
        // keepass database (e.g. screen locking) we can consider it as successful
        return EXIT_SUCCESS;
    }

    if (!Crypto::init()) {
        QString error = QObject::tr("Fatal error while testing the cryptographic functions.");
        error.append("\n");
        error.append(Crypto::errorString());
        MessageBox::critical(nullptr, QObject::tr("KeePassXC - Error"), error);
        return EXIT_FAILURE;
    }

    Utils::setDefaultTextStreams();

    // Apply the configured theme before creating any GUI elements
    app.applyTheme();

    Application::bootstrap(config()->get(Config::GUI_Language).toString());

    MainWindow mainWindow;
#ifdef Q_OS_WIN
    // Qt Hack - Prevent white flicker when showing window
    mainWindow.setProperty("windowOpacity", 0.0);
#endif

    // Screen capture is allowed unless the user turned it off (View > Allow
    // Screen Capture is remembered) or asked on the command line. A flag on
    // the command line wins over the remembered choice for this run only.
    bool allowScreenCapture = config()->get(Config::GUI_AllowScreenCapture).toBool();
    if (parser.isSet(preventScreenCaptureOption)) {
        allowScreenCapture = false;
    } else if (parser.isSet(allowScreenCaptureOption)) {
        allowScreenCapture = true;
    }
    mainWindow.setAllowScreenCapture(allowScreenCapture, /*persist=*/false);

    const bool pwstdin = parser.isSet(pwstdinOption);
    for (const QString& filename : fileNames) {
        QString password;
        if (pwstdin) {
            // we always need consume a line of STDIN if --pw-stdin is set to clear out the
            // buffer for native messaging, even if the specified file does not exist
            QTextStream out(stdout, QIODevice::WriteOnly);
            out << QObject::tr("Database password: ") << Qt::flush;
            password = Utils::getPassword();
        }
        mainWindow.openDatabase(filename, password, parser.value(keyfileOption));
    }

    // start minimized if configured
    if (parser.isSet(startMinimized) || config()->get(Config::GUI_MinimizeOnStartup).toBool()) {
        mainWindow.hideWindow();
    } else {
        mainWindow.bringToFront();
        Application::processEvents();
        if (parser.isSet(captureRouteOption)) {
            Material::CaptureRoute::Request request;
            request.receiptPath = parser.value(captureReceiptOption);
            QString error;
            if (!Material::CaptureRoute::parse(parser.value(captureRouteOption), request, &error)) {
                QTextStream err(stderr, QIODevice::WriteOnly);
                err << error << Qt::endl;
                return EXIT_FAILURE;
            }
            Material::CaptureRoute::schedule(&mainWindow, request);
        } else {
            Material::DimSum::showIfDue(&mainWindow);
        }
    }

    int exitCode = Application::exec();

    // Check if restart was requested
    if (exitCode == RESTART_EXITCODE) {
#ifdef KPXC_FEATURE_UPDATES
        const bool updateRestart = updateCheck()->state() == UpdateChecker::State::ReadyToRestart
                                   || updateCheck()->state() == UpdateChecker::State::Deferred;
        if (!updateRestart || !updateCheck()->launchUpdatedVersion()) {
            QProcess::startDetached(QCoreApplication::applicationFilePath(), {});
        }
#else
        QProcess::startDetached(QCoreApplication::applicationFilePath(), {});
#endif
    }

#if defined(WITH_ASAN) && defined(WITH_LSAN)
    // do leak check here to prevent massive tail of end-of-process leak errors from third-party libraries
    __lsan_do_leak_check();
    __lsan_disable();
#endif

    Utils::resetTextStreams();

    return exitCode;
}
