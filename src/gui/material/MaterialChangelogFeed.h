/*
 *  Copyright (C) 2026 KeePassXC Team <team@keepassxc.org>
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

#ifndef KEEPASSXC_MATERIALCHANGELOGFEED_H
#define KEEPASSXC_MATERIALCHANGELOGFEED_H

#include "MaterialChangelogScreen.h"

#include <QObject>
#include <QString>
#include <QVector>

namespace Material
{
    /**
     * What fills the changelog destination.
     *
     * Parses the CHANGELOG.md shipped with the build - every released version,
     * not only the newest - into release cards: the version and its date from
     * the `## 2.7.12 (2026-03-10)` headings, and every bullet underneath tagged
     * with the `### Fixes` style section it sits in. Versions that predate the
     * sectioned format keep their bullets under a plain note tag.
     *
     * Nothing is invented. A version whose section holds no bullets is shown
     * saying exactly that, and the version this binary reports is marked so the
     * list cannot imply you are running something you are not.
     */
    class ChangelogFeed : public QObject
    {
        Q_OBJECT

    public:
        explicit ChangelogFeed(ChangelogScreen* screen, QObject* parent = nullptr);
        ~ChangelogFeed() override;

        /** Parse @p markdown into releases, newest first as the file lists them. */
        static QVector<Release> parse(const QString& markdown);

        /** The releases currently on screen, before the search filter. */
        QVector<Release> releases() const;

    private:
        QVector<Release> filtered() const;
        QString markdown() const;
        void exportMarkdown();

        ChangelogScreen* m_screen = nullptr;
        QVector<Release> m_releases;
    };

} // namespace Material

#endif // KEEPASSXC_MATERIALCHANGELOGFEED_H
