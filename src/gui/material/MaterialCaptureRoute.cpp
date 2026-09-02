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

#include "MaterialCaptureRoute.h"

#include "MaterialAppearanceEditor.h"

#include "MaterialDimSum.h"
#include "MaterialShell.h"
#include "MaterialSpecSheet.h"
#include "MaterialTheme.h"
#include "config-keepassx.h"
#include "core/Config.h"
#include "gui/MainWindow.h"

#include <QAbstractButton>
#include <QAbstractScrollArea>
#include <QComboBox>
#include <QFile>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QScrollBar>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QWindow>

namespace Material
{
    namespace CaptureRoute
    {
        namespace
        {
            constexpr int kSettleMs = 350;
            constexpr int kReceiptMs = 1200;

            /**
             * Measure every visible widget under the shell for the clipping
             * matrix: geometry in window coordinates, size hint versus actual
             * size, whether a label's or button's text is wider than the space
             * it has, and the scroll range of every scroll area. This is what
             * turns "it looks clipped" into a number a test can assert.
             */
            QJsonArray probeWidgets(MainWindow* window)
            {
                QJsonArray widgets;
                auto* shell = Shell::instance();
                if (!shell) {
                    return widgets;
                }
                const QList<QWidget*> all = shell->findChildren<QWidget*>();
                for (QWidget* widget : all) {
                    if (!widget->isVisible() || widget->width() <= 0 || widget->height() <= 0) {
                        continue;
                    }
                    QJsonObject entry;
                    const QPoint origin = widget->mapTo(window, QPoint(0, 0));
                    entry.insert(QStringLiteral("class"), QString::fromLatin1(widget->metaObject()->className()));
                    entry.insert(QStringLiteral("name"), widget->objectName());
                    entry.insert(QStringLiteral("rect"),
                                 QStringLiteral("%1,%2 %3x%4").arg(origin.x()).arg(origin.y()).arg(widget->width()).arg(widget->height()));
                    const QSize hint = widget->sizeHint();
                    const QSize minimum = widget->minimumSizeHint();
                    entry.insert(QStringLiteral("hint"), QStringLiteral("%1x%2").arg(hint.width()).arg(hint.height()));
                    entry.insert(QStringLiteral("minimumHint"), QStringLiteral("%1x%2").arg(minimum.width()).arg(minimum.height()));
                    // A control narrower than its own minimum hint has been squeezed.
                    // Containers and stacks report the largest of their children as a
                    // hint, so only leaf controls are judged this way.
                    const bool leaf = widget->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly).isEmpty()
                                      || qobject_cast<QAbstractButton*>(widget) || qobject_cast<QComboBox*>(widget)
                                      || qobject_cast<QLineEdit*>(widget) || qobject_cast<QLabel*>(widget);
                    entry.insert(QStringLiteral("leaf"), leaf);
                    entry.insert(QStringLiteral("squeezed"),
                                 leaf
                                     && ((minimum.width() > 0 && widget->width() < minimum.width())
                                         || (minimum.height() > 0 && widget->height() < minimum.height())));
                    // Text that does not fit its widget is the definition of clipping;
                    // a label that elides on purpose still reports it so the row can
                    // decide whether the elision was declared.
                    QString text;
                    bool wordWrap = false;
                    if (auto* label = qobject_cast<QLabel*>(widget)) {
                        // Rich text measures its markup, not its rendering; skip it.
                        if (label->textFormat() == Qt::PlainText
                            || (label->textFormat() == Qt::AutoText && !label->text().contains(QLatin1Char('<')))) {
                            text = label->text();
                        }
                        wordWrap = label->wordWrap();
                    } else if (auto* button = qobject_cast<QAbstractButton*>(widget)) {
                        text = button->text();
                        // A button that reports height-for-width wraps its label.
                        wordWrap = widget->sizePolicy().hasHeightForWidth();
                    } else if (auto* edit = qobject_cast<QLineEdit*>(widget)) {
                        text = edit->placeholderText();
                    }
                    if (!text.isEmpty()) {
                        const int textWidth = widget->fontMetrics().horizontalAdvance(text);
                        entry.insert(QStringLiteral("text"), text.left(80));
                        entry.insert(QStringLiteral("textWidth"), textWidth);
                        // A one-line text overflows sideways; a wrapping label overflows
                        // downwards when its lines need more height than it was given.
                        bool overflows = !wordWrap && textWidth > widget->contentsRect().width();
                        if (wordWrap && widget->sizePolicy().hasHeightForWidth()) {
                            overflows = widget->heightForWidth(widget->width()) > widget->height() + 1;
                        }
                        entry.insert(QStringLiteral("textOverflows"), overflows);
                    }
                    if (auto* area = qobject_cast<QAbstractScrollArea*>(widget)) {
                        entry.insert(QStringLiteral("hScrollMax"), area->horizontalScrollBar()->maximum());
                        entry.insert(QStringLiteral("vScrollMax"), area->verticalScrollBar()->maximum());
                    }
                    // Part of the widget outside the window's client area is off-screen,
                    // unless it lives inside a scroll area, where content below the fold
                    // is reachable by design.
                    bool scrollable = false;
                    for (QWidget* ancestor = widget->parentWidget(); ancestor; ancestor = ancestor->parentWidget()) {
                        if (qobject_cast<QAbstractScrollArea*>(ancestor)) {
                            scrollable = true;
                            break;
                        }
                    }
                    entry.insert(QStringLiteral("scrollable"), scrollable);
                    const QRect client(QPoint(0, 0), window->size());
                    entry.insert(QStringLiteral("offscreen"), !scrollable && !client.contains(QRect(origin, widget->size())));
                    widgets.append(entry);
                }
                return widgets;
            }

