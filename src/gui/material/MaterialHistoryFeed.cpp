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
#include "MaterialHistoryStore.h"
#include "MaterialNotifier.h"
#include "MaterialSearchBar.h"

#include "core/Clock.h"
#include "core/Database.h"
#include "core/Entry.h"
#include "core/EntryAttachments.h"
#include "core/EntryAttributes.h"
#include "core/Group.h"
#include "gui/Clipboard.h"

#include <QCryptographicHash>
#include <QDir>
#include <QLocale>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>

#include <algorithm>

namespace Material
{
    namespace
    {
        /** The design's ISO stamp in the meta line: `2026-07-28 09:14`. */
        const QString MetaTimeFormat = QStringLiteral("yyyy-MM-dd HH:mm");
        /** Length of the short revision identifier the design's meta line opens with. */
        constexpr int ShortIdLength = 7;
        /**
         * How many rows are built at once. An entry that has been edited for
         * years carries hundreds of revisions and each row is a widget, so the
         * rest are counted and reported rather than quietly dropped.
         */
        constexpr int MaximumRows = 200;
        /**
         * How many of this window's own restores are remembered. They are a
         * report of what was done here, not a record anything else keeps, so
         * the oldest are dropped rather than letting the list grow for as long
         * as a database stays unlocked.
         */
        constexpr int MaximumSessionRestores = 100;
        /** How much of a stored value one diff line shows before eliding. */
        constexpr int DiffValueLength = 60;
        /** The arrow between the two sides of a diff line. */
        constexpr char16_t DiffArrow = u'→';
        /** The ellipsis that marks an elided value. */
        constexpr char16_t Ellipsis = u'…';

        /** The glyph of a save record follows what the save recorded. */
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

        RevisionTint tintFor(const HistoryRevision& revision)
        {
            // The design gives every circle a real container colour, so a save
            // that changed nothing countable still reads as an event.
            if (revision.kind != RevisionKind::Entry) {
                return RevisionTint::Neutral;
            }
            if (revision.removed > 0) {
                return RevisionTint::Negative;
            }
            if (revision.added > 0) {
                return RevisionTint::Positive;
            }
            return RevisionTint::Neutral;
        }

        /** The kind token that closes the meta line of a save record. */
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

        QString stamp(const QDateTime& timestamp)
        {
            return timestamp.toLocalTime().toString(MetaTimeFormat);
        }

        /** `a91f04c · 2026-07-28 09:14 · entry`, the design's three tokens. */
        QString metaLine(const QString& id, const QDateTime& timestamp, const QString& kind)
        {
            return QStringLiteral("%1 · %2 · %3").arg(id.left(ShortIdLength), stamp(timestamp), kind);
        }

        /**
         * A stable identifier for one revision of one entry.
         *
         * The design opens its meta line with a short digest, so one is
         * computed rather than borrowed: the entry's own UUID never reaches the
         * screen, and the id survives the list being rebuilt.
         *
         * The seed is the revision's own timestamp, not its position in the
         * history: Entry::truncateHistory() drops the oldest revisions, which
         * renumbers every position that is left but changes no timestamp.
         * @p ordinal separates the case KDBX makes possible - two revisions of
         * one entry stamped within the same second - because two rows sharing
         * an id would share an entry in the lookup table, and a click on one
         * would then act on the other.
         */
        QString entryRevisionId(const Entry* entry, const QDateTime& revisionTime, int ordinal)
        {
            QString seed = QStringLiteral("%1:%2").arg(entry->uuidToHex(), revisionTime.toString(Qt::ISODateWithMs));
            if (ordinal > 0) {
                seed += QStringLiteral(":%1").arg(ordinal);
            }
            return QString::fromLatin1(QCryptographicHash::hash(seed.toUtf8(), QCryptographicHash::Sha256).toHex());
        }

