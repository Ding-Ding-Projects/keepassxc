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
        /** The design's ISO stamp in the meta line: `2026-07-28 09:14`. */
        const QString MetaTimeFormat = QStringLiteral("yyyy-MM-dd HH:mm");
        /** Length of the short revision identifier the design's meta line opens with. */
        constexpr int ShortIdLength = 7;

        /** The glyph and tint of a revision follow what it recorded. */
        QString symbolFor(const HistoryRevision& revision)
        {
            switch (revision.kind) {
            case RevisionKind::Group:
                return QStringLiteral("group_add");
            case RevisionKind::Settings:
                return QStringLiteral("tune");
            case RevisionKind::Entry:
                break;
            }
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
            // The design gives every circle a real container colour, so a
            // revision that changed nothing countable still reads as one.
            if (revision.kind != RevisionKind::Entry) {
                return PillKind::Value;
            }
            if (revision.removed > 0) {
                return PillKind::Bad;
            }
            if (revision.added > 0) {
                return PillKind::Good;
            }
            return PillKind::Value;
        }

        /** The kind token that closes the meta line. */
        QString kindLabel(RevisionKind kind)
        {
            switch (kind) {
            case RevisionKind::Entry:
                return HistoryFeed::tr("entry");
            case RevisionKind::Group:
                return HistoryFeed::tr("group");
            case RevisionKind::Settings:
                break;
            }
            return HistoryFeed::tr("settings");
        }

        QString when(const QDateTime& timestamp)
        {
            return QLocale::system().toString(timestamp.toLocalTime(), QLocale::ShortFormat);
        }

        /**
         * `a91f04c · 2026-07-28 09:14 · entry`, the design's three tokens.
         *
         * The design only ever shows one vault, so it needs no fourth. When the
         * list is not scoped to a database the name is appended, because two
         * files' revisions would otherwise be indistinguishable.
         */
        QString describe(const HistoryRevision& revision, bool scoped)
        {
            const QString meta = QStringLiteral("%1 · %2 · %3")
                                     .arg(revision.id.left(ShortIdLength),
                                          revision.timestamp.toLocalTime().toString(MetaTimeFormat),
                                          kindLabel(revision.kind));
            return scoped ? meta : QStringLiteral("%1 · %2").arg(meta, revision.databaseName);
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

        m_screen->searchBar()->setShowRegexControls(false);
        connect(m_screen->searchBar(), &SearchBar::textChanged, this, [this](const QString& text) {
            m_query = text.trimmed();
            refresh();
        });
        connect(m_screen, &HistoryScreen::filterChanged, this, &HistoryFeed::refresh);

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

        const RevisionFilter kind = m_screen->kindFilter();
        const QDateTime since = QDateTime::currentDateTime().addDays(-HistoryScreen::recentDays());
        const bool recentOnly = m_screen->isRecentOnly();

        QVector<Revision> rows;
        for (const HistoryRevision& recordedRevision : recorded) {
            if (kind == RevisionFilter::Entries && recordedRevision.kind != RevisionKind::Entry) {
                continue;
            }
            // Settings is the other half of the pair: everything the save was
            // about that was not an entry.
            if (kind == RevisionFilter::Settings && recordedRevision.kind == RevisionKind::Entry) {
                continue;
            }
            if (recentOnly && recordedRevision.timestamp < since) {
                continue;
            }

            const QString meta = describe(recordedRevision, !m_databasePath.isEmpty());
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
            // No id: HistoryScreen draws this as a plain line, without the Diff
            // and Restore actions a real revision carries.
            Revision row;
            row.symbol = QStringLiteral("history");
            row.tint = PillKind::Off;
            if (!m_query.isEmpty() || kind != RevisionFilter::All || recentOnly) {
                row.label = tr("No revision matches these filters");
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
            dialog->setSupportingText(tr("That revision is no longer in the log."));
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
