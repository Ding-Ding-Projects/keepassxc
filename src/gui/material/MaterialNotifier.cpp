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

#include "MaterialNotifier.h"

#include "MaterialNotificationCentre.h"

// ---------------------------------------------------------------------------
// INTEGRATION POINT - Material::Voice
//
// Every string this router shows passes through voiced() below. The voice
// catalogue is optional: when MaterialVoice.h is present the copy is resolved
// through it, and when it is not the plain string goes out unchanged. This
// header guard and voiced() are the only two places that know about Voice.
// ---------------------------------------------------------------------------
#if __has_include("MaterialVoice.h")
#include "MaterialVoice.h"
#define KPXC_MATERIAL_VOICE 1
#endif

#include <QApplication>
#include <QHash>
#include <QPointer>
#include <QTimer>
#include <QWidget>

namespace Material
{
    namespace Notify
    {
        namespace
        {
            QPointer<QWidget> g_host;

            /** A running job: the toast it owns and the entry it filed. */
            struct Job
            {
                QPointer<Snackbar> bar;
                quint64 entry = 0;
            };

            QHash<QString, Job>& jobs()
            {
                static QHash<QString, Job> map;
                return map;
            }

            /**
             * The single integration point with Material::Voice.
             *
             * @p text is either a catalogue id or literal copy; the catalogue
             * hands an id it does not know straight back, so both work. Without
             * Voice the string goes out as it came in.
             */
            QString voiced(SeverityLevel severity, const QString& text)
            {
#ifdef KPXC_MATERIAL_VOICE
                if (text.isEmpty()) {
                    return text;
                }
                Voice::Category category = Voice::Category::Info;
                switch (severity) {
                case SeverityLevel::Success:
                    category = Voice::Category::Success;
                    break;
                case SeverityLevel::Warning:
                    category = Voice::Category::Warning;
                    break;
                case SeverityLevel::Error:
                    category = Voice::Category::Error;
                    break;
                case SeverityLevel::Info:
                    break;
                }
                return Voice::say(text, category);
#else
                Q_UNUSED(severity)
                return text;
#endif
            }

            /** The window notifications belong to, pinned or freshly guessed. */
            QWidget* resolveHost()
            {
                if (g_host) {
                    return g_host->window();
                }
                if (QWidget* active = QApplication::activeWindow()) {
                    return active;
                }
                const auto windows = QApplication::topLevelWidgets();
                for (QWidget* candidate : windows) {
                    if (candidate->isWindow() && candidate->isVisible()
                        && candidate->windowType() != Qt::ToolTip) {
                        return candidate;
                    }
                }
                return nullptr;
            }

            void dispatch(SeverityLevel severity,
                          const QString& title,
                          const QString& body,
                          const QList<NotificationAction>& actions)
            {
                QWidget* window = resolveHost();
                if (!window) {
                    return;
                }

                const QString spokenTitle = voiced(severity, title);
                const QString spokenBody = voiced(severity, body);

                if (auto* history = NotificationCentre::centreFor(window)) {
                    history->record(severity, spokenTitle, spokenBody, actions);
                }
                if (auto* bars = SnackbarHost::hostFor(window)) {
                    bars->show(severity, spokenTitle, spokenBody, actions);
                }
            }
        } // namespace

        void setHost(QWidget* newHost)
        {
            g_host = newHost;
        }

        QWidget* host()
        {
            return resolveHost();
        }

        void info(const QString& text)
        {
            dispatch(SeverityLevel::Info, QString(), text, {});
        }

        void info(const QString& title, const QString& body, const QList<NotificationAction>& actions)
        {
            dispatch(SeverityLevel::Info, title, body, actions);
        }

        void success(const QString& text)
        {
            dispatch(SeverityLevel::Success, QString(), text, {});
        }

        void success(const QString& title, const QString& body, const QList<NotificationAction>& actions)
        {
            dispatch(SeverityLevel::Success, title, body, actions);
        }

        void warning(const QString& text)
        {
            dispatch(SeverityLevel::Warning, QString(), text, {});
        }

        void warning(const QString& title, const QString& body, const QList<NotificationAction>& actions)
        {
            dispatch(SeverityLevel::Warning, title, body, actions);
        }

        void error(const QString& text)
        {
            dispatch(SeverityLevel::Error, QString(), text, {});
        }

        void error(const QString& title, const QString& body, const QList<NotificationAction>& actions)
        {
            dispatch(SeverityLevel::Error, title, body, actions);
        }

        void progress(const QString& id, const QString& text, int percent)
        {
            QWidget* window = resolveHost();
            if (!window) {
                return;
            }

            const QString body = voiced(SeverityLevel::Info, text);

            Job& job = jobs()[id];
            if (!job.bar) {
                // A job owns its toast until it finishes, so it never times out.
                if (auto* bars = SnackbarHost::hostFor(window)) {
                    job.bar = bars->show(SeverityLevel::Info, QString(), body, {}, 0);
                }
                if (auto* history = NotificationCentre::centreFor(window)) {
                    job.entry = history->record(SeverityLevel::Info, QString(), body);
                }
            }

            Snackbar* bar = job.bar.data();
            const quint64 entry = job.entry;
            if (bar) {
                bar->setMessage(body);
                bar->setProgress(percent < 0 ? Snackbar::NoProgress : qMin(percent, 100));
            }

            if (percent < 100) {
                return;
            }

            if (auto* history = NotificationCentre::centreFor(window)) {
                history->updateEntry(entry, body, SeverityLevel::Success);
            }
            if (bar) {
                bar->setProgress(Snackbar::NoProgress);
                QTimer::singleShot(ToastLifetime, bar, &Snackbar::dismiss);
            }
            jobs().remove(id);
        }

        void endProgress(const QString& id)
        {
            const Job job = jobs().take(id);
            if (job.bar) {
                job.bar->dismiss();
            }
        }

        NotificationCentre* centre()
        {
            QWidget* window = resolveHost();
            return window ? NotificationCentre::centreFor(window) : nullptr;
        }

    } // namespace Notify

} // namespace Material