        /**
         * The identifier of a restore this feed performed. Seeded differently
         * from entryRevisionId() so a restore row and a revision row can never
         * be handed the same id.
         */
        QString sessionRestoreId(const Entry* entry, const QDateTime& timestamp)
        {
            const QByteArray seed =
                QStringLiteral("restore:%1:%2").arg(entry->uuidToHex(), timestamp.toString(Qt::ISODateWithMs)).toUtf8();
            return QString::fromLatin1(QCryptographicHash::hash(seed, QCryptographicHash::Sha256).toHex());
        }

        QString entryName(const Entry* entry)
        {
            return entry->title().isEmpty() ? HistoryFeed::tr("Untitled entry") : entry->title();
        }

        /** Attachment names on @p to that @p from did not have. Names only, never bytes. */
        QStringList attachmentsGained(const Entry* from, const Entry* to)
        {
            QStringList names;
            const QList<QString> keys = to->attachments()->keys();
            for (const QString& key : keys) {
                if (!from->attachments()->hasKey(key)) {
                    names.append(key);
                }
            }
            return names;
        }

        /** One flattened, length-capped line for a value that is safe to print. */
        QString shown(const QString& value)
        {
            if (value.isEmpty()) {
                return HistoryFeed::tr("(empty)");
            }
            QString flat = value;
            flat.replace(QLatin1Char('\n'), QLatin1Char(' '));
            flat.replace(QLatin1Char('\r'), QLatin1Char(' '));
            if (flat.size() > DiffValueLength) {
                flat = flat.left(DiffValueLength - 1) + QChar(Ellipsis);
            }
            return flat;
        }

        /** What the design's row says about one step of an entry's own history. */
        struct EntryChange
        {
            QString symbol;
            QString label;
            QString kind;
            RevisionTint tint = RevisionTint::Neutral;
        };

        /**
         * Describe the step from @p before to @p after.
         *
         * The kind is decided from the values themselves rather than from the
         * translated field names Entry::calculateDifference() returns, so a row
         * does not change kind with the interface language. The field list is
         * still taken from there, because it is the wording the rest of the
         * application already uses for the same comparison.
         */
        EntryChange describeEntryChange(Entry* before, Entry* after)
        {
            EntryChange described;
            described.kind = HistoryFeed::tr("entry");
            described.symbol = QStringLiteral("edit");

            const QString name = entryName(after);
            const QString separator = HistoryFeed::tr(", ");
            const QStringList fields = after->calculateDifference(before);

            // calculateDifference() reports all attachments as one field, so a
            // lone entry in that list is a change that touched nothing else.
            if (fields.size() == 1 && *after->attachments() != *before->attachments()) {
                described.kind = HistoryFeed::tr("attachment");
                described.symbol = QStringLiteral("attachment");
                const QStringList gained = attachmentsGained(before, after);
                const QStringList lost = attachmentsGained(after, before);
                if (!gained.isEmpty() && lost.isEmpty()) {
                    described.label = HistoryFeed::tr("Attached %1 to \"%2\"").arg(gained.join(separator), name);
                } else if (gained.isEmpty() && !lost.isEmpty()) {
                    described.label = HistoryFeed::tr("Removed %1 from \"%2\"").arg(lost.join(separator), name);
                    described.tint = RevisionTint::Negative;
                } else if (!gained.isEmpty()) {
                    described.label = HistoryFeed::tr("Replaced %1 with %2 on \"%3\"")
                                          .arg(lost.join(separator), gained.join(separator), name);
                } else {
                    // Same names, different bytes: a file was attached again.
                    described.label = HistoryFeed::tr("Replaced the attachment contents of \"%1\"").arg(name);
                }
                return described;
            }

            if (before->title() != after->title()) {
                described.symbol = QStringLiteral("label");
                described.label = before->title().isEmpty()
                                      ? HistoryFeed::tr("Named the untitled entry \"%1\"").arg(name)
                                      : HistoryFeed::tr("Renamed \"%1\" to \"%2\"").arg(before->title(), name);
                return described;
            }

            if (before->password() != after->password()) {
                described.symbol = QStringLiteral("password");
                described.tint = RevisionTint::Accent;
                described.label = HistoryFeed::tr("Rotated the password of \"%1\"").arg(name);
                return described;
            }

            described.label = fields.isEmpty()
                                  ? HistoryFeed::tr("Edited \"%1\"").arg(name)
                                  : HistoryFeed::tr("Changed %1 on \"%2\"").arg(fields.join(separator), name);
            return described;
        }

