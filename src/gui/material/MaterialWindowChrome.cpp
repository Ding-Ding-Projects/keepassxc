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

#include "MaterialWindowChrome.h"

#include "MaterialTheme.h"

#include "core/Config.h"

#include <QLibrary>
#include <QSettings>
#include <QWidget>
#include <QWindow>

#include <windows.h>
#include <windowsx.h>

namespace Material
{
    namespace WindowChrome
    {
        namespace
        {
            // The DWM window attributes used here, named locally rather than
            // taken from <dwmapi.h> so the build does not depend on the Windows
            // SDK being new enough to declare them. They are numbers on the wire
            // either way, and an older desktop window manager fails the call.
            constexpr const char* FramelessProperty = "materialFrameless";
            /**
             * The native handle the frameless frame change was last sent to.
             * Qt destroys and recreates the HWND on setWindowFlags() (View >
             * Always on Top does exactly that), so a bool on the QWidget would
             * outlive the window it described and the new handle would never
             * be asked to recompute its frame: no caption, no resize borders,
             * no drag. Keyed on the handle, the install repeats when needed.
             */
            constexpr const char* FramelessHandleProperty = "materialFramelessHwnd";
            /** Logical height of the strip that counts as caption without a bar. */
            constexpr int FallbackCaptionHeight = 44;
            constexpr unsigned long AttrUseImmersiveDarkMode = 20; // DWMWA_USE_IMMERSIVE_DARK_MODE
            constexpr unsigned long AttrUseImmersiveDarkModeOld = 19; // ... as numbered before Windows 10 20H1
            constexpr unsigned long AttrWindowCornerPreference = 33; // DWMWA_WINDOW_CORNER_PREFERENCE
            constexpr unsigned long AttrBorderColor = 34; // DWMWA_BORDER_COLOR
            constexpr unsigned long AttrCaptionColor = 35; // DWMWA_CAPTION_COLOR
            constexpr unsigned long AttrTextColor = 36; // DWMWA_TEXT_COLOR
            constexpr unsigned long AttrSystemBackdropType = 38; // DWMWA_SYSTEMBACKDROP_TYPE

            constexpr int CornerRound = 2; // DWMWCP_ROUND
            constexpr int BackdropNone = 1; // DWMSBT_NONE
            constexpr int BackdropMainWindow = 2; // DWMSBT_MAINWINDOW - Mica

            /** Marks a window whose theme connection has already been made. */
            const char InstalledProperty[] = "materialWindowChrome";
            /** Marks a window that currently has a Mica backdrop behind it. */
            const char BackdropProperty[] = "materialBackdropActive";

            using SetWindowAttributeFn = HRESULT(__stdcall*)(HWND, DWORD, LPCVOID, DWORD);

            /**
             * DwmSetWindowAttribute, or nullptr where the desktop window manager
             * is not there to ask. Resolved once, at run time, so that none of
             * this adds a link dependency.
             */
            SetWindowAttributeFn resolveSetWindowAttribute()
            {
                static const auto function = reinterpret_cast<SetWindowAttributeFn>(
                    QLibrary::resolve(QStringLiteral("dwmapi"), "DwmSetWindowAttribute"));
                return function;
            }

            /** Set one attribute, reporting whether the desktop accepted it. */
            bool setAttribute(HWND handle, unsigned long attribute, const void* value, unsigned long size)
            {
                const auto function = resolveSetWindowAttribute();
                if (!function || !handle) {
                    return false;
                }
                return SUCCEEDED(function(handle, attribute, value, size));
            }

            bool setIntAttribute(HWND handle, unsigned long attribute, int value)
            {
                return setAttribute(handle, attribute, &value, sizeof(value));
            }

            /** Attributes taking a COLORREF: 0x00bbggrr, alpha ignored. */
            bool setColorAttribute(HWND handle, unsigned long attribute, const QColor& color)
            {
                const COLORREF value = RGB(color.red(), color.green(), color.blue());
                return setAttribute(handle, attribute, &value, sizeof(value));
            }

            /**
             * The native handle of the top-level window behind @p widget, or
             * nullptr if there is not one yet. The handle is not forced into
             * existence: the caller applies the chrome once the window is shown.
             */
            HWND handleFor(QWidget* widget)
            {
                if (!widget || !widget->window()) {
                    return nullptr;
                }
                QWindow* window = widget->window()->windowHandle();
                return window ? reinterpret_cast<HWND>(window->winId()) : nullptr;
            }

            /** Settings > Personalisation > Colours > Transparency effects. */
            bool transparencyEnabled()
            {
                QSettings settings(R"(HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize)",
                                   QSettings::NativeFormat);
                return settings.value(QStringLiteral("EnableTransparency"), 1).toInt() != 0;
            }

