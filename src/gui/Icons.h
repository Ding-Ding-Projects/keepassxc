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

#ifndef KEEPASSX_ICONS_H
#define KEEPASSX_ICONS_H

#include <QIcon>

#include <core/Database.h>
#include <gui/DatabaseIcons.h>

class Icons
{
public:
    QString applicationIconName();
    QIcon applicationIcon();
    /**
     * Validate and import a local PNG or JPEG as the presentation logo. The source path is never
     * persisted: the accepted pixels are normalised into the private app-data cache instead.
     */
    bool importApplicationLogo(const QString& sourcePath, QString* error = nullptr);
    /** Rebuild the current derived display icon using the stored presentation settings. */
    bool refreshApplicationLogo(QString* error = nullptr);
    /** Stage fit/background rendering and commit both settings only after a successful rebuild. */
    bool setApplicationLogoPresentation(const QString& fitMode, const QColor& background, QString* error = nullptr);
    /** Remove the local derived logo and restore the shipped application mark. */
    bool resetApplicationLogo(QString* error = nullptr);
    bool hasCustomApplicationLogo() const;
    QString applicationLogoPath() const;
    // Test-only fault controls exercise transactional storage failures without using real personal files.
    static void setApplicationLogoCacheDirectoryForTests(const QString& path);
    static void setApplicationLogoFailureStageForTests(int stage);
    QIcon trayIcon(bool unlocked = true);
    QString trayIconAppearance() const;
    QIcon icon(const QString& name, bool recolor = true, const QColor& overrideColor = QColor::Invalid);
    QIcon onOffIcon(const QString& name, bool on, bool recolor = true);

    static QPixmap customIconPixmap(const Database* db, const QUuid& uuid, IconSize size = IconSize::Default);
    static QHash<QUuid, QPixmap> customIconsPixmaps(const Database* db, IconSize size = IconSize::Default);
    static QPixmap entryIconPixmap(const Entry* entry, IconSize size = IconSize::Default);
    static QPixmap groupIconPixmap(const Group* group, IconSize size = IconSize::Default);

    static QByteArray saveToBytes(const QImage& image);
    static QString imageFormatsFilter();

    static Icons* instance();

private:
    Icons();

    QIcon customApplicationIcon() const;
    QString applicationLogoSourcePath() const;
    QString applicationLogoCacheDirectory() const;
    bool ensureApplicationLogoCache(QString* error) const;
    bool renderApplicationLogo(const QImage& source, const QString& fitMode, const QColor& background, QString* error);
    void refreshApplicationIcon();

    static Icons* m_instance;
    static QString m_testLogoCacheDirectory;
    static int m_testLogoFailureStage;

    QHash<QString, QIcon> m_iconCache;

    Q_DISABLE_COPY(Icons)
};

inline Icons* icons()
{
    return Icons::instance();
}

#endif // KEEPASSX_ICONS_H
