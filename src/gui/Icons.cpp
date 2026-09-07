/*
 *  Copyright (C) 2020 KeePassXC Team <team@keepassxc.org>
 *  Copyright (C) 2011 Felix Geyer <debfx@fobos.de>
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

#include "Icons.h"

#include <QBuffer>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIconEngine>
#include <QImageReader>
#include <QPaintDevice>
#include <QPainter>
#include <QSaveFile>
#include <QStandardPaths>
#include <QWidget>

#include <algorithm>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#include "config-keepassx.h"
#include "core/Config.h"
#include "core/Database.h"
#include "gui/DatabaseIcons.h"
#include "gui/MainWindow.h"
#include "gui/osutils/OSUtils.h"
#include "keeshare/KeeShare.h"

class AdaptiveIconEngine : public QIconEngine
{
public:
    explicit AdaptiveIconEngine(QIcon baseIcon, QColor overrideColor = {});
    void paint(QPainter* painter, const QRect& rect, QIcon::Mode mode, QIcon::State state) override;
    QPixmap pixmap(const QSize& size, QIcon::Mode mode, QIcon::State state) override;
    QIconEngine* clone() const override;

private:
    QIcon m_baseIcon;
    QColor m_overrideColor;
};

Icons* Icons::m_instance(nullptr);
QString Icons::m_testLogoCacheDirectory;
int Icons::m_testLogoFailureStage = 0;

Icons::Icons() = default;

QString Icons::applicationIconName()
{
    return "keepassxc";
}

QIcon Icons::applicationIcon()
{
    const auto customIcon = customApplicationIcon();
    if (!customIcon.isNull()) {
        return customIcon;
    }
    return icon(applicationIconName(), false);
}

namespace
{
    constexpr qint64 CustomLogoMaximumBytes = 5 * 1024 * 1024;
    constexpr int CustomLogoMaximumDimension = 4096;
    constexpr qint64 CustomLogoMaximumPixels = 16LL * 1024 * 1024;

    bool isAllowedLogoFormat(const QByteArray& format)
    {
        return format.compare("png", Qt::CaseInsensitive) == 0 || format.compare("jpeg", Qt::CaseInsensitive) == 0
               || format.compare("jpg", Qt::CaseInsensitive) == 0;
    }

    bool isReparseOrLink(const QString& path)
    {
        const QFileInfo info(path);
        if (info.isSymLink()) return true;
#ifdef Q_OS_WIN
        const auto attributes = GetFileAttributesW(reinterpret_cast<LPCWSTR>(path.utf16()));
        return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT);
#else
        return false;
#endif
    }

    bool isContainedPath(const QString& child, const QString& parent)
    {
        const auto cleanChild = QDir::cleanPath(child);
        const auto cleanParent = QDir::cleanPath(parent);
#ifdef Q_OS_WIN
        return cleanChild.startsWith(cleanParent + QLatin1Char('/'), Qt::CaseInsensitive);
#else
        return cleanChild.startsWith(cleanParent + QLatin1Char('/'));
#endif
    }
}

QString Icons::applicationLogoCacheDirectory() const
{
    if (!m_testLogoCacheDirectory.isEmpty()) return m_testLogoCacheDirectory;
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath(QStringLiteral("logos"));
}

void Icons::setApplicationLogoCacheDirectoryForTests(const QString& path)
{
    m_testLogoCacheDirectory = path;
}

void Icons::setApplicationLogoFailureStageForTests(int stage)
{
    m_testLogoFailureStage = stage;
}

QString Icons::applicationLogoPath() const
{
    return QDir(applicationLogoCacheDirectory()).filePath(QStringLiteral("application-logo.png"));
}

QString Icons::applicationLogoSourcePath() const
{
    return QDir(applicationLogoCacheDirectory()).filePath(QStringLiteral("application-logo-source.png"));
}

bool Icons::ensureApplicationLogoCache(QString* error) const
{
    auto fail = [error](const QString& message) {
        if (error) *error = message;
        return false;
    };
    const auto appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (appData.isEmpty() || (m_testLogoCacheDirectory.isEmpty() && isReparseOrLink(appData))) {
        return fail(QStringLiteral("The private logo cache location is unavailable or linked."));
    }
    const auto cache = applicationLogoCacheDirectory();
    if (QFileInfo::exists(cache) && isReparseOrLink(cache)) {
        return fail(QStringLiteral("The private logo cache directory is linked and cannot be used."));
    }
    if (!QDir().mkpath(cache) || isReparseOrLink(cache)) {
        return fail(QStringLiteral("The private logo cache could not be created safely."));
    }
    const auto canonicalCache = QFileInfo(cache).canonicalFilePath();
    if (canonicalCache.isEmpty() || !isContainedPath(applicationLogoPath(), canonicalCache)
        || !isContainedPath(applicationLogoSourcePath(), canonicalCache)) {
        return fail(QStringLiteral("The private logo cache path failed containment validation."));
    }
    const QStringList entries{
        applicationLogoPath(), applicationLogoSourcePath(),
        QDir(cache).filePath(QStringLiteral("application-logo.pending.png")),
        QDir(cache).filePath(QStringLiteral("application-logo-source.pending.png")),
        applicationLogoPath() + QStringLiteral(".previous"), applicationLogoSourcePath() + QStringLiteral(".previous"),
        applicationLogoPath() + QStringLiteral(".removing"), applicationLogoSourcePath() + QStringLiteral(".removing")};
    for (const auto& entry : entries) {
        if (!isContainedPath(entry, canonicalCache) || isReparseOrLink(entry)) {
            return fail(QStringLiteral("A private logo cache entry is linked or outside the validated cache."));
        }
    }
    return true;
}

bool Icons::hasCustomApplicationLogo() const
{
    return config()->get(Config::GUI_CustomLogoEnabled).toBool() && !isReparseOrLink(applicationLogoPath())
        && QFileInfo(applicationLogoPath()).isFile();
}

QIcon Icons::customApplicationIcon() const
{
    if (!hasCustomApplicationLogo()) {
        return {};
    }
    QImageReader reader(applicationLogoPath());
    reader.setAutoTransform(true);
    const auto image = reader.read();
    return image.isNull() ? QIcon() : QIcon(QPixmap::fromImage(image));
}

bool Icons::importApplicationLogo(const QString& sourcePath, QString* error)
{
    auto fail = [error](const QString& message) {
        if (error) *error = message;
        return false;
    };

    QFile inputFile(sourcePath);
    if (!inputFile.exists() || !inputFile.open(QIODevice::ReadOnly)) {
        return fail(QStringLiteral("The selected logo cannot be read."));
    }
    if (inputFile.size() <= 0 || inputFile.size() > CustomLogoMaximumBytes) {
        return fail(QStringLiteral("The selected logo must be between 1 byte and 5 MiB."));
    }

    QImageReader reader(&inputFile);
    reader.setAutoTransform(true);
    const auto format = reader.format();
    if (!isAllowedLogoFormat(format)) {
        return fail(QStringLiteral("Choose a PNG or JPEG logo. The file contents did not match a supported logo format."));
    }
    if (reader.supportsAnimation() && reader.imageCount() != 1) {
        return fail(QStringLiteral("Animated images cannot be used as an application logo."));
    }
    const auto size = reader.size();
    if (!size.isValid() || size.width() > CustomLogoMaximumDimension || size.height() > CustomLogoMaximumDimension
        || qint64(size.width()) * size.height() > CustomLogoMaximumPixels) {
        return fail(QStringLiteral("The selected logo exceeds the 4096 px or 16 megapixel safety limit."));
    }
    const auto image = reader.read();
    if (image.isNull()) {
        return fail(QStringLiteral("The selected logo is malformed or could not be decoded."));
    }

    if (!ensureApplicationLogoCache(error)) return false;

    const auto stagedSource = QDir(applicationLogoCacheDirectory()).filePath(QStringLiteral("application-logo-source.pending.png"));
    QSaveFile output(stagedSource);
    if (!output.open(QIODevice::WriteOnly) || !image.save(&output, "PNG") || !output.commit()) {
        return fail(QStringLiteral("The selected logo could not be converted into the private source staging cache."));
    }
    if (!renderApplicationLogo(image,
                                config()->get(Config::GUI_CustomLogoFitMode).toString(),
                                QColor(config()->get(Config::GUI_CustomLogoBackground).toString()),
                                error)) {
        QFile::remove(stagedSource);
        return false;
    }

    if (m_testLogoFailureStage == 1) {
        QFile::remove(stagedSource);
        QFile::remove(QDir(applicationLogoCacheDirectory()).filePath(QStringLiteral("application-logo.pending.png")));
        return fail(QStringLiteral("The selected logo could not complete its second private cache write."));
    }

    const auto source = applicationLogoSourcePath();
    const auto display = applicationLogoPath();
    const auto sourceBackup = source + QStringLiteral(".previous");
    const auto displayBackup = display + QStringLiteral(".previous");
    const auto stagedDisplay = QDir(applicationLogoCacheDirectory()).filePath(QStringLiteral("application-logo.pending.png"));
    if ((QFileInfo::exists(sourceBackup) && !QFile::remove(sourceBackup))
        || (QFileInfo::exists(displayBackup) && !QFile::remove(displayBackup))) {
        return fail(QStringLiteral("A stale private logo rollback file could not be removed safely."));
    }
    const bool sourceBackedUp = !QFileInfo::exists(source) || QFile::rename(source, sourceBackup);
    const bool sourceActivated = sourceBackedUp && QFile::rename(stagedSource, source);
    const bool displayBackedUp = sourceActivated && (!QFileInfo::exists(display) || QFile::rename(display, displayBackup));
    const bool displayActivated = displayBackedUp && m_testLogoFailureStage != 7 && m_testLogoFailureStage != 8
        && QFile::rename(stagedDisplay, display);
    if (!displayActivated) {
        bool rollbackOk = true;
        if (sourceActivated) {
            rollbackOk = QFile::remove(source) && (!QFileInfo::exists(sourceBackup)
                                                    || (m_testLogoFailureStage != 8 && QFile::rename(sourceBackup, source)));
        }
        if (displayBackedUp && QFileInfo::exists(displayBackup)) rollbackOk = QFile::rename(displayBackup, display) && rollbackOk;
        const bool stageSourceRemoved = !QFileInfo::exists(stagedSource) || QFile::remove(stagedSource);
        const bool stageDisplayRemoved = !QFileInfo::exists(stagedDisplay) || QFile::remove(stagedDisplay);
        return fail(rollbackOk && stageSourceRemoved && stageDisplayRemoved
                        ? QStringLiteral("The new logo could not replace the active private cache; the previous logo remains active.")
                        : QStringLiteral("The new logo could not replace the active cache and rollback left residual private data."));
    }
    config()->set(Config::GUI_CustomLogoEnabled, true);
    refreshApplicationIcon();
    if (m_testLogoFailureStage == 6 || (QFileInfo::exists(sourceBackup) && !QFile::remove(sourceBackup))
        || (QFileInfo::exists(displayBackup) && !QFile::remove(displayBackup))) {
        if (error) *error = QStringLiteral("The new logo is active, but private rollback cleanup left residual data. Retry the replacement or reset.");
    }
    return true;
}

bool Icons::refreshApplicationLogo(QString* error)
{
    return setApplicationLogoPresentation(config()->get(Config::GUI_CustomLogoFitMode).toString(),
                                          QColor(config()->get(Config::GUI_CustomLogoBackground).toString()), error);
}

bool Icons::setApplicationLogoPresentation(const QString& fitMode, const QColor& background, QString* error)
{
    auto fail = [error](const QString& message) { if (error) *error = message; return false; };
    if (fitMode != QLatin1String("fit") && fitMode != QLatin1String("crop")) return fail(QStringLiteral("The logo fit mode is invalid."));
    if (!background.isValid() || !ensureApplicationLogoCache(error)) return false;
    QImage source(applicationLogoSourcePath());
    if (source.isNull()) {
        return fail(QStringLiteral("The private source image is unavailable. Choose a logo again."));
    }
    if (!renderApplicationLogo(source, fitMode, background, error)) return false;
    const auto staged = QDir(applicationLogoCacheDirectory()).filePath(QStringLiteral("application-logo.pending.png"));
    const auto active = applicationLogoPath();
    const auto backup = active + QStringLiteral(".previous");
    if (m_testLogoFailureStage == 4) {
        QFile::remove(staged);
        return fail(QStringLiteral("The updated logo could not replace the active cache; settings were not changed."));
    }
    if (QFileInfo::exists(backup) && !QFile::remove(backup)) {
        QFile::remove(staged);
        return fail(QStringLiteral("A stale private logo rollback file could not be removed safely."));
    }
    if (QFileInfo::exists(active) && !QFile::rename(active, backup)) {
        QFile::remove(staged);
        return fail(QStringLiteral("The active logo cache could not be staged for replacement."));
    }
    if (!QFile::rename(staged, active)) {
        const bool restored = !QFileInfo::exists(backup) || QFile::rename(backup, active);
        return fail(restored ? QStringLiteral("The updated logo could not replace the active cache; settings were not changed.")
                             : QStringLiteral("The updated logo could not replace the active cache and rollback left residual private data."));
    }
    config()->set(Config::GUI_CustomLogoFitMode, fitMode);
    config()->set(Config::GUI_CustomLogoBackground, background.name(QColor::HexArgb));
    refreshApplicationIcon();
    if (m_testLogoFailureStage == 6 || (QFileInfo::exists(backup) && !QFile::remove(backup))) {
        if (error) *error = QStringLiteral("The updated logo is active, but private rollback cleanup left residual data. Retry the replacement or reset.");
    }
    return true;
}

bool Icons::renderApplicationLogo(const QImage& source, const QString& fitMode, const QColor& background, QString* error)
{
    auto fail = [error](const QString& message) { if (error) *error = message; return false; };
    const int edge = qMax(source.width(), source.height());
    QImage canvas(edge, edge, QImage::Format_ARGB32_Premultiplied);
    canvas.fill(background.rgba());
    QPainter painter(&canvas);
    const bool crop = fitMode == QLatin1String("crop");
    const auto scaled = source.scaled(edge, edge, crop ? Qt::KeepAspectRatioByExpanding : Qt::KeepAspectRatio,
                                      Qt::SmoothTransformation);
    painter.drawImage((edge - scaled.width()) / 2, (edge - scaled.height()) / 2, scaled);
    painter.end();

    const auto stagedDisplay = QDir(applicationLogoCacheDirectory()).filePath(QStringLiteral("application-logo.pending.png"));
    QFile::remove(stagedDisplay);
    if (m_testLogoFailureStage == 2) return fail(QStringLiteral("The selected logo could not complete its second private cache write."));
    QSaveFile output(stagedDisplay);
    if (!output.open(QIODevice::WriteOnly) || !canvas.save(&output, "PNG") || !output.commit()) {
        return fail(QStringLiteral("The selected logo could not be converted into the private display cache."));
    }
    return true;
}

bool Icons::resetApplicationLogo(QString* error)
{
    auto fail = [error](const QString& message) { if (error) *error = message; return false; };
    if (!ensureApplicationLogoCache(error)) return false;
    const auto display = applicationLogoPath();
    const auto source = applicationLogoSourcePath();
    const auto displayBackup = display + QStringLiteral(".removing");
    const auto sourceBackup = source + QStringLiteral(".removing");
    if (QFileInfo::exists(displayBackup) || QFileInfo::exists(sourceBackup)) {
        if (QFileInfo::exists(display) || QFileInfo::exists(source)) {
            return fail(QStringLiteral("A prior logo reset left mixed active and residual private data."));
        }
        const bool displayRemoved = !QFileInfo::exists(displayBackup) || QFile::remove(displayBackup);
        const bool sourceRemoved = !QFileInfo::exists(sourceBackup) || QFile::remove(sourceBackup);
        if (!displayRemoved || !sourceRemoved || QFileInfo::exists(displayBackup) || QFileInfo::exists(sourceBackup)) {
            return fail(QStringLiteral("Residual private logo data could not be removed. Cleanup can be retried."));
        }
        config()->set(Config::GUI_CustomLogoEnabled, false);
        refreshApplicationIcon();
        return true;
    }
    if (QFileInfo::exists(display) && !QFile::rename(display, displayBackup)) {
        return fail(QStringLiteral("The active display logo could not be staged for removal."));
    }
    if (QFileInfo::exists(source) && !QFile::rename(source, sourceBackup)) {
        const bool restored = !QFileInfo::exists(displayBackup) || QFile::rename(displayBackup, display);
        return fail(restored ? QStringLiteral("The source logo could not be staged; the active logo was restored.")
                             : QStringLiteral("The source logo could not be staged and display-logo rollback failed; residual private data remains."));
    }
    if (m_testLogoFailureStage == 3) {
        const bool sourceRestored = !QFileInfo::exists(sourceBackup) || QFile::rename(sourceBackup, source);
        const bool displayRestored = !QFileInfo::exists(displayBackup) || QFile::rename(displayBackup, display);
        return fail(sourceRestored && displayRestored
                        ? QStringLiteral("The custom logo could not be removed; the active logo was restored.")
                        : QStringLiteral("The custom logo could not be removed and rollback failed; residual private data remains."));
    }
    if (QFileInfo::exists(sourceBackup) && !QFile::remove(sourceBackup)) {
        const bool sourceRestored = QFile::rename(sourceBackup, source);
        const bool displayRestored = !QFileInfo::exists(displayBackup) || QFile::rename(displayBackup, display);
        return fail(sourceRestored && displayRestored
                        ? QStringLiteral("The source logo could not be removed; the active logo was restored.")
                        : QStringLiteral("The source logo could not be removed and rollback failed; residual private data remains."));
    }
    if (m_testLogoFailureStage == 5 || (QFileInfo::exists(displayBackup) && !QFile::remove(displayBackup))) {
        // The active display has already been removed. Do not claim a successful reset, but do
        // disable the custom presentation and retain the remaining private entry for a later cleanup.
        config()->set(Config::GUI_CustomLogoEnabled, false);
        refreshApplicationIcon();
        return fail(QStringLiteral("The custom logo was only partly removed; residual private data remains and cleanup can be retried."));
    }
    config()->set(Config::GUI_CustomLogoEnabled, false);
    refreshApplicationIcon();
    return true;
}

void Icons::refreshApplicationIcon()
{
    m_iconCache.clear();
    const auto refreshed = applicationIcon();
    QApplication::setWindowIcon(refreshed);
    for (auto* widget : QApplication::topLevelWidgets()) {
        if (widget) widget->setWindowIcon(refreshed);
    }
}

QString Icons::trayIconAppearance() const
{
    auto iconAppearance = config()->get(Config::GUI_TrayIconAppearance).toString();
    if (iconAppearance.isNull()) {
#ifdef Q_OS_MACOS
        iconAppearance = osUtils->isDarkMode() ? "monochrome-light" : "monochrome-dark";
#else
        iconAppearance = "monochrome-light";
#endif
    }
    return iconAppearance;
}

QIcon Icons::trayIcon(bool unlocked)
{
    QString suffix;
    if (!unlocked) {
        suffix = "-locked";
    }

    auto iconAppearance = trayIconAppearance();
    if (!iconAppearance.startsWith("monochrome")) {
        return icon(QString("%1%2").arg(applicationIconName(), suffix), false);
    }

    QIcon i;
#if defined(Q_OS_WIN)
    if (osUtils->isStatusBarDark()) {
        i = icon(QString("keepassxc-monochrome-light%1").arg(suffix), false);
    } else {
        i = icon(QString("keepassxc-monochrome-dark%1").arg(suffix), false);
    }
#elif defined(Q_OS_MACOS)
    i = icon(QString("keepassxc-monochrome-light%1").arg(suffix), false);
#else
    i = icon(QString("%1-%2%3").arg(applicationIconName(), iconAppearance, suffix), false);
#endif
    // Set as mask to allow the operating system to recolour the tray icon. This may look weird
    // if we failed to detect the status bar background colour correctly, but it is certainly
    // better than a barely visible icon and even if we did guess correctly, it allows for better
    // integration should the system's preferred colours not be 100% black or white.
    i.setIsMask(true);
    return i;
}

AdaptiveIconEngine::AdaptiveIconEngine(QIcon baseIcon, QColor overrideColor)
    : QIconEngine()
    , m_baseIcon(std::move(baseIcon))
    , m_overrideColor(overrideColor)
{
}

void AdaptiveIconEngine::paint(QPainter* painter, const QRect& rect, QIcon::Mode mode, QIcon::State state)
{
    // Temporary image canvas to ensure that the background is transparent and alpha blending works.
    auto scale = painter->device()->devicePixelRatioF();
    QImage img(rect.size() * scale, QImage::Format_ARGB32_Premultiplied);
    img.fill(0);
    QPainter p(&img);

    m_baseIcon.paint(&p, img.rect(), Qt::AlignCenter, mode, state);

    if (m_overrideColor.isValid()) {
        p.setCompositionMode(QPainter::CompositionMode_SourceIn);
        p.fillRect(img.rect(), m_overrideColor);
    } else if (getMainWindow()) {
        QPalette palette = getMainWindow()->palette();
        p.setCompositionMode(QPainter::CompositionMode_SourceIn);

        if (mode == QIcon::Active) {
            p.fillRect(img.rect(), palette.color(QPalette::Active, QPalette::ButtonText));
        } else if (mode == QIcon::Selected) {
            p.fillRect(img.rect(), palette.color(QPalette::Active, QPalette::HighlightedText));
        } else if (mode == QIcon::Disabled) {
            p.fillRect(img.rect(), palette.color(QPalette::Disabled, QPalette::WindowText));
        } else {
            p.fillRect(img.rect(), palette.color(QPalette::Normal, QPalette::WindowText));
        }
    }

    painter->drawImage(rect, img);
}

QPixmap AdaptiveIconEngine::pixmap(const QSize& size, QIcon::Mode mode, QIcon::State state)
{
    QImage img(size, QImage::Format_ARGB32_Premultiplied);
    img.fill(0);
    QPainter painter(&img);
    paint(&painter, QRect(0, 0, size.width(), size.height()), mode, state);
    return QPixmap::fromImage(img, Qt::ImageConversionFlag::NoFormatConversion);
}

QIconEngine* AdaptiveIconEngine::clone() const
{
    return new AdaptiveIconEngine(m_baseIcon);
}

QIcon Icons::icon(const QString& name, bool recolor, const QColor& overrideColor)
{
    QString cacheName =
        QString("%1:%2:%3").arg(recolor ? "1" : "0", overrideColor.isValid() ? overrideColor.name() : "#", name);
    QIcon icon = m_iconCache.value(cacheName);

    if (!icon.isNull() && !overrideColor.isValid()) {
        return icon;
    }

    icon = QIcon::fromTheme(name);
    if (recolor) {
        icon = QIcon(new AdaptiveIconEngine(icon, overrideColor));
        icon.setIsMask(true);
    }

    m_iconCache.insert(cacheName, icon);
    return icon;
}

QIcon Icons::onOffIcon(const QString& name, bool on, bool recolor)
{
    return icon(name + (on ? "-on" : "-off"), recolor);
}

Icons* Icons::instance()
{
    if (!m_instance) {
        m_instance = new Icons();

        Q_INIT_RESOURCE(icons);
        QIcon::setThemeSearchPaths(QStringList{":/icons"} << QIcon::themeSearchPaths());
        QIcon::setThemeName("application");
    }

    return m_instance;
}

QPixmap Icons::customIconPixmap(const Database* db, const QUuid& uuid, IconSize size)
{
    if (!db->metadata()->hasCustomIcon(uuid)) {
        return {};
    }
    // Generate QIcon with pre-baked resolutions
    auto icon = QImage::fromData(db->metadata()->customIcon(uuid).data);
    auto basePixmap = QPixmap::fromImage(icon.scaled(64, 64, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    return QIcon(basePixmap).pixmap(databaseIcons()->iconSize(size));
}

QHash<QUuid, QPixmap> Icons::customIconsPixmaps(const Database* db, IconSize size)
{
    QHash<QUuid, QPixmap> result;

    for (const QUuid& uuid : db->metadata()->customIconsOrder()) {
        result.insert(uuid, Icons::customIconPixmap(db, uuid, size));
    }

    return result;
}

QPixmap Icons::entryIconPixmap(const Entry* entry, IconSize size)
{
    QPixmap icon(size, size);
    if (entry->iconUuid().isNull()) {
        icon = databaseIcons()->icon(entry->iconNumber(), size);
    } else {
        if (entry->database()) {
            icon = Icons::customIconPixmap(entry->database(), entry->iconUuid(), size);
        }
    }

    if (entry->isExpired()) {
        icon = databaseIcons()->applyBadge(icon, DatabaseIcons::Badges::Expired);
    }

    return icon;
}

QPixmap Icons::groupIconPixmap(const Group* group, IconSize size)
{
    QPixmap icon(size, size);
    if (group->iconUuid().isNull()) {
        icon = databaseIcons()->icon(group->iconNumber(), size);
    } else {
        if (group->database()) {
            icon = Icons::customIconPixmap(group->database(), group->iconUuid(), size);
        }
    }

    if (group->isExpired()) {
        icon = databaseIcons()->applyBadge(icon, DatabaseIcons::Badges::Expired);
    } else if (KeeShare::isShared(group)) {
        icon = KeeShare::indicatorBadge(group, icon);
    }

    return icon;
}

QString Icons::imageFormatsFilter()
{
    const QList<QByteArray> formats = QImageReader::supportedImageFormats();
    QStringList formatsStringList;

    for (const QByteArray& format : formats) {
        if (std::all_of(format.cbegin(), format.cend(), [](char codePoint) -> bool {
                return QChar(codePoint).isLetterOrNumber();
            })) {
            formatsStringList.append("*." + QString::fromLatin1(format).toLower());
        }
    }

    return formatsStringList.join(" ");
}

QByteArray Icons::saveToBytes(const QImage& image)
{
    QByteArray ba;
    QBuffer buffer(&ba);
    buffer.open(QIODevice::WriteOnly);
    // TODO: check !icon.save()
    image.save(&buffer, "PNG");
    buffer.close();
    return ba;
}
