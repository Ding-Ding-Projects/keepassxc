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

#include "MaterialReportsFeed.h"

#include "MaterialButtons.h"
#include "MaterialNotifier.h"
#include "MaterialReportsScreen.h"
#include "MaterialSearchBar.h"

#include "core/AsyncTask.h"
#include "core/Clock.h"
#include "core/Database.h"
#include "core/DatabaseStats.h"
#include "core/Entry.h"
#include "core/EntryAttributes.h"
#include "core/Group.h"
#include "core/Metadata.h"
#include "core/PasswordHealth.h"
#include "gui/FileDialog.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QScopedPointer>
#include <QTextStream>

#include <algorithm>

namespace Material
{
    namespace
    {
        const QString ExportDirectoryRole = QStringLiteral("reports");

        QString qualityName(PasswordHealth::Quality quality)
        {
            switch (quality) {
            case PasswordHealth::Quality::Bad:
                return ReportsFeed::tr("Bad");
            case PasswordHealth::Quality::Poor:
                return ReportsFeed::tr("Poor");
            case PasswordHealth::Quality::Weak:
                return ReportsFeed::tr("Weak");
            case PasswordHealth::Quality::Good:
                return ReportsFeed::tr("Good");
            case PasswordHealth::Quality::Excellent:
                return ReportsFeed::tr("Excellent");
            }
            return {};
        }

        Health healthOf(bool bad)
        {
            return bad ? Health::Breached : Health::Weak;
        }

        /** Escape the pipes that would otherwise break a Markdown table cell. */
        QString cell(const QString& text)
        {
            QString value = text;
            value.replace(QLatin1Char('\n'), QLatin1Char(' '));
            value.replace(QLatin1Char('|'), QStringLiteral("\\|"));
            return value.trimmed();
        }
    } // namespace

    ReportsFeed::ReportsFeed(ReportsScreen* screen, QObject* parent)
        : QObject(parent)
        , m_screen(screen)
    {
        Q_ASSERT(m_screen);

        m_screen->setSupportingText(
            tr("Password health and statistics for the database in front, computed the same way the detailed reports "
               "compute them. Breach exposure is not shown here because it can only be answered by an online Have I "
               "Been Pwned lookup; open the detailed reports to run one."));

        auto detailed = new OutlinedButton(QStringLiteral("lab_profile"), tr("Detailed reports"));
        connect(detailed, &QAbstractButton::clicked, this, &ReportsFeed::detailedReportsRequested);
        m_screen->addHeaderWidget(detailed);

        m_screen->searchBar()->setPlaceholder(tr("Filter findings and statistics"));
        m_screen->searchBar()->setShowRegexControls(false);
        connect(m_screen->searchBar(), &SearchBar::textChanged, this, [this](const QString& text) {
            m_query = text.trimmed();
            apply();
        });

        // Findings carry an entry UUID. The healthy / filtered-empty placeholder
        // carries none, and its action opens the detailed reports instead of
        // leaving a button that does nothing.
        connect(m_screen, &ReportsScreen::fixRequested, this, [this](const QString& id) {
            if (id.isEmpty()) {
                emit detailedReportsRequested();
            } else {
                emit entryEditRequested(id);
            }
        });
        connect(m_screen, &ReportsScreen::exportRequested, this, &ReportsFeed::exportMarkdown);
    }

    ReportsFeed::~ReportsFeed() = default;

    void ReportsFeed::setDatabase(const QSharedPointer<Database>& db)
    {
        m_db = db;
        refresh();
    }

    void ReportsFeed::refresh()
    {
        // The health check runs in a worker thread and is waited on with a
        // nested event loop, so a second refresh can arrive mid-flight.
        if (m_busy) {
            return;
        }

        if (!m_db || !m_db->rootGroup()) {
            m_snapshot = Snapshot();
            apply();
            return;
        }

        m_busy = true;
        auto db = m_db;
        const QScopedPointer<Snapshot> computed(
            AsyncTask::runAndWaitForFuture([db] { return ReportsFeed::compute(db); }));
        m_snapshot = *computed;
        m_busy = false;

        apply();
    }