        void appendFieldLine(QStringList& lines, const QString& label, const QString& was, const QString& now)
        {
            if (was == now) {
                return;
            }
            lines.append(
                QStringLiteral("%1: %2 %3 %4").arg(label, shown(was), QString(QChar(DiffArrow)), shown(now)));
        }

        /**
         * How @p revision differs from @p current, in words.
         *
         * Values are printed only for what the entry list already shows in the
         * clear. Passwords, TOTP settings, notes and custom attributes are
         * compared and reported as changed but never spelled out: wanting a
         * diff is no reason to put a secret on screen unmasked.
         */
        QStringList diffLines(Entry* current, Entry* revision)
        {
            QStringList lines;
            appendFieldLine(lines, HistoryFeed::tr("Title"), revision->title(), current->title());
            appendFieldLine(lines, HistoryFeed::tr("Username"), revision->username(), current->username());
            appendFieldLine(lines, HistoryFeed::tr("URL"), revision->url(), current->url());
            appendFieldLine(lines, HistoryFeed::tr("Tags"), revision->tags(), current->tags());

            if (revision->password() != current->password()) {
                lines.append(HistoryFeed::tr("Password: changed"));
            }
            if (revision->notes() != current->notes()) {
                lines.append(HistoryFeed::tr("Notes: changed"));
            }
            if (revision->totpSettingsString() != current->totpSettingsString()) {
                lines.append(HistoryFeed::tr("TOTP: changed"));
            }

            const auto expiry = [](Entry* entry) {
                return entry->timeInfo().expires() ? stamp(entry->timeInfo().expiryTime()) : HistoryFeed::tr("never");
            };
            appendFieldLine(lines, HistoryFeed::tr("Expires"), expiry(revision), expiry(current));

            const QStringList gained = attachmentsGained(revision, current);
            const QStringList lost = attachmentsGained(current, revision);
            const QString separator = HistoryFeed::tr(", ");
            if (!gained.isEmpty()) {
                lines.append(HistoryFeed::tr("Attachments added since: %1").arg(gained.join(separator)));
            }
            if (!lost.isEmpty()) {
                lines.append(HistoryFeed::tr("Attachments removed since: %1").arg(lost.join(separator)));
            }

            QStringList attributeKeys = current->attributes()->customKeys();
            const QList<QString> revisionKeys = revision->attributes()->customKeys();
            for (const QString& key : revisionKeys) {
                if (!attributeKeys.contains(key)) {
                    attributeKeys.append(key);
                }
            }
            int changedAttributes = 0;
            for (const QString& key : attributeKeys) {
                if (revision->attributes()->hasKey(key) != current->attributes()->hasKey(key)
                    || revision->attributes()->value(key) != current->attributes()->value(key)) {
                    ++changedAttributes;
                }
            }
            if (changedAttributes > 0) {
                lines.append(HistoryFeed::tr("%n custom attribute(s) changed", "", changedAttributes));
            }

            return lines;
        }
    } // namespace

    HistoryFeed::HistoryFeed(HistoryScreen* screen, QObject* parent)
        : QObject(parent)
        , m_screen(screen)
    {
        Q_ASSERT(m_screen);

        m_screen->setSupportingText(
            tr("Two records are merged here, newest first. The database's own per-entry revisions keep the previous "
               "values in full, so those rows can be compared with the entry as it stands and put back; they are the "
               "same revisions the entry editor's History tab lists. The other record is the append-only save log "
               "KeePassXC keeps in the application's data folder, never inside your database folder: it holds no "
               "contents at all, only when a save happened and how many entries and groups it then had, so its rows "
               "can be compared but not restored. Restores made in this window are listed for as long as the database "
               "they were made in stays unlocked, because nothing in the database or the log marks a revision as a "
               "restore."));

        connect(m_screen->searchBar(), &SearchBar::textChanged, this, [this](const QString& text) {
            m_query = text.trimmed();
            refresh();
        });
        // The design draws the search field with the regex button beside it, so
        // the box holds a pattern and the button opens the builder for it.
        connect(m_screen, &HistoryScreen::filterChanged, this, &HistoryFeed::refresh);

        connect(m_screen, &HistoryScreen::diffRequested, this, &HistoryFeed::showDiff);
        connect(m_screen, &HistoryScreen::restoreRequested, this, &HistoryFeed::restoreRevision);
        connect(HistoryStore::instance(), &HistoryStore::revisionsChanged, this, &HistoryFeed::rebuild);
    }

