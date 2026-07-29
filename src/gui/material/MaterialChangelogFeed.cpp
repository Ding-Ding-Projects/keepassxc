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

#include "MaterialChangelogFeed.h"

#include "MaterialNotifier.h"
#include "MaterialSearchBar.h"

#include "config-keepassx.h"
#include "core/Clock.h"
#include "gui/FileDialog.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QStringList>
#include <QTextStream>

namespace Material
{
    namespace
    {
        const QString ChangelogResource = QStringLiteral(":/docs/CHANGELOG.md");
        const QString ExportDirectoryRole = QStringLiteral("changelog");

        /** `## 2.7.12 (2026-03-10)` - the version and whatever is in the brackets. */
        const QRegularExpression& releaseHeading()
        {
            static const QRegularExpression expression(QStringLiteral("^(.+?)\\s*\\(([^()]*)\\)\\s*$"));
            return expression;
        }

        bool isIsoDate(const QString& text)
        {
            static const QRegularExpression expression(QStringLiteral("^\\d{4}-\\d{2}-\\d{2}$"));
            return expression.match(text).hasMatch();
        }

        /** Turn a `### Fixes` style section name into the tag pill of its bullets. */
        void tagForSection(const QString& section, QString* tag, PillKind* tint)
        {
            const QString name = section.toLower();
            if (name.contains(QLatin1String("add"))) {
                *tag = ChangelogFeed::tr("added");
                *tint = PillKind::Value;
            } else if (name.contains(QLatin1String("change"))) {
                *tag = ChangelogFeed::tr("changed");
                *tint = PillKind::Warn;
            } else if (name.contains(QLatin1String("fix"))) {
                *tag = ChangelogFeed::tr("fixed");
                *tint = PillKind::Good;
            } else {
                *tag = section.toLower();
                *tint = PillKind::Value;
            }
        }

        bool matches(const ChangeItem& item, const QString& query)
        {
            return item.tag.contains(query, Qt::CaseInsensitive) || item.text.contains(query, Qt::CaseInsensitive);
        }
    } // namespace

    ChangelogFeed::ChangelogFeed(ChangelogScreen* screen, QObject* parent)
        : QObject(parent)
        , m_screen(screen)
    {
        Q_ASSERT(m_screen);

        m_screen->setSupportingText(tr("Every release recorded in the CHANGELOG.md this build was made from, oldest "
                                       "entry to newest. The search box filters versions and change text; the export "
                                       "button writes out whatever the filter leaves."));
        m_screen->searchBar()->setShowRegexControls(false);

        QFile file(ChangelogResource);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            m_releases = parse(QString::fromUtf8(file.readAll()));
            file.close();
        }

        if (m_releases.isEmpty()) {
            Release missing;
            missing.version = QString::fromLatin1(KEEPASSXC_VERSION);
            missing.status = tr("No changelog");
            missing.statusTint = PillKind::Warn;
            missing.items.append({tr("note"),
                                  tr("This build does not carry a readable CHANGELOG.md, so there is nothing to "
                                     "list here."),
                                  PillKind::Off});
            m_releases.append(missing);
        }

