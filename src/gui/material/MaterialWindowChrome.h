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

#ifndef KEEPASSXC_MATERIALWINDOWCHROME_H
#define KEEPASSXC_MATERIALWINDOWCHROME_H

class QWidget;

namespace Material
{
    /**
     * The window's own title bar, dressed as part of the application.
     *
     * Everything below the caption is Material; the caption itself belongs to
     * the desktop window manager, and left alone it is a grey stripe with the
     * wrong colour, the wrong glyphs and square corners sitting on top of a
     * Material 3 window. Windows 11 lets an application say what that stripe
     * should look like, so this says it:
     *
     *   - immersive dark mode, following theme()->isDark() rather than the OS;
     *   - rounded corners, matching the shape scale;
     *   - caption, caption text and border painted from SurfaceContainer,
     *     OnSurface and OutlineVariant;
     *   - a Mica backdrop, when the user has transparency effects and window
     *     animations turned on and has not turned it off in the configuration.
     *
     * Every entry point is resolved from dwmapi.dll at run time and every
     * attribute is optional: an older Windows rejects the ones it does not
     * know, the calls fail, and the window looks exactly as it did before.
     * Nothing here is load bearing.
     */
    namespace WindowChrome
    {
        /** True when this Windows build offers DwmSetWindowAttribute at all. */
        bool isSupported();

        /**
         * Apply the chrome to the top-level window of @p window and keep it in
         * step with the theme.
         *
         * Safe to call more than once - the theme connection is made on the
         * first call only, so a window that comes back from the tray simply
         * has its attributes refreshed.
         */
        void install(QWidget* window);

        /** Apply the chrome once, without subscribing to theme changes. */
        void apply(QWidget* window);

        /**
         * Whether a Mica backdrop is wanted right now: the GUI_MaterialBackdrop
         * key, the user's transparency effects switch, the client area
         * animation preference and high contrast mode all get a veto.
         */
        bool backdropAllowed();

        /**
         * Whether the last apply() actually put Mica behind @p window. Stored
         * on the window as the `materialBackdropActive` property so a surface
         * can decide whether it is worth drawing translucently.
         */
        bool backdropActive(const QWidget* window);
    } // namespace WindowChrome

} // namespace Material

#endif // KEEPASSXC_MATERIALWINDOWCHROME_H