    ReportsFeed::Snapshot* ReportsFeed::compute(QSharedPointer<Database> db)
    {
        auto snapshot = new Snapshot();
        snapshot->valid = true;

        const HealthChecker checker(db);
        for (const Group* group : db->rootGroup()->groupsRecursive(true)) {
            if (group->isRecycled()) {
                continue;
            }
            const QString path = group->hierarchy().join(QLatin1Char('/'));

            for (Entry* entry : group->entries()) {
                if (entry->isRecycled()) {
                    continue;
                }
                if (entry->attributes()->hasKey(EntryAttributes::KPEX_PASSKEY_PRIVATE_KEY_PEM)) {
                    ++snapshot->passkeys;
                }
                // The health check skips entries without a password, exactly as
                // the healthcheck report does, so an empty entry is neither a
                // finding nor counted as healthy.
                if (entry->password().isEmpty()) {
                    continue;
                }

                const auto health = checker.evaluate(entry);
                if (health->quality() >= PasswordHealth::Quality::Good) {
                    ++snapshot->healthy;
                    continue;
                }

                Finding finding;
                finding.uuid = entry->uuidToHex();
                finding.title = entry->title();
                finding.path = path;
                finding.reason = health->scoreReason().split(QLatin1Char('\n'), Qt::SkipEmptyParts).join(QStringLiteral(" · "));
                finding.quality = qualityName(health->quality());
                finding.score = health->score();
                finding.bad = health->quality() == PasswordHealth::Quality::Bad;
                finding.excluded = entry->excludeFromReports();
                finding.expired = entry->isExpired();
                snapshot->findings.append(finding);
            }
        }

        // Worst score first, which is the order the healthcheck report uses.
        std::sort(snapshot->findings.begin(), snapshot->findings.end(), [](const Finding& left, const Finding& right) {
            return left.score < right.score;
        });

        const DatabaseStats stats(db);
        snapshot->databaseName = db->metadata()->name().isEmpty() ? QFileInfo(db->filePath()).completeBaseName()
                                                                 : db->metadata()->name();
        snapshot->databasePath = db->filePath();
        snapshot->entries = stats.entryCount;
        snapshot->groups = stats.groupCount;
        snapshot->shortPasswords = stats.shortPasswords;

        auto& rows = snapshot->statistics;
        rows.append({tr("Database name"), db->metadata()->name()});
        rows.append({tr("Description"), db->metadata()->description()});
        rows.append({tr("Location"), db->filePath()});
        rows.append({tr("Database created"), Clock::toString(db->rootGroup()->timeInfo().creationTime())});
        rows.append({tr("Last saved"), Clock::toString(stats.modified)});
        rows.append({tr("Unsaved changes"), db->isModified() ? tr("yes") : tr("no")});
        rows.append({tr("Number of groups"), QString::number(stats.groupCount)});
        rows.append({tr("Number of entries"), QString::number(stats.entryCount)});
        rows.append({tr("Number of expired entries"), QString::number(stats.expiredEntries)});
        rows.append({tr("Unique passwords"), QString::number(stats.uniquePasswords)});
        rows.append({tr("Non-unique passwords"), QString::number(stats.reusedPasswords)});
        rows.append({tr("Maximum password reuse"), QString::number(stats.maxPwdReuse())});
        rows.append({tr("Number of short passwords"), QString::number(stats.shortPasswords)});
        rows.append({tr("Number of weak passwords"), QString::number(stats.weakPasswords)});
        rows.append({tr("Entries excluded from reports"), QString::number(stats.excludedEntries)});
        rows.append({tr("Average password length"), tr("%n character(s)", "", stats.averagePwdLength())});

        return snapshot;
    }

    QVector<ReportsFeed::Finding> ReportsFeed::filteredFindings() const
    {
        if (m_query.isEmpty()) {
            return m_snapshot.findings;
        }

        QVector<Finding> matching;
        for (const Finding& finding : m_snapshot.findings) {
            if (finding.title.contains(m_query, Qt::CaseInsensitive)
                || finding.path.contains(m_query, Qt::CaseInsensitive)
                || finding.reason.contains(m_query, Qt::CaseInsensitive)
                || finding.quality.contains(m_query, Qt::CaseInsensitive)) {
                matching.append(finding);
            }
        }
        return matching;
    }

    QVector<QPair<QString, QString>> ReportsFeed::filteredStatistics() const
    {
        if (m_query.isEmpty()) {
            return m_snapshot.statistics;
        }

        QVector<QPair<QString, QString>> matching;
        for (const auto& row : m_snapshot.statistics) {
            if (row.first.contains(m_query, Qt::CaseInsensitive) || row.second.contains(m_query, Qt::CaseInsensitive)) {
                matching.append(row);
            }
        }
        return matching;
    }

    int ReportsFeed::findingCount() const
    {
        return m_snapshot.valid ? m_snapshot.findings.size() : 0;
    }

    void ReportsFeed::apply()
    {
        emit findingCountChanged(findingCount());

        QVector<StatCard> cards;
        if (m_snapshot.valid) {
            cards.append({tr("Entries"),
                          QString::number(m_snapshot.entries),
                          tr("in %n group(s)", "", m_snapshot.groups),
                          Health::Unknown});
            cards.append({tr("Healthy passwords"),
                          QString::number(m_snapshot.healthy),
                          tr("rated good or excellent"),
                          Health::Ok});
            cards.append({tr("Weak or poor"),
                          QString::number(m_snapshot.findings.size()),
                          tr("%n short password(s)", "", m_snapshot.shortPasswords),
                          m_snapshot.findings.isEmpty() ? Health::Ok : Health::Weak});
            cards.append({tr("Passkeys"),
                          QString::number(m_snapshot.passkeys),
                          tr("stored in this database"),
                          Health::Unknown});
        }

        QVector<HealthRow> rows;
        for (const Finding& finding : filteredFindings()) {
            HealthRow row;
            row.id = finding.uuid;
            row.symbol = finding.bad ? QStringLiteral("lock_open") : QStringLiteral("lock_clock");
            row.title = finding.title;
            if (finding.excluded) {
                row.title.append(tr(" (Excluded)"));
            }
            if (finding.expired) {
                row.title.append(tr(" (Expired)"));
            }
            row.reason = finding.reason.isEmpty() ? finding.path : finding.reason;
            row.score = tr("%1 · %2").arg(finding.quality, QString::number(finding.score));
            row.status = healthOf(finding.bad);
            rows.append(row);
        }

        if (m_snapshot.valid && rows.isEmpty()) {
            HealthRow row;
            row.symbol = QStringLiteral("check_circle");
            row.title = m_query.isEmpty() ? tr("Every password is healthy") : tr("Nothing matches this filter");
            row.reason = m_query.isEmpty() ? tr("The health check found nothing to change.")
                                           : tr("%n finding(s) hidden by the filter.", "", m_snapshot.findings.size());
            row.score = m_query.isEmpty() ? tr("Clear") : tr("Filtered");
            row.status = Health::Ok;
            rows.append(row);
        }

        m_screen->setStatCards(cards);
        m_screen->setHealthRows(rows);
        m_screen->setStatistics(filteredStatistics());
    }