    HistoryFeed::~HistoryFeed() = default;

    void HistoryFeed::setDatabase(const QSharedPointer<Database>& db)
    {
        if (m_database.lock() != db) {
            // The restores carry entry titles, which belong to the database
            // they were read from and to nothing else.
            m_restores.clear();
        }

        for (const QMetaObject::Connection& watch : m_databaseWatch) {
            disconnect(watch);
        }
        m_databaseWatch.clear();

        m_database = db.toWeakRef();
        m_databasePath = db ? QDir::fromNativeSeparators(db->filePath()) : QString();
        if (db) {
            // An entry gains a revision from the entry editor, from Auto-Type,
            // from a merge - none of which pass through here. Database's own
            // modified() signal is already debounced, so following it costs one
            // rebuild per burst of edits.
            m_databaseWatch.append(connect(db.data(), &Database::modified, this, &HistoryFeed::rebuild));
            // Save As gives the file a new name, and the save log is keyed by
            // the name: without this the rows recorded under the old one would
            // silently stop being listed.
            m_databaseWatch.append(
                connect(db.data(), &Database::filePathChanged, this, [this](const QString&, const QString& newPath) {
                    m_databasePath = QDir::fromNativeSeparators(newPath);
                    rebuild();
                }));
            if (db->rootGroup()) {
                m_databaseWatch.append(
                    connect(db->rootGroup(), &QObject::destroyed, this, &HistoryFeed::forgetDatabase));
            }
        }
        rebuild();
    }

    void HistoryFeed::forgetDatabase()
    {
        // Cleared here rather than left to setDatabase(): when the database is
        // being destroyed outright the weak reference has already expired, so
        // the two would compare equal and the titles would stay.
        m_restores.clear();
        setDatabase({});
    }

    void HistoryFeed::rebuild()
    {
        m_changes = entryRevisions();
        m_changes.append(savedRevisions());
        m_changes.append(sessionRestores());
        std::sort(m_changes.begin(), m_changes.end(), [](const Change& first, const Change& second) {
            return first.when > second.when;
        });
        refresh();
    }

    QVector<HistoryFeed::Change> HistoryFeed::entryRevisions() const
    {
        QVector<Change> changes;
        const auto database = m_database.lock();
        if (!database || !database->rootGroup()) {
            return changes;
        }

        // Ids have to be unique across the whole listing, because one lookup
        // table maps all of them back to their subjects.
        QSet<QString> taken;

        const QList<Entry*> entries = database->rootGroup()->entriesRecursive(false);
        for (Entry* entry : entries) {
            // entriesRecursive() walks the recycle bin too. The save log counts
            // only what is outside it, so leaving deleted entries in would list
            // their titles as if they were still live and would disagree with
            // the other source about what this database holds.
            if (entry->isRecycled()) {
                continue;
            }

            // historyItems() are the states an entry left behind, oldest first,
            // so the change item i records is the step from it to whatever came
            // next - the following item, or the entry as it stands now.
            const QList<Entry*> history = entry->historyItems();
            for (int i = 0; i < history.size(); ++i) {
                Entry* before = history.at(i);
                Entry* after = (i + 1 < history.size()) ? history.at(i + 1) : entry;
                if (!before || !after) {
                    continue;
                }

                const EntryChange described = describeEntryChange(before, after);
                // Seeded from the revision Restore puts back, which is the one
                // frozen item of the pair: the last row's "after" is the live
                // entry, and its timestamp moves with every edit.
                QString id;
                for (int ordinal = 0;; ++ordinal) {
                    id = entryRevisionId(entry, before->timeInfo().lastModificationTime(), ordinal);
                    if (!taken.contains(id)) {
                        break;
                    }
                }
                taken.insert(id);

                Change change;
                change.when = after->timeInfo().lastModificationTime();
                change.entryScoped = true;
                change.origin.kind = Origin::Kind::EntryRevision;
                change.origin.entryUuid = entry->uuid();
                change.origin.revision = before;
                change.row.id = id;
                change.row.symbol = described.symbol;
                change.row.label = described.label;
                change.row.meta = metaLine(id, change.when, described.kind);
                change.row.tint = described.tint;
                change.row.canDiff = true;
                change.row.canRestore = true;
                changes.append(change);
            }
        }
        return changes;
    }