        m_screen->setReleases(m_releases);
        connect(m_screen, &ChangelogScreen::exportRequested, this, &ChangelogFeed::exportMarkdown);
    }

    ChangelogFeed::~ChangelogFeed() = default;

    QVector<Release> ChangelogFeed::releases() const
    {
        return m_releases;
    }

    QVector<Release> ChangelogFeed::parse(const QString& markdown)
    {
        QVector<Release> releases;
        Release current;
        bool inRelease = false;
        QString tag = tr("note");
        PillKind tint = PillKind::Value;
        QString newestDated;

        const auto flush = [&releases, &current, &inRelease] {
            if (!inRelease) {
                return;
            }
            if (current.items.isEmpty()) {
                current.items.append({tr("none"),
                                      tr("No changes are recorded for this version in the changelog."),
                                      PillKind::Off});
            }
            releases.append(current);
            inRelease = false;
        };

        const QStringList lines = markdown.split(QLatin1Char('\n'));
        for (const QString& raw : lines) {
            const QString line = raw.trimmed();

            if (line.startsWith(QLatin1String("## "))) {
                flush();

                current = Release();
                inRelease = true;
                tag = tr("note");
                tint = PillKind::Value;

                const QString heading = line.mid(3).trimmed();
                const auto match = releaseHeading().match(heading);
                QString bracketed;
                if (match.hasMatch()) {
                    current.version = match.captured(1).trimmed();
                    bracketed = match.captured(2).trimmed();
                } else {
                    current.version = heading;
                }

                QStringList marks;
                if (isIsoDate(bracketed)) {
                    current.date = bracketed;
                    if (newestDated.isEmpty()) {
                        newestDated = current.version;
                    }
                } else if (!bracketed.isEmpty()) {
                    marks << bracketed;
                }

                if (current.version == QLatin1String(KEEPASSXC_VERSION)) {
                    marks << tr("this build");
                    current.statusTint = PillKind::Good;
                } else if (!newestDated.isEmpty() && current.version == newestDated) {
                    marks << tr("latest release");
                    current.statusTint = PillKind::Value;
                } else {
                    current.statusTint = PillKind::Warn;
                }
                current.status = marks.join(QStringLiteral(" · "));
                continue;
            }

            if (line.startsWith(QLatin1String("### "))) {
                tagForSection(line.mid(4).trimmed(), &tag, &tint);
                continue;
            }

            if (!inRelease) {
                continue;
            }

            if (line.startsWith(QLatin1String("- ")) || line.startsWith(QLatin1String("* "))) {
                const QString text = line.mid(2).trimmed();
                if (!text.isEmpty()) {
                    current.items.append({tag, text, tint});
                }
            }
        }

        flush();
        return releases;
    }

    QVector<Release> ChangelogFeed::filtered() const
    {
        // The same rule the screen filters by, so an export matches what is shown.
        const QString query = m_screen->searchBar()->text().trimmed();
        if (query.isEmpty()) {
            return m_releases;
        }

        QVector<Release> shown;
        for (const Release& release : m_releases) {
            if (release.version.contains(query, Qt::CaseInsensitive)) {
                shown.append(release);
                continue;
            }
            Release trimmed = release;
            trimmed.items.clear();
            for (const ChangeItem& item : release.items) {
                if (matches(item, query)) {
                    trimmed.items.append(item);
                }
            }
            if (!trimmed.items.isEmpty()) {
                shown.append(trimmed);
            }
        }
        return shown;
    }

    QString ChangelogFeed::markdown() const
    {
        const QString query = m_screen->searchBar()->text().trimmed();
        const QVector<Release> shown = filtered();

        QString text;
        QTextStream out(&text);
        out << "# " << tr("KeePassXC changelog") << "\n\n";
        out << "- " << tr("Exported") << ": " << Clock::toString(Clock::currentDateTime()) << "\n";
        out << "- " << tr("Build") << ": " << QString::fromLatin1(KEEPASSXC_VERSION) << "\n";
        if (!query.isEmpty()) {
            out << "- " << tr("Filter") << ": `" << query << "`\n";
        }
        out << "- " << tr("Releases") << ": " << shown.size() << " / " << m_releases.size() << "\n\n";

        if (shown.isEmpty()) {
            out << tr("No release matches the active filter.") << "\n";
            return text;
        }

        for (const Release& release : shown) {
            out << "## " << release.version;
            if (!release.date.isEmpty()) {
                out << " (" << release.date << ")";
            }
            if (!release.status.isEmpty()) {
                out << " - " << release.status;
            }
            out << "\n\n";
            for (const ChangeItem& item : release.items) {
                out << "- **" << item.tag << "** " << item.text << "\n";
            }
            out << "\n";
        }
        return text;
    }

    void ChangelogFeed::exportMarkdown()
    {
        const QString directory = FileDialog::getLastDir(ExportDirectoryRole);
        const QString fileName =
            fileDialog()->getSaveFileName(m_screen,
                                          tr("Export changelog to Markdown"),
                                          QDir(directory).filePath(QStringLiteral("keepassxc-changelog.md")),
                                          tr("Markdown (*.md);;All files (*)"));
        if (fileName.isEmpty()) {
            return;
        }
        FileDialog::saveLastDir(ExportDirectoryRole, fileName);

        QFile file(fileName);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            Notify::error(tr("Export failed"), tr("Could not write %1.").arg(QDir::toNativeSeparators(fileName)));
            return;
        }
        file.write(markdown().toUtf8());
        file.close();

        Notify::success(tr("Changelog exported"),
                        tr("%n release(s) written to %1.", "", filtered().size())
                            .arg(QDir::toNativeSeparators(fileName)));
    }

} // namespace Material