    QString ReportsFeed::markdown() const
    {
        QString text;
        QTextStream out(&text);

        out << "# " << tr("KeePassXC database report") << "\n\n";
        out << "- " << tr("Generated") << ": " << Clock::toString(Clock::currentDateTime()) << "\n";
        if (!m_snapshot.databaseName.isEmpty()) {
            out << "- " << tr("Database") << ": " << cell(m_snapshot.databaseName) << "\n";
        }
        if (!m_snapshot.databasePath.isEmpty()) {
            out << "- " << tr("File") << ": " << cell(QDir::toNativeSeparators(m_snapshot.databasePath)) << "\n";
        }
        if (!m_query.isEmpty()) {
            out << "- " << tr("Filter") << ": `" << cell(m_query) << "`\n";
        }
        out << "\n";

        if (!m_snapshot.valid) {
            out << tr("No database is open, so there is nothing to report.") << "\n";
            return text;
        }

        out << "## " << tr("Summary") << "\n\n";
        out << "| " << tr("Figure") << " | " << tr("Value") << " |\n|---|---:|\n";
        out << "| " << tr("Entries") << " | " << m_snapshot.entries << " |\n";
        out << "| " << tr("Groups") << " | " << m_snapshot.groups << " |\n";
        out << "| " << tr("Healthy passwords") << " | " << m_snapshot.healthy << " |\n";
        out << "| " << tr("Weak or poor passwords") << " | " << m_snapshot.findings.size() << " |\n";
        out << "| " << tr("Short passwords") << " | " << m_snapshot.shortPasswords << " |\n";
        out << "| " << tr("Passkeys") << " | " << m_snapshot.passkeys << " |\n\n";
        out << "> " << tr("Breach exposure is not included: it requires an online Have I Been Pwned lookup, which "
                          "this report does not perform.")
            << "\n\n";

        const QVector<Finding> findings = filteredFindings();
        out << "## " << tr("Password health") << "\n\n";
        if (findings.isEmpty()) {
            out << (m_query.isEmpty() ? tr("The health check found nothing to change.")
                                      : tr("No finding matches the active filter."))
                << "\n\n";
        } else {
            out << "| " << tr("Title") << " | " << tr("Path") << " | " << tr("Quality") << " | " << tr("Score") << " | "
                << tr("Reason") << " |\n|---|---|---|---:|---|\n";
            for (const Finding& finding : findings) {
                out << "| " << cell(finding.title) << " | " << cell(finding.path) << " | " << cell(finding.quality)
                    << " | " << finding.score << " | " << cell(finding.reason) << " |\n";
            }
            out << "\n";
        }

        const QVector<QPair<QString, QString>> statistics = filteredStatistics();
        out << "## " << tr("Statistics") << "\n\n";
        if (statistics.isEmpty()) {
            out << tr("No statistic matches the active filter.") << "\n";
        } else {
            out << "| " << tr("Name") << " | " << tr("Value") << " |\n|---|---|\n";
            for (const auto& row : statistics) {
                out << "| " << cell(row.first) << " | " << cell(row.second) << " |\n";
            }
        }

        return text;
    }

    void ReportsFeed::exportMarkdown()
    {
        QString suggestion = QStringLiteral("keepassxc-report.md");
        const QString base = QFileInfo(m_snapshot.databasePath).completeBaseName();
        if (!base.isEmpty()) {
            suggestion = QStringLiteral("%1-report.md").arg(base);
        }

        const QString directory = FileDialog::getLastDir(ExportDirectoryRole);
        const QString fileName = fileDialog()->getSaveFileName(m_screen,
                                                              tr("Export report to Markdown"),
                                                              QDir(directory).filePath(suggestion),
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

        Notify::success(tr("Report exported"),
                        m_query.isEmpty()
                            ? tr("Written to %1.").arg(QDir::toNativeSeparators(fileName))
                            : tr("Written to %1, filtered by \"%2\".")
                                  .arg(QDir::toNativeSeparators(fileName), m_query));
    }

} // namespace Material
