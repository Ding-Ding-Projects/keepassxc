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

#include "MaterialHistoryFeed.h"

#include "MaterialDialog.h"
#include "MaterialHistoryScreen.h"
#include "MaterialHistoryStore.h"
#include "MaterialSearchBar.h"

#include "core/Database.h"

#include <QDir>
#include <QLocale>
#include <QStringList>

namespace Material
{
    namespace
    {
        /** The glyph and tint of a revision follow what it recorded. */
        QString symbolFor(const HistoryRevision& revision)
        {
            if (revision.removed > 0) {
                return QStringLiteral("delete");
            }
            if (revision.added > 0) {
                return QStringLiteral("add");
            }
            if (revision.edited > 0) {
                return QStringLiteral("edit");
            }
            return QStringLiteral("save");
        }

        PillKind tintFor(const HistoryRevision& revision)
        {
            if (revision.removed > 0) {
                return PillKind::Bad;
            }
            if (revision.added > 0) {
                return PillKind::Good;
            }
            if (revision.edited > 0) {
                return PillKind::Value;
            }
            return PillKind::Off;
        }

        QString when(const QDateTime& timestamp)
        {
            return QLocale::system().toString(timestamp.toLocalTime(), QLocale::ShortFormat);
        }

        QString describe(const HistoryRevision& revision)
        {
            return HistoryFeed::tr("%1 · %2 · %3 entries in %4 groups")
                .arg(when(revision.timestamp),
                     revision.databaseName,
                     QString::number(revision.entryCount),
                     QString::number(revision.groupCount));
        }
    } // namespace

    HistoryFeed::HistoryFeed(HistoryScreen* screen, QObject* parent)
        : QObject(parent)
        , m_screen(screen)
    {
        Q_ASSERT(m_screen);

        m_screen->setSupportingText(
            tr("KeePassXC keeps its own append-only log of every database it saves, in the application's data folder "
               "and never inside your database folder. A revision records when the save happened, which file it was, "
               "and how many entries were added, removed or edited since the save before it. No entry contents are "
               "written here, which is also why restoring contents is not yet possible."));

        m_screen->searchBar()->setPlaceholder(tr("Search recorded revisions"));
        m_screen->searchBar()->setShowRegexControls(false);
        connect(m_screen->searchBar(), &SearchBar::textChanged, this, [this](const QString& text) {
            m_query = text.trimmed();
            refresh();
        });

        connect(m_screen, &HistoryScreen::diffRequested, this, &HistoryFeed::showDiff);
        connect(m_screen, &HistoryScreen::restoreRequested, this, &HistoryFeed::explainRestore);
        connect(HistoryStore::instance(), &HistoryStore::revisionsChanged, this, &HistoryFeed::refresh);
    }

    HistoryFeed::~HistoryFeed() = default;

    void HistoryFeed::setDatabase(const QSharedPointer<Database>& db)
    {
        m_databasePath = db ? QDir::fromNativeSeparators(db->filePath()) : QString();
        refresh();
    }

    void HistoryFeed::refresh()
    {
        auto* store = HistoryStore::instance();
        const QVector<HistoryRevision> recorded =
            m_databasePath.isEmpty() ? store->revisions() : store->revisionsFor(m_databasePath);

        QVector<Revision> rows;
        for (const HistoryRevision& recordedRevision : recorded) {
            const QString meta = describe(recordedRevision);
            if (!m_query.isEmpty() && !recordedRevision.label.contains(m_query, Qt::CaseInsensitive)
                && !meta.contains(m_query, Qt::CaseInsensitive)) {
                continue;
            }

            Revision row;
            row.id = recordedRevision.id;
            row.symbol = symbolFor(recordedRevision);
            row.label = recordedRevision.label;
            row.meta = meta;
            row.tint = tintFor(recordedRevision);
            rows.append(row);
        }

        if (rows.isEmpty()) {
            Revision row;
            row.symbol = QStringLiteral("history_toggle_off");
            row.tint = PillKind::Off;
            if (!m_query.isEmpty()) {
                row.label = tr("No revision matches this search");
                row.meta = tr("%n revision(s) recorded in total", "", recorded.size());
            } else if (!recorded.isEmpty()) {
                row.label = tr("Nothing recorded for this database yet");
                row.meta = tr("The log holds %n revision(s) of other databases", "", recorded.size());
            } else {
                row.label = tr("Nothing recorded yet");
                row.meta = tr("A revision is written every time a database is saved");
            }
            rows.append(row);
        }

        m_screen->setRevisions(rows);
    }