            /** Accessibility > Visual effects > Animation effects, off means still. */
            bool animationsEnabled()
            {
                BOOL animate = TRUE;
                if (::SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animate, 0)) {
                    return animate != FALSE;
                }
                return true;
            }

            /** High contrast wants flat, legible fills, not a blurred material. */
            bool highContrastEnabled()
            {
                HIGHCONTRASTW contrast{};
                contrast.cbSize = sizeof(contrast);
                if (::SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0)) {
                    return (contrast.dwFlags & HCF_HIGHCONTRASTON) != 0;
                }
                return false;
            }
        } // namespace

        bool isSupported()
        {
            return resolveSetWindowAttribute() != nullptr;
        }

        bool backdropAllowed()
        {
            if (!config()->get(Config::GUI_MaterialBackdrop).toBool()) {
                return false;
            }
            return transparencyEnabled() && animationsEnabled() && !highContrastEnabled();
        }

        void apply(QWidget* widget)
        {
            HWND handle = handleFor(widget);
            if (!handle) {
                return;
            }
            QWidget* window = widget->window();

            // A light caption over a dark application is the single most visible
            // thing wrong with the stock chrome, so the title bar is told which
            // way the application went rather than which way the OS did.
            const BOOL dark = theme()->isDark() ? TRUE : FALSE;
            if (!setAttribute(handle, AttrUseImmersiveDarkMode, &dark, sizeof(dark))) {
                setAttribute(handle, AttrUseImmersiveDarkModeOld, &dark, sizeof(dark));
            }

            // Material 3 rounds everything it draws; the window edge should not
            // be the one square corner in the picture.
            setIntAttribute(handle, AttrWindowCornerPreference, CornerRound);

            // The caption reads as the top of the application: the same container
            // colour as the surfaces below it, its text in the same ink, and a
            // hairline border in the same outline colour every card uses.
            setColorAttribute(handle, AttrCaptionColor, theme()->color(Role::SurfaceContainer));
            setColorAttribute(handle, AttrTextColor, theme()->color(Role::OnSurface));
            setColorAttribute(handle, AttrBorderColor, theme()->color(Role::OutlineVariant));

            // Mica, but only if the user asked for translucency and movement.
            // Turning it off is a real state, not the absence of a call: the
            // window may already be wearing one from a previous apply().
            const bool wanted = backdropAllowed();
            const bool accepted =
                setIntAttribute(handle, AttrSystemBackdropType, wanted ? BackdropMainWindow : BackdropNone);
            window->setProperty(BackdropProperty, wanted && accepted);
        }

        void installFrameless(QWidget* widget)
        {
            HWND handle = handleFor(widget);
            if (!handle) {
                return;
            }
            QWidget* window = widget->window();
            const auto current = static_cast<qulonglong>(reinterpret_cast<quintptr>(handle));
            if (window->property(FramelessProperty).toBool()
                && window->property(FramelessHandleProperty).toULongLong() == current) {
                return;
            }
            window->setProperty(FramelessProperty, true);
            window->setProperty(FramelessHandleProperty, current);
            // Keep WS_THICKFRAME and WS_CAPTION so the desktop still draws the
            // shadow, animates minimise and restore, and offers snap layouts;
            // WM_NCCALCSIZE below is what removes the visible caption. A frame
            // change makes the window manager ask again.
            ::SetWindowPos(handle,
                           nullptr,
                           0,
                           0,
                           0,
                           0,
                           SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        }

        void showSystemMenu(QWidget* widget, const QPoint& globalPos)
        {
            HWND handle = handleFor(widget);
            if (!handle) {
                return;
            }
            HMENU menu = ::GetSystemMenu(handle, FALSE);
            if (!menu) {
                return;
            }
            const bool maximized = ::IsZoomed(handle);
            ::EnableMenuItem(menu, SC_RESTORE, MF_BYCOMMAND | (maximized ? MF_ENABLED : MF_GRAYED));
            ::EnableMenuItem(menu, SC_MAXIMIZE, MF_BYCOMMAND | (maximized ? MF_GRAYED : MF_ENABLED));
            ::EnableMenuItem(menu, SC_SIZE, MF_BYCOMMAND | (maximized ? MF_GRAYED : MF_ENABLED));
            ::EnableMenuItem(menu, SC_MOVE, MF_BYCOMMAND | (maximized ? MF_GRAYED : MF_ENABLED));
            ::SetMenuDefaultItem(menu, SC_CLOSE, FALSE);
            // Device pixels: the popup is positioned by the desktop.
            const qreal ratio = widget->window()->devicePixelRatioF();
            const QPoint origin = widget->window()->mapToGlobal(QPoint(0, 0));
            RECT bounds{};
            ::GetWindowRect(handle, &bounds);
            const int x = bounds.left + qRound((globalPos.x() - origin.x()) * ratio);
            const int y = bounds.top + qRound((globalPos.y() - origin.y()) * ratio);
            const UINT flags = TPM_RETURNCMD | TPM_NONOTIFY | TPM_LEFTBUTTON | TPM_RIGHTBUTTON;
            const BOOL command = ::TrackPopupMenu(menu, flags, x, y, 0, handle, nullptr);
            if (command) {
                ::PostMessageW(handle, WM_SYSCOMMAND, static_cast<WPARAM>(command), 0);
            }
        }

        namespace
        {
            int frameThickness(HWND handle)
            {
                const UINT dpi = ::GetDpiForWindow(handle);
                return ::GetSystemMetricsForDpi(SM_CXSIZEFRAME, dpi) + ::GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
            }
        } // namespace

        bool handleNativeEvent(QWidget* widget,
                               void* message,
                               qintptr* result,
                               const std::function<bool(const QPoint&)>& captionTest)
        {
            auto* msg = static_cast<MSG*>(message);
            if (!msg || !widget || !widget->window() || !widget->window()->property(FramelessProperty).toBool()) {
                return false;
            }
            QWidget* window = widget->window();
            const HWND handle = msg->hwnd;

            // A message from a handle other than the one the frame change was
            // sent to means the window was recreated underneath us; ask the
            // new one to recompute its frame before answering for it.
            if (handle
                && window->property(FramelessHandleProperty).toULongLong()
                       != static_cast<qulonglong>(reinterpret_cast<quintptr>(handle))
                && handle == handleFor(widget)) {
                installFrameless(widget);
            }

            if (msg->message == WM_NCCALCSIZE && msg->wParam == TRUE) {
                // The client area is the whole window; when maximised the frame
                // hangs off the monitor edge, so pull the rectangle back inside
                // by the frame width or the top row of pixels is lost.
                auto* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(msg->lParam);
                if (::IsZoomed(handle)) {
                    const int frame = frameThickness(handle);
                    RECT& rect = params->rgrc[0];
                    rect.left += frame;
                    rect.top += frame;
                    rect.right -= frame;
                    rect.bottom -= frame;
                }
                *result = 0;
                return true;
            }

            if (msg->message == WM_NCHITTEST) {
                const int frame = frameThickness(handle);
                const QPoint global(GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam));
                RECT bounds{};
                ::GetWindowRect(handle, &bounds);
                const bool maximized = ::IsZoomed(handle);
                const bool left = global.x() < bounds.left + frame;
                const bool right = global.x() >= bounds.right - frame;
                const bool top = global.y() < bounds.top + frame;
                const bool bottom = global.y() >= bounds.bottom - frame;
                if (!maximized && (left || right || top || bottom)) {
                    if (top && left) {
                        *result = HTTOPLEFT;
                    } else if (top && right) {
                        *result = HTTOPRIGHT;
                    } else if (bottom && left) {
                        *result = HTBOTTOMLEFT;
                    } else if (bottom && right) {
                        *result = HTBOTTOMRIGHT;
                    } else if (left) {
                        *result = HTLEFT;
                    } else if (right) {
                        *result = HTRIGHT;
                    } else if (top) {
                        *result = HTTOP;
                    } else {
                        *result = HTBOTTOM;
                    }
                    return true;
                }
                // Device pixels to the window's logical coordinates. The DPI
                // is asked of the handle rather than Qt so the answer is right
                // on the very message that follows a monitor change.
                const UINT dpi = ::GetDpiForWindow(handle);
                const qreal ratio = dpi > 0 ? dpi / 96.0 : window->devicePixelRatioF();
                const int inset = maximized ? frame : 0;
                const QPoint local(qRound((global.x() - bounds.left - inset) / ratio),
                                   qRound((global.y() - bounds.top - inset) / ratio));
                if (captionTest && captionTest(local)) {
                    *result = HTCAPTION;
                    return true;
                }
                // Without a bar to ask (the shell is not built yet, or is gone)
                // the top strip still has to be caption, or the window can
                // never be moved again: DefWindowProc has no caption to find
                // once WM_NCCALCSIZE above has removed it.
                if (!captionTest && local.y() >= 0 && local.y() < FallbackCaptionHeight) {
                    *result = HTCAPTION;
                    return true;
                }
                *result = HTCLIENT;
                return true;
            }
            return false;
        }

        void install(QWidget* widget)
        {
            if (!widget || !widget->window()) {
                return;
            }
            QWidget* window = widget->window();

            if (!window->property(InstalledProperty).toBool()) {
                window->setProperty(InstalledProperty, true);
                // The caption is part of the theme, so it changes with it. The
                // connection is owned by the window and dies with it.
                QObject::connect(theme(), &Theme::changed, window, [window] { apply(window); });
            }

            apply(window);
        }

        bool backdropActive(const QWidget* widget)
        {
            if (!widget || !widget->window()) {
                return false;
            }
            return widget->window()->property(BackdropProperty).toBool();
        }

    } // namespace WindowChrome
} // namespace Material