    QVector<HistoryFeed::Change> HistoryFeed::savedRevisions() const
    {
        auto* store = HistoryStore::instance();
        const QVector<HistoryRevision> recorded =
            m_databasePath.isEmpty() ? store->revisions() : store->revisionsFor(m_databasePath);

        QVector<Change> changes;
        changes.reserve(recorded.size());
        for (const HistoryRevision& recordedRevision : recorded) {
            // The design shows one vault, so it needs no fourth token. When the
            // list is not scoped to a database the name is appended, because
            // two files' records would otherwise be indistinguishable.
            QString meta = metaLine(recordedRevision.id, recordedRevision.timestamp, kindLabel(recordedRevision.kind));
            if (m_databasePath.isEmpty()) {
                meta = QStringLiteral("%1 · %2").arg(meta, recordedRevision.databaseName);
            }

            Change change;
            change.when = recordedRevision.timestamp;
            change.entryScoped = recordedRevision.kind == RevisionKind::Entry;
            change.origin.kind = Origin::Kind::SaveLog;
            change.origin.logId = recordedRevision.id;
            change.row.id = recordedRevision.id;
            change.row.symbol = symbolFor(recordedRevision);
            change.row.label = recordedRevision.label;
            change.row.meta = meta;
            change.row.tint = tintFor(recordedRevision);
            // The log keeps counts, not contents: something to compare against
            // the save before it, nothing to put back.
            change.row.canDiff = true;
            change.row.canRestore = false;
            changes.append(change);
        }
        return changes;
    }

    QVector<HistoryFeed::Change> HistoryFeed::sessionRestores() const
    {
        QVector<Change> changes;
        // No filtering by database: the list is emptied whenever the database
        // in front changes, so everything in it belongs to the one shown.
        for (const Restored& restored : m_restores) {
            Change change;
            change.when = restored.when;
            change.entryScoped = true;
            change.origin.kind = Origin::Kind::SessionRestore;
            change.row.id = restored.id;
            change.row.symbol = QStringLiteral("restore");
            change.row.label = tr("Restored \"%1\" from %2").arg(restored.entryTitle, stamp(restored.revisionTime));
            change.row.meta = metaLine(restored.id, restored.when, tr("restore"));
            change.row.tint = RevisionTint::Positive;
            // The restore has happened and the revision it came from is listed
            // in its own right, so this row is a record, not a handle.
            change.row.canDiff = false;
            change.row.canRestore = false;
            changes.append(change);
        }
        return changes;
    }