            void writeReceipt(MainWindow* window, const Request& request, const QString& outcome)
            {
                if (request.receiptPath.isEmpty()) {
                    return;
                }
                auto* shell = Shell::instance();
                QJsonObject receipt;
                receipt.insert(QStringLiteral("schemaVersion"), 1);
                receipt.insert(QStringLiteral("outcome"), outcome);
                receipt.insert(QStringLiteral("screen"), request.screen);
                receipt.insert(QStringLiteral("state"), request.state);
                receipt.insert(QStringLiteral("page"), request.page);
                receipt.insert(QStringLiteral("theme"), theme()->isDark() ? QStringLiteral("dark") : QStringLiteral("light"));
                receipt.insert(QStringLiteral("languageMode"), config()->get(Config::GUI_VoiceLanguage).toString());
                receipt.insert(QStringLiteral("version"), QString::fromLatin1(KEEPASSXC_VERSION));
                receipt.insert(QStringLiteral("target"), request.fitPage ? QStringLiteral("page") : QStringLiteral("shell"));
                receipt.insert(QStringLiteral("requestedViewport"),
                               QStringLiteral("%1x%2").arg(request.viewport.width()).arg(request.viewport.height()));
                if (shell) {
                    receipt.insert(QStringLiteral("shellViewport"),
                                   QStringLiteral("%1x%2").arg(shell->width()).arg(shell->height()));
                    receipt.insert(QStringLiteral("destination"), shell->currentDestination());
                    // The destination page's rectangle in window client coordinates,
                    // so a harness can crop a client capture to the design's
                    // destination-only reference.
                    if (auto* page = shell->destination(shell->currentDestination())) {
                        const QPoint origin = page->mapTo(window, QPoint(0, 0));
                        receipt.insert(QStringLiteral("pageRect"),
                                       QStringLiteral("%1,%2 %3x%4")
                                           .arg(origin.x())
                                           .arg(origin.y())
                                           .arg(page->width())
                                           .arg(page->height()));
                    }
                    const QPoint shellOrigin = shell->mapTo(window, QPoint(0, 0));
                    receipt.insert(QStringLiteral("shellRect"),
                                   QStringLiteral("%1,%2 %3x%4")
                                       .arg(shellOrigin.x())
                                       .arg(shellOrigin.y())
                                       .arg(shell->width())
                                       .arg(shell->height()));
                }
                receipt.insert(QStringLiteral("windowGeometry"),
                               QStringLiteral("%1,%2 %3x%4")
                                   .arg(window->x())
                                   .arg(window->y())
                                   .arg(window->width())
                                   .arg(window->height()));
                receipt.insert(QStringLiteral("hwnd"), QString::number(static_cast<qulonglong>(window->winId())));
                receipt.insert(QStringLiteral("devicePixelRatio"), window->devicePixelRatioF());
                receipt.insert(QStringLiteral("title"), window->windowTitle());
                if (request.probe) {
                    receipt.insert(QStringLiteral("widgets"), probeWidgets(window));
                }

                QSaveFile file(request.receiptPath);
                if (file.open(QIODevice::WriteOnly)) {
                    file.write(QJsonDocument(receipt).toJson(QJsonDocument::Indented));
                    file.commit();
                }
            }
            /**
             * Grow or shrink the window so the measured surface - the shell, or
             * the current destination page when the route targets the page -
             * has exactly the requested viewport. The window owns whatever
             * chrome sits around that surface, so the difference between the
             * two sizes is what has to change.
             */
            void fitViewport(MainWindow* window, const Request& request)
            {
                auto* shell = Shell::instance();
                if (!shell) {
                    return;
                }
                QWidget* surface = shell;
                if (request.fitPage) {
                    if (auto* page = shell->destination(shell->currentDestination())) {
                        surface = page;
                    }
                }
                if (surface->size() == request.viewport) {
                    return;
                }
                const QSize chrome = window->size() - surface->size();
                window->resize(request.viewport + chrome);
            }
        } // namespace

