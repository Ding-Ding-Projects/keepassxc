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

#ifndef KEEPASSXC_MATERIALCAPTUREROUTE_H
#define KEEPASSXC_MATERIALCAPTUREROUTE_H

#include <QSize>
#include <QString>

class MainWindow;

namespace Material
{
    /**
     * Deterministic capture routes for design-parity evidence.
     *
     * A route is a `kpxc://capture/<screen>` URL naming one screen and state
     * from design/parity/inventory.json, plus the capture tuple as query
     * parameters:
     *
     *   kpxc://capture/reports?state=default&width=1200&height=860
     *        &theme=light&lang=bilingual&target=page&page=<sheet-or-settings-page>
     *
     * The route is applied once the main window is on screen: the dim sum
     * surprise is suppressed, the theme mode and voice language are forced,
     * the window is resized so the Material shell's client area matches the
     * requested viewport exactly, and the shell is navigated to the screen.
     * When everything has settled a JSON receipt is written so a capture
     * harness can poll for readiness instead of guessing a delay.
     *
     * Screens map onto shell destinations: `shell` and `regex-builder` land
     * on the vault (the builder additionally opens its overlay),
     * `sheet-editor` lands on the entry sheet, and every other screen is the
     * destination of the same name.
     */
    namespace CaptureRoute
    {
        struct Request
        {
            QString screen;
            QString state = QStringLiteral("default");
            QString page;
            QSize viewport{1440, 920};
            QString theme = QStringLiteral("light");
            QString languageMode = QStringLiteral("bilingual");
            /** Size the current destination page, not the whole shell, to the viewport. */
            bool fitPage = false;
            QString receiptPath;
        };

        /** True when @p url is a well-formed capture route; otherwise @p error says why. */
        bool parse(const QString& url, Request& out, QString* error = nullptr);

        /** The shell destination a screen lands on, or an empty string for an unknown screen. */
        QString destinationFor(const QString& screen);

        /** Apply @p request to @p window once it is shown, then write the receipt. */
        void schedule(MainWindow* window, const Request& request);
    } // namespace CaptureRoute
} // namespace Material

#endif // KEEPASSXC_MATERIALCAPTUREROUTE_H