    void HistoryFeed::refresh()
    {
        const RevisionFilter kind = m_screen->kindFilter();
        const QDateTime since = QDateTime::currentDateTime().addDays(-HistoryScreen::recentDays());
        const bool recentOnly = m_screen->isRecentOnly();
        const bool narrowed = !m_query.isEmpty() || kind != RevisionFilter::All || recentOnly;

        // The search bar carries the regular expression builder, so what it
        // holds is a pattern. A pattern that will not compile is what half-typed
        // input looks like, so that falls back to plain containment and the list
        // narrows as it is typed instead of emptying.
        const QRegularExpression pattern(m_query, QRegularExpression::CaseInsensitiveOption);
        const bool byPattern = !m_query.isEmpty() && pattern.isValid();
        const auto matches = [this, &pattern, byPattern](const QString& text) {
            return byPattern ? pattern.match(text).hasMatch() : text.contains(m_query, Qt::CaseInsensitive);
        };

        m_origins.clear();
        QVector<Revision> rows;
        int matched = 0;
        for (const Change& change : m_changes) {
            if (kind == RevisionFilter::Entries && !change.entryScoped) {
                continue;
            }
            // Settings is the other half of the pair: everything that was not
            // about a single entry.
            if (kind == RevisionFilter::Settings && change.entryScoped) {
                continue;
            }
            if (recentOnly && change.when < since) {
                continue;
            }
            if (!m_query.isEmpty() && !matches(change.row.label) && !matches(change.row.meta)) {
                continue;
            }

            ++matched;
            if (rows.size() >= MaximumRows) {
                continue;
            }
            m_origins.insert(change.row.id, change.origin);
            rows.append(change.row);
        }

        if (matched > rows.size()) {
            Revision more;
            more.symbol = QStringLiteral("more_horiz");
            more.tint = RevisionTint::Muted;
            more.label = tr("%n further change(s) are not listed", "", matched - rows.size());
            more.meta = tr("The %n newest are shown; narrow with the search box or the filters", "", rows.size());
            rows.append(more);
        }

        if (rows.isEmpty()) {
            // Neither action flag is set: HistoryScreen draws these lines with
            // no buttons, because there is nothing for a button to act on.
            Revision row;
            row.symbol = QStringLiteral("history");
            row.tint = RevisionTint::Muted;
            if (narrowed) {
                row.label = tr("No change matches these filters");
                row.meta = tr("%n change(s) in this history", "", m_changes.size());
            } else if (m_database.isNull()) {
                row.label = tr("No database is open");
                row.meta = tr("Entry revisions are read from the open database; unlock one to see them");
            } else {
                row.label = tr("Nothing recorded yet");
                row.meta = tr("An entry gains a revision when it is edited, and the log a record when you save");
            }
            rows.append(row);
        }

        m_screen->setRevisions(rows);
    }

    Entry* HistoryFeed::revisionAt(const Origin& origin, Entry** owner) const
    {
        if (owner) {
            *owner = nullptr;
        }
        const auto database = m_database.lock();
        if (!database || !database->rootGroup() || origin.revision.isNull()) {
            return nullptr;
        }
        Entry* entry = database->rootGroup()->findEntryByUuid(origin.entryUuid);
        if (!entry || !entry->historyItems().contains(origin.revision.data())) {
            return nullptr;
        }
        if (owner) {
            *owner = entry;
        }
        return origin.revision.data();
    }

    void HistoryFeed::showDiff(const QString& id)
    {
        const Origin origin = m_origins.value(id);
        if (origin.kind == Origin::Kind::EntryRevision) {
            showEntryDiff(origin);
            return;
        }
        // An unknown id lands here with an empty log id, which showSaveDiff()
        // answers the same way as a record that has gone.
        showSaveDiff(origin.logId);
    }

    void HistoryFeed::showEntryDiff(const Origin& origin)
    {
        auto dialog = new Dialog(m_screen->window());
        dialog->setSymbol(QStringLiteral("difference"));
        connect(dialog, &Overlay::closed, dialog, &QObject::deleteLater);

        Entry* entry = nullptr;
        Entry* revision = revisionAt(origin, &entry);
        if (!revision || !entry) {
            dialog->setHeadline(tr("Nothing to compare"));
            dialog->setSupportingText(tr("That revision is no longer in the database."));
            dialog->addAction(tr("Close"), true);
            dialog->openOverlay();
            return;
        }

        QStringList lines;
        lines << tr("Entry: %1").arg(entryName(entry));
        if (entry->group()) {
            lines << tr("Group: %1").arg(entry->group()->hierarchy().join(QStringLiteral(" / ")));
        }
        lines << tr("This revision: %1").arg(when(revision->timeInfo().lastModificationTime()));
        lines << tr("The entry now: %1").arg(when(entry->timeInfo().lastModificationTime()));
        lines << QString();

        const QStringList differences = diffLines(entry, revision);
        if (differences.isEmpty()) {
            lines << tr("This revision matches the entry as it stands, so restoring it would change nothing.");
        } else {
            lines << tr("Changed since this revision:");
            lines << differences;
            lines << QString();
            lines << tr("Passwords, TOTP settings, notes and custom attributes are compared but not printed here.");
        }

        dialog->setHeadline(tr("Revision of %1").arg(stamp(revision->timeInfo().lastModificationTime())));
        dialog->setSupportingText(lines.join(QLatin1Char('\n')));
        dialog->addAction(tr("Close"), true);
        dialog->openOverlay();
    }