        QString destinationFor(const QString& screen)
        {
            // "welcome" is the vault destination with no database open: the front
            // screen a user meets first, where the version provenance lives.
            if (screen == QLatin1String("shell") || screen == QLatin1String("regex-builder")
                || screen == QLatin1String("vault") || screen == QLatin1String("welcome")) {
                return QStringLiteral("vault");
            }
            if (screen == QLatin1String("sheet-editor")) {
                return QStringLiteral("editor");
            }
            static const QStringList direct = {QStringLiteral("reports"),
                                               QStringLiteral("history"),
                                               QStringLiteral("changelog"),
                                               QStringLiteral("settings"),
                                               QStringLiteral("appearance"),
                                               QStringLiteral("editor"),
                                               QStringLiteral("database"),
                                               QStringLiteral("tools"),
                                               QStringLiteral("help")};
            return direct.contains(screen) ? screen : QString();
        }

        bool parse(const QString& text, Request& out, QString* error)
        {
            const QUrl url(text);
            auto fail = [error](const QString& message) {
                if (error) {
                    *error = message;
                }
                return false;
            };
            if (!url.isValid() || url.scheme() != QLatin1String("kpxc") || url.host() != QLatin1String("capture")) {
                return fail(QStringLiteral("A capture route must look like kpxc://capture/<screen>?state=<state>."));
            }
            const QString screen = url.path().mid(1);
            if (destinationFor(screen).isEmpty()) {
                return fail(QStringLiteral("Unknown capture screen: %1").arg(screen));
            }
            Request request;
            request.receiptPath = out.receiptPath;
            request.screen = screen;
            const QUrlQuery query(url);
            if (query.hasQueryItem(QStringLiteral("state"))) {
                request.state = query.queryItemValue(QStringLiteral("state"));
            }
            request.page = query.queryItemValue(QStringLiteral("page"));
            bool widthOk = true;
            bool heightOk = true;
            const int width = query.hasQueryItem(QStringLiteral("width"))
                                  ? query.queryItemValue(QStringLiteral("width")).toInt(&widthOk)
                                  : request.viewport.width();
            const int height = query.hasQueryItem(QStringLiteral("height"))
                                   ? query.queryItemValue(QStringLiteral("height")).toInt(&heightOk)
                                   : request.viewport.height();
            if (!widthOk || !heightOk || width < 320 || height < 240 || width > 7680 || height > 4320) {
                return fail(QStringLiteral("The capture viewport must be between 320x240 and 7680x4320."));
            }
            request.viewport = QSize(width, height);
            if (query.hasQueryItem(QStringLiteral("theme"))) {
                request.theme = query.queryItemValue(QStringLiteral("theme")).toLower();
                if (request.theme != QLatin1String("light") && request.theme != QLatin1String("dark")) {
                    return fail(QStringLiteral("The capture theme must be light or dark."));
                }
            }
            request.probe = query.queryItemValue(QStringLiteral("probe")) == QLatin1String("1");
            if (query.hasQueryItem(QStringLiteral("target"))) {
                const QString target = query.queryItemValue(QStringLiteral("target")).toLower();
                if (target == QLatin1String("page")) {
                    request.fitPage = true;
                } else if (target != QLatin1String("shell")) {
                    return fail(QStringLiteral("The capture target must be shell or page."));
                }
            }
            if (query.hasQueryItem(QStringLiteral("lang"))) {
                const QString lang = query.queryItemValue(QStringLiteral("lang")).toLower();
                if (lang == QLatin1String("english") || lang == QLatin1String("en")) {
                    request.languageMode = QStringLiteral("english");
                } else if (lang == QLatin1String("cantonese") || lang == QLatin1String("yue")) {
                    request.languageMode = QStringLiteral("cantonese");
                } else if (lang == QLatin1String("bilingual") || lang == QLatin1String("both")) {
                    request.languageMode = QStringLiteral("bilingual");
                } else {
                    return fail(QStringLiteral("The capture language must be english, cantonese or bilingual."));
                }
            }
            out = request;
            return true;
        }