    void HistoryFeed::showDiff(const QString& id)
    {
        const HistoryRevision revision = HistoryStore::instance()->revision(id);
        auto dialog = new Dialog(m_screen->window());
        dialog->setSymbol(QStringLiteral("difference"));
        connect(dialog, &Overlay::closed, dialog, &QObject::deleteLater);

        if (!revision.isValid()) {
            dialog->setHeadline(tr("Nothing to compare"));
            dialog->setSupportingText(tr("This row is a placeholder, not a recorded revision."));
            dialog->addAction(tr("Close"), true);
            dialog->openOverlay();
            return;
        }

        QStringList lines;
        lines << tr("Database: %1").arg(revision.databaseName);
        lines << tr("File: %1").arg(QDir::toNativeSeparators(revision.databasePath));
        lines << tr("Saved: %1").arg(when(revision.timestamp));
        lines << tr("Recorded: %1").arg(revision.label);
        lines << tr("State: %1 entries in %2 groups")
                     .arg(QString::number(revision.entryCount), QString::number(revision.groupCount));

        const HistoryRevision previous = HistoryStore::instance()->predecessor(id);
        if (previous.isValid()) {
            lines << QString();
            lines << tr("Previous revision, saved %1: %2 entries in %3 groups")
                         .arg(when(previous.timestamp),
                              QString::number(previous.entryCount),
                              QString::number(previous.groupCount));
            lines << tr("Difference: %1 entries, %2 groups")
                         .arg(QStringLiteral("%1%2")
                                  .arg(revision.entryCount - previous.entryCount >= 0 ? QStringLiteral("+")
                                                                                      : QString())
                                  .arg(revision.entryCount - previous.entryCount),
                              QStringLiteral("%1%2")
                                  .arg(revision.groupCount - previous.groupCount >= 0 ? QStringLiteral("+")
                                                                                      : QString())
                                  .arg(revision.groupCount - previous.groupCount));
        } else {
            lines << QString();
            lines << tr("This is the first revision recorded for this database, so there is nothing before it.");
        }

        lines << QString();
        lines << tr("The log records counts, not contents, so this compares what was recorded rather than the "
                    "entries themselves.");

        dialog->setHeadline(tr("Revision of %1").arg(when(revision.timestamp)));
        dialog->setSupportingText(lines.join(QLatin1Char('\n')));
        dialog->addAction(tr("Close"), true);
        dialog->openOverlay();
    }

    void HistoryFeed::explainRestore(const QString& id)
    {
        const HistoryRevision revision = HistoryStore::instance()->revision(id);
        auto dialog = new Dialog(m_screen->window());
        dialog->setSymbol(QStringLiteral("restore"));
        connect(dialog, &Overlay::closed, dialog, &QObject::deleteLater);
        dialog->setHeadline(tr("Restore is not wired up"));

        QString body = tr("Nothing has been changed.\n\n"
                          "This history is a log of what changed at each save. It deliberately keeps no copy of your "
                          "database - only counts and hashes - so there is nothing here to put back. Restoring "
                          "contents needs snapshots that do not exist yet.\n\n"
                          "To recover an older state today, use the database backups KeePassXC writes on save, or the "
                          "per-entry history inside the entry editor.");
        if (revision.isValid()) {
            body.prepend(tr("Selected revision: %1, saved %2.\n\n").arg(revision.label, when(revision.timestamp)));
        }

        dialog->setSupportingText(body);
        dialog->addAction(tr("Close"), true);
        dialog->openOverlay();
    }

} // namespace Material