    void HistoryFeed::showSaveDiff(const QString& logId)
    {
        const HistoryRevision revision = HistoryStore::instance()->revision(logId);
        auto dialog = new Dialog(m_screen->window());
        dialog->setSymbol(QStringLiteral("difference"));
        connect(dialog, &Overlay::closed, dialog, &QObject::deleteLater);

        if (!revision.isValid()) {
            dialog->setHeadline(tr("Nothing to compare"));
            dialog->setSupportingText(tr("That save record is no longer in the log."));
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

        const HistoryRevision previous = HistoryStore::instance()->predecessor(logId);
        if (previous.isValid()) {
            lines << QString();
            lines << tr("Previous save, %1: %2 entries in %3 groups")
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
            lines << tr("This is the first save recorded for this database, so there is nothing before it.");
        }

        lines << QString();
        lines << tr("The save log records counts, not contents, so this compares what was recorded rather than the "
                    "entries themselves. The rows that came from the database's own entry history do compare "
                    "contents, and can be restored.");

        dialog->setHeadline(tr("Save of %1").arg(when(revision.timestamp)));
        dialog->setSupportingText(lines.join(QLatin1Char('\n')));
        dialog->addAction(tr("Close"), true);
        dialog->openOverlay();
    }

    void HistoryFeed::restoreRevision(const QString& id)
    {
        const Origin origin = m_origins.value(id);
        if (origin.kind != Origin::Kind::EntryRevision) {
            // The screen draws no Restore on the other rows, so arriving here
            // means the list moved under the click. Do nothing rather than act
            // on a revision the user did not point at.
            return;
        }

        Entry* entry = nullptr;
        Entry* revision = revisionAt(origin, &entry);
        if (!revision || !entry) {
            Notify::warning(tr("Nothing restored"), tr("That revision is no longer in the database."));
            return;
        }

        // The listing leaves recycled entries out, so a row naming one was
        // drawn before the entry was deleted: Database::modified is debounced,
        // so the list is briefly older than the database. Deleting only moves
        // an entry to the recycle bin, which leaves it and its history in the
        // tree for revisionAt() to find, so without this the restore would put
        // the old values back onto an entry the user has thrown away.
        if (entry->isRecycled()) {
            Notify::warning(tr("Nothing restored"),
                            tr("\"%1\" is in the recycle bin. Restore the entry itself first, then put this revision "
                               "back.")
                                .arg(entryName(entry)));
            return;
        }

        const QStringList fields = entry->calculateDifference(revision);
        if (fields.isEmpty()) {
            Notify::info(tr("Nothing to restore"), tr("\"%1\" already matches that revision.").arg(entryName(entry)));
            return;
        }

        auto confirm = Dialog::confirm(
            m_screen->window(),
            tr("Restore this revision?"),
            tr("\"%1\" goes back to how it was on %2, changing %3.\n\n"
               "The entry as it is now is kept as a new revision, so this can itself be undone. Nothing is written to "
               "disk until you save the database.")
                .arg(entryName(entry), stamp(revision->timeInfo().lastModificationTime()), fields.join(tr(", "))),
            tr("Restore"));
        // The origin carries a guarded pointer to the revision itself, so what
        // is put back is what was on the row, however the history has moved
        // while the question was on screen.
        connect(confirm, &Dialog::accepted, this, [this, origin] { applyRestore(origin); });
        confirm->openOverlay();
    }

    void HistoryFeed::applyRestore(const Origin& origin)
    {
        Entry* entry = nullptr;
        Entry* revision = revisionAt(origin, &entry);
        if (!revision || !entry) {
            Notify::warning(tr("Nothing restored"), tr("That revision is no longer in the database."));
            return;
        }

        // Asked again for the same reason the check above is: the confirmation
        // stays on screen for as long as the user takes, and a merge or another
        // window can delete the entry while it is up.
        if (entry->isRecycled()) {
            Notify::warning(tr("Nothing restored"),
                            tr("\"%1\" is in the recycle bin. Restore the entry itself first, then put this revision "
                               "back.")
                                .arg(entryName(entry)));
            return;
        }

        // Everything the report needs is read first: truncateHistory() below
        // may delete the revision to keep the entry within the database's
        // history limits, and reading it afterwards would be reading freed
        // memory.
        const QString name = entryName(entry);
        const QDateTime revisionTime = revision->timeInfo().lastModificationTime();
        const QStringList fields = entry->calculateDifference(revision);
        const TimeInfo before = entry->timeInfo();

        // Entry::copyDataFrom() is the restore itself: it is what puts a stored
        // state back onto an entry, and it is the same data the entry editor's
        // Restore ends up writing. The undo step is taken by hand rather than
        // with Entry::beginUpdate()/endUpdate(), because that pair only keeps
        // its snapshot when a modified() signal arrived and copyDataFrom()
        // assigns Entry's plain data members directly - a revision differing
        // only in an icon, a colour or a tag would be applied and then dropped
        // from the history, leaving the database not even marked as changed.
        // The two calls below are what endUpdate() does once it decides to keep
        // the snapshot it took.
        Entry* undo = entry->clone(Entry::CloneNoFlags);
        entry->copyDataFrom(revision);

        // copyDataFrom() carried the revision's whole TimeInfo across with the
        // rest of its data. The expiry belongs to the state being restored, but
        // when the entry was created, last reached and last moved does not.
        TimeInfo restored = entry->timeInfo();
        restored.setCreationTime(before.creationTime());
        restored.setLastAccessTime(before.lastAccessTime());
        restored.setLocationChanged(before.locationChanged());
        restored.setUsageCount(before.usageCount());

        // addHistoryItem() raises modified(), which is what marks the database
        // as changed so the restore can be saved; truncateHistory() can raise it
        // again. Entry answers modified() with updateTimeinfo(), which stamps
        // both the modification and the access time with the moment it arrived -
        // so the times above are put back afterwards, or the access time this
        // restore is meant to leave alone would be the one it destroyed.
        entry->addHistoryItem(undo);
        entry->truncateHistory();

        restored.setLastModificationTime(Clock::currentDateTimeUtc());
        entry->setTimeInfo(restored);

        // Nothing has told the entry list that this row changed: it refreshes a
        // row on Group::entryDataChanged, which carries Entry::entryDataChanged,
        // and copyDataFrom() assigns Entry's data members directly without
        // raising it. That signal is emitted only from Entry's own
        // emitDataChanged() slot, so it is called through the meta-object; the
        // vault would otherwise keep showing the values the restore replaced.
        const bool announced = QMetaObject::invokeMethod(entry, "emitDataChanged", Qt::DirectConnection);
        Q_ASSERT(announced);
        Q_UNUSED(announced)

        Restored record;
        record.when = Clock::currentDateTimeUtc();
        record.id = sessionRestoreId(entry, record.when);
        record.entryTitle = name;
        record.revisionTime = revisionTime;
        m_restores.append(record);
        while (m_restores.size() > MaximumSessionRestores) {
            m_restores.removeFirst();
        }

        Notify::success(tr("Revision restored"),
                        tr("\"%1\" is back as it was on %2. Changed back: %3. The state it was in has been kept as a "
                           "new revision. Save the database to make this permanent.")
                            .arg(name, stamp(revisionTime), fields.join(tr(", "))));
        rebuild();
    }

} // namespace Material