        void schedule(MainWindow* window, const Request& request)
        {
            // Never let the launch draw a dim sum card over the surface being measured.
            DimSum::suppress();

            QTimer::singleShot(kSettleMs, window, [window, request] {
                const QString language = request.languageMode == QLatin1String("english")
                                             ? QStringLiteral("English")
                                             : request.languageMode == QLatin1String("cantonese")
                                                   ? QStringLiteral("Cantonese")
                                                   : QStringLiteral("Bilingual");
                config()->set(Config::GUI_VoiceLanguage, language);
                theme()->setMode(request.theme == QLatin1String("dark") ? Mode::Dark : Mode::Light);

                const bool navigated = window->captureNavigate(request.screen, request.page);
                fitViewport(window, request);

                QTimer::singleShot(kReceiptMs, window, [window, request, navigated] {
                    // A first resize can be clamped by the platform or change the
                    // breakpoint; measure again and correct once.
                    fitViewport(window, request);
                    QTimer::singleShot(kSettleMs, window, [window, request, navigated] {
                        // The appearance-editor state opens the per-element editor on the
                        // interface font family select, so the editor is addressable.
                        if (request.state == QLatin1String("appearance-editor")) {
                            if (auto* target = window->findChild<QWidget*>(QStringLiteral("appearanceFontFamily"))) {
                                AppearanceEditor::instance()->editElement(target);
                            }
                        }
                        // The personal-vocabulary state opens Settings > Interface and
                        // scrolls the upload row into view so a driver can click it.
                        if (request.state == QLatin1String("personal-vocabulary")) {
                            // Several hubs own a spec sheet; the one with an interface
                            // page is the application settings hub.
                            for (SpecSheet* sheet : window->findChildren<SpecSheet*>()) {
                                auto* page = sheet->page(QStringLiteral("interface"));
                                if (!page) {
                                    continue;
                                }
                                sheet->setCurrentPage(QStringLiteral("interface"));
                                if (auto* row = page->findChild<QWidget*>(QStringLiteral("specRow_upload_file"))) {
                                    page->ensureWidgetVisible(row, 0, 80);
                                }
                                break;
                            }
                        }
                        writeReceipt(window, request, navigated ? QStringLiteral("ready") : QStringLiteral("unreachable"));
                    });
                });
            });
        }
    } // namespace CaptureRoute
} // namespace Material
